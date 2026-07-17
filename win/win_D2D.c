/*
 * win_D2D.c - Direct2D + DirectWrite renderer backend for renderer.h
 *
 * Implements renderer_platform_* functions declared in renderer_backend.h.
 *
 * Each renderer_t is backed by a win_d2d_ctx_t holding:
 *   ID2D1Factory*          - per-renderer D2D factory (MULTI_THREADED)
 *   ID2D1HwndRenderTarget* - one per window, bound to HWND
 *   IDWriteFactory*        - per-renderer DirectWrite factory (SHARED)
 *   IDWriteTextFormat*     - one per font id (MONO=Consolas, UI=Segoe UI)
 *   ID2D1SolidColorBrush*  - one brush, colour updated per draw call
 *   fMonoCellW/H           - cached RENDERER_FONT_MONO cell dimensions
 *
 * All D2D/DWrite objects are COM interfaces; released via ->Release()
 * in renderer_platform_destroy().
 *
 * COM lifecycle: CoInitializeEx / CoUninitialize are called in
 * win_wnd_thread() (win_gui.c) — the thread that owns the renderer.
 *
 * Direct2D coordinates: float pixels, top-left origin.
 * renderer_rect_t {fX, fY, fW, fH} -> D2D1_RECT_F {left, top, right, bottom}
 *
 * Text: MultiByteToWideChar (UTF-8 -> WCHAR) then DrawText().
 *       Alignment set on format before draw, reset to LEADING after.
 *
 * UC3.2: full implementation — rect, line, text visible in window.
 * TODO(UC26): renderer_platform_draw_bitmap (screen emulator).
 */

#include "../src/renderer_backend.h"
#include "../src/gui_backend.h"
#include "../src/trace.h"

#ifdef ST_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS   /* enable dwrite.h lpVtbl C macros (IDWriteFactory_*) */
#include <windows.h>
#include <initguid.h>  /* DEFINE_GUID expansion: defines IID_* in this .o */
#include <d2d1.h>
#include <dwrite.h>
#include <stdlib.h>
#include <string.h>
#include "win.h"

/* ------------------------------------------------------------------
 * Test-only spy capture (R26) - see win.h for the public API.
 * ------------------------------------------------------------------ */
/*
 * win_D2D_spy_reset() - Clear the spy ring buffer (see win.h).
 */
st_error_t win_D2D_spy_reset(win_d2d_ctx_t *ptCtx)
{
    // No LOG_TRACE - R22: test-only spy accessor invoked before every
    // capture pass in use_case_*.c; tracing it would flood the log
    // during test runs without adding diagnostic value.
    /* -- [SPY]1. Resets the spies pointers and counter -- */
    for (int i = 0; i < WIN_D2D_SPY_MAX_ENTRIES; i++)
    {
        /* -- [SPY]2. Free memory for each allocated spy -- */
        if (ptCtx->pD2DSpies[i] != NULL)
        {
            free(ptCtx->pD2DSpies[i]);
        }
        ptCtx->pD2DSpies[i] = NULL;
    }
    ptCtx->uiSpiesCount = 0;

    return ST_NO_ERROR;
}

/*
 * win_D2D_spy_add() - Append an already-allocated spy entry to the
 *                     ring buffer (see win.h), or count it as dropped
 *                     once the buffer is full.
 */
static void win_D2D_spy_add(void          *data,
                            win_d2d_ctx_t *ptCtx)
{
    // No LOG_TRACE - R22: internal helper invoked once per captured
    // draw primitive - same tight-loop category as the draw calls it
    // records.
    /* -- [SPY]3. Stops logging spy entries when limit is reached -- */
    if (ptCtx->uiSpiesCount >= WIN_D2D_SPY_MAX_ENTRIES)
    {
        ptCtx->uiSpiesCount++;
        return;
    }

    /* -- [SPY]4. Add a new spy into the ring buffer -- */
    ptCtx->pD2DSpies[ptCtx->uiSpiesCount] = data;
    ptCtx->uiSpiesCount++;
}

/*
 * win_D2D_spy_draw_text() - Capture the parameters of one
 *                           renderer_platform_draw_text() call into a
 *                           new win_D2D_spy_draw_text_t entry (R27).
 */
