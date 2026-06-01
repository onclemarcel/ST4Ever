/*
 * use_case_05.c - UC5: where, info, history, console title, trace
 *                      clear/level, TAB common prefix, dir H/F5.
 *
 * TEST MATRIX - UC5:
 *   [N] Nominal    : 31 tests - trace_get/set_view_level; trace_clear;
 *                               line_init P24 colors-auto;
 *                               line_update_console_title;
 *                               where/info/history/trace in completion table;
 *                               trace level round-trip; history add/get/count;
 *                               TAB candidates (empty/h/he/info/hist);
 *                               line_shutdown
 *   [R] Robustness :  4 tests - trace_set_view_level(invalid),
 *                               trace_clear twice when closed,
 *                               line_update_console_title(NULL),
 *                               history_get on empty ring
 *   [S] Skipped    :  9 tests - run make manual UC=05
 *                               (console title updated in title bar;
 *                                P21: H key toggles hidden in dir view;
 *                                P22: F5 refreshes dir view;
 *                                P23bis: TAB inserts common prefix live;
 *                                P24: colors off when redirected to file;
 *                                P27: trace clear empties GUI window;
 *                                P28: trace level info hides TRACE rows;
 *                                where/info output correct in terminal)
 *
 * Traceability:
 *   INT-CON-050..062 → TC-TRC-035..044, TC-CON-101..120, TC-DIR-050..051
 *   UFR-CON-080 (where), UFR-CON-081 (info), UFR-CON-082 (history [N])
 *   UFR-CON-083 (P8 console title), UFR-CON-084 (P23bis TAB prefix)
 *   UFR-CON-085 (P24 colors auto), REQ-CON-025..031
 *   UFR-TRC-011 (P27 trace clear), UFR-TRC-012 (P28 trace level)
 *   REQ-TRC-023..026
 *   UFR-DIR-011 (P21 H hidden toggle), UFR-DIR-012 (P22 F5 preserve)
 *   REQ-DIR-021..022
 */

#include "use_cases.h"

int g_uc_fails = 0;

