/*
 * exec.c - Execution engine + view synchronisation (stub)
 * TODO(UC25): implement.
 */
#include "exec.h"
#include "trace.h"

st_error_t exec_open(exec_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_open: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
st_error_t exec_close(exec_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_close: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
