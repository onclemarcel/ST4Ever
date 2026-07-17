/*
 * use_case_03.c - UC3 Validation: GUI & Direct2D renderer (renderer.c
 *                   portable layer + win_D2D.c backend)
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * Strategy (R23/R24/R25/R26,  model: use_case_00.c/01.c/02.c):
 *   ptCtx is captured once in main() via ST4Ever_init() (R24 §"Accès aux
 *   contextes de modules"); gui_init() is already done by ST4Ever_init(),
 *   so groups reuse it directly - no local gui_init()/trace_init().
 *
 * This use_case_03.c makes use of the "D2D Spy" feature that records, on
 * request, the internal parameters sent to the platform-specific rendering
 * functions (e.g. D2D on Windows). The spied values are available through
 * the gui_context_t* ptCtx->aptWnd[idx]->ptRenderer->pPlatform pointer.
 * 
 * Introduction of a specific helper to look for the last LOG_xxx message
 * (see uc03_check_last_log_entry()) : this helper could be useful for other
 * use_cases_*.c - TODO: there may be a need to create a use_cases.c top-level
 * file for common testing functions
 *
 * TEST MATRIX - UC3:
 *   [N] Nominal    : 63 tests - Spied D2D functions (draw_text() only)
 *                               (lifecycle/draw guards are all [R]) 
 *   [R] Robustness : 35 tests - NULL hWnd/phCtx/hCtx/piCellW/piCellH/
 *                               ptColor/szText/ptRect on every public
 *                               renderer_*() function
 *   [S] Skipped    :  0 tests - manual tests are replaced by D2D spies
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

#ifdef ST_PLATFORM_WINDOWS
#include "../win/win.h"
#endif

int       g_uc_fails = 0;
st_bool_t gIsObject  = ST_FALSE;
int       g_log_last_line = 0;

static ST4Ever_context_t *ptCtx;

/* Dummy non-NULL pointer used to test NULL-guard bypasses (never
 * dereferenced by renderer.c - all guarded calls in Group 1/2 return
 * before reaching the platform layer) */
static int g_dummy = 0;

/* Test function to create a test GUI window */
static void uc03_paint_cb(gui_window_t   hWnd,
                            gui_event_t   *ptEvent,
                            void          *pCtx);

/* Helper for log file inspection */
static int uc03_check_last_log_entry(const char *szTag,
                                       const char *szContent)
{
    FILE     *pFile;
    int       iLine = 0;
    char      szLine[TRACE_VIEW_LINE_LEN];
    
    if (szTag == NULL || szContent == NULL)
    {
        return iLine;
    }

    pFile  = fopen(TRACE_LOGFILE, "r");
    if (pFile == NULL)
    {
        return iLine;
    }

    while (fgets(szLine, (int)sizeof(szLine), pFile) != NULL)
    {
        iLine++;
    }

    if (iLine == g_log_last_line) 
    {
        printf("[LOG CHECK] No new message logged since last check...\n");
        return 0;
    }

    g_log_last_line = iLine;
	
    if (strstr(szLine, szTag) == NULL || strstr(szLine, szContent) == NULL)
    {
        printf("[LOG CHECK] No matching pattern in last log message...\n");
        iLine = 0;
    }
    
    fclose(pFile);
    return iLine;
}


/* ------------------------------------------------------------------
 * Group 1: renderer lifecycle guards
 * ------------------------------------------------------------------ */

