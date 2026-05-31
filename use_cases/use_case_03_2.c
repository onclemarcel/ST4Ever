/*
 * use_case_03_2.c - UC3.2 Validation: Direct2D renderer
 *
 * SRTD reference: SRTD.md §5.x — test cases TC-RND-* (to be written)
 * Traceability chain per INTENT block:
 *   INTENT[INT-RND-NNN → TC-RND-NNN → REQ-RND-NNN]: intent text
 *
 * TEST MATRIX - UC3.2:
 *   [N] Nominal    :  7 tests - renderer lifecycle (create/destroy),
 *                               NULL params on all public functions
 *   [R] Robustness :  8 tests - NULL hWnd, NULL phCtx, NULL ptColor,
 *                               NULL szText, NULL ptRect
 *   [S] Skipped    :  5 tests - visual output (run make manual):
 *                               fill_rect, draw_rect, draw_line,
 *                               draw_text, font metrics visible
 *
 * Architecture:
 *   renderer_create() requires an open gui_window_t.  The headless
 *   tests validate the NULL-guard layer in renderer.c without opening
 *   a real window.  Visual tests require a window and a live D2D
 *   render target — deferred to make manual (UC4).
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"
#include <string.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

/* Dummy non-NULL pointer used to test NULL-guard bypasses */
static int g_dummy = 0;

#ifdef ST_MANUAL_TEST
/* Persistent renderer handle for the manual paint callback */
static renderer_t g_uc32_hR = NULL;

