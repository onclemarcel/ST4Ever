/*
 * win_gui.c - Win32 window management backend for gui.h
 *
 * Implements gui_platform_* functions declared in gui_backend.h.
 *
 * Each gui_window_t is backed by a win_wnd_state_t that holds:
 *   - The Win32 HWND
 *   - A Win32 Event used to synchronise window creation with the
 *     calling thread (signaled once the HWND is live)
 *   - Back-pointer to the portable gui_window_s
 *
 * Win32 window class "ST4EverView" is registered once in
 * gui_platform_init() and shared by all ST4Ever windows.
 *
 * Message pump architecture:
 *   gui_platform_window_create() spawns a dedicated thread via
 *   platform_thread_create().  That thread calls CreateWindowEx(),
 *   signals the ready Event, then enters a GetMessage() loop.
 *   The WndProc translates Win32 messages to gui_event_t and
 *   forwards them to the registered gui_event_fn callback.
 *
 * UC3.1: blank window (dark background), full event translation.
 * UC3.2: CoInitialize per thread; WM_PAINT fires GUI_EVT_PAINT only
 *         (view callback owns D2D drawing); gui_platform_get_native_handle
 *         and gui_platform_window_set_title added (R18).
 */

#include "../src/gui_backend.h"
#include "../src/trace.h"

#ifdef ST_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>   /* CoInitializeEx / CoUninitialize */
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------ */

#define ST4EVER_WNDCLASS  "ST4EverView"
#define WIN_READY_TIMEOUT  5000   /* ms to wait for window creation   */

/* ------------------------------------------------------------------
 * Default window dimensions per type (pixels)
 * ------------------------------------------------------------------ */

static void win_default_size(gui_wnd_type_t eType,
                              int           *piW,
                              int           *piH)
{
    switch (eType)
    {
    case GUI_WND_TRACE:    *piW = 900;  *piH = 500; break;
    case GUI_WND_DIR:      *piW = 480;  *piH = 680; break;
    case GUI_WND_EDIT_TXT: *piW = 900;  *piH = 700; break;
    case GUI_WND_EDIT_HEX: *piW = 1000; *piH = 700; break;
    case GUI_WND_MOUNT:    *piW = 480;  *piH = 620; break;
    case GUI_WND_EXEC_MON: *piW = 800;  *piH = 600; break;
    case GUI_WND_EXEC_CPU: *piW = 420;  *piH = 520; break;
    case GUI_WND_EXEC_MEM: *piW = 700;  *piH = 520; break;
    case GUI_WND_EXEC_ASM: *piW = 700;  *piH = 520; break;
    case GUI_WND_EXEC_SCR: *piW = 640;  *piH = 400; break;
    default:               *piW = 600;  *piH = 500; break;
    }
}

/* ------------------------------------------------------------------
 * Platform window state  (one per open gui_window_t)
 * ------------------------------------------------------------------ */

typedef struct
{
    HWND                 hWnd;   /* Win32 window handle               */
    HANDLE               hReady; /* CreateEvent, signaled on ready    */
    struct gui_window_s *ptWnd;  /* back-pointer to portable record   */
} win_wnd_state_t;

/* ------------------------------------------------------------------
 * VKey → gui_key_t translation
 * ------------------------------------------------------------------ */

static gui_key_t win_translate_vkey(UINT uVk)
{
    switch (uVk)
    {
    case VK_UP:     return GUI_KEY_UP;
    case VK_DOWN:   return GUI_KEY_DOWN;
    case VK_LEFT:   return GUI_KEY_LEFT;
    case VK_RIGHT:  return GUI_KEY_RIGHT;
    case VK_HOME:   return GUI_KEY_HOME;
    case VK_END:    return GUI_KEY_END;
    case VK_PRIOR:  return GUI_KEY_PAGE_UP;
    case VK_NEXT:   return GUI_KEY_PAGE_DOWN;
    case VK_RETURN: return GUI_KEY_ENTER;
    case VK_ESCAPE: return GUI_KEY_ESCAPE;
    case VK_SPACE:  return GUI_KEY_SPACE;
    case VK_BACK:   return GUI_KEY_BACKSPACE;
    case VK_DELETE: return GUI_KEY_DELETE;
    case VK_TAB:    return GUI_KEY_TAB;
    case VK_F1:     return GUI_KEY_F1;
    case VK_F2:     return GUI_KEY_F2;
    case VK_F3:     return GUI_KEY_F3;
    case VK_F4:     return GUI_KEY_F4;
    case VK_F5:     return GUI_KEY_F5;
    case VK_F6:     return GUI_KEY_F6;
    case VK_F7:     return GUI_KEY_F7;
    case VK_F8:     return GUI_KEY_F8;
    case VK_F9:     return GUI_KEY_F9;
    case VK_F10:    return GUI_KEY_F10;
    case VK_F11:    return GUI_KEY_F11;
    case VK_F12:    return GUI_KEY_F12;
    default:        return GUI_KEY_NONE;
    }
}

