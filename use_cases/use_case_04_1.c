/*
 * use_case_04_1.c - UC4.1 Validation: UX dir improvements + make manual
 *
 * SRTD reference: SRTD.md §5.10 — TC-DIR4-001..014
 *
 * TEST MATRIX - UC4.1:
 *   [N] Nominal    :  6 tests - TC-DIR4-001..006
 *                               gui_request_close NULL guard
 *                               dir_open bShowHidden=ST_FALSE hides dotfiles
 *                               dir_open bShowHidden=ST_TRUE shows dotfiles
 *                               iFlatCount differs between the two modes
 *   [R] Robustness :  2 tests - TC-DIR4-007..008
 *                               gui_request_close(NULL) → ST_ERROR
 *                               dir_open with bShowHidden, NULL ptLineCtx
 *   [S] Skipped    :  6 tests - TC-DIR4-009..014 (run make manual):
 *                               ESC closes window
 *                               LEFT collapses expanded dir
 *                               RIGHT expands collapsed dir
 *                               SPACE selects without expand/collapse
 *                               ENTER still triggers action (expand/nav)
 *                               Focus auto-set on open (no alt-tab needed)
 *
 * Test data:
 *   use_cases/UC04_1/testdata/  contains:
 *     visible.txt      (normal file)
 *     visible_dir/     (normal directory)
 *     .hidden_file     (hidden by '.' prefix filter)
 *     .hidden_dir/     (hidden directory by '.' prefix filter)
 *
 *   With bShowHidden=ST_FALSE: iFlatCount == 2 (dirs first: visible_dir,
 *                              then files: visible.txt)
 *   With bShowHidden=ST_TRUE:  iFlatCount == 4 (.hidden_dir, visible_dir,
 *                              .hidden_file, visible.txt)
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"

int g_uc_fails = 0;

#define TEST_DATA_PATH  "use_cases/UC04_1/testdata"

int main(void)
{
    dir_view_t     *ptView;
    st_error_t      eResult;
    int             iFlatHidden;
    int             iFlatVisible;

    /* ---- Setup ---------------------------------------------------- */
    trace_init(ST_FALSE);
    gui_init();
    line_init("\0");

    printf("\n--- Test group 1: gui_request_close() guard ---\n");

    /* INTENT[INT-GUI-010 → TC-DIR4-001 → REQ-GUI-010 → UFR-DIR-001]:
     * gui_request_close(NULL) is rejected before any side effect */
    eResult = gui_request_close(NULL);
    UC_TEST("[R] TC-DIR4-001 gui_request_close(NULL) -> ST_ERROR",
            eResult == ST_ERROR);

    printf("\n--- Test group 2: bShowHidden filter ---\n");

    /* INTENT[INT-DIR-011 → TC-DIR4-002..004 → REQ-DIR-016 → UFR-DIR-004]:
     * dir_open with bShowHidden=ST_FALSE does not include '.*' entries */
    ptView  = NULL;
    eResult = dir_open(TEST_DATA_PATH, ST_TRUE, ST_FALSE, &ptView);
    UC_TEST("[N] TC-DIR4-002 dir_open(bShowHidden=F) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);

    iFlatVisible = 0;
    if (ptView != NULL)
    {
        iFlatVisible = ptView->iFlatCount;
        UC_TEST("[N] TC-DIR4-003 bShowHidden=F: iFlatCount == 2 "
                "(visible_dir + visible.txt)",
                ptView->iFlatCount == 2);
        dir_close(&ptView);
    }
    else
    {
        TEST_SKIP("[N] TC-DIR4-003 bShowHidden=F: iFlatCount == 2 "
                  "(open failed)");
    }

    /* INTENT[INT-DIR-012 → TC-DIR4-005..006 → REQ-DIR-016 → UFR-DIR-004]:
     * dir_open with bShowHidden=ST_TRUE includes '.*' entries */
    ptView  = NULL;
    eResult = dir_open(TEST_DATA_PATH, ST_TRUE, ST_TRUE, &ptView);
    UC_TEST("[N] TC-DIR4-004 dir_open(bShowHidden=T) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);

    iFlatHidden = 0;
    if (ptView != NULL)
    {
        iFlatHidden = ptView->iFlatCount;
        UC_TEST("[N] TC-DIR4-005 bShowHidden=T: iFlatCount == 4 "
                "(.hidden_dir + visible_dir + .hidden_file + visible.txt)",
                ptView->iFlatCount == 4);
        dir_close(&ptView);
    }
    else
    {
        TEST_SKIP("[N] TC-DIR4-005 bShowHidden=T: iFlatCount == 4 "
                  "(open failed)");
    }

    /* INTENT[INT-DIR-013 → TC-DIR4-006 → REQ-DIR-016 → UFR-DIR-004]:
     * bShowHidden=T reveals more entries than bShowHidden=F */
    UC_TEST("[N] TC-DIR4-006 bShowHidden=T count > bShowHidden=F count",
            iFlatHidden > iFlatVisible);

    printf("\n--- Test group 3: bShowHidden robustness ---\n");

    /* INTENT[INT-DIR-014 → TC-DIR4-007 → REQ-DIR-001 → UFR-DIR-001]:
     * bShowHidden=ST_TRUE with NULL ptLineCtx is still rejected */
    ptView  = NULL;
    eResult = dir_open(TEST_DATA_PATH, ST_FALSE, ST_TRUE, &ptView);
    UC_TEST("[R] TC-DIR4-007 dir_open(bShowHidden=T, NULL ptLineCtx) "
            "-> ST_ERROR",
            eResult == ST_ERROR);
    UC_TEST("[R] TC-DIR4-008 dir_open(bShowHidden=T, NULL): "
            "*pptView unchanged",
            ptView == NULL);

    printf("\n--- Test group 4: visual UX dir (manual) ---\n");

    /* Each test opens a fresh dir window so the user can interact with it
     * before answering y/n. dir_close() is safe even if ESC already closed
     * the window (join on already-terminated thread returns immediately). */

    /* INTENT[INT-DIR-015 -> TC-DIR4-009 -> REQ-DIR-017 -> UFR-DIR-005]:
     * ESC key closes the dir window without console thread action */
    ptView = NULL;
    dir_open("use_cases", ST_TRUE, ST_FALSE, &ptView);
    platform_sleep_ms(400);
    TEST_MANUAL("[S] TC-DIR4-009 ESC closes the dir window",
                "Press ESC in the open window. Did it close?");
    if (ptView != NULL) dir_close(&ptView);

    /* INTENT[INT-DIR-016 -> TC-DIR4-010 -> REQ-DIR-018 -> UFR-DIR-005]:
     * LEFT collapses an expanded directory */
    ptView = NULL;
    dir_open("use_cases", ST_TRUE, ST_FALSE, &ptView);
    platform_sleep_ms(400);
    TEST_MANUAL("[S] TC-DIR4-010 LEFT collapses expanded directory",
                "Press RIGHT to expand a dir, then LEFT. Did it collapse?");
    if (ptView != NULL) dir_close(&ptView);

    /* INTENT[INT-DIR-017 -> TC-DIR4-011 -> REQ-DIR-018 -> UFR-DIR-005]:
     * RIGHT expands a collapsed directory */
    ptView = NULL;
    dir_open("use_cases", ST_TRUE, ST_FALSE, &ptView);
    platform_sleep_ms(400);
    TEST_MANUAL("[S] TC-DIR4-011 RIGHT expands collapsed directory",
                "Select a collapsed dir, press RIGHT. Did it expand?");
    if (ptView != NULL) dir_close(&ptView);

    /* INTENT[INT-DIR-018 -> TC-DIR4-012 -> REQ-DIR-019 -> UFR-DIR-005]:
     * SPACE selects a file/dir without triggering expand or navigate */
    ptView = NULL;
    dir_open("use_cases", ST_TRUE, ST_FALSE, &ptView);
    platform_sleep_ms(400);
    TEST_MANUAL("[S] TC-DIR4-012 SPACE selects without expand",
                "Press SPACE on a collapsed directory. Did it stay "
                "collapsed while the path was selected?");
    if (ptView != NULL) dir_close(&ptView);

    /* INTENT[INT-DIR-019 -> TC-DIR4-013 -> REQ-DIR-008 -> UFR-DIR-003]:
     * ENTER still triggers expand/collapse and navigation */
    ptView = NULL;
    dir_open("use_cases", ST_TRUE, ST_FALSE, &ptView);
    platform_sleep_ms(400);
    TEST_MANUAL("[S] TC-DIR4-013 ENTER triggers expand/collapse/navigate",
                "Press ENTER on a collapsed dir. Did it expand?");
    if (ptView != NULL) dir_close(&ptView);

    /* INTENT[INT-DIR-020 -> TC-DIR4-014 -> REQ-DIR-020 -> UFR-DIR-005]:
     * window receives keyboard focus on open without requiring alt-tab */
    ptView = NULL;
    dir_open("use_cases", ST_TRUE, ST_FALSE, &ptView);
    platform_sleep_ms(200);
    TEST_MANUAL("[S] TC-DIR4-014 Window focused on open (no alt-tab needed)",
                "Try arrow keys in the window immediately after it opened. "
                "Did selection move without needing to click first?");
    if (ptView != NULL) dir_close(&ptView);

    /* ---- Teardown ------------------------------------------------- */
    line_shutdown();
    gui_shutdown();
    trace_shutdown();

    printf("\n");
    if (g_uc_fails == 0)
    {
        printf("================================================================\n");
        printf(" UC4.1 PASSED - 0 failures\n");
        printf("================================================================\n");
    }
    else
    {
        printf("================================================================\n");
        printf(" UC4.1 FAILED - %d failure(s)\n", g_uc_fails);
        printf("================================================================\n");
    }
    printf("\n");

    return (g_uc_fails > 0) ? 1 : 0;
}