static void win_D2D_spy_draw_text(win_d2d_ctx_t          *ptCtx,
                                  const WCHAR            *wcText,
                                  UINT32                  uiLen,
                                  IDWriteTextFormat      *pFmt,
                                  const D2D1_RECT_F       tRect,
                                  D2D1_DRAW_TEXT_OPTIONS  eTextOpts,
                                  DWRITE_MEASURING_MODE   eMeasureMode)
{
    // No LOG_TRACE - R22: spy capture invoked on every
    // renderer_platform_draw_text() call - same tight-loop category
    // as the primitive it records.
    win_D2D_spy_draw_text_t *ptEntry;
    DWRITE_TEXT_ALIGNMENT eAlign = IDWriteTextFormat_GetTextAlignment(pFmt);
    D2D1_COLOR_F          tColor = ptCtx->tColor;
    
    /* -- [SPY]5. Create a new spy of type ST_WIN_D2D_SPY_DT -- */
    ptEntry = (win_D2D_spy_draw_text_t *)calloc(1, sizeof(win_D2D_spy_draw_text_t));
    ptEntry->ulMagic = 0xCAFEDECA;
    ptEntry->eObject = ST_WIN_D2D_SPY_DT;
    
    // Catch sent text (wchar_t)
    wcsncpy(ptEntry->wcText, wcText, WIN_D2D_SPY_TEXT_LEN);
    ptEntry->uiLen   = uiLen;

    // Catch used font and alignment
    ptEntry->eFontId = (pFmt == ptCtx->pFmtUI) ? RENDERER_FONT_UI : RENDERER_FONT_MONO;
    switch (eAlign)
    {
        case DWRITE_TEXT_ALIGNMENT_CENTER:
            ptEntry->eAlign = RENDERER_ALIGN_CENTER;
            break;
        case DWRITE_TEXT_ALIGNMENT_TRAILING:
            ptEntry->eAlign = RENDERER_ALIGN_RIGHT;
            break;
        case DWRITE_TEXT_ALIGNMENT_LEADING:
            ptEntry->eAlign = RENDERER_ALIGN_LEFT;
            break;
        default:
            ptEntry->eAlign = RENDERER_ALIGN_OTHER;
            break;
    }

    // Catch text rectangle area
    ptEntry->tRect.fX = tRect.left;
    ptEntry->tRect.fY = tRect.top;
    ptEntry->tRect.fW = tRect.right - tRect.left;
    ptEntry->tRect.fH = tRect.bottom - tRect.top;

    // Catch text color
    ptEntry->tColor.r = tColor.r;
    ptEntry->tColor.g = tColor.g;
    ptEntry->tColor.b = tColor.b;
    ptEntry->tColor.a = tColor.a;
    
    // Retrieve text options
    switch(eTextOpts)
    {
        case D2D1_DRAW_TEXT_OPTIONS_NONE:
            ptEntry->eClip = RENDERER_TEXT_OPT_NONE;
            break;
        case D2D1_DRAW_TEXT_OPTIONS_NO_SNAP:
            ptEntry->eClip = RENDERER_TEXT_OPT_NO_SNAP;
            break;
        case D2D1_DRAW_TEXT_OPTIONS_CLIP:
            ptEntry->eClip = RENDERER_TEXT_OPT_CLIP;
            break;
        default:
            ptEntry->eClip = RENDERER_TEXT_OPT_UNKNOWN;
            break;
    }

    // Retrieve the measuring mode
    switch(eMeasureMode)
    {
        case DWRITE_MEASURING_MODE_NATURAL:
            ptEntry->eMeasure = RENDERER_MEASURING_NATURAL;
            break;
        default:
            ptEntry->eMeasure = RENDERER_MEASURING_OTHER;
            break;
    }

    win_D2D_spy_add((void*)ptEntry, ptCtx);
}

/*
 * win_D2D_spy_clear() - Capture the parameters of one
 *                       renderer_platform_begin_draw() Clear() call
 *                       into a new win_D2D_spy_clear_t entry (R27).
 */
static void win_D2D_spy_clear(win_d2d_ctx_t  *ptCtx,
                         D2D1_COLOR_F    tColor)
{
    // No LOG_TRACE - R22: spy capture invoked on every
    // renderer_platform_begin_draw() call - same tight-loop category
    // as the primitive it records.
    win_D2D_spy_clear_t *ptEntry;

    /* -- [SPY]9. Create a new spy of type ST_WIN_D2D_SPY_CLR -- */
    ptEntry = (win_D2D_spy_clear_t *)calloc(1, sizeof(win_D2D_spy_clear_t));
    ptEntry->ulMagic = 0xCAFEDECA;
    ptEntry->eObject = ST_WIN_D2D_SPY_CLR;
    
    // Catch background color
    ptEntry->tColor.r = tColor.r;
    ptEntry->tColor.g = tColor.g;
    ptEntry->tColor.b = tColor.b;
    ptEntry->tColor.a = tColor.a;
    
    win_D2D_spy_add((void*)ptEntry, ptCtx);
}

/*
 * win_D2D_spy_fill_rectangle() - Capture the parameters of one
 *                                renderer_platform_fill_rect() call
 *                                into a new
 *                                win_D2D_spy_fill_rectangle_t entry
 *                                (R27).
 */
static void win_D2D_spy_fill_rectangle(win_d2d_ctx_t     *ptCtx,
                                       const D2D1_RECT_F  tRect)
{
    // No LOG_TRACE - R22: spy capture invoked on every
    // renderer_platform_fill_rect() call - same tight-loop category
    // as the primitive it records.
    win_D2D_spy_fill_rectangle_t *ptEntry;
    D2D1_COLOR_F         tColor = ptCtx->tColor;
    
    /* -- [SPY]10. Create a new spy of type ST_WIN_D2D_SPY_FR -- */
    ptEntry = (win_D2D_spy_fill_rectangle_t*)calloc(1, sizeof(win_D2D_spy_fill_rectangle_t));
    ptEntry->ulMagic = 0xCAFEDECA;
    ptEntry->eObject = ST_WIN_D2D_SPY_FR;
    
    // Catch text rectangle area
    ptEntry->tRect.fX = tRect.left;
    ptEntry->tRect.fY = tRect.top;
    ptEntry->tRect.fW = tRect.right - tRect.left;
    ptEntry->tRect.fH = tRect.bottom - tRect.top;

    // Catch text color
    ptEntry->tColor.r = tColor.r;
    ptEntry->tColor.g = tColor.g;
    ptEntry->tColor.b = tColor.b;
    ptEntry->tColor.a = tColor.a;
    
    win_D2D_spy_add((void*)ptEntry, ptCtx);
}

/*
 * win_D2D_spy_draw_rectangle() - Capture the parameters of one
 *                                renderer_platform_draw_rect() call
 *                                into a new
 *                                win_D2D_spy_draw_rectangle_t entry
 *                                (R27).
 */