int main(void)
{
    line_context_t  tCtx;
    char            aOut[32][ST_MAX_PATH];
    int             iCount;
    log_level_t     eLevel;
    int             iHistInit;
    char            szEntry[ST_MAX_CMD];

    printf("=== UC5: where / info / history / trace clear+level ===\n\n");

    /* ----------------------------------------------------------------
     * BLOCK 1 — trace_get/set_view_level (no GUI needed)
     * ----------------------------------------------------------------
     * INTENT[INT-CON-050 → TC-TRC-035..039 → REQ-TRC-024..025 → UFR-TRC-012]:
     * trace_get_view_level() returns LOG_LEVEL_TRACE initially (show all).
     * trace_set_view_level() changes the display filter without affecting
     * the ring buffer or log file. Persists across open/close cycles.
     */
    printf("--- trace view level (P28) ---\n");

    eLevel = trace_get_view_level();
    UC_TEST("[N] trace_get_view_level() == LOG_LEVEL_TRACE initially",
            eLevel == LOG_LEVEL_TRACE);

    UC_CHECK("[N] trace_set_view_level(INFO) returns ST_NO_ERROR",
             trace_set_view_level(LOG_LEVEL_INFO));

    eLevel = trace_get_view_level();
    UC_TEST("[N] trace_get_view_level() == LOG_LEVEL_INFO after set",
            eLevel == LOG_LEVEL_INFO);

    UC_CHECK("[N] trace_set_view_level(TRACE) restores default",
             trace_set_view_level(LOG_LEVEL_TRACE));

    eLevel = trace_get_view_level();
    UC_TEST("[N] trace_get_view_level() == LOG_LEVEL_TRACE restored",
            eLevel == LOG_LEVEL_TRACE);

    /* Robustness: out-of-range enum value — must not crash */
    UC_CHECK("[R] trace_set_view_level(999) no-crash",
             trace_set_view_level((log_level_t)999));

    /* Restore to sane value */
    trace_set_view_level(LOG_LEVEL_TRACE);

    /* ----------------------------------------------------------------
     * BLOCK 2 — trace_clear() without an open window
     * ----------------------------------------------------------------
     * INTENT[INT-CON-052 → TC-TRC-040..041 → REQ-TRC-023 → UFR-TRC-011]:
     * trace_clear() is a no-op (ST_NO_ERROR) when the trace window is
     * not open.  It must not crash or return ST_ERROR.
     */
    printf("\n--- trace_clear() — no window (P27) ---\n");

    UC_CHECK("[N] trace_clear() when window closed returns ST_NO_ERROR",
             trace_clear());

    UC_CHECK("[R] trace_clear() twice when closed: still ST_NO_ERROR",
             trace_clear());

    /* ----------------------------------------------------------------
     * BLOCK 3 — line_init() with P24 colors-auto
     * ----------------------------------------------------------------
     * INTENT[INT-CON-053 → TC-CON-101 → REQ-CON-001..003, REQ-CON-029 → UFR-CON-001, UFR-CON-085]:
     * line_init() calls isatty(stdout): in CI / test mode stdout is
     * usually a pipe, so g_line_bColors is expected ST_FALSE.
     * We only verify that it is a valid st_bool_t value (no UB).
     */
    printf("\n--- line_init() + P24 colors-auto ---\n");

    UC_CHECK("[N] line_init()", line_init(&tCtx));
    UC_TEST("[N] bRunning == ST_TRUE after line_init",
            tCtx.bRunning == ST_TRUE);
    UC_TEST("[N] szCwd non-empty after line_init",
            tCtx.szCwd[0] != '\0');

    {
        st_bool_t bCol = line_get_colors();
        UC_TEST("[N] line_get_colors() is a valid st_bool_t",
                bCol == ST_TRUE || bCol == ST_FALSE);
    }

    /* ----------------------------------------------------------------
     * BLOCK 4 — line_update_console_title()
     * ----------------------------------------------------------------
     * INTENT[INT-CON-054 → TC-CON-102..103, TC-CON-116 → REQ-CON-028 → UFR-CON-083]:
     * line_update_console_title() must not crash with NULL or with a
     * valid context.  Visual check of the title bar is manual.
     */
    printf("\n--- line_update_console_title() (P8) ---\n");

    /* NULL context: must not crash */
    line_update_console_title(NULL);
    UC_TEST("[R] line_update_console_title(NULL) does not crash", 1);

    line_update_console_title(&tCtx);
    UC_TEST("[N] line_update_console_title(&tCtx) does not crash", 1);

    TEST_MANUAL("[S] Console title bar shows ST4Ever version and cwd",
                "Is the terminal title bar updated correctly?");

    /* ----------------------------------------------------------------
     * BLOCK 5 — command table: where / info / history in completion
     * ----------------------------------------------------------------
     * INTENT[INT-CON-055 → TC-CON-104..107 → REQ-CON-025..027, REQ-CON-031 → UFR-CON-080..082]:
     * line_complete_cmd_query() uses the same command table as the
     * dispatcher.  A command present in the table will appear as a
     * completion candidate for its prefix.  This indirectly proves
     * the commands are registered (parsing is internal/static).
     */
    printf("\n--- command table: where / info / history in completion ---\n");

    iCount = line_complete_cmd_query("where", aOut, 32);
    UC_TEST("[N] 'where' prefix finds 'where' in completion table",
            iCount == 1 && strcmp(aOut[0], "where") == 0);

    iCount = line_complete_cmd_query("info", aOut, 32);
    UC_TEST("[N] 'info' prefix finds 'info' in completion table",
            iCount == 1 && strcmp(aOut[0], "info") == 0);

    iCount = line_complete_cmd_query("history", aOut, 32);
    UC_TEST("[N] 'history' prefix finds 'history' in completion table",
            iCount == 1 && strcmp(aOut[0], "history") == 0);

    iCount = line_complete_cmd_query("trace", aOut, 32);
    UC_TEST("[N] 'trace' prefix finds 'trace' in completion table",
            iCount == 1 && strcmp(aOut[0], "trace") == 0);

    /* ----------------------------------------------------------------
     * BLOCK 6 — trace view-level set/get round-trip after init
     * ----------------------------------------------------------------
     * INTENT[INT-CON-056 → TC-TRC-042 → REQ-TRC-024..025 → UFR-TRC-012]:
     * After line_init, trace_set_view_level and get_view_level must
     * be consistent (persistent global, not tied to view lifecycle).
     */
    printf("\n--- trace view level round-trip ---\n");

    UC_CHECK("[N] trace_set_view_level(ERROR) after line_init",
             trace_set_view_level(LOG_LEVEL_ERROR));
    eLevel = trace_get_view_level();
    UC_TEST("[N] trace_get_view_level() == ERROR",
            eLevel == LOG_LEVEL_ERROR);

    UC_CHECK("[N] trace_set_view_level(TRACE) restore",
             trace_set_view_level(LOG_LEVEL_TRACE));

    /* ----------------------------------------------------------------
     * BLOCK 7 — history command: content and count
     * ----------------------------------------------------------------
     * INTENT[INT-CON-057..058 → TC-CON-108..110, TC-CON-117 → REQ-CON-027 → UFR-CON-082]:
     * After line_init loads history from disk, line_history_count()
     * returns a non-negative count.  Adding an entry bumps the count.
     * line_history_get() retrieves the last added entry.
     * get() on an empty ring returns ST_ERROR.
     */
    printf("\n--- history command (P25) ---\n");

    iHistInit = line_history_count();
    UC_TEST("[N] line_history_count() >= 0 after init",
            iHistInit >= 0);

    line_history_clear();
    UC_TEST("[N] line_history_count() == 0 after clear",
            line_history_count() == 0);

    UC_CHECK("[N] line_history_add(\"where\") adds entry",
             line_history_add("where"));
    UC_TEST("[N] line_history_count() == 1",
            line_history_count() == 1);

    UC_CHECK("[N] line_history_get(0, ...) retrieves entry",
             line_history_get(0, szEntry, sizeof(szEntry)));
    UC_TEST("[N] retrieved entry == \"where\"",
            strcmp(szEntry, "where") == 0);

    /* Robustness: count==0 corner case (tested via clear then get) */
    line_history_clear();
    UC_TEST("[R] line_history_get(0, ...) on empty history → ST_ERROR",
            line_history_get(0, szEntry, sizeof(szEntry)) == ST_ERROR);

    TEST_MANUAL("[S] 'history' shows last entries numbered",
                "Does 'history 3' show the 3 most recent commands?");

    /* ----------------------------------------------------------------
     * BLOCK 8 — TAB common-prefix (P23bis) via line_complete_cmd_query
     * ----------------------------------------------------------------
     * INTENT[INT-CON-059..061 → TC-CON-111..114, TC-CON-118..119 → REQ-CON-018 → UFR-CON-009, UFR-CON-084]:
     * When N>1 candidates share a common prefix beyond the typed prefix,
     * the first TAB must insert the longest common prefix.
     * Tested indirectly: query returns candidate list; the live-editor
     * P23bis is validated manually.
     *
     * Example: typing "h" matches "help" and "history".
     *   common prefix = "h" (already typed) → no insertion needed.
     *   Both candidates returned.
     *
     * Example: typing "" matches all commands — all returned.
     */
    printf("\n--- TAB completion candidates (P23bis) ---\n");

    iCount = line_complete_cmd_query("", aOut, 32);
    UC_TEST("[N] empty prefix → all commands returned (>0)",
            iCount > 0);

    /* "h" prefix matches "help" and "history" (at minimum) */
    iCount = line_complete_cmd_query("h", aOut, 32);
    UC_TEST("[N] 'h' prefix → at least 2 candidates (help, history)",
            iCount >= 2);

    /* "he" prefix → only "help" (1 candidate → instant insert) */
    iCount = line_complete_cmd_query("he", aOut, 32);
    UC_TEST("[N] 'he' prefix → 1 candidate (help)",
            iCount == 1 && strcmp(aOut[0], "help") == 0);

    /* "info" prefix → 1 candidate (exact match) */
    iCount = line_complete_cmd_query("info", aOut, 32);
    UC_TEST("[N] 'info' prefix → 1 candidate",
            iCount == 1 && strcmp(aOut[0], "info") == 0);

    /* "hist" prefix → 1 candidate "history" */
    iCount = line_complete_cmd_query("hist", aOut, 32);
    UC_TEST("[N] 'hist' prefix → 1 candidate (history)",
            iCount == 1 && strcmp(aOut[0], "history") == 0);

    TEST_MANUAL("[S] TAB common prefix live: type 'h' + TAB",
                "Does first TAB complete 'h' to 'h' (no change, ghost shows candidates)?");

    TEST_MANUAL("[S] TAB single candidate live: type 'he' + TAB",
                "Does TAB complete 'he' to 'help' immediately?");

    /* ----------------------------------------------------------------
     * BLOCK 9 — line_shutdown
     * ----------------------------------------------------------------
     * INTENT[INT-CON-062 → TC-CON-115 → REQ-CON-004..005 → UFR-CON-001]:
     * line_shutdown() must return ST_NO_ERROR and set bRunning=FALSE.
     */
    printf("\n--- line_shutdown ---\n");

    UC_CHECK("[N] line_shutdown() returns ST_NO_ERROR",
             line_shutdown(&tCtx));
    UC_TEST("[N] bRunning == ST_FALSE after shutdown",
            tCtx.bRunning == ST_FALSE);

    /* ----------------------------------------------------------------
     * BLOCK 10 — manual-only tests (P21, P22, P27, P28 GUI, P24 piped)
     * ----------------------------------------------------------------
     * INTENT[INT-CON-053..062 → TC-DIR-050..051, TC-TRC-043..044, TC-CON-116..120]:
     * Visual / TTY-dependent behaviours not automatable headless.
     * Run 'make manual UC=05' to validate interactively.
     */
    printf("\n--- manual-only (dir H, F5, trace P28, colors P24) ---\n");

    TEST_MANUAL("[S] P21: H key in dir view toggles hidden files",
                "Open 'dir', press H — do hidden entries appear/disappear?");
    TEST_MANUAL("[S] P22: F5 refreshes listing and preserves expansion state",
                "Open 'dir', expand some dirs, add a file externally, press F5 — do expanded dirs stay open and new file appear?");
    TEST_MANUAL("[S] P28: 'trace level info' hides LOG_TRACE rows in GUI",
                "Open trace, type 'trace level info' — are TRACE rows gone from window?");
    TEST_MANUAL("[S] P27: 'trace clear' empties the GUI trace window",
                "With trace open, type 'trace clear' — does the window become empty?");
    TEST_MANUAL("[S] P24: colors off when stdout piped",
                "Run 'ST4Ever > out.txt' — does out.txt contain no ANSI codes?");

    /* ----------------------------------------------------------------
     * Summary
     * ----------------------------------------------------------------
     */
    printf("\n");
    if (g_uc_fails == 0)
    {
        printf("  All tests PASSED.\n\n");
    }
    else
    {
        printf("  %d test(s) FAILED.\n\n", g_uc_fails);
    }

    return (g_uc_fails == 0) ? 0 : 1;
}
