/*
 * edit_hex.h - ST4Ever hex + ASCII editor view
 *
 * Opens a Direct2D window displaying file content as:
 *
 *   XXXXXX: XX XX XX XX XX XX XX XX  XX XX XX XX XX XX XX XX  |................|
 *   ^addr   ^--- 16 bytes hex, gap after byte 7 --------^     ^-- ASCII ------^
 *
 * Two editing zones, toggled with TAB:
 *   HEX_ZONE_HEX   — type 0–9/A–F to overwrite nibble; cursor
 *                    advances through high then low nibble.
 *   HEX_ZONE_ASCII — type printable ASCII to overwrite byte;
 *                    cursor advances by one byte.
 *
 * Editing is replace-mode only (file size is fixed after open).
 *
 * Keyboard bindings (both zones):
 *   Arrow keys      : cursor movement (±1 byte, ±16 bytes row)
 *   Home / End      : start / end of current row
 *   PgUp / PgDn     : scroll one screen
 *   CTRL+Home       : go to byte 0
 *   CTRL+End        : go to last byte
 *   TAB             : switch zone (HEX ↔ ASCII)
 *   CTRL+S          : save file
 *   ESC             : close (unsaved changes noted in trace)
 *
 * Mouse:
 *   Left click      : set cursor to clicked byte; select zone
 *
 * Title: "ST4Ever - Hex: <filename> [*]"  ([*] when dirty)
 *
 * R18: title updated on open and on every save.
 * UC9: full implementation.
 */

#ifndef EDIT_HEX_H
#define EDIT_HEX_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "line.h"

/* ------------------------------------------------------------------
 * Layout constants
 * ------------------------------------------------------------------ */

#define EDIT_HEX_BYTES_PER_ROW  16    /* bytes per displayed row        */
#define EDIT_HEX_ADDR_CHARS      7    /* "XXXXXX:" (6 digits + colon)   */
#define EDIT_HEX_ADDR_PAD        1    /* space between colon and hex    */
/* Total address column width = (EDIT_HEX_ADDR_CHARS+EDIT_HEX_ADDR_PAD)
 * characters = 8 cells.                                                */

/* Hex area: "XX " × 8 + " " + "XX " × 8 = 49 chars                    */
#define EDIT_HEX_HEX_CHARS      49

/* Separator between hex and ASCII: " | "                               */
#define EDIT_HEX_SEP_CHARS       3

/* ASCII area: 16 chars                                                  */
#define EDIT_HEX_ASCII_CHARS    EDIT_HEX_BYTES_PER_ROW

/* Maximum file size that can be loaded (16 MB)                         */
#define EDIT_HEX_MAX_SIZE       (16u * 1024u * 1024u)

/* ------------------------------------------------------------------
 * Editing zone
 * ------------------------------------------------------------------ */

typedef enum edit_hex_zone_e
{
    HEX_ZONE_HEX   = 0,   /* cursor in hex columns                     */
    HEX_ZONE_ASCII = 1    /* cursor in ASCII column                     */
} edit_hex_zone_t;

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct edit_hex_view_s
{
    gui_window_t      hWnd;
    renderer_t        hRenderer;

    char              szPath[ST_MAX_PATH]; /* Absolute path             */
    st_u8_t          *pData;              /* File content (heap)        */
    size_t            uiSize;             /* File size in bytes         */

    size_t            uiCursor;           /* Byte offset of cursor      */
    int               iNibble;            /* 0=high, 1=low (HEX zone)   */
    edit_hex_zone_t   eZone;              /* Active editing zone        */

    int               iScrollRow;         /* First visible row index    */

    st_bool_t         bDirty;             /* Unsaved changes            */

    int               iWndWidth;          /* Client area width  (px)    */
    int               iWndHeight;         /* Client area height (px)    */
    int               iCellW;            /* Monospace cell width (px)  */
    int               iCellH;            /* Monospace cell height (px) */

    /* Pre-computed layout X positions (px), set on first paint        */
    int               iAddrX;            /* Start of address column    */
    int               iHexX;             /* Start of hex area          */
    int               iAsciiX;           /* Start of ASCII area        */

    line_context_t   *ptLineCtx;
} edit_hex_view_t;

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

/*
 * edit_hex_open() - Load a binary file and open the hex editor window.
 *
 * Reads the entire file into a heap buffer.  The window runs in a
 * dedicated thread; edit_hex_open() returns once the window is live.
 *
 * Parameters:
 *   szPath     [in]  : Absolute or relative path to the file.
 *   ptLineCtx  [in]  : Console context (trace feedback on save/close).
 *   pptView    [out] : Receives the allocated view context.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL, file cannot be read,
 *               file exceeds EDIT_HEX_MAX_SIZE, or window fails.
 */
st_error_t edit_hex_open(const char       *szPath,
                          line_context_t   *ptLineCtx,
                          edit_hex_view_t **pptView);

/*
 * edit_hex_close() - Close the editor window and release resources.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t edit_hex_close(edit_hex_view_t **pptView);

#endif /* EDIT_HEX_H */
