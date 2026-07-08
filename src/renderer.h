/*
 * renderer.h - ST4Ever portable 2D rendering abstraction
 *
 * Provides a minimal, efficient 2D drawing API used by all GUI views
 * and by the Atari ST screen emulator.  All coordinates are in pixels,
 * origin top-left of the client area.
 *
 * Platform backends:
 *   win/win_D2D.c  - Direct2D (ID2D1HwndRenderTarget) + DirectWrite
 *   linux/lx_X11.c - Xlib (XRender extension for compositing)
 *
 * Lifecycle per frame:
 *   renderer_begin_draw()
 *     renderer_fill_rect() / renderer_draw_line() / draw_text() ...
 *   renderer_end_draw()     <- swaps / presents the frame
 *
 * The renderer_t handle is created once per window and destroyed when
 * the window closes.  Resize events trigger renderer_resize().
 *
 * Font model:
 *   A fixed-pitch monospace font is the primary font (hex editor,
 *   disassembler, CPU view, memory view).  A proportional font is
 *   used for UI chrome.  Font handles are created by the renderer
 *   and cached internally.
 *
 * TODO(UC3): Implement renderer_create() and all draw primitives.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "common.h"
#include "gui.h"

/* ------------------------------------------------------------------
 * Opaque renderer handle
 * ------------------------------------------------------------------ */

typedef struct renderer_s *renderer_t;   /* NULL = invalid            */

/* ------------------------------------------------------------------
 * Opaque font handle
 * ------------------------------------------------------------------ */

typedef struct renderer_font_s *renderer_font_t;   /* NULL = default  */

/* ------------------------------------------------------------------
 * Colour (linear RGBA, 0.0 - 1.0)
 * ------------------------------------------------------------------ */

typedef struct renderer_color_s
{
    float r;
    float g;
    float b;
    float a;   /* 1.0 = fully opaque */
} renderer_color_t;

/* Predefined palette - Atari ST-inspired */
#define RENDERER_COLOR_BLACK    { 0.00f, 0.00f, 0.00f, 1.0f }
#define RENDERER_COLOR_WHITE    { 1.00f, 1.00f, 1.00f, 1.0f }
#define RENDERER_COLOR_GRAY     { 0.25f, 0.25f, 0.25f, 1.0f }
#define RENDERER_COLOR_LTGRAY   { 0.80f, 0.80f, 0.80f, 1.0f }
#define RENDERER_COLOR_GREEN    { 0.00f, 0.85f, 0.20f, 1.0f }
#define RENDERER_COLOR_CYAN     { 0.00f, 0.85f, 0.85f, 1.0f }
#define RENDERER_COLOR_YELLOW   { 0.95f, 0.90f, 0.10f, 1.0f }
#define RENDERER_COLOR_RED      { 0.95f, 0.10f, 0.10f, 1.0f }
#define RENDERER_COLOR_MAGENTA  { 0.85f, 0.00f, 0.85f, 1.0f }
#define RENDERER_COLOR_BLUE     { 0.20f, 0.40f, 0.95f, 1.0f }

/* ------------------------------------------------------------------
 * Rectangle  (pixel coordinates, float for D2D compatibility)
 * ------------------------------------------------------------------ */

typedef struct renderer_rect_s
{
    float fX;   /* Left edge                                         */
    float fY;   /* Top edge                                          */
    float fW;   /* Width                                             */
    float fH;   /* Height                                            */
} renderer_rect_t;

/* Helper macro */
#define RENDERER_RECT(x, y, w, h)  { (float)(x), (float)(y), \
                                       (float)(w), (float)(h) }

/* ------------------------------------------------------------------
 * Text alignment
 * ------------------------------------------------------------------ */

typedef enum renderer_align_s
{
    RENDERER_ALIGN_LEFT   = 0,
    RENDERER_ALIGN_CENTER = 1,
    RENDERER_ALIGN_RIGHT  = 2
} renderer_align_t;

/* ------------------------------------------------------------------
 * Font descriptors
 * ------------------------------------------------------------------ */

