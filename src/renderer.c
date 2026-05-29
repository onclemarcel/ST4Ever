/*
 * renderer.c - ST4Ever renderer stub
 *
 * TODO(UC3): Implement via win_D2D.c (Direct2D) / lx_X11.c (XRender).
 */

#include "renderer.h"
#include "trace.h"

st_error_t renderer_create(gui_window_t hWnd, renderer_t *phCtx)
{
    LOG_TRACE("hWnd=%p", (void *)hWnd);
    ST_UNUSED(hWnd);
    if (phCtx != NULL) { *phCtx = NULL; }
    LOG_TODO("renderer_create: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_resize(renderer_t hCtx, int iW, int iH)
{
    LOG_TRACE("hCtx=%p %dx%d", (void *)hCtx, iW, iH);
    ST_UNUSED(hCtx); ST_UNUSED(iW); ST_UNUSED(iH);
    LOG_TODO("renderer_resize: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_get_font_metrics(renderer_t hCtx,
    renderer_font_id_t eFontId, int *piCellW, int *piCellH)
{
    LOG_TRACE("hCtx=%p fontId=%d", (void *)hCtx, (int)eFontId);
    ST_UNUSED(hCtx); ST_UNUSED(eFontId);
    if (piCellW) *piCellW = 8;
    if (piCellH) *piCellH = 16;
    LOG_TODO("renderer_get_font_metrics: stub values 8x16 (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_begin_draw(renderer_t hCtx,
    const renderer_color_t *ptBgColor)
{
    LOG_TRACE("hCtx=%p", (void *)hCtx);
    ST_UNUSED(hCtx); ST_UNUSED(ptBgColor);
    LOG_TODO("renderer_begin_draw: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_end_draw(renderer_t hCtx)
{
    LOG_TRACE("hCtx=%p", (void *)hCtx);
    ST_UNUSED(hCtx);
    LOG_TODO("renderer_end_draw: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_fill_rect(renderer_t hCtx,
    const renderer_rect_t *ptRect, const renderer_color_t *ptColor)
{
    LOG_TRACE("hCtx=%p", (void *)hCtx);
    ST_UNUSED(hCtx); ST_UNUSED(ptRect); ST_UNUSED(ptColor);
    LOG_TODO("renderer_fill_rect: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_draw_rect(renderer_t hCtx,
    const renderer_rect_t *ptRect, const renderer_color_t *ptColor,
    float fStroke)
{
    LOG_TRACE("hCtx=%p stroke=%.1f", (void *)hCtx, (double)fStroke);
    ST_UNUSED(hCtx); ST_UNUSED(ptRect);
    ST_UNUSED(ptColor); ST_UNUSED(fStroke);
    LOG_TODO("renderer_draw_rect: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_draw_line(renderer_t hCtx,
    float fX1, float fY1, float fX2, float fY2,
    const renderer_color_t *ptColor, float fStroke)
{
    LOG_TRACE("hCtx=%p (%.0f,%.0f)-(%.0f,%.0f)",
              (void *)hCtx, (double)fX1, (double)fY1,
              (double)fX2, (double)fY2);
    ST_UNUSED(hCtx); ST_UNUSED(fX1); ST_UNUSED(fY1);
    ST_UNUSED(fX2); ST_UNUSED(fY2);
    ST_UNUSED(ptColor); ST_UNUSED(fStroke);
    LOG_TODO("renderer_draw_line: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_draw_text(renderer_t hCtx, const char *szText,
    const renderer_rect_t *ptRect, const renderer_color_t *ptColor,
    renderer_font_id_t eFontId, renderer_align_t eAlign)
{
    LOG_TRACE("hCtx=%p text='%.20s'", (void *)hCtx,
              szText ? szText : "(null)");
    ST_UNUSED(hCtx); ST_UNUSED(szText); ST_UNUSED(ptRect);
    ST_UNUSED(ptColor); ST_UNUSED(eFontId); ST_UNUSED(eAlign);
    LOG_TODO("renderer_draw_text: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_draw_bitmap(renderer_t hCtx,
    const st_u8_t *pPixels, int iSrcW, int iSrcH,
    const renderer_rect_t *ptDest)
{
    LOG_TRACE("hCtx=%p src=%dx%d", (void *)hCtx, iSrcW, iSrcH);
    ST_UNUSED(hCtx); ST_UNUSED(pPixels);
    ST_UNUSED(iSrcW); ST_UNUSED(iSrcH); ST_UNUSED(ptDest);
    LOG_TODO("renderer_draw_bitmap: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
st_error_t renderer_destroy(renderer_t *phCtx)
{
    LOG_TRACE("phCtx=%p", (void *)phCtx);
    if (phCtx != NULL) { *phCtx = NULL; }
    LOG_TODO("renderer_destroy: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
