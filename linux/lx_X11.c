/*
 * lx_X11.c - X11 / XRender renderer backend for renderer.h
 *
 * Each renderer_t is backed by an lx_renderer_ctx_t holding:
 *   Display*    - shared with lx_gui.c
 *   Window      - the target XID from lx_gui.c
 *   GC          - graphics context for basic draw primitives
 *   XftDraw*    - Xft anti-aliased text rendering (optional, UC3+)
 *   Pixmap      - double-buffer pixmap for flicker-free rendering
 *
 * Double-buffer strategy:
 *   All drawing goes to the off-screen Pixmap.
 *   renderer_end_draw() calls XCopyArea(pDpy, pixmap, window, ...)
 *   then XFlush().
 *
 * Colour model:
 *   renderer_color_t (float RGBA) → XAllocColor (XColor with 16-bit RGB).
 *   Colours are cached in a small LRU table to avoid exhausting colormaps.
 *
 * Text rendering:
 *   RENDERER_FONT_MONO  → XLoadFont "fixed" or Xft "monospace:size=10"
 *   RENDERER_FONT_UI    → Xft "sans:size=10"
 *
 * TODO(UC3): Include <X11/Xlib.h>, <X11/Xft/Xft.h> and implement.
 *            Link with: -lX11 -lXft -lfontconfig
 */

#include "../src/common.h"
#include "../src/trace.h"
#include "../src/renderer.h"

#ifdef ST_PLATFORM_LINUX

st_error_t lx_x11_renderer_create(void *pDisplay,
                                     unsigned long xWnd,
                                     void        **ppCtx)
{
    LOG_TRACE("pDisplay=%p xWnd=%lu", pDisplay, xWnd);
    ST_UNUSED(pDisplay); ST_UNUSED(xWnd);
    if (ppCtx != NULL) { *ppCtx = NULL; }
    LOG_TODO("lx_x11_renderer_create: GC + Pixmap init (UC3)");
    return ST_NO_ERROR;
}

st_error_t lx_x11_renderer_destroy(void **ppCtx)
{
    LOG_TRACE("ppCtx=%p", (void *)ppCtx);
    if (ppCtx != NULL) { *ppCtx = NULL; }
    LOG_TODO("lx_x11_renderer_destroy: XFreeGC / XFreePixmap (UC3)");
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_LINUX */
