/*
 * exec_cpu.h - ST4Ever CPU 68000 register view during execution
 * TODO(UC25): implement this module.
 */
#ifndef EXEC_CPU_H
#define EXEC_CPU_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct { gui_window_t hWnd; renderer_t hRenderer; } exec_cpu_view_t;

st_error_t exec_cpu_open(exec_cpu_view_t **pptView);
st_error_t exec_cpu_close(exec_cpu_view_t **pptView);

#endif /* EXEC_CPU_H */