typedef enum renderer_font_id_s
{
    RENDERER_FONT_MONO  = 0,  /* Fixed-pitch (Courier / Consolas)    */
    RENDERER_FONT_UI    = 1   /* Proportional UI font (Segoe / Sans) */
} renderer_font_id_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * renderer_create() - Create a renderer attached to a GUI window.
 *
 * On Windows, creates an ID2D1HwndRenderTarget bound to the window's
 * HWND.  On Linux, creates an X11 Graphics Context bound to the
 * window's XID.
 *
 * Parameters:
 *   hWnd    [in]  : Target window (must be open).
 *   phCtx   [out] : Receives the renderer handle.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on platform failure (logged).
 */
st_error_t renderer_create(gui_window_t  hWnd,
                             renderer_t   *phCtx);

/*
 * renderer_resize() - Notify the renderer of a window resize.
 *
 * Must be called from the GUI_EVT_RESIZE handler.  On D2D this calls
 * ID2D1HwndRenderTarget::Resize().
 *
 * Parameters:
 *   hCtx    [in] : Renderer to resize.
 *   iWidth  [in] : New client width in pixels.
 *   iHeight [in] : New client height in pixels.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hCtx is NULL or the platform call fails.
 */
st_error_t renderer_resize(renderer_t hCtx, int iWidth, int iHeight);

/*
 * renderer_get_font_metrics() - Query character cell size for a font.
 *
 * Used by views to compute layout (line height, column width).
 *
 * Parameters:
 *   hCtx      [in]  : Renderer handle.
 *   eFontId   [in]  : Font to query.
 *   piCellW   [out] : Character cell width in pixels.
 *   piCellH   [out] : Character cell height in pixels.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL.
 */
st_error_t renderer_get_font_metrics(renderer_t         hCtx,
                                      renderer_font_id_t eFontId,
                                      int               *piCellW,
                                      int               *piCellH);

/*
 * renderer_begin_draw() - Begin a rendering frame.
 *
 * Must be called before any draw primitive.  Clears the back-buffer
 * with the specified background colour.
 *
 * Parameters:
 *   hCtx      [in] : Renderer handle.
 *   ptBgColor [in] : Background fill colour.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hCtx is NULL or D2D/X11 BeginDraw fails.
 */
st_error_t renderer_begin_draw(renderer_t             hCtx,
                                const renderer_color_t *ptBgColor);

/*
 * renderer_end_draw() - End the frame and present / flush.
 *
 * On D2D calls EndDraw().  On X11 calls XFlush().
 *
 * Parameters:
 *   hCtx [in] : Renderer handle.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hCtx is NULL or platform call fails.
 */
st_error_t renderer_end_draw(renderer_t hCtx);

/*
 * renderer_fill_rect() - Fill a rectangle with a solid colour.
 *
 * Parameters:
 *   hCtx    [in] : Renderer handle.
 *   ptRect  [in] : Rectangle to fill.
 *   ptColor [in] : Fill colour.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL.
 */
st_error_t renderer_fill_rect(renderer_t             hCtx,
                                const renderer_rect_t  *ptRect,
                                const renderer_color_t *ptColor);

/*
 * renderer_draw_rect() - Draw a hollow rectangle outline.
 *
 * Parameters:
 *   hCtx      [in] : Renderer handle.
 *   ptRect    [in] : Rectangle to outline.
 *   ptColor   [in] : Stroke colour.
 *   fStroke   [in] : Stroke width in pixels (1.0 typical).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL.
 */
st_error_t renderer_draw_rect(renderer_t             hCtx,
                                const renderer_rect_t  *ptRect,
                                const renderer_color_t *ptColor,
                                float                   fStroke);

/*
 * renderer_draw_line() - Draw a line segment.
 *
 * Parameters:
 *   hCtx    [in] : Renderer handle.
 *   fX1     [in] : Start X.
 *   fY1     [in] : Start Y.
 *   fX2     [in] : End X.
 *   fY2     [in] : End Y.
 *   ptColor [in] : Stroke colour.
 *   fStroke [in] : Stroke width in pixels.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if hCtx or ptColor is NULL.
 */
st_error_t renderer_draw_line(renderer_t             hCtx,
                                float                  fX1,
                                float                  fY1,
                                float                  fX2,
                                float                  fY2,
                                const renderer_color_t *ptColor,
                                float                   fStroke);

