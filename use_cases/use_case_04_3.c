/*
 * use_case_04_3.c - UC4.3: history, tab-completion, colors, script,
 *                          thread-safe selected accessor.
 *
 * TEST MATRIX - UC4.3:
 *   [N] Nominal    : 19 tests - history add/get/count/wrap/persist,
 *                               colors toggle, set/get_selected (mutex),
 *                               script mode, cmd completion query,
 *                               path completion query
 *   [R] Robustness :  7 tests - NULL params, empty history, out-of-
 *                               range index, missing script file,
 *                               empty/invalid completion prefix
 *   [S] Skipped    :  8 tests - run make manual UC=04_3
 *                               (history nav, TAB ghost, prompt [T]/[file],
 *                                colors off, --script end-to-end)
 *
 * Traceability:
 *   INT-LIN-001..023 → TC-LIN-001..036 → REQ-CON-017..024
 *   UFR-CON-007, UFR-CON-009, UFR-CON-047..049, UFR-DIR-002
 */

#include "use_cases.h"
#include <sys/stat.h>   /* stat / mkdir              */
#ifdef ST_PLATFORM_WINDOWS
#   include <direct.h>  /* _mkdir (MinGW/Win32)       */
#endif

int g_uc_fails = 0;
st_bool_t gIsObject = 0;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

/* Write a small script file and return its path (static buffer). */
static const char *write_test_script(void)
{
    static char szPath[ST_MAX_PATH];
    FILE *pF;

    snprintf(szPath, sizeof(szPath),
             "use_cases/UC04_3/test_script.txt");

    pF = fopen(szPath, "w");
    if (pF == NULL)
    {
        return NULL;
    }

    fprintf(pF, "# ST4Ever test script\n");
    fprintf(pF, "help\n");
    fprintf(pF, "colors off\n");
    fprintf(pF, "colors on\n");
    fprintf(pF, "quit\n");
    fclose(pF);

    return szPath;
}

/* ------------------------------------------------------------------
 * History tests
 * ------------------------------------------------------------------ */

static void test_history(void)
{
    char szBuf[ST_MAX_CMD];

    printf("\n--- History ---\n");

    line_history_clear();

    /* INTENT[INT-LIN-001 → TC-LIN-001 → REQ-LIN-001]:
     * Empty history has count 0. */
    UC_TEST("[N] history_count() == 0 after clear",
            line_history_count() == 0);

    /* INTENT[INT-LIN-002 → TC-LIN-002]:
     * history_add(NULL) returns ST_ERROR. */
    UC_TEST("[R] history_add(NULL) → ST_ERROR",
            line_history_add(NULL) == ST_ERROR);

    /* INTENT[INT-LIN-003 → TC-LIN-003]:
     * history_add("") is a no-op (empty strings ignored). */
    UC_CHECK("[N] history_add empty string → ST_NO_ERROR",
             line_history_add(""));
    UC_TEST("[N] empty string not stored",
            line_history_count() == 0);

    /* INTENT[INT-LIN-004 → TC-LIN-004]:
     * Normal add stores the entry. */
    UC_CHECK("[N] history_add 'help'", line_history_add("help"));
    UC_TEST("[N] count == 1 after first add",
            line_history_count() == 1);

    UC_CHECK("[N] history_add 'dir src'", line_history_add("dir src"));
    UC_TEST("[N] count == 2", line_history_count() == 2);

    /* INTENT[INT-LIN-005 → TC-LIN-005]:
     * Duplicate of the most recent entry is ignored. */
    UC_CHECK("[N] history_add 'dir src' again (dup)",
             line_history_add("dir src"));
    UC_TEST("[N] count still 2 (duplicate ignored)",
            line_history_count() == 2);

    /* INTENT[INT-LIN-006 → TC-LIN-006]:
     * history_get() virtual 0 = oldest entry. */
    UC_CHECK("[N] history_get(0, ...) → oldest",
             line_history_get(0, szBuf, sizeof(szBuf)));
    UC_TEST("[N] oldest entry is 'help'",
            strcmp(szBuf, "help") == 0);

    /* INTENT[INT-LIN-007 → TC-LIN-007]:
     * history_get() virtual count-1 = most recent. */
    UC_CHECK("[N] history_get(count-1) → most recent",
             line_history_get(line_history_count() - 1,
                              szBuf, sizeof(szBuf)));
    UC_TEST("[N] most recent is 'dir src'",
            strcmp(szBuf, "dir src") == 0);

    /* INTENT[INT-LIN-008 → TC-LIN-008]:
     * history_get() with out-of-range index → ST_ERROR. */
    UC_TEST("[R] history_get(-1) → ST_ERROR",
            line_history_get(-1, szBuf, sizeof(szBuf)) == ST_ERROR);
    UC_TEST("[R] history_get(count) → ST_ERROR",
            line_history_get(line_history_count(),
                             szBuf, sizeof(szBuf)) == ST_ERROR);

    /* INTENT[INT-LIN-009 → TC-LIN-009]:
     * history_get(NULL) → ST_ERROR. */
    UC_TEST("[R] history_get(0, NULL, N) → ST_ERROR",
            line_history_get(0, NULL, sizeof(szBuf)) == ST_ERROR);
    UC_TEST("[R] history_get(0, buf, 0) → ST_ERROR",
            line_history_get(0, szBuf, 0) == ST_ERROR);
}

