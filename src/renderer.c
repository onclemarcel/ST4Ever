/*
 * renderer.c - ST4Ever renderer portable layer
 *
 * Delegates every draw operation to renderer_platform_* functions
 * implemented in the platform backend (win/win_D2D.c, linux/lx_X11.c).
 *
 * Responsibilities of this layer:
 *   - NULL-parameter validation (returns ST_ERROR before any platform call)
 *   - struct renderer_s allocation / deallocation
 *   - Forwarding to renderer_platform_* with minimal overhead
 *
 * renderer_draw_bitmap() → stub (UC26 screen emulator).
 *
 * UC3.2: fully wired — all draw primitives active via win_D2D.c.
 */

#include "renderer_backend.h"
#include "trace.h"
#include <stdlib.h>
#include <string.h>

st_error_t renderer_create(gui_window_t hWnd, renderer_t *phCtx)
{
    struct renderer_s *ptCtx;
    st_error_t         eResult;

    LOG_TRACE("hWnd=%p phCtx=%p", (void *)hWnd, (void *)phCtx);

    if (hWnd == NULL || phCtx == NULL)
    {
        LOG_ERROR("NULL parameter: hWnd=%p phCtx=%p",
                  (void *)hWnd, (void *)phCtx);
        return ST_ERROR;
    }

    *phCtx = NULL;

    ptCtx = (struct renderer_s *)malloc(sizeof(struct renderer_s));
    if (ptCtx == NULL)
    {
        LOG_ERROR("malloc failed for renderer_s");
        return ST_ERROR;
    }
    memset(ptCtx, 0, sizeof(struct renderer_s));

    eResult = renderer_platform_create(ptCtx, hWnd);
    if (eResult != ST_NO_ERROR)
    {
        free(ptCtx);
        LOG_ERROR("renderer_platform_create failed");
        return ST_ERROR;
    }

    *phCtx = (renderer_t)ptCtx;
    LOG_INFO("renderer created for window %p", (void *)hWnd);
    return ST_NO_ERROR;
}

st_error_t renderer_resize(renderer_t hCtx, int iWidth, int iHeight)
{
    LOG_TRACE("hCtx=%p %dx%d", (void *)hCtx, iWidth, iHeight);

    if (hCtx == NULL)
    {
        LOG_ERROR("NULL hCtx");
        return ST_ERROR;
    }

    return renderer_platform_resize(
               (struct renderer_s *)hCtx, iWidth, iHeight);
}

st_error_t renderer_get_font_metrics(renderer_t         hCtx,
                                      renderer_font_id_t eFontId,
                                      int               *piCellW,
                                      int               *piCellH)
{
    if (hCtx == NULL || piCellW == NULL || piCellH == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    return renderer_platform_get_font_metrics(
               (struct renderer_s *)hCtx, eFontId, piCellW, piCellH);
}

st_error_t renderer_begin_draw(renderer_t             hCtx,
                                const renderer_color_t *ptBgColor)
{
    if (hCtx == NULL || ptBgColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    return renderer_platform_begin_draw(
               (struct renderer_s *)hCtx, ptBgColor);
}

st_error_t renderer_end_draw(renderer_t hCtx)
{
    if (hCtx == NULL)
    {
        LOG_ERROR("NULL hCtx");
        return ST_ERROR;
    }

    return renderer_platform_end_draw((struct renderer_s *)hCtx);
}

st_error_t renderer_fill_rect(renderer_t             hCtx,
                                const renderer_rect_t  *ptRect,
                                const renderer_color_t *ptColor)
{
    if (hCtx == NULL || ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    return renderer_platform_fill_rect(
               (struct renderer_s *)hCtx, ptRect, ptColor);
}

st_error_t renderer_draw_rect(renderer_t             hCtx,
                                const renderer_rect_t  *ptRect,
                                const renderer_color_t *ptColor,
                                float                   fStroke)
{
    if (hCtx == NULL || ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    return renderer_platform_draw_rect(
               (struct renderer_s *)hCtx, ptRect, ptColor, fStroke);
}

st_error_t renderer_draw_line(renderer_t             hCtx,
                                float                  fX1,
                                float                  fY1,
                                float                  fX2,
                                float                  fY2,
                                const renderer_color_t *ptColor,
                                float                   fStroke)
{
    if (hCtx == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    return renderer_platform_draw_line(
               (struct renderer_s *)hCtx,
               fX1, fY1, fX2, fY2, ptColor, fStroke);
}

st_error_t renderer_draw_text(renderer_t              hCtx,
                                const char             *szText,
                                const renderer_rect_t  *ptRect,
                                const renderer_color_t *ptColor,
                                renderer_font_id_t      eFontId,
                                renderer_align_t        eAlign)
{
    if (hCtx == NULL || szText == NULL
    ||  ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    return renderer_platform_draw_text(
               (struct renderer_s *)hCtx,
               szText, ptRect, ptColor, eFontId, eAlign);
}

st_error_t renderer_draw_bitmap(renderer_t            hCtx,
                                  const st_u8_t        *pPixels,
                                  int                   iSrcW,
                                  int                   iSrcH,
                                  const renderer_rect_t *ptDest)
{
    ST_UNUSED(hCtx);
    ST_UNUSED(pPixels);
    ST_UNUSED(iSrcW);
    ST_UNUSED(iSrcH);
    ST_UNUSED(ptDest);
    LOG_TODO("renderer_draw_bitmap: Atari ST screen blit (UC26)");
    return ST_NO_ERROR;
}

st_error_t renderer_destroy(renderer_t *phCtx)
{
    struct renderer_s *ptCtx;

    LOG_TRACE("phCtx=%p", (void *)phCtx);

    if (phCtx == NULL || *phCtx == NULL)
    {
        LOG_ERROR("NULL phCtx or *phCtx");
        return ST_ERROR;
    }

    ptCtx = (struct renderer_s *)*phCtx;
    renderer_platform_destroy(ptCtx);
    free(ptCtx);
    *phCtx = NULL;

    LOG_INFO("renderer destroyed");
    return ST_NO_ERROR;
}
