/*
 * dir.h - ST4Ever directory tree view
 *
 * Implements the `dir` command: opens a Win32/X11 window showing an
 * indented, expandable ASCII tree of the target directory.
 *
 * Lazy loading (R4 variant):
 *   Only the root's direct children are loaded on dir_open().  Each
 *   sub-directory's children are loaded on first expand (bChildrenLoaded
 *   = ST_FALSE until the user clicks/presses ENTER on the entry).
 *
 * Flat render list:
 *   dir_flat_rebuild() converts the currently visible (expanded) tree
 *   into a flat array of dir_flat_entry_t.  O(1) hit-testing and
 *   O(n-visible) rendering.  Rebuilt after every expand/collapse.
 *
 * ASCII tree style (RENDERER_FONT_MONO, each depth level = 4 chars):
 *
 *   ..
 *   +-- subdir/           depth 0, not-last, collapsed
 *   |   +-- child.prg     depth 1, not-last
 *   |   \-- data.bin      depth 1, last
 *   \-- readme.txt        depth 0, last (selected = highlight rect)
 *
 * Selection (iSelectedFlat in dir_view_t):
 *   -2 = nothing selected
 *   -1 = ".." row selected
 *  >=0 = aptFlat[iSelectedFlat] selected
 *
 * On ENTER / left-click:
 *   directory -> expand (lazy load) or collapse
 *   file      -> write ptLineCtx->szSelected
 *   ".."      -> reload tree from parent directory
 *
 * Scroll: iScrollOffset = index of first visible row (0 = ".." row).
 * Navigation: arrow keys / Page Up-Down / Home / End + mouse wheel.
 *
 * R18: window title updated on open and on navigation:
 *   "ST4Ever - Dir: <current root path>"
 *
 * UC3.3: full implementation.
 */

#ifndef DIR_H
#define DIR_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "line.h"

/* ------------------------------------------------------------------
 * Flat list capacity
 * ------------------------------------------------------------------ */

#define DIR_FLAT_MAX  4096   /* max visible (expanded) tree entries   */

/* ------------------------------------------------------------------
 * Tree node
 * ------------------------------------------------------------------ */

typedef struct dir_node_s
{
    char               szName[ST_MAX_PATH]; /* Entry name (not path)  */
    char               szPath[ST_MAX_PATH]; /* Full absolute path      */
    st_bool_t          bIsDir;              /* ST_TRUE if directory    */
    st_bool_t          bExpanded;           /* Expanded in tree view   */
    st_bool_t          bChildrenLoaded;     /* ST_FALSE = lazy, unscan */
    int                iDepth;              /* Depth (-1 for root)     */
    struct dir_node_s *ptParent;            /* Parent node (NULL=root) */
    struct dir_node_s *ptFirstChild;        /* First child node        */
    struct dir_node_s *ptNextSibling;       /* Next sibling            */
} dir_node_t;

/* ------------------------------------------------------------------
 * Flat render list entry
 *
 * Built by dir_flat_rebuild() via a depth-first traversal of the
 * currently expanded tree.  Stores everything needed for O(1) prefix
 * generation and hit-testing.
 *
 * ASCII prefix construction (dir_build_prefix):
 *   for i in 0..iDepth-1:
 *     if bit i of uiLastMask → "    " (ancestor i was last sibling)
 *     else                   → "|   " (vertical continuation line)
 *   connector: bLastSibling ? "\\-- " : "+-- "
 * ------------------------------------------------------------------ */

typedef struct dir_flat_entry_s
{
    dir_node_t *ptNode;
    int         iDepth;        /* 0 = direct child of root             */
    st_bool_t   bLastSibling;  /* last child of its parent             */
    st_u32_t    uiLastMask;    /* bit i = 1 → ancestor at depth i was  */
                               /* the last sibling (max depth 31)       */
} dir_flat_entry_t;

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct dir_view_s
{
    gui_window_t      hWnd;                  /* GUI window handle      */
    renderer_t        hRenderer;             /* Renderer (created lazy)*/
    dir_node_t       *ptRoot;                /* Tree root (depth = -1) */
    line_context_t   *ptLineCtx;             /* Back-ref for selection */
    dir_flat_entry_t *aptFlat;               /* Heap-allocated flat arr*/
    int               iFlatCount;            /* Entries in aptFlat     */
    int               iScrollOffset;         /* First visible row index*/
    int               iSelectedFlat;         /* -2=none,-1="..",>=0=idx*/
    int               iWndWidth;             /* Client area width (px) */
    int               iWndHeight;            /* Client area height (px)*/
    int               iCellW;               /* Monospace cell width   */
    int               iCellH;               /* Monospace cell height  */
    st_bool_t         bShowHidden;           /* Show '.*' entries (P15)*/
    char              szRootPath[ST_MAX_PATH];
} dir_view_t;

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

/*
 * dir_open() - Open the directory tree view window.
 *
 * Loads the root's direct children immediately.  Sub-directories are
 * loaded lazily on first expand.  The window runs in a dedicated
 * thread (gui_open_window); dir_open() returns once the window is
 * visible.
 *
 * Parameters:
 *   szPath      [in]  : Root path (NULL or "" = cwd).
 *   ptLineCtx   [in]  : Console context (szSelected updated on file pick).
 *   bShowHidden [in]  : ST_TRUE to show '.*' entries (P15 / dir -a).
 *   pptView     [out] : Receives the allocated view context.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptLineCtx or pptView is NULL, or window fails.
 */
st_error_t dir_open(const char     *szPath,
                     line_context_t *ptLineCtx,
                     st_bool_t       bShowHidden,
                     dir_view_t    **pptView);

/*
 * dir_close() - Close the view window and release all resources.
 *
 * Waits for the window thread to exit (gui_close_window), then frees
 * the tree, the flat list, and the view context itself.
 *
 * Parameters:
 *   pptView [in/out] : View to close; set to NULL on return.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptView is NULL.
 */
st_error_t dir_close(dir_view_t **pptView);

#endif /* DIR_H */
