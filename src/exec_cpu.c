/*
 * exec_cpu.c - CPU 68000 register view during execution (stub)
 * TODO(UC25): implement.
 */
#include "exec_cpu.h"
#include "trace.h"

st_error_t exec_cpu_open(exec_cpu_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_cpu_open: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
st_error_t exec_cpu_close(exec_cpu_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_cpu_close: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
