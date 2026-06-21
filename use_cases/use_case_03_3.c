/*
 * use_case_03_3.c - UC3.3 Validation: directory tree view
 *
 * SRTD reference: SRTD.md §5.9 — TC-DIR-001..020
 *
 * TEST MATRIX - UC3.3:
 *   [N] Nominal    :  8 tests - TC-DIR-001..008
 *                               dir_open lifecycle, flat list built,
 *                               double open (reuse), valid path arg
 *   [R] Robustness :  8 tests - TC-DIR-009..016
 *                               NULL ptLineCtx, NULL pptView, NULL both,
 *                               NULL to dir_close, empty *pptView close,
 *                               non-existent path (graceful empty tree x2)
 *   [S] Skipped    :  4 tests - TC-DIR-017..020 (run make manual):
 *                               ASCII tree visible, selection highlight,
 *                               keyboard navigation, mouse click expand
 *
 * Architecture notes:
 *   dir_open() calls gui_open_window() which creates a real Win32 window
 *   in a dedicated thread.  These tests call gui_init() / gui_shutdown()
 *   explicitly.  All [N] and [R] tests are headless-safe: the window is
 *   opened then immediately closed without any user interaction.
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"

int g_uc_fails = 0;

/* Dummy valid dir: use the use_cases directory itself */
#define TEST_DIR_PATH  "use_cases"