/* ------------------------------------------------------------------
 * WndProc - Win32 window procedure
 * ------------------------------------------------------------------ */

static LRESULT CALLBACK win_wnd_proc(HWND   hWnd,
                                      UINT   uMsg,
                                      WPARAM wParam,
                                      LPARAM lParam)
{
    win_wnd_state_t     *ptState;
    struct gui_window_s *ptWnd;
    gui_event_t          tEvt;

    ptState = (win_wnd_state_t *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    ptWnd   = (ptState != NULL) ? ptState->ptWnd : NULL;

    memset(&tEvt, 0, sizeof(tEvt));

    switch (uMsg)
    {
    /* Store per-window state pointer on window creation */
    case WM_NCCREATE:
    {
        CREATESTRUCT    *ptCs;
        win_wnd_state_t *ptSt;

        ptCs = (CREATESTRUCT *)lParam;
        ptSt = (win_wnd_state_t *)ptCs->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)ptSt);
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    /* Paint: view callback owns all drawing via renderer (UC3.2+).
     * GDI fallback only when no view is attached (testing / debug). */
    case WM_PAINT:
    {
        PAINTSTRUCT tPs;
        HDC         hDC;

        hDC = BeginPaint(hWnd, &tPs);

        if (ptWnd != NULL && ptWnd->tDesc.pfnEvent != NULL)
        {
            /* View draws via renderer_begin/end_draw in callback */
            tEvt.eType = GUI_EVT_PAINT;
            ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                   ptWnd->tDesc.pUserCtx);
        }
        else if (hDC != NULL)
        {
            /* No view attached: dark fallback until renderer exists */
            RECT   tRect;
            HBRUSH hBrush;
            GetClientRect(hWnd, &tRect);
            hBrush = CreateSolidBrush(RGB(24, 24, 36));
            if (hBrush != NULL)
            {
                FillRect(hDC, &tRect, hBrush);
                DeleteObject(hBrush);
            }
        }

        EndPaint(hWnd, &tPs);
        return 0;
    }

    /* Close request: destroy the window */
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    /* Window destroyed: notify view, exit message loop */
    case WM_DESTROY:
        if (ptWnd != NULL)
        {
            ptWnd->bOpen = ST_FALSE;
            if (ptWnd->tDesc.pfnEvent != NULL)
            {
                tEvt.eType = GUI_EVT_CLOSE;
                ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                       ptWnd->tDesc.pUserCtx);
            }
        }
        PostQuitMessage(0);
        return 0;

    /* Window resized */
    case WM_SIZE:
        if (ptWnd != NULL && ptWnd->tDesc.pfnEvent != NULL)
        {
            tEvt.eType                 = GUI_EVT_RESIZE;
            tEvt.uData.tResize.iWidth  = (int)LOWORD(lParam);
            tEvt.uData.tResize.iHeight = (int)HIWORD(lParam);
            ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                   ptWnd->tDesc.pUserCtx);
        }
        break;

    /* Non-printable navigation keys */
    case WM_KEYDOWN:
    {
        gui_key_t eKey;
        eKey = win_translate_vkey((UINT)wParam);
        if (eKey != GUI_KEY_NONE
        &&  ptWnd != NULL && ptWnd->tDesc.pfnEvent != NULL)
        {
            tEvt.eType            = GUI_EVT_KEY_DOWN;
            tEvt.uData.tKey.eKey  = eKey;
            tEvt.uData.tKey.cChar = 0;
            ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                   ptWnd->tDesc.pUserCtx);
        }
        break;
    }

    /* Printable ASCII character (translated from WM_KEYDOWN by Win32) */
    case WM_CHAR:
    {
        char cChar = (char)(wParam & 0x7F);
        if (cChar >= 32
        &&  ptWnd != NULL && ptWnd->tDesc.pfnEvent != NULL)
        {
            tEvt.eType            = GUI_EVT_KEY_DOWN;
            tEvt.uData.tKey.eKey  =
                (gui_key_t)(GUI_KEY_PRINTABLE + (unsigned)cChar);
            tEvt.uData.tKey.cChar = cChar;
            ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                   ptWnd->tDesc.pUserCtx);
        }
        break;
    }

    /* Mouse button down */
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (ptWnd != NULL && ptWnd->tDesc.pfnEvent != NULL)
        {
            tEvt.eType = GUI_EVT_MOUSE_DOWN;
            if (uMsg == WM_LBUTTONDOWN)
            {
                tEvt.uData.tMouse.eBtn = GUI_MOUSE_LEFT;
            }
            else if (uMsg == WM_RBUTTONDOWN)
            {
                tEvt.uData.tMouse.eBtn = GUI_MOUSE_RIGHT;
            }
            else
            {
                tEvt.uData.tMouse.eBtn = GUI_MOUSE_MIDDLE;
            }
            tEvt.uData.tMouse.iX = (int)(short)LOWORD(lParam);
            tEvt.uData.tMouse.iY = (int)(short)HIWORD(lParam);
            ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                   ptWnd->tDesc.pUserCtx);
        }
        break;

    /* Mouse button up */
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        if (ptWnd != NULL && ptWnd->tDesc.pfnEvent != NULL)
        {
            tEvt.eType             = GUI_EVT_MOUSE_UP;
            tEvt.uData.tMouse.eBtn = (uMsg == WM_LBUTTONUP)
                                      ? GUI_MOUSE_LEFT : GUI_MOUSE_RIGHT;
            tEvt.uData.tMouse.iX   = (int)(short)LOWORD(lParam);
            tEvt.uData.tMouse.iY   = (int)(short)HIWORD(lParam);
            ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                   ptWnd->tDesc.pUserCtx);
        }
        break;

    /* Mouse move */
    case WM_MOUSEMOVE:
        if (ptWnd != NULL && ptWnd->tDesc.pfnEvent != NULL)
        {
            tEvt.eType            = GUI_EVT_MOUSE_MOVE;
            tEvt.uData.tMouse.iX  = (int)(short)LOWORD(lParam);
            tEvt.uData.tMouse.iY  = (int)(short)HIWORD(lParam);
            ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                   ptWnd->tDesc.pUserCtx);
        }
        break;

    /* Mouse wheel */
    case WM_MOUSEWHEEL:
        if (ptWnd != NULL && ptWnd->tDesc.pfnEvent != NULL)
        {
            tEvt.eType              = GUI_EVT_SCROLL;
            tEvt.uData.tScroll.iDelta =
                GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            ptWnd->tDesc.pfnEvent((gui_window_t)ptWnd, &tEvt,
                                   ptWnd->tDesc.pUserCtx);
        }
        break;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/* ------------------------------------------------------------------
 * Window thread entry point
 * ------------------------------------------------------------------ */

