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
 * win_D2D_spy_draw_text_count() - Number of draw_text() calls
 * captured since the last win_D2D_spy_reset().
 *
 * Returns:
 *   0..WIN_D2D_SPY_MAX_ENTRIES (capture stops silently past the cap;
 *   the count keeps growing so a test can also assert "too many").
 */
int win_D2D_spy_draw_text_count(win_d2d_ctx_t  *ptCtx);

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
 * win_D2D_spy_get_draw_text() - Retrieve a captured draw_text() call
 * by index for detailed assertions (rect, colour, font, alignment).
 *
 * Parameters:
 *   iIndex [in] : 0..win_D2D_spy_draw_text_count()-1.
 *
 * Returns:
 *   Pointer to the captured record, or NULL if iIndex is out of range.
 */
const void* win_D2D_get_spy(int iIndex,
                            win_d2d_ctx_t  *ptCtx,
                            st_object_t     type);

#endif /* WIN_H */