/*
 * edit.h - ST4Ever Integrated editor dispatcher (edit command)
 * TODO(UC10): implement this module.
 */
#ifndef EDIT_H
#define EDIT_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct { gui_window_t hWnd; renderer_t hRenderer; } edit_view_t;

st_error_t edit_open(edit_view_t **pptView);
st_error_t edit_close(edit_view_t **pptView);

#endif /* EDIT_H */