static void win_wnd_thread(void *pArg)
{
    struct gui_window_s *ptWnd;
    win_wnd_state_t     *ptState;
    const char          *szTitle;
    int                  iW;
    int                  iH;
    MSG                  tMsg;

    /* COM must be initialised per thread for DirectWrite (UC3.2+) */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    ptWnd   = (struct gui_window_s *)pArg;
    ptState = (win_wnd_state_t *)ptWnd->pPlatform;

    szTitle = ptWnd->tDesc.szTitle;
    if (szTitle == NULL || szTitle[0] == '\0')
    {
        szTitle = ST_APP_NAME;
    }

    iW = ptWnd->tDesc.iWidth;
    iH = ptWnd->tDesc.iHeight;
    if (iW <= 0 || iH <= 0)
    {
        win_default_size(ptWnd->tDesc.eType, &iW, &iH);
    }

    ptState->hWnd = CreateWindowExA(
        WS_EX_OVERLAPPEDWINDOW,
        ST4EVER_WNDCLASS,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        iW, iH,
        NULL, NULL,
        GetModuleHandleA(NULL),
        ptState   /* retrieved as lpCreateParams in WM_NCCREATE */
    );

    if (ptState->hWnd == NULL)
    {
        LOG_ERROR("CreateWindowEx failed: %lu", GetLastError());
        SetEvent(ptState->hReady);
        return;
    }

    ptWnd->bOpen = ST_TRUE;
    ShowWindow(ptState->hWnd, SW_SHOWNORMAL);
    UpdateWindow(ptState->hWnd);
    SetEvent(ptState->hReady); /* signal creating thread: HWND is live */

    while (GetMessage(&tMsg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&tMsg);
        DispatchMessage(&tMsg);
    }

    /* bOpen already set to ST_FALSE by WM_DESTROY handler */
    LOG_INFO("window thread exiting: '%s'", szTitle);
    CoUninitialize();
}

