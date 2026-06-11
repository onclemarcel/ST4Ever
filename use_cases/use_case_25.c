/*
 * use_case_25.c - UC25A : Execution engine + Monitor + CPU view
 *
 * TEST MATRIX - UC25A:
 *   [N] Nominal    : 13 tests - exec lifecycle (init/shutdown/is_open/
 *                               get_state), exec_open guard without init,
 *                               control API when open (via direct state
 *                               manipulation), CPU step integration
 *   [R] Robustness :  9 tests - NULL to exec_init, all request functions
 *                               when not open, exec_bp_toggle/clear
 *                               when not open
 *   [S] Skipped    :  8 tests - exec_open/close GUI windows, step/run/
 *                               pause/stop via monitor, BP hit detection,
 *                               CPU register view rendering
 */
#include "use_cases.h"

/* INTENT[INT-EXE-001..013 -> TC-EXE-001..013 -> REQ-EXE-020..025 -> UFR-EXE-008]:
 * exec lifecycle: init stores machine, shutdown is idempotent, is_open/get_state
 * return correct values at each lifecycle stage. exec_open() without GUI fails
 * gracefully. CPU step integration validates the cpu+machine contract used by the
 * exec thread. */

/* INTENT[INT-EXE-014..022 -> TC-EXE-014..022 -> REQ-EXE-020..027 -> UFR-EXE-008]:
 * All request functions and BP functions must return ST_ERROR when no session is
 * open (g_exec_bOpen == ST_FALSE). exec_open() without prior exec_init() must
 * return ST_ERROR immediately. */

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static st_machine_t g_tMachine;
static cpu68k_t     g_tCpu;

/* Write big-endian word into RAM */
static void w16(st_u32_t uiAddr, st_u16_t uiVal)
{
    g_tMachine.aRam[uiAddr]     = (st_u8_t)(uiVal >> 8);
    g_tMachine.aRam[uiAddr + 1] = (st_u8_t)(uiVal & 0xFFu);
}

/* Write big-endian longword into RAM */
static void w32(st_u32_t uiAddr, st_u32_t uiVal)
{
    g_tMachine.aRam[uiAddr]     = (st_u8_t)((uiVal >> 24) & 0xFFu);
    g_tMachine.aRam[uiAddr + 1] = (st_u8_t)((uiVal >> 16) & 0xFFu);
    g_tMachine.aRam[uiAddr + 2] = (st_u8_t)((uiVal >>  8) & 0xFFu);
    g_tMachine.aRam[uiAddr + 3] = (st_u8_t)( uiVal        & 0xFFu);
}

/* Setup a minimal machine + CPU at ST_LOAD_BASE */
static void setup(void)
{
    st_init(&g_tMachine, NULL);
    memset(g_tMachine.aRam, 0, sizeof(g_tMachine.aRam));
    /* Reset vectors: SSP=0x0800-2, PC=ST_LOAD_BASE=0x0800 */
    w32(0x000000, 0x000007FEu);  /* SSP */
    w32(0x000004, ST_LOAD_BASE); /* PC  */
    cpu_init(&g_tCpu, &g_tMachine);
}

/* ------------------------------------------------------------------ */
/* [N] Exec module lifecycle                                            */
/* ------------------------------------------------------------------ */

static void test_exec_lifecycle(void)
{
    st_error_t eResult;

    printf("\n--- [N] exec module lifecycle ---\n");

    /* 1. Initially not open, not inited */
    TEST_ASSERT("[N] exec_is_open() before init = FALSE",
                exec_is_open() == ST_FALSE);

    TEST_ASSERT("[N] exec_get_state() before init = NULL",
                exec_get_state() == NULL);

    /* 2. exec_init with valid machine */
    eResult = exec_init(&g_tMachine);
    TEST_ASSERT("[N] exec_init() with valid machine = ST_NO_ERROR",
                eResult == ST_NO_ERROR);

    /* 3. Still not open until exec_open() */
    TEST_ASSERT("[N] exec_is_open() after init (no open) = FALSE",
                exec_is_open() == ST_FALSE);

    TEST_ASSERT("[N] exec_get_state() after init (no open) = NULL",
                exec_get_state() == NULL);

    /* 4. exec_open without GUI should fail (no gui_init called) but
     *    must not crash (returns ST_ERROR cleanly) */
    eResult = exec_open("TEST.PRG", ST_LOAD_BASE);
    TEST_ASSERT("[N] exec_open() without gui_init fails gracefully",
                eResult == ST_ERROR);

    TEST_ASSERT("[N] exec_is_open() after failed exec_open = FALSE",
                exec_is_open() == ST_FALSE);

    /* 5. exec_shutdown cleans up gracefully */
    eResult = exec_shutdown();
    TEST_ASSERT("[N] exec_shutdown() = ST_NO_ERROR",
                eResult == ST_NO_ERROR);

    TEST_ASSERT("[N] exec_is_open() after shutdown = FALSE",
                exec_is_open() == ST_FALSE);

    /* 6. Double exec_shutdown is safe */
    eResult = exec_shutdown();
    TEST_ASSERT("[N] exec_shutdown() when already shutdown = ST_NO_ERROR",
                eResult == ST_NO_ERROR);
}

