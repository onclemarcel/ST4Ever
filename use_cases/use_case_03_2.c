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

    /* INTENT[INT-RND-003 → TC-RND-007 → REQ-RND-006]:
     * renderer_create on a live window returns ST_NO_ERROR and
     * the window background switches from GDI-dark to D2D-dark */
    TEST_SKIP("[S] renderer_create on open window (run make manual)");

    /* INTENT[INT-RND-003 → TC-RND-008 → REQ-RND-007]:
     * renderer_fill_rect draws a coloured rectangle */
    TEST_SKIP("[S] renderer_fill_rect: green rect visible in window");

    /* INTENT[INT-RND-003 → TC-RND-009 → REQ-RND-007]:
     * renderer_draw_rect draws a hollow rectangle outline */
    TEST_SKIP("[S] renderer_draw_rect: cyan outline visible");

    /* INTENT[INT-RND-003 → TC-RND-010 → REQ-RND-007]:
     * renderer_draw_line draws a diagonal line */
    TEST_SKIP("[S] renderer_draw_line: yellow diagonal visible");

    /* INTENT[INT-RND-003 → TC-RND-011 → REQ-RND-007,REQ-RND-008]:
     * renderer_draw_text renders Consolas + Segoe UI text */
    TEST_SKIP("[S] renderer_draw_text: 'ST4Ever UC3.2' text visible");

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
