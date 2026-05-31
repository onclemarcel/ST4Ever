/*
 * edit_txt.h - ST4Ever Text / source editor view
 * TODO(UC8): implement this module.
 */
#ifndef EDIT_TXT_H
#define EDIT_TXT_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct edit_txt_view_s { gui_window_t hWnd; renderer_t hRenderer; } edit_txt_view_t;

st_error_t edit_txt_open(edit_txt_view_t **pptView);
st_error_t edit_txt_close(edit_txt_view_t **pptView);

#endif /* EDIT_TXT_H */