static void win_D2D_spy_draw_rectangle(win_d2d_ctx_t     *ptCtx,
                                       const D2D1_RECT_F  tRect,
                                       float              fStroke)
{
    // No LOG_TRACE - R22: spy capture invoked on every
    // renderer_platform_draw_rect() call - same tight-loop category
    // as the primitive it records.
    win_D2D_spy_draw_rectangle_t *ptEntry;
    D2D1_COLOR_F         tColor = ptCtx->tColor;
    
    /* -- [SPY]11. Create a new spy of type ST_WIN_D2D_SPY_DR -- */
    ptEntry = (win_D2D_spy_draw_rectangle_t*)calloc(1, sizeof(win_D2D_spy_draw_rectangle_t));
    ptEntry->ulMagic = 0xCAFEDECA;
    ptEntry->eObject = ST_WIN_D2D_SPY_DR;
    
    // Catch text rectangle area
    ptEntry->tRect.fX = tRect.left;
    ptEntry->tRect.fY = tRect.top;
    ptEntry->tRect.fW = tRect.right - tRect.left;
    ptEntry->tRect.fH = tRect.bottom - tRect.top;

    // Catch text color
    ptEntry->tColor.r = tColor.r;
    ptEntry->tColor.g = tColor.g;
    ptEntry->tColor.b = tColor.b;
    ptEntry->tColor.a = tColor.a;

    // Retrieve stroke width
    ptEntry->fStroke = fStroke;
    
    win_D2D_spy_add((void*)ptEntry, ptCtx);
}

/*
 * win_D2D_spy_draw_line() - Capture the parameters of one
 *                           renderer_platform_draw_line() call into a
 *                           new win_D2D_spy_draw_line_t entry (R27).
 */
static void win_D2D_spy_draw_line(win_d2d_ctx_t      *ptCtx,
                                  const D2D1_POINT_2F tP0,
                                  const D2D1_POINT_2F tP1,
                                  float               fStroke)
{
    // No LOG_TRACE - R22: spy capture invoked on every
    // renderer_platform_draw_line() call - same tight-loop category
    // as the primitive it records.
    win_D2D_spy_draw_line_t *ptEntry;
    D2D1_COLOR_F    tColor = ptCtx->tColor;
    
    /* -- [SPY]12. Create a new spy of type ST_WIN_D2D_SPY_DL -- */
    ptEntry = (win_D2D_spy_draw_line_t*)calloc(1, sizeof(win_D2D_spy_draw_line_t));
    ptEntry->ulMagic = 0xCAFEDECA;
    ptEntry->eObject = ST_WIN_D2D_SPY_DL;
    
    // Catch text rectangle area
    ptEntry->fX1 = tP0.x;
    ptEntry->fY1 = tP0.y;
    ptEntry->fX2 = tP1.x;
    ptEntry->fY2 = tP1.y;

    // Catch text color
    ptEntry->tColor.r = tColor.r;
    ptEntry->tColor.g = tColor.g;
    ptEntry->tColor.b = tColor.b;
    ptEntry->tColor.a = tColor.a;

    // Retrieve stroke width
    ptEntry->fStroke = fStroke;
    
    win_D2D_spy_add((void*)ptEntry, ptCtx);
}

/*
 * win_D2D_get_spy() - Retrieve a captured spy entry by index and type
 *                     (see win.h).
 */
const void *win_D2D_get_spy(int iIndex,
                            win_d2d_ctx_t  *ptCtx,
                            st_object_t     type)
{
    st_bool_t bOK;

    // No LOG_TRACE - R22: query function, called repeatedly by
    // use_case_*.c assertions.
    /* -- [SPY]6. Return NULL for index out of expected range -- */
    if (iIndex < -1 || iIndex >= ptCtx->uiSpiesCount
                    ||  iIndex >= WIN_D2D_SPY_MAX_ENTRIES)
    {
        return NULL;
    }
    
    /* -- [SPY]7. Look for last spy of corresponding type, if index is -1 -- */
    if (iIndex == -1)
    {
        for (int i = (ptCtx->uiSpiesCount - 1); i >= 0; i--)
        {
            CHECK_OBJ(ptCtx->pD2DSpies[i], type, bOK);
            if (bOK) 
            {
                return ptCtx->pD2DSpies[i];
            }
        }
        return NULL;    // No entry of type ST_WIN_D2D_SPY_DT has been found
    }
    else
    {
        /* -- [SPY]8. Return entry if type is confirmed, otherwise return NULL -- */
        CHECK_OBJ(ptCtx->pD2DSpies[iIndex], type, bOK);
        if (bOK) 
        {
            return ptCtx->pD2DSpies[iIndex];
        }
        else
        {
            return NULL;
        }
    }
}

/*
 * win_D2D_spy_find_text() - Find the first captured draw_text() call
 *                           whose wcText contains szNeedle (see win.h).
 */
int win_D2D_spy_find_text(const char *szNeedle, win_d2d_ctx_t *ptCtx)
{
    int      i;
    wchar_t  wNeedle[WIN_D2D_SPY_TEXT_LEN];
    const win_D2D_spy_draw_text_t *ptSpy;

    // No LOG_TRACE - R22: query function, called repeatedly by
    // use_case_*.c assertions.
    if (szNeedle == NULL || ptCtx == NULL)
    {
        return -1;
    }

    mbstowcs(wNeedle, szNeedle, sizeof(wNeedle) / sizeof(wNeedle[0]) - 1);
    wNeedle[sizeof(wNeedle) / sizeof(wNeedle[0]) - 1] = L'\0';

    /* -- [SPY]13. Scan the ring buffer for the first DrawText spy containing szNeedle -- */
    for (i = 0; i < ptCtx->uiSpiesCount; i++)
    {
        ptSpy = (const win_D2D_spy_draw_text_t *)
                win_D2D_get_spy(i, ptCtx, ST_WIN_D2D_SPY_DT);
        if (ptSpy == NULL) continue;
        if (wcsstr(ptSpy->wcText, wNeedle) != NULL) return i;
    }
    return -1;
}

