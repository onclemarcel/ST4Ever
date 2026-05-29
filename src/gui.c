/*
 * gui.c - ST4Ever GUI subsystem stub
 *
 * Portable logic only.  Platform backends:
 *   win/win_gui.c  - Win32 window creation and message pump
 *   linux/lx_gui.c - X11 window creation and event loop
 *
 * TODO(UC3): Implement gui_init, gui_open_window, message queues.
 */

#include "gui.h"
#include "trace.h"
#include <stdlib.h>
#include <string.h>

st_error_t gui_init(void)
{
    LOG_TRACE("(void)");
    LOG_TODO("gui_init: Win32/X11 initialisation not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t gui_open_window(const gui_wnd_desc_t *ptDesc,
                             gui_window_t         *phWnd)
{
    LOG_TRACE("ptDesc=%p phWnd=%p", (void *)ptDesc, (void *)phWnd);
    ST_UNUSED(ptDesc);
    if (phWnd != NULL) { *phWnd = NULL; }
    LOG_TODO("gui_open_window: not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t gui_close_window(gui_window_t hWnd)
{
    LOG_TRACE("hWnd=%p", (void *)hWnd);
    ST_UNUSED(hWnd);
    LOG_TODO("gui_close_window: not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t gui_invalidate(gui_window_t hWnd)
{
    LOG_TRACE("hWnd=%p", (void *)hWnd);
    ST_UNUSED(hWnd);
    LOG_TODO("gui_invalidate: not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t gui_get_size(gui_window_t hWnd, int *piWidth, int *piHeight)
{
    LOG_TRACE("hWnd=%p", (void *)hWnd);
    ST_UNUSED(hWnd);
    if (piWidth  != NULL) { *piWidth  = 0; }
    if (piHeight != NULL) { *piHeight = 0; }
    LOG_TODO("gui_get_size: not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t gui_shutdown(void)
{
    LOG_TRACE("(void)");
    LOG_TODO("gui_shutdown: not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t gui_msg_create(gui_msg_queue_t *pphQueue, size_t uiCapacity)
{
    LOG_TRACE("uiCapacity=%zu", uiCapacity);
    ST_UNUSED(uiCapacity);
    if (pphQueue != NULL) { *pphQueue = NULL; }
    LOG_TODO("gui_msg_create: not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t gui_msg_post(gui_msg_queue_t    hQueue,
                          const gui_event_t *ptEvent)
{
    LOG_TRACE("hQueue=%p", (void *)hQueue);
    ST_UNUSED(hQueue);
    ST_UNUSED(ptEvent);
    LOG_TODO("gui_msg_post: not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t gui_msg_get(gui_msg_queue_t hQueue,
                         gui_event_t    *ptEvent,
                         st_bool_t       bBlock)
{
    LOG_TRACE("hQueue=%p bBlock=%d", (void *)hQueue, (int)bBlock);
    ST_UNUSED(hQueue);
    ST_UNUSED(bBlock);
    if (ptEvent != NULL) { memset(ptEvent, 0, sizeof(gui_event_t)); }
    LOG_TODO("gui_msg_get: not yet implemented (UC3)");
    return ST_ERROR; /* No event available */
}

st_error_t gui_msg_destroy(gui_msg_queue_t *pphQueue)
{
    LOG_TRACE("pphQueue=%p", (void *)pphQueue);
    if (pphQueue != NULL) { *pphQueue = NULL; }
    LOG_TODO("gui_msg_destroy: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
