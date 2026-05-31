/*
 * exec_asm.h - ST4Ever Disassembly view during execution
 * TODO(UC25): implement this module.
 */
#ifndef EXEC_ASM_H
#define EXEC_ASM_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct exec_asm_view_s { gui_window_t hWnd; renderer_t hRenderer; } exec_asm_view_t;

st_error_t exec_asm_open(exec_asm_view_t **pptView);
st_error_t exec_asm_close(exec_asm_view_t **pptView);

#endif /* EXEC_ASM_H */