/*
 * win_D2D_spy_find_fill_rect_color() - Find whether a captured
 *                                       fill_rectangle() call matches
 *                                       the given colour exactly
 *                                       (see win.h).
 */
st_bool_t win_D2D_spy_find_fill_rect_color(float           fR,
                                            float           fG,
                                            float           fB,
                                            float           fA,
                                            win_d2d_ctx_t  *ptCtx)
{
    int i;
    const win_D2D_spy_fill_rectangle_t *ptSpy;

    // No LOG_TRACE - R22: query function, called repeatedly by
    // use_case_*.c assertions.
    if (ptCtx == NULL)
    {
        return ST_FALSE;
    }

    /* -- [SPY]14. Scan the ring buffer for a FillRectangle spy matching the exact color -- */
    for (i = 0; i < ptCtx->uiSpiesCount; i++)
    {
        ptSpy = (const win_D2D_spy_fill_rectangle_t *)
                win_D2D_get_spy(i, ptCtx, ST_WIN_D2D_SPY_FR);
        if (ptSpy == NULL) continue;
        if (ptSpy->tColor.r == fR && ptSpy->tColor.g == fG
        &&  ptSpy->tColor.b == fB && ptSpy->tColor.a == fA)
        {
            return ST_TRUE;
        }
    }
    return ST_FALSE;
}

/* ------------------------------------------------------------------
 * END OF TEST RELATED SECTION
 * ------------------------------------------------------------------ */

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/*
 * win_d2d_set_color() - Update brush colour before a draw call.
 */
static void win_d2d_set_color(win_d2d_ctx_t          *ptD2D,
                                const renderer_color_t *ptColor)
{
    // No LOG_TRACE - R22: called on every draw primitive (fill/draw
    // rect/line/text) - same tight-loop category as its callers.
    D2D1_COLOR_F tC;

    tC.r = ptColor->r;
    tC.g = ptColor->g;
    tC.b = ptColor->b;
    tC.a = ptColor->a;
    ID2D1SolidColorBrush_SetColor(ptD2D->pBrush, &tC);
    ptD2D->tColor = tC;
}

/*
 * win_d2d_make_rect() - Convert renderer_rect_t to D2D1_RECT_F.
 */
static D2D1_RECT_F win_d2d_make_rect(const renderer_rect_t *ptRect)
{
    // No LOG_TRACE - R22: pure coordinate-conversion helper with no
    // side effect, called on every draw primitive.
    D2D1_RECT_F tR;

    tR.left   = ptRect->fX;
    tR.top    = ptRect->fY;
    tR.right  = ptRect->fX + ptRect->fW;
    tR.bottom = ptRect->fY + ptRect->fH;
    return tR;
}

/*
 * win_d2d_measure_mono() - Measure Consolas cell dimensions via
 *                           a single-char text layout of 'M'.
 */
static void win_d2d_measure_mono(win_d2d_ctx_t *ptD2D)
{
    IDWriteTextLayout   *pLayout;
    DWRITE_TEXT_METRICS  tM;
    WCHAR                wM;
    HRESULT              hr;

    /* Lifecycle: called once per renderer_platform_create() */
    LOG_TRACE("ptD2D=%p", (void *)ptD2D);

    wM = L'M';
    hr = IDWriteFactory_CreateTextLayout(ptD2D->pDWFactory,
                                          &wM, 1,
                                          ptD2D->pFmtMono,
                                          1000.0f, 1000.0f,
                                          &pLayout);
    if (FAILED(hr))
    {
        ptD2D->fMonoCellW = 8.0f;
        ptD2D->fMonoCellH = 16.0f;
        return;
    }

    memset(&tM, 0, sizeof(tM));
    if (SUCCEEDED(IDWriteTextLayout_GetMetrics(pLayout, &tM)))
    {
        ptD2D->fMonoCellW = tM.widthIncludingTrailingWhitespace;
        ptD2D->fMonoCellH = tM.height;
    }
    else
    {
        ptD2D->fMonoCellW = 8.0f;
        ptD2D->fMonoCellH = 16.0f;
    }

    IDWriteTextLayout_Release(pLayout);
}

/*
 * win_d2d_cleanup() - Release all D2D/DW COM objects and free context.
 *                     Safe to call even when partially initialised.
 */
static void win_d2d_cleanup(win_d2d_ctx_t *ptD2D)
{
    /* Lifecycle: called once from renderer_platform_destroy() and on
     * every renderer_platform_create() failure path */
    LOG_TRACE("ptD2D=%p", (void *)ptD2D);

    if (ptD2D == NULL)
    {
        return;
    }

    if (ptD2D->pBrush    != NULL)
    {
        ID2D1SolidColorBrush_Release(ptD2D->pBrush);
    }
    if (ptD2D->pFmtUI    != NULL)
    {
        IDWriteTextFormat_Release(ptD2D->pFmtUI);
    }
    if (ptD2D->pFmtMono  != NULL)
    {
        IDWriteTextFormat_Release(ptD2D->pFmtMono);
    }
    if (ptD2D->pDWFactory != NULL)
    {
        IDWriteFactory_Release(ptD2D->pDWFactory);
    }
    if (ptD2D->pRT        != NULL)
    {
        ID2D1HwndRenderTarget_Release(ptD2D->pRT);
    }
    if (ptD2D->pFactory   != NULL)
    {
        ID2D1Factory_Release(ptD2D->pFactory);
    }

    free(ptD2D);
}

