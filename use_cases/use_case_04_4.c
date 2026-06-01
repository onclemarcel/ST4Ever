/*
 * use_case_04_4.c - UC4.4: GUI trace window (D2D append-only scroll view).
 *
 * TEST MATRIX - UC4.4:
 *   [N] Nominal    :  8 tests - gui_init; trace_init/open/close lifecycle;
 *                               is_open state; log entries while open;
 *                               shutdown while open; reinit after shutdown
 *   [R] Robustness :  2 tests - double open (idempotent), double close
 *   [S] Skipped    :  8 tests - run make manual UC=04_4
 *                               (window visible, log lines appear,
 *                                colors per log level,
 *                                auto-scroll on new lines,
 *                                scroll wheel / PgUp/PgDn / Home/End,
 *                                ESC closes window,
 *                                terminal free of log output when window open,
 *                                window closes on trace_close)
 *
 * Traceability:
 *   INT-TRC-020..026 → TC-TRC-018..026 → REQ-TRC-017..022
 *   UFR-TRC-007 (trace GUI window), UFR-CON-030..032 (trace commands)
 *
 * NOTE: This UC is heavily GUI-driven.  The automated tests only cover
 * the headless-safe portions (init/open/close state, no window creation).
 * GUI validation is in the [S] manual tests.
 */

#include "use_cases.h"

int g_uc_fails = 0;

