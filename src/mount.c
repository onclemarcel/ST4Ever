/*
 * mount.c - ST4Ever floppy disk emulation - portable logic
 *
 * Implements the `mount` command: emulates an Atari ST floppy drive
 * A:\ from a PC host directory or a .st / .msa disk image.
 *
 * This file contains portable logic only.  GUI tree view and
 * drag-and-drop are delegated to the platform backend via gui.h.
 * Disk image read/write is delegated to ST.c (TODO UC16/UC17).
 *
 * TODO(UC18): GUI tree view for the mounted floppy content.
 * TODO(UC17): .msa RLE-compressed image read/write.
 * TODO(UC19): umount with optional image save dialog.
 */
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