/* ------------------------------------------------------------------
 * Platform backend implementation
 * ------------------------------------------------------------------ */

st_error_t renderer_platform_create(struct renderer_s *ptCtx,
                                      gui_window_t     hWnd)
{
    win_d2d_ctx_t                       *ptD2D;
    HWND                                 hNative;
    RECT                                 tClientRect;
    D2D1_SIZE_U                          tSize;
    D2D1_RENDER_TARGET_PROPERTIES        tRTProp;
    D2D1_HWND_RENDER_TARGET_PROPERTIES   tHwndProp;
    D2D1_COLOR_F                         tBlack;
    HRESULT                              hr;

    LOG_TRACE("ptCtx=%p hWnd=%p", (void *)ptCtx, (void *)hWnd);

    if (ptCtx == NULL || hWnd == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    hNative = (HWND)gui_platform_get_native_handle(
                  (struct gui_window_s *)hWnd);
    if (hNative == NULL)
    {
        LOG_ERROR("gui_platform_get_native_handle returned NULL");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)calloc(1, sizeof(win_d2d_ctx_t));
    if (ptD2D == NULL)
    {
        LOG_ERROR("malloc failed for win_d2d_ctx_t");
        return ST_ERROR;
    }

    /* Init the object magic & key */
    ptD2D->ulMagic = 0xCAFEDECA;
    ptD2D->eObject = ST_WIN_D2D_CTX;

    /* D2D factory (multi-threaded: safe to use from any thread) */
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED,
                            &IID_ID2D1Factory,
                            NULL,
                            (void **)&ptD2D->pFactory);
    if (FAILED(hr))
    {
        LOG_ERROR("D2D1CreateFactory failed: hr=0x%08lX", (unsigned long)hr);
        win_d2d_cleanup(ptD2D);
        return ST_ERROR;
    }

    /* HwndRenderTarget bound to the window */
    GetClientRect(hNative, &tClientRect);
    tSize.width  = (UINT32)(tClientRect.right  - tClientRect.left);
    tSize.height = (UINT32)(tClientRect.bottom - tClientRect.top);

    memset(&tRTProp, 0, sizeof(tRTProp));
    tRTProp.type                    = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    tRTProp.pixelFormat.format      = DXGI_FORMAT_UNKNOWN;
    tRTProp.pixelFormat.alphaMode   = D2D1_ALPHA_MODE_UNKNOWN;
    tRTProp.dpiX                    = 0.0f;
    tRTProp.dpiY                    = 0.0f;
    tRTProp.usage                   = D2D1_RENDER_TARGET_USAGE_NONE;
    tRTProp.minLevel                = D2D1_FEATURE_LEVEL_DEFAULT;

    tHwndProp.hwnd           = hNative;
    tHwndProp.pixelSize      = tSize;
    tHwndProp.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

    hr = ID2D1Factory_CreateHwndRenderTarget(ptD2D->pFactory,
                                              &tRTProp,
                                              &tHwndProp,
                                              &ptD2D->pRT);
    if (FAILED(hr))
    {
        LOG_ERROR("CreateHwndRenderTarget failed: hr=0x%08lX",
                  (unsigned long)hr);
        win_d2d_cleanup(ptD2D);
        return ST_ERROR;
    }

    /* DirectWrite factory (shared process-wide instance) */
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                              &IID_IDWriteFactory,
                              (IUnknown **)&ptD2D->pDWFactory);
    if (FAILED(hr))
    {
        LOG_ERROR("DWriteCreateFactory failed: hr=0x%08lX",
                  (unsigned long)hr);
        win_d2d_cleanup(ptD2D);
        return ST_ERROR;
    }

    /* Monospace font — Consolas RENDERER_FONT_SIZE_MONO pt */
    hr = IDWriteFactory_CreateTextFormat(
             ptD2D->pDWFactory,
             L"Consolas",
             NULL,
             DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_FONT_STYLE_NORMAL,
             DWRITE_FONT_STRETCH_NORMAL,
             RENDERER_FONT_SIZE_MONO,
             L"en-us",
             &ptD2D->pFmtMono);
    if (FAILED(hr))
    {
        LOG_ERROR("CreateTextFormat(mono) failed: hr=0x%08lX",
                  (unsigned long)hr);
        win_d2d_cleanup(ptD2D);
        return ST_ERROR;
    }

    /* UI font — Segoe UI RENDERER_FONT_SIZE_UI pt */
    hr = IDWriteFactory_CreateTextFormat(
             ptD2D->pDWFactory,
             L"Segoe UI",
             NULL,
             DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_FONT_STYLE_NORMAL,
             DWRITE_FONT_STRETCH_NORMAL,
             RENDERER_FONT_SIZE_UI,
             L"en-us",
             &ptD2D->pFmtUI);
    if (FAILED(hr))
    {
        LOG_ERROR("CreateTextFormat(ui) failed: hr=0x%08lX",
                  (unsigned long)hr);
        win_d2d_cleanup(ptD2D);
        return ST_ERROR;
    }

    /* One reusable solid color brush */
    tBlack.r = 0.0f; tBlack.g = 0.0f;
    tBlack.b = 0.0f; tBlack.a = 1.0f;
    hr = ID2D1RenderTarget_CreateSolidColorBrush(
             (ID2D1RenderTarget *)ptD2D->pRT,
             &tBlack, NULL,
             &ptD2D->pBrush);
    if (FAILED(hr))
    {
        LOG_ERROR("CreateSolidColorBrush failed: hr=0x%08lX",
                  (unsigned long)hr);
        win_d2d_cleanup(ptD2D);
        return ST_ERROR;
    }

    /* Cache mono cell metrics for renderer_get_font_metrics() */
    win_d2d_measure_mono(ptD2D);

    ptCtx->pPlatform = ptD2D;
    LOG_INFO("D2D renderer created: mono cell=%.1fx%.1f",
             (double)ptD2D->fMonoCellW, (double)ptD2D->fMonoCellH);
    return ST_NO_ERROR;
}

