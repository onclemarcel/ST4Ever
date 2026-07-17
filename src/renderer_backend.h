/*
 * renderer_backend.h - ST4Ever renderer subsystem internal platform interface
 *
 * Private header: included ONLY by renderer.c and platform backends
 * (win/win_D2D.c, linux/lx_X11.c).  Not part of the public API.
 *
 * Provides:
 *   - Full definition of struct renderer_s (opaque in renderer.h).
 *   - Declaration of renderer_platform_* functions implemented by
 *     each backend.
 *   - Font size constants agreed at UC3.2 (13pt mono / 12pt UI).
 *
 * UC3.2: win_D2D.c implemented (Direct2D + DirectWrite).
 * TODO(UC3-Linux): Implement renderer_platform_* in linux/lx_X11.c.
 */

#ifndef RENDERER_BACKEND_H
#define RENDERER_BACKEND_H

#include "renderer.h"
#include "gui.h"
#include "common.h"

/* ------------------------------------------------------------------
 * Font size constants  (agreed UC3.2 — configurable via UC5-bis)
 * ------------------------------------------------------------------ */

#define RENDERER_FONT_SIZE_MONO   13.0f   /* Consolas — hex/asm/CPU views */
#define RENDERER_FONT_SIZE_UI     12.0f   /* Segoe UI — labels, titles     */

/* ------------------------------------------------------------------
 * Full definition of struct renderer_s
 * (opaque in renderer.h as renderer_t)
 * ------------------------------------------------------------------ */

struct renderer_s
{
    void *pPlatform;  /* win_d2d_ctx_t* (Win32) or lx_rnd_ctx_t* (Linux) */
    st_bool_t bActiveSpies; /* Test-related boolean to activate spies */
};

/* ------------------------------------------------------------------
 * Platform backend interface
 * Implemented in win/win_D2D.c (Direct2D) and linux/lx_X11.c (X11).
 * Called exclusively from renderer.c.
 * ------------------------------------------------------------------ */

/*
 * renderer_platform_create() - Create the platform rendering context.
 *
 * Must allocate and populate ptCtx->pPlatform.
 * Called from the window thread after the HWND/XID is live.
 *
 * Parameters:
 *   ptCtx [in/out] : Pre-allocated renderer record.
 *   hWnd  [in]     : Owning GUI window (HWND obtained via
 *                    gui_platform_get_native_handle).
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on platform failure.
 */
st_error_t renderer_platform_create(struct renderer_s *ptCtx,
                                     gui_window_t       hWnd);

/*
 * renderer_platform_destroy() - Release platform rendering resources.
 *
 * Must free ptCtx->pPlatform and set it to NULL.
 *
 * Parameters:
 *   ptCtx [in/out] : Renderer to destroy.
 *
 * Returns: ST_NO_ERROR.
 */
st_error_t renderer_platform_destroy(struct renderer_s *ptCtx);

/*
 * renderer_platform_resize() - Notify backend of a window resize.
 *
 * Parameters:
 *   ptCtx   [in] : Renderer handle.
 *   iWidth  [in] : New client width in pixels.
 *   iHeight [in] : New client height in pixels.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on failure.
 */
st_error_t renderer_platform_resize(struct renderer_s *ptCtx,
                                     int                iWidth,
                                     int                iHeight);

/*
 * renderer_platform_get_font_metrics() - Query character cell size.
 *
 * Parameters:
 *   ptCtx    [in]  : Renderer handle.
 *   eFontId  [in]  : Font to query.
 *   piCellW  [out] : Character cell width in pixels.
 *   piCellH  [out] : Character cell height in pixels.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if ptCtx is NULL.
 */
st_error_t renderer_platform_get_font_metrics(
                                     struct renderer_s  *ptCtx,
                                     renderer_font_id_t  eFontId,
                                     int                *piCellW,
                                     int                *piCellH);

/*
 * renderer_platform_begin_draw() - Begin a rendering frame.
 *
 * Calls BeginDraw (D2D) or clears the back-buffer (X11 Pixmap)
 * and fills it with the given background colour.
 *
 * Parameters:
 *   ptCtx     [in] : Renderer handle.
 *   ptBgColor [in] : Background fill colour.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on failure.
 */
