/*
 * lx_X11.c - X11 / XRender renderer backend for renderer.h
 *
 * Implements renderer_platform_* functions declared in renderer_backend.h.
 *
 * Each renderer_t is backed by an lx_rnd_ctx_t holding:
 *   Display*  - shared X11 display connection from lx_gui.c
 *   Window    - target XID from lx_gui.c
 *   GC        - graphics context for draw primitives
 *   XftDraw*  - Xft anti-aliased text rendering
 *   Pixmap    - off-screen double-buffer (XCopyArea in end_draw)
 *
 * Colour model:
 *   renderer_color_t (float RGBA) -> XAllocColor (XColor 16-bit RGB).
 *   Colours are cached in a small table to avoid exhausting colormaps.
 *
 * Text rendering:
 *   RENDERER_FONT_MONO -> Xft "monospace:size=13"
 *   RENDERER_FONT_UI   -> Xft "sans:size=12"
 *
 * TODO(UC3-Linux): Implement with <X11/Xlib.h> and <X11/Xft/Xft.h>.
 *                  Link with: -lX11 -lXft -lfontconfig
 */

#include "../src/renderer_backend.h"
#include "../src/gui_backend.h"
#include "../src/trace.h"

#ifdef ST_PLATFORM_LINUX

st_error_t renderer_platform_create(struct renderer_s *ptCtx,
                                      gui_window_t       hWnd)
{
    LOG_TRACE("ptCtx=%p hWnd=%p", (void *)ptCtx, (void *)hWnd);
    if (ptCtx == NULL || hWnd == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    ptCtx->pPlatform = NULL;
    LOG_TODO("renderer_platform_create: GC + Pixmap + Xft (UC3-Linux)");
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
    ptCtx->pPlatform = NULL;
    LOG_TODO("renderer_platform_destroy: XFreeGC / XFreePixmap (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t renderer_platform_resize(struct renderer_s *ptCtx,
                                      int                iWidth,
                                      int                iHeight)
{
    LOG_TRACE("ptCtx=%p %dx%d", (void *)ptCtx, iWidth, iHeight);
    if (ptCtx == NULL)
    {
        LOG_ERROR("NULL ptCtx");
        return ST_ERROR;
    }
    LOG_TODO("renderer_platform_resize: recreate Pixmap (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t renderer_platform_get_font_metrics(
                                      struct renderer_s  *ptCtx,
                                      renderer_font_id_t  eFontId,
                                      int                *piCellW,
                                      int                *piCellH)
{
    LOG_TRACE("ptCtx=%p fontId=%d", (void *)ptCtx, (int)eFontId);
    if (ptCtx == NULL || piCellW == NULL || piCellH == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    *piCellW = 8;
    *piCellH = 16;
    LOG_TODO("renderer_platform_get_font_metrics: Xft metrics (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t renderer_platform_begin_draw(
                                      struct renderer_s      *ptCtx,
                                      const renderer_color_t *ptBgColor)
{
    LOG_TRACE("ptCtx=%p", (void *)ptCtx);
    if (ptCtx == NULL || ptBgColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    LOG_TODO("renderer_platform_begin_draw: XFillRect on Pixmap (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t renderer_platform_end_draw(struct renderer_s *ptCtx)
{
    LOG_TRACE("ptCtx=%p", (void *)ptCtx);
    if (ptCtx == NULL)
    {
        LOG_ERROR("NULL ptCtx");
        return ST_ERROR;
    }
    LOG_TODO("renderer_platform_end_draw: XCopyArea + XFlush (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t renderer_platform_fill_rect(
                                      struct renderer_s      *ptCtx,
                                      const renderer_rect_t  *ptRect,
                                      const renderer_color_t *ptColor)
{
    LOG_TRACE("ptCtx=%p", (void *)ptCtx);
    if (ptCtx == NULL || ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    LOG_TODO("renderer_platform_fill_rect: XFillRectangle (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t renderer_platform_draw_rect(
                                      struct renderer_s      *ptCtx,
                                      const renderer_rect_t  *ptRect,
                                      const renderer_color_t *ptColor,
                                      float                   fStroke)
{
    LOG_TRACE("ptCtx=%p stroke=%.1f", (void *)ptCtx, (double)fStroke);
    if (ptCtx == NULL || ptRect == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    LOG_TODO("renderer_platform_draw_rect: XDrawRectangle (UC3-Linux)");
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
    LOG_TRACE("ptCtx=%p (%.0f,%.0f)-(%.0f,%.0f)",
              (void *)ptCtx,
              (double)fX1, (double)fY1,
              (double)fX2, (double)fY2);
    if (ptCtx == NULL || ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    LOG_TODO("renderer_platform_draw_line: XDrawLine (UC3-Linux)");
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
    LOG_TRACE("ptCtx=%p text='%.20s'",
              (void *)ptCtx, szText ? szText : "(null)");
    if (ptCtx == NULL || szText == NULL || ptRect == NULL
    ||  ptColor == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    LOG_TODO("renderer_platform_draw_text: XftDrawString (UC3-Linux)");
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_LINUX */