/*
 * uc03_test_lifecycle_guards() - renderer_create/destroy/resize/
 *                                 get_font_metrics reject NULL params
 *
 * Code Coverage:
 *   renderer.c:
 *   -- [RND]1. renderer_create rejects NULL hWnd or phCtx --
 *   -- [RND]2. renderer_resize rejects NULL hCtx --
 *   -- [RND]3. renderer_get_font_metrics rejects NULL params --
 *   -- [RND]10. renderer_destroy rejects NULL phCtx or *phCtx --
 *   -- [RND]12. renderer_create returns any platform-related error --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc03_test_lifecycle_guards(void)
{
    renderer_t hR;
    st_error_t eRet;
    int        iW;
    int        iH;

    printf("\n--- Test group 1: renderer lifecycle guards ---\n");

    /* -- [RND]1. renderer_create rejects NULL hWnd or phCtx -- */
    /* -- [RND]12. renderer_create returns any platform-related error -- */
	/* INTENT[INT-RND-001 → TC-RND-001...004 → REQ-RND-001 → UFR-xxx-yyy]:
     * renderer_create() with NULL or incorrect inputs must return ST_ERROR 
	 * and leave the output handle untouched (still NULL) */
	hR   = NULL;
    UC_INFO("(INT-RND-001) Checking NULL parameter on renderer_create()");
    
	eRet = renderer_create((gui_window_t)(void *)&g_dummy, NULL);
	UC_TEST("[R] (TC-RND-001) renderer_create(NULL phCtx) -> ST_ERROR",
            eRet == ST_ERROR);
	UC_TEST("[N] (TC-RND-002) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL parameter"));
    
	eRet = renderer_create(NULL, &hR);
    UC_TEST("[R] (TC-RND-003) renderer_create(NULL hWnd) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-004) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL parameter"));
    
	eRet = renderer_create((gui_window_t)(void *)&g_dummy, &hR);
	UC_TEST("[R] (TC-RND-005) renderer_create() -> ST_ERROR due to platform",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-006) Trace log last [ERR] is from platform",
            uc03_check_last_log_entry("[ERR ]", "renderer_platform_create failed"));
    UC_TEST("[R] (TC-RND-007) renderer_create(NULL hWnd): handle stays NULL",
            hR == NULL);
	
    /* -- [RND]10. renderer_destroy rejects NULL phCtx or *phCtx -- */
    /* INTENT[INT-RND-002 → TC-RND-008...011 → REQ-RND-002 → UFR-xxx-yyy]:
     * renderer_destroy(NULL) and renderer_destroy(&hR) with *hR==NULL
     * must both return ST_ERROR */
    UC_INFO("(INT-RND-002) Checking NULL parameter on renderer_destroy()");
    eRet = renderer_destroy(NULL);
    UC_TEST("[R] (TC-RND-008) renderer_destroy(NULL) -> ST_ERROR",
            eRet == ST_ERROR);
	UC_TEST("[N] (TC-RND-009) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    
    hR   = NULL;
    eRet = renderer_destroy(&hR);
    UC_TEST("[R] (TC-RND-010) renderer_destroy(*phCtx=NULL) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-011) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));

    /* -- [RND]2. renderer_resize rejects NULL hCtx -- */
    /* INTENT[INT-RND-003 → TC-RND-012...013 → REQ-RND-003 → UFR-xxx-yyy]:
     * renderer_resize(NULL hCtx) must return ST_ERROR */
    UC_INFO("(INT-RND-003) Checking NULL parameter on renderer_resize()");
    eRet = renderer_resize(NULL, 800, 600);
    UC_TEST("[R] (TC-RND-012) renderer_resize(NULL) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-013) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));

    /* -- [RND]3. renderer_get_font_metrics rejects NULL params -- */
    /* INTENT[INT-RND-004 → TC-RND-014..019 → REQ-RND-004 → UFR-xxx-yyy]:
     * renderer_get_font_metrics() must return ST_ERROR when hCtx,
     * piCellW, or piCellH is NULL (checked independently) */
    UC_INFO("(INT-RND-004) Checking NULL parameters on renderer_get_font_metrics()");
    iW = 0; iH = 0;
    eRet = renderer_get_font_metrics(NULL, RENDERER_FONT_MONO, &iW, &iH);
    UC_TEST("[R] (TC-RND-014) renderer_get_font_metrics(NULL hCtx) "
            "-> ST_ERROR", eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-015) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));

    eRet = renderer_get_font_metrics((renderer_t)(void *)&g_dummy,
                                      RENDERER_FONT_MONO, NULL, &iH);
    UC_TEST("[R] (TC-RND-016) renderer_get_font_metrics(NULL piCellW) "
            "-> ST_ERROR", eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-017) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));

    eRet = renderer_get_font_metrics((renderer_t)(void *)&g_dummy,
                                      RENDERER_FONT_MONO, &iW, NULL);
    UC_TEST("[R] (TC-RND-018) renderer_get_font_metrics(NULL piCellH) "
            "-> ST_ERROR", eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-019) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
}

/* ------------------------------------------------------------------
 * Group 2: draw primitive guards
 * ------------------------------------------------------------------ */