st_error_t renderer_platform_destroy(struct renderer_s *ptCtx)
{
    LOG_TRACE("ptCtx=%p", (void *)ptCtx);

    if (ptCtx == NULL)
    {
        LOG_ERROR("NULL ptCtx");
        return ST_ERROR;
    }

    win_d2d_cleanup((win_d2d_ctx_t *)ptCtx->pPlatform);
    ptCtx->pPlatform = NULL;
    return ST_NO_ERROR;
}

st_error_t renderer_platform_resize(struct renderer_s *ptCtx,
                                      int                iWidth,
                                      int                iHeight)
{
    win_d2d_ctx_t *ptD2D;
    D2D1_SIZE_U    tSize;
    HRESULT        hr;

    LOG_TRACE("ptCtx=%p %dx%d", (void *)ptCtx, iWidth, iHeight);

    if (ptCtx == NULL)
    {
        LOG_ERROR("NULL ptCtx");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL || ptD2D->pRT == NULL)
    {
        return ST_NO_ERROR;
    }

    tSize.width  = (UINT32)iWidth;
    tSize.height = (UINT32)iHeight;
    hr = ID2D1HwndRenderTarget_Resize(ptD2D->pRT, &tSize);
    if (FAILED(hr))
    {
        LOG_ERROR("HwndRenderTarget Resize failed: hr=0x%08lX",
                  (unsigned long)hr);
        return ST_ERROR;
    }

    return ST_NO_ERROR;
}

st_error_t renderer_platform_get_font_metrics(
                                      struct renderer_s  *ptCtx,
                                      renderer_font_id_t  eFontId,
                                      int                *piCellW,
                                      int                *piCellH)
{
    win_d2d_ctx_t *ptD2D;

    // No LOG_TRACE - R22 (established table): queried every layout
    // pass, same tight-loop category as the other renderer_platform_*
    // draw primitives below.
    if (ptCtx == NULL || piCellW == NULL || piCellH == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;

    if (eFontId == RENDERER_FONT_MONO && ptD2D != NULL)
    {
        *piCellW = (int)ptD2D->fMonoCellW;
        *piCellH = (int)ptD2D->fMonoCellH;
    }
    else
    {
        /* UI font or fallback */
        *piCellW = (int)RENDERER_FONT_SIZE_UI;
        *piCellH = (int)(RENDERER_FONT_SIZE_UI * 1.4f);
    }

    return ST_NO_ERROR;
}

st_error_t renderer_platform_begin_draw(
                                      struct renderer_s      *ptCtx,
                                      const renderer_color_t *ptBgColor)
{
    win_d2d_ctx_t *ptD2D;
    D2D1_COLOR_F   tClear;

    // No LOG_TRACE - R22 (established table): called once per
    // rendered frame, same tight-loop category as the other
    // renderer_platform_* draw primitives below.
    /* -- [D2D]1. NULL parameter are rejected with ST_ERROR and log message -- */
    if (ptCtx == NULL || ptBgColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- [D2D]2. NULL platform-specific context is rejected with ST_ERROR -- */
    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL || ptD2D->pRT == NULL)
    {
        LOG_ERROR("renderer not initialised - use renderer_create() first");
        return ST_ERROR;
    }

    /* -- [D2D]3. Call D2D primitives BeginDraw & Clear -- */
    tClear.r = ptBgColor->r;
    tClear.g = ptBgColor->g;
    tClear.b = ptBgColor->b;
    tClear.a = ptBgColor->a;

    /* D2D Primitives */
    ID2D1RenderTarget_BeginDraw((ID2D1RenderTarget *)ptD2D->pRT);
    ID2D1RenderTarget_Clear((ID2D1RenderTarget *)ptD2D->pRT, &tClear);

    if (ptCtx->bActiveSpies)
    {
        /* -- [D2D]22. Spy parameters sent to Win D2D Clear -- */
        win_D2D_spy_clear(ptD2D, tClear);
    }
        
    return ST_NO_ERROR;
}

st_error_t renderer_platform_end_draw(struct renderer_s *ptCtx)
{
    win_d2d_ctx_t *ptD2D;
    HRESULT        hr;

    // No LOG_TRACE - R22 (established table): called once per
    // rendered frame, same tight-loop category as the other
    // renderer_platform_* draw primitives.
    /* -- [D2D]4. NULL parameter are rejected with ST_ERROR and log message -- */
    if (ptCtx == NULL)
    {
        LOG_ERROR("NULL ptCtx");
        return ST_ERROR;
    }

    /* -- [D2D]5. NULL platform-specific context is rejected with ST_ERROR -- */
    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL || ptD2D->pRT == NULL)
    {
        LOG_ERROR("renderer not initialised - use renderer_create() first");
        return ST_ERROR;
    }

    /* -- [D2D]6. Call D2D primitive EndDraw -- */
    hr = ID2D1RenderTarget_EndDraw((ID2D1RenderTarget *)ptD2D->pRT, NULL, NULL);
    if (FAILED(hr))
    {
        LOG_ERROR("EndDraw failed: hr=0x%08lX", (unsigned long)hr);
        return ST_ERROR;
    }

    return ST_NO_ERROR;
}

