/*
 * exec_asm.h - ST4Ever Disassembly view during execution
 *
 * Displays a scrollable disassembly of ST machine RAM.  Each line
 * shows address, raw hex word(s), mnemonic and operands.  The line
 * at PC is highlighted yellow.
 *
 * Window layout (EXEC_ASM_WND_W x EXEC_ASM_WND_H):
 *
 *   $XXXXXX  XXXX [XXXX]  MNEMONIC    OPERANDS
 *   ...
 *   ---hint bar---
 *
 * Navigation: UP/DOWN, PgUp/PgDn, F snap to PC, ESC close.
 *
 * UC25B: initial implementation.
 */

#ifndef EXEC_ASM_H
#define EXEC_ASM_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "exec.h"

/* ------------------------------------------------------------------
 * Window geometry
 * ------------------------------------------------------------------ */

#define EXEC_ASM_WND_W   700
#define EXEC_ASM_WND_H   440
#define EXEC_ASM_HINT_H   20

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct exec_asm_view_s
{
    gui_window_t  hWnd;
    renderer_t    hRenderer;
    int           iWndWidth;
    int           iWndHeight;
    int           iCellW;
    int           iCellH;
    st_u32_t      uiAsmBase;   /* address of top-visible instruction */
    exec_state_t *ptState;     /* back-pointer to shared state       */
} exec_asm_view_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * exec_asm_open() - Open the disassembly view window.
 *
 * Parameters:
 *   pptView [out] : Receives the allocated view context.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL or window creation fails.
 */
st_error_t exec_asm_open(exec_asm_view_t **pptView);

/*
 * exec_asm_close() - Close the window and release resources.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t exec_asm_close(exec_asm_view_t **pptView);

#endif /* EXEC_ASM_H */
