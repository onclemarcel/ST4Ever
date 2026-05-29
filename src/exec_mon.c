/*
 * exec_mon.c - Execution monitor (step/run/breakpoint) (stub)
 * TODO(UC25): implement.
 */
#include "exec_mon.h"
#include "trace.h"

st_error_t exec_mon_open(exec_mon_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_mon_open: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
st_error_t exec_mon_close(exec_mon_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_mon_close: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