st_error_t renderer_platform_fill_rect(
                                      struct renderer_s      *ptCtx,
                                      const renderer_rect_t  *ptRect,
                                      const renderer_color_t *ptColor)
{
    win_d2d_ctx_t *ptD2D;
    D2D1_RECT_F    tR;

    // No LOG_TRACE - R22 (established table): called once per draw
    // call inside the paint loop, same tight-loop category as the
    // other renderer_platform_* draw primitives.
    /* -- [D2D]7. NULL parameter are rejected with ST_ERROR and log message -- */
    if (ptCtx == NULL || ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- [D2D]8. NULL platform-specific context is rejected with ST_ERROR -- */
    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL)
    {
        LOG_ERROR("renderer not initialised - use renderer_create() first");
        return ST_ERROR;
    }

    /* -- [D2D]9. Call D2D Primitive FillRectangle -- */
    tR = win_d2d_make_rect(ptRect);
    win_d2d_set_color(ptD2D, ptColor);
    ID2D1RenderTarget_FillRectangle(
        (ID2D1RenderTarget *)ptD2D->pRT,
        &tR,
        (ID2D1Brush *)ptD2D->pBrush);

    if (ptCtx->bActiveSpies)
    {
        /* -- [D2D]23. Spy parameters sent to Win D2D FillRectangle -- */
        win_D2D_spy_fill_rectangle(ptD2D, tR);
    }
        
    return ST_NO_ERROR;
}

st_error_t renderer_platform_draw_rect(
                                      struct renderer_s      *ptCtx,
                                      const renderer_rect_t  *ptRect,
                                      const renderer_color_t *ptColor,
                                      float                   fStroke)
{
    win_d2d_ctx_t *ptD2D;
    D2D1_RECT_F    tR;

    // No LOG_TRACE - R22 (established table): called once per draw
    // call inside the paint loop, same tight-loop category as the
    // other renderer_platform_* draw primitives.
    /* -- [D2D]10. NULL parameter are rejected with ST_ERROR and log message -- */
    if (ptCtx == NULL || ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- [D2D]11. NULL platform-specific context is rejected with ST_ERROR -- */
    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL)
    {
        LOG_ERROR("renderer not initialised - use renderer_create() first");
        return ST_ERROR;
    }

    /* -- [D2D]12. Call D2D Primitive DrawRectangle -- */
    tR = win_d2d_make_rect(ptRect);
    win_d2d_set_color(ptD2D, ptColor);
    ID2D1RenderTarget_DrawRectangle(
        (ID2D1RenderTarget *)ptD2D->pRT,
        &tR,
        (ID2D1Brush *)ptD2D->pBrush,
        fStroke,
        NULL);
    
    if (ptCtx->bActiveSpies)
    {
        /* -- [D2D]24. Spy parameters sent to Win D2D DrawRectangle -- */
        win_D2D_spy_draw_rectangle(ptD2D, tR, fStroke);
    }
    
    return ST_NO_ERROR;
}

st_error_t renderer_platform_draw_line(
                                      struct renderer_s      *ptCtx,
                                      float                   fX1,
                                      float                   fY1,
                                      float                   fX2,
                                      float                   fY2,
                                      const renderer_color_t *ptColor,
                                      float                   fStroke)
{
    win_d2d_ctx_t  *ptD2D;
    D2D1_POINT_2F   tP0;
    D2D1_POINT_2F   tP1;

    // No LOG_TRACE - R22 (established table): called once per draw
    // call inside the paint loop, same tight-loop category as the
    // other renderer_platform_* draw primitives.
    /* -- [D2D]13. NULL parameter are rejected with ST_ERROR and log message -- */
    if (ptCtx == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- [D2D]14. NULL platform-specific context is rejected with ST_ERROR -- */
    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL)
    {
        LOG_ERROR("renderer not initialised - use renderer_create() first");
        return ST_ERROR;
    }

    /* -- [D2D]15. Call D2D Primitive DrawLine -- */
    tP0.x = fX1; tP0.y = fY1;
    tP1.x = fX2; tP1.y = fY2;
    win_d2d_set_color(ptD2D, ptColor);
    ID2D1RenderTarget_DrawLine(
        (ID2D1RenderTarget *)ptD2D->pRT,
        tP0, tP1,
        (ID2D1Brush *)ptD2D->pBrush,
        fStroke,
        NULL);

    if (ptCtx->bActiveSpies)
    {
        /* -- [D2D]25. Spy parameters sent to Win D2D DrawLine -- */
        win_D2D_spy_draw_line(ptD2D, tP0, tP1, fStroke);
    }
    
    return ST_NO_ERROR;
}

