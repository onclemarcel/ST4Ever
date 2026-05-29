/*
 * lx_gui.c - X11 window management backend for gui.h
 *
 * Each gui_window_t is backed by an lx_wnd_state_t that holds:
 *   - Xlib Display* and Window (XID)
 *   - The dedicated thread running XNextEvent loop
 *   - The event callback and user context from gui_wnd_desc_t
 *
 * X11 window creation per window (UC3):
 *   Display *pDpy = XOpenDisplay(NULL)
 *   Window   xWnd = XCreateSimpleWindow(pDpy, ...)
 *   XSelectInput(pDpy, xWnd, ExposureMask | KeyPressMask |
 *                ButtonPressMask | StructureNotifyMask)
 *   XMapWindow(pDpy, xWnd)
 *
 * Event loop translates XEvent → gui_event_t → gui_event_fn callback.
 *
 * TODO(UC3): Implement lx_wnd_create(), XNextEvent loop thread,
 *            XDestroyWindow in lx_gui_close_window().
 */

#include "../src/common.h"
#include "../src/trace.h"
#include "../src/gui.h"

#ifdef ST_PLATFORM_LINUX

st_error_t lx_gui_init(void)
{
    LOG_TRACE("(void)");
    LOG_TODO("lx_gui_init: XOpenDisplay + Xlib setup (UC3)");
    return ST_NO_ERROR;
}

st_error_t lx_gui_open_window(const gui_wnd_desc_t *ptDesc,
                                 gui_window_t         *phWnd)
{
    LOG_TRACE("title='%s'",
              ptDesc ? ptDesc->szTitle : "(null)");
    ST_UNUSED(ptDesc);
    if (phWnd != NULL) { *phWnd = NULL; }
    LOG_TODO("lx_gui_open_window: XCreateWindow + thread (UC3)");
    return ST_NO_ERROR;
}

st_error_t lx_gui_close_window(gui_window_t hWnd)
{
    LOG_TRACE("hWnd=%p", (void *)hWnd);
    ST_UNUSED(hWnd);
    LOG_TODO("lx_gui_close_window: XDestroyWindow (UC3)");
    return ST_NO_ERROR;
}

st_error_t lx_gui_shutdown(void)
{
    LOG_TRACE("(void)");
    LOG_TODO("lx_gui_shutdown: XCloseDisplay (UC3)");
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_LINUX */
