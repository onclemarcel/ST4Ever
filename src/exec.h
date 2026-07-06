/*
 * exec.h - ST4Ever Execution engine
 *
 * Manages a step/run CPU execution session over the emulated Atari ST
 * machine.  Exposes a shared exec_state_t (protected by ptMutex) that
 * exec_mon (monitor window) and exec_cpu (register view) read to render
 * the current execution context.
 *
 * Thread model:
 *   exec_open() starts a dedicated CPU execution thread and two GUI
 *   window threads (GUI_WND_EXEC_MON, GUI_WND_EXEC_CPU).
 *   The exec thread owns the live cpu68k_t; after each instruction it
 *   copies registers into exec_state_t.tCpuSnap (under ptMutex) and
 *   calls gui_invalidate() on both windows.
 *   The monitor window posts control requests (bStepReq, bRunReq, ...)
 *   into exec_state_t under ptMutex; the exec thread reads and clears
 *   them each iteration.
 *
 * Lifecycle (main.c):
 *   exec_init(&tMachine)
 *   ... exec_open() / exec_close() may be called multiple times ...
 *   exec_shutdown()
 *
 * UC25A: exec_open/close, step/run/pause/stop/breakpoint.
 * UC25B: memory view (exec_mem) + disassembly view (exec_asm).
 */

#ifndef EXEC_H
#define EXEC_H

#include "common.h"
#include "gui.h"
#include "CPU.h"
#include "ST.h"
#include "disassemble.h"

/* ------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------ */

#define EXEC_BP_MAX     8    /* max simultaneous breakpoints         */
#define EXEC_QUANTUM    200  /* max instructions per run-mode slice  */

/* ------------------------------------------------------------------
 * Execution run-mode state
 * ------------------------------------------------------------------ */

typedef enum exec_run_state_e
{
    EXEC_RUN_IDLE    = 0,  /* engine not started                    */
    EXEC_RUN_PAUSED  = 1,  /* paused, awaiting user command         */
    EXEC_RUN_RUNNING = 2,  /* running continuously                  */
    EXEC_RUN_HALTED  = 3   /* CPU halted (double bus / bad state)   */
} exec_run_state_t;

/* ------------------------------------------------------------------
 * Shared execution state
 * All fields are protected by ptMutex.
 * ------------------------------------------------------------------ */

typedef struct exec_state_s
{
    /* CPU snapshot — updated after each step */
    cpu_context_t    *tCpuSnap;
    cpu_step_result_t tLastResult;
    exec_run_state_t  eRunState;
    /* Disassembly of the NEXT instruction to execute (at tCpuSnap.uiPC) */
    char   szNextDisasm[DISASM_LINE_MAX];

    /* Breakpoints */
    st_u32_t  auBP[EXEC_BP_MAX];
    int       iBPCount;

    /* Control flags (set by monitor thread, cleared by exec thread) */
    st_bool_t bStepReq;   /* execute 1 instruction then pause */
    st_bool_t bRunReq;    /* transition to RUNNING             */
    st_bool_t bPauseReq;  /* transition to PAUSED              */
    st_bool_t bStopReq;   /* reset PC to uiLoadAddr + PAUSED   */
    st_bool_t bQuitReq;   /* exit the execution loop           */

    /* Mutex protecting all fields above */
    st_mutex_t *ptMutex;

    /* Window handles (for gui_invalidate; set once in exec_open) */
    gui_window_t hMonWnd;
    gui_window_t hCpuWnd;
    gui_window_t hMemWnd;   /* memory dump view (UC25B)              */
    gui_window_t hAsmWnd;   /* disassembly view (UC25B)              */
    gui_window_t hScrWnd;   /* screen emulation view (UC27)          */

    /* ST machine (read-only after exec_open; no mutex needed) */
    st_machine_context_t *ptMachine;

    /* Program info (set once in exec_open, never modified after) */
    st_u32_t uiLoadAddr;
    char     szProgName[ST_MAX_PATH];
} exec_state_t;

/* ------------------------------------------------------------------
 * Exec command context
 * ------------------------------------------------------------------ */