int main(void)
{
    int iState;

    printf("=== UC4.4: GUI Trace Window ===\n\n");

    /* ----------------------------------------------------------------
     * BLOCK 1 — gui_init (required before trace_open opens a window)
     * ----------------------------------------------------------------
     * INTENT[INT-TRC-020 → TC-TRC-017 → REQ-TRC-017 → UFR-CON-004]:
     * gui_init() must succeed before any GUI window can be opened.
     */
    printf("--- gui_init ---\n");
    UC_CHECK("[N] gui_init succeeds", gui_init());

    /* ----------------------------------------------------------------
     * BLOCK 2 — trace_init / trace_open / trace_close lifecycle
     * ----------------------------------------------------------------
     * INTENT[INT-TRC-021 → TC-TRC-018 → REQ-TRC-001 → UFR-CON-004]:
     * trace_init(ST_FALSE) initialises the subsystem without opening
     * the GUI window; trace_is_open() must return ST_FALSE.
     */
    printf("--- init without open ---\n");
    UC_CHECK("[N] trace_init(ST_FALSE) succeeds",
             trace_init(ST_FALSE));

    iState = (int)trace_is_open();
    UC_TEST("[N] trace_is_open() == ST_FALSE after init without open",
            iState == (int)ST_FALSE);

    /*
     * INTENT[INT-TRC-022 → TC-TRC-020 → REQ-TRC-017 → UFR-TRC-007]:
     * trace_open() opens the D2D GUI window; trace_is_open() returns ST_TRUE.
     * The window shall not steal keyboard focus from the console.
     */
    printf("--- trace_open opens GUI window ---\n");
    UC_CHECK("[N] trace_open() succeeds", trace_open());

    iState = (int)trace_is_open();
    UC_TEST("[N] trace_is_open() == ST_TRUE after trace_open",
            iState == (int)ST_TRUE);

    /* Let the GUI window fully initialise and render once */
    platform_sleep_ms(200);

    /*
     * INTENT[INT-TRC-023 → TC-TRC-021 → REQ-TRC-018,REQ-TRC-019 → UFR-TRC-007]:
     * Log entries emitted while the window is open must not crash and must
     * reach the GUI ring buffer.  The headless-verifiable side-effect is that
     * is_open() remains TRUE; visual ring-buffer content is in the [S] tests.
     * ADAPTED UC4.4: trace_log() no longer writes to stderr when bOpen==TRUE
     * and the GUI view is live (terminal stays clean).
     */
    printf("--- log entries while window open ---\n");
    LOG_INFO("UC4.4 test: LOG_INFO entry");
    LOG_TRACE("UC4.4 test: LOG_TRACE entry");
    LOG_ERROR("UC4.4 test: LOG_ERROR entry");
    LOG_TODO("UC4.4 test: LOG_TODO entry");

    platform_sleep_ms(100);

    iState = (int)trace_is_open();
    UC_TEST("[N] trace_is_open() still ST_TRUE after log entries",
            iState == (int)ST_TRUE);

    /*
     * INTENT[INT-TRC-024 → TC-TRC-022 → REQ-TRC-017 → UFR-TRC-007]:
     * Double trace_open() is idempotent: a second call while already
     * open must return ST_NO_ERROR and leave the window open without
     * creating a second window.
     */
    printf("--- double open (robustness) ---\n");
    UC_CHECK("[R] second trace_open() idempotent (ST_NO_ERROR)",
             trace_open());

    iState = (int)trace_is_open();
    UC_TEST("[R] trace_is_open() still ST_TRUE after double open",
            iState == (int)ST_TRUE);

    /*
     * INTENT[INT-TRC-025 → TC-TRC-023,TC-TRC-024 → REQ-TRC-006,REQ-TRC-007]:
     * trace_close() closes the GUI window; trace_is_open() returns ST_FALSE.
     * A second trace_close() must also return ST_NO_ERROR (idempotent).
     */
    printf("--- trace_close / double close ---\n");
    UC_CHECK("[N] trace_close() succeeds", trace_close());

    iState = (int)trace_is_open();
    UC_TEST("[N] trace_is_open() == ST_FALSE after trace_close",
            iState == (int)ST_FALSE);

    UC_CHECK("[R] second trace_close() idempotent (ST_NO_ERROR)",
             trace_close());

    /*
     * INTENT[INT-TRC-026 → TC-TRC-025,TC-TRC-026 → REQ-TRC-012 → UFR-TRC-004]:
     * trace_shutdown() closes the log file, joins the GUI thread if open,
     * and resets state cleanly.  Calling trace_shutdown() while the window
     * is open must close it without requiring an explicit trace_close().
     */
    printf("--- shutdown / reinit ---\n");
    UC_CHECK("[N] trace_shutdown() succeeds", trace_shutdown());

    iState = (int)trace_is_open();
    UC_TEST("[N] trace_is_open() == ST_FALSE after shutdown",
            iState == (int)ST_FALSE);

    /* Shutdown with open window: trace_open then trace_shutdown */
    UC_CHECK("[N] trace_init for shutdown-while-open test",
             trace_init(ST_FALSE));
    UC_CHECK("[N] trace_open for shutdown-while-open test",
             trace_open());
    platform_sleep_ms(100);
    UC_CHECK("[N] trace_shutdown() closes open window cleanly",
             trace_shutdown());

    /* Cleanup GUI */
    gui_shutdown();

    /* ----------------------------------------------------------------
     * BLOCK 3 — Manual / visual tests
     * ----------------------------------------------------------------
     * These tests require a human observer.  Run: make manual UC=04_4
     */
    printf("\n--- Manual / visual tests ---\n");

    /* Re-init for manual tests */
    gui_init();
    trace_init(ST_FALSE);
    trace_open();
    platform_sleep_ms(200);

    LOG_INFO("Manual test: INFO line — should appear in CYAN");
    LOG_ERROR("Manual test: ERROR line — should appear in RED");
    LOG_TODO("Manual test: TODO line — should appear in MAGENTA");
    LOG_TRACE("Manual test: TRACE line — should appear in GRAY");
    platform_sleep_ms(100);

    TEST_MANUAL("[S] GUI trace window visible",
                "Is the trace window open and showing 4 log lines?");

    TEST_MANUAL("[S] Log level colors correct",
                "INFO=cyan, ERROR=red, TODO=magenta, TRACE=gray?");

    /* Generate many lines to test scroll */
    {
        int i;
        for (i = 0; i < 30; i++)
        {
            LOG_INFO("Scroll test line %d of 30", i + 1);
        }
        platform_sleep_ms(100);
    }

    TEST_MANUAL("[S] Auto-scroll to newest line",
                "Is the window scrolled to show the last lines?");

    TEST_MANUAL("[S] Mouse wheel scrolls content",
                "Can you scroll up/down with the mouse wheel?");

    TEST_MANUAL("[S] PageUp/PageDown scrolling",
                "Do PageUp/PageDown scroll by one screen?");

    TEST_MANUAL("[S] Home/End keys",
                "Does Home go to oldest line, End back to newest?");

    /* ADAPTED UC4.4: trace_log() suppresses stderr when GUI view is live.
     * Terminal must be CLEAN (no log output) while the window is open. */
    TEST_MANUAL("[S] Terminal clean — stderr suppressed when window open",
                "Is the terminal free of log lines while the trace window is open?");

    TEST_MANUAL("[S] ESC closes trace window",
                "Does pressing ESC inside the trace window close it?");

    trace_close();
    trace_shutdown();
    gui_shutdown();

    /* ----------------------------------------------------------------
     * Summary
     * ---------------------------------------------------------------- */
    printf("\n=== UC4.4 Results: %d failure(s) ===\n\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
