/*
 * edit_hex.c - Hex + ASCII editor view (stub)
 * TODO(UC9): implement.
 */
#include "edit_hex.h"
#include "trace.h"

st_error_t edit_hex_open(edit_hex_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("edit_hex_open: not yet implemented (UC9)");
    return ST_NO_ERROR;
}
st_error_t edit_hex_close(edit_hex_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("edit_hex_close: not yet implemented (UC9)");
    return ST_NO_ERROR;
}
