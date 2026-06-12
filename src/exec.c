/*
 * exec.c - ST4Ever Execution engine
 *
 * Owns the live cpu68k_t and the execution thread.  The monitor and
 * CPU-register views read from the shared exec_state_t snapshot.
 *
 * UC25A: step/run/pause/stop/breakpoint, exec_mon, exec_cpu windows.
 * UC25B: memory dump view (exec_mem) + disassembly view (exec_asm).
 * UC28: linea_init() called at exec_open to prepare Line-A block.
 */

#include "exec.h"
#include "exec_mon.h"
#include "exec_cpu.h"
#include "exec_mem.h"
#include "exec_asm.h"
#include "exec_screen.h"
#include "linea.h"
#include "load.h"
#include "trace.h"

#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------
 * Module globals
 * ------------------------------------------------------------------ */

static st_bool_t      g_exec_bInited  = ST_FALSE;
static st_bool_t      g_exec_bOpen    = ST_FALSE;
static st_machine_t  *g_exec_ptMachine = NULL;
static st_thread_t   *g_exec_ptThread  = NULL;
static exec_state_t   g_exec_tState;
static cpu68k_t       g_exec_tCpu;     /* owned exclusively by exec thread */

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/* Disassemble instruction at ptCpu->uiPC into ptState->szNextDisasm.
 * Must be called with ptState->ptMutex held. */
static void exec_refresh_disasm(exec_state_t *ptState)
{
    disasm_result_t tDisasm;
    st_u32_t        uiPC;

    memset(&tDisasm, 0, sizeof(tDisasm));
    uiPC = ptState->tCpuSnap.uiPC;
    if (uiPC < (st_u32_t)ST_RAM_SIZE)
    {
        disasm_one(g_exec_ptMachine->aRam + uiPC,
                   (size_t)(ST_RAM_SIZE - uiPC),
                   uiPC,
                   &tDisasm);
        strncpy(ptState->szNextDisasm, tDisasm.szLine,
                DISASM_LINE_MAX - 1);
        ptState->szNextDisasm[DISASM_LINE_MAX - 1] = '\0';
    }
    else
    {
        ptState->szNextDisasm[0] = '\0';
    }
}