/* ------------------------------------------------------------------
 * Platform backend interface implementation
 * ------------------------------------------------------------------ */

st_error_t gui_platform_init(void)
{
    WNDCLASSEXA tWce;
    DWORD       dwErr;

    LOG_TRACE("(void)");

    memset(&tWce, 0, sizeof(tWce));
    tWce.cbSize        = sizeof(tWce);
    tWce.style         = CS_HREDRAW | CS_VREDRAW;
    tWce.lpfnWndProc   = win_wnd_proc;
    tWce.hInstance     = GetModuleHandleA(NULL);
    tWce.hIcon         = LoadIconA(NULL, IDI_APPLICATION);
    tWce.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    tWce.hbrBackground = NULL; /* WM_PAINT handles background */
    tWce.lpszClassName = ST4EVER_WNDCLASS;
    tWce.hIconSm       = LoadIconA(NULL, IDI_APPLICATION);

    if (RegisterClassExA(&tWce) == 0)
    {
        dwErr = GetLastError();
        if (dwErr != ERROR_CLASS_ALREADY_EXISTS)
        {
            LOG_ERROR("RegisterClassExA failed: %lu", dwErr);
            return ST_ERROR;
        }
    }

    LOG_INFO("WNDCLASS '%s' registered", ST4EVER_WNDCLASS);
    return ST_NO_ERROR;
}

st_error_t gui_platform_window_create(struct gui_window_s *ptWnd)
{
    win_wnd_state_t *ptState;
    DWORD            dwResult;
    st_error_t       eResult;

    LOG_TRACE("ptWnd=%p", (void *)ptWnd);

    if (ptWnd == NULL)
    {
        LOG_ERROR("NULL ptWnd");
        return ST_ERROR;
    }

    ptState = (win_wnd_state_t *)malloc(sizeof(win_wnd_state_t));
    if (ptState == NULL)
    {
        LOG_ERROR("malloc failed for win_wnd_state_t");
        return ST_ERROR;
    }
    memset(ptState, 0, sizeof(win_wnd_state_t));
    ptState->ptWnd = ptWnd;

    ptState->hReady = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (ptState->hReady == NULL)
    {
        LOG_ERROR("CreateEvent failed: %lu", GetLastError());
        free(ptState);
        return ST_ERROR;
    }

    ptWnd->pPlatform = ptState;

    eResult = platform_thread_create(&ptWnd->ptThread,
                                      win_wnd_thread, ptWnd);
    if (eResult != ST_NO_ERROR)
    {
        CloseHandle(ptState->hReady);
        free(ptState);
        ptWnd->pPlatform = NULL;
        LOG_ERROR("platform_thread_create failed");
        return ST_ERROR;
    }

    /* Block until the window thread signals window is live */
    dwResult = WaitForSingleObject(ptState->hReady, WIN_READY_TIMEOUT);
    CloseHandle(ptState->hReady);
    ptState->hReady = NULL;

    if (dwResult != WAIT_OBJECT_0)
    {
        LOG_ERROR("WaitForSingleObject: result=%lu err=%lu",
                  dwResult, GetLastError());
        return ST_ERROR;
    }

    if (ptState->hWnd == NULL)
    {
        LOG_ERROR("window thread failed to create HWND");
        return ST_ERROR;
    }

    LOG_INFO("platform window created: HWND=%p", (void *)ptState->hWnd);
    return ST_NO_ERROR;
}