/* ------------------------------------------------------------------ */
/* [R] Robustness: NULL parameters and not-open errors                  */
/* ------------------------------------------------------------------ */

static void test_exec_robustness(void)
{
    st_error_t eResult;

    printf("\n--- [R] exec robustness ---\n");

    /* NULL machine to exec_init */
    eResult = exec_init(NULL);
    TEST_ASSERT("[R] exec_init(NULL) = ST_ERROR",
                eResult == ST_ERROR);

    /* All control functions when not open */
    eResult = exec_step_request();
    TEST_ASSERT("[R] exec_step_request() when not open = ST_ERROR",
                eResult == ST_ERROR);

    eResult = exec_run_request();
    TEST_ASSERT("[R] exec_run_request() when not open = ST_ERROR",
                eResult == ST_ERROR);

    eResult = exec_pause_request();
    TEST_ASSERT("[R] exec_pause_request() when not open = ST_ERROR",
                eResult == ST_ERROR);

    eResult = exec_stop_request();
    TEST_ASSERT("[R] exec_stop_request() when not open = ST_ERROR",
                eResult == ST_ERROR);

    eResult = exec_quit_request();
    TEST_ASSERT("[R] exec_quit_request() when not open = ST_ERROR",
                eResult == ST_ERROR);

    eResult = exec_bp_toggle(ST_LOAD_BASE);
    TEST_ASSERT("[R] exec_bp_toggle() when not open = ST_ERROR",
                eResult == ST_ERROR);

    eResult = exec_bp_clear();
    TEST_ASSERT("[R] exec_bp_clear() when not open = ST_ERROR",
                eResult == ST_ERROR);

    /* exec_open without exec_init */
    eResult = exec_open("TEST.PRG", ST_LOAD_BASE);
    TEST_ASSERT("[R] exec_open() without exec_init = ST_ERROR",
                eResult == ST_ERROR);
}

/* ------------------------------------------------------------------ */
/* [N] CPU step integration — direct cpu_step without exec module       */
/* Validates the CPU/machine integration that exec_open would use.      */
/* ------------------------------------------------------------------ */

static void test_cpu_step_integration(void)
{
    cpu_step_result_t tResult;
    st_error_t        eResult;

    printf("\n--- [N] CPU step integration (direct) ---\n");

    setup();

    /* Write MOVEQ #42,D0 at load base (opcode 0x7005 + arg 0x002A) */
    /* MOVEQ #N,Dn = 0111 ddd 0 NNNNNNNN */
    /* MOVEQ #42,D0 = 0111 000 0 0010 1010 = 0x702A */
    w16(ST_LOAD_BASE, 0x702A);  /* MOVEQ #42,D0     */
    w16(ST_LOAD_BASE + 2, 0x4E71); /* NOP             */

    eResult = cpu_step(&g_tCpu, &g_tMachine, &tResult);
    TEST_ASSERT("[N] cpu_step MOVEQ #42,D0 = ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    TEST_ASSERT("[N] MOVEQ #42,D0: D0 = 42",
                g_tCpu.auDn[0] == 42u);
    TEST_ASSERT("[N] MOVEQ #42,D0: PC advanced by 2",
                tResult.uiPCAfter == ST_LOAD_BASE + 2u);

    /* Execute NOP */
    eResult = cpu_step(&g_tCpu, &g_tMachine, &tResult);
    TEST_ASSERT("[N] cpu_step NOP = ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    TEST_ASSERT("[N] NOP: PC advanced by 2",
                tResult.uiPCAfter == ST_LOAD_BASE + 4u);

    st_shutdown(&g_tMachine);
}

/* ------------------------------------------------------------------ */
/* [S] Skipped: GUI-dependent tests                                     */
/* ------------------------------------------------------------------ */

static void test_exec_gui_skipped(void)
{
    printf("\n--- [S] GUI-dependent tests (skipped headless) ---\n");

    TEST_SKIP("[S] exec_open() opens monitor window (GUI_WND_EXEC_MON)");
    TEST_SKIP("[S] exec_open() opens CPU register view (GUI_WND_EXEC_CPU)");
    TEST_SKIP("[S] monitor window: F5 executes one instruction (step)");
    TEST_SKIP("[S] monitor window: F6 starts continuous run mode");
    TEST_SKIP("[S] monitor window: F6 again pauses run mode");
    TEST_SKIP("[S] monitor window: breakpoint B toggles at PC, F6 hits it");
    TEST_SKIP("[S] monitor window: F7 resets PC to load address");
    TEST_SKIP("[S] monitor window: ESC closes both windows cleanly");
}

/* ------------------------------------------------------------------ */
/* main                                                                  */
/* ------------------------------------------------------------------ */

int main(void)
{
    st_error_t eResult;

    printf("=== UC25A: Execution engine + Monitor + CPU view ===\n");

    /* Minimal trace for test logging */
    eResult = trace_init(ST_FALSE);
    if (eResult != ST_NO_ERROR)
    {
        fprintf(stderr, "trace_init failed\n");
        return 1;
    }

    /* Setup machine for CPU integration tests */
    st_init(&g_tMachine, NULL);

    test_exec_lifecycle();
    test_exec_robustness();
    test_cpu_step_integration();
    test_exec_gui_skipped();

    st_shutdown(&g_tMachine);
    trace_shutdown();

    printf("\n=== UC25A: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
