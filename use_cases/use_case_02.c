/*
 * use_case_02.c - UC2 Validation: trace on/off/toggle command
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * Strategy: line_cmd_trace() and line_dispatch() are static; direct
 * calls from a test binary are not possible.  UC2 validates behavioral
 * contracts by driving the public trace API in the exact sequences
 * used by `trace`, `trace on`, and `trace off`.  Command-dispatch
 * tests (full stdin path) are [S] Skipped and validated manually
 * with `make manual`.
 *
 * R23/R24/R25: migrated from a free-standing trace_init()/trace_gui_open()
 * driver to the ST4Ever_init() context-access pattern (R24 §"Accès aux
 * contextes de modules") - ptCtx->ptTraceCtx is captured once in main()
 * and reused by every group, exactly like use_case_01.c's uc01_test_trace().
 * INT-TRC-xxx/TC-TRC-xxx numbering continues from the highest id used in
 * use_case_00.c/use_case_01.c (INT-TRC-008, TC-TRC-025) - the only two
 * other files already migrated to R24 at the time of writing.  Files not
 * yet migrated (use_case_04_4.c, use_case_05.c, ...) will be renumbered
 * into this same continuous sequence when their turn comes; REQ-xxx/
 * UFR-xxx traceability is deferred to that later cleanup pass too.
 *
 * TEST MATRIX:
 *   [N] Nominal    : 12 tests  - toggle, on, off, state consistency
 *   [R] Robustness :  4 tests  - idempotent open/close, level isolation
 *   [S] Skipped    :  4 tests  - command dispatch via stdin (manual)
 *
 * Test groups:
 *   Group 1: trace (toggle)   - open when closed, close when open,
 *                               trace_enabled state preserved
 *   Group 2: trace on         - open + enable LOG_TRACE, idempotent
 *   Group 3: trace off        - disable LOG_TRACE only; view stays open (P19)
 *   Group 4: state consistency - LOG levels unaffected by trace off
 *   Group 5: skipped tests    - manual stdin dispatch validation
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"

/* Required by the UC_TEST / UC_CHECK macros in use_cases.h */
int       g_uc_fails = 0;
st_bool_t gIsObject  = ST_FALSE;

static ST4Ever_context_t *ptCtx;

/* ------------------------------------------------------------------
 * Group 1: trace (toggle - no argument)
 * ------------------------------------------------------------------ */

/*
 * uc02_test_trace_toggle() - `trace` (no argument) toggle behaviour
 *
 * Code Coverage:
 *   trace.c:
 *   -- [TRACE]12. Calling trace_gui_open() when GUI is open is harmless --
 *   -- [TRACE]13. Allocate a new trace_view structure --
 *   -- [TRACE]14. Open the GUI window --
 *   -- [TRACE]15. Do not close if trace module is not initialized --
 *   -- [TRACE]16. Close the GUI window --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc02_test_trace_toggle(void)
{
    trace_context_t* ptTraceCtx = (trace_context_t*)ptCtx->ptTraceCtx;

    printf("\n--- Test group 1: trace toggle (no argument) ---\n");

    /* -- [TRACE]13. Allocate a new trace_view structure -- */
    /* -- [TRACE]14. Open the GUI window -- */
    /* INTENT[INT-TRC-009 → TC-TRC-026...027 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * `trace` (no arg) must open the GUI window (allocate the trace
     * view structure) when the console is closed.*/
    UC_TEST("(INT-TRC-009) [Chk] console closed at startup (no -t/--trace)",
            ptTraceCtx->bOpen == ST_FALSE);
    UC_CHECK("(INT-TRC-009) [Chk] trace_gui_open()  [toggle: closed -> open]",
            trace_gui_open());
    UC_TEST("[N] (TC-TRC-026) GUI is created with a valid pointer",
            ptTraceCtx->ptView != NULL);        
    UC_TEST("[N] (TC-TRC-027) trace_is_open() == TRUE after toggle-open",
            ptTraceCtx->bOpen == ST_TRUE);

    /* -- [TRACE]15. Do not close if trace module is not initialized -- */
    /* -- [TRACE]16. Close the GUI window -- */
    /* INTENT[INT-TRC-010 → TC-TRC-028 → REQ-TRC-006,007 → UFR-CON-031]:
     * `trace` (no arg) must close the console when it is open. */
    UC_CHECK("(INT-TRC-010) [Chk] trace_gui_close()  [toggle: open -> closed]",
             trace_gui_close());
    UC_TEST("[N] (TC-TRC-028) trace_is_open() == FALSE after toggle-close",
            ptTraceCtx->bOpen == ST_FALSE);
    UC_TEST("[N] (TC-TRC-029) GUI is closed, its pointer in NULL",
            ptTraceCtx->ptView == NULL);        

    /* -- [TRACE]12. Calling trace_gui_open() when GUI is open is harmless -- */
    /* INTENT[INT-TRC-011 → TC-TRC-029,030 → REQ-TRC-011 → UFR-CON-032]:
     * toggle must not change the trace_enabled state in either
     * direction; calling trace_gui_open() again while the GUI window
     * is already open must stay harmless (line_cmd_trace no-arg path
     * never calls trace_set_trace_enabled) */
    trace_set_trace_enabled(ST_TRUE);
    UC_CHECK("(INT-TRC-011) [Chk] trace_gui_open()  [toggle: verify enabled unchanged]",
             trace_gui_open());
    UC_TEST("[N] (TC-TRC-029) trace_is_trace_enabled() unchanged after toggle-open",
            trace_is_trace_enabled() == ST_TRUE);
    UC_CHECK("(INT-TRC-011) [Chk] trace_gui_close()  [toggle: verify enabled unchanged]",
             trace_gui_close());
    UC_TEST("[N] (TC-TRC-030) trace_is_trace_enabled() unchanged after toggle-close",
            trace_is_trace_enabled() == ST_TRUE);

    /* Restore ST_FALSE precondition for group 2 (which re-enables it) */
    trace_set_trace_enabled(ST_FALSE);
}