static void uc32_paint_cb(gui_window_t   hWnd,
                           gui_event_t   *ptEvent,
                           void          *pCtx)
{
    renderer_color_t tBg     = {0.09f, 0.09f, 0.14f, 1.0f};
    renderer_color_t tGreen  = RENDERER_COLOR_GREEN;
    renderer_color_t tCyan   = RENDERER_COLOR_CYAN;
    renderer_color_t tYellow = RENDERER_COLOR_YELLOW;
    renderer_color_t tWhite  = RENDERER_COLOR_WHITE;
    renderer_rect_t  tRect;
    int              iW;
    int              iH;

    (void)pCtx;

    if (ptEvent->eType == GUI_EVT_PAINT || ptEvent->eType == GUI_EVT_RESIZE)
    {
        if (g_uc32_hR == NULL)
            renderer_create(hWnd, &g_uc32_hR);
        if (g_uc32_hR == NULL)
            return;

        gui_get_size(hWnd, &iW, &iH);
        tRect.fX = 0.0f;  tRect.fY = 0.0f;
        tRect.fW = (float)iW; tRect.fH = (float)iH;
        renderer_begin_draw(g_uc32_hR, &tBg);

        tRect.fX = 20.0f; tRect.fY =  20.0f;
        tRect.fW = 200.0f; tRect.fH =  80.0f;
        renderer_fill_rect(g_uc32_hR, &tRect, &tGreen);

        tRect.fX = 20.0f; tRect.fY = 120.0f;
        tRect.fW = 200.0f; tRect.fH =  80.0f;
        renderer_draw_rect(g_uc32_hR, &tRect, &tCyan, 2.0f);

        renderer_draw_line(g_uc32_hR, 20, 220, 200, 300, &tYellow, 2.0f);

        tRect.fX = 20.0f;  tRect.fY = 320.0f;
        tRect.fW = 380.0f; tRect.fH =  30.0f;
        renderer_draw_text(g_uc32_hR, "ST4Ever UC3.2", &tRect, &tWhite,
                            RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

        renderer_end_draw(g_uc32_hR);
    }
    else if (ptEvent->eType == GUI_EVT_CLOSE)
    {
        if (g_uc32_hR != NULL)
            renderer_destroy(&g_uc32_hR);
    }
}
#endif

int main(void)
{
    renderer_t      hR;
    st_error_t      eRet;
    int             iW;
    int             iH;

    printf("\n");
    printf("================================================================\n");
    printf(" UC3.2 - Direct2D renderer (win_D2D.c + renderer.c)\n");
    printf("================================================================\n");

    trace_init(ST_FALSE);

    /* ================================================================ */
    printf("\n--- Test group 1: renderer lifecycle ---\n");
    /* ================================================================ */

    /* INTENT[INT-RND-001 → TC-RND-001 → REQ-RND-001]:
     * renderer_create rejects NULL hWnd */
    hR   = NULL;
    eRet = renderer_create(NULL, &hR);
    UC_TEST("[R] renderer_create(NULL hWnd) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[R] renderer_create(NULL hWnd): handle stays NULL",
            hR == NULL);

    /* INTENT[INT-RND-001 → TC-RND-002 → REQ-RND-001]:
     * renderer_create rejects NULL phCtx */
    eRet = renderer_create((gui_window_t)(void *)&g_dummy, NULL);
    UC_TEST("[R] renderer_create(NULL phCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-RND-001 → TC-RND-003 → REQ-RND-002]:
     * renderer_destroy rejects NULL phCtx */
    eRet = renderer_destroy(NULL);
    UC_TEST("[R] renderer_destroy(NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-RND-001 → TC-RND-003 → REQ-RND-002]:
     * renderer_destroy rejects *phCtx == NULL */
    hR   = NULL;
    eRet = renderer_destroy(&hR);
    UC_TEST("[R] renderer_destroy(*phCtx=NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-RND-001 → TC-RND-004 → REQ-RND-003]:
     * renderer_resize rejects NULL hCtx */
    eRet = renderer_resize(NULL, 800, 600);
    UC_TEST("[R] renderer_resize(NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-RND-001 → TC-RND-005 → REQ-RND-004]:
     * renderer_get_font_metrics rejects NULL params */
    iW = 0; iH = 0;
    eRet = renderer_get_font_metrics(NULL, RENDERER_FONT_MONO,
                                      &iW, &iH);
    UC_TEST("[R] renderer_get_font_metrics(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    eRet = renderer_get_font_metrics(
               (renderer_t)(void *)&g_dummy,
               RENDERER_FONT_MONO, NULL, &iH);
    UC_TEST("[R] renderer_get_font_metrics(NULL piCellW) -> ST_ERROR",
            eRet == ST_ERROR);

    eRet = renderer_get_font_metrics(
               (renderer_t)(void *)&g_dummy,
               RENDERER_FONT_MONO, &iW, NULL);
    UC_TEST("[R] renderer_get_font_metrics(NULL piCellH) -> ST_ERROR",
            eRet == ST_ERROR);

    /* ================================================================ */
    printf("\n--- Test group 2: draw primitives NULL-guard ---\n");
    /* ================================================================ */

    /* INTENT[INT-RND-002 → TC-RND-006 → REQ-RND-005]:
     * All draw functions reject NULL renderer handle */
    {
        renderer_color_t tColor = RENDERER_COLOR_WHITE;
        renderer_rect_t  tRect  = RENDERER_RECT(0, 0, 100, 20);

        eRet = renderer_begin_draw(NULL, &tColor);
        UC_TEST("[R] renderer_begin_draw(NULL) -> ST_ERROR",
                eRet == ST_ERROR);

        eRet = renderer_end_draw(NULL);
        UC_TEST("[R] renderer_end_draw(NULL) -> ST_ERROR",
                eRet == ST_ERROR);

        eRet = renderer_fill_rect(NULL, &tRect, &tColor);
        UC_TEST("[R] renderer_fill_rect(NULL hCtx) -> ST_ERROR",
                eRet == ST_ERROR);

        eRet = renderer_draw_rect(NULL, &tRect, &tColor, 1.0f);
        UC_TEST("[R] renderer_draw_rect(NULL hCtx) -> ST_ERROR",
                eRet == ST_ERROR);

        eRet = renderer_draw_line(NULL, 0,0,10,10, &tColor, 1.0f);
        UC_TEST("[R] renderer_draw_line(NULL hCtx) -> ST_ERROR",
                eRet == ST_ERROR);

        eRet = renderer_draw_text(NULL, "test", &tRect, &tColor,
                                   RENDERER_FONT_MONO,
                                   RENDERER_ALIGN_LEFT);
        UC_TEST("[R] renderer_draw_text(NULL hCtx) -> ST_ERROR",
                eRet == ST_ERROR);

        eRet = renderer_draw_text((renderer_t)(void *)&g_dummy,
                                   NULL, &tRect, &tColor,
                                   RENDERER_FONT_MONO,
                                   RENDERER_ALIGN_LEFT);
        UC_TEST("[R] renderer_draw_text(NULL szText) -> ST_ERROR",
                eRet == ST_ERROR);
    }

    /* ================================================================ */
    printf("\n--- Test group 3: visual output (manual) ---\n");
    /* ================================================================ */

    /* INTENT[INT-RND-003 → TC-RND-007..011 → REQ-RND-006..008]:
     * renderer on a live window draws: fill_rect, draw_rect, draw_line,
     * draw_text — all visible in a D2D window */
#ifdef ST_MANUAL_TEST
    {
        gui_wnd_desc_t tManualDesc;
        gui_window_t   hManualWnd;

        gui_init();
        memset(&tManualDesc, 0, sizeof(tManualDesc));
        tManualDesc.szTitle  = "ST4Ever - UC3.2 D2D Renderer";
        tManualDesc.eType    = GUI_WND_DIR;
        tManualDesc.pfnEvent = uc32_paint_cb;
        tManualDesc.pUserCtx = NULL;
        hManualWnd = NULL;
        gui_open_window(&tManualDesc, &hManualWnd);
        platform_sleep_ms(600);

        TEST_MANUAL("[S] TC-RND-007 renderer_create on open window",
                    "Is a dark window with D2D content visible?");
        TEST_MANUAL("[S] TC-RND-008 renderer_fill_rect: green filled rect",
                    "Is a green filled rectangle visible in the upper area?");
        TEST_MANUAL("[S] TC-RND-009 renderer_draw_rect: cyan hollow outline",
                    "Is a cyan hollow rectangle visible below the green one?");
        TEST_MANUAL("[S] TC-RND-010 renderer_draw_line: yellow diagonal",
                    "Is a yellow diagonal line visible below the rectangles?");
        TEST_MANUAL("[S] TC-RND-011 renderer_draw_text: 'ST4Ever UC3.2'",
                    "Is white text 'ST4Ever UC3.2' visible at the bottom?");

        gui_close_window(hManualWnd);
        hManualWnd = NULL;
        gui_shutdown();
    }
#else
    TEST_SKIP("[S] TC-RND-007 renderer_create on open window (run make manual)");
    TEST_SKIP("[S] TC-RND-008 renderer_fill_rect: green rect visible in window");
    TEST_SKIP("[S] TC-RND-009 renderer_draw_rect: cyan outline visible");
    TEST_SKIP("[S] TC-RND-010 renderer_draw_line: yellow diagonal visible");
    TEST_SKIP("[S] TC-RND-011 renderer_draw_text: 'ST4Ever UC3.2' text visible");
#endif

    /* ================================================================ */
    printf("\n[cleanup] trace_shutdown()\n");
    trace_shutdown();

    printf("\n================================================================\n");
    if (g_uc_fails == 0)
    {
        printf(" UC3.2 PASSED - 0 failures\n");
    }
    else
    {
        printf(" UC3.2 FAILED - %d failure(s)\n", g_uc_fails);
    }
    printf("================================================================\n\n");

    return (g_uc_fails > 0) ? 1 : 0;
}
