/*
 * mount.h - ST4Ever floppy disk emulation view
 *
 * Emulates an Atari ST floppy drive A:\ from a PC directory or a
 * .st / .msa disk image.  Provides a GUI tree view to browse content,
 * drag files in/out, and trigger `image` creation.
 *
 * TODO(UC18): GUI tree view for the mounted floppy.
 * TODO(UC16): .st image read/write.
 * TODO(UC17): .msa image read/write.
 */

#ifndef MOUNT_H
#define MOUNT_H

#include "common.h"
#include "gui.h"

/* Floppy geometry (standard ST DD 3.5") */
#define MOUNT_TRACKS        80
#define MOUNT_SIDES         2
#define MOUNT_SECTORS       9
#define MOUNT_SECTOR_SIZE   512
#define MOUNT_DISK_SIZE     (MOUNT_TRACKS * MOUNT_SIDES * \
                             MOUNT_SECTORS * MOUNT_SECTOR_SIZE)
/* 737,280 bytes */

typedef enum mount_src_s
{
    MOUNT_SRC_DIR  = 0,   /* Mounted from a PC directory             */
    MOUNT_SRC_ST   = 1,   /* Mounted from a .st image                */
    MOUNT_SRC_MSA  = 2    /* Mounted from a .msa image               */
} mount_src_t;

typedef struct mount_context_s
{
    mount_src_t  eSrc;                  /* Source type               */
    char         szSrcPath[ST_MAX_PATH];/* Source dir or image path  */
    st_u8_t     *pDiskData;             /* In-memory disk image      */
    st_bool_t    bDirty;                /* Modified since last save  */
    gui_window_t hWnd;                  /* GUI view handle           */
} mount_context_t;

st_error_t mount_open(const char *szPath, mount_context_t **pptCtx);
st_error_t mount_close(mount_context_t **pptCtx);
st_error_t mount_save_image(mount_context_t *ptCtx,
                              const char *szDestPath);

#endif /* MOUNT_H */
