/*
 * dir.c - ST4Ever directory tree view implementation (stub)
 * TODO(UC3): Implement dir_open with Win32/X11 window + tree rendering.
 */
#include "dir.h"
#include "trace.h"

st_error_t dir_open(const char     *szPath,
                     line_context_t *ptLineCtx,
                     dir_view_t    **pptView)
{
    LOG_TRACE("szPath='%s' ptLineCtx=%p",
              szPath ? szPath : "(cwd)", (void *)ptLineCtx);
    ST_UNUSED(szPath);
    ST_UNUSED(ptLineCtx);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("dir_open: Win32 tree-view window not yet implemented (UC3)");
    return ST_NO_ERROR;
}

st_error_t dir_close(dir_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);
    if (pptView != NULL) { *pptView = NULL; }
    LOG_TODO("dir_close: not yet implemented (UC3)");
    return ST_NO_ERROR;
}
