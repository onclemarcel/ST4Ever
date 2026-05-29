#include "mount.h"
#include "trace.h"
#include <stdlib.h>
#include <string.h>

st_error_t mount_open(const char *szPath, mount_context_t **pptCtx)
{
    LOG_TRACE("szPath='%s'", szPath ? szPath : "(null)");
    ST_UNUSED(szPath);
    if (pptCtx != NULL) { *pptCtx = NULL; }
    LOG_TODO("mount_open: not yet implemented (UC18)");
    return ST_NO_ERROR;
}
st_error_t mount_close(mount_context_t **pptCtx)
{
    LOG_TRACE("pptCtx=%p", (void *)pptCtx);
    if (pptCtx != NULL) { *pptCtx = NULL; }
    LOG_TODO("mount_close: not yet implemented (UC18)");
    return ST_NO_ERROR;
}
st_error_t mount_save_image(mount_context_t *ptCtx, const char *szDest)
{
    LOG_TRACE("ptCtx=%p dest='%s'", (void *)ptCtx,
              szDest ? szDest : "(null)");
    ST_UNUSED(ptCtx); ST_UNUSED(szDest);
    LOG_TODO("mount_save_image: not yet implemented (UC19/UC20)");
    return ST_NO_ERROR;
}