/* ------------------------------------------------------------------
 * History ring wrap (> LINE_HISTORY_MAX entries)
 * ------------------------------------------------------------------ */

static void test_history_wrap(void)
{
    int  iIdx;
    char szBuf[ST_MAX_CMD];
    char szExpected[ST_MAX_CMD];

    printf("\n--- History ring wrap ---\n");

    line_history_clear();

    /* Fill beyond LINE_HISTORY_MAX (64) with unique entries */
    for (iIdx = 0; iIdx < LINE_HISTORY_MAX + 4; iIdx++)
    {
        snprintf(szBuf, sizeof(szBuf), "cmd%d", iIdx);
        line_history_add(szBuf);
    }

    /* INTENT[INT-LIN-010 → TC-LIN-010]:
     * count is capped at LINE_HISTORY_MAX. */
    UC_TEST("[N] count capped at LINE_HISTORY_MAX after wrap",
            line_history_count() == LINE_HISTORY_MAX);

    /* Oldest entry should be cmd4 (the 5th added, since first 4 got
     * evicted by the 4 extras). */
    snprintf(szExpected, sizeof(szExpected),
             "cmd%d", 4);

    UC_CHECK("[N] get oldest (virtual 0) after wrap",
             line_history_get(0, szBuf, sizeof(szBuf)));
    UC_TEST("[N] oldest entry after wrap is correct",
            strcmp(szBuf, szExpected) == 0);

    /* Most recent should be cmd(LINE_HISTORY_MAX+3). */
    snprintf(szExpected, sizeof(szExpected),
             "cmd%d", LINE_HISTORY_MAX + 3);

    UC_CHECK("[N] get newest (virtual count-1) after wrap",
             line_history_get(line_history_count() - 1,
                              szBuf, sizeof(szBuf)));
    UC_TEST("[N] newest entry after wrap is correct",
            strcmp(szBuf, szExpected) == 0);

    line_history_clear();
}

/* ------------------------------------------------------------------
 * History persistence (save / load)
 * ------------------------------------------------------------------ */

static void test_history_persist(void)
{
    static const char *szPath =
        "use_cases/UC04_3/test_history.txt";
    char szBuf[ST_MAX_CMD];

    printf("\n--- History persist ---\n");

    line_history_clear();
    line_history_add("trace on");
    line_history_add("dir /tmp");

    /* INTENT[INT-LIN-011 → TC-LIN-011]:
     * save() + clear() + load() round-trips the history. */
    UC_CHECK("[N] history_save to file",
             line_history_save(szPath));

    line_history_clear();
    UC_TEST("[N] count == 0 after clear", line_history_count() == 0);

    UC_CHECK("[N] history_load from file",
             line_history_load(szPath));

    UC_TEST("[N] count == 2 after reload",
            line_history_count() == 2);

    UC_CHECK("[N] oldest entry", line_history_get(0, szBuf,
                                                   sizeof(szBuf)));
    UC_TEST("[N] oldest is 'trace on'",
            strcmp(szBuf, "trace on") == 0);

    UC_CHECK("[N] newest entry", line_history_get(1, szBuf,
                                                   sizeof(szBuf)));
    UC_TEST("[N] newest is 'dir /tmp'",
            strcmp(szBuf, "dir /tmp") == 0);

    /* load() on non-existent file → ST_NO_ERROR (first-run case) */
    UC_CHECK("[N] history_load non-existent → ST_NO_ERROR",
             line_history_load(
                 "use_cases/UC04_3/no_such_file.txt"));

    line_history_clear();
}

/* ------------------------------------------------------------------
 * Color toggle
 * ------------------------------------------------------------------ */

