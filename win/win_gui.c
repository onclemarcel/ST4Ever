/*
 * win_gui.c - Win32 window management backend for gui.h
 *
 * Each gui_window_t is backed by a win_wnd_state_t that holds:
 *   - The Win32 HWND
 *   - The dedicated thread running the Win32 message pump
 *   - The event callback and user context from gui_wnd_desc_t
 *
 * Win32 window class "ST4EverView" is registered once in gui_init().
 * All ST4Ever windows share this class; the window type drives
 * default sizing and the WndProc event routing.
 *
 * Message pump architecture:
 *   The thread spawned by gui_open_window() calls CreateWindowEx(),
 *   then enters a GetMessage() / TranslateMessage() / DispatchMessage()
 *   loop.  WM_PAINT, WM_KEYDOWN, WM_LBUTTONDOWN etc. are translated
 *   into gui_event_t and forwarded to the registered gui_event_fn
 *   callback.
 *
 * TODO(UC3): Implement win_wnd_create(), win_wnd_proc(),
 *            message pump thread, WM_PAINT → renderer integration.
 */

#include "../src/common.h"
#include "../src/trace.h"
#include "../src/gui.h"

#ifdef ST_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static const char *WIN_CLASS_NAME = "ST4EverView";
static BOOL g_win_gui_bClassRegistered = FALSE;

/* Opaque state behind gui_window_t */
typedef struct
{
    HWND            hWnd;
    HANDLE          hThread;
    gui_wnd_desc_t  tDesc;     /* Copy of the descriptor */
} win_wnd_state_t;

st_error_t win_gui_init(void)
{
    WNDCLASSEXA wc;

    LOG_TRACE("(void)");
    if (g_win_gui_bClassRegistered) { return ST_NO_ERROR; }

    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = DefWindowProcA;  /* TODO(UC3): win_wnd_proc */
    wc.hInstance     = GetModuleHandleA(NULL);
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WIN_CLASS_NAME;

    if (!RegisterClassExA(&wc))
    {
        LOG_ERROR("RegisterClassExA failed: %lu", GetLastError());
        return ST_ERROR;
    }

    g_win_gui_bClassRegistered = TRUE;
    LOG_INFO("Win32 window class '%s' registered", WIN_CLASS_NAME);
    LOG_TODO("win_gui_init: WndProc, D2D integration (UC3)");
    return ST_NO_ERROR;
}

st_error_t win_gui_open_window(const gui_wnd_desc_t *ptDesc,
                                 gui_window_t         *phWnd)
{
    LOG_TRACE("title='%s' type=%d",
              ptDesc ? ptDesc->szTitle : "(null)",
              ptDesc ? (int)ptDesc->eType : -1);
    ST_UNUSED(ptDesc);
    if (phWnd != NULL) { *phWnd = NULL; }
    LOG_TODO("win_gui_open_window: CreateWindowEx + thread pump (UC3)");
    return ST_NO_ERROR;
}

st_error_t win_gui_close_window(gui_window_t hWnd)
{
    LOG_TRACE("hWnd=%p", (void *)hWnd);
    ST_UNUSED(hWnd);
    LOG_TODO("win_gui_close_window: PostMessage WM_CLOSE (UC3)");
    return ST_NO_ERROR;
}

st_error_t win_gui_shutdown(void)
{
    LOG_TRACE("(void)");
    LOG_TODO("win_gui_shutdown: close all open windows (UC3)");
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_WINDOWS */
