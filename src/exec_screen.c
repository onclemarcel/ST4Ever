/*
 * exec_screen.c - Atari ST screen emulation view (stub)
 * TODO(UC27): implement.
 */
#include "exec_screen.h"
#include "trace.h"

st_error_t exec_screen_open(exec_screen_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_screen_open: not yet implemented (UC27)");
    return ST_NO_ERROR;
}
st_error_t exec_screen_close(exec_screen_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_screen_close: not yet implemented (UC27)");
    return ST_NO_ERROR;
}