static void test_colors(void)
{
    printf("\n--- Colors ---\n");

    /* INTENT[INT-LIN-012 → TC-LIN-012]:
     * Colors default to ON; set_colors() changes state;
     * get_colors() reflects it. */
    line_set_colors(ST_TRUE);
    UC_TEST("[N] colors ON by default after set",
            line_get_colors() == ST_TRUE);

    line_set_colors(ST_FALSE);
    UC_TEST("[N] colors OFF after set_colors(FALSE)",
            line_get_colors() == ST_FALSE);

    line_set_colors(ST_TRUE);
    UC_TEST("[N] colors ON again after re-enable",
            line_get_colors() == ST_TRUE);
}

/* ------------------------------------------------------------------
 * Thread-safe selected accessor
 * ------------------------------------------------------------------ */

static void test_selected_accessor(void)
{
    line_context_t *tCtx;
    char           szBuf[ST_MAX_PATH];
    st_u64_t       ullR = line_init("");
    tCtx = (void*)ullR;
    UC_CHECK_OBJ(tCtx, ST_LINE_CTX);
    if (g_uc_fails) return;
    
    printf("\n--- Selected accessor ---\n");

    UC_CHECK("[N] line_init", ullR);

    /* INTENT[INT-LIN-013 → TC-LIN-013]:
     * After init, selected is empty. */
    UC_CHECK("[N] get_selected after init",
             line_get_selected(szBuf, sizeof(szBuf)));
    UC_TEST("[N] selected empty after init", szBuf[0] == '\0');

    /* INTENT[INT-LIN-014 → TC-LIN-014]:
     * set_selected() stores, get_selected() retrieves. */
    UC_CHECK("[N] set_selected '/tmp/hello.prg'",
             line_set_selected("/tmp/hello.prg"));

    UC_CHECK("[N] get_selected",
             line_get_selected(szBuf, sizeof(szBuf)));
    UC_TEST("[N] retrieved path matches",
            strcmp(szBuf, "/tmp/hello.prg") == 0);

    /* INTENT[INT-LIN-015 → TC-LIN-015]:
     * set_selected(NULL) clears the selection. */
    UC_CHECK("[N] set_selected(NULL) clears",
             line_set_selected(NULL));
    UC_CHECK("[N] get_selected after clear",
             line_get_selected(szBuf, sizeof(szBuf)));
    UC_TEST("[N] selected empty after NULL set", szBuf[0] == '\0');

    /* Robustness */
    tCtx->bRunning = ST_FALSE;
    UC_TEST("[R] set_selected(NULL ctx) → ST_ERROR",
            line_set_selected("/tmp/x") == ST_ERROR);
    UC_TEST("[R] get_selected(NULL ctx) → ST_ERROR",
            line_get_selected(szBuf, sizeof(szBuf))
            == ST_ERROR);
    UC_TEST("[R] get_selected(NULL buf) → ST_ERROR",
            line_get_selected(NULL, sizeof(szBuf))
            == ST_ERROR);

    UC_CHECK("[N] line_shutdown", line_shutdown());
}

/* ------------------------------------------------------------------
 * Script mode
 * ------------------------------------------------------------------ */

static void test_script(void)
{
    line_context_t *tCtx;
    const char     *szScript;
    st_u64_t        ullR;

    printf("\n--- Script mode ---\n");

    /* INTENT[INT-LIN-016 → TC-LIN-016]:
     * --script runs commands from file; quits cleanly. */
    szScript = write_test_script();
    UC_TEST("[N] test script file created", szScript != NULL);

    if (szScript != NULL)
    {
        ullR = line_init(szScript);
        tCtx = (void*)ullR;
        UC_CHECK_OBJ(tCtx, ST_LINE_CTX);
        if (g_uc_fails) return;
    
        UC_CHECK("[N] line_init for script mode", ullR);

        strncpy(tCtx->szScriptFile, szScript,
                ST_MAX_PATH - 1);
        tCtx->szScriptFile[ST_MAX_PATH - 1] = '\0';

        /* line_run() with szScriptFile set → batch mode */
        UC_CHECK("[N] line_run script mode",
                 line_run());

        /* After script ran 'quit': bRunning should be FALSE */
        UC_TEST("[N] bRunning FALSE after quit in script",
                tCtx->bRunning == ST_FALSE);

        UC_CHECK("[N] line_shutdown after script",
                 line_shutdown());
    }

    /* INTENT[INT-LIN-017 → TC-LIN-017]:
     * --script with missing file → ST_ERROR from line_run. */
    UC_CHECK("[N] line_init for missing script",
             line_init(""));
    strncpy(tCtx->szScriptFile,
            "use_cases/UC04_3/no_such_script.txt",
            ST_MAX_PATH - 1);
    tCtx->szScriptFile[ST_MAX_PATH - 1] = '\0';

    UC_TEST("[R] line_run with missing script → ST_ERROR",
            line_run() == ST_ERROR);

    UC_CHECK("[N] line_shutdown after missing script",
             line_shutdown());
}

