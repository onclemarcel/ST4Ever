/*
 * win.h - Windows-specific functions
 *
 * This include file declares the context structure of the win_xxx.c
 * files
 * 
 */

#ifndef WIN_H
#define WIN_H

#include "../src/common.h"

#ifdef ST_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <d2d1.h>
#include <dwrite.h>
#endif /* ST_PLATFORM_WINDOWS */

/* ------------------------------------------------------------------
 * Windows Console types (stdin)
 * ------------------------------------------------------------------ */
typedef enum win_stdin_type_e
{
    WIN_STDIN_UNKNOWN = 0,
    WIN_STDIN_PIPE,     /* mintty / MSYS2 — pipe, VT100 byte stream   */
    WIN_STDIN_CONSOLE   /* cmd.exe — Win32 console, VK event stream   */
} win_stdin_type_t;

/* ------------------------------------------------------------------
 * Windows Console Context - types & enums are 'translated' for ST4Ever
 * ------------------------------------------------------------------ */
typedef struct win_console_context_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */
    
    /* Windows Console context */
    win_stdin_type_t eStdinType;      /* Type of stdin: mintty or cmd */
    void*            hStdin;          /* Pointer to stdin   */
    st_u32_t         dwOrigConMode;   /* cmd.exe path only  */

 } win_console_context_t;

/* ------------------------------------------------------------------
 * Win D2D Spy Test Context
 * ------------------------------------------------------------------ */
#define WIN_D2D_SPY_MAX_ENTRIES 512
#define WIN_D2D_SPY_TEXT_LEN    128

typedef struct win_D2D_spy_draw_text_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */

    wchar_t    wcText[WIN_D2D_SPY_TEXT_LEN];
    st_u32_t   uiLen;
    renderer_font_id_t eFontId;
    renderer_rect_t    tRect;
    renderer_align_t   eAlign;
    renderer_color_t   tColor;
    renderer_text_clip_t eClip;
    renderer_measuring_t eMeasure;
    
} win_D2D_spy_draw_text_t;

typedef struct win_D2D_spy_clear_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */

    renderer_color_t   tColor;             /* Background Color          */
    
} win_D2D_spy_clear_t;

typedef struct win_D2D_spy_fill_rectangle_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */

    renderer_rect_t    tRect;
    renderer_color_t   tColor;
    
} win_D2D_spy_fill_rectangle_t;

typedef struct win_D2D_spy_draw_rectangle_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */

    renderer_rect_t    tRect;
    renderer_color_t   tColor;
    float              fStroke;
    
} win_D2D_spy_draw_rectangle_t;

typedef struct win_D2D_spy_draw_line_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */

    float              fX1;
    float              fY1;
    float              fX2;
    float              fY2;
    renderer_color_t   tColor;
    float              fStroke;
    
} win_D2D_spy_draw_line_t;

/* ------------------------------------------------------------------
 * Win D2D context
 * ------------------------------------------------------------------ */
typedef struct win_d2d_ctx_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */

    ID2D1Factory            *pFactory;
    ID2D1HwndRenderTarget   *pRT;
    IDWriteFactory          *pDWFactory;
    IDWriteTextFormat       *pFmtMono;
    IDWriteTextFormat       *pFmtUI;
    ID2D1SolidColorBrush    *pBrush;
    D2D1_COLOR_F             tColor;
    float                    fMonoCellW;
    float                    fMonoCellH;

    /* Test-related D2D spies */
    void            *pD2DSpies[WIN_D2D_SPY_MAX_ENTRIES];
    int              uiSpiesCount;

} win_d2d_ctx_t;

 /* ------------------------------------------------------------------
 * Platform window state  (one per open gui_window_t)
 * ------------------------------------------------------------------ */
typedef struct win_wnd_state_s
{
    HWND                 hWnd;          /* Win32 window handle         */
    HANDLE               hReady;    /* CreateEvent, signaled on ready  */
    struct gui_window_s *ptWnd;     /* back-pointer to portable record */
    HWND                 hPrevFgWnd; /* foreground window before open  */
} win_wnd_state_t;

