/*
 * use_case_04.c - UC4 Validation: directory tree view (dir.c) + `dir`
 *                  console command (line.c: line_cmd_dir)
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * Migration note (2026-07-17): replaces use_case_03_3.c (UC3.3 dir_open/
 * dir_close lifecycle) and use_case_04_1.c (UC4.1 bShowHidden + UX dir
 * improvements). Everything previously marked [S]/TEST_MANUAL
 * in those two files (ASCII tree rendering, selection highlight,
 * keyboard navigation, ESC/LEFT/RIGHT/SPACE/ENTER, window focus) is
 * migrated here to real, automated [N]/[R] coverage:
 *   - R26 script-debug mode drives the real line_cmd_dir() dispatch
 *     path (console `dir`/`dir -a`/`dir --select` commands).
 *   - Real Win32 events (WM_KEYDOWN/WM_CHAR/WM_LBUTTONDOWN/WM_MOUSEWHEEL,
 *     sent via SendMessageA to the real HWND obtained through
 *     gui_platform_get_native_handle()) drive the real, static
 *     dir_handle_key()/dir_handle_click()/dir_handle_scroll() code path
 *     - the same technique R26 uses for console dispatch, applied here
 *     to GUI event dispatch since dir_event_callback() is only ever
 *     invoked from the real WndProc.
 *   - R27 D2D spies replace the old TEST_MANUAL visual checks: text
 *     content of the ".." / directory / file rows and the fill-rect
 *     colour of each of the three selection layers (P11 green, P14
 *     purple, cursor blue) are asserted directly.
 *   - CTRL+SPACE (P14/P60) and ALT+LEFT/RIGHT (P10) are modifier-gated
 *     branches: dir_handle_key() reads the *real* OS modifier state via
 *     GetKeyState(), which SendMessageA cannot fake. These two cases use
 *     keybd_event() to inject a genuine, brief CTRL/ALT press bracketing
 *     the SendMessageA call - the standard Win32 UI-automation technique.
 *     This requires the test window to hold real OS focus, which
 *     dir_open() already guarantees (P16: SetForegroundWindow/SetFocus).
 *
 * TEST MATRIX - UC4:
 *   [N] Nominal    : 82 tests - dir_open/dir_close lifecycle (incl. the
 *                    szPath="" -> cwd resolution), bShowHidden filter,
 *                    real console dispatch (dir/-a/--select/--unselect),
 *                    real keyboard/mouse navigation and selection
 *                    (incl. P70 SPACE toggle-deselect), 'H'/'h' expansion
 *                    + cursor persistence (P22-style, fixed 2026-07-18),
 *                    D2D spy verification of rendered rows and selection
 *                    layers, real resize+scroll pagination against a
 *                    dedicated 14-file fixture (testdata_scroll)
 *   [R] Robustness :  8 tests - NULL pptView, bIsLineRunning==ST_FALSE,
 *                    dir_close idempotency, non-existent path (empty
 *                    tree)
 *   [S] Skipped    :  0 tests - all former manual checks now automated
 *                    (R26/R27)
 *
 * Test groups:
 *   Group 1: dir_open / dir_close lifecycle (nominal + robustness)
 *   Group 2: bShowHidden filter (P15 / dir -a)
 *   Group 3: real console dispatch via line_cmd_dir() (R26 script debug)
 *   Group 4: real window keyboard/mouse interaction + D2D spy (R27)
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"
#include "io.h"

#ifdef ST_PLATFORM_WINDOWS
#include "../win/win.h"
#endif

int       g_uc_fails = 0;
st_bool_t gIsObject  = ST_FALSE;

static ST4Ever_context_t *ptCtx;

/* Small, fixed fixture reused by every group in this file:
 *   visible.txt      (file)
 *   visible_dir/     (empty directory)
 *   .hidden_file     (hidden by '.' prefix filter)
 *   .hidden_dir/     (hidden directory by '.' prefix filter)
 */
#define UC04_TESTDATA   "./use_cases/UC04_1/testdata"
#define UC04_DUMMY_FILE "./use_cases/UC04_1/testdata/visible_dir/dummy.file"

/* Wider fixture used only by the resize+scroll pagination test
 * (INT-DIR-021): 14 flat files, zero-padded so lexical order matches
 * numeric order. Small enough to fit the default-size dir view
 * untouched (no scroll), but too many to fit once the window is
 * resized down to a handful of visible rows. */
#define UC04_TESTDATA_SCROLL "./use_cases/UC04_1/testdata_scroll"
#define UC04_SCROLL_FILE_COUNT 14

#define WIN_REPAINT_DELAY_MS    20

/* ------------------------------------------------------------------
 * Group 1: dir_open / dir_close lifecycle
 * ------------------------------------------------------------------ */

/*
 * uc04_test_lifecycle() - dir_open/dir_close nominal + robustness
 *
 * Code Coverage:
 *   dir.c:
 *   -- [DIR]1. dir_open rejects NULL pptView --
 *   -- [DIR]2. dir_open requires an initialized line context (bIsLineRunning) --
 *   -- [DIR]3. dir_open resolves the root path to cwd when szPath is empty --
 *   -- [DIR]4. dir_open loads the root's children honoring bShowHidden --
 *   -- [DIR]5. dir_open opens the GUI window and returns the populated view --
 *   -- [DIR]6. dir_close rejects NULL pptView --
 *   -- [DIR]7. dir_close is idempotent when *pptView is already NULL --
 *   -- [DIR]8. dir_close releases the window, tree and flat list --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc04_test_lifecycle(void)
{
    dir_view_t     *ptView;
    dir_view_t     *ptNull;
    line_context_t *ptLineCtx = (line_context_t *)ptCtx->ptConsoleCtx;
    st_error_t      eResult;
    char           szCwd[ST_MAX_PATH];

    printf("\n--- Test group 1: dir_open / dir_close lifecycle ---\n");

    /* -- [DIR]4. dir_open loads the root's children honoring bShowHidden -- */
    /* -- [DIR]5. dir_open opens the GUI window and returns the populated view -- */
    /* INTENT[INT-DIR-001 → TC-DIR-001...004 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * dir_open() on a valid path opens a real GUI window, loads the
     * root's children and returns a populated, non-NULL view */
    UC_INFO("(INT-DIR-001) Opening the 'dir' GUI on UC04_1 folder,"
            "with hidden files visible");
    ptView = NULL;
    eResult = dir_open(UC04_TESTDATA, ptLineCtx->bRunning, ST_TRUE, &ptView);
    UC_TEST("[N] (TC-DIR-001) dir_open(valid path) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    UC_TEST("[N] (TC-DIR-002) dir_open: *pptView != NULL", ptView != NULL);

    if (ptView != NULL)
    {
        UC_TEST("[N] (TC-DIR-003) dir_open: ptRoot != NULL and "
                "szRootPath not empty",
                ptView->ptRoot != NULL && ptView->szRootPath[0] != '\0');
        UC_TEST("[N] (TC-DIR-004) dir_open: iFlatCount == 4 "
                "(visible & hidden folders and files)",
                ptView->iFlatCount == 4);

        /* -- [DIR]8. dir_close releases the window, tree and flat list -- */
        /* INTENT[INT-DIR-002 → TC-DIR-005 → REQ-xxx-yyy → UFR-xxx-yyy]:
         * dir_close() releases the window and sets the pointer to NULL */
        UC_INFO("(INT-DIR-002) Closing the previously open 'dir' GUI");
        eResult = dir_close(&ptView);
        UC_TEST("[N] (TC-DIR-005) dir_close(valid view) -> ST_NO_ERROR "
                "and *pptView == NULL",
                eResult == ST_NO_ERROR && ptView == NULL);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-003...004) dir_open: ptRoot/iFlatCount (open failed)");
        TEST_SKIP("[N] (TC-DIR-005) dir_close(valid view) (open failed)");
    }

    /* -- [DIR]5. dir_open opens the GUI window and returns the populated view -- */
    /* INTENT[INT-DIR-003 → TC-DIR-006 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * dir_open opens the GUI window again on the same path after a
     * prior close */
    UC_INFO("(INT-DIR-003) Opening the 'dir' GUI on UC04_1 folder again, but hiding files");
    ptView = NULL;
    eResult = dir_open(UC04_TESTDATA, ptLineCtx->bRunning, ST_TRUE, &ptView);
    UC_TEST("[N] (TC-DIR-006) dir_open second time -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    if (ptView != NULL) dir_close(&ptView);

    /* -- [DIR]3. dir_open resolves the root path to cwd when szPath is empty -- */
    /* INTENT[INT-DIR-004 → TC-DIR-007 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * dir_open(szPath="") resolves the root to the current working
     * directory instead of failing */
    ptView = NULL;
    UC_INFO("(INT-DIR-004) Opening the 'dir' GUI with no given path");
    _getcwd(szCwd, sizeof(szCwd));
    eResult = dir_open("", ptLineCtx->bRunning, ST_FALSE, &ptView);
    UC_TEST("[N] (TC-DIR-007) dir_open(szPath=\"\") -> ST_NO_ERROR, "
            "szRootPath resolved to cwd",
            eResult == ST_NO_ERROR && ptView != NULL
            && !strncmp(ptView->szRootPath, szCwd, sizeof(szCwd)));
    
    if (ptView != NULL) dir_close(&ptView);

    printf("\n--- Test group 1 (robustness) ---\n");

    /* -- [DIR]1. dir_open rejects NULL pptView -- */
    /* -- [DIR]2. dir_open requires an initialized line context (bIsLineRunning) -- */
    /* -- [DIR]6. dir_close rejects NULL pptView -- */
    /* -- [DIR]7. dir_close is idempotent when *pptView is already NULL -- */
    /* INTENT[INT-DIR-005 → TC-DIR-008...012 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * dir_open/dir_close reject invalid parameters before any side
     * effect; dir_close(&NULL) is a safe idempotent no-op */
    UC_INFO("(INT-DIR-005) Robustness tests on dir_open()/dir_close() with NULL params");
    ptView = NULL;
    eResult = dir_open(UC04_TESTDATA, ST_FALSE, ST_FALSE, &ptView);
    UC_TEST("[R] (TC-DIR-008) dir_open(bIsLineRunning=ST_FALSE) -> ST_ERROR",
            eResult == ST_ERROR);
    UC_TEST("[R] (TC-DIR-009) dir_open(bIsLineRunning=ST_FALSE): "
            "*pptView unchanged (NULL)", ptView == NULL);

    eResult = dir_open(UC04_TESTDATA, ptLineCtx->bRunning, ST_FALSE, NULL);
    UC_TEST("[R] (TC-DIR-010) dir_open(NULL pptView) -> ST_ERROR",
            eResult == ST_ERROR);

    eResult = dir_close(NULL);
    UC_TEST("[R] (TC-DIR-011) dir_close(NULL) -> ST_ERROR",
            eResult == ST_ERROR);

    ptNull  = NULL;
    eResult = dir_close(&ptNull);
    UC_TEST("[R] (TC-DIR-012) dir_close(&NULL) -> ST_NO_ERROR (idempotent)",
            eResult == ST_NO_ERROR);

    /* -- [DIR]4. dir_open loads the root's children honoring bShowHidden -- */
    /* INTENT[INT-DIR-006 → TC-DIR-013...014 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * loading the root's children for a non-existent path yields an
     * empty tree (non-fatal scan failure), not an error */
    UC_INFO("(INT-DIR-006) Robustness tests on dir_open() with non-existent path");
    ptView = NULL;
    eResult = dir_open("nonexistent_path_xyz_st4ever", ptLineCtx->bRunning,
                       ST_FALSE, &ptView);
    UC_TEST("[R] (TC-DIR-013) dir_open(non-existent path) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    if (ptView != NULL)
    {
        UC_TEST("[R] (TC-DIR-014) dir_open(non-existent): iFlatCount == 0",
                ptView->iFlatCount == 0);
        dir_close(&ptView);
    }
    else
    {
        TEST_SKIP("[R] (TC-DIR-014) iFlatCount == 0 (open failed)");
    }
}

/* ------------------------------------------------------------------
 * Group 2: bShowHidden filter (P15 / dir -a)
 * ------------------------------------------------------------------ */

/*
 * uc04_test_hidden_filter() - dir_open() bShowHidden ST_FALSE/ST_TRUE
 *
 * Code Coverage:
 *   dir.c:
 *   -- [DIR]2. dir_open requires an initialized line context (bIsLineRunning) --
 *   -- [DIR]9. P15: '.*' entries are skipped unless bShowHidden is set --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc04_test_hidden_filter(void)
{
    line_context_t *ptLineCtx = (line_context_t *)ptCtx->ptConsoleCtx;
    dir_view_t      *ptView;
    st_error_t       eResult;
    int              iFlatVisible;
    int              iFlatHidden;

    printf("\n--- Test group 2: bShowHidden filter ---\n");

    /* -- [DIR]9. P15: '.*' entries are skipped unless bShowHidden is set -- */
    /* INTENT[INT-DIR-007 → TC-DIR-015...019 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * dir_open(bShowHidden=ST_FALSE) does not include '.*' entries 
     * dir_open(bShowHidden=ST_TRUE) includes '.*' entries; the hidden
     * count is strictly greater than the visible-only count */
    UC_INFO("(INT-DIR-007) Toggling the 'dir' GUI on UC04_1 folder,"
            "with hidden files masked/visible");
    ptView = NULL;
    eResult = dir_open(UC04_TESTDATA, ptLineCtx->bRunning, ST_FALSE, &ptView);
    UC_TEST("[N] (TC-DIR-015) dir_open(bShowHidden=F) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    iFlatVisible = 0;
    if (ptView != NULL)
    {
        iFlatVisible = ptView->iFlatCount;
        UC_TEST("[N] (TC-DIR-016) bShowHidden=F: iFlatCount == 2 "
                "(visible_dir + visible.txt)", ptView->iFlatCount == 2);
        dir_close(&ptView);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-016) bShowHidden=F: iFlatCount (open failed)");
    }

    ptView = NULL;
    eResult = dir_open(UC04_TESTDATA, ptLineCtx->bRunning, ST_TRUE, &ptView);
    UC_TEST("[N] (TC-DIR-017) dir_open(bShowHidden=T) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    iFlatHidden = 0;
    if (ptView != NULL)
    {
        iFlatHidden = ptView->iFlatCount;
        UC_TEST("[N] (TC-DIR-018) bShowHidden=T: iFlatCount == 4 "
                "(hidden & visible files/folders)",
                ptView->iFlatCount == 4);
        dir_close(&ptView);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-018) bShowHidden=T: iFlatCount (open failed)");
    }
    UC_TEST("[N] (TC-DIR-019) bShowHidden=T count > bShowHidden=F count",
            iFlatHidden > iFlatVisible);

    /* -- [DIR]2. dir_open requires an initialized line context (bIsLineRunning) -- */
    /* INTENT[INT-DIR-008 → TC-DIR-020 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * bShowHidden=ST_TRUE with bIsLineRunning=ST_FALSE is still rejected */
    UC_INFO("(INT-DIR-008) Trying the GUI opening with console line not initialized");
    ptView = NULL;
    eResult = dir_open(UC04_TESTDATA, ST_FALSE, ST_TRUE, &ptView);
    UC_TEST("[R] (TC-DIR-020) dir_open(bShowHidden=T, bIsLineRunning=F) "
            "-> ST_ERROR", eResult == ST_ERROR && ptView == NULL);
}

/* ------------------------------------------------------------------
 * Group 3: real console dispatch via line_cmd_dir() (R26 script debug)
 * ------------------------------------------------------------------ */

/*
 * uc04_test_console_dispatch() - drive the real `dir`/`dir -a`/
 * `dir --select`/`dir --unselect` console commands through
 * line_parse_cmd()/line_dispatch()/line_cmd_dir() one line at a time
 * (R26).
 *
 * Code Coverage:
 *   line.c:
 *   -- [LINE]18. 'dir --select <path>' sets szSelected headlessly --
 *   -- [LINE]19. 'dir' closes any previously open view before opening a new one --
 *   -- [LINE]20. 'dir -a' opens the view with hidden files shown --
 *   -- [LINE]22. 'dir --unselect' clears szSelected headlessly (P70) --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc04_test_console_dispatch(void)
{
    static char      szPath[ST_MAX_PATH];
    FILE            *pF;
    line_context_t  *ptLineCtx = (line_context_t *)ptCtx->ptConsoleCtx;
    char             szBuf[ST_MAX_PATH];

    printf("\n--- Test group 3: real console dispatch (dir/-a/--select/--unselect) ---\n");

    /*************************************************************************/
    // Setup of the test script - 4 calls to 'dir' command with several options
    //
    ptLineCtx->bQuiet = ST_TRUE;      // Remove online info messages (see log file)

    UC_SCRIPT_CREATE(szPath, sizeof(szPath),
                      "use_cases/UC04/dir_dispatch.txt", pF);
    UC_SCRIPT_ADD_LINE(pF, "# UC4 Group 3 dir dispatch\n");
    UC_SCRIPT_ADD_LINE(pF, "dir -a --select " UC04_TESTDATA "/visible.txt\n");
    UC_SCRIPT_ADD_LINE(pF, "dir --unselect\n");
    UC_SCRIPT_ADD_LINE(pF, "dir " UC04_TESTDATA "\n");
    UC_SCRIPT_ADD_LINE(pF, "dir -a " UC04_TESTDATA "\n");
    UC_SCRIPT_CLOSE(pF);

    strncpy(ptLineCtx->szScriptFile, szPath, ST_MAX_PATH - 1);
    ptLineCtx->szScriptFile[ST_MAX_PATH - 1] = '\0';

    line_enable_script_debug();
    line_run();  // Silently skip the first #Comment line
    //
    // End of the test script setup
    /*************************************************************************/
    
    /* -- [LINE]18. 'dir --select <path>' sets szSelected headlessly -- */
    /* INTENT[INT-DIR-009 → TC-DIR-021...022 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'dir --select <path>' updates szSelected via the real
     * line_cmd_dir() dispatch, without opening a GUI window */
    UC_CHECK("(INT-DIR-009) [Chk] line_run() [dir --select]", line_run());
    memset(szBuf, 0, sizeof(szBuf));
    line_get_selected(szBuf, sizeof(szBuf));
    UC_TEST("[N] (TC-DIR-021) dir --select: szSelected contains "
            "'visible.txt'", strstr(szBuf, "visible.txt") != NULL);
    UC_TEST("[N] (TC-DIR-022) dir --select: no window opened "
            "(ptDirView == NULL)", ptLineCtx->ptDirView == NULL);

    /* -- [LINE]22. 'dir --unselect' clears szSelected headlessly (P70) -- */
    /* INTENT[INT-DIR-010 → TC-DIR-023 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'dir --unselect' clears szSelected via the real line_cmd_dir()
     * dispatch, without opening a GUI window (P70, third means of
     * resetting a selection) */
    UC_CHECK("(INT-DIR-010) [Chk] line_run() [dir --unselect]", line_run());
    memset(szBuf, 0, sizeof(szBuf));
    line_get_selected(szBuf, sizeof(szBuf));
    UC_TEST("[N] (TC-DIR-023) dir --unselect: szSelected is now empty",
            szBuf[0] == '\0');

    /* -- [LINE]19. 'dir' closes any previously open view before opening a new one -- */
    /* INTENT[INT-DIR-011 → TC-DIR-024...025 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'dir <path>' opens a real GUI window for that path, with hidden
     * files not shown by default */
    UC_CHECK("(INT-DIR-011) [Chk] line_run() [dir <path>]", line_run());
    UC_TEST("[N] (TC-DIR-024) dir <path>: ptDirView != NULL",
            ptLineCtx->ptDirView != NULL);
    if (ptLineCtx->ptDirView != NULL)
    {
        UC_TEST("[N] (TC-DIR-025) dir <path>: bShowHidden == ST_FALSE, "
                "iFlatCount == 2",
                ptLineCtx->ptDirView->bShowHidden == ST_FALSE
                && ptLineCtx->ptDirView->iFlatCount == 2);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-025) dir <path>: bShowHidden/iFlatCount");
    }

    /* -- [LINE]20. 'dir -a' opens the view with hidden files shown -- */
    /* INTENT[INT-DIR-012 → TC-DIR-026...027 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'dir -a <path>' closes the previous view (LINE15) and reopens
     * with hidden files shown */
    UC_CHECK("(INT-DIR-012) [Chk] line_run() [dir -a <path>]", line_run());
    UC_TEST("[N] (TC-DIR-026) dir -a: ptDirView != NULL",
            ptLineCtx->ptDirView != NULL);
    if (ptLineCtx->ptDirView != NULL)
    {
        UC_TEST("[N] (TC-DIR-027) dir -a: bShowHidden == ST_TRUE, "
                "iFlatCount == 4",
                ptLineCtx->ptDirView->bShowHidden == ST_TRUE
                && ptLineCtx->ptDirView->iFlatCount == 4);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-027) dir -a: bShowHidden/iFlatCount");
    }

    /*************************************************************************/
    // Epilogue of test script run
    // R26: one more line_run() call past the last physical line closes
    // the script file and resets bDebugSteps automatically.
    line_run();   // Silently run to complete the debugging steps

    /* Teardown: close the view opened by the script above */
    if (ptLineCtx->ptDirView != NULL)
    {
        dir_close(&ptLineCtx->ptDirView);
    }
    ptLineCtx->bQuiet = ST_FALSE;   // Reset on-line info messages
    //
    // Epilogue of test script run
    /*************************************************************************/
}

/* ------------------------------------------------------------------
 * Group 4: real window keyboard/mouse interaction + D2D spy (R27)
 * ------------------------------------------------------------------ */

#ifdef ST_PLATFORM_WINDOWS

/*
 * uc04_test_window_interaction() - real Win32 keyboard/mouse events
 * driving the real, static dir_handle_key()/dir_handle_click()/
 * dir_handle_scroll(), plus D2D spy verification of dir_render().
 *
 * Code Coverage:
 *   line.c:
 *   -- [LINE]19. 'dir' closes any previously open view before opening a new one --
 * 
 *   dir.c:
 *   -- [DIR]5. dir_open opens the GUI window and returns the populated view --
 *   -- [DIR]10. dir_activate_sel: ".." row navigates to the parent directory --
 *   -- [DIR]11. dir_activate_sel: directory entry toggles expand/collapse --
 *   -- [DIR]12. dir_activate_sel: file entry commits selection (P11) --
 *   -- [DIR]13. dir_handle_key: UP/DOWN move the cursor selection --
 *   -- [DIR]14. dir_handle_key: ENTER activates the current selection --
 *   -- [DIR]15. dir_handle_key: SPACE selects and clears multi-selection (P13/P60) --
 *   -- [DIR]16. dir_handle_key: CTRL+SPACE toggles multi-selection on files (P14/P60) --
 *   -- [DIR]17. dir_handle_key: LEFT/RIGHT collapse/expand the selected directory (P12) --
 *   -- [DIR]18. dir_handle_key: ALT+LEFT/RIGHT navigate the history stack (P10) --
 *   -- [DIR]19. dir_handle_key: ESCAPE requests a non-blocking window close (P9) --
 *   -- [DIR]20. dir_handle_key: F5 refreshes the tree preserving expansion (P22) --
 *   -- [DIR]21. dir_handle_key: 'H'/'h' toggles hidden-file visibility (P21) --
 *   -- [DIR]22. dir_handle_click: left-click selects a row and expands directories --
 *   -- [DIR]23. dir_handle_scroll: mouse wheel adjusts iScrollOffset within bounds --
 *   -- [DIR]24. dir_render: P11 green background marks the last committed selection --
 *   -- [DIR]25. dir_render: P14 purple background marks multi-selected files --
 *   -- [DIR]26. dir_render: blue background marks the keyboard cursor selection --
 *   -- [DIR]27. dir_render: rows are drawn as ".." / "[+/-] dir/" / file name --
 *   -- [DIR]28. Dir GUI react on GUI_EVT_PAINT, first event creates renderer --
 *   -- [DIR]29. Dir GUI react on GUI_EVT_RESIZE --
 *   -- [DIR]30. Dir GUI react on GUI_EVT_KEY_DOWN and dispatch --
 *   -- [DIR]31. writes selected file path to the console selection --
 *   -- [DIR]32. dir_handle_key: SPACE on a selected entry toggles deselection (P70) --
 *   -- [DIR]33. Dir GUI react on GUI_EVT_MOUSE_DOWN and dispatch --
 *   -- [DIR]34. Dir GUI react on GUI_EVT_SCROLL and dispatch --
 *
 *   win_gui.c:
 *   -- [EVT]1. Inject a real WM_KEYDOWN and wait uiEventDelayMs --
 *   -- [EVT]2. Inject a real WM_CHAR and wait uiEventDelayMs --
 *   -- [EVT]3. Inject a real WM_LBUTTONDOWN and wait uiEventDelayMs --
 *   -- [EVT]4. Inject a real WM_MOUSEWHEEL and wait uiEventDelayMs --
 *   -- [EVT]5. Inject a real CTRL+<key> chord via bracketing keybd_event() --
 *   -- [EVT]6. Inject a real ALT+<key> chord via bracketing keybd_event() --
 *   -- [EVT]7. Inject a real WM_SIZE and wait uiEventDelayMs --
 *
 *   win_D2D.c:
 *   -- [SPY]13. Scan the ring buffer for the last DrawText spy containing szNeedle --
 *   -- [SPY]14. Scan the ring buffer for the last FillRectangle spy matching the exact color --
 *   -- [SPY]15. Count the number of spies matching FillRectangle exact color --
 * 
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc04_test_window_interaction(void)
{
    // Line & GUI context variables
    line_context_t      *ptLineCtx = (line_context_t *)ptCtx->ptConsoleCtx;
    gui_context_t       *ptGUICtx  = (gui_context_t *)ptCtx->ptGUICtx;
    dir_view_t          *ptView;
    struct gui_window_s *ptWnd;

    // Debug script variables
    static char      szPath[ST_MAX_PATH];
    FILE            *pF;

    // Renderer & Spies & Events variables
    HWND             hNative;
    int              iClickY;
    renderer_t       ptRenderer;
    win_d2d_ctx_t   *ptD2D;
    int              iFound;    // Index of found D2D Spy (text, fill, ...)
    int              iNewFound; // Index of another D2D Spy of the same category
    st_bool_t        bWndStillOpen; // gui_is_window_open() result (ESC teardown)

    ptGUICtx->bActiveSpies = ST_TRUE;

    printf("\n--- Test group 4: real window keyboard/mouse + D2D spy ---\n");

    /*************************************************************************/
    // Setup of the test script - one single 'dir <path>' call
    //
    ptLineCtx->bQuiet = ST_TRUE;      // Remove online info messages (see log file)

    UC_SCRIPT_CREATE(szPath, sizeof(szPath),
                      "use_cases/UC04/dir_group4.txt", pF);
    UC_SCRIPT_ADD_LINE(pF, "# UC4 Group 4 'dir' command for GUI handling \n");
    UC_SCRIPT_ADD_LINE(pF, "dir " UC04_TESTDATA "\n");
    UC_SCRIPT_ADD_LINE(pF, "dir " UC04_TESTDATA_SCROLL "\n");
    UC_SCRIPT_ADD_LINE(pF, "dir " UC04_TESTDATA "\n");
    UC_SCRIPT_ADD_LINE(pF, "where\n");
    UC_SCRIPT_CLOSE(pF);

    strncpy(ptLineCtx->szScriptFile, szPath, ST_MAX_PATH - 1);
    ptLineCtx->szScriptFile[ST_MAX_PATH - 1] = '\0';

    line_enable_script_debug();
    line_run();  // Silently skip the first #Comment line
    //
    // End of the test script setup
    /*************************************************************************/

    /* -- [LINE]19. 'dir' closes any previously open view before opening a new one -- */
    /* -- [DIR]5. dir_open opens the GUI window and returns the populated view -- */
    /* -- [DIR]28. Dir GUI react on GUI_EVT_PAINT, first event creates renderer -- */
    /* INTENT[INT-DIR-013 → TC-DIR-028 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'dir <path>' opens a real GUI window for that path */
    line_run();
    ptView = ptLineCtx->ptDirView;
    UC_TEST("(INT-DIR-013) [Chk] line_run() [dir <path>] - "
            "check for valid GUI pointer", ptView != NULL);
    if (ptView == NULL)
    {
        TEST_SKIP("[N] (TC-DIR-028...090) window interaction group "
                  "(dir_open failed)");
        return;
    }
    platform_sleep_ms(WIN_REPAINT_DELAY_MS); /* let the first PAINT create the renderer */

    ptWnd      = (struct gui_window_s *)ptView->hWnd;
    hNative    = (HWND)gui_platform_get_native_handle(ptWnd);
    ptRenderer = (renderer_t)ptWnd->ptRenderer;
    ptD2D      = (ptRenderer != NULL)
               ? (win_d2d_ctx_t *)ptRenderer->pPlatform : NULL;

    UC_TEST("[N] (TC-DIR-028) native HWND obtained and renderer is created", 
                hNative != NULL && ptD2D != NULL);
    if (hNative == NULL || ptD2D == NULL)
    {
        TEST_SKIP("[N] (TC-DIR-029...090) window interaction group "
                  "(no native HWND or no available renderer)");
        dir_close(&ptView);
        return;
    }

    /* -- [EVT]1. Inject a real WM_KEYDOWN and wait uiEventDelayMs -- */
    /* -- [DIR]30. Dir GUI react on GUI_EVT_KEY_DOWN and dispatch -- */
    /* -- [DIR]13. dir_handle_key: UP/DOWN move the cursor selection -- */
    /* -- [SPY]13. Scan the ring buffer for the last DrawText spy containing szNeedle -- */
    /* INTENT[INT-DIR-014 → TC-DIR-029...032 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * real WM_KEYDOWN(VK_DOWN)/WM_KEYDOWN(VK_UP) events move
     * ptView->iSelectedFlat through the real dir_handle_key() code
     * path (default selection is -1, the ".." row) */
    UC_INFO("(INT-DIR-014) Injecting KEYDOWN events for UP/DOWN moves");
    win_D2D_spy_reset(ptD2D);
    
    UC_TEST("[N] (TC-DIR-029) initial selection is '..' (iSelectedFlat==-1)",
            ptView->iSelectedFlat == -1);
    win_evt_send_key(ptWnd, VK_DOWN);
    iFound  = win_D2D_spy_find_text("visible_dir", ptD2D);
    UC_TEST("[N] (TC-DIR-030) DOWN selects flat index 0 (visible_dir)",
            ptView->iSelectedFlat == 0 && iFound >= 0);
    win_evt_send_key(ptWnd, VK_DOWN);
    iNewFound  = win_D2D_spy_find_text("visible.txt", ptD2D);
    UC_TEST("[N] (TC-DIR-031) DOWN again selects flat index 1 (visible.txt)",
            ptView->iSelectedFlat == 1 && iNewFound >= 0);
    win_evt_send_key(ptWnd, VK_UP);
    iNewFound  = win_D2D_spy_find_text("visible_dir", ptD2D);
    UC_TEST("[N] (TC-DIR-032) UP moves back to flat index 0",
            ptView->iSelectedFlat == 0 && iNewFound >= 0 && iFound != iNewFound);

    /* -- [DIR]30. Dir GUI react on GUI_EVT_KEY_DOWN and dispatch -- */
    /* -- [DIR]17. dir_handle_key: LEFT/RIGHT collapse/expand the selected directory (P12) -- */
    /* -- [DIR]11. dir_activate_sel: directory entry toggles expand/collapse -- */
    /* -- [DIR]14. dir_handle_key: ENTER activates the current selection -- */
    /* INTENT[INT-DIR-015 → TC-DIR-033...036 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * on the selected directory row (visible_dir, flat index 0): RIGHT
     * expands, LEFT collapses, ENTER toggles again via dir_activate_sel */
    UC_INFO("(INT-DIR-015) Injecting KEYDOWN events on folders"
            " for LEFT/RIGHT/ENTER for expand/collapse");
    win_D2D_spy_reset(ptD2D);
    
    win_evt_send_key(ptWnd, VK_RIGHT);
    iFound    = win_D2D_spy_find_text("[-] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("file002", ptD2D);
    UC_TEST("[N] (TC-DIR-033) RIGHT expands visible_dir : 4 visible files"
            " and [-] indicator is shown",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE &&
            ptView->iFlatCount == 4 && iFound >= 0 && iNewFound >= 0);

    win_D2D_spy_reset(ptD2D);
    win_evt_send_key(ptWnd, VK_LEFT);
    iFound    = win_D2D_spy_find_text("[+] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("file002", ptD2D);
    UC_TEST("[N] (TC-DIR-034) LEFT collapses visible_dir : 2 visible files"
            " and [+] indicator is shown",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE &&
            ptView->iFlatCount == 2 && iFound >= 0 && iNewFound == -1);

    win_D2D_spy_reset(ptD2D);
    win_evt_send_key(ptWnd, VK_RETURN);
    iFound    = win_D2D_spy_find_text("[-] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("file001", ptD2D);
    UC_TEST("[N] (TC-DIR-035) ENTER toggles visible_dir back to expanded",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE &&
            ptView->iFlatCount == 4 && iFound >= 0 && iNewFound >= 0);
    
    win_D2D_spy_reset(ptD2D);
    win_evt_send_key(ptWnd, VK_RETURN);
    iFound    = win_D2D_spy_find_text("[+] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("file001", ptD2D);
    UC_TEST("[N] (TC-DIR-036) ENTER toggles visible_dir back to collapsed",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE &&
            ptView->iFlatCount == 2 && iFound >= 0 && iNewFound == -1);

    /* -- [DIR]12. dir_activate_sel: file entry commits selection (P11) -- */
    /* -- [DIR]14. dir_handle_key: ENTER activates the current selection -- */
    /* -- [DIR]30. Dir GUI react on GUI_EVT_KEY_DOWN and dispatch -- */
    /* -- [DIR]31. writes selected file path to the console selection -- */
    /* -- [DIR]32. dir_handle_key: SPACE on the already-selected entry toggles deselection (P70) -- */
    /* INTENT[INT-DIR-016 → TC-DIR-037...041 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * ENTER on a file row commits the selection to both
     * ptView->szLastSelected (P11) and the console's szSelected */
    UC_INFO("(INT-DIR-016) Injecting ENTER keydown events on a file for selection");
    win_D2D_spy_reset(ptD2D);

    win_evt_send_key(ptWnd, VK_DOWN); /* -> flat index 1 (visible.txt) */
    iFound  = win_D2D_spy_find_text("visible.txt", ptD2D);
    UC_TEST("[N] (TC-DIR-037) DOWN highlights flat index 1 (visible.txt) without selecting it",
            ptView->iSelectedFlat == 1 && iFound >= 0 && ptLineCtx->szSelected[0] == '\0');

    win_evt_send_key(ptWnd, VK_RETURN);
    UC_TEST("[N] (TC-DIR-038) ENTER on file: szLastSelected has "
            "'visible.txt'", strstr(ptView->szLastSelected, "visible.txt")
            != NULL);
    UC_TEST("[N] (TC-DIR-039) ENTER on file: console szSelected has "
                "'visible.txt'", strstr(ptLineCtx->szSelected, "visible.txt") != NULL);
    
                win_evt_send_key(ptWnd, VK_RETURN);
    UC_TEST("[N] (TC-DIR-040) ENTER again on file: console szSelected still has "
                "'visible.txt'", strstr(ptLineCtx->szSelected, "visible.txt") != NULL);
    
                win_evt_send_key(ptWnd, VK_SPACE);
    UC_TEST("[N] (TC-DIR-041) SPACE toggles console szSelected", 
                 ptLineCtx->szSelected[0] == '\0');
    
    /* -- [DIR]14. dir_handle_key: ENTER activates the current selection -- */
    /* -- [DIR]15. dir_handle_key: SPACE selects and clears multi-selection (P13/P60) -- */
    /* -- [DIR]31. writes selected file path to the console selection -- */
    /* -- [DIR]32. dir_handle_key: SPACE on the already-selected entry toggles deselection (P70) -- */
    /* INTENT[INT-DIR-017 → TC-DIR-042...048 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * dir_handle_key() SPACE (no CTRL) selects the directory row and
     * clears any multi-selection, without toggling expand/collapse */
    UC_INFO("(INT-DIR-017) Injecting SPACE on directory for selection");
    
    win_evt_send_key(ptWnd, VK_UP); /* -> flat index 0 (visible_dir) */
    win_evt_send_key(ptWnd, VK_SPACE);
    UC_TEST("[N] (TC-DIR-042) SPACE on visible_dir: szLastSelected has "
            "'visible_dir'", strstr(ptView->szLastSelected, "visible_dir")
            != NULL);
    UC_TEST("[N] (TC-DIR-043) SPACE did not expand/collapse the directory",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE && ptView->iFlatCount == 2);

    win_evt_send_key(ptWnd, VK_RETURN);
    UC_TEST("[N] (TC-DIR-044) RETURN expand the directory",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE && ptView->iFlatCount == 4);
    UC_TEST("[N] (TC-DIR-045) Selection is still on visible_dir",
            strstr(ptLineCtx->szSelected, "visible_dir"));

    win_evt_send_key(ptWnd, VK_SPACE);
    UC_TEST("[N] (TC-DIR-046) SPACE toggles directory selection to none",
            ptLineCtx->szSelected[0] == '\0');

    win_evt_send_key(ptWnd, VK_RETURN);
    UC_TEST("[N] (TC-DIR-047) RETURN collapses the directory",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE && ptView->iFlatCount == 2);
    UC_TEST("[N] (TC-DIR-048) Selection is still empty",
            ptLineCtx->szSelected[0] == '\0');

    /* -- [DIR]16. dir_handle_key: CTRL+SPACE toggles multi-selection on files (P14/P60) -- */
    /* -- [EVT]5. Inject a real CTRL+<key> chord via bracketing keybd_event() -- */
    /* INTENT[INT-DIR-018 → TC-DIR-049...051 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * CTRL+SPACE on a file toggles multi-selection (P14) and clears
     * the single (green) selection first (P60 exclusivity) */
    UC_INFO("(INT-DIR-018) Injecting CTRL+SPACE for multi-selection");
    
    win_evt_send_key(ptWnd, VK_DOWN); /* -> flat index 1 (visible.txt) */
    win_evt_send_ctrl_key(ptWnd, VK_SPACE);
    UC_TEST("[N] (TC-DIR-049) CTRL+SPACE on visible.txt: "
            "iMultiSelCount == 1", ptView->iMultiSelCount == 1);
    UC_TEST("[N] (TC-DIR-050) CTRL+SPACE: aszMultiSel[0] has "
            "'visible.txt'",
            ptView->iMultiSelCount > 0
            && strstr(ptView->aszMultiSel[0], "visible.txt") != NULL);
    UC_TEST("[N] (TC-DIR-051) CTRL+SPACE clears the single "
            "selection (P60)", ptView->szLastSelected[0] == '\0');

    /* -- [DIR]15. dir_handle_key: SPACE selects and clears multi-selection (P13/P60) -- */
    /* -- [DIR]32. dir_handle_key: SPACE on a selected entry toggles deselection (P70) -- */
    /* INTENT[INT-DIR-019 → TC-DIR-052...054 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * a later plain SPACE clears the multi-selection set (P60
     * exclusivity, the other direction) pressing SPACE again on the same 
     * (already-selected) row clears the single selection instead of re-committing it */
    UC_INFO("(INT-DIR-019) Clearing CTRL+SPACE multi-selection with a plain SPACE");
    
    win_evt_send_key(ptWnd, VK_UP); /* -> flat index 0 (visible_dir) */
    win_evt_send_key(ptWnd, VK_SPACE);
    UC_TEST("[N] (TC-DIR-052) plain SPACE clears multi-selection "
            "(iMultiSelCount == 0)", ptView->iMultiSelCount == 0);
    UC_TEST("[N] (TC-DIR-053) plain SPACE re-commits visible_dir "
            "(szLastSelected has 'visible_dir')",
            strstr(ptView->szLastSelected, "visible_dir") != NULL);

    win_evt_send_key(ptWnd, VK_SPACE); /* same row (visible_dir), still selected */
    UC_TEST("[N] (TC-DIR-054) SPACE toggle on already-selected "
            "visible_dir: szLastSelected is now empty",
            ptView->szLastSelected[0] == '\0');

    /* -- [DIR]22. dir_handle_click: left-click selects a row and expands directories -- */
    /* -- [DIR]33. Dir GUI react on GUI_EVT_MOUSE_DOWN and dispatch -- */
    /* -- [EVT]3. Inject a real WM_LBUTTONDOWN and wait uiEventDelayMs -- */
    /* INTENT[INT-DIR-020 → TC-DIR-055...058 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * a left-click on the visible_dir row (row 1, below the ".." row 0)
     * selects it and toggles expand via dir_activate_sel */
    UC_INFO("(INT-DIR-020) Injecting a mouse L-Click on directory for expand/collapse");
    iClickY = ptView->iCellH + (ptView->iCellH / 2);
    
    win_D2D_spy_reset(ptD2D);
    win_evt_send_click(ptWnd, 20, iClickY);
    iFound    = win_D2D_spy_find_text("[-] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("file001", ptD2D);
    UC_TEST("[N] (TC-DIR-055) click on visible_dir row: iSelectedFlat == 0",
            ptView->iSelectedFlat == 0);
    UC_TEST("[N] (TC-DIR-056) click expands the directory",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE && ptView->iFlatCount == 4
            && iFound >= 0 && iNewFound >= 0);
    
    win_D2D_spy_reset(ptD2D);
    win_evt_send_click(ptWnd, 20, iClickY);
    iFound    = win_D2D_spy_find_text("[+] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("file001", ptD2D);
    UC_TEST("[N] (TC-DIR-057) click again on visible_dir row: iSelectedFlat == 0",
            ptView->iSelectedFlat == 0);
    UC_TEST("[N] (TC-DIR-058) L-Click collapses the open directory",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE && ptView->iFlatCount == 2
            && iFound >= 0 && iNewFound == -1);
    
    /* -- [DIR]23. dir_handle_scroll: mouse wheel adjusts iScrollOffset within bounds -- */
    /* -- [DIR]29. Dir GUI react on GUI_EVT_RESIZE -- */
    /* -- [DIR]34. Dir GUI react on GUI_EVT_SCROLL and dispatch -- */
    /* -- [EVT]4. Inject a real WM_MOUSEWHEEL and wait uiEventDelayMs -- */
    /* -- [EVT]7. Inject a real WM_SIZE and wait uiEventDelayMs -- */
    /* -- [LINE]19. 'dir' closes any previously open view before opening a new one -- */
    /* INTENT[INT-DIR-021 → TC-DIR-059...065 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Use 'testdata_scroll' folder (14 flat files) that still fits the
     * default-size dir view untouched (no scroll needed), but is too
     * big to fit once the window is resized down to a handful of rows*/
        const int iVisRowsSmall = 6;  /* Set resize to 6 rows incl. ".."  */
        int       iResizeH;

        line_run();  /* script line 3: "dir <testdata_scroll>" */
        ptView = ptLineCtx->ptDirView;
        UC_TEST("(INT-DIR-021) [Chk] line_run() [dir testdata_scroll]",
                ptView != NULL);
        if (ptView == NULL)
        {
            TEST_SKIP("[N] (TC-DIR-059...064) resize+scroll pagination "
                      "(dir_open on testdata_scroll failed)");
        }
        else
        {
            platform_sleep_ms(WIN_REPAINT_DELAY_MS); /* let the first PAINT create the renderer */

            ptWnd      = (struct gui_window_s *)ptView->hWnd;
            hNative    = (HWND)gui_platform_get_native_handle(ptWnd);
            ptRenderer = (renderer_t)ptWnd->ptRenderer;
            ptD2D      = (ptRenderer != NULL)
                       ? (win_d2d_ctx_t *)ptRenderer->pPlatform : NULL;

            UC_TEST("[N] (TC-DIR-059) default-size view shows the whole "
                    "scroll fixture without scrolling",
                    ptView->iFlatCount == UC04_SCROLL_FILE_COUNT
                    && ptView->iScrollOffset == 0
                    && win_D2D_spy_find_text("file01.txt", ptD2D) >= 0
                    && win_D2D_spy_find_text("file14.txt", ptD2D) >= 0);

            /* Shrink the window so only iVisRowsSmall rows fir at once */
            iResizeH = iVisRowsSmall * ptView->iCellH;
            win_D2D_spy_reset(ptD2D);
            win_evt_send_resize(ptWnd, 300, iResizeH);
            UC_TEST("[N] (TC-DIR-060) resize down updates iWndWidth/"
                    "iWndHeight", ptView->iWndWidth == 300
                    && ptView->iWndHeight == iResizeH);

            platform_sleep_ms(WIN_REPAINT_DELAY_MS);
            UC_TEST("[N] (TC-DIR-061) after resize, only the first page "
                    "is rendered (file01..file05 shown)",
                    win_D2D_spy_find_text("file01.txt", ptD2D) >= 0
                    && win_D2D_spy_find_text("file05.txt", ptD2D) >= 0
                    && win_D2D_spy_find_text("file06.txt", ptD2D) == -1
                    && win_D2D_spy_find_text("file14.txt", ptD2D) == -1);

            win_D2D_spy_reset(ptD2D);
            win_evt_send_wheel(ptWnd, -3);
            platform_sleep_ms(WIN_REPAINT_DELAY_MS);
            UC_TEST("[N] (TC-DIR-062) wheel scroll of -3 moves the "
                    "window down 3 rows (file03..file08 shown)",
                    ptView->iScrollOffset == 3
                    && win_D2D_spy_find_text("file01.txt", ptD2D) == -1
                    && win_D2D_spy_find_text("file02.txt", ptD2D) == -1
                    && win_D2D_spy_find_text("file03.txt", ptD2D) >= 0
                    && win_D2D_spy_find_text("file08.txt", ptD2D) >= 0
                    && win_D2D_spy_find_text("file09.txt", ptD2D) == -1);

            win_D2D_spy_reset(ptD2D);
            win_evt_send_wheel(ptWnd, -20); /* far past iMaxScroll */
            platform_sleep_ms(WIN_REPAINT_DELAY_MS);
            UC_TEST("[N] (TC-DIR-063) wheel scroll clamps at iMaxScroll "
                    "(last page: file09..file14 all shown)",
                    ptView->iScrollOffset
                        == (UC04_SCROLL_FILE_COUNT + 1 - iVisRowsSmall)
                    && win_D2D_spy_find_text("file09.txt", ptD2D) >= 0
                    && win_D2D_spy_find_text("file14.txt", ptD2D) >= 0
                    && win_D2D_spy_find_text("..", ptD2D) == -1);

            win_D2D_spy_reset(ptD2D);
            win_evt_send_wheel(ptWnd, 20); /* far past 0 */
            platform_sleep_ms(WIN_REPAINT_DELAY_MS);
            UC_TEST("[N] (TC-DIR-064) wheel scroll clamps back at 0 "
                    "('..' and file01..file05 shown again)",
                    ptView->iScrollOffset == 0
                    && win_D2D_spy_find_text("..", ptD2D) >= 0
                    && win_D2D_spy_find_text("file01.txt", ptD2D) >= 0
                    && win_D2D_spy_find_text("file05.txt", ptD2D) >= 0);


        /* Restore the shared 'testdata' fixture for the remaining
         * Group 4 tests */
        line_run();  /* script line 4: "dir <testdata>" */
        ptView = ptLineCtx->ptDirView;
        UC_TEST("[N] (TC-DIR-065) 'testdata' fixture restored after "
                "the scroll pagination excursion",
                ptView != NULL && ptView->iFlatCount == 2);
        if (ptView == NULL)
        {
            TEST_SKIP("[N] (TC-DIR-066...090) window interaction group "
                      "(dir_open on testdata restore failed)");
            return;
        }
        platform_sleep_ms(WIN_REPAINT_DELAY_MS); /* let the first PAINT create the renderer */

        ptWnd      = (struct gui_window_s *)ptView->hWnd;
        hNative    = (HWND)gui_platform_get_native_handle(ptWnd);
        ptRenderer = (renderer_t)ptWnd->ptRenderer;
        ptD2D      = (ptRenderer != NULL)
                   ? (win_d2d_ctx_t *)ptRenderer->pPlatform : NULL;

        /* A freshly dir_open()'d view always starts with the cursor on
         * ".." (iSelectedFlat == -1, dir.c [DIR]5). The rest of Group 4
         * expects the cursor already sitting on visible_dir */
        win_evt_send_key(ptWnd, VK_DOWN);
    }

    /* -- [DIR]21. dir_handle_key: 'H'/'h' toggles hidden-file visibility (P21) -- */
    /* -- [EVT]2. Inject a real WM_CHAR and wait uiEventDelayMs -- */
    /* INTENT[INT-DIR-022 → TC-DIR-066...071 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'H'/'h' toggle bShowHidden while preserving both the current
     * expansion state and the cursor position, exactly like F5 (P22). */
    UC_INFO("(INT-DIR-022) Expanding visible_dir before toggling hidden files");

    win_evt_send_key(ptWnd, VK_RETURN); /* expand visible_dir (flat idx 0) */
    UC_TEST("[N] (TC-DIR-066) RETURN expands visible_dir before the "
            "H/h persistence test",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE
            && ptView->iFlatCount == 4 && ptView->iSelectedFlat == 0);

    win_D2D_spy_reset(ptD2D);
    win_evt_send_char(ptWnd, 'H');
    iFound    = win_D2D_spy_find_text("[-] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text(".secret", ptD2D);
    UC_TEST("[N] (TC-DIR-067) 'H' toggles bShowHidden ON while "
            "visible_dir stays expanded (iFlatCount == 7)",
            ptView->bShowHidden == ST_TRUE && ptView->iFlatCount == 7
            && iFound >= 0 && iNewFound >= 0);
    UC_TEST("[N] (TC-DIR-068) 'H' preserves the cursor position "
            "instead of resetting it to -2",
            ptView->iSelectedFlat == 0);

    win_D2D_spy_reset(ptD2D);
    win_evt_send_char(ptWnd, 'h');
    iFound    = win_D2D_spy_find_text("[-] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text(".secret", ptD2D);
    UC_TEST("[N] (TC-DIR-069) 'h' toggles bShowHidden back OFF while "
            "visible_dir stays expanded",
            ptView->bShowHidden == ST_FALSE && ptView->iFlatCount == 4
            && iFound >= 0 && iNewFound == -1);
    UC_TEST("[N] (TC-DIR-070) 'h' preserves the cursor position too",
            ptView->iSelectedFlat == 0);

    win_evt_send_key(ptWnd, VK_RETURN); /* back to the baseline used below */
    UC_TEST("[N] (TC-DIR-071) RETURN re-collapses visible_dir "
            "(baseline for the next groups)",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE
            && ptView->iFlatCount == 2);

    /* -- [DIR]20. dir_handle_key: F5 refreshes the tree preserving expansion (P22) -- */
    /* INTENT[INT-DIR-023 → TC-DIR-072...076 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * F5 reloads the tree from disk and keep the current visible expansion.
     * The entry count is unchanged since nothing changed on disk */
    UC_INFO("(INT-DIR-023) Injecting F5 to reload view after adding a file in 'visible_dir'");

    win_evt_send_key(ptWnd, VK_F5);

    /* iSelectedFlat is already 0 (visible_dir) coming out of the H/h
     * block above and F5 preserves it (P22) */
    win_evt_send_key(ptWnd, VK_RETURN);
    UC_TEST("[N] (TC-DIR-072) RETURN expand the directory 'visible_dir'",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE && ptView->iFlatCount == 4);

    FILE *pfTemp = fopen(UC04_DUMMY_FILE, "w");
    fclose(pfTemp);

    win_D2D_spy_reset(ptD2D);
    win_evt_send_key(ptWnd, VK_F5);
    iFound    = win_D2D_spy_find_text("[-] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("dummy.file", ptD2D);
    UC_TEST("[N] (TC-DIR-073) F5 shows a new entry in 'visible_dir'",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE && ptView->iFlatCount == 5
            && iFound >= 0 && iNewFound >= 0);

    win_D2D_spy_reset(ptD2D);
    win_evt_send_key(ptWnd, VK_RETURN);
    iFound    = win_D2D_spy_find_text("[+] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("dummy.file", ptD2D);
    UC_TEST("[N] (TC-DIR-074) F5 shows a new entry in 'visible_dir'",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE && ptView->iFlatCount == 2
            && iFound >= 0 && iNewFound == -1);

    remove(UC04_DUMMY_FILE);
    win_evt_send_key(ptWnd, VK_F5);

    UC_TEST("[N] (TC-DIR-075) F5 does not change the current collapsed view",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE && ptView->iFlatCount == 2);
    
    win_D2D_spy_reset(ptD2D);
    win_evt_send_key(ptWnd, VK_RETURN);
    iFound    = win_D2D_spy_find_text("[-] visible_dir", ptD2D);
    iNewFound = win_D2D_spy_find_text("dummy.file", ptD2D);
    UC_TEST("[N] (TC-DIR-076) A new expand after F5 should reflect the removal",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE && ptView->iFlatCount == 4
            && iFound >= 0 && iNewFound == -1);

    /* -- [DIR]10. dir_activate_sel: ".." row navigates to the parent directory -- */
    /* -- [DIR]18. dir_handle_key: ALT+LEFT/RIGHT navigate the history stack (P10) -- */
    /* -- [EVT]6. Inject a real ALT+<key> chord via bracketing keybd_event() -- */
    /* INTENT[INT-DIR-024 → TC-DIR-077...080 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * ENTER on the ".." row navigates up (dir_navigate_up), pushing a
     * new history entry; ALT+LEFT then goes back to the original
     * root, ALT+RIGHT goes forward to the parent again */
    UC_INFO("(INT-DIR-024) Using HOME to set selection on '..' and navigate in other folder");
    win_evt_send_key(ptWnd, VK_HOME); /* -> select '..' row */
    UC_TEST("[N] (TC-DIR-077) HOME selects '..' (iSelectedFlat==-1)",
            ptView->iSelectedFlat == -1);
    {
        /* ADAPTED: session 2026-07-18 (Tonton Marcel review) - INT-
         * DIR-021 now switches fixtures via two extra real 'dir'
         * script commands (script lines 3-4), which - like every
         * 'dir' call in this binary, including Group 3's console
         * dispatch tests - grows the static navigation history
         * g_line_aDirNavHist (line.c BUG-09) that seeds this fresh
         * view's aszNavHistory/iNavHistHead. The absolute head/count
         * therefore depends on how many 'dir' commands ran earlier in
         * the whole test binary, not just in this group - assert the
         * relative +1/-1 deltas from a baseline instead of hardcoded
         * absolute values. */
        int iBaselineHead = ptView->iNavHistHead;

        win_evt_send_key(ptWnd, VK_RETURN);
        UC_TEST("[N] (TC-DIR-078) ENTER on '..': history grows by "
                "one entry (iNavHistHead == baseline + 1)",
                ptView->iNavHistHead == iBaselineHead + 1
                && ptView->iNavHistCount == ptView->iNavHistHead + 1);
        win_evt_send_alt_key(ptWnd, VK_LEFT);
        UC_TEST("[N] (TC-DIR-079) ALT+LEFT: history back "
                "(iNavHistHead back to baseline, root path restored)",
                ptView->iNavHistHead == iBaselineHead
                && strstr(ptView->szRootPath, "testdata") != NULL);
        win_evt_send_alt_key(ptWnd, VK_RIGHT);
        UC_TEST("[N] (TC-DIR-080) ALT+RIGHT: history forward "
                "(iNavHistHead == baseline + 1)",
                ptView->iNavHistHead == iBaselineHead + 1);
    }
    /* Go back once more so the remaining assertions use the small,
     * predictable testdata/ fixture again. */
    win_evt_send_alt_key(ptWnd, VK_LEFT);

    /* -- [DIR]27. dir_render: rows are drawn as ".." / "[+/-] dir/" / file name -- */
    /* -- [DIR]24. dir_render: P11 green background marks the last committed selection -- */
    /* -- [DIR]25. dir_render: P14 purple background marks multi-selected files -- */
    /* -- [DIR]26. dir_render: blue background marks the keyboard cursor selection -- */
    /* -- [SPY]14. Scan the ring buffer for the last FillRectangle spy matching the exact color -- */
    /* -- [SPY]15. Count the number of spies matching FillRectangle exact color -- */
    /* INTENT[INT-DIR-025 → TC-DIR-081...087 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * D2D spy (R27) verifies dir_render() actually draws the ".."/
     * directory/file rows with the right text and colour, and the
     * three selection background layers with the right colours */
    UC_INFO("(INT-DIR-025) Spying dir_render() output via D2D spies");
    if (ptD2D != NULL)
    {
        int iTextDD;
        int iTextDir;
        int iTextFile;

        /* Re-establish a known cursor position: the preceding F5/ENTER
         * on ".."/ALT navigation sequence leaves the selection in an
         * unpredictable state - HOME then DOWN deterministically
         * selects flat index 0 (visible_dir). */
        win_D2D_spy_reset(ptD2D);
        win_evt_send_key(ptWnd, VK_HOME);
        win_evt_send_key(ptWnd, VK_DOWN);   /* -> flat idx 0 (visible_dir) */
        win_evt_send_key(ptWnd, VK_SPACE);  /* commits green on visible_dir */
        platform_sleep_ms(WIN_REPAINT_DELAY_MS);

        iTextDD   = win_D2D_spy_find_text("..", ptD2D);
        iTextDir  = win_D2D_spy_find_text("visible_dir", ptD2D);
        iTextFile = win_D2D_spy_find_text("visible.txt", ptD2D);

        UC_TEST("[N] (TC-DIR-081) '..' row text spied", iTextDD >= 0);
        UC_TEST("[N] (TC-DIR-082) directory row text spied "
                "('visible_dir')", iTextDir >= 0);
        if (iTextDir >= 0)
        {
            const win_D2D_spy_draw_text_t *ptSpy =
                (const win_D2D_spy_draw_text_t *)
                win_D2D_get_spy(iTextDir, ptD2D, ST_WIN_D2D_SPY_DT);
            UC_TEST("[N] (TC-DIR-083) directory row shows an expand/"
                    "collapse indicator ('[+]' or '[-]')",
                    ptSpy != NULL
                    && (wcsstr(ptSpy->wcText, L"[+]") != NULL
                    ||  wcsstr(ptSpy->wcText, L"[-]") != NULL));
        }
        else
        {
            TEST_SKIP("[N] (TC-DIR-083) indicator check (no spy found)");
        }
        UC_TEST("[N] (TC-DIR-084) file row text spied ('visible.txt')",
                iTextFile >= 0);

        /* dir_render() draws green (Layer 1) then blue (Layer 3) at the
         * same rectangle, so blue fully covers green on screen even though
         * both fill_rect() calls were spied */
        int iBlueRect = win_D2D_spy_find_fill_rect_color(0.20f, 0.30f, 0.65f,
                                                         1.0f, ptD2D);
        int iGreenRect = win_D2D_spy_find_fill_rect_color(0.04f, 0.28f, 0.10f,
                                                         1.0f, ptD2D);
        const win_D2D_spy_fill_rectangle_t *ptBlueFR =
                              (const win_D2D_spy_fill_rectangle_t *)
                        win_D2D_get_spy(iBlueRect, ptD2D, ST_WIN_D2D_SPY_FR);
        const win_D2D_spy_fill_rectangle_t *ptGreenFR =
                              (const win_D2D_spy_fill_rectangle_t *)
                        win_D2D_get_spy(iGreenRect, ptD2D, ST_WIN_D2D_SPY_FR);
        
        st_bool_t bIsSameRectangle = (ptBlueFR->tRect.fX == ptGreenFR->tRect.fX) &&
                                     (ptBlueFR->tRect.fY == ptGreenFR->tRect.fY) &&
                                     (ptBlueFR->tRect.fW == ptGreenFR->tRect.fW) &&
                                     (ptBlueFR->tRect.fH == ptGreenFR->tRect.fH);

        UC_TEST("[N] (TC-DIR-085) blue cursor layer wins over green "
                "when cursor & selection share the same row",
                iBlueRect >= 0 && iGreenRect >= 0 && iBlueRect > iGreenRect &&
                bIsSameRectangle);

        /* One key up allows the visibility of both green and blue lines. 
         * In that case, both lines have different renderer_rect_t values and
         * blue line is know renderer before the green one */
        win_evt_send_key(ptWnd, VK_UP); /* cursor -> flat idx -1 (..) */
        platform_sleep_ms(WIN_REPAINT_DELAY_MS);

        iBlueRect = win_D2D_spy_find_fill_rect_color(0.20f, 0.30f, 0.65f,
                                                         1.0f, ptD2D);
        iGreenRect = win_D2D_spy_find_fill_rect_color(0.04f, 0.28f, 0.10f,
                                                         1.0f, ptD2D);
        ptBlueFR = (const win_D2D_spy_fill_rectangle_t *)
                        win_D2D_get_spy(iBlueRect, ptD2D, ST_WIN_D2D_SPY_FR);
        ptGreenFR = (const win_D2D_spy_fill_rectangle_t *)
                        win_D2D_get_spy(iGreenRect, ptD2D, ST_WIN_D2D_SPY_FR);
        
        bIsSameRectangle = (ptBlueFR->tRect.fX == ptGreenFR->tRect.fX) &&
                                     (ptBlueFR->tRect.fY == ptGreenFR->tRect.fY) &&
                                     (ptBlueFR->tRect.fW == ptGreenFR->tRect.fW) &&
                                     (ptBlueFR->tRect.fH == ptGreenFR->tRect.fH);

        UC_TEST("[N] (TC-DIR-086) after moving the cursor away, "
                "green and blue mark two distinct rows",
                iBlueRect >= 0 && iGreenRect >= 0 && iBlueRect < iGreenRect &&
                !bIsSameRectangle);

        /* Multi-select visible.txt (purple), then re-spy this layer
         * specifically. CTRL+SPACE only toggles multi-selection for
         * files (dir.c [DIR]16 guards on !bIsDir); the cursor is
         * already on visible.txt (flat idx 1) from the DOWN above. */
        win_evt_send_char(ptWnd, 'H');   /* show hidden files */
        win_evt_send_key(ptWnd, VK_END); /* cursor -> end of list (2nd file) */
        win_evt_send_ctrl_key(ptWnd, VK_SPACE);  /* Multi-select 2nd text file */
        win_evt_send_key(ptWnd, VK_UP); /* cursor up */
        win_evt_send_ctrl_key(ptWnd, VK_SPACE);  /* Multi-select 1st text file */
        
        win_D2D_spy_reset(ptD2D);
        win_evt_send_key(ptWnd, VK_UP); /* cursor up */
        
        platform_sleep_ms(WIN_REPAINT_DELAY_MS);
        UC_TEST("[N] (TC-DIR-087) purple background layer present "
                "(P14 multi-selection)", 
                 win_D2D_spy_count_fill_rect_color(0.28f, 0.15f, 0.55f, 1.0f, ptD2D) == 2);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-081...087) D2D spy checks "
                  "(no D2D context found)");
    }

    /* -- [DIR]19. dir_handle_key: ESCAPE requests a non-blocking window close (P9) -- */
    /* -- [GUI]12. gui_is_window_open: expose bOpen for owner-side self-close detection -- */
    /* -- [LINE]24. Reap any view whose window self-closed since the last command -- */
    /* INTENT[INT-DIR-026 → TC-DIR-088,089 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * ESCAPE requests a non-blocking close (P9): the native window and
     * its D2D renderer are destroyed right away, but ptLineCtx->ptDirView
     * itself stays a valid (non-NULL) pointer until the console notices
     * the self-close and finishes the teardown - line_reap_closed_views()
     * runs at the top of every dispatched command, so the very next
     * console command reaps it. */
    UC_INFO("(INT-DIR-026) ESCAPE closes the window");
    win_evt_send_key(ptWnd, VK_ESCAPE);
    platform_sleep_ms(100);

    bWndStillOpen = ST_TRUE;
    gui_is_window_open(ptView->hWnd, &bWndStillOpen);
    UC_TEST("[N] (TC-DIR-088) ESC destroys the native window right away "
            "(gui_is_window_open == FALSE)", bWndStillOpen == ST_FALSE);

    /* script line 5: "where" - any command ticks line_dispatch(), which
     * reaps the self-closed view before doing anything else. */
    line_run();
    UC_TEST("[N] (TC-DIR-089) the next console command reaps the view: "
            "ptLineCtx->ptDirView is now NULL",
            ptLineCtx->ptDirView == NULL);

    /*************************************************************************/
    // Epilogue of test script run
    // R26: one more line_run() call past the last physical line closes
    // the script file and resets bDebugSteps automatically.
    line_run();   // Silently run to complete the debugging steps
    ptLineCtx->bQuiet = ST_FALSE;   // Reset on-line info messages
    //
    // Epilogue of test script run
    /*************************************************************************/
    
    ptGUICtx->bActiveSpies = ST_FALSE;
}

#else /* !ST_PLATFORM_WINDOWS */

static void uc04_test_window_interaction(void)
{
    printf("\n--- Test group 4: real window keyboard/mouse + D2D spy ---\n");
    TEST_SKIP("[N] (TC-DIR-033...090) window interaction group "
              "(Win32-only: SendMessageA/keybd_event/D2D spy)");
}

#endif /* ST_PLATFORM_WINDOWS */

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    char *args[] = {"UC04"};

    printf("\n");
    printf("================================================================\n");
    printf(" UC4 - dir.c directory tree view + `dir` console command\n");
    printf("================================================================\n");

    ptCtx = (ST4Ever_context_t *)ST4Ever_init(1, args);
    UC_CHECK("(UC4) [Chk] Launch ST4Ever with no argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject)
    {
        uc04_test_lifecycle();
        uc04_test_hidden_filter();
        uc04_test_console_dispatch();
        uc04_test_window_interaction();
    }
    else
    {
        printf("  [SKIP] (UC4) ST_MAIN_CTX Object Check failed\n\n");
    }

    printf("\n--- Shutdown ---\n");
    ST4Ever_shutdown();

    printf("\n================================================================\n");
    if (g_uc_fails == 0)
    {
        printf(" UC4 PASSED - 0 failures\n");
    }
    else
    {
        printf(" UC4 FAILED - %d failure(s)\n", g_uc_fails);
    }
    printf("================================================================\n\n");

    return (g_uc_fails > 0) ? 1 : 0;
}
