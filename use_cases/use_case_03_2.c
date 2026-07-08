/*
 * use_case_03_2.c - UC3.2 Validation: Direct2D renderer (renderer.c
 *                   portable layer + win_D2D.c backend)
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * Strategy (R23/R24/R25/R26, migrated from the pre-R23 trace_init()-only
 * driver - model: use_case_00.c/01.c/02.c):
 *   ptCtx is captured once in main() via ST4Ever_init() (R24 §"Accès aux
 *   contextes de modules"); gui_init() is already done by ST4Ever_init(),
 *   so groups reuse it directly - no local gui_init()/trace_init().
 *
 *   Group 3 (draw_text) uses the R26 spy capture (renderer.h,
 *   -DST_TEST_FWK): renderer_draw_text() records every call (text, rect,
 *   colour, font, alignment) into a ring buffer before forwarding to the
 *   real renderer_platform_draw_text().  A real window is still opened
 *   (renderer_draw_text() always forwards to the platform layer - a fake
 *   hCtx would crash dereferencing garbage D2D members) but the test
 *   asserts on the spy buffer instead of a human eyeballing pixels.
 *   This is a deliberate, scoped exception to R15's "GUI/renderer: skip
 *   automated, manual only" table: ST4Ever has no headless-CI runner,
 *   only a developer desktop, so opening one window during `make tests`
 *   is safe.  fill_rect/draw_rect/draw_line/window-visible (TC-RND-020..
 *   023) are not yet spied and remain TEST_MANUAL.
 *
 * TEST MATRIX - UC3.2:
 *   [N] Nominal    :  0 tests - (lifecycle/draw guards are all [R])
 *   [R] Robustness : 16 tests - NULL hWnd/phCtx/hCtx/piCellW/piCellH/
 *                               ptColor/szText/ptRect on every public
 *                               renderer_*() function
 *   [N] Nominal (spy) : 5 tests - TC-RND-017..021: real draw_text() call
 *                               captured with correct text/rect/colour/
 *                               font/alignment
 *   [S] Skipped    :  4 tests - fill_rect/draw_rect/draw_line/window
 *                               visible (run make manual): not yet spied
 *
 * Test groups:
 *   Group 1: renderer lifecycle guards - create/destroy/resize/
 *            get_font_metrics reject NULL params
 *   Group 2: draw primitive guards - begin/end_draw, fill_rect,
 *            draw_rect, draw_line, draw_text reject NULL params
 *   Group 3: real window + draw_text spy (auto) + remaining visual
 *            checks (manual)
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"
#include <string.h>

int       g_uc_fails = 0;
st_bool_t gIsObject  = ST_FALSE;

static ST4Ever_context_t *ptCtx;

/* Dummy non-NULL pointer used to test NULL-guard bypasses (never
 * dereferenced by renderer.c - all guarded calls in Group 1/2 return
 * before reaching the platform layer) */
static int g_dummy = 0;

/* ------------------------------------------------------------------
 * Group 1: renderer lifecycle guards
 * ------------------------------------------------------------------ */

