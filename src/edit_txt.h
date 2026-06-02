/*
 * edit_txt.h - ST4Ever text / source editor view
 *
 * Opens a Direct2D window showing the content of a text file with:
 *   - line number gutter (right-aligned, auto-width)
 *   - cursor (vertical bar on current line)
 *   - rectangular selection (anchor + cursor end points)
 *   - horizontal + vertical scroll
 *   - tab expansion (EDIT_TXT_TAB_WIDTH spaces per tab stop)
 *
 * Keyboard bindings:
 *   Arrow keys   : cursor movement
 *   Home / End   : beginning / end of line
 *   PgUp / PgDn  : scroll one screen
 *   CTRL+Home    : go to first line
 *   CTRL+End     : go to last line
 *   CTRL+Left    : word left
 *   CTRL+Right   : word right
 *   SHIFT+any    : extend selection
 *   CTRL+A       : select all
 *   CTRL+C       : copy selection to clipboard
 *   CTRL+X       : cut selection to clipboard
 *   CTRL+V       : paste from clipboard
 *   CTRL+S       : save file
 *   Tab          : insert tab character
 *   Enter        : insert newline (split line)
 *   Backspace    : delete char before cursor (or merge lines)
 *   Delete       : delete char at cursor  (or merge lines)
 *   Printable    : insert at cursor
 *   ESC          : close view (unsaved changes noted in trace)
 *
 * Title: "ST4Ever - Edit: <filename> [*]"  ([*] when dirty)
 *
 * R18: title updated on open and on every save.
 * UC8: full implementation.
 */

#ifndef EDIT_TXT_H
#define EDIT_TXT_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "line.h"

/* ------------------------------------------------------------------
 * Limits
 * ------------------------------------------------------------------ */

#define EDIT_TXT_TAB_WIDTH    4       /* visual spaces per tab stop      */
#define EDIT_TXT_MAX_LINE_LEN 4096    /* bytes per line (hard cap)       */
#define EDIT_TXT_CLIP_MAX     65536   /* clipboard buffer (64 KB)        */
#define EDIT_TXT_UNDO_LEVELS  20      /* undo ring depth                 */

/* ------------------------------------------------------------------
 * Text cursor / selection position
 * ------------------------------------------------------------------ */

typedef struct edit_pos_s
{
    int iLine;   /* 0-based line index   */
    int iCol;    /* 0-based byte offset  */
} edit_pos_t;

/* ------------------------------------------------------------------
 * Undo ring entry
 * ------------------------------------------------------------------ */

typedef struct edit_undo_entry_s
{
    char     **aszLines;   /* Heap copy of all lines (NULL = empty slot) */
    int        iLineCount;
    edit_pos_t tCursor;
} edit_undo_entry_t;

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct edit_txt_view_s
{
    gui_window_t    hWnd;
    renderer_t      hRenderer;

    char            szPath[ST_MAX_PATH]; /* Absolute path of file        */
    char          **aszLines;            /* Heap array of heap strings   */
    int             iLineCount;          /* >= 1 (empty file = 1 line)   */
    int             iLineAlloc;          /* Allocated slots in aszLines  */

    edit_pos_t      tCursor;             /* Current insertion point      */
    int             iScrollLine;         /* Index of first visible line  */
    int             iScrollCol;          /* First visible display column */

    int             iSelAnchorLine;      /* Selection anchor; -1 = none  */
    int             iSelAnchorCol;

    st_bool_t       bDirty;              /* Unsaved changes              */

    edit_undo_entry_t aUndo[EDIT_TXT_UNDO_LEVELS]; /* Undo ring         */
    int               iUndoHead;         /* Next write slot (ring)       */
    int               iUndoCount;        /* Valid entries in ring        */
    st_bool_t         bUndoGroupInsert;  /* TRUE = last op was a char    */
                                         /* insert; group until broken   */

    int             iWndWidth;           /* Client area width  (px)      */
    int             iWndHeight;          /* Client area height (px)      */
    int             iCellW;             /* Monospace cell width  (px)   */
    int             iCellH;             /* Monospace cell height (px)   */
    int             iGutterW;           /* Line-number gutter width (px)*/

    line_context_t *ptLineCtx;
} edit_txt_view_t;

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

/*
 * edit_txt_open() - Load a text file and open the editor window.
 *
 * The window runs in a dedicated thread; edit_txt_open() returns once
 * the window is live.  szPath must point to an existing file.
 *
 * Parameters:
 *   szPath     [in]  : Absolute or relative path to the file.
 *   ptLineCtx  [in]  : Console context (trace feedback on save/close).
 *   pptView    [out] : Receives the allocated view context.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL, file cannot be read,
 *               or the window thread fails to start.
 */
st_error_t edit_txt_open(const char       *szPath,
                          line_context_t   *ptLineCtx,
                          edit_txt_view_t **pptView);

/*
 * edit_txt_close() - Close the editor window and release resources.
 *
 * Joins the window thread then frees all line buffers and the view
 * itself.  Sets *pptView to NULL on return.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t edit_txt_close(edit_txt_view_t **pptView);

#endif /* EDIT_TXT_H */
