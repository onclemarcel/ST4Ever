/*
 * as.c - DEVPAC3 assembler stub
 * TODO(UC30): tokeniser, symbol table, two-pass assembly.
 */
#include "as.h"
#include "trace.h"
#include <string.h>
#include <stdlib.h>

st_error_t as_init(as_context_t *ptCtx, const char *szSourcePath,
                    const char *szOutputPath, st_bool_t bRelocatable)
{
    LOG_TRACE("src='%s' out='%s' reloc=%d",
              szSourcePath ? szSourcePath : "(null)",
              szOutputPath ? szOutputPath : "(null)",
              (int)bRelocatable);
    if (ptCtx == NULL || szSourcePath == NULL || szOutputPath == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    memset(ptCtx, 0, sizeof(as_context_t));
    strncpy(ptCtx->szSourcePath, szSourcePath, ST_MAX_PATH - 1);
    strncpy(ptCtx->szOutputPath, szOutputPath, ST_MAX_PATH - 1);
    ptCtx->bRelocatable = bRelocatable;
    LOG_TODO("as_init: assembler not yet implemented (UC30)");
    return ST_NO_ERROR;
}

st_error_t as_assemble(as_context_t *ptCtx)
{
    LOG_TRACE("ptCtx=%p src='%s'",
              (void *)ptCtx,
              ptCtx ? ptCtx->szSourcePath : "(null)");
    if (ptCtx == NULL) { LOG_ERROR("NULL ptCtx"); return ST_ERROR; }
    LOG_TODO("as_assemble: two-pass assembly not yet implemented (UC30)");
    return ST_ERROR; /* No output produced yet */
}

st_error_t as_shutdown(as_context_t *ptCtx)
{
    LOG_TRACE("ptCtx=%p", (void *)ptCtx);
    if (ptCtx == NULL) { LOG_ERROR("NULL ptCtx"); return ST_ERROR; }
    if (ptCtx->pBinary  != NULL) { free(ptCtx->pBinary);  ptCtx->pBinary  = NULL; }
    if (ptCtx->ptErrors != NULL) { free(ptCtx->ptErrors); ptCtx->ptErrors = NULL; }
    memset(ptCtx, 0, sizeof(as_context_t));
    return ST_NO_ERROR;
}
