/*
 * exec_asm.c - Disassembly view during execution (stub)
 * TODO(UC25): implement.
 */
#include "exec_asm.h"
#include "trace.h"

st_error_t exec_asm_open(exec_asm_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_asm_open: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
st_error_t exec_asm_close(exec_asm_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("exec_asm_close: not yet implemented (UC25)");
    return ST_NO_ERROR;
}