st_error_t renderer_platform_draw_text(struct renderer_s     *ptCtx,
                                      const char             *szText,
                                      const renderer_rect_t  *ptRect,
                                      const renderer_color_t *ptColor,
                                      renderer_font_id_t      eFontId,
                                      renderer_align_t        eAlign)
{
    win_d2d_ctx_t          *ptD2D;
    IDWriteTextFormat      *pFmt;
    DWRITE_TEXT_ALIGNMENT   eDWAlign;
    D2D1_RECT_F             tR;
    WCHAR                   awBuf[ST_MAX_MSG];
    UINT32                  uiLen;
    D2D1_DRAW_TEXT_OPTIONS eTextOpts = D2D1_DRAW_TEXT_OPTIONS_CLIP;
    DWRITE_MEASURING_MODE  eMeasureMode = DWRITE_MEASURING_MODE_NATURAL;

    // No LOG_TRACE - R22 (established table): called once per draw
    // call inside the paint loop, same tight-loop category as the
    // other renderer_platform_* draw primitives.
    /* -- [D2D]16. NULL parameter are rejected with ST_ERROR and log message -- */
    if (ptCtx == NULL || szText == NULL || ptRect == NULL
    ||  ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- [D2D]17. NULL platform-specific context is rejected with ST_ERROR -- */
    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL)
    {
        LOG_ERROR("renderer not initialised - use renderer_create() first");
        return ST_ERROR;
    }

    /* -- [D2D]18. Draw text manages MONO FONT or UI internal font -- */
    pFmt = (eFontId == RENDERER_FONT_MONO)
           ? ptD2D->pFmtMono
           : ptD2D->pFmtUI;

    /* -- [D2D]19. Draw text manages Text Alignment -- */
    switch (eAlign)
    {
    case RENDERER_ALIGN_CENTER:
        eDWAlign = DWRITE_TEXT_ALIGNMENT_CENTER;
        break;
    case RENDERER_ALIGN_RIGHT:
        eDWAlign = DWRITE_TEXT_ALIGNMENT_TRAILING;
        break;
    default:
        eDWAlign = DWRITE_TEXT_ALIGNMENT_LEADING;
        break;
    }

    IDWriteTextFormat_SetTextAlignment(pFmt, eDWAlign);

    /* -- [D2D]20. Call D2D Primitive DrawText -- */
    /* UTF-8 → wide string */
    uiLen = MultiByteToWideChar(CP_UTF8, 0, szText, -1,
                                awBuf, ST_MAX_MSG - 1);
    if (uiLen <= 0)
    {
        LOG_ERROR("MultiByteToWideChar failed: %lu", GetLastError());
        IDWriteTextFormat_SetTextAlignment(
            pFmt, DWRITE_TEXT_ALIGNMENT_LEADING);
        return ST_ERROR;
    }

    tR = win_d2d_make_rect(ptRect);
    win_d2d_set_color(ptD2D, ptColor);

    ID2D1RenderTarget_DrawText(
        (ID2D1RenderTarget *)ptD2D->pRT,
        awBuf,
        (UINT32)(uiLen - 1),  /* exclude null terminator */
        pFmt,
        &tR,
        (ID2D1Brush *)ptD2D->pBrush,
        eTextOpts,
        eMeasureMode);

    if (ptCtx->bActiveSpies)
    {
        /* -- [D2D]21. Spy parameters sent to Win D2D DrawText -- */
        win_D2D_spy_draw_text(ptD2D, awBuf, uiLen - 1, pFmt, tR, eTextOpts, eMeasureMode);
    }
    
    /* Reset to default alignment for next caller */
    IDWriteTextFormat_SetTextAlignment(
        pFmt, DWRITE_TEXT_ALIGNMENT_LEADING);

    return ST_NO_ERROR;
}

st_error_t renderer_platform_draw_bitmap(
                                      struct renderer_s      *ptCtx,
                                      const st_u8_t         *pPixels,
                                      int                    iSrcW,
                                      int                    iSrcH,
                                      const renderer_rect_t *ptDest)
{
    win_d2d_ctx_t          *ptD2D;
    ID2D1Bitmap            *pBmp;
    D2D1_BITMAP_PROPERTIES  tBmpProps;
    D2D1_SIZE_U             tBmpSize;
    D2D1_RECT_F             tDestR;
    HRESULT                 hr;

    // No LOG_TRACE - R22: called once per rendered frame (screen
    // emulator refresh, UC27) - same tight-loop category as the other
    // renderer_platform_* draw primitives above.
    if (ptCtx == NULL || pPixels == NULL || ptDest == NULL
    ||  iSrcW <= 0 || iSrcH <= 0)
    {
        LOG_ERROR("NULL parameter or zero dimension");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL || ptD2D->pRT == NULL)
    {
        LOG_ERROR("renderer not initialised");
        return ST_ERROR;
    }

    /* BGRA8 + ALPHA_IGNORE: shifter output 0x00RRGGBB maps to bytes
     * [B,G,R,0x00] in memory — correct BGRA layout; alpha 0x00 is ignored
     * so all pixels render as fully opaque. */
    memset(&tBmpProps, 0, sizeof(tBmpProps));
    tBmpProps.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    tBmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    tBmpProps.dpiX                  = 0.0f;
    tBmpProps.dpiY                  = 0.0f;

    tBmpSize.width  = (UINT32)iSrcW;
    tBmpSize.height = (UINT32)iSrcH;

    hr = ID2D1RenderTarget_CreateBitmap(
             (ID2D1RenderTarget *)ptD2D->pRT,
             tBmpSize,
             pPixels,
             (UINT32)(iSrcW * 4),
             &tBmpProps,
             &pBmp);
    if (FAILED(hr))
    {
        LOG_ERROR("CreateBitmap failed: hr=0x%08lX", (unsigned long)hr);
        return ST_ERROR;
    }

    tDestR = win_d2d_make_rect(ptDest);
    /* Nearest-neighbour: preserves sharp pixel art look of ST screen */
    ID2D1RenderTarget_DrawBitmap(
        (ID2D1RenderTarget *)ptD2D->pRT,
        pBmp,
        &tDestR,
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
        NULL);

    ID2D1Bitmap_Release(pBmp);
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_WINDOWS */
