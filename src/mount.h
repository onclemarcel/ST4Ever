/*
 * mount.h - ST4Ever floppy disk emulation view
 *
 * Emulates an Atari ST floppy drive A:\ from a PC directory or a
 * .st / .msa disk image.  Provides a D2D GUI view to browse disk
 * content, delete files, and inspect disk properties.
 *
 * Window layout (MOUNT_WND_WIDTH x MOUNT_WND_HEIGHT):
 *   Left panel (MOUNT_LIST_WIDTH px):
 *     Row 0  : header "A:\  [SRC]  N file(s)"  (yellow, always visible)
 *     Row 1+ : file list "  FILENAME.EXT   12345"  (scrollable)
 *     DEL = remove selected file.  UP/DOWN/PgUp/PgDn = navigate.
 *     ESC = close.  Left-click = select.  Scroll wheel = scroll.
 *   Right panel (remainder):
 *     Disk properties: source type, path, file count, free/total space,
 *     dirty flag, keyboard hints.
 *
 * Source types:
 *   MOUNT_SRC_DIR  — blank image populated from a PC directory.
 *   MOUNT_SRC_ST   — existing .st raw image loaded from disk.
 *   MOUNT_SRC_MSA  — existing .msa compressed image loaded from disk.
 *
 * Thread model:
 *   mount_view_open() spawns a GUI thread that owns the D2D renderer.
 *   The console thread calls mount_view_add_file() and
 *   mount_view_close().  No shared state beyond ptImg (immutable after
 *   open) and iEntryCount (only written by the window thread on DEL).
 *
 * UC18.1 : D2D view, mount / umount commands, add-file API.
 * UC18.2 : BPB properties (P34), P36 header cleanup, bootable (P37),
 *           bootsector hex viewer (P38), dir nav history (P10),
 *           dir multi-select (P14).
 * UC19   : umount with optional image-save dialog.
 * UC20   : image creation from mounted content.
 */

#ifndef MOUNT_H
#define MOUNT_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "image_st.h"
#include "line.h"

/* ------------------------------------------------------------------
 * Window geometry
 * ------------------------------------------------------------------ */

#define MOUNT_WND_WIDTH    780
#define MOUNT_WND_HEIGHT   500
#define MOUNT_LIST_WIDTH   516   /* left panel: file list              */

/* ------------------------------------------------------------------
 * Source type
 * ------------------------------------------------------------------ */

typedef enum mount_src_e
{
    MOUNT_SRC_DIR  = 0,  /* Populated from a PC directory              */
    MOUNT_SRC_ST   = 1,  /* Loaded from a .st raw image                */
    MOUNT_SRC_MSA  = 2   /* Loaded from a .msa compressed image        */
} mount_src_t;

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct mount_view_s
{
    gui_window_t       hWnd;        /* GUI window handle (NULL = closed) */
    renderer_t         hRenderer;   /* D2D renderer (lazy-created)       */

    image_st_t        *ptImg;       /* FAT12 disk image in memory        */
    mount_src_t        eSrc;        /* Origin: dir / .st / .msa          */
    char               szSrcPath[ST_MAX_PATH]; /* Source path            */
    st_bool_t          bDirty;      /* Modified since load / creation    */

    /* Root directory contents (flat snapshot) */
    image_st_dirent_t  aEntries[IST_RDE];
    int                iEntryCount;    /* Valid entries in aEntries      */
    int                iSelectedEntry; /* -1=none, >=0 file index        */
    int                iScrollOffset;  /* First visible file row index   */

    /* Client area metrics (set on first PAINT) */
    int                iWndWidth;
    int                iWndHeight;
    int                iCellW;   /* Monospace cell width  (px)           */
    int                iCellH;   /* Monospace cell height (px)           */

    /* P38: bootsector hex viewer (edit_hex_view_t* stored as void*)     */
    void              *ptBootHexView;
    /* Back-ref to console context for P38 edit_hex_open                 */
    line_context_t    *ptLineCtx;
} mount_view_t;

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

/*
 * mount_view_open() - Open a floppy emulation view from szPath.
 *
 * szPath dispatch:
 *   Directory      -> MOUNT_SRC_DIR  : blank image, populated from dir.
 *   *.st file      -> MOUNT_SRC_ST   : loaded via image_st_load().
 *   *.msa file     -> MOUNT_SRC_MSA  : loaded via image_msa_load().
 *   NULL / ""      -> uses ptLineCtx->szCwd.
 *
 * Parameters:
 *   szPath    [in]  : Source path (NULL = current working directory).
 *   ptLineCtx [in]  : Console context (provides szCwd).
 *   pptView   [out] : Receives the allocated view on success.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptLineCtx or pptView is NULL, the path does not
 *               exist, the source type is unsupported, or the GUI
 *               window cannot be opened.
 */
st_error_t mount_view_open(const char     *szPath,
                             line_context_t *ptLineCtx,
                             mount_view_t  **pptView);

/*
 * mount_view_close() - Close the view and release all resources.
 *
 * Sends WM_CLOSE to the window thread and joins it, frees the disk
 * image, then zeroes *pptView.  mount_view_close(&NULL) is a no-op.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t mount_view_close(mount_view_t **pptView);

/*
 * mount_view_add_file() - Import a PC file into the mounted disk image.
 *
 * Reads szSrcPath from the host filesystem and writes it into the disk
 * image using image_st_write_file().  The filename used on the disk is
 * the basename of szSrcPath (must be a valid FAT 8.3 name).  Refreshes
 * the entry list and redraws the window.
 *
 * Called directly by line_cmd_mount (initial dir populate) and by the
 * drag-and-drop handler in UC18.2.
 *
 * Parameters:
 *   ptView    [in/out] : Open mount view with a valid disk image.
 *   szSrcPath [in]     : Absolute path of the PC file to import.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL, the file is unreadable, the
 *               name is not a valid 8.3 name, or the disk is full.
 */
st_error_t mount_view_add_file(mount_view_t *ptView,
                                const char   *szSrcPath);

/*
 * mount_is_bootable() - Test whether a 512-byte bootsector is Atari ST
 *                       bootable.
 *
 * Uses the WD1772 checksum: the sum of the 256 LE16 words in the sector
 * must equal 0x1234 (mod 0x10000).
 *
 * Parameters:
 *   pBootSect [in] : Pointer to a 512-byte bootsector buffer.
 *
 * Returns:
 *   ST_TRUE  if the WD1772 checksum matches.
 *   ST_FALSE if pBootSect is NULL or the checksum does not match.
 */
st_bool_t mount_is_bootable(const st_u8_t *pBootSect);

#endif /* MOUNT_H */
