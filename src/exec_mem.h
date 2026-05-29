/*
 * exec_mem.h - ST4Ever Memory view during execution
 * TODO(UC25): implement this module.
 */
#ifndef EXEC_MEM_H
#define EXEC_MEM_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct { gui_window_t hWnd; renderer_t hRenderer; } exec_mem_view_t;

st_error_t exec_mem_open(exec_mem_view_t **pptView);
st_error_t exec_mem_close(exec_mem_view_t **pptView);

#endif /* EXEC_MEM_H */