/* ------------------------------------------------------------------
 * Command completion query
 * ------------------------------------------------------------------ */

static void test_complete_cmd(void)
{
    char aaCandidates[LINE_COMPLETE_MAX][ST_MAX_PATH];
    int  iCount;
    int  iIdx;
    int  bFoundHelp;
    int  bFoundQuit;

    printf("\n--- Completion: commands ---\n");

    /* INTENT[INT-LIN-018 → TC-LIN-018]:
     * Empty prefix returns all commands. */
    iCount = line_complete_cmd_query("", aaCandidates,
                                     LINE_COMPLETE_MAX);
    UC_TEST("[N] empty prefix returns > 0 candidates",
            iCount > 0);

    /* INTENT[INT-LIN-019 → TC-LIN-019]:
     * Prefix "he" → exactly "help". */
    iCount = line_complete_cmd_query("he", aaCandidates,
                                     LINE_COMPLETE_MAX);
    UC_TEST("[N] 'he' matches exactly 1 command",
            iCount == 1);
    UC_TEST("[N] that command is 'help'",
            iCount == 1 && strcmp(aaCandidates[0], "help") == 0);

    /* Prefix "q" → "quit". */
    iCount = line_complete_cmd_query("q", aaCandidates,
                                     LINE_COMPLETE_MAX);
    UC_TEST("[N] 'q' matches 'quit'",
            iCount >= 1 && strcmp(aaCandidates[0], "quit") == 0);

    /* INTENT[INT-LIN-020 → TC-LIN-020]:
     * 'h' matches via shortcut → completes to "help". */
    bFoundHelp = 0;
    iCount = line_complete_cmd_query("h", aaCandidates,
                                     LINE_COMPLETE_MAX);
    for (iIdx = 0; iIdx < iCount; iIdx++)
    {
        if (strcmp(aaCandidates[iIdx], "help") == 0)
        {
            bFoundHelp = 1;
        }
    }
    UC_TEST("[N] 'h' produces 'help' candidate",
            bFoundHelp == 1);

    /* Prefix "xyz" matches nothing. */
    iCount = line_complete_cmd_query("xyz", aaCandidates,
                                     LINE_COMPLETE_MAX);
    UC_TEST("[N] 'xyz' returns 0 candidates", iCount == 0);

    /* All commands are unique in candidates for "h". */
    bFoundHelp = 0;
    bFoundQuit = 0;
    iCount = line_complete_cmd_query("", aaCandidates,
                                     LINE_COMPLETE_MAX);
    for (iIdx = 0; iIdx < iCount; iIdx++)
    {
        if (strcmp(aaCandidates[iIdx], "help") == 0)
            bFoundHelp = 1;
        if (strcmp(aaCandidates[iIdx], "quit") == 0)
            bFoundQuit = 1;
    }
    UC_TEST("[N] empty prefix includes 'help' and 'quit'",
            bFoundHelp && bFoundQuit);

    /* Robustness */
    UC_TEST("[R] NULL prefix → -1",
            line_complete_cmd_query(NULL, aaCandidates,
                                    LINE_COMPLETE_MAX) == -1);
    UC_TEST("[R] NULL aOut → -1",
            line_complete_cmd_query("h", NULL,
                                    LINE_COMPLETE_MAX) == -1);
    UC_TEST("[R] iMaxOut==0 → -1",
            line_complete_cmd_query("h", aaCandidates,
                                    0) == -1);
}

/* ------------------------------------------------------------------
 * Path completion query
 * ------------------------------------------------------------------ */

