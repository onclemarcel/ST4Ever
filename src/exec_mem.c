/*
 * exec_mem.c - Memory view during execution (stub)
 * TODO(UC25): implement.
 */
#include "exec_mem.h"
#include "trace.h"

st_error_t exec_mem_open(exec_mem_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_mem_open: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
st_error_t exec_mem_close(exec_mem_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_mem_close: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