/* ------------------------------------------------------------------
 * Win Console API
 * ------------------------------------------------------------------ */
st_u64_t   win_console_init(void);
st_error_t win_console_shutdown(void);

/* ------------------------------------------------------------------
 * Test-only spy API 
 * ------------------------------------------------------------------
 * Functions used to record every rendering call into a small ring
 * buffer so use_case_*.c binaries can assert on what was actually
 * sent towards the Windows D2D backend.
 * ------------------------------------------------------------------ */
/*
 * win_D2D_spy_reset() - Clear the spy ring buffer.
 *
 * Call before the gui_invalidate()/paint pass under test, so only
 * calls made during that pass are captured.
 */
st_error_t win_D2D_spy_reset(win_d2d_ctx_t  *ptCtx);

/*
 * win_D2D_spy_find_text() - Find the first captured draw_text() call
 * whose szText contains szNeedle.
 *
 * Parameters:
 *   szNeedle [in] : Substring to search for.
 *
 * Returns:
 *   Index (0-based) of the first match, or -1 if none / NULL szNeedle.
 */
int win_D2D_spy_find_text(const char *szNeedle, win_d2d_ctx_t  *ptCtx);

/*
 * win_D2D_get_spy() - To be updated
 *
 * Parameters:
 *
 *
 * Returns:
 *   Pointer to the captured record, or NULL if iIndex is out of range.
 */
const void* win_D2D_get_spy(int iIndex,
                            win_d2d_ctx_t  *ptCtx,
                            st_object_t     type);

/*
 * win_D2D_spy_find_fill_rect_color() - Find whether a captured
 * fill_rectangle() call matches the given colour exactly.
 *
 * Parameters:
 *   fR/fG/fB/fA [in] : colour components to match exactly (colours are
 *                      static float literals in the .c under test, so
 *                      exact comparison is reliable).
 *   ptCtx       [in] : D2D context holding the spy ring buffer.
 *
 * Returns:
 *   the index of the last spy structure matching with the needle, -1 otherwise
 */
int win_D2D_spy_find_fill_rect_color(float           fR,
                                     float           fG,
                                     float           fB,
                                     float           fA,
                                     win_d2d_ctx_t  *ptCtx);

                                     /*
 * win_D2D_spy_count_fill_rect_color() - Retrieve the number of captured
 * fill_rectangle() call matches the given colour exactly.
 *
 * Parameters:
 *   fR/fG/fB/fA [in] : colour components to match exactly (colours are
 *                      static float literals in the .c under test, so
 *                      exact comparison is reliable).
 *   ptCtx       [in] : D2D context holding the spy ring buffer.
 *
 * Returns:
 *   the number of spies elements matching with the needle, -1 otherwise
 */
int win_D2D_spy_count_fill_rect_color(float           fR,
                                     float           fG,
                                     float           fB,
                                     float           fA,
                                     win_d2d_ctx_t  *ptCtx);

/* ------------------------------------------------------------------
 * Test-only UI event injection API
 * ------------------------------------------------------------------
 * Functions that inject real Win32 UI events (WM_KEYDOWN, WM_CHAR,
 * WM_LBUTTONDOWN, WM_MOUSEWHEEL, keybd_event()) into an open window's
 * real WndProc, so use_case_*.c binaries can drive the real, static
 * event handlers (dir_handle_key(), dir_handle_click()...) exactly as
 * a human would - the technique R27 uses for GUI event dispatch.
 *
 * Each call sleeps ptWnd->uiEventDelayMs milliseconds afterwards
 * (gui_backend.h; GUI_DEFAULT_EVENT_DELAY_MS by default) so the target
 * window thread has processed the message before the next one is
 * sent. Set ptWnd->uiEventDelayMs directly (white-box, same pattern as
 * bActiveSpies) to speed up or slow down a given test.
 * ------------------------------------------------------------------ */