/* Check if uiAddr is a breakpoint. Returns index or -1. */
static int exec_bp_find(const exec_state_t *ptState, st_u32_t uiAddr)
{
    int i;

    for (i = 0; i < ptState->iBPCount; i++)
    {
        if (ptState->auBP[i] == uiAddr)
        {
            return i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------
 * Execution thread
 * ------------------------------------------------------------------ */

static void exec_thread_fn(void *pArg)
{
    exec_state_t     *ptState;
    cpu68k_t         *ptCpu;
    cpu_step_result_t tResult;
    st_bool_t         bQuit;
    st_bool_t         bDoStep;
    st_bool_t         bDoRun;
    exec_run_state_t  eRunState;
    int               iQuantum;
    int               i;
    int               iBPIdx;

    ptState = (exec_state_t *)pArg;
    ptCpu   = &g_exec_tCpu;
    bQuit   = ST_FALSE;

    while (!bQuit)
    {
        /* ---- Acquire mutex and process all pending control flags ---- */
        platform_mutex_lock(ptState->ptMutex);

        bDoStep   = ptState->bStepReq;
        bDoRun    = ptState->bRunReq;
        eRunState = ptState->eRunState;

        if (ptState->bQuitReq)
        {
            ptState->bQuitReq  = ST_FALSE;
            ptState->eRunState = EXEC_RUN_PAUSED;
            bQuit = ST_TRUE;
        }
        if (ptState->bStopReq)
        {
            ptState->bStopReq  = ST_FALSE;
            ptState->eRunState = EXEC_RUN_PAUSED;
            eRunState          = EXEC_RUN_PAUSED;
            /* Reset CPU to load address */
            cpu_init(ptCpu, g_exec_ptMachine);
            ptCpu->uiPC  = ptState->uiLoadAddr;
            ptCpu->uiSSP = ST_LOAD_BASE - 2u;
            ptCpu->auAn[7] = ptCpu->uiSSP;
            memcpy(&ptState->tCpuSnap, ptCpu, sizeof(cpu68k_t));
            exec_refresh_disasm(ptState);
            bDoStep = ST_FALSE;
            bDoRun  = ST_FALSE;
        }
        if (ptState->bPauseReq)
        {
            ptState->bPauseReq = ST_FALSE;
            ptState->eRunState = EXEC_RUN_PAUSED;
            eRunState          = EXEC_RUN_PAUSED;
            bDoRun             = ST_FALSE;
        }
        if (bDoStep)
        {
            ptState->bStepReq  = ST_FALSE;
        }
        if (bDoRun)
        {
            ptState->bRunReq   = ST_FALSE;
            ptState->eRunState = EXEC_RUN_RUNNING;
            eRunState          = EXEC_RUN_RUNNING;
        }

        platform_mutex_unlock(ptState->ptMutex);

        if (bQuit) { break; }

        /* ---- Nothing to execute? Sleep briefly ---- */
        if (!bDoStep
            && eRunState != EXEC_RUN_RUNNING
            && eRunState != EXEC_RUN_HALTED)
        {
            platform_sleep_ms(10);
            continue;
        }

        if (eRunState == EXEC_RUN_HALTED)
        {
            platform_sleep_ms(50);
            continue;
        }

        /* ---- Execute up to iQuantum instructions ---- */
        iQuantum = bDoStep ? 1 : EXEC_QUANTUM;

        for (i = 0; i < iQuantum; i++)
        {
            /* PC bounds check: below load base = illegal territory */
            if (ptCpu->uiPC < ST_LOAD_BASE)
            {
                platform_mutex_lock(ptState->ptMutex);
                ptState->eRunState = EXEC_RUN_HALTED;
                platform_mutex_unlock(ptState->ptMutex);
                iQuantum = 0; /* force exit loop */
                break;
            }

            /* CPU already halted by previous instruction? */
            if (ptCpu->eState == CPU_STATE_HALTED
                || ptCpu->eState == CPU_STATE_STOPPED)
            {
                platform_mutex_lock(ptState->ptMutex);
                ptState->eRunState = EXEC_RUN_HALTED;
                platform_mutex_unlock(ptState->ptMutex);
                iQuantum = 0;
                break;
            }

            memset(&tResult, 0, sizeof(tResult));
            cpu_step(ptCpu, g_exec_ptMachine, &tResult);

            /* Check breakpoints — lock for read */
            platform_mutex_lock(ptState->ptMutex);
            iBPIdx = exec_bp_find(ptState, ptCpu->uiPC);
            if (iBPIdx >= 0)
            {
                ptState->eRunState = EXEC_RUN_PAUSED;
                platform_mutex_unlock(ptState->ptMutex);
                iQuantum = i + 1; /* stop after this instruction */
                break;
            }
            platform_mutex_unlock(ptState->ptMutex);
        }

        /* ---- Update snapshot with final state ---- */
        platform_mutex_lock(ptState->ptMutex);
        memcpy(&ptState->tCpuSnap, ptCpu, sizeof(cpu68k_t));
        ptState->tLastResult = tResult;
        if (bDoStep)
        {
            ptState->eRunState = EXEC_RUN_PAUSED;
        }
        exec_refresh_disasm(ptState);
        platform_mutex_unlock(ptState->ptMutex);

        /* ---- Invalidate views ---- */
        if (ptState->hMonWnd != NULL)
        {
            gui_invalidate(ptState->hMonWnd);
        }
        if (ptState->hCpuWnd != NULL)
        {
            gui_invalidate(ptState->hCpuWnd);
        }
        if (ptState->hMemWnd != NULL)
        {
            gui_invalidate(ptState->hMemWnd);
        }
        if (ptState->hAsmWnd != NULL)
        {
            gui_invalidate(ptState->hAsmWnd);
        }
        if (ptState->hScrWnd != NULL)
        {
            gui_invalidate(ptState->hScrWnd);
        }
    }
}

/* ------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------ */

st_error_t exec_init(st_machine_t *ptMachine)
{
    LOG_TRACE("ptMachine=%p", (void *)ptMachine);

    if (ptMachine == NULL)
    {
        LOG_ERROR("NULL parameter: ptMachine");
        return ST_ERROR;
    }

    g_exec_ptMachine = ptMachine;
    g_exec_bInited   = ST_TRUE;
    LOG_INFO("execution engine initialised");
    return ST_NO_ERROR;
}

st_error_t exec_shutdown(void)
{
    LOG_TRACE("bOpen=%d", g_exec_bOpen);

    if (g_exec_bOpen)
    {
        exec_close();
    }

    g_exec_ptMachine = NULL;
    g_exec_bInited   = ST_FALSE;
    LOG_INFO("execution engine shutdown");
    return ST_NO_ERROR;
}

st_error_t exec_open(const char *szProgName, st_u32_t uiLoadAddr)
{
    exec_mon_view_t    *ptMonView;
    exec_cpu_view_t    *ptCpuView;
    exec_mem_view_t    *ptMemView;
    exec_asm_view_t    *ptAsmView;
    exec_screen_view_t *ptScrView;
    st_error_t          eResult;

    LOG_TRACE("szProgName=%s uiLoadAddr=$%06X",
              szProgName ? szProgName : "(null)", uiLoadAddr);

    if (!g_exec_bInited)
    {
        LOG_ERROR("exec_init() not called");
        return ST_ERROR;
    }

    /* Close any existing session */
    if (g_exec_bOpen)
    {
        exec_close();
    }

    /* Initialise shared state */
    memset(&g_exec_tState, 0, sizeof(g_exec_tState));
    memset(&g_exec_tCpu,   0, sizeof(g_exec_tCpu));

    eResult = platform_mutex_create(&g_exec_tState.ptMutex);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("platform_mutex_create failed");
        return ST_ERROR;
    }

    g_exec_tState.uiLoadAddr = uiLoadAddr;
    g_exec_tState.ptMachine  = g_exec_ptMachine;
    if (szProgName != NULL)
    {
        strncpy(g_exec_tState.szProgName, szProgName, ST_MAX_PATH - 1);
    }

    /* Initialise CPU at load address */
    cpu_init(&g_exec_tCpu, g_exec_ptMachine);
    g_exec_tCpu.uiPC   = uiLoadAddr;
    g_exec_tCpu.uiSSP  = ST_LOAD_BASE - 2u;
    g_exec_tCpu.auAn[7] = g_exec_tCpu.uiSSP;

    /* Prepare Line-A parameter block in RAM (non-fatal if fails) */
    linea_init(g_exec_ptMachine);

    /* Copy initial state to snapshot */
    memcpy(&g_exec_tState.tCpuSnap, &g_exec_tCpu, sizeof(cpu68k_t));
    g_exec_tState.eRunState = EXEC_RUN_PAUSED;

    /* Disassemble first instruction */
    platform_mutex_lock(g_exec_tState.ptMutex);
    exec_refresh_disasm(&g_exec_tState);
    platform_mutex_unlock(g_exec_tState.ptMutex);

    /* Set bOpen early so exec_get_state() works inside view opens.
     * Cleared on any error path below. */
    g_exec_bOpen = ST_TRUE;

    /* Open monitor window */
    ptMonView = NULL;
    eResult = exec_mon_open(&ptMonView);
    if (eResult != ST_NO_ERROR || ptMonView == NULL)
    {
        LOG_ERROR("exec_mon_open failed");
        g_exec_bOpen = ST_FALSE;
        platform_mutex_destroy(&g_exec_tState.ptMutex);
        return ST_ERROR;
    }
    g_exec_tState.hMonWnd = ptMonView->hWnd;

    /* Open CPU register view */
    ptCpuView = NULL;
    eResult = exec_cpu_open(&ptCpuView);
    if (eResult != ST_NO_ERROR || ptCpuView == NULL)
    {
        LOG_ERROR("exec_cpu_open failed");
        g_exec_bOpen = ST_FALSE;
        exec_mon_close(&ptMonView);
        platform_mutex_destroy(&g_exec_tState.ptMutex);
        return ST_ERROR;
    }
    g_exec_tState.hCpuWnd = ptCpuView->hWnd;

    /* Open memory dump view */
    ptMemView = NULL;
    eResult = exec_mem_open(&ptMemView);
    if (eResult != ST_NO_ERROR || ptMemView == NULL)
    {
        LOG_ERROR("exec_mem_open failed");
        g_exec_bOpen = ST_FALSE;
        exec_cpu_close(&ptCpuView);
        exec_mon_close(&ptMonView);
        platform_mutex_destroy(&g_exec_tState.ptMutex);
        return ST_ERROR;
    }
    g_exec_tState.hMemWnd = ptMemView->hWnd;

    /* Open disassembly view */
    ptAsmView = NULL;
    eResult = exec_asm_open(&ptAsmView);
    if (eResult != ST_NO_ERROR || ptAsmView == NULL)
    {
        LOG_ERROR("exec_asm_open failed");
        g_exec_bOpen = ST_FALSE;
        exec_mem_close(&ptMemView);
        exec_cpu_close(&ptCpuView);
        exec_mon_close(&ptMonView);
        platform_mutex_destroy(&g_exec_tState.ptMutex);
        return ST_ERROR;
    }
    g_exec_tState.hAsmWnd = ptAsmView->hWnd;

    /* Open screen emulation view */
    ptScrView = NULL;
    eResult = exec_screen_open(&ptScrView);
    if (eResult != ST_NO_ERROR || ptScrView == NULL)
    {
        LOG_ERROR("exec_screen_open failed");
        g_exec_bOpen = ST_FALSE;
        exec_asm_close(&ptAsmView);
        exec_mem_close(&ptMemView);
        exec_cpu_close(&ptCpuView);
        exec_mon_close(&ptMonView);
        platform_mutex_destroy(&g_exec_tState.ptMutex);
        return ST_ERROR;
    }
    g_exec_tState.hScrWnd = ptScrView->hWnd;

    /* Start execution thread */
    eResult = platform_thread_create(&g_exec_ptThread,
                                     exec_thread_fn,
                                     &g_exec_tState);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("platform_thread_create failed");
        g_exec_bOpen = ST_FALSE;
        exec_screen_close(&ptScrView);
        exec_asm_close(&ptAsmView);
        exec_mem_close(&ptMemView);
        exec_cpu_close(&ptCpuView);
        exec_mon_close(&ptMonView);
        platform_mutex_destroy(&g_exec_tState.ptMutex);
        return ST_ERROR;
    }

    /* g_exec_bOpen already ST_TRUE; session is now fully active */
    LOG_INFO("execution session opened: %s at $%06X",
             g_exec_tState.szProgName, uiLoadAddr);
    return ST_NO_ERROR;
}

st_error_t exec_close(void)
{
    gui_window_t hMonWnd;
    gui_window_t hCpuWnd;
    gui_window_t hMemWnd;
    gui_window_t hAsmWnd;
    gui_window_t hScrWnd;

    LOG_TRACE("bOpen=%d", g_exec_bOpen);

    if (!g_exec_bOpen)
    {
        return ST_NO_ERROR;
    }

    /* Signal execution thread to exit */
    platform_mutex_lock(g_exec_tState.ptMutex);
    g_exec_tState.bQuitReq = ST_TRUE;
    hMonWnd = g_exec_tState.hMonWnd;
    hCpuWnd = g_exec_tState.hCpuWnd;
    hMemWnd = g_exec_tState.hMemWnd;
    hAsmWnd = g_exec_tState.hAsmWnd;
    hScrWnd = g_exec_tState.hScrWnd;
    g_exec_tState.hMonWnd  = NULL;
    g_exec_tState.hCpuWnd  = NULL;
    g_exec_tState.hMemWnd  = NULL;
    g_exec_tState.hAsmWnd  = NULL;
    g_exec_tState.hScrWnd  = NULL;
    platform_mutex_unlock(g_exec_tState.ptMutex);

    /* Join execution thread */
    if (g_exec_ptThread != NULL)
    {
        platform_thread_join(g_exec_ptThread);
        platform_thread_destroy(&g_exec_ptThread);
    }

    /* Close windows */
    if (hMonWnd != NULL)
    {
        gui_close_window(hMonWnd);
    }
    if (hCpuWnd != NULL)
    {
        gui_close_window(hCpuWnd);
    }
    if (hMemWnd != NULL)
    {
        gui_close_window(hMemWnd);
    }
    if (hAsmWnd != NULL)
    {
        gui_close_window(hAsmWnd);
    }
    if (hScrWnd != NULL)
    {
        gui_close_window(hScrWnd);
    }

    /* Release mutex */
    if (g_exec_tState.ptMutex != NULL)
    {
        platform_mutex_destroy(&g_exec_tState.ptMutex);
    }

    g_exec_bOpen = ST_FALSE;
    LOG_INFO("execution session closed");
    return ST_NO_ERROR;
}

st_bool_t exec_is_open(void)
{
    return g_exec_bOpen;
}

exec_state_t *exec_get_state(void)
{
    if (!g_exec_bOpen)
    {
        return NULL;
    }
    return &g_exec_tState;
}

/* ------------------------------------------------------------------
 * Thread-safe control API
 * ------------------------------------------------------------------ */

st_error_t exec_step_request(void)
{
    if (!g_exec_bOpen)
    {
        LOG_ERROR("no execution session open");
        return ST_ERROR;
    }
    platform_mutex_lock(g_exec_tState.ptMutex);
    if (g_exec_tState.eRunState != EXEC_RUN_HALTED)
    {
        g_exec_tState.bStepReq = ST_TRUE;
    }
    platform_mutex_unlock(g_exec_tState.ptMutex);
    return ST_NO_ERROR;
}

st_error_t exec_run_request(void)
{
    if (!g_exec_bOpen)
    {
        LOG_ERROR("no execution session open");
        return ST_ERROR;
    }
    platform_mutex_lock(g_exec_tState.ptMutex);
    if (g_exec_tState.eRunState != EXEC_RUN_HALTED)
    {
        g_exec_tState.bRunReq = ST_TRUE;
    }
    platform_mutex_unlock(g_exec_tState.ptMutex);
    return ST_NO_ERROR;
}

st_error_t exec_pause_request(void)
{
    if (!g_exec_bOpen)
    {
        LOG_ERROR("no execution session open");
        return ST_ERROR;
    }
    platform_mutex_lock(g_exec_tState.ptMutex);
    g_exec_tState.bPauseReq = ST_TRUE;
    platform_mutex_unlock(g_exec_tState.ptMutex);
    return ST_NO_ERROR;
}

st_error_t exec_stop_request(void)
{
    if (!g_exec_bOpen)
    {
        LOG_ERROR("no execution session open");
        return ST_ERROR;
    }
    platform_mutex_lock(g_exec_tState.ptMutex);
    g_exec_tState.bStopReq = ST_TRUE;
    platform_mutex_unlock(g_exec_tState.ptMutex);
    return ST_NO_ERROR;
}

st_error_t exec_quit_request(void)
{
    if (!g_exec_bOpen)
    {
        LOG_ERROR("no execution session open");
        return ST_ERROR;
    }
    platform_mutex_lock(g_exec_tState.ptMutex);
    g_exec_tState.bQuitReq = ST_TRUE;
    platform_mutex_unlock(g_exec_tState.ptMutex);
    return ST_NO_ERROR;
}

st_error_t exec_bp_toggle(st_u32_t uiAddr)
{
    int iIdx;

    if (!g_exec_bOpen)
    {
        LOG_ERROR("no execution session open");
        return ST_ERROR;
    }

    platform_mutex_lock(g_exec_tState.ptMutex);

    iIdx = exec_bp_find(&g_exec_tState, uiAddr);
    if (iIdx >= 0)
    {
        /* Remove: shift remaining entries down */
        int j;
        for (j = iIdx; j < g_exec_tState.iBPCount - 1; j++)
        {
            g_exec_tState.auBP[j] = g_exec_tState.auBP[j + 1];
        }
        g_exec_tState.iBPCount--;
        LOG_INFO("breakpoint removed at $%06X", uiAddr);
    }
    else if (g_exec_tState.iBPCount < EXEC_BP_MAX)
    {
        g_exec_tState.auBP[g_exec_tState.iBPCount] = uiAddr;
        g_exec_tState.iBPCount++;
        LOG_INFO("breakpoint set at $%06X", uiAddr);
    }
    else
    {
        platform_mutex_unlock(g_exec_tState.ptMutex);
        LOG_ERROR("breakpoint list full (%d/%d)", EXEC_BP_MAX, EXEC_BP_MAX);
        return ST_ERROR;
    }

    platform_mutex_unlock(g_exec_tState.ptMutex);
    return ST_NO_ERROR;
}

st_error_t exec_bp_clear(void)
{
    if (!g_exec_bOpen)
    {
        LOG_ERROR("no execution session open");
        return ST_ERROR;
    }
    platform_mutex_lock(g_exec_tState.ptMutex);
    g_exec_tState.iBPCount = 0;
    platform_mutex_unlock(g_exec_tState.ptMutex);
    LOG_INFO("all breakpoints cleared");
    return ST_NO_ERROR;
}
