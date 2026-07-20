/*
 * gui.h - ST4Ever portable GUI window and event abstraction
 *
 * Provides a platform-independent interface for creating and managing
 * GUI windows.  Each view opened by a user command (dir, edit, mount,
 * execute...) is a gui_window_t running its message/event loop in a
 * dedicated thread.
 *
 * Platform backends:
 *   win/win_gui.c  - Win32: CreateWindowEx, WndProc, GetMessage pump
 *   linux/lx_gui.c - X11:   XCreateWindow, XNextEvent loop
 *
 * Threading model (R4):
 *   gui_open_window() spawns a platform_thread_create() thread that
 *   owns the window and runs its event loop.  The console thread
 *   communicates with views via msg_queue_t (defined below).
 *   Views post events back via their registered gui_event_fn callback,
 *   which is called from the view thread.
 *
 * UC3.1: gui_open_window(), message queues and window lifecycle implemented.
 * TODO(UC3.2): renderer integration (D2D).
 * TODO(UC3.3): dir view event handling.
 */

#ifndef GUI_H
#define GUI_H

#include "common.h"

#define GUI_MAX_WINDOWS  16

/* ------------------------------------------------------------------
 * Opaque window handle
 * ------------------------------------------------------------------ */

typedef struct gui_window_s *gui_window_t;   /* NULL = invalid        */

/* ------------------------------------------------------------------
 * GUI context
 * ------------------------------------------------------------------ */

typedef struct gui_context_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */
    
    struct gui_window_s *aptWnd[GUI_MAX_WINDOWS]; /* Open windows list  */
    size_t               uiWndCount;        /* Number of open windows   */ 
    st_mutex_t          *ptMutex;           /* Protects windows list    */
    st_error_t           eGUIPtfInit;       /* Success of platform-init */
    st_bool_t            bInit;             /* Trace of initialization  */

    // Test-related context variables
    size_t     uiLastOpenWindow;       /* index of the last open window */
    st_bool_t  bActiveSpies;     /* Activate the platform-related spies */
} gui_context_t;

/* ------------------------------------------------------------------
 * Window types  (determine default sizing and drawing mode)
 * ------------------------------------------------------------------ */

typedef enum gui_wnd_type_e
{
    GUI_WND_TRACE    = 0,  /* Trace / log console               */
    GUI_WND_DIR      = 1,  /* Directory tree view               */
    GUI_WND_EDIT_TXT = 2,  /* Text / source editor              */
    GUI_WND_EDIT_HEX = 3,  /* Hex + ASCII + disasm editor       */
    GUI_WND_MOUNT    = 4,  /* Floppy emulation tree view        */
    GUI_WND_EXEC_MON = 5,  /* Execution monitor                 */
    GUI_WND_EXEC_CPU = 6,  /* CPU 68000 registers               */
    GUI_WND_EXEC_MEM = 7,  /* Memory view                       */
    GUI_WND_EXEC_ASM = 8,  /* Disassembly view                  */
    GUI_WND_EXEC_SCR = 9   /* Atari ST screen emulation         */
} gui_wnd_type_t;

/* ------------------------------------------------------------------
 * Input events
 * ------------------------------------------------------------------ */

/* Modifier key bitmask flags (combinable, passed in tKey.uiMods) */
#define GUI_MOD_CTRL  0x01u
#define GUI_MOD_SHIFT 0x02u
#define GUI_MOD_ALT   0x04u

/* Key codes (platform-independent subset) */
typedef enum gui_key_e
{
    GUI_KEY_NONE    = 0,
    GUI_KEY_UP,
    GUI_KEY_DOWN,
    GUI_KEY_LEFT,
    GUI_KEY_RIGHT,
    GUI_KEY_HOME,
    GUI_KEY_END,
    GUI_KEY_PAGE_UP,
    GUI_KEY_PAGE_DOWN,
    GUI_KEY_ENTER,
    GUI_KEY_ESCAPE,
    GUI_KEY_SPACE,
    GUI_KEY_BACKSPACE,
    GUI_KEY_DELETE,
    GUI_KEY_TAB,
    GUI_KEY_F1,  GUI_KEY_F2,  GUI_KEY_F3,  GUI_KEY_F4,
    GUI_KEY_F5,  GUI_KEY_F6,  GUI_KEY_F7,  GUI_KEY_F8,
    GUI_KEY_F9,  GUI_KEY_F10, GUI_KEY_F11, GUI_KEY_F12,
    GUI_KEY_PRINTABLE = 0x100  /* Base for printable ASCII (+ char) */
} gui_key_t;

/* Mouse button identifiers */
typedef enum gui_mouse_btn_e
{
    GUI_MOUSE_NONE   = 0,
    GUI_MOUSE_LEFT   = 1,
    GUI_MOUSE_RIGHT  = 2,
    GUI_MOUSE_MIDDLE = 3
} gui_mouse_btn_t;

/* Event types */
typedef enum gui_evt_type_e
{
    GUI_EVT_NONE       = 0,
    GUI_EVT_KEY_DOWN,        /* Key pressed (tKey, cChar)            */
    GUI_EVT_MOUSE_DOWN,      /* Button pressed (eButton, iX, iY)     */
    GUI_EVT_MOUSE_UP,        /* Button released                      */
    GUI_EVT_MOUSE_MOVE,      /* Mouse moved (iX, iY)                 */
    GUI_EVT_SCROLL,          /* Wheel scroll (iDelta, +up/-down)     */
    GUI_EVT_RESIZE,          /* Window resized (iWidth, iHeight)     */
    GUI_EVT_CLOSE,           /* User closed the window               */
    GUI_EVT_PAINT,           /* Window needs repainting              */
    GUI_EVT_CUSTOM           /* Application-defined (uiId, pData)    */
} gui_evt_type_t;

/* Unified event structure */
typedef struct gui_event_s
{
    gui_evt_type_t  eType;

    union
    {
        struct sKey { gui_key_t eKey; char cChar;
                      st_u8_t uiMods; } tKey;
        struct sMouse { gui_mouse_btn_t eBtn; int iX; int iY; } tMouse;
        struct sScroll { int iDelta; } tScroll;
        struct sResize { int iWidth; int iHeight; } tResize;
        struct sCustom { unsigned int uiId; void *pData; } tCustom;
    } uData;

} gui_event_t;

/* ------------------------------------------------------------------
 * Window descriptor  (passed to gui_open_window)
 * ------------------------------------------------------------------ */

/* Callback invoked from the view thread for each GUI event */
typedef void (*gui_event_fn)(gui_window_t   hWnd,
                              gui_event_t   *ptEvent,
                              void          *pUserCtx);

typedef struct gui_wnd_desc_s
{
    const char    *szTitle;    /* Window title bar text             */
    gui_wnd_type_t eType;      /* Window type (default sizing)      */
    int            iWidth;     /* Initial width  in pixels (0=auto) */
    int            iHeight;    /* Initial height in pixels (0=auto) */
    gui_event_fn   pfnEvent;   /* Event callback (may be NULL)      */
    void          *pUserCtx;   /* Forwarded as-is to pfnEvent       */
} gui_wnd_desc_t;

/* ------------------------------------------------------------------
 * Message queue  (inter-thread communication, see R4)
 *
 * Capacity is the maximum number of events that can be queued before
 * gui_msg_post() blocks the caller.
 * ------------------------------------------------------------------ */

typedef struct gui_msg_queue_s *gui_msg_queue_t;   /* NULL = invalid */

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * gui_init() - Initialise the GUI subsystem.
 *
 * Must be called once from the main thread before any other gui_*
 * function.  On Windows this registers the WNDCLASS used for all
 * ST4Ever windows.
 *
 * Returns:
 *   Value of the global gui_context_t structure pointer on success.
 *   ST_ERROR    on platform initialisation failure (logged).
 */
st_u64_t gui_init(void);

/*
 * gui_open_window() - Create and show a window in a new thread.
 *
 * Spawns a platform_thread_create() thread that owns the window and
 * runs its event loop.  Returns immediately; the window appears
 * asynchronously.
 *
 * Parameters:
 *   ptDesc [in]  : Window descriptor (copied internally).
 *   phWnd  [out] : Receives the new window handle.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on thread or platform failure.
 */