/*
 * win_evt_send_key() - Inject a non-printable key (arrows, ENTER, ESC,
 * F5, SPACE...) as a real WM_KEYDOWN.
 *
 * Parameters:
 *   ptWnd [in] : Open window to send the event to.
 *   iVk   [in] : Win32 virtual-key code (VK_UP, VK_RETURN, ...).
 *
 * Returns:
 *   ST_NO_ERROR on success, ST_ERROR if ptWnd has no native handle.
 */
st_error_t win_evt_send_key(struct gui_window_s *ptWnd, int iVk);

/*
 * win_evt_send_char() - Inject a printable character as a real
 * WM_CHAR.
 *
 * Parameters:
 *   ptWnd [in] : Open window to send the event to.
 *   cChar [in] : Character to send (e.g. 'H', 'h').
 *
 * Returns:
 *   ST_NO_ERROR on success, ST_ERROR if ptWnd has no native handle.
 */
st_error_t win_evt_send_char(struct gui_window_s *ptWnd, char cChar);

/*
 * win_evt_send_click() - Inject a left mouse click at (iX,iY) client
 * coordinates as a real WM_LBUTTONDOWN.
 *
 * Parameters:
 *   ptWnd [in] : Open window to send the event to.
 *   iX/iY [in] : Client-area coordinates of the click.
 *
 * Returns:
 *   ST_NO_ERROR on success, ST_ERROR if ptWnd has no native handle.
 */
st_error_t win_evt_send_click(struct gui_window_s *ptWnd, int iX, int iY);

/*
 * win_evt_send_wheel() - Inject one mouse-wheel notch as a real
 * WM_MOUSEWHEEL.
 *
 * Parameters:
 *   ptWnd    [in] : Open window to send the event to.
 *   iNotches [in] : Notch count, >0 scrolls up, <0 scrolls down.
 *
 * Returns:
 *   ST_NO_ERROR on success, ST_ERROR if ptWnd has no native handle.
 */
st_error_t win_evt_send_wheel(struct gui_window_s *ptWnd, int iNotches);

/*
 * win_evt_send_ctrl_key() - Inject a real CTRL+<key> chord: a genuine
 * CTRL keybd_event() press brackets the WM_KEYDOWN, since the target
 * WndProc reads the live OS modifier state via GetKeyState(VK_CONTROL)
 * rather than a message flag. Requires the window to hold real OS
 * focus (guaranteed by e.g. dir_open()'s P16 SetForegroundWindow/
 * SetFocus).
 *
 * Parameters:
 *   ptWnd [in] : Open window to send the event to (must hold OS focus).
 *   iVk   [in] : Virtual-key code to chord with CTRL (e.g. VK_SPACE).
 *
 * Returns:
 *   ST_NO_ERROR on success, ST_ERROR if ptWnd has no native handle.
 */
st_error_t win_evt_send_ctrl_key(struct gui_window_s *ptWnd, int iVk);

/*
 * win_evt_send_alt_key() - Same real-modifier technique as
 * win_evt_send_ctrl_key(), for VK_MENU (ALT).
 *
 * Parameters:
 *   ptWnd [in] : Open window to send the event to (must hold OS focus).
 *   iVk   [in] : Virtual-key code to chord with ALT (e.g. VK_LEFT).
 *
 * Returns:
 *   ST_NO_ERROR on success, ST_ERROR if ptWnd has no native handle.
 */
st_error_t win_evt_send_alt_key(struct gui_window_s *ptWnd, int iVk);

/*
 * win_evt_send_resize() - Inject a client-area resize as a real
 * WM_SIZE, the same message Windows sends when the user drags an
 * edge/corner or the window is maximized/restored.
 *
 * Parameters:
 *   ptWnd            [in] : Open window to send the event to.
 *   iWidth/iHeight   [in] : New client-area size in pixels (each must
 *                           fit a 16-bit WORD, per WM_SIZE's LOWORD/
 *                           HIWORD lParam encoding).
 *
 * Returns:
 *   ST_NO_ERROR on success, ST_ERROR if ptWnd has no native handle.
 */
st_error_t win_evt_send_resize(struct gui_window_s *ptWnd,
                                 int iWidth, int iHeight);

#endif /* WIN_H */