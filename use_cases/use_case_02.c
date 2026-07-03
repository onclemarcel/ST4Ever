/*
 * use_case_02.c - UC2 Validation: trace on/off/toggle command
 *
 * SRTD reference: SRTD.md §5 — test cases TC-TRC-010..TC-TRC-021
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN]: intent text
 *
 * Strategy: line_cmd_trace() and line_dispatch() are static; direct
 * calls from a test binary are not possible.  UC2 validates behavioral
 * contracts by driving the public trace API in the exact sequences
 * used by `trace`, `trace on`, and `trace off`.  Command-dispatch
 * tests (full stdin path) are [S] Skipped and validated manually
 * with `make run`.
 *
 * TEST MATRIX:
 *   [N] Nominal    : 21 tests  - toggle, on, off, state consistency
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
int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Group 1: trace (toggle - no argument)
 * ------------------------------------------------------------------ */

static void test_trace_toggle(void)
{
    printf("\n--- Test group 1: trace toggle (no argument) ---\n");

    /* INTENT[INT-TRC-010 → TC-TRC-010 → REQ-TRC-001]:
     * trace subsystem must initialise cleanly with console closed */
    UC_CHECK("[N] trace_init(ST_FALSE)", trace_init(ST_FALSE));
    UC_TEST("[N] console closed at startup",
            trace_is_open() == ST_FALSE);

    /* INTENT[INT-TRC-011 → TC-TRC-011 → REQ-TRC-011]:
     * `trace` (no arg) must open the console when it is closed.
     * Simulates: if (!trace_is_open()) trace_gui_open() */
    UC_CHECK("[N] trace_gui_open()  [toggle: closed -> open]",
             trace_gui_open());
    UC_TEST("[N] trace_is_open() == TRUE after toggle-open",
            trace_is_open() == ST_TRUE);

    /* INTENT[INT-TRC-012 → TC-TRC-012 → REQ-TRC-011]:
     * `trace` (no arg) must close the console when it is open.
     * Simulates: if (trace_is_open()) trace_close() */
    UC_CHECK("[N] trace_close()  [toggle: open -> closed]",
             trace_close());
    UC_TEST("[N] trace_is_open() == FALSE after toggle-close",
            trace_is_open() == ST_FALSE);

    /* INTENT[INT-TRC-013 → TC-TRC-013 → REQ-TRC-011]:
     * toggle must not change the trace_enabled state in either
     * direction (line_cmd_trace no-arg path never calls
     * trace_set_trace_enabled) */
    UC_CHECK("[N] trace_set_trace_enabled(TRUE) before toggle",
             trace_set_trace_enabled(ST_TRUE));
    UC_CHECK("[N] trace_gui_open()  [toggle: verify enabled unchanged]",
             trace_gui_open());
    UC_TEST("[N] trace_is_trace_enabled() unchanged after toggle-open",
            trace_is_trace_enabled() == ST_TRUE);
    UC_CHECK("[N] trace_close()  [toggle: verify enabled unchanged]",
             trace_close());
    UC_TEST("[N] trace_is_trace_enabled() unchanged after toggle-close",
            trace_is_trace_enabled() == ST_TRUE);
}

/* ------------------------------------------------------------------
 * Group 2: trace on
 * ------------------------------------------------------------------ */

static void test_trace_on(void)
{
    printf("\n--- Test group 2: trace on ---\n");

    /* Precondition: console closed, trace_enabled TRUE (from group 1).
     * Simulates the sequence: trace_gui_open() + trace_set_trace_enabled */

    /* INTENT[INT-TRC-014 → TC-TRC-014 → REQ-TRC-012]:
     * `trace on` must open the trace console */
    UC_CHECK("[N] trace_gui_open()  [trace on: open console]",
             trace_gui_open());
    UC_TEST("[N] trace_is_open() == TRUE after trace on",
            trace_is_open() == ST_TRUE);

    /* INTENT[INT-TRC-015 → TC-TRC-015 → REQ-TRC-013]:
     * `trace on` must activate LOG_TRACE output */
    UC_CHECK("[N] trace_set_trace_enabled(TRUE)  [trace on: enable]",
             trace_set_trace_enabled(ST_TRUE));
    UC_TEST("[N] trace_is_trace_enabled() == TRUE after trace on",
            trace_is_trace_enabled() == ST_TRUE);

    /* Verify LOG_TRACE is now active */
    LOG_TRACE("LOG_TRACE active: this must appear in st4ever_trace.log");

    /* INTENT[INT-TRC-016 → TC-TRC-016 → REQ-TRC-012]:
     * `trace on` when console already open must be a no-op (open is
     * idempotent per UC1 contract) */
    UC_TEST("[R] trace_gui_open() idempotent: already open -> ST_NO_ERROR",
            trace_gui_open() == ST_NO_ERROR);
    UC_TEST("[R] trace_is_open() still TRUE after second open",
            trace_is_open() == ST_TRUE);
}

