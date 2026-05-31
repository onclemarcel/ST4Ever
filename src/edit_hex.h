/*
 * edit_hex.h - ST4Ever Hex + ASCII editor view
 * TODO(UC9): implement this module.
 */
#ifndef EDIT_HEX_H
#define EDIT_HEX_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct edit_hex_view_s { gui_window_t hWnd; renderer_t hRenderer; } edit_hex_view_t;

st_error_t edit_hex_open(edit_hex_view_t **pptView);
st_error_t edit_hex_close(edit_hex_view_t **pptView);

#endif /* EDIT_HEX_H */
