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

/* ------------------------------------------------------------------
 * Internal context
 * ------------------------------------------------------------------ */

typedef struct
{
    ID2D1Factory            *pFactory;
    ID2D1HwndRenderTarget   *pRT;
    IDWriteFactory          *pDWFactory;
    IDWriteTextFormat       *pFmtMono;
    IDWriteTextFormat       *pFmtUI;
    ID2D1SolidColorBrush    *pBrush;
    float                    fMonoCellW;
    float                    fMonoCellH;
} win_d2d_ctx_t;

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/*
 * win_d2d_set_color() - Update brush colour before a draw call.
 */
static void win_d2d_set_color(win_d2d_ctx_t          *ptD2D,
                                const renderer_color_t *ptColor)
{
    D2D1_COLOR_F tC;

    tC.r = ptColor->r;
    tC.g = ptColor->g;
    tC.b = ptColor->b;
    tC.a = ptColor->a;
    ID2D1SolidColorBrush_SetColor(ptD2D->pBrush, &tC);
}

/*
 * win_d2d_make_rect() - Convert renderer_rect_t to D2D1_RECT_F.
 */
static D2D1_RECT_F win_d2d_make_rect(const renderer_rect_t *ptRect)
{
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
                                      gui_window_t       hWnd)
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

    if (ptCtx == NULL || ptBgColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL || ptD2D->pRT == NULL)
    {
        LOG_ERROR("renderer not initialised");
        return ST_ERROR;
    }

    tClear.r = ptBgColor->r;
    tClear.g = ptBgColor->g;
    tClear.b = ptBgColor->b;
    tClear.a = ptBgColor->a;

    ID2D1RenderTarget_BeginDraw((ID2D1RenderTarget *)ptD2D->pRT);
    ID2D1RenderTarget_Clear((ID2D1RenderTarget *)ptD2D->pRT, &tClear);
    return ST_NO_ERROR;
}

st_error_t renderer_platform_end_draw(struct renderer_s *ptCtx)
{
    win_d2d_ctx_t *ptD2D;
    HRESULT        hr;

    if (ptCtx == NULL)
    {
        LOG_ERROR("NULL ptCtx");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL || ptD2D->pRT == NULL)
    {
        LOG_ERROR("renderer not initialised");
        return ST_ERROR;
    }

    hr = ID2D1RenderTarget_EndDraw(
             (ID2D1RenderTarget *)ptD2D->pRT, NULL, NULL);
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

    if (ptCtx == NULL || ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL)
    {
        return ST_ERROR;
    }

    tR = win_d2d_make_rect(ptRect);
    win_d2d_set_color(ptD2D, ptColor);
    ID2D1RenderTarget_FillRectangle(
        (ID2D1RenderTarget *)ptD2D->pRT,
        &tR,
        (ID2D1Brush *)ptD2D->pBrush);
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

    if (ptCtx == NULL || ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL)
    {
        return ST_ERROR;
    }

    tR = win_d2d_make_rect(ptRect);
    win_d2d_set_color(ptD2D, ptColor);
    ID2D1RenderTarget_DrawRectangle(
        (ID2D1RenderTarget *)ptD2D->pRT,
        &tR,
        (ID2D1Brush *)ptD2D->pBrush,
        fStroke,
        NULL);
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

    if (ptCtx == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL)
    {
        return ST_ERROR;
    }

    tP0.x = fX1; tP0.y = fY1;
    tP1.x = fX2; tP1.y = fY2;
    win_d2d_set_color(ptD2D, ptColor);
    ID2D1RenderTarget_DrawLine(
        (ID2D1RenderTarget *)ptD2D->pRT,
        tP0, tP1,
        (ID2D1Brush *)ptD2D->pBrush,
        fStroke,
        NULL);
    return ST_NO_ERROR;
}

st_error_t renderer_platform_draw_text(
                                      struct renderer_s      *ptCtx,
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
    int                     iLen;

    if (ptCtx == NULL || szText == NULL || ptRect == NULL
    ||  ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    ptD2D = (win_d2d_ctx_t *)ptCtx->pPlatform;
    if (ptD2D == NULL)
    {
        return ST_ERROR;
    }

    pFmt = (eFontId == RENDERER_FONT_MONO)
           ? ptD2D->pFmtMono
           : ptD2D->pFmtUI;

    /* Map alignment */
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

    /* UTF-8 → wide string */
    iLen = MultiByteToWideChar(CP_UTF8, 0, szText, -1,
                                awBuf, ST_MAX_MSG - 1);
    if (iLen <= 0)
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
        (UINT32)(iLen - 1),  /* exclude null terminator */
        pFmt,
        &tR,
        (ID2D1Brush *)ptD2D->pBrush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL);

    /* Reset to default alignment for next caller */
    IDWriteTextFormat_SetTextAlignment(
        pFmt, DWRITE_TEXT_ALIGNMENT_LEADING);

    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_WINDOWS */
