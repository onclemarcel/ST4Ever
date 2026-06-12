/*
 * exec_screen.h - ST4Ever Atari ST screen emulation view
 *
 * Displays the Atari ST video output (low/med/high res) during execution.
 * Renders the Shifter frame buffer via shifter_render() into an RGB32 pixel
 * buffer, then blits it to the D2D window via renderer_draw_bitmap().
 *
 * Window layout (EXEC_SCR_WND_W x EXEC_SCR_WND_H = 640 x 400):
 *   Matches high-res 1:1.  Low-res (320x200) is scaled 2x; med-res
 *   (640x200) is scaled 1x wide / 2x tall.  Image is aspect-ratio
 *   preserved with black borders when the window is resized.
 *
 * The view reads ptMachine directly (like exec_mem) without holding the
 * mutex during shifter_render() — a slightly torn frame is acceptable.
 * The exec thread calls gui_invalidate(hScrWnd) after each quantum.
 *
 * UC27: initial implementation.
 */

#ifndef EXEC_SCREEN_H
#define EXEC_SCREEN_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "exec.h"
#include "shifter.h"

/* ------------------------------------------------------------------
 * Window geometry
 * ------------------------------------------------------------------ */

#define EXEC_SCR_WND_W  640   /* matches high-res 1:1, low-res 2x */
#define EXEC_SCR_WND_H  400

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct exec_screen_view_s
{
    gui_window_t   hWnd;
    renderer_t     hRenderer;
    int            iWndWidth;
    int            iWndHeight;
    exec_state_t  *ptState;
    /* ~1 MB frame buffer — struct must be heap-allocated */
    st_u32_t       auPixels[SHIFTER_MAX_PIXELS];
} exec_screen_view_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * exec_screen_open() - Open the screen emulation view window.
 *
 * Parameters:
 *   pptView [out] : Receives the allocated view context.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL or window creation fails.
 */
st_error_t exec_screen_open(exec_screen_view_t **pptView);

/*
 * exec_screen_close() - Close the window and release resources.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t exec_screen_close(exec_screen_view_t **pptView);

#endif /* EXEC_SCREEN_H */