/* ------------------------------------------------------------------
 * Group 3: trace off
 * ------------------------------------------------------------------ */

static void test_trace_off(void)
{
    printf("\n--- Test group 3: trace off ---\n");

    /* Precondition: console open, trace_enabled TRUE (from group 2).
     * ADAPTED: P19 - trace off no longer calls trace_close().
     * Simulates: trace_set_trace_enabled(FALSE) only; view stays open. */

    /* INTENT[INT-TRC-017 → TC-TRC-017 → REQ-TRC-009]:
     * `trace off` must disable LOG_TRACE output */
    UC_CHECK("[N] trace_set_trace_enabled(FALSE)  [trace off: disable]",
             trace_set_trace_enabled(ST_FALSE));
    UC_TEST("[N] trace_is_trace_enabled() == FALSE after trace off",
            trace_is_trace_enabled() == ST_FALSE);

    /* ADAPTED: P19 - INT-TRC-018: trace off must NOT close the console.
     * INTENT[INT-TRC-018 → TC-TRC-015 → UFR-CON-031]:
     * `trace off` must leave the trace view open (filter only) */
    UC_TEST("[N] trace_is_open() == TRUE after trace off (P19: view stays open)",
            trace_is_open() == ST_TRUE);

    /* ADAPTED: P19 - INT-TRC-019: idempotency now covers second disable,
     * not trace_close() (which is no longer called by trace off).
     * INTENT[INT-TRC-019 → TC-TRC-016 → REQ-TRC-016]:
     * `trace off` twice is harmless; view remains open */
    UC_TEST("[R] trace_set_trace_enabled(FALSE) idempotent -> ST_NO_ERROR",
            trace_set_trace_enabled(ST_FALSE) == ST_NO_ERROR);
    UC_TEST("[R] trace_is_open() still TRUE after second trace off",
            trace_is_open() == ST_TRUE);
}

/* ------------------------------------------------------------------
 * Group 4: state consistency after trace off
 * ------------------------------------------------------------------ */

static void test_state_consistency(void)
{
    printf("\n--- Test group 4: state consistency ---\n");

    /* Precondition: console OPEN, trace_enabled FALSE (from group 3 — P19) */

    /* INTENT[INT-TRC-020 → TC-TRC-020 → REQ-TRC-016]:
     * After trace off, LOG_INFO / LOG_ERROR / LOG_TODO must still
     * emit to the log file without crash.  LOG_TRACE must be silent
     * (trace_enabled FALSE). */
    printf("  [INFO] Verifying LOG levels with trace disabled:\n");
    LOG_TRACE("THIS LINE MUST NOT APPEAR (trace_enabled is FALSE)");
    LOG_INFO("LOG_INFO active: emitting to st4ever_trace.log");
    LOG_ERROR("LOG_ERROR active: emitting (not a real error)");
    LOG_TODO("LOG_TODO active: emitting");
    printf("  [PASS] [R] LOG_INFO/ERROR/TODO emitted without crash\n");
    printf("         (check st4ever_trace.log: LOG_TRACE suppressed)\n");

    /* INTENT[INT-TRC-021 → TC-TRC-021 → REQ-TRC-013]:
     * LOG_TRACE must become active again after trace_set_trace_enabled */
    UC_CHECK("[N] trace_set_trace_enabled(TRUE) re-enable",
             trace_set_trace_enabled(ST_TRUE));
    UC_TEST("[N] trace_is_trace_enabled() == TRUE after re-enable",
            trace_is_trace_enabled() == ST_TRUE);
    LOG_TRACE("This LOG_TRACE MUST appear after re-enable");
}

/* ------------------------------------------------------------------
 * Group 5: skipped - command dispatch via stdin
 * ------------------------------------------------------------------ */

static void test_skipped(void)
{
    printf("\n--- Test group 5: command dispatch (manual via make run) ---\n");

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
    printf("\n");
    printf("============================================================"
           "====\n");
    printf(" UC2 - trace on / off / toggle command validation\n");
    printf("============================================================"
           "====\n");

    test_trace_toggle();
    test_trace_on();
    test_trace_off();
    test_state_consistency();
    test_skipped();

    UC_CHECK("[cleanup] trace_shutdown()", trace_shutdown());

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