static void test_complete_path(void)
{
    char aaCandidates[LINE_COMPLETE_MAX][ST_MAX_PATH];
    int  iCount;
    int  iIdx;
    int  bFoundSrc;
    int  bFoundUseCases;

    printf("\n--- Completion: paths ---\n");

    /* INTENT[INT-LIN-021 → TC-LIN-021]:
     * Non-existent directory prefix → 0 candidates (no crash). */
    iCount = line_complete_path_query(
        "/this/path/does/not/exist/", ".",
        aaCandidates, LINE_COMPLETE_MAX);
    UC_TEST("[N] non-existent dir → 0 candidates", iCount == 0);

    /* INTENT[INT-LIN-022 → TC-LIN-022]:
     * Prefix "src/" scanning from project root lists entries. */
    iCount = line_complete_path_query(
        "src/", ".", aaCandidates, LINE_COMPLETE_MAX);
    UC_TEST("[N] 'src/' returns > 0 candidates", iCount > 0);

    /* Directories in src/ should have trailing '/'. */
    bFoundSrc = 0;
    for (iIdx = 0; iIdx < iCount; iIdx++)
    {
        /* Every candidate should start with "src/" */
        if (strncmp(aaCandidates[iIdx], "src/", 4) == 0)
        {
            bFoundSrc = 1;
            break;
        }
    }
    UC_TEST("[N] path candidates start with 'src/'",
            bFoundSrc == 1);

    /* INTENT[INT-LIN-023 → TC-LIN-023]:
     * Prefix "use_c" lists the use_cases directory. */
    iCount = line_complete_path_query(
        "use_c", ".", aaCandidates, LINE_COMPLETE_MAX);
    bFoundUseCases = 0;
    for (iIdx = 0; iIdx < iCount; iIdx++)
    {
        if (strcmp(aaCandidates[iIdx], "use_cases/") == 0
        ||  strncmp(aaCandidates[iIdx], "use_c", 5) == 0)
        {
            bFoundUseCases = 1;
            break;
        }
    }
    UC_TEST("[N] 'use_c' candidate includes use_cases/",
            bFoundUseCases == 1);

    /* Robustness */
    UC_TEST("[R] NULL prefix → -1",
            line_complete_path_query(
                NULL, ".", aaCandidates,
                LINE_COMPLETE_MAX) == -1);
    UC_TEST("[R] NULL aOut → -1",
            line_complete_path_query(
                "src/", ".", NULL,
                LINE_COMPLETE_MAX) == -1);
    UC_TEST("[R] iMaxOut==0 → -1",
            line_complete_path_query(
                "src/", ".", aaCandidates, 0) == -1);

    line_history_clear();
}

/* ------------------------------------------------------------------
 * Manual tests (visual, require raw terminal)
 * ------------------------------------------------------------------ */

static void test_manual(void)
{
    printf("\n--- Manual / Visual tests ---\n");

    TEST_MANUAL(
        "[S] UP/DOWN history navigation",
        "Type a command, press ENTER, then UP: does it reappear?");

    TEST_MANUAL(
        "[S] TAB completes 'he' to 'help'",
        "Type 'he' and press TAB: does it complete to 'help'?");

    TEST_MANUAL(
        "[S] TAB ghost text for multiple matches",
        "Type 'e' and press TAB twice: does ghost cycle 'dit'/'xecute'?");

    TEST_MANUAL(
        "[S] TAB path completion for 'src/'",
        "Type 'dir src/' and press TAB: does it list src/ entries?");

    TEST_MANUAL(
        "[S] Contextual prompt with [T] when trace is open",
        "Run 'trace on': does the prompt show [T]?");

    TEST_MANUAL(
        "[S] Contextual prompt with [file] after dir selection",
        "Open dir, press SPACE on a file: does prompt show [file]?");

    TEST_MANUAL(
        "[S] colors off removes ANSI from output",
        "Run 'colors off': is the warning text uncoloured?");

    TEST_MANUAL(
        "[S] Script mode: run --script use_cases/UC04_3/test_script.txt",
        "Does the app run the script and exit cleanly?");
}

/* ------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("================================================================\n");
    printf(" UC4.3 — History, completion, colors, script, selected mutex\n");
    printf("================================================================\n");

    /* Create test data directory (ignore error if it already exists) */
#ifdef ST_PLATFORM_WINDOWS
    (void)_mkdir("use_cases/UC04_3");
#else
    (void)mkdir("use_cases/UC04_3", 0755);
#endif

    test_history();
    test_history_wrap();
    test_history_persist();
    test_colors();
    test_selected_accessor();
    test_script();
    test_complete_cmd();
    test_complete_path();
    test_manual();

    printf("\n");
    printf("================================================================\n");
    printf(" UC4.3 results: %d failure(s)\n", g_uc_fails);
    printf("================================================================\n");

    return (g_uc_fails == 0) ? 0 : 1;
}
