/*
 * exec_cpu.h - ST4Ever CPU 68000 register view
 *
 * Displays the CPU register file snapshot during execution.
 *
 * Window layout (EXEC_CPU_WND_W x EXEC_CPU_WND_H):
 *
 *   D0: $XXXXXXXX   A0: $XXXXXXXX
 *   D1: $XXXXXXXX   A1: $XXXXXXXX
 *   ...
 *   D7: $XXXXXXXX   A7: $XXXXXXXX  (USP)
 *   ---separator---
 *   SSP: $XXXXXXXX    PC: $XXXXXX
 *   ---separator---
 *    SR: $XXXX   state
 *   Flags: T=N S=N I=N X=N N=N Z=N V=N C=N
 *
 * The view is read-only; keyboard closes the window (ESC).
 *
 * UC25A: initial implementation.
 */

#ifndef EXEC_CPU_H
#define EXEC_CPU_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "exec.h"

/* ------------------------------------------------------------------
 * Window geometry
 * ------------------------------------------------------------------ */

#define EXEC_CPU_WND_W   500
#define EXEC_CPU_WND_H   320

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct exec_cpu_view_s
{
    gui_window_t  hWnd;
    renderer_t    hRenderer;
    int           iWndWidth;
    int           iWndHeight;
    int           iCellW;
    int           iCellH;
    exec_state_t *ptState;   /* back-pointer to shared state */
} exec_cpu_view_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * exec_cpu_open() - Open the CPU register view window.
 *
 * Parameters:
 *   pptView [out] : Receives the allocated view context.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL or window creation fails.
 */
st_error_t exec_cpu_open(exec_cpu_view_t **pptView);

/*
 * exec_cpu_close() - Close the window and release resources.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t exec_cpu_close(exec_cpu_view_t **pptView);

#endif /* EXEC_CPU_H */
