/*
 * use_case_04.c - UC4 Validation: directory tree view (dir.c) + `dir`
 *                  console command (line.c: line_cmd_dir)
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * Migration note (2026-07-17): replaces use_case_03_3.c (UC3.3 dir_open/
 * dir_close lifecycle) and use_case_04_1.c (UC4.1 bShowHidden + UX dir
 * improvements), both pre-R23 and already excluded from `make tests`
 * (see Makefile UC_FILES). Everything previously marked [S]/TEST_MANUAL
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
 *   [N] Nominal    : 53 tests - dir_open/dir_close lifecycle (incl. the
 *                    szPath="" -> cwd resolution), bShowHidden filter,
 *                    real console dispatch (dir/-a/--select), real
 *                    keyboard/mouse navigation and selection, D2D spy
 *                    verification of rendered rows and selection layers
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
#define UC04_TESTDATA  "use_cases/UC04_1/testdata"

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
 * `dir --select` console commands through line_parse_cmd()/
 * line_dispatch()/line_cmd_dir() one line at a time (R26).
 *
 * Code Coverage:
 *   line.c:
 *   -- [LINE]18. 'dir --select <path>' sets szSelected headlessly --
 *   -- [LINE]19. 'dir' closes any previously open view before opening a new one --
 *   -- [LINE]20. 'dir -a' opens the view with hidden files shown --
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

    printf("\n--- Test group 3: real console dispatch (dir/-a/--select) ---\n");

    ptLineCtx->bQuiet = ST_TRUE;

    UC_SCRIPT_CREATE(szPath, sizeof(szPath),
                      "use_cases/UC04/dir_dispatch.txt", pF);
    UC_SCRIPT_ADD_LINE(pF, "# UC4 Group 3 dir dispatch\n");
    UC_SCRIPT_ADD_LINE(pF, "dir --select " UC04_TESTDATA "/visible.txt\n");
    UC_SCRIPT_ADD_LINE(pF, "dir " UC04_TESTDATA "\n");
    UC_SCRIPT_ADD_LINE(pF, "dir -a " UC04_TESTDATA "\n");
    UC_SCRIPT_CLOSE(pF);

    strncpy(ptLineCtx->szScriptFile, szPath, ST_MAX_PATH - 1);
    ptLineCtx->szScriptFile[ST_MAX_PATH - 1] = '\0';

    line_enable_script_debug();
    line_run();  // Silently skip the first #Comment line

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

    /* -- [LINE]19. 'dir' closes any previously open view before opening a new one -- */
    /* INTENT[INT-DIR-010 → TC-DIR-023...024 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'dir <path>' opens a real GUI window for that path, with hidden
     * files not shown by default */
    UC_CHECK("(INT-DIR-010) [Chk] line_run() [dir <path>]", line_run());
    UC_TEST("[N] (TC-DIR-023) dir <path>: ptDirView != NULL",
            ptLineCtx->ptDirView != NULL);
    if (ptLineCtx->ptDirView != NULL)
    {
        UC_TEST("[N] (TC-DIR-024) dir <path>: bShowHidden == ST_FALSE, "
                "iFlatCount == 2",
                ptLineCtx->ptDirView->bShowHidden == ST_FALSE
                && ptLineCtx->ptDirView->iFlatCount == 2);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-024) dir <path>: bShowHidden/iFlatCount");
    }

    /* -- [LINE]20. 'dir -a' opens the view with hidden files shown -- */
    /* INTENT[INT-DIR-011 → TC-DIR-025...026 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'dir -a <path>' closes the previous view (LINE15) and reopens
     * with hidden files shown */
    UC_CHECK("(INT-DIR-011) [Chk] line_run() [dir -a <path>]", line_run());
    UC_TEST("[N] (TC-DIR-025) dir -a: ptDirView != NULL",
            ptLineCtx->ptDirView != NULL);
    if (ptLineCtx->ptDirView != NULL)
    {
        UC_TEST("[N] (TC-DIR-026) dir -a: bShowHidden == ST_TRUE, "
                "iFlatCount == 4",
                ptLineCtx->ptDirView->bShowHidden == ST_TRUE
                && ptLineCtx->ptDirView->iFlatCount == 4);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-026) dir -a: bShowHidden/iFlatCount");
    }

    /* R26: one more line_run() call past the last physical line closes
     * the script file and resets bDebugSteps automatically. */
    line_run();   // Silently run to complete the debugging steps

    /* Teardown: close the view opened by the script above */
    if (ptLineCtx->ptDirView != NULL)
    {
        dir_close(&ptLineCtx->ptDirView);
    }
    ptLineCtx->bQuiet = ST_FALSE;
}