/* ------------------------------------------------------------------
 * Group 2: trace on
 * ------------------------------------------------------------------ */

/*
 * uc02_test_trace_on() - `trace on` opens the console and enables
 *                        LOG_TRACE output
 *
 * Code Coverage:
 *   trace.c:
 *   -- [TRACE]12. Calling trace_gui_open() when GUI is open is harmless --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc02_test_trace_on(void)
{
    printf("\n--- Test group 2: trace on ---\n");

    /* Precondition: console closed, trace_enabled FALSE (from group 1).
     * Simulates the sequence: trace_gui_open() + trace_set_trace_enabled */

    /* INTENT[INT-TRC-012 → TC-TRC-031 → REQ-TRC-008 → UFR-CON-030]:
     * `trace on` must open the trace console */
    UC_CHECK("(INT-TRC-012) [Chk] trace_gui_open()  [trace on: open console]",
             trace_gui_open());
    UC_TEST("[N] (TC-TRC-031) trace_is_open() == TRUE after trace on",
            trace_is_open() == ST_TRUE);

    /* INTENT[INT-TRC-013 → TC-TRC-032 → REQ-TRC-010 → UFR-TRC-006]:
     * `trace on` must activate LOG_TRACE output */
    trace_set_trace_enabled(ST_TRUE);
    UC_TEST("[N] (TC-TRC-032) trace_is_trace_enabled() == TRUE after trace on",
            trace_is_trace_enabled() == ST_TRUE);

    /* Verify LOG_TRACE is now active */
    LOG_TRACE("LOG_TRACE active: this must appear in st4ever_trace.log");

    /* -- [TRACE]12. Calling trace_gui_open() when GUI is open is harmless -- */
    /* INTENT[INT-TRC-014 → TC-TRC-033,034 → REQ-TRC-008,017 → UFR-TRC-007]:
     * `trace on` when the GUI window is already open must stay harmless:
     * calling trace_gui_open() a second time is idempotent (UC1/UC4.4) */
    UC_TEST("[R] (TC-TRC-033) trace_gui_open() idempotent: already open -> ST_NO_ERROR",
            trace_gui_open() == ST_NO_ERROR);
    UC_TEST("[R] (TC-TRC-034) trace_is_open() still TRUE after second open",
            trace_is_open() == ST_TRUE);
}

/* ------------------------------------------------------------------
 * Group 3: trace off
 * ------------------------------------------------------------------ */

/*
 * uc02_test_trace_off() - `trace off` disables LOG_TRACE without
 *                         closing the console (P19)
 *
 * Code Coverage:
 *   trace.c: (no additional source tags - trace_set_trace_enabled() is
 *             a plain setter with no [TRACE]N tag; see [TRACE]11..16
 *             already exercised in Group 1 for the open/close lifecycle)
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc02_test_trace_off(void)
{
    printf("\n--- Test group 3: trace off ---\n");

    /* Precondition: console open, trace_enabled TRUE (from group 2).
     * ADAPTED: P19 - trace off no longer calls trace_gui_close().
     * Simulates: trace_set_trace_enabled(FALSE) only; view stays open. */

    /* INTENT[INT-TRC-015 → TC-TRC-035 → REQ-TRC-009 → UFR-TRC-006]:
     * `trace off` must disable LOG_TRACE output */
    trace_set_trace_enabled(ST_FALSE);
    UC_TEST("[N] (TC-TRC-035) trace_is_trace_enabled() == FALSE after trace off",
            trace_is_trace_enabled() == ST_FALSE);

    /* ADAPTED: P19 - trace off must NOT close the console.
     * INTENT[INT-TRC-016 → TC-TRC-036 → REQ-TRC-016 → UFR-CON-031]:
     * `trace off` must leave the trace view open (filter only) */
    UC_TEST("[N] (TC-TRC-036) trace_is_open() == TRUE after trace off (P19: view stays open)",
            trace_is_open() == ST_TRUE);

    /* ADAPTED: P19 - idempotency now covers a second disable, not
     * trace_gui_close() (which is no longer called by trace off).
     * INTENT[INT-TRC-017 → TC-TRC-037,038 → REQ-TRC-016 → UFR-CON-031]:
     * `trace off` twice is harmless; view remains open */
    trace_set_trace_enabled(ST_FALSE);
    UC_TEST("[R] (TC-TRC-037) trace_set_trace_enabled(FALSE) idempotent (no crash)",
            trace_is_trace_enabled() == ST_FALSE);
    UC_TEST("[R] (TC-TRC-038) trace_is_open() still TRUE after second trace off",
            trace_is_open() == ST_TRUE);
}

