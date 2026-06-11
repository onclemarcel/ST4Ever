/*
 * exec_mon.h - ST4Ever Execution monitor window
 *
 * Displays the current execution state and provides keyboard controls
 * for step/run/pause/stop and breakpoint management.
 *
 * Window layout (EXEC_MON_WND_W x EXEC_MON_WND_H):
 *
 *   Line 0 : "PROGNAME — state"                    (header)
 *   Line 1 : blank
 *   Line 2 : "PC: $XXXXXX"                         (current address)
 *   Line 3 : "  mnemonic operands"                 (next instruction)
 *   Line 4 : "Instr: NNNN   Cycles: NNNN"          (counters)
 *   --------separator-------
 *   Line 5+ : breakpoints list
 *   --------separator-------
 *   Last-2  : key hints row 1
 *   Last-1  : key hints row 2
 *
 * Keyboard:
 *   F5       = step one instruction
 *   F6       = run / pause toggle
 *   F7       = stop (reset PC to load address)
 *   F8 / ESC = quit (close execution session)
 *   B        = toggle breakpoint at current PC
 *   C        = clear all breakpoints
 *
 * UC25A: initial implementation.
 */

#ifndef EXEC_MON_H
#define EXEC_MON_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "exec.h"

/* ------------------------------------------------------------------
 * Window geometry
 * ------------------------------------------------------------------ */

#define EXEC_MON_WND_W   660
#define EXEC_MON_WND_H   420

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct exec_mon_view_s
{
    gui_window_t  hWnd;
    renderer_t    hRenderer;
    int           iWndWidth;
    int           iWndHeight;
    int           iCellW;
    int           iCellH;
    exec_state_t *ptState;   /* back-pointer to shared state */
} exec_mon_view_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * exec_mon_open() - Open the execution monitor window.
 *
 * The view context is heap-allocated.  The window runs in a dedicated
 * GUI thread.  Returns once the window is visible.
 *
 * Parameters:
 *   pptView [out] : Receives the allocated view context.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL or window creation fails.
 */
st_error_t exec_mon_open(exec_mon_view_t **pptView);

/*
 * exec_mon_close() - Close the window and release resources.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t exec_mon_close(exec_mon_view_t **pptView);

#endif /* EXEC_MON_H */
