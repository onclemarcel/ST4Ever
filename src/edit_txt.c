/*
 * edit_txt.c - Text / source editor view (stub)
 * TODO(UC8): implement.
 */
#include "edit_txt.h"
#include "trace.h"

st_error_t edit_txt_open(edit_txt_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("edit_txt_open: not yet implemented (UC8)");
    return ST_NO_ERROR;
}
st_error_t edit_txt_close(edit_txt_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("edit_txt_close: not yet implemented (UC8)");
    return ST_NO_ERROR;
}
