/*
 * use_case_03_3.c - UC3.3 Validation: directory tree view
 *
 * SRTD reference: SRTD.md §5.9 — TC-DIR-*
 *
 * TEST MATRIX - UC3.3:
 *   [N] Nominal    :  8 tests - dir_open lifecycle, flat list built,
 *                               double open (reuse), valid path arg
 *   [R] Robustness :  6 tests - NULL ptLineCtx, NULL pptView, NULL both,
 *                               NULL to dir_close, empty *pptView close,
 *                               non-existent path (graceful empty tree)
 *   [S] Skipped    :  4 tests - visual rendering (run make manual):
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
    line_context_t  tCtx;
    st_error_t      eResult;

    /* ---- Setup ---------------------------------------------------- */
    trace_init(ST_FALSE);
    gui_init();
    line_init(&tCtx);

    printf("\n--- Test group 1: dir_open / dir_close lifecycle ---\n");

    /* INTENT: dir_open with valid path returns ST_NO_ERROR and a view */
    ptView  = NULL;
    eResult = dir_open(TEST_DIR_PATH, &tCtx, &ptView);
    UC_TEST("[N] dir_open(valid path) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    UC_TEST("[N] dir_open: *pptView != NULL",
            ptView != NULL);

    if (ptView != NULL)
    {
        /* INTENT: after dir_open, root and flat list are initialised */
        UC_TEST("[N] dir_open: ptRoot != NULL",
                ptView->ptRoot != NULL);
        UC_TEST("[N] dir_open: szRootPath not empty",
                ptView->szRootPath[0] != '\0');
        UC_TEST("[N] dir_open: iFlatCount >= 0",
                ptView->iFlatCount >= 0);

        /* INTENT: dir_close releases the view and nulls the pointer */
        eResult = dir_close(&ptView);
        UC_TEST("[N] dir_close(valid view) -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
        UC_TEST("[N] dir_close: *pptView == NULL after close",
                ptView == NULL);
    }
    else
    {
        /* Skip the remaining lifecycle tests if open failed */
        TEST_SKIP("[N] dir_open: ptRoot != NULL (open failed)");
        TEST_SKIP("[N] dir_open: szRootPath not empty (open failed)");
        TEST_SKIP("[N] dir_open: iFlatCount >= 0 (open failed)");
        TEST_SKIP("[N] dir_close(valid view) -> ST_NO_ERROR");
        TEST_SKIP("[N] dir_close: *pptView == NULL after close");
    }

    /* INTENT: dir_open can be called again on same path after close */
    ptView  = NULL;
    eResult = dir_open(TEST_DIR_PATH, &tCtx, &ptView);
    UC_TEST("[N] dir_open second time -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    if (ptView != NULL)
    {
        dir_close(&ptView);
    }

    printf("\n--- Test group 2: robustness ---\n");

    /* INTENT: NULL ptLineCtx must be rejected */
    ptView  = NULL;
    eResult = dir_open(TEST_DIR_PATH, NULL, &ptView);
    UC_TEST("[R] dir_open(NULL ptLineCtx) -> ST_ERROR",
            eResult == ST_ERROR);
    UC_TEST("[R] dir_open(NULL ptLineCtx): *pptView unchanged (NULL)",
            ptView == NULL);

    /* INTENT: NULL pptView must be rejected */
    eResult = dir_open(TEST_DIR_PATH, &tCtx, NULL);
    UC_TEST("[R] dir_open(NULL pptView) -> ST_ERROR",
            eResult == ST_ERROR);

    /* INTENT: both NULL parameters must be rejected */
    eResult = dir_open(TEST_DIR_PATH, NULL, NULL);
    UC_TEST("[R] dir_open(NULL, NULL) -> ST_ERROR",
            eResult == ST_ERROR);

    /* INTENT: dir_close(NULL) must be rejected */
    eResult = dir_close(NULL);
    UC_TEST("[R] dir_close(NULL) -> ST_ERROR",
            eResult == ST_ERROR);

    /* INTENT: dir_close(&NULL) is idempotent (already closed) */
    ptNull  = NULL;
    eResult = dir_close(&ptNull);
    UC_TEST("[R] dir_close(&NULL) -> ST_NO_ERROR (idempotent)",
            eResult == ST_NO_ERROR);

    /* INTENT: non-existent path opens with an empty tree (non-fatal) */
    ptView  = NULL;
    eResult = dir_open("nonexistent_path_xyz_st4ever", &tCtx, &ptView);
    UC_TEST("[R] dir_open(non-existent path) -> ST_NO_ERROR (empty tree)",
            eResult == ST_NO_ERROR);
    if (ptView != NULL)
    {
        UC_TEST("[R] dir_open(non-existent): iFlatCount == 0",
                /* ADAPTED: UC6 - real platform file API may differ */
                ptView->iFlatCount == 0);
        dir_close(&ptView);
    }
    else
    {
        TEST_SKIP("[R] dir_open(non-existent): iFlatCount == 0 (open failed)");
    }

    printf("\n--- Test group 3: visual output (manual) ---\n");

    TEST_SKIP("[S] ASCII tree rendered in window (run make manual)");
    TEST_SKIP("[S] Selected row shows highlight rect (run make manual)");
    TEST_SKIP("[S] Arrow keys scroll and move selection (run make manual)");
    TEST_SKIP("[S] Left-click on dir expands/collapses (run make manual)");

    /* ---- Teardown ------------------------------------------------- */
    line_shutdown(&tCtx);
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
