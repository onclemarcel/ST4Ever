/*
 * gui_backend.h - ST4Ever GUI subsystem internal platform interface
 *
 * Private header: included ONLY by gui.c and platform backends
 * (win/win_gui.c, linux/lx_gui.c).  Not part of the public API.
 *
 * Provides:
 *   - Full definition of struct gui_window_s (opaque in gui.h).
 *   - Declaration of gui_platform_* functions implemented by each
 *     backend.
 *
 * UC3.1: Win32 backend implemented.  Linux backend is stubbed.
 * TODO(UC3-Linux): Implement gui_platform_* in linux/lx_gui.c.
 */

#ifndef GUI_BACKEND_H
#define GUI_BACKEND_H

#include "gui.h"
#include "common.h"
#include "renderer.h"

/* Default delay (ms) win_evt_send_*() sleeps after each injected UI
 * event - long enough for the target window thread to process the
 * message before the next one is sent. Per-window, tunable via
 * gui_window_s.uiEventDelayMs to speed up or slow down a test. */
#define GUI_DEFAULT_EVENT_DELAY_MS 10u

/* ------------------------------------------------------------------
 * Full definition of gui_window_s
 *
 * gui.h forward-declares this struct as opaque (gui_window_t).
 * The full layout is only needed by gui.c and the backends.
 * ------------------------------------------------------------------ */

struct gui_window_s
{
    gui_wnd_desc_t  tDesc;      /* verbatim copy of the caller's desc  */
    st_thread_t    *ptThread;   /* window / event-loop thread          */
    void           *pPlatform;  /* backend state (win_wnd_state_t etc) */
    st_bool_t       bOpen;      /* ST_FALSE once the window has closed */
    renderer_t      ptRenderer; /* Associated renderer */
    
    /* Test-related context variable */
    st_bool_t     bActiveSpies;   /* Are spies active for this window  */
    st_u32_t      uiEventDelayMs; /* win_evt_send_*() post-event delay */
};

/* ------------------------------------------------------------------
 * Platform backend interface
 *
 * Implemented in win/win_gui.c (Win32) and linux/lx_gui.c (X11).
 * Called exclusively from gui.c.
 * ------------------------------------------------------------------ */

/*
 * gui_platform_init() - Initialise the platform window system.
 *
 * Called once by gui_init().  On Win32: registers "ST4EverView"
 * WNDCLASS.  On X11: opens the Display connection.
 *
 * Returns: ST_NO_ERROR, ST_ERROR on platform failure.
 */
st_error_t gui_platform_init(void);

/*
 * gui_platform_window_create() - Create a platform window and thread.
 *
 * Must set ptWnd->pPlatform (platform state) and ptWnd->ptThread.
 * Returns only after the window is visible (internal synchronisation
 * via a Win32 Event / X11 semaphore).
 *
 * Parameters:
 *   ptWnd [in/out] : Pre-allocated record with tDesc already set.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on failure.
 */
st_error_t gui_platform_window_create(struct gui_window_s *ptWnd);

/*
 * gui_platform_window_close() - Ask the window thread to exit.
 *
 * Posts a close request (Win32: PostMessage(WM_CLOSE)) and returns
 * immediately.  gui.c joins the thread after this call.
 *
 * Parameters:
 *   ptWnd [in] : Open window to close.
 *
 * Returns: ST_NO_ERROR, ST_ERROR if ptWnd is NULL.
 */
st_error_t gui_platform_window_close(struct gui_window_s *ptWnd);

/*
 * gui_platform_window_invalidate() - Request a window repaint.
 *
 * Parameters:
 *   ptWnd [in] : Window to repaint.
 *
 * Returns: ST_NO_ERROR, ST_ERROR if ptWnd is NULL.
 */
st_error_t gui_platform_window_invalidate(struct gui_window_s *ptWnd);

/*
 * gui_platform_window_get_size() - Query client area dimensions.
 *
 * Parameters:
 *   ptWnd    [in]  : Target window.
 *   piWidth  [out] : Client width in pixels.
 *   piHeight [out] : Client height in pixels.
 *
 * Returns: ST_NO_ERROR, ST_ERROR if any parameter is NULL.
 */
st_error_t gui_platform_window_get_size(struct gui_window_s *ptWnd,
                                         int                 *piWidth,
                                         int                 *piHeight);

/*
 * gui_platform_shutdown() - Release platform GUI resources.
 *
 * Called from gui_shutdown() after all windows are closed.
 * On Win32: UnregisterClass.  On X11: XCloseDisplay.
 *
 * Returns: ST_NO_ERROR.
 */
st_error_t gui_platform_shutdown(void);

/*
 * gui_platform_get_native_handle() - Return the OS window handle.
 *
 * Returns the platform-specific window handle as a void*:
 *   Win32 : HWND   (cast to HWND in win-specific code)
 *   X11   : Window (cast to Window/unsigned long in lx-specific code)
 *
 * Used by renderer_platform_create() to obtain the HWND/XID needed
 * to create a D2D HwndRenderTarget or X11 Drawable.
 *
 * Parameters:
 *   ptWnd [in] : Open window record.
 *
 * Returns: native handle, or NULL if ptWnd is NULL / window not open.
 */
void *gui_platform_get_native_handle(struct gui_window_s *ptWnd);

/*
 * gui_platform_window_set_title() - Update the window title bar text.
 *
 * Called by views to implement R18 (dynamic title per context).
 * Format convention: "ST4Ever - <Type>: <context>"
 * e.g. "ST4Ever - Dir: C:\demos\"
 *
 * Parameters:
 *   ptWnd   [in] : Target window.
 *   szTitle [in] : New null-terminated title string.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if any parameter is NULL.
 */
st_error_t gui_platform_window_set_title(struct gui_window_s *ptWnd,
                                          const char          *szTitle);

#endif /* GUI_BACKEND_H */