st_error_t gui_open_window(const gui_wnd_desc_t *ptDesc,
                            gui_window_t         *phWnd);

/*
 * gui_close_window() - Request a window to close.
 *
 * Posts a GUI_EVT_CLOSE to the window's event loop and waits for
 * the thread to exit.
 *
 * Parameters:
 *   hWnd [in] : Window to close.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hWnd is NULL.
 */
st_error_t gui_close_window(gui_window_t hWnd);

/*
 * gui_is_window_open() - Query whether a window is still open.
 *
 * A window can close itself asynchronously (ESC / native close button
 * -> gui_request_close() -> WM_CLOSE/WM_DESTROY) without the owner
 * ever calling gui_close_window(). This accessor lets a view's owner
 * (line.c, exec.c, trace.c...) detect that self-close and finish the
 * teardown its own <module>_close() normally performs (freeing the
 * view struct, nulling the owner's pointer) - see dir_close(),
 * mount_view_close(), edit_txt_close(), edit_hex_close(),
 * exec_close(), trace_gui_close(). Same factoring precedent as
 * gui_handle_resize_event(): one shared check instead of six
 * near-identical copies.
 *
 * Parameters:
 *   hWnd   [in]  : Window to query.
 *   pbOpen [out] : ST_TRUE if still open, ST_FALSE if self-closed.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hWnd or pbOpen is NULL.
 */
st_error_t gui_is_window_open(gui_window_t hWnd, st_bool_t *pbOpen);

/*
 * gui_invalidate() - Request a repaint of the window.
 *
 * Posts a GUI_EVT_PAINT to the window's event loop.
 *
 * Parameters:
 *   hWnd [in] : Window to repaint.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hWnd is NULL.
 */
st_error_t gui_invalidate(gui_window_t hWnd);

/*
 * gui_handle_resize_event() - Generic GUI_EVT_RESIZE handling shared by
 * every view (dir, mount, edit_hex, edit_txt, exec_*).
 *
 * Updates the caller's window size fields from ptEvent, resizes the
 * renderer if one already exists, and invalidates the window so the
 * new geometry is repainted.  Factored out because every view's
 * GUI_EVT_RESIZE case performed these same three steps verbatim -
 * a missing gui_invalidate() here in one view (dir.c) went unnoticed
 * until a resize+pagination test caught it.
 *
 * Parameters:
 *   hWnd       [in]  : Window that received the resize (for invalidate).
 *   ptEvent    [in]  : The GUI_EVT_RESIZE event (uData.tResize used).
 *   piWndWidth [out] : View's iWndWidth field to update.
 *   piWndHeight[out] : View's iWndHeight field to update.
 *   hRenderer  [in]  : View's renderer_t, or NULL if not created yet.
 *                      Typed void* here to avoid a gui.h/renderer.h
 *                      circular include (renderer.h already includes
 *                      gui.h) - cast internally to renderer_t.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hWnd, ptEvent, piWndWidth or piWndHeight is NULL.
 */
st_error_t gui_handle_resize_event(gui_window_t       hWnd,
                                    const gui_event_t *ptEvent,
                                    int               *piWndWidth,
                                    int               *piWndHeight,
                                    void              *hRenderer);

/*
 * gui_get_size() - Query the current client area size.
 *
 * Parameters:
 *   hWnd     [in]  : Target window.
 *   piWidth  [out] : Client width in pixels.
 *   piHeight [out] : Client height in pixels.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL.
 */
st_error_t gui_get_size(gui_window_t  hWnd,
                         int          *piWidth,
                         int          *piHeight);

/*
 * gui_shutdown() - Close all open windows and release GUI resources.
 *
 * Returns:
 *   ST_NO_ERROR.
 */
st_error_t gui_shutdown(void);

/*
 * gui_request_close() - Ask a window to close without blocking (UC4.1/P9).
 *
 * Posts a close request to the window's event loop and returns immediately.
 * Unlike gui_close_window(), this does NOT join the window thread.
 * Intended for use from the window thread itself (e.g. ESC key handler)
 * or when the caller does not want to block.
 *
 * The caller is responsible for eventually calling gui_close_window() (or
 * dir_close() etc.) to join the thread and free resources.
 * gui_close_window() handles the case where the window is already closed.
 *
 * Parameters:
 *   hWnd [in] : Window to close.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hWnd is NULL.
 */
