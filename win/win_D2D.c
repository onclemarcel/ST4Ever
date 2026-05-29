/*
 * win_D2D.c - Direct2D + DirectWrite renderer backend for renderer.h
 *
 * Each renderer_t is backed by a win_d2d_ctx_t that holds:
 *
 *   ID2D1Factory*           - Created once per process in renderer_init_factory()
 *   ID2D1HwndRenderTarget*  - One per window; created in renderer_create()
 *   IDWriteFactory*         - One per process
 *   IDWriteTextFormat*      - One per font id (MONO / UI)
 *   ID2D1SolidColorBrush*   - Re-created per color change or cached
 *
 * Direct2D coordinate system: float pixels, top-left origin,
 * matches renderer_rect_t exactly.
 *
 * DirectWrite font selection:
 *   RENDERER_FONT_MONO → "Consolas" (fallback: "Courier New")
 *   RENDERER_FONT_UI   → "Segoe UI" (fallback: "Arial")
 *
 * Begin/End draw maps directly to:
 *   ID2D1HwndRenderTarget::BeginDraw()
 *   ID2D1HwndRenderTarget::EndDraw()
 *
 * TODO(UC3): Include <d2d1.h>, <dwrite.h> and implement all functions.
 *            Link with: -ld2d1 -ldwrite -lole32 -loleaut32
 */

#include "../src/common.h"
#include "../src/trace.h"
#include "../src/renderer.h"

#ifdef ST_PLATFORM_WINDOWS

/*
 * UC3 implementation note:
 *
 * 1. Add to Makefile LDFLAGS: -ld2d1 -ldwrite -lole32 -luuid
 * 2. CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) in each view thread
 * 3. D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, ...)
 * 4. DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, ...)
 * 5. CreateHwndRenderTarget bound to the HWND from win_gui.c
 *
 * All D2D objects are COM interfaces (IUnknown* semantics).
 * Use ->Release() in renderer_destroy().
 */

st_error_t win_d2d_renderer_create(void *hWnd, void **ppCtx)
{
    LOG_TRACE("hWnd=%p", hWnd);
    ST_UNUSED(hWnd);
    if (ppCtx != NULL) { *ppCtx = NULL; }
    LOG_TODO("win_d2d_renderer_create: D2D1 + DirectWrite init (UC3)");
    return ST_NO_ERROR;
}

st_error_t win_d2d_renderer_destroy(void **ppCtx)
{
    LOG_TRACE("ppCtx=%p", (void *)ppCtx);
    if (ppCtx != NULL) { *ppCtx = NULL; }
    LOG_TODO("win_d2d_renderer_destroy: COM Release calls (UC3)");
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_WINDOWS */
