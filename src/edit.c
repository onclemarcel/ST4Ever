/*
 * edit.c - Integrated editor dispatcher (edit command) (stub)
 * TODO(UC10): implement.
 */
#include "edit.h"
#include "trace.h"

st_error_t edit_open(edit_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("edit_open: not yet implemented (UC10)");
    return ST_NO_ERROR;
}
st_error_t edit_close(edit_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("edit_close: not yet implemented (UC10)");
    return ST_NO_ERROR;
}