/*
 * renderer_draw_text() - Draw a UTF-8 string.
 *
 * Text is clipped to ptRect.  Newlines in szText are not handled;
 * callers split multi-line content before calling.
 *
 * Parameters:
 *   hCtx     [in] : Renderer handle.
 *   szText   [in] : Null-terminated UTF-8 string.
 *   ptRect   [in] : Bounding box (text is clipped here).
 *   ptColor  [in] : Text colour.
 *   eFontId  [in] : Font to use.
 *   eAlign   [in] : Horizontal alignment within ptRect.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer parameter is NULL.
 */
st_error_t renderer_draw_text(renderer_t              hCtx,
                                const char             *szText,
                                const renderer_rect_t  *ptRect,
                                const renderer_color_t *ptColor,
                                renderer_font_id_t      eFontId,
                                renderer_align_t        eAlign);

/*
 * renderer_draw_bitmap() - Blit a raw RGBA pixel buffer.
 *
 * Used by exec_screen.c to display the Atari ST video output.
 * The buffer must be width * height * 4 bytes (RGBA8888).
 *
 * Parameters:
 *   hCtx    [in] : Renderer handle.
 *   pPixels [in] : Source pixel data (RGBA8888, row-major).
 *   iSrcW   [in] : Source bitmap width.
 *   iSrcH   [in] : Source bitmap height.
 *   ptDest  [in] : Destination rectangle (scaled to fit).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL or dimensions are zero.
 */
st_error_t renderer_draw_bitmap(renderer_t            hCtx,
                                  const st_u8_t        *pPixels,
                                  int                   iSrcW,
                                  int                   iSrcH,
                                  const renderer_rect_t *ptDest);

/*
 * renderer_destroy() - Release renderer resources.
 *
 * Sets *phCtx to NULL on success.
 *
 * Parameters:
 *   phCtx [in/out] : Renderer to destroy.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if phCtx or *phCtx is NULL.
 */
st_error_t renderer_destroy(renderer_t *phCtx);

/* ------------------------------------------------------------------
 * Test-only spy API (ST_TEST_FWK builds only, R26)
 * ------------------------------------------------------------------
 * renderer_draw_text() records every call into a small ring buffer
 * when compiled with -DST_TEST_FWK, so use_case_*.c binaries can
 * assert on what was actually sent towards the platform backend
 * (text, rect, colour, font, alignment) without opening a real D2D
 * window and eyeballing pixels.  Compiled out entirely otherwise -
 * zero cost and zero footprint in the release binary.
 * ------------------------------------------------------------------ */
#ifdef ST_TEST_FWK

#define RENDERER_SPY_MAX_ENTRIES 64
#define RENDERER_SPY_TEXT_LEN    128

typedef struct renderer_spy_draw_text_s
{
    char               szText[RENDERER_SPY_TEXT_LEN];
    renderer_rect_t    tRect;
    renderer_color_t   tColor;
    renderer_font_id_t eFontId;
    renderer_align_t   eAlign;
} renderer_spy_draw_text_t;

/*
 * renderer_spy_reset() - Clear the draw_text spy ring buffer.
 *
 * Call before the gui_invalidate()/paint pass under test, so only
 * calls made during that pass are captured.
 */
void renderer_spy_reset(void);

/*
 * renderer_spy_draw_text_count() - Number of draw_text() calls
 * captured since the last renderer_spy_reset().
 *
 * Returns:
 *   0..RENDERER_SPY_MAX_ENTRIES (capture stops silently past the cap;
 *   the count keeps growing so a test can also assert "too many").
 */
int renderer_spy_draw_text_count(void);

/*
 * renderer_spy_find_text() - Find the first captured draw_text() call
 * whose szText contains szNeedle.
 *
 * Parameters:
 *   szNeedle [in] : Substring to search for.
 *
 * Returns:
 *   Index (0-based) of the first match, or -1 if none / NULL szNeedle.
 */
int renderer_spy_find_text(const char *szNeedle);

/*
 * renderer_spy_get_draw_text() - Retrieve a captured draw_text() call
 * by index for detailed assertions (rect, colour, font, alignment).
 *
 * Parameters:
 *   iIndex [in] : 0..renderer_spy_draw_text_count()-1.
 *
 * Returns:
 *   Pointer to the captured record, or NULL if iIndex is out of range.
 */
const renderer_spy_draw_text_t *renderer_spy_get_draw_text(int iIndex);

#endif /* ST_TEST_FWK */

#endif /* RENDERER_H */