/* ------------------------------------------------------------------
 * Group 4: real window keyboard/mouse interaction + D2D spy (R27)
 * ------------------------------------------------------------------ */

#ifdef ST_PLATFORM_WINDOWS

/* Send a non-printable key (arrows, ENTER, ESC, F5, SPACE...) as a
 * real WM_KEYDOWN, synchronously (SendMessageA blocks until the
 * window thread's real dir_handle_key() has returned). */
static void uc04_send_key(HWND hNative, int iVk)
{
    SendMessageA(hNative, WM_KEYDOWN, (WPARAM)iVk, 0);
    platform_sleep_ms(10);
}

/* Send a printable character (e.g. 'H'/'h') as a real WM_CHAR. */
static void uc04_send_char(HWND hNative, char cChar)
{
    SendMessageA(hNative, WM_CHAR, (WPARAM)cChar, 0);
    platform_sleep_ms(10);
}

/* Send a left mouse click at (iX,iY) client coordinates. */
static void uc04_send_click(HWND hNative, int iX, int iY)
{
    LPARAM lParam = (LPARAM)((iY << 16) | (iX & 0xFFFF));
    SendMessageA(hNative, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
    platform_sleep_ms(10);
}

/* Send one mouse-wheel notch (iNotches>0 = scroll up). */
static void uc04_send_wheel(HWND hNative, int iNotches)
{
    WPARAM wParam = (WPARAM)((short)(iNotches * WHEEL_DELTA) << 16);
    SendMessageA(hNative, WM_MOUSEWHEEL, wParam, 0);
    platform_sleep_ms(10);
}

/* CTRL+SPACE (P14/P60): dir_handle_key() reads the real OS modifier
 * state via GetKeyState(VK_CONTROL) - inject a genuine, brief CTRL
 * press bracketing the SPACE key (standard Win32 UI-automation
 * technique; requires the window to hold real OS focus, guaranteed
 * by dir_open()'s P16 SetForegroundWindow/SetFocus). */
static void uc04_send_ctrl_space(HWND hNative)
{
    keybd_event((BYTE)VK_CONTROL, 0, 0, 0);
    platform_sleep_ms(10);
    SendMessageA(hNative, WM_KEYDOWN, VK_SPACE, 0);
    platform_sleep_ms(10);
    keybd_event((BYTE)VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    platform_sleep_ms(10);
}

/* ALT+LEFT / ALT+RIGHT (P10): same real-modifier technique as
 * uc04_send_ctrl_space() above, for VK_MENU (ALT). */
static void uc04_send_alt_key(HWND hNative, int iVk)
{
    keybd_event((BYTE)VK_MENU, 0, 0, 0);
    platform_sleep_ms(10);
    SendMessageA(hNative, WM_KEYDOWN, (WPARAM)iVk, 0);
    platform_sleep_ms(10);
    keybd_event((BYTE)VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    platform_sleep_ms(10);
}

/*
 * uc04_test_window_interaction() - real Win32 keyboard/mouse events
 * driving the real, static dir_handle_key()/dir_handle_click()/
 * dir_handle_scroll(), plus D2D spy verification of dir_render().
 *
 * Code Coverage:
 *   dir.c:
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
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc04_test_window_interaction(void)
{
    line_context_t *ptLineCtx = (line_context_t *)ptCtx->ptConsoleCtx;
    gui_context_t  *ptGUICtx  = (gui_context_t *)ptCtx->ptGUICtx;
    dir_view_t      *ptView;
    st_error_t       eResult;
    struct gui_window_s *ptWnd;
    HWND             hNative;
    renderer_t       ptRenderer;
    win_d2d_ctx_t   *ptD2D;
    int              iClickY;

    printf("\n--- Test group 4: real window keyboard/mouse + D2D spy ---\n");

    ptGUICtx->bActiveSpies = ST_TRUE;

    ptView = NULL;
    eResult = dir_open(UC04_TESTDATA, ptLineCtx->bRunning, ST_FALSE, &ptView);
    UC_CHECK("(INT-DIR-013) [Chk] dir_open() for window interaction group",
             eResult);
    if (ptView == NULL)
    {
        TEST_SKIP("[N] (TC-DIR-027...060) window interaction group "
                  "(dir_open failed)");
        return;
    }
    platform_sleep_ms(250); /* let the first PAINT create the renderer */

    ptWnd      = (struct gui_window_s *)ptView->hWnd;
    hNative    = (HWND)gui_platform_get_native_handle(ptWnd);
    ptRenderer = (renderer_t)ptWnd->ptRenderer;
    ptD2D      = (ptRenderer != NULL)
               ? (win_d2d_ctx_t *)ptRenderer->pPlatform : NULL;

    /* -- [DIR]5. dir_open opens the GUI window and returns the populated view -- */
    /* INTENT[INT-DIR-013 → TC-DIR-027 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * dir_open() for the interaction-group fixture succeeds and exposes
     * a real native HWND through the opened GUI window */
    UC_TEST("[N] (TC-DIR-027) native HWND obtained", hNative != NULL);
    if (hNative == NULL)
    {
        TEST_SKIP("[N] (TC-DIR-028...060) window interaction group "
                  "(no native HWND)");
        dir_close(&ptView);
        return;
    }

    /* -- [DIR]13. dir_handle_key: UP/DOWN move the cursor selection -- */
    /* INTENT[INT-DIR-014 → TC-DIR-028...031 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * real WM_KEYDOWN(VK_DOWN)/WM_KEYDOWN(VK_UP) events move
     * ptView->iSelectedFlat through the real dir_handle_key() code
     * path (default selection is -1, the ".." row) */
    UC_TEST("[N] (TC-DIR-028) initial selection is '..' (iSelectedFlat==-1)",
            ptView->iSelectedFlat == -1);
    uc04_send_key(hNative, VK_DOWN);
    UC_TEST("[N] (TC-DIR-029) DOWN selects flat index 0 (visible_dir)",
            ptView->iSelectedFlat == 0);
    uc04_send_key(hNative, VK_DOWN);
    UC_TEST("[N] (TC-DIR-030) DOWN again selects flat index 1 (visible.txt)",
            ptView->iSelectedFlat == 1);
    uc04_send_key(hNative, VK_UP);
    UC_TEST("[N] (TC-DIR-031) UP moves back to flat index 0",
            ptView->iSelectedFlat == 0);

    /* -- [DIR]17. dir_handle_key: LEFT/RIGHT collapse/expand the selected directory (P12) -- */
    /* -- [DIR]11. dir_activate_sel: directory entry toggles expand/collapse -- */
    /* -- [DIR]14. dir_handle_key: ENTER activates the current selection -- */
    /* INTENT[INT-DIR-015 → TC-DIR-032...035 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * on the selected directory row (visible_dir, flat index 0): RIGHT
     * expands, LEFT collapses, ENTER toggles again via dir_activate_sel */
    uc04_send_key(hNative, VK_RIGHT);
    UC_TEST("[N] (TC-DIR-032) RIGHT expands visible_dir",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE);
    uc04_send_key(hNative, VK_LEFT);
    UC_TEST("[N] (TC-DIR-033) LEFT collapses visible_dir",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE);
    uc04_send_key(hNative, VK_RETURN);
    UC_TEST("[N] (TC-DIR-034) ENTER toggles visible_dir back to expanded",
            ptView->aptFlat[0].ptNode->bExpanded == ST_TRUE);
    uc04_send_key(hNative, VK_RETURN);
    UC_TEST("[N] (TC-DIR-035) ENTER toggles visible_dir back to collapsed",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE);

    /* -- [DIR]12. dir_activate_sel: file entry commits selection (P11) -- */
    /* INTENT[INT-DIR-016 → TC-DIR-036...038 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * ENTER on a file row commits the selection to both
     * ptView->szLastSelected (P11) and the console's szSelected */
    uc04_send_key(hNative, VK_DOWN); /* -> flat index 1 (visible.txt) */
    UC_TEST("[N] (TC-DIR-036) DOWN selects visible.txt",
            ptView->iSelectedFlat == 1);
    uc04_send_key(hNative, VK_RETURN);
    UC_TEST("[N] (TC-DIR-037) ENTER on file: szLastSelected has "
            "'visible.txt'", strstr(ptView->szLastSelected, "visible.txt")
            != NULL);
    {
        char szBuf[ST_MAX_PATH];
        memset(szBuf, 0, sizeof(szBuf));
        line_get_selected(szBuf, sizeof(szBuf));
        UC_TEST("[N] (TC-DIR-038) ENTER on file: console szSelected has "
                "'visible.txt'", strstr(szBuf, "visible.txt") != NULL);
    }

    /* -- [DIR]15. dir_handle_key: SPACE selects and clears multi-selection (P13/P60) -- */
    /* INTENT[INT-DIR-017 → TC-DIR-039...040 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * dir_handle_key() SPACE (no CTRL) selects the directory row and
     * clears any multi-selection, without toggling expand/collapse */
    uc04_send_key(hNative, VK_UP); /* -> flat index 0 (visible_dir) */
    uc04_send_key(hNative, VK_SPACE);
    UC_TEST("[N] (TC-DIR-039) SPACE on visible_dir: szLastSelected has "
            "'visible_dir'", strstr(ptView->szLastSelected, "visible_dir")
            != NULL);
    UC_TEST("[N] (TC-DIR-040) SPACE did not expand/collapse the directory",
            ptView->aptFlat[0].ptNode->bExpanded == ST_FALSE);

    /* -- [DIR]16. dir_handle_key: CTRL+SPACE toggles multi-selection on files (P14/P60) -- */
    /* INTENT[INT-DIR-018 → TC-DIR-041...043 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * CTRL+SPACE on a file toggles multi-selection (P14) and clears
     * the single (green) selection first (P60 exclusivity) */
    uc04_send_key(hNative, VK_DOWN); /* -> flat index 1 (visible.txt) */
    uc04_send_ctrl_space(hNative);
    UC_TEST("[N] (TC-DIR-041) CTRL+SPACE on visible.txt: "
            "iMultiSelCount == 1", ptView->iMultiSelCount == 1);
    UC_TEST("[N] (TC-DIR-042) CTRL+SPACE: aszMultiSel[0] has "
            "'visible.txt'",
            ptView->iMultiSelCount > 0
            && strstr(ptView->aszMultiSel[0], "visible.txt") != NULL);
    UC_TEST("[N] (TC-DIR-043) CTRL+SPACE clears the single (green) "
            "selection (P60)", ptView->szLastSelected[0] == '\0');

    /* -- [DIR]15. dir_handle_key: SPACE selects and clears multi-selection (P13/P60) -- */
    /* INTENT[INT-DIR-019 → TC-DIR-044 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * a later plain SPACE clears the multi-selection set (P60
     * exclusivity, the other direction) */
    uc04_send_key(hNative, VK_UP); /* -> flat index 0 (visible_dir) */
    uc04_send_key(hNative, VK_SPACE);
    UC_TEST("[N] (TC-DIR-044) plain SPACE clears multi-selection "
            "(iMultiSelCount == 0)", ptView->iMultiSelCount == 0);

    /* -- [DIR]22. dir_handle_click: left-click selects a row and expands directories -- */
    /* INTENT[INT-DIR-020 → TC-DIR-045 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * a left-click on the visible_dir row (row 1, below the ".." row 0)
     * selects it and toggles expand via dir_activate_sel */
    iClickY = ptView->iCellH + (ptView->iCellH / 2);
    uc04_send_click(hNative, 20, iClickY);
    UC_TEST("[N] (TC-DIR-045) click on visible_dir row: iSelectedFlat == 0",
            ptView->iSelectedFlat == 0);

    /* -- [DIR]23. dir_handle_scroll: mouse wheel adjusts iScrollOffset within bounds -- */
    /* INTENT[INT-DIR-021 → TC-DIR-046 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * mouse-wheel scroll is clamped to [0, iMaxScroll]; with only 3
     * visible rows the offset stays at 0 in both directions */
    uc04_send_wheel(hNative, -1);
    uc04_send_wheel(hNative, 1);
    UC_TEST("[N] (TC-DIR-046) wheel scroll stays clamped at 0 "
            "(short list)", ptView->iScrollOffset == 0);

    /* -- [DIR]21. dir_handle_key: 'H'/'h' toggles hidden-file visibility (P21) -- */
    /* INTENT[INT-DIR-022 → TC-DIR-047...048 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 'H' toggles bShowHidden on, reloading with the hidden entries
     * (iFlatCount 2 -> 4); 'h' toggles it back off */
    uc04_send_char(hNative, 'H');
    UC_TEST("[N] (TC-DIR-047) 'H' toggles bShowHidden ON, iFlatCount == 4",
            ptView->bShowHidden == ST_TRUE && ptView->iFlatCount == 4);
    uc04_send_char(hNative, 'h');
    UC_TEST("[N] (TC-DIR-048) 'h' toggles bShowHidden back OFF, "
            "iFlatCount == 2",
            ptView->bShowHidden == ST_FALSE && ptView->iFlatCount == 2);

    /* -- [DIR]20. dir_handle_key: F5 refreshes the tree preserving expansion (P22) -- */
    /* INTENT[INT-DIR-023 → TC-DIR-049 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * F5 reloads the tree from disk without crashing; the entry count
     * is unchanged since nothing changed on disk */
    uc04_send_key(hNative, VK_F5);
    UC_TEST("[N] (TC-DIR-049) F5 refresh: iFlatCount unchanged (== 2)",
            ptView->iFlatCount == 2);

    /* -- [DIR]10. dir_activate_sel: ".." row navigates to the parent directory -- */
    /* -- [DIR]18. dir_handle_key: ALT+LEFT/RIGHT navigate the history stack (P10) -- */
    /* INTENT[INT-DIR-024 → TC-DIR-050...053 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * ENTER on the ".." row navigates up (dir_navigate_up), pushing a
     * new history entry; ALT+LEFT then goes back to the original
     * root, ALT+RIGHT goes forward to the parent again */
    uc04_send_key(hNative, VK_HOME); /* -> select '..' row */
    UC_TEST("[N] (TC-DIR-050) HOME selects '..' (iSelectedFlat==-1)",
            ptView->iSelectedFlat == -1);
    uc04_send_key(hNative, VK_RETURN);
    UC_TEST("[N] (TC-DIR-051) ENTER on '..': iNavHistCount == 2",
            ptView->iNavHistCount == 2 && ptView->iNavHistHead == 1);
    uc04_send_alt_key(hNative, VK_LEFT);
    UC_TEST("[N] (TC-DIR-052) ALT+LEFT: history back "
            "(iNavHistHead == 0, root path restored)",
            ptView->iNavHistHead == 0
            && strstr(ptView->szRootPath, "testdata") != NULL);
    uc04_send_alt_key(hNative, VK_RIGHT);
    UC_TEST("[N] (TC-DIR-053) ALT+RIGHT: history forward "
            "(iNavHistHead == 1)", ptView->iNavHistHead == 1);
    /* Go back once more so the remaining assertions use the small,
     * predictable testdata/ fixture again. */
    uc04_send_alt_key(hNative, VK_LEFT);

    /* -- [DIR]27. dir_render: rows are drawn as ".." / "[+/-] dir/" / file name -- */
    /* -- [DIR]24. dir_render: P11 green background marks the last committed selection -- */
    /* -- [DIR]25. dir_render: P14 purple background marks multi-selected files -- */
    /* -- [DIR]26. dir_render: blue background marks the keyboard cursor selection -- */
    /* INTENT[INT-DIR-025 → TC-DIR-054...060 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * D2D spy (R27) verifies dir_render() actually draws the ".."/
     * directory/file rows with the right text and colour, and the
     * three selection background layers with the right colours */
    UC_INFO("(INT-DIR-025) Spying dir_render() output via D2D spies");
    if (ptD2D != NULL)
    {
        int iTextDD;
        int iTextDir;
        int iTextFile;

        /* Re-establish a known cursor position: the preceding H/h
         * toggle (which resets iSelectedFlat to -2) plus the F5/ENTER
         * on ".."/ALT navigation sequence leaves the selection in an
         * unpredictable state - HOME then DOWN deterministically
         * selects flat index 0 (visible_dir). */
        uc04_send_key(hNative, VK_HOME);
        uc04_send_key(hNative, VK_DOWN);   /* -> flat idx 0 (visible_dir) */
        uc04_send_key(hNative, VK_SPACE);  /* commits green on visible_dir */
        win_D2D_spy_reset(ptD2D);
        gui_invalidate((gui_window_t)ptWnd);
        platform_sleep_ms(150);

        iTextDD   = win_D2D_spy_find_text("..", ptD2D);
        iTextDir  = win_D2D_spy_find_text("visible_dir", ptD2D);
        iTextFile = win_D2D_spy_find_text("visible.txt", ptD2D);

        UC_TEST("[N] (TC-DIR-054) '..' row text spied", iTextDD >= 0);
        UC_TEST("[N] (TC-DIR-055) directory row text spied "
                "('visible_dir')", iTextDir >= 0);
        if (iTextDir >= 0)
        {
            const win_D2D_spy_draw_text_t *ptSpy =
                (const win_D2D_spy_draw_text_t *)
                win_D2D_get_spy(iTextDir, ptD2D, ST_WIN_D2D_SPY_DT);
            UC_TEST("[N] (TC-DIR-056) directory row shows an expand/"
                    "collapse indicator ('[+]' or '[-]')",
                    ptSpy != NULL
                    && (wcsstr(ptSpy->wcText, L"[+]") != NULL
                    ||  wcsstr(ptSpy->wcText, L"[-]") != NULL));
        }
        else
        {
            TEST_SKIP("[N] (TC-DIR-056) indicator check (no spy found)");
        }
        UC_TEST("[N] (TC-DIR-057) file row text spied ('visible.txt')",
                iTextFile >= 0);

        UC_TEST("[N] (TC-DIR-058) green background layer present "
                "(P11 last-committed selection)",
                win_D2D_spy_find_fill_rect_color(0.04f, 0.28f, 0.10f,
                                          1.0f, ptD2D) == ST_TRUE);
        UC_TEST("[N] (TC-DIR-059) blue background layer present "
                "(cursor selection)",
                win_D2D_spy_find_fill_rect_color(0.20f, 0.30f, 0.65f,
                                          1.0f, ptD2D) == ST_TRUE);

        /* Multi-select visible.txt (purple), then re-spy this layer
         * specifically. CTRL+SPACE only toggles multi-selection for
         * files (dir.c [DIR]16 guards on !bIsDir), so the cursor must
         * be moved from visible_dir (flat idx 0) to visible.txt
         * (flat idx 1) first. */
        uc04_send_key(hNative, VK_DOWN); /* -> flat idx 1 (visible.txt) */
        uc04_send_ctrl_space(hNative);
        win_D2D_spy_reset(ptD2D);
        gui_invalidate((gui_window_t)ptWnd);
        platform_sleep_ms(150);
        UC_TEST("[N] (TC-DIR-060) purple background layer present "
                "(P14 multi-selection)",
                win_D2D_spy_find_fill_rect_color(0.28f, 0.15f, 0.55f,
                                          1.0f, ptD2D) == ST_TRUE);
    }
    else
    {
        TEST_SKIP("[N] (TC-DIR-054...060) D2D spy checks "
                  "(no D2D context found)");
    }

    /* -- [DIR]19. dir_handle_key: ESCAPE requests a non-blocking window close (P9) -- */
    /* INTENT[INT-DIR-026 → TC-DIR-061 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * ESCAPE requests a non-blocking close (P9); dir_close() afterwards
     * is a safe no-op join, as documented in dir.h */
    uc04_send_key(hNative, VK_ESCAPE);
    platform_sleep_ms(200);
    eResult = dir_close(&ptView);
    UC_TEST("[N] (TC-DIR-061) dir_close() after ESC is safe "
            "(ST_NO_ERROR, *pptView == NULL)",
            eResult == ST_NO_ERROR && ptView == NULL);

    ptGUICtx->bActiveSpies = ST_FALSE;
}

#else /* !ST_PLATFORM_WINDOWS */

static void uc04_test_window_interaction(void)
{
    printf("\n--- Test group 4: real window keyboard/mouse + D2D spy ---\n");
    TEST_SKIP("[N] (TC-DIR-027...061) window interaction group "
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
