/*
 * exec.h - ST4Ever Execution engine + view synchronisation
 * TODO(UC25): implement this module.
 */
#ifndef EXEC_H
#define EXEC_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct { gui_window_t hWnd; renderer_t hRenderer; } exec_view_t;

st_error_t exec_open(exec_view_t **pptView);
st_error_t exec_close(exec_view_t **pptView);

#endif /* EXEC_H */