/*
 * uc032_test_lifecycle_guards() - renderer_create/destroy/resize/
 *                                 get_font_metrics reject NULL params
 *
 * Code Coverage:
 *   renderer.c:
 *   -- [RND]1. renderer_create rejects NULL hWnd or phCtx --
 *   -- [RND]2. renderer_resize rejects NULL hCtx --
 *   -- [RND]3. renderer_get_font_metrics rejects NULL params --
 *   -- [RND]10. renderer_destroy rejects NULL phCtx or *phCtx --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc032_test_lifecycle_guards(void)
{
    renderer_t hR;
    st_error_t eRet;
    int        iW;
    int        iH;

    printf("\n--- Test group 1: renderer lifecycle guards ---\n");

    /* -- [RND]1. renderer_create rejects NULL hWnd or phCtx -- */
    /* INTENT[INT-RND-001 → TC-RND-001,002 → REQ-RND-001 → UFR-xxx-yyy]:
     * renderer_create(NULL hWnd) must return ST_ERROR and leave the
     * output handle untouched (still NULL) */
    hR   = NULL;
    eRet = renderer_create(NULL, &hR);
    UC_TEST("[R] (TC-RND-001) renderer_create(NULL hWnd) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[R] (TC-RND-002) renderer_create(NULL hWnd): handle stays NULL",
            hR == NULL);

    /* INTENT[INT-RND-002 → TC-RND-003 → REQ-RND-001 → UFR-xxx-yyy]:
     * renderer_create(NULL phCtx) must return ST_ERROR */
    eRet = renderer_create((gui_window_t)(void *)&g_dummy, NULL);
    UC_TEST("[R] (TC-RND-003) renderer_create(NULL phCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    /* -- [RND]10. renderer_destroy rejects NULL phCtx or *phCtx -- */
    /* INTENT[INT-RND-003 → TC-RND-004,005 → REQ-RND-002 → UFR-xxx-yyy]:
     * renderer_destroy(NULL) and renderer_destroy(&hR) with *hR==NULL
     * must both return ST_ERROR without touching any memory */
    eRet = renderer_destroy(NULL);
    UC_TEST("[R] (TC-RND-004) renderer_destroy(NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    hR   = NULL;
    eRet = renderer_destroy(&hR);
    UC_TEST("[R] (TC-RND-005) renderer_destroy(*phCtx=NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* -- [RND]2. renderer_resize rejects NULL hCtx -- */
    /* INTENT[INT-RND-004 → TC-RND-006 → REQ-RND-003 → UFR-xxx-yyy]:
     * renderer_resize(NULL hCtx) must return ST_ERROR */
    eRet = renderer_resize(NULL, 800, 600);
    UC_TEST("[R] (TC-RND-006) renderer_resize(NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* -- [RND]3. renderer_get_font_metrics rejects NULL params -- */
    /* INTENT[INT-RND-005 → TC-RND-007..009 → REQ-RND-004 → UFR-xxx-yyy]:
     * renderer_get_font_metrics() must return ST_ERROR when hCtx,
     * piCellW, or piCellH is NULL (checked independently) */
    iW = 0; iH = 0;
    eRet = renderer_get_font_metrics(NULL, RENDERER_FONT_MONO, &iW, &iH);
    UC_TEST("[R] (TC-RND-007) renderer_get_font_metrics(NULL hCtx) "
            "-> ST_ERROR", eRet == ST_ERROR);

    eRet = renderer_get_font_metrics((renderer_t)(void *)&g_dummy,
                                      RENDERER_FONT_MONO, NULL, &iH);
    UC_TEST("[R] (TC-RND-008) renderer_get_font_metrics(NULL piCellW) "
            "-> ST_ERROR", eRet == ST_ERROR);

    eRet = renderer_get_font_metrics((renderer_t)(void *)&g_dummy,
                                      RENDERER_FONT_MONO, &iW, NULL);
    UC_TEST("[R] (TC-RND-009) renderer_get_font_metrics(NULL piCellH) "
            "-> ST_ERROR", eRet == ST_ERROR);
}

/* ------------------------------------------------------------------
 * Group 2: draw primitive guards
 * ------------------------------------------------------------------ */

/*
 * uc032_test_draw_guards() - begin/end_draw, fill_rect, draw_rect,
 *                            draw_line, draw_text reject NULL params
 *
 * Code Coverage:
 *   renderer.c:
 *   -- [RND]4. renderer_begin_draw rejects NULL params --
 *   -- [RND]5. renderer_end_draw rejects NULL hCtx --
 *   -- [RND]6. renderer_fill_rect rejects NULL params --
 *   -- [RND]7. renderer_draw_rect rejects NULL params --
 *   -- [RND]8. renderer_draw_line rejects NULL params --
 *   -- [RND]9. renderer_draw_text rejects NULL params --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc032_test_draw_guards(void)
{
    renderer_color_t tColor = RENDERER_COLOR_WHITE;
    renderer_rect_t  tRect  = RENDERER_RECT(0, 0, 100, 20);
    st_error_t       eRet;

    printf("\n--- Test group 2: draw primitives guards ---\n");

    /* -- [RND]4. renderer_begin_draw rejects NULL params -- */
    /* INTENT[INT-RND-006 → TC-RND-010 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_begin_draw(NULL) must return ST_ERROR */
    eRet = renderer_begin_draw(NULL, &tColor);
    UC_TEST("[R] (TC-RND-010) renderer_begin_draw(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    /* -- [RND]5. renderer_end_draw rejects NULL hCtx -- */
    /* INTENT[INT-RND-007 → TC-RND-011 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_end_draw(NULL) must return ST_ERROR */
    eRet = renderer_end_draw(NULL);
    UC_TEST("[R] (TC-RND-011) renderer_end_draw(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    /* -- [RND]6. renderer_fill_rect rejects NULL params -- */
    /* INTENT[INT-RND-008 → TC-RND-012 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_fill_rect(NULL) must return ST_ERROR */
    eRet = renderer_fill_rect(NULL, &tRect, &tColor);
    UC_TEST("[R] (TC-RND-012) renderer_fill_rect(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    /* -- [RND]7. renderer_draw_rect rejects NULL params -- */
    /* INTENT[INT-RND-009 → TC-RND-013 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_draw_rect(NULL) must return ST_ERROR */
    eRet = renderer_draw_rect(NULL, &tRect, &tColor, 1.0f);
    UC_TEST("[R] (TC-RND-013) renderer_draw_rect(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    /* -- [RND]8. renderer_draw_line rejects NULL params -- */
    /* INTENT[INT-RND-010 → TC-RND-014 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_draw_line(NULL) must return ST_ERROR */
    eRet = renderer_draw_line(NULL, 0, 0, 10, 10, &tColor, 1.0f);
    UC_TEST("[R] (TC-RND-014) renderer_draw_line(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    /* -- [RND]9. renderer_draw_text rejects NULL params -- */
    /* INTENT[INT-RND-011 → TC-RND-015,016 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_draw_text() must return ST_ERROR when hCtx or szText
     * is NULL (checked independently) */
    eRet = renderer_draw_text(NULL, "test", &tRect, &tColor,
                               RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    UC_TEST("[R] (TC-RND-015) renderer_draw_text(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);

    eRet = renderer_draw_text((renderer_t)(void *)&g_dummy, NULL, &tRect,
                               &tColor, RENDERER_FONT_MONO,
                               RENDERER_ALIGN_LEFT);
    UC_TEST("[R] (TC-RND-016) renderer_draw_text(NULL szText) -> ST_ERROR",
            eRet == ST_ERROR);
}

/* ------------------------------------------------------------------
 * Group 3: real window - draw_text spy (auto) + remaining visual (manual)
 * ------------------------------------------------------------------ */

/* Persistent renderer handle for the paint callback */
static renderer_t g_uc032_hR = NULL;

static void uc032_paint_cb(gui_window_t   hWnd,
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
        if (g_uc032_hR == NULL)
            renderer_create(hWnd, &g_uc032_hR);
        if (g_uc032_hR == NULL)
            return;

        gui_get_size(hWnd, &iW, &iH);
        tRect.fX = 0.0f;  tRect.fY = 0.0f;
        tRect.fW = (float)iW; tRect.fH = (float)iH;
        renderer_begin_draw(g_uc032_hR, &tBg);

        tRect.fX = 20.0f; tRect.fY =  20.0f;
        tRect.fW = 200.0f; tRect.fH =  80.0f;
        renderer_fill_rect(g_uc032_hR, &tRect, &tGreen);

        tRect.fX = 20.0f; tRect.fY = 120.0f;
        tRect.fW = 200.0f; tRect.fH =  80.0f;
        renderer_draw_rect(g_uc032_hR, &tRect, &tCyan, 2.0f);

        renderer_draw_line(g_uc032_hR, 20, 220, 200, 300, &tYellow, 2.0f);

        tRect.fX = 20.0f;  tRect.fY = 320.0f;
        tRect.fW = 380.0f; tRect.fH =  30.0f;
        renderer_draw_text(g_uc032_hR, "ST4Ever UC3.2", &tRect, &tWhite,
                            RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

        renderer_end_draw(g_uc032_hR);
    }
    else if (ptEvent->eType == GUI_EVT_CLOSE)
    {
        if (g_uc032_hR != NULL)
            renderer_destroy(&g_uc032_hR);
    }
}

/*
 * uc032_test_visual() - Open a real window, let uc032_paint_cb() run
 * the real draw calls, then assert on the R26 draw_text spy buffer
 * (auto) and ask for the remaining checks not yet spied (manual).
 *
 * Code Coverage:
 *   renderer.c:
 *   -- [RND]11. renderer_draw_text spy capture (test builds only) --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc032_test_visual(void)
{
    gui_wnd_desc_t            tDesc;
    gui_window_t              hWnd;
    const renderer_spy_draw_text_t *ptSpy;

    printf("\n--- Test group 3: real window - draw_text spy + visual ---\n");

    renderer_spy_reset();

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle  = "ST4Ever - UC3.2 D2D Renderer";
    tDesc.eType    = GUI_WND_DIR;
    tDesc.pfnEvent = uc032_paint_cb;
    tDesc.pUserCtx = NULL;
    hWnd = NULL;
    gui_open_window(&tDesc, &hWnd);
    platform_sleep_ms(600);

    /* -- [RND]11. renderer_draw_text spy capture (test builds only) -- */
    /* INTENT[INT-RND-012 → TC-RND-017..021 → REQ-RND-006 → UFR-xxx-yyy]:
     * the draw_text spy capture must record the real
     * renderer_draw_text("ST4Ever UC3.2", ...) call made by
     * uc032_paint_cb() intact - same text/rect/colour/font/alignment
     * it was called with.  Windows fires more than one repaint while a
     * window is created and shown, so the spy capture legitimately
     * fires more than once with identical arguments - assert "at
     * least one" captured call, not an exact repaint count. */
    UC_TEST("[N] (TC-RND-017) at least one draw_text() call captured",
            renderer_spy_draw_text_count() >= 1);

    UC_TEST("[N] (TC-RND-018) captured text is 'ST4Ever UC3.2'",
            renderer_spy_find_text("ST4Ever UC3.2") == 0);

    ptSpy = renderer_spy_get_draw_text(0);
    UC_TEST("[N] (TC-RND-019) captured rect matches (20,320,380,30)",
            ptSpy != NULL
            && ptSpy->tRect.fX == 20.0f  && ptSpy->tRect.fY   == 320.0f
            && ptSpy->tRect.fW == 380.0f && ptSpy->tRect.fH   ==  30.0f);
    UC_TEST("[N] (TC-RND-020) captured colour is white",
            ptSpy != NULL
            && ptSpy->tColor.r == 1.0f && ptSpy->tColor.g == 1.0f
            && ptSpy->tColor.b == 1.0f && ptSpy->tColor.a == 1.0f);
    UC_TEST("[N] (TC-RND-021) captured font/alignment: MONO/LEFT",
            ptSpy != NULL
            && ptSpy->eFontId == RENDERER_FONT_MONO
            && ptSpy->eAlign  == RENDERER_ALIGN_LEFT);

    /* Not yet spied - visual-only, run make manual (R16) */
    TEST_MANUAL("[S] TC-RND-022 renderer_create on open window",
                "Is a dark window with D2D content visible?");
    TEST_MANUAL("[S] TC-RND-023 renderer_fill_rect: green filled rect",
                "Is a green filled rectangle visible in the upper area?");
    TEST_MANUAL("[S] TC-RND-024 renderer_draw_rect: cyan hollow outline",
                "Is a cyan hollow rectangle visible below the green one?");
    TEST_MANUAL("[S] TC-RND-025 renderer_draw_line: yellow diagonal",
                "Is a yellow diagonal line visible below the rectangles?");

    gui_close_window(hWnd);
    hWnd = NULL;
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    char *args[] = {"UC03_2"};

    printf("\n");
    printf("================================================================\n");
    printf(" UC3.2 - Direct2D renderer (win_D2D.c + renderer.c)\n");
    printf("================================================================\n");

    ptCtx = (ST4Ever_context_t *)ST4Ever_init(1, args);
    UC_CHECK("(UC03_2) [Chk] Launch ST4Ever with no argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject)
    {
        uc032_test_lifecycle_guards();
        uc032_test_draw_guards();
        uc032_test_visual();
    }
    else
    {
        printf("  [SKIP] (UC03_2) ST_MAIN_CTX Object Check failed\n\n");
    }

    printf("\n--- Shutdown ---\n");
    ST4Ever_shutdown();

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
