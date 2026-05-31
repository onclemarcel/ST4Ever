/*
 * exec_mon.h - ST4Ever Execution monitor (step/run/breakpoint)
 * TODO(UC25): implement this module.
 */
#ifndef EXEC_MON_H
#define EXEC_MON_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct exec_mon_view_s { gui_window_t hWnd; renderer_t hRenderer; } exec_mon_view_t;

st_error_t exec_mon_open(exec_mon_view_t **pptView);
st_error_t exec_mon_close(exec_mon_view_t **pptView);

#endif /* EXEC_MON_H */
