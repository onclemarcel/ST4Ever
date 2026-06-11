/*
 * exec_mem.h - ST4Ever Memory dump view during execution
 *
 * Displays a scrollable hex dump of ST machine RAM.  16 bytes per row,
 * hex + ASCII.  The row containing PC is highlighted yellow.
 *
 * Window layout (EXEC_MEM_WND_W x EXEC_MEM_WND_H):
 *
 *   $XXXXXX: XX XX XX XX  XX XX XX XX  XX XX XX XX  XX XX XX XX  ...
 *   ...
 *   ---hint bar---
 *
 * Navigation: UP/DOWN row, PgUp/PgDn page, HOME snap to PC, ESC close.
 *
 * UC25B: initial implementation.
 */

#ifndef EXEC_MEM_H
#define EXEC_MEM_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "exec.h"

/* ------------------------------------------------------------------
 * Window geometry
 * ------------------------------------------------------------------ */

#define EXEC_MEM_WND_W          760
#define EXEC_MEM_WND_H          460
#define EXEC_MEM_BYTES_PER_ROW   16
#define EXEC_MEM_HINT_H          20

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct exec_mem_view_s
{
    gui_window_t  hWnd;
    renderer_t    hRenderer;
    int           iWndWidth;
    int           iWndHeight;
    int           iCellW;
    int           iCellH;
    st_u32_t      uiMemBase;   /* address of the top-visible row     */
    exec_state_t *ptState;     /* back-pointer to shared state       */
} exec_mem_view_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * exec_mem_open() - Open the memory dump view window.
 *
 * Parameters:
 *   pptView [out] : Receives the allocated view context.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL or window creation fails.
 */
st_error_t exec_mem_open(exec_mem_view_t **pptView);

/*
 * exec_mem_close() - Close the window and release resources.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t exec_mem_close(exec_mem_view_t **pptView);

#endif /* EXEC_MEM_H */