st_error_t renderer_platform_begin_draw(
                                     struct renderer_s      *ptCtx,
                                     const renderer_color_t *ptBgColor);

/*
 * renderer_platform_end_draw() - End the frame and present.
 *
 * Calls EndDraw (D2D) or XCopyArea+XFlush (X11).
 *
 * Parameters:
 *   ptCtx [in] : Renderer handle.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on platform failure.
 */
st_error_t renderer_platform_end_draw(struct renderer_s *ptCtx);

/*
 * renderer_platform_fill_rect() - Fill a rectangle with a solid colour.
 *
 * Parameters:
 *   ptCtx   [in] : Renderer handle.
 *   ptRect  [in] : Rectangle to fill (x, y, width, height).
 *   ptColor [in] : Fill colour.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if any parameter is NULL.
 */
st_error_t renderer_platform_fill_rect(
                                     struct renderer_s      *ptCtx,
                                     const renderer_rect_t  *ptRect,
                                     const renderer_color_t *ptColor);

/*
 * renderer_platform_draw_rect() - Draw a hollow rectangle outline.
 *
 * Parameters:
 *   ptCtx   [in] : Renderer handle.
 *   ptRect  [in] : Rectangle to outline.
 *   ptColor [in] : Stroke colour.
 *   fStroke [in] : Stroke width in pixels.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if any pointer is NULL.
 */
st_error_t renderer_platform_draw_rect(
                                     struct renderer_s      *ptCtx,
                                     const renderer_rect_t  *ptRect,
                                     const renderer_color_t *ptColor,
                                     float                   fStroke);

/*
 * renderer_platform_draw_line() - Draw a line segment.
 *
 * Parameters:
 *   ptCtx   [in] : Renderer handle.
 *   fX1..fY2[in] : Start and end coordinates.
 *   ptColor [in] : Stroke colour.
 *   fStroke [in] : Stroke width in pixels.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on NULL.
 */
st_error_t renderer_platform_draw_line(
                                     struct renderer_s      *ptCtx,
                                     float                   fX1,
                                     float                   fY1,
                                     float                   fX2,
                                     float                   fY2,
                                     const renderer_color_t *ptColor,
                                     float                   fStroke);

/*
 * renderer_platform_draw_text() - Draw a UTF-8 string.
 *
 * Text is clipped to ptRect.
 *
 * Parameters:
 *   ptCtx   [in] : Renderer handle.
 *   szText  [in] : Null-terminated UTF-8 string.
 *   ptRect  [in] : Bounding box for text and clip region.
 *   ptColor [in] : Text colour.
 *   eFontId [in] : Font to use.
 *   eAlign  [in] : Horizontal alignment within ptRect.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if any pointer is NULL.
 */
st_error_t renderer_platform_draw_text(
                                     struct renderer_s      *ptCtx,
                                     const char             *szText,
                                     const renderer_rect_t  *ptRect,
                                     const renderer_color_t *ptColor,
                                     renderer_font_id_t      eFontId,
                                     renderer_align_t        eAlign);

/*
 * renderer_platform_draw_bitmap() - Blit a raw pixel buffer to the window.
 *
 * Pixel layout: BGRA, 4 bytes per pixel, row-major.  Alpha byte is ignored
 * (D2D1_ALPHA_MODE_IGNORE on Windows) — fully opaque rendering.
 * shifter_render() outputs 0x00RRGGBB which in little-endian memory is
 * bytes [B, G, R, 0x00]: correct BGRA layout for this function.
 *
 * Parameters:
 *   ptCtx   [in] : Renderer handle.
 *   pPixels [in] : Pixel data (4 bytes per pixel, iSrcW * iSrcH * 4 bytes).
 *   iSrcW   [in] : Source bitmap width in pixels.
 *   iSrcH   [in] : Source bitmap height in pixels.
 *   ptDest  [in] : Destination rectangle (image scaled to fit).
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on NULL or platform failure.
 */
st_error_t renderer_platform_draw_bitmap(
                                     struct renderer_s      *ptCtx,
                                     const st_u8_t         *pPixels,
                                     int                    iSrcW,
                                     int                    iSrcH,
                                     const renderer_rect_t *ptDest);

#endif /* RENDERER_BACKEND_H */