int main(void)
{
    dir_view_t     *ptView;
    dir_view_t     *ptNull;
    line_context_t *tCtx;
    st_u64_t        ullR;
    st_error_t      eResult;

    /* ---- Setup ---------------------------------------------------- */
    trace_init(ST_FALSE);
    gui_init();
    ullR = line_init("");
    UC_CHECK("[N] line_init(\"\")", ullR);
    tCtx = (void*)ullR;
    UC_CHECK_OBJ(tCtx, ST_LINE_CTX);
    if (g_uc_fails) return 1;
    
    printf("\n--- Test group 1: dir_open / dir_close lifecycle ---\n");

    /* INTENT[INT-DIR-001 → TC-DIR-001..002 → REQ-DIR-001,004 → UFR-DIR-001]:
     * dir_open with valid path returns ST_NO_ERROR and a non-NULL view */
    ptView  = NULL;
    /* ADAPTED: UC4.1 - dir_open() gained bShowHidden parameter */
    eResult = dir_open(TEST_DIR_PATH, tCtx, ST_FALSE, &ptView);
    UC_TEST("[N] TC-DIR-001 dir_open(valid path) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    UC_TEST("[N] TC-DIR-002 dir_open: *pptView != NULL",
            ptView != NULL);

    if (ptView != NULL)
    {
        /* INTENT[INT-DIR-002 → TC-DIR-003..005 → REQ-DIR-004,005 → UFR-DIR-001]:
         * after dir_open, root and flat list are initialised */
        UC_TEST("[N] TC-DIR-003 dir_open: ptRoot != NULL",
                ptView->ptRoot != NULL);
        UC_TEST("[N] TC-DIR-004 dir_open: szRootPath not empty",
                ptView->szRootPath[0] != '\0');
        UC_TEST("[N] TC-DIR-005 dir_open: iFlatCount >= 0",
                ptView->iFlatCount >= 0);

        /* INTENT[INT-DIR-003 → TC-DIR-006..007 → REQ-DIR-006,007 → UFR-DIR-001]:
         * dir_close releases the view and sets the pointer to NULL */
        eResult = dir_close(&ptView);
        UC_TEST("[N] TC-DIR-006 dir_close(valid view) -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
        UC_TEST("[N] TC-DIR-007 dir_close: *pptView == NULL after close",
                ptView == NULL);
    }
    else
    {
        /* Skip the remaining lifecycle tests if open failed */
        TEST_SKIP("[N] TC-DIR-003 dir_open: ptRoot != NULL (open failed)");
        TEST_SKIP("[N] TC-DIR-004 dir_open: szRootPath not empty (open failed)");
        TEST_SKIP("[N] TC-DIR-005 dir_open: iFlatCount >= 0 (open failed)");
        TEST_SKIP("[N] TC-DIR-006 dir_close(valid view) -> ST_NO_ERROR");
        TEST_SKIP("[N] TC-DIR-007 dir_close: *pptView == NULL after close");
    }

    /* INTENT[INT-DIR-004 → TC-DIR-008 → REQ-DIR-001 → UFR-DIR-001]:
     * dir_open can be called again on the same path after a close */
    ptView  = NULL;
    /* ADAPTED: UC4.1 - dir_open() gained bShowHidden parameter */
    eResult = dir_open(TEST_DIR_PATH, tCtx, ST_FALSE, &ptView);
    UC_TEST("[N] TC-DIR-008 dir_open second time -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    if (ptView != NULL)
    {
        dir_close(&ptView);
    }

    printf("\n--- Test group 2: robustness ---\n");

    /* INTENT[INT-DIR-005 → TC-DIR-009..010 → REQ-DIR-001 → UFR-DIR-001]:
     * NULL ptLineCtx must be rejected before any side effect */
    ptView  = NULL;
    /* ADAPTED: UC4.1 - bShowHidden parameter added */
    eResult = dir_open(TEST_DIR_PATH, NULL, ST_FALSE, &ptView);
    UC_TEST("[R] TC-DIR-009 dir_open(NULL ptLineCtx) -> ST_ERROR",
            eResult == ST_ERROR);
    UC_TEST("[R] TC-DIR-010 dir_open(NULL ptLineCtx): *pptView unchanged (NULL)",
            ptView == NULL);

    /* INTENT[INT-DIR-006 → TC-DIR-011 → REQ-DIR-001 → UFR-DIR-001]:
     * NULL pptView must be rejected before any side effect */
    /* ADAPTED: UC4.1 - bShowHidden parameter added */
    eResult = dir_open(TEST_DIR_PATH, tCtx, ST_FALSE, NULL);
    UC_TEST("[R] TC-DIR-011 dir_open(NULL pptView) -> ST_ERROR",
            eResult == ST_ERROR);

    /* INTENT[INT-DIR-007 → TC-DIR-012 → REQ-DIR-001 → UFR-DIR-001]:
     * both NULL parameters must be rejected */
    /* ADAPTED: UC4.1 - bShowHidden parameter added */
    eResult = dir_open(TEST_DIR_PATH, NULL, ST_FALSE, NULL);
    UC_TEST("[R] TC-DIR-012 dir_open(NULL, NULL) -> ST_ERROR",
            eResult == ST_ERROR);

    /* INTENT[INT-DIR-008 → TC-DIR-013 → REQ-DIR-006 → UFR-DIR-001]:
     * dir_close(NULL) must be rejected */
    eResult = dir_close(NULL);
    UC_TEST("[R] TC-DIR-013 dir_close(NULL) -> ST_ERROR",
            eResult == ST_ERROR);

    /* INTENT[INT-DIR-009 → TC-DIR-014 → REQ-DIR-006 → UFR-DIR-001]:
     * dir_close(&NULL) is idempotent — view already closed, safe no-op */
    ptNull  = NULL;
    eResult = dir_close(&ptNull);
    UC_TEST("[R] TC-DIR-014 dir_close(&NULL) -> ST_NO_ERROR (idempotent)",
            eResult == ST_NO_ERROR);

    /* INTENT[INT-DIR-010 → TC-DIR-015..016 → REQ-DIR-003 → UFR-DIR-001]:
     * non-existent path opens with an empty tree (non-fatal scan failure) */
    ptView  = NULL;
    /* ADAPTED: UC4.1 - bShowHidden parameter added */
    eResult = dir_open("nonexistent_path_xyz_st4ever", tCtx, ST_FALSE,
                       &ptView);
    UC_TEST("[R] TC-DIR-015 dir_open(non-existent path) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    if (ptView != NULL)
    {
        /* ADAPTED: UC6 - real platform file API may differ */
        UC_TEST("[R] TC-DIR-016 dir_open(non-existent): iFlatCount == 0",
                ptView->iFlatCount == 0);
        dir_close(&ptView);
    }
    else
    {
        TEST_SKIP("[R] TC-DIR-016 dir_open(non-existent): iFlatCount == 0 (open failed)");
    }

    printf("\n--- Test group 3: visual output (manual) ---\n");

    /* INTENT[INT-DIR-001 → TC-DIR-017 → REQ-DIR-004 → UFR-DIR-001]:
     * dir tree lines and file names are rendered in the window */
    /* INTENT[INT-DIR-001..002 → TC-DIR-017..020 → REQ-DIR-004,008,012]:
     * dir tree: ASCII render, highlight, keyboard, mouse click */
#ifdef ST_MANUAL_TEST
    ptView = NULL;
    dir_open("use_cases", tCtx, ST_FALSE, &ptView);
    platform_sleep_ms(500);
    TEST_MANUAL("[S] TC-DIR-017 ASCII tree rendered in window",
                "Is the use_cases/ tree shown as ASCII lines in the window?");
#else
    TEST_SKIP("[S] TC-DIR-017 ASCII tree rendered in window (run make manual)");
#endif

    /* INTENT[INT-DIR-002 → TC-DIR-018 → REQ-DIR-012 �� UFR-DIR-002]:
     * selected row shows a distinct highlight rectangle */
#ifdef ST_MANUAL_TEST
    TEST_MANUAL("[S] TC-DIR-018 Selected row has a highlight rectangle",
                "Does the selected row show a distinct highlighted background?");
#else
    TEST_SKIP("[S] TC-DIR-018 Selected row shows highlight rect (run make manual)");
#endif

    /* INTENT[INT-DIR-002 → TC-DIR-019 → REQ-DIR-012 → UFR-DIR-001]:
     * arrow keys move selection and guarantee scroll-to-visible */
#ifdef ST_MANUAL_TEST
    TEST_MANUAL("[S] TC-DIR-019 UP/DOWN arrows navigate and scroll",
                "Press UP/DOWN in the window. Does the selection move?");
#else
    TEST_SKIP("[S] TC-DIR-019 Arrow keys scroll and move selection (run make manual)");
#endif

    /* INTENT[INT-DIR-002 → TC-DIR-020 → REQ-DIR-008 → UFR-DIR-003]:
     * left-click on a directory node toggles expand/collapse */
#ifdef ST_MANUAL_TEST
    TEST_MANUAL("[S] TC-DIR-020 Left-click on dir expands/collapses",
                "Left-click on a directory row. Did it toggle expand/collapse?");
    if (ptView != NULL)
        dir_close(&ptView);
#else
    TEST_SKIP("[S] TC-DIR-020 Left-click on dir expands/collapses (run make manual)");
#endif

    /* ---- Teardown ------------------------------------------------- */
    line_shutdown();
    gui_shutdown();
    trace_shutdown();

    printf("\n");
    if (g_uc_fails == 0)
    {
        printf("================================================================\n");
        printf(" UC3.3 PASSED - 0 failures\n");
        printf("================================================================\n");
    }
    else
    {
        printf("================================================================\n");
        printf(" UC3.3 FAILED - %d failure(s)\n", g_uc_fails);
        printf("================================================================\n");
    }
    printf("\n");

    return (g_uc_fails > 0) ? 1 : 0;
}