/* ------------------------------------------------------------------
 * Group 4: state consistency after trace off
 * ------------------------------------------------------------------ */

/*
 * uc02_test_state_consistency() - LOG_INFO/ERROR/TODO stay active while
 *                                 LOG_TRACE is filtered; re-enable works
 *
 * Code Coverage:
 *   trace.c: (no additional source tags - LOG_TRACE/LOG_INFO/LOG_ERROR/
 *             LOG_TODO routing through trace_log() is already tagged
 *             [TRACE]5..10 and exercised in use_case_01.c's Group 1)
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc02_test_state_consistency(void)
{
    printf("\n--- Test group 4: state consistency ---\n");

    /* Precondition: console OPEN, trace_enabled FALSE (from group 3 — P19) */

    /* INTENT[INT-TRC-018 → TC-TRC-039 → REQ-TRC-004,009 → UFR-TRC-006]:
     * After trace off, LOG_INFO / LOG_ERROR / LOG_TODO must still
     * emit to the log file without crash.  LOG_TRACE must be silent
     * (trace_enabled FALSE). */
    LOG_TRACE("THIS LINE MUST NOT APPEAR (trace_enabled is FALSE)");
    LOG_INFO("LOG_INFO active: emitting to st4ever_trace.log");
    LOG_ERROR("LOG_ERROR active: emitting (not a real error)");
    LOG_TODO("LOG_TODO active: emitting");
    UC_TEST("[R] (TC-TRC-039) LOG_INFO/ERROR/TODO emitted without crash while"
            " trace_enabled is FALSE", trace_is_trace_enabled() == ST_FALSE);

    /* INTENT[INT-TRC-019 → TC-TRC-040 → REQ-TRC-010 → UFR-TRC-006]:
     * LOG_TRACE must become active again after trace_set_trace_enabled */
    trace_set_trace_enabled(ST_TRUE);
    UC_TEST("[N] (TC-TRC-040) trace_is_trace_enabled() == TRUE after re-enable",
            trace_is_trace_enabled() == ST_TRUE);
    LOG_TRACE("This LOG_TRACE MUST appear after re-enable");
}

/* ------------------------------------------------------------------
 * Group 5: skipped - command dispatch via stdin
 * ------------------------------------------------------------------ */

/*
 * uc02_test_skipped() - Manual validation of `trace`/`trace on`/`trace off`
 *                       dispatch through the interactive console
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc02_test_skipped(void)
{
    printf("\n--- Test group 5: command dispatch (manual via make manual) ---\n");

    TEST_MANUAL("[S] 'trace' toggles the trace window",
                "In a second terminal run './release/st4ever.exe', type "
                "'trace' ENTER: did trace open? Type 'trace' again: "
                "did it close?");
    TEST_MANUAL("[S] 'trace on' opens trace and enables LOG_TRACE",
                "Type 'trace on': trace open and LOG_TRACE lines visible? "
                "Type 'trace off': LOG_TRACE filtered, console still open?");
    TEST_MANUAL("[S] 'trace off' is idempotent when LOG_TRACE already disabled",
                "Type 'trace off' twice: no error, console still open?");
    TEST_MANUAL("[S] 'trace <bad>' shows warning with no state change",
                "Type 'trace xyz': warning shown, trace state unchanged?");
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    char *args[] = {"UC02"};

    printf("\n");
    printf("============================================================"
           "====\n");
    printf(" UC2 - trace on / off / toggle command validation\n");
    printf("============================================================"
           "====\n");

    ptCtx = (ST4Ever_context_t *)ST4Ever_init(1, args);
    UC_CHECK("(UC02) [Chk] Launch ST4Ever with no argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject)
    {
        uc02_test_trace_toggle();
        uc02_test_trace_on();
        uc02_test_trace_off();
        uc02_test_state_consistency();
        uc02_test_skipped();
    }
    else
    {
        printf("  [SKIP] (UC02) ST_MAIN_CTX Object Check failed\n\n");
    }

    printf("\n--- Shutdown ---\n");
    ST4Ever_shutdown();
    printf("  [INFO] trace_shutdown() complete - "
           "see st4ever_trace.log for full trace\n");

    printf("\n");
    printf("============================================================"
           "====\n");
    if (g_uc_fails == 0)
    {
        printf(" UC2 PASSED - 0 failures\n");
    }
    else
    {
        printf(" UC2 FAILED - %d failure(s)\n", g_uc_fails);
    }
    printf("============================================================"
           "====\n\n");

    return (g_uc_fails > 0) ? 1 : 0;
}
