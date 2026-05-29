/*
 * exec_screen.h - ST4Ever Atari ST screen emulation view
 * TODO(UC27): implement this module.
 */
#ifndef EXEC_SCREEN_H
#define EXEC_SCREEN_H
#include "common.h"
#include "gui.h"
#include "renderer.h"

typedef struct { gui_window_t hWnd; renderer_t hRenderer; } exec_screen_view_t;

st_error_t exec_screen_open(exec_screen_view_t **pptView);
st_error_t exec_screen_close(exec_screen_view_t **pptView);

#endif /* EXEC_SCREEN_H */
