/*
 * lx_gui.c - X11 window management backend for gui.h
 *
 * Implements gui_platform_* functions declared in gui_backend.h.
 *
 * Each gui_window_t is backed by an lx_wnd_state_t that holds:
 *   - Xlib Display* and Window (XID)
 *   - The dedicated thread running XNextEvent loop
 *   - Back-pointer to the portable gui_window_s
 *
 * X11 window creation per window:
 *   Display *pDpy = XOpenDisplay(NULL)
 *   Window   xWnd = XCreateSimpleWindow(pDpy, ...)
 *   XSelectInput(pDpy, xWnd, ExposureMask | KeyPressMask |
 *                ButtonPressMask | StructureNotifyMask)
 *   XMapWindow(pDpy, xWnd)
 *
 * Event loop translates XEvent → gui_event_t → gui_event_fn callback.
 *
 * TODO(UC3-Linux): Implement all gui_platform_* with Xlib.
 */

#include "../src/gui_backend.h"
#include "../src/trace.h"

#ifdef ST_PLATFORM_LINUX

st_error_t gui_platform_init(void)
{
    LOG_TRACE("(void)");
    LOG_TODO("gui_platform_init: XOpenDisplay + Xlib setup (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t gui_platform_window_create(struct gui_window_s *ptWnd)
{
    LOG_TRACE("ptWnd=%p", (void *)ptWnd);
    if (ptWnd == NULL)
    {
        LOG_ERROR("NULL ptWnd");
        return ST_ERROR;
    }
    ptWnd->bOpen     = ST_FALSE;
    ptWnd->pPlatform = NULL;
    ptWnd->ptThread  = NULL;
    LOG_TODO("gui_platform_window_create: XCreateWindow + thread"
             " (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t gui_platform_window_close(struct gui_window_s *ptWnd)
{
    LOG_TRACE("ptWnd=%p", (void *)ptWnd);
    if (ptWnd == NULL)
    {
        LOG_ERROR("NULL ptWnd");
        return ST_ERROR;
    }
    LOG_TODO("gui_platform_window_close: XDestroyWindow (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t gui_platform_window_invalidate(struct gui_window_s *ptWnd)
{
    LOG_TRACE("ptWnd=%p", (void *)ptWnd);
    if (ptWnd == NULL)
    {
        LOG_ERROR("NULL ptWnd");
        return ST_ERROR;
    }
    LOG_TODO("gui_platform_window_invalidate: XClearArea (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t gui_platform_window_get_size(struct gui_window_s *ptWnd,
                                         int                 *piWidth,
                                         int                 *piHeight)
{
    LOG_TRACE("ptWnd=%p", (void *)ptWnd);
    if (ptWnd == NULL || piWidth == NULL || piHeight == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    *piWidth  = 0;
    *piHeight = 0;
    LOG_TODO("gui_platform_window_get_size: XGetGeometry (UC3-Linux)");
    return ST_NO_ERROR;
}

st_error_t gui_platform_shutdown(void)
{
    LOG_TRACE("(void)");
    LOG_TODO("gui_platform_shutdown: XCloseDisplay (UC3-Linux)");
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_LINUX */