typedef struct exec_context_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */
    
    st_bool_t               bInitOK;
    st_bool_t               bOpen;
    st_bool_t               bIsMachineOn;
    st_machine_context_t   *ptMachine;
    st_thread_t            *ptThread;  
    exec_state_t            tState;
    cpu_context_t          *tCpu;   /* owned exclusively by exec thread */
} exec_context_t;

/* ------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------ */

/*
 * exec_init() - Attach execution engine to the ST machine.
 *
 * Must be called once from main.c after st_init().
 *
 * Parameters:
 *   ptMachine [in] : Initialised ST machine (must not be NULL).
 *
 * Returns:
 *   Value of the global load_context_t structure pointer on success.
 *   ST_ERROR is ST Machine context is not initialized
 */
st_u64_t exec_init(st_u64_t ulSTMachineCtx);

/*
 * exec_shutdown() - Release execution engine resources.
 *
 * Closes any open session (calls exec_close() internally).
 * Must be called from main.c before st_shutdown().
 *
 * Returns:
 *   ST_NO_ERROR always.
 */
st_error_t exec_shutdown(void);

/*
 * exec_open() - Start a new execution session.
 *
 * Initialises the CPU at uiLoadAddr, opens the monitor window
 * (GUI_WND_EXEC_MON) and CPU register view (GUI_WND_EXEC_CPU),
 * and starts the execution thread paused.
 * If a session is already open, closes it first.
 *
 * Parameters:
 *   szProgName [in] : Display name shown in window title.
 *   uiLoadAddr [in] : Initial PC (.text start in ST RAM).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if exec_init() was not called or window/thread fails.
 */
st_error_t exec_open(const char *szProgName, st_u32_t uiLoadAddr);

/*
 * exec_close() - Stop the execution session and release resources.
 *
 * Signals the exec thread to exit, joins it, closes both windows.
 * Safe to call when no session is open (returns ST_NO_ERROR).
 *
 * Returns:
 *   ST_NO_ERROR always.
 */
st_error_t exec_close(void);

/*
 * exec_is_open() - Return ST_TRUE if an execution session is active.
 */
st_bool_t exec_is_open(void);

/*
 * exec_get_state() - Return pointer to the shared execution state.
 *
 * Valid between exec_open() and exec_close().
 * Callers must lock ptMutex before accessing any field.
 * Returns NULL if no session is open.
 */
exec_state_t *exec_get_state(void);

/* ------------------------------------------------------------------
 * Thread-safe control API  (used by exec_mon)
 * ------------------------------------------------------------------ */

/*
 * exec_step_request() - Request execution of exactly one instruction.
 *
 * Transitions state to RUNNING for one instruction then back to
 * PAUSED.  Ignored if state is HALTED.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if session not open.
 */
st_error_t exec_step_request(void);

/*
 * exec_run_request() - Request continuous execution.
 *
 * Transitions state from PAUSED to RUNNING.
 * Ignored if already RUNNING or HALTED.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if session not open.
 */
st_error_t exec_run_request(void);

/*
 * exec_pause_request() - Request a pause while running.
 *
 * Transitions state from RUNNING to PAUSED after the current
 * instruction batch completes.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if session not open.
 */
st_error_t exec_pause_request(void);

/*
 * exec_stop_request() - Reset CPU to uiLoadAddr and enter PAUSED.
 *
 * Re-initialises the CPU register file.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if session not open.
 */
st_error_t exec_stop_request(void);

/*
 * exec_quit_request() - Signal the execution loop to exit.
 *
 * The exec thread exits after the current instruction batch.
 * Call exec_close() to join and free resources.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if session not open.
 */
st_error_t exec_quit_request(void);

/*
 * exec_bp_toggle() - Add or remove a breakpoint at uiAddr.
 *
 * If uiAddr is already in the list, removes it.
 * Otherwise adds it (fails if list is full).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if no session or breakpoints list full.
 */
st_error_t exec_bp_toggle(st_u32_t uiAddr);

/*
 * exec_bp_clear() - Remove all breakpoints.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if session not open.
 */
st_error_t exec_bp_clear(void);

#endif /* EXEC_H */