st_error_t gui_platform_window_close(struct gui_window_s *ptWnd)
{
    win_wnd_state_t *ptState;

    LOG_TRACE("ptWnd=%p", (void *)ptWnd);

    if (ptWnd == NULL)
    {
        LOG_ERROR("NULL ptWnd");
        return ST_ERROR;
    }

    ptState = (win_wnd_state_t *)ptWnd->pPlatform;
    if (ptState == NULL || ptState->hWnd == NULL)
    {
        return ST_NO_ERROR;
    }

    PostMessageA(ptState->hWnd, WM_CLOSE, 0, 0);
    return ST_NO_ERROR;
}

st_error_t gui_platform_window_invalidate(struct gui_window_s *ptWnd)
{
    win_wnd_state_t *ptState;

    LOG_TRACE("ptWnd=%p", (void *)ptWnd);

    if (ptWnd == NULL)
    {
        LOG_ERROR("NULL ptWnd");
        return ST_ERROR;
    }

    ptState = (win_wnd_state_t *)ptWnd->pPlatform;
    if (ptState != NULL && ptState->hWnd != NULL)
    {
        InvalidateRect(ptState->hWnd, NULL, FALSE);
    }

    return ST_NO_ERROR;
}

st_error_t gui_platform_window_get_size(struct gui_window_s *ptWnd,
                                         int                 *piWidth,
                                         int                 *piHeight)
{
    win_wnd_state_t *ptState;
    RECT             tRect;

    LOG_TRACE("ptWnd=%p", (void *)ptWnd);

    if (ptWnd == NULL || piWidth == NULL || piHeight == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    *piWidth  = 0;
    *piHeight = 0;

    ptState = (win_wnd_state_t *)ptWnd->pPlatform;
    if (ptState == NULL || ptState->hWnd == NULL)
    {
        return ST_NO_ERROR;
    }

    if (GetClientRect(ptState->hWnd, &tRect) == 0)
    {
        LOG_ERROR("GetClientRect failed: %lu", GetLastError());
        return ST_ERROR;
    }

    *piWidth  = (int)(tRect.right  - tRect.left);
    *piHeight = (int)(tRect.bottom - tRect.top);
    return ST_NO_ERROR;
}

void *gui_platform_get_native_handle(struct gui_window_s *ptWnd)
{
    win_wnd_state_t *ptState;

    LOG_TRACE("ptWnd=%p", (void *)ptWnd);

    if (ptWnd == NULL)
    {
        return NULL;
    }

    ptState = (win_wnd_state_t *)ptWnd->pPlatform;
    if (ptState == NULL)
    {
        return NULL;
    }

    return (void *)ptState->hWnd;
}

st_error_t gui_platform_window_set_title(struct gui_window_s *ptWnd,
                                          const char          *szTitle)
{
    win_wnd_state_t *ptState;

    LOG_TRACE("ptWnd=%p szTitle='%s'",
              (void *)ptWnd, szTitle ? szTitle : "(null)");

    if (ptWnd == NULL || szTitle == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    ptState = (win_wnd_state_t *)ptWnd->pPlatform;
    if (ptState == NULL || ptState->hWnd == NULL)
    {
        return ST_NO_ERROR;
    }

    SetWindowTextA(ptState->hWnd, szTitle);
    return ST_NO_ERROR;
}

st_error_t gui_platform_shutdown(void)
{
    LOG_TRACE("(void)");
    UnregisterClassA(ST4EVER_WNDCLASS, GetModuleHandleA(NULL));
    LOG_INFO("WNDCLASS '%s' unregistered", ST4EVER_WNDCLASS);
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_WINDOWS */