/*
 * uc03_test_draw_guards() - begin/end_draw, fill_rect, draw_rect,
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
 *   win_D2D.c: (platform-specific wrapper for Windows)
 *   -- [D2D]1. NULL parameter are rejected with ST_ERROR and log message --
 *   -- [D2D]2. NULL platform-specific context is rejected with ST_ERROR --    
 *   -- [D2D]4. NULL parameter are rejected with ST_ERROR and log message --
 *   -- [D2D]5. NULL platform-specific context is rejected with ST_ERROR --
 *   -- [D2D]7. NULL parameter are rejected with ST_ERROR and log message --
 *   -- [D2D]8. NULL platform-specific context is rejected with ST_ERROR --
 *   -- [D2D]10. NULL parameter are rejected with ST_ERROR and log message --
 *   -- [D2D]11. NULL platform-specific context is rejected with ST_ERROR --
 *   -- [D2D]13. NULL parameter are rejected with ST_ERROR and log message --
 *   -- [D2D]14. NULL platform-specific context is rejected with ST_ERROR --
 *   -- [D2D]16. NULL parameter are rejected with ST_ERROR and log message --
 *   -- [D2D]17. NULL platform-specific context is rejected with ST_ERROR --
 * 
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc03_test_draw_guards(void)
{
    renderer_color_t tColor = RENDERER_COLOR_WHITE;
    renderer_rect_t  tRect  = RENDERER_RECT(0, 0, 100, 20);
    st_error_t       eRet;

    printf("\n--- Test group 2: draw primitives guards ---\n");

    /* -- [RND]4. renderer_begin_draw rejects NULL params -- */
    /* -- [D2D]1. NULL parameter are rejected with ST_ERROR and log message -- */
    /* -- [D2D]2. NULL platform-specific context is rejected with ST_ERROR -- */
    /* INTENT[INT-RND-005 → TC-RND-020...025 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_begin_draw(NULL) must return ST_ERROR, as well as valid but
     * not initialized renderer structure - Robustness of platform-specific
     * calls */
    UC_INFO("(INT-RND-005) Checking NULL parameters on renderer_begin_draw()");
    eRet = renderer_begin_draw(NULL, &tColor);
    UC_TEST("[R] (TC-RND-020) renderer_begin_draw(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-021) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_begin_draw((renderer_t)(void *)&g_dummy, NULL);
    UC_TEST("[R] (TC-RND-022) renderer_begin_draw(NULL ptBgColor) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-023) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_begin_draw((renderer_t)(void *)&g_dummy, &tColor);
    UC_TEST("[R] (TC-RND-024) renderer_begin_draw() with uninitialized renderer -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-025) Trace log last [ERR] is due to uninitialized renderer",
            uc03_check_last_log_entry("[ERR ]", "renderer not initialised"));
    
    /* -- [RND]5. renderer_end_draw rejects NULL hCtx -- */
    /* -- [D2D]4. NULL parameter are rejected with ST_ERROR and log message -- */
    /* -- [D2D]5. NULL platform-specific context is rejected with ST_ERROR -- */
    /* INTENT[INT-RND-006 → TC-RND-026...029 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_end_draw(NULL) must return ST_ERROR, as well as valid but not
     * initialized renderer structure - robustness of the platform-specific call */
    UC_INFO("(INT-RND-006) Checking NULL parameters on renderer_end_draw()");
    eRet = renderer_end_draw(NULL);
    UC_TEST("[R] (TC-RND-026) renderer_end_draw(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-027) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_end_draw((renderer_t)(void *)&g_dummy);
    UC_TEST("[R] (TC-RND-028) renderer_end_draw() with uninitialized renderer",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-029) Trace log last [ERR] is due to uninitialized renderer",
            uc03_check_last_log_entry("[ERR ]", "renderer not initialised"));
    
    /* -- [RND]6. renderer_fill_rect rejects NULL params -- */
    /* -- [D2D]7. NULL parameter are rejected with ST_ERROR and log message -- */
    /* -- [D2D]8. NULL platform-specific context is rejected with ST_ERROR -- */
    /* INTENT[INT-RND-007 → TC-RND-030...037 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_fill_rect(NULL) must return ST_ERROR */
    UC_INFO("(INT-RND-007) Checking NULL parameters on renderer_fill_rect()");
    eRet = renderer_fill_rect(NULL, &tRect, &tColor);
    UC_TEST("[R] (TC-RND-030) renderer_fill_rect(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-031) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_fill_rect((renderer_t)(void *)&g_dummy, NULL, &tColor);
    UC_TEST("[R] (TC-RND-032) renderer_fill_rect(NULL tRect) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-033) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_fill_rect((renderer_t)(void *)&g_dummy, &tRect, NULL);
    UC_TEST("[R] (TC-RND-034) renderer_fill_rect(NULL tColor) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-035) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_fill_rect((renderer_t)(void *)&g_dummy, &tRect, &tColor);
    UC_TEST("[R] (TC-RND-036) renderer_fill_rect() with unititialized renderer",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-037) Trace log last [ERR] is due to uninitialized renderer",
            uc03_check_last_log_entry("[ERR ]", "renderer not initialised"));
    
    /* -- [RND]7. renderer_draw_rect rejects NULL params -- */
    /* -- [D2D]10. NULL parameter are rejected with ST_ERROR and log message -- */
    /* -- [D2D]11. NULL platform-specific context is rejected with ST_ERROR -- */
    /* INTENT[INT-RND-008 → TC-RND-038...045 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_draw_rect(NULL) must return ST_ERROR, as well as valid but not
     * initialized renderer structure - robustness of platform-specific calls */
    UC_INFO("(INT-RND-008) Checking NULL parameters on renderer_draw_rect()");
    eRet = renderer_draw_rect(NULL, &tRect, &tColor, 1.0f);
    UC_TEST("[R] (TC-RND-038) renderer_draw_rect(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-039) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_rect((renderer_t)(void *)&g_dummy, NULL, &tColor, 1.0f);
    UC_TEST("[R] (TC-RND-040) renderer_draw_rect(NULL tRect) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-041) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_rect((renderer_t)(void *)&g_dummy, &tRect, NULL, 1.0f);
    UC_TEST("[R] (TC-RND-042) renderer_draw_rect(NULL tColor) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-043) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_rect((renderer_t)(void *)&g_dummy, &tRect, &tColor, 1.0f);
    UC_TEST("[R] (TC-RND-044) renderer_draw_rect() with unitiliazed renderer",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-045) Trace log last [ERR] is due to uninitialized renderer",
            uc03_check_last_log_entry("[ERR ]", "renderer not initialised"));

    
    /* -- [RND]8. renderer_draw_line rejects NULL params -- */
    /* -- [D2D]13. NULL parameter are rejected with ST_ERROR and log message -- */
    /* -- [D2D]14. NULL platform-specific context is rejected with ST_ERROR -- */
    /* INTENT[INT-RND-009 → TC-RND-046...051 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_draw_line(NULL) must return ST_ERROR, as well as valid but not
     * initialized renderer structure - robustness of platform-specific calls */
    UC_INFO("(INT-RND-009) Checking NULL parameters on renderer_draw_rect()");
    eRet = renderer_draw_line(NULL, 0, 0, 10, 10, &tColor, 1.0f);
    UC_TEST("[R] (TC-RND-046) renderer_draw_line(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-047) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_line((renderer_t)(void *)&g_dummy, 0, 0, 10, 10, NULL, 1.0f);
    UC_TEST("[R] (TC-RND-048) renderer_draw_line(NULL tColor) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-049) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_line((renderer_t)(void *)&g_dummy, 0, 0, 10, 10, &tColor, 1.0f);
    UC_TEST("[R] (TC-RND-050) renderer_draw_line() with unitiliazed renderer",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-051) Trace log last [ERR] is due to uninitialized renderer",
            uc03_check_last_log_entry("[ERR ]", "renderer not initialised"));
    
    /* -- [RND]9. renderer_draw_text rejects NULL params -- */
    /* -- [D2D]16. NULL parameter are rejected with ST_ERROR and log message -- */
    /* -- [D2D]17. NULL platform-specific context is rejected with ST_ERROR -- */
    /* INTENT[INT-RND-010 → TC-RND-052...061 → REQ-RND-005 → UFR-xxx-yyy]:
     * renderer_draw_text() must return ST_ERROR when hCtx or szText
     * is NULL (checked independently) */
    UC_INFO("(INT-RND-010) Checking NULL parameters on renderer_draw_rect()");
    eRet = renderer_draw_text(NULL, "test", &tRect, &tColor,
                               RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    UC_TEST("[R] (TC-RND-052) renderer_draw_text(NULL hCtx) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-053) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_text((renderer_t)(void *)&g_dummy, NULL, &tRect,
                               &tColor, RENDERER_FONT_MONO,
                               RENDERER_ALIGN_LEFT);
    UC_TEST("[R] (TC-RND-054) renderer_draw_text(NULL szText) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-055) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_text((renderer_t)(void *)&g_dummy, "text", NULL,
                               &tColor, RENDERER_FONT_MONO,
                               RENDERER_ALIGN_LEFT);
    UC_TEST("[R] (TC-RND-056) renderer_draw_text(NULL tRect) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-057) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_text((renderer_t)(void *)&g_dummy, "text", &tRect,
                               NULL, RENDERER_FONT_MONO,
                               RENDERER_ALIGN_LEFT);
    UC_TEST("[R] (TC-RND-058) renderer_draw_text(NULL tColor) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-059) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL"));
    eRet = renderer_draw_text((renderer_t)(void *)&g_dummy, "text", &tRect,
                               &tColor, RENDERER_FONT_MONO,
                               RENDERER_ALIGN_LEFT);
    UC_TEST("[R] (TC-RND-060) renderer_draw_text() with unitiliazed renderer",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-RND-061) Trace log last [ERR] is due to uninitialized renderer",
            uc03_check_last_log_entry("[ERR ]", "renderer not initialised"));
        
}

/* ------------------------------------------------------------------
 * Group 3: real window & D2D spy (auto) + remaining visual (manual)
 * ------------------------------------------------------------------ */

/* Persistent renderer handle for the paint callback */
static renderer_t g_uc03_hR = NULL;

static void uc03_paint_cb(gui_window_t   hWnd,
                            gui_event_t   *ptEvent,
                            void          *pCtx)
{
    renderer_color_t tBg     = {0.19f, 0.19f, 0.44f, 1.0f};
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
        if (g_uc03_hR == NULL)
            renderer_create(hWnd, &g_uc03_hR);
        if (g_uc03_hR == NULL)
            return;

        gui_get_size(hWnd, &iW, &iH);
        tRect.fX = 0.0f;  tRect.fY = 0.0f;
        tRect.fW = (float)iW; tRect.fH = (float)iH;
        renderer_begin_draw(g_uc03_hR, &tBg);

        tRect.fX = 20.0f; tRect.fY =  20.0f;
        tRect.fW = 200.0f; tRect.fH =  80.0f;
        renderer_fill_rect(g_uc03_hR, &tRect, &tGreen);

        tRect.fX = 20.0f; tRect.fY = 120.0f;
        tRect.fW = 200.0f; tRect.fH =  80.0f;
        renderer_draw_rect(g_uc03_hR, &tRect, &tCyan, 2.0f);

        renderer_draw_line(g_uc03_hR, 20, 220, 200, 300, &tYellow, 3.0f);

        tRect.fX = 20.0f;  tRect.fY = 320.0f;
        tRect.fW = 380.0f; tRect.fH =  30.0f;
        renderer_draw_text(g_uc03_hR, "ST4Ever UC3", &tRect, &tWhite,
                            RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

        renderer_end_draw(g_uc03_hR);
    }
    else if (ptEvent->eType == GUI_EVT_CLOSE)
    {
        if (g_uc03_hR != NULL)
            renderer_destroy(&g_uc03_hR);
    }
}

/*
 * uc03_win_test_visual() - Open a real window, let uc03_paint_cb() run
 * the renderer draw calls, then assert the spy buffer from win_D2D.c
 *
 * Code Coverage:
 *   win_D2D.c: (platform-specific wrapper for Windows)
 *    -- [D2D]3. Call D2D primitives BeginDraw & Clear --
 *    -- [D2D]9. Call D2D Primitive FillRectangle -- 
 *    -- [D2D]12. Call D2D Primitive DrawRectangle --
 *    -- [D2D]15. Call D2D Primitive DrawLine -- 
 *    -- [D2D]18. Draw text manages MONO FONT or UI internal font -- 
 *    -- [D2D]19. Draw text manages Text Alignment -- 
 *    -- [D2D]20. Call D2D Primitive DrawText --
 *    -- [D2D]21. Spy parameters sent to Win D2D DrawText --
 *    -- [D2D]22. Spy parameters sent to Win D2D Clear --
 *    -- [D2D]23. Spy parameters sent to Win D2D FillRectangle --
 *    -- [D2D]24. Spy parameters sent to Win D2D DrawRectangle --
 *    -- [D2D]25. Spy parameters sent to Win D2D DrawLine --
 *    -- [SPY]1. Resets the spies pointers and counter --
 *    -- [SPY]2. Free memory for each allocated spy --
 *    -- [SPY]4. Add a new spy into the ring buffer -- 
 *    -- [SPY]5. Create a new spy of type ST_WIN_D2D_SPY_DT --
 *    -- [SPY]7. Look for last spy of corresponding type, if index is -1 --
 *    -- [SPY]9. Create a new spy of type ST_WIN_D2D_SPY_CLR --
 *    -- [SPY]10. Create a new spy of type ST_WIN_D2D_SPY_FR --
 *    -- [SPY]11. Create a new spy of type ST_WIN_D2D_SPY_DR --
 *    -- [SPY]12. Create a new spy of type ST_WIN_D2D_SPY_DL -- 
 * 
 *   gui.c:
 *    -- [GUI]6. NULL pointers leads to ST_ERROR and message in log file --   
 *    -- [GUI]7. Opening a window without GUI initialized returns ST_ERROR --
 *    -- [GUI]8. Create an OS window & add it in the GUI context structure --
 *    -- [GUI]11. Create a new entry & increment number of windows --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc03_test_visual(void)
{
    gui_wnd_desc_t     tDesc;
    gui_window_t        hWnd;
    st_error_t          eRet;
    gui_context_t* ptGUICtx = (gui_context_t*)gui_init();
    
    // Windows specific pointer for D2D spies
#ifdef ST_PLATFORM_WINDOWS
    win_d2d_ctx_t *ptD2D = NULL;
    ptGUICtx->bActiveSpies = ST_TRUE;
#endif

    printf("\n--- Test group 3: real window - D2D spy + visual ---\n");

    /* setup the test window */
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle  = "ST4Ever - UC3 D2D Renderer";
    tDesc.eType    = GUI_WND_DIR;
    tDesc.pfnEvent = uc03_paint_cb;
    tDesc.pUserCtx = NULL;
    hWnd = NULL;
    
    /* -- [GUI]6. NULL pointers leads to ST_ERROR and message in log file -- */
    /* INTENT[INT-GUI-004 → TC-GUI-008...011 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * gui_open_window() must return ST_ERROR when ptDesc or phWnd
     * is NULL (checked independently) */
    UC_INFO("(INT-GUI-004) Checking NULL parameters on gui_open_window()");
    eRet = gui_open_window(NULL, &hWnd);
    UC_TEST("[R] (TC-GUI-008) gui_open_window(NULL ptDesc) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-GUI-009) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL parameter"));
    eRet = gui_open_window(&tDesc, NULL);
    UC_TEST("[R] (TC-GUI-010) gui_open_window(NULL phWnd) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-GUI-011) Trace log last [ERR] is NULL parameter",
            uc03_check_last_log_entry("[ERR ]", "NULL parameter"));
    
    /* -- [GUI]7. Opening a window without GUI initialized returns ST_ERROR -- */
    /* INTENT[INT-GUI-005 → TC-GUI-012...013 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * gui_open_window() returns ST_ERROR when GUI is not initialized */
    ptGUICtx->bInit = ST_FALSE;
    UC_INFO("(INT-GUI-005) Checking GUI initialization before opening a window");
    eRet = gui_open_window(&tDesc, &hWnd);
    UC_TEST("[R] (TC-GUI-012) gui_open_window() with unitiliazed GUI",
            eRet == ST_ERROR);
    UC_TEST("[N] (TC-GUI-013) Trace log last [ERR] is due to uninitialized GUI",
            uc03_check_last_log_entry("[ERR ]", "called before gui_init"));
    ptGUICtx->bInit = ST_TRUE;

    /* -- [GUI]8. Create an OS window & add it in the GUI context structure -- */
    /* -- [GUI]11. Create a new entry & increment number of windows -- */
    /* INTENT[INT-GUI-006 → TC-GUI-014...017 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * gui_open_window() returns ST_NO_ERROR after creation of an OS window,
     * the window pointer is initialized, as well as the GUI context */
    UC_CHECK("(INT-GUI-006) [Chk] gui_open_window() returns the window pointer"
              , gui_open_window(&tDesc, &hWnd));
    platform_sleep_ms(200);  // Let the window paint itself completely
    UC_TEST("[N] (TC-GUI-014) Window pointer is not NULL", hWnd != NULL);
    UC_TEST("[N] (TC-GUI-015) Last window added in gui_context_t is correct", 
        ptGUICtx->uiLastOpenWindow < GUI_MAX_WINDOWS);
    
    if (ptGUICtx->uiLastOpenWindow < GUI_MAX_WINDOWS)
    {
        struct gui_window_s *ptWnd = ptGUICtx->aptWnd[ptGUICtx->uiLastOpenWindow];
        UC_TEST("[N] (TC-GUI-016) gui_context_t is updated with the new window",
                ptWnd != NULL);
        if (ptWnd)
        {
            UC_TEST("[N] (TC-GUI-017) Created window is open",
                ptWnd->bOpen == ST_TRUE);
            UC_TEST("[N] (TC-GUI-018) Pointer to related renderer is valid",
                ptWnd->ptRenderer != NULL);

#ifdef ST_PLATFORM_WINDOWS
            if (ptWnd->ptRenderer)
            {
                renderer_t ptRenderer = (renderer_t)ptWnd->ptRenderer;
                if (ptRenderer->pPlatform)
                {
                    gIsObject = ST_FALSE;
                    ptD2D = ptRenderer->pPlatform;
                    UC_CHECK_OBJ(ptD2D, ST_WIN_D2D_CTX);
                    UC_TEST("[N] (TC-GUI-019) Check renderer object is retrieved",
                        gIsObject == ST_TRUE);
                } else printf("  [SKIP] (TC-GUI-019) Pointer to Win D2D check failed\n\n"); 
            } else printf("  [SKIP] (TC-GUI-019) Pointer to renderer check failed\n\n"); 
#else
            printf("  [SKIP] (TC-GUI-019) --> Add here a platform specific test <--");
#endif
        } else printf("  [SKIP] (TC-GUI-017...019) Pointer to last window check failed\n\n"); 
    } else printf("  [SKIP] (TC-GUI-016...019) Last added window check failed\n\n"); 

    
#ifdef ST_PLATFORM_WINDOWS
    if (ptD2D)
    {
        /* -- [SPY]1. Resets the spies pointers and counter -- */
        /* -- [SPY]4. Add a new spy into the ring buffer -- */
        /* -- [SPY]2. Free memory for each allocated spy -- */
        /* INTENT[INT-SPY-001 → TC-SPY-001..007 → REQ-xxx-yyy → UFR-xxx-yyy]:
        * Make use of the D2D spies to look into internal parameters sent to
        * the Windows D2D rendering backend */
        UC_INFO("(INT-SPY-001) Check D2D spy framework");
        UC_CHECK("(INT-SPY-001) [Chk] Initialize the spies ring buffer", 
                win_D2D_spy_reset(ptD2D));
        UC_TEST("[N] (TC-SPY-001) First spy pointer is NULL",
                ptD2D->pD2DSpies[0] == NULL);
        UC_TEST("[N] (TC-SPY-002) Spy counter is reset",
                ptD2D->uiSpiesCount == 0);
    
        gui_invalidate(hWnd);
        platform_sleep_ms(100);  // Let the window paint itself completely
    
        UC_TEST("[N] (TC-SPY-003) After gui_invalidate(), first spy pointer is NOT NULL",
                ptD2D->pD2DSpies[0] != NULL);
        UC_TEST("[N] (TC-SPY-004) Spy counter is above zero",
                ptD2D->uiSpiesCount > 0);
        UC_CHECK_OBJ(ptD2D->pD2DSpies[0], ST_WIN_D2D_SPY_CLR);
        UC_TEST("[N] (TC-SPY-005) Type of spy is ST_WIN_D2D_SPY_CLR (always start with BeginDraw)",
                gIsObject == ST_TRUE);
        
        win_D2D_spy_reset(ptD2D);
        
        UC_TEST("[N] (TC-SPY-006) Reset again => first spy pointer is NULL",
                ptD2D->pD2DSpies[0] == NULL);
        UC_TEST("[N] (TC-SPY-007) Spy counter is reset",
                ptD2D->uiSpiesCount == 0);
    
        gui_invalidate(hWnd);
        platform_sleep_ms(100);  // Let the window paint itself completely
    
        /* -- [D2D]18. Draw text manages MONO FONT or UI internal font -- */
        /* -- [D2D]19. Draw text manages Text Alignment -- */
        /* -- [D2D]20. Call D2D Primitive DrawText -- */
        /* -- [D2D]21. Spy parameters sent to Win D2D DrawText -- */
        /* -- [SPY]5. Create a new spy of type ST_WIN_D2D_SPY_DT -- */
        /* -- [SPY]7. Look for last spy of corresponding type, if index is -1 -- */
        /* INTENT[INT-RND-011 → TC-RND-062..069 → REQ-xxx-yyy → UFR-xxx-yyy]:
        * the draw_text spy capture records the parameters sent to win D2D
        * DrawText function.
        * uc03_paint_cb() is called when Windows fires repaint events.*/
        UC_INFO("(INT-RND-011) Spying text draw after window's repaint event");
        
        const win_D2D_spy_draw_text_t* ptTxtSpy = 
        (win_D2D_spy_draw_text_t*)win_D2D_get_spy(-1, ptD2D, ST_WIN_D2D_SPY_DT);
        
        UC_TEST("[N] (TC-RND-062) There is at least a DrawText Spy captured",
                ptTxtSpy != NULL);
        if (ptTxtSpy)
        {
            UC_TEST("[N] (TC-RND-063) Spied text is \"ST4Ever UC3\"",
                wcsstr(ptTxtSpy->wcText, L"ST4Ever UC3") != NULL);
            UC_TEST("[N] (TC-RND-064) Spied text length is 11",
                ptTxtSpy->uiLen == 11);
            UC_TEST("[N] (TC-RND-065) captured font/alignment: MONO/LEFT",
                ptTxtSpy->eFontId == RENDERER_FONT_MONO
                && ptTxtSpy->eAlign  == RENDERER_ALIGN_LEFT);
            UC_TEST("[N] (TC-RND-066) captured rect matches (20,320,380,30)",
                ptTxtSpy->tRect.fX == 20.0f  && ptTxtSpy->tRect.fY   == 320.0f
                && ptTxtSpy->tRect.fW == 380.0f && ptTxtSpy->tRect.fH   ==  30.0f);
            UC_TEST("[N] (TC-RND-067) captured colour is white",
                ptTxtSpy->tColor.r == 1.0f && ptTxtSpy->tColor.g == 1.0f
                && ptTxtSpy->tColor.b == 1.0f && ptTxtSpy->tColor.a == 1.0f);
            UC_TEST("[N] (TC-RND-068) Text Clip option is CLIP",
                ptTxtSpy->eClip == RENDERER_TEXT_OPT_CLIP);
            UC_TEST("[N] (TC-RND-069) Text Measuring is Natural",
                ptTxtSpy->eMeasure == RENDERER_MEASURING_NATURAL);    
        } else printf("  [SKIP] (TC-RND-063...069) No Corresponding spy found\n\n");

        /* -- [D2D]3. Call D2D primitives BeginDraw & Clear -- */
        /* -- [D2D]22. Spy parameters sent to Win D2D Clear -- */
        /* -- [SPY]9. Create a new spy of type ST_WIN_D2D_SPY_CLR -- */
        /* -- [SPY]7. Look for last spy of corresponding type, if index is -1 -- */
        /* INTENT[INT-RND-012 → TC-RND-070..071 → REQ-XXX-yyy → UFR-xxx-yyy]:
        * Renderer clear function clears the rendering area with a background 
        * color - Spy parameters set in uc03_paint_cb callback */
        UC_INFO("(INT-RND-012) Spying begin draw after window's repaint event");
        
        const win_D2D_spy_clear_t* ptClrSpy = 
                (win_D2D_spy_clear_t*)win_D2D_get_spy(-1, ptD2D, ST_WIN_D2D_SPY_CLR);
        
        UC_TEST("[N] (TC-RND-070) There is at least a Clear Spy captured",
                ptClrSpy != NULL);
        if (ptClrSpy)
        {
            UC_TEST("[N] (TC-RND-071) Background color is the one from uc03_paint_cb()",
                ptClrSpy->tColor.r == 0.19f && ptClrSpy->tColor.g == 0.19f
                && ptClrSpy->tColor.b == 0.44f && ptClrSpy->tColor.a == 1.0f);
        } else printf("  [SKIP] (TC-RND-071) No Corresponding spy found\n\n"); 

        /* -- [D2D]9. Call D2D Primitive FillRectangle -- */
        /* -- [D2D]23. Spy parameters sent to Win D2D FillRectangle -- */
        /* -- [SPY]10. Create a new spy of type ST_WIN_D2D_SPY_FR -- */
        /* -- [SPY]7. Look for last spy of corresponding type, if index is -1 -- */
        /* INTENT[INT-RND-013 → TC-RND-072..074 → REQ-xxx-yyy → UFR-xxx-yyy]:
        * Renderer fill rectangle function paints a rectangle area with a solid brush 
        * color - Check last spy parameters set in uc03_paint_cb callback */
        UC_INFO("(INT-RND-013) Spying fill rectangle after window's repaint event");
        
        const win_D2D_spy_fill_rectangle_t* ptFillSpy = 
        (win_D2D_spy_fill_rectangle_t*)win_D2D_get_spy(-1, ptD2D, ST_WIN_D2D_SPY_FR);
        
        UC_TEST("[N] (TC-RND-072) There is at least a Fill Rectangle Spy captured",
                ptFillSpy != NULL);
        if (ptFillSpy)
        {
            UC_TEST("[N] (TC-RND-073) captured rect matches (20,20,200,80)",
                ptFillSpy->tRect.fX == 20.0f  && ptFillSpy->tRect.fY   == 20.0f
                && ptFillSpy->tRect.fW == 200.0f && ptFillSpy->tRect.fH   ==  80.0f);
            UC_TEST("[N] (TC-RND-074) Rectangle color is green, as per uc03_paint_cb()",
                ptFillSpy->tColor.r == 0.00f && ptFillSpy->tColor.g == 0.85f
                && ptFillSpy->tColor.b == 0.20f && ptFillSpy->tColor.a == 1.0f);
        } else printf("  [SKIP] (TC-RND-073...074) No Corresponding spy found\n\n"); 

        /* -- [D2D]12. Call D2D Primitive DrawRectangle -- */
        /* -- [D2D]24. Spy parameters sent to Win D2D DrawRectangle -- */
        /* -- [SPY]11. Create a new spy of type ST_WIN_D2D_SPY_DR -- */
        /* -- [SPY]7. Look for last spy of corresponding type, if index is -1 -- */
        /* INTENT[INT-RND-014 → TC-RND-075..078 → REQ-xxx-yyy → UFR-xxx-yyy]:
        * Renderer draw rectangle function paints a rectangle with a solid brush 
        * color - Check last spy parameters set in uc03_paint_cb callback */
        UC_INFO("(INT-RND-014) Spying draw rectangle after window's repaint event");
        
        const win_D2D_spy_draw_rectangle_t* ptRectSpy = 
        (win_D2D_spy_draw_rectangle_t*)win_D2D_get_spy(-1, ptD2D, ST_WIN_D2D_SPY_DR);
        
        UC_TEST("[N] (TC-RND-075) There is at least a draw Rectangle Spy captured",
                ptRectSpy != NULL);
        if (ptRectSpy)
        {
            UC_TEST("[N] (TC-RND-076) captured rect matches (20,120,200,80)",
                ptRectSpy->tRect.fX == 20.0f  && ptRectSpy->tRect.fY   == 120.0f
                && ptRectSpy->tRect.fW == 200.0f && ptRectSpy->tRect.fH   ==  80.0f);
            UC_TEST("[N] (TC-RND-077) Rectangle color is cyan, as per uc03_paint_cb()",
                ptRectSpy->tColor.r == 0.00f && ptRectSpy->tColor.g == 0.85f
                && ptRectSpy->tColor.b == 0.85f && ptRectSpy->tColor.a == 1.0f);
            UC_TEST("[N] (TC-RND-078) Stroke width is 2.0f, as per uc03_paint_cb()",
                ptRectSpy->fStroke == 2.0f);
        } else printf("  [SKIP] (TC-RND-076...078) No Corresponding spy found\n\n"); 

        /* -- [D2D]15. Call D2D Primitive DrawLine -- */
        /* -- [D2D]25. Spy parameters sent to Win D2D DrawLine -- */
        /* -- [SPY]12. Create a new spy of type ST_WIN_D2D_SPY_DL -- */
        /* -- [SPY]7. Look for last spy of corresponding type, if index is -1 -- */
        /* INTENT[INT-RND-015 → TC-RND-079..082 → REQ-xxx-yyy → UFR-xxx-yyy]:
        * Renderer line function paints a line between 2 points with a solid brush 
        * color - Check last spy parameters set in uc03_paint_cb callback */
        UC_INFO("(INT-RND-015) Spying draw line after window's repaint event");
        
        const win_D2D_spy_draw_line_t* ptLineSpy = 
        (win_D2D_spy_draw_line_t*)win_D2D_get_spy(-1, ptD2D, ST_WIN_D2D_SPY_DL);
        
        UC_TEST("[N] (TC-RND-079) There is at least a Draw line Spy captured",
                ptLineSpy != NULL);
        if (ptLineSpy)
        {
            UC_TEST("[N] (TC-RND-080) Line points from (20,220) to (200,300)",
                ptLineSpy->fX1 == 20.0f  && ptLineSpy->fY1   == 220.0f
                && ptLineSpy->fX2 == 200.0f && ptLineSpy->fY2   ==  300.0f);
            UC_TEST("[N] (TC-RND-081) Line color is yellow, as per uc03_paint_cb()",
                ptLineSpy->tColor.r == 0.95f && ptLineSpy->tColor.g == 0.90f
                && ptLineSpy->tColor.b == 0.10f && ptLineSpy->tColor.a == 1.0f);
            UC_TEST("[N] (TC-RND-082) Stroke width is 3.0f, as per uc03_paint_cb()",
                ptLineSpy->fStroke == 3.0f);
        } else printf("  [SKIP] (TC-RND-079...082) No Corresponding spy found\n\n"); 
    }
#endif
    
    gui_close_window(hWnd);
    hWnd = NULL;
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    char *args[] = {"UC3"};

    printf("\n");
    printf("================================================================\n");
    printf(" UC3 - Direct2D renderer (win_D2D.c + renderer.c)\n");
    printf("================================================================\n");

    ptCtx = (ST4Ever_context_t *)ST4Ever_init(1, args);
    UC_CHECK("(UC3) [Chk] Launch ST4Ever with no argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject)
    {
        uc03_test_lifecycle_guards();
        uc03_test_draw_guards();
        uc03_test_visual();
    }
    else
    {
        printf("  [SKIP] (UC3) ST_MAIN_CTX Object Check failed\n\n");
    }

    printf("\n--- Shutdown ---\n");
    ST4Ever_shutdown();

    printf("\n================================================================\n");
    if (g_uc_fails == 0)
    {
        printf(" UC3 PASSED - 0 failures\n");
    }
    else
    {
        printf(" UC3 FAILED - %d failure(s)\n", g_uc_fails);
    }
    printf("================================================================\n\n");

    return (g_uc_fails > 0) ? 1 : 0;
}