st_error_t gui_request_close(gui_window_t hWnd);

/*
 * gui_set_title() - Update the window title bar text (R18).
 *
 * Delegates to gui_platform_window_set_title().  Intended for views
 * that implement the dynamic title convention:
 *   "ST4Ever - <Type>: <context>"
 *
 * Parameters:
 *   hWnd    [in] : Target window (must be open).
 *   szTitle [in] : New null-terminated title string.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hWnd or szTitle is NULL.
 */
st_error_t gui_set_title(gui_window_t hWnd, const char *szTitle);

/* ------------------------------------------------------------------
 * Message queue API
 * ------------------------------------------------------------------ */

/*
 * gui_msg_create() - Create a bounded message queue.
 *
 * Parameters:
 *   pphQueue    [out] : Receives the new queue handle.
 *   uiCapacity  [in]  : Maximum number of queued events.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on memory or mutex failure.
 */
st_error_t gui_msg_create(gui_msg_queue_t *pphQueue,
                           size_t           uiCapacity);

/*
 * gui_msg_post() - Append an event to the queue (blocks if full).
 *
 * Parameters:
 *   hQueue   [in] : Target queue.
 *   ptEvent  [in] : Event to post (copied by value).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hQueue is NULL.
 */
st_error_t gui_msg_post(gui_msg_queue_t    hQueue,
                         const gui_event_t *ptEvent);

/*
 * gui_msg_get() - Remove the next event from the queue.
 *
 * Parameters:
 *   hQueue   [in]  : Source queue.
 *   ptEvent  [out] : Receives the dequeued event.
 *   bBlock   [in]  : ST_TRUE to block until an event is available.
 *
 * Returns:
 *   ST_NO_ERROR if an event was returned.
 *   ST_ERROR    if the queue is empty (non-blocking) or on error.
 */
st_error_t gui_msg_get(gui_msg_queue_t  hQueue,
                        gui_event_t     *ptEvent,
                        st_bool_t        bBlock);

/*
 * gui_msg_destroy() - Release queue resources.
 *
 * Sets *pphQueue to NULL on success.
 *
 * Parameters:
 *   pphQueue [in/out] : Queue to destroy.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pphQueue or *pphQueue is NULL.
 */
st_error_t gui_msg_destroy(gui_msg_queue_t *pphQueue);

/* ------------------------------------------------------------------
 * Clipboard
 * ------------------------------------------------------------------ */

/*
 * gui_clipboard_set_text() - Copy a NUL-terminated string to the
 * system clipboard.
 *
 * Parameters:
 *   szText [in] : Text to copy (must not be NULL).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on platform failure or NULL parameter.
 */
st_error_t gui_clipboard_set_text(const char *szText);

/*
 * gui_clipboard_get_text() - Read text from the system clipboard.
 *
 * szBuf is always NUL-terminated on return (empty string on failure).
 *
 * Parameters:
 *   szBuf  [out] : Destination buffer.
 *   uiMax  [in]  : Buffer capacity in bytes (including NUL).
 *
 * Returns:
 *   ST_NO_ERROR if text was read.
 *   ST_ERROR    on platform failure, empty clipboard, or bad params.
 */
st_error_t gui_clipboard_get_text(char *szBuf, size_t uiMax);

/*
 * gui_find_window_by_type() - Find the first open window of a given type.
 *
 * Searches the global window registry under mutex for the first open
 * gui_window_t whose type matches eType.  Used by the mount view to let
 * the dir view locate it for drag-and-drop (UC18.2).
 *
 * Parameters:
 *   eType [in]  : Window type to locate (e.g. GUI_WND_MOUNT).
 *   phWnd [out] : Receives the first matching handle, or NULL if none.
 *
 * Returns:
 *   ST_NO_ERROR always (phWnd is NULL when the type is not found).
 *   ST_ERROR    if phWnd is NULL.
 */
st_error_t gui_find_window_by_type(gui_wnd_type_t  eType,
                                    gui_window_t   *phWnd);

st_bool_t gui_is_initialized();

#endif /* GUI_H */
