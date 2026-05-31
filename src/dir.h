/*
 * dir.h - ST4Ever directory tree view
 *
 * Implements the `dir` command: opens a Win32/X11 window showing an
 * indented, expandable tree of the target directory.  A selected
 * file/directory is stored in line_context_t.szSelected and becomes
 * the default argument for load, edit, mount, image.
 *
 * TODO(UC3): Implement using gui.h / renderer.h.
 */

#ifndef DIR_H
#define DIR_H

#include "common.h"
#include "gui.h"
#include "renderer.h"
#include "line.h"

/* ------------------------------------------------------------------
 * Tree node
 * ------------------------------------------------------------------ */

typedef struct dir_node_s
{
    char               szName[ST_MAX_PATH]; /* Entry name (not path)  */
    char               szPath[ST_MAX_PATH]; /* Full path              */
    st_bool_t          bIsDir;              /* ST_TRUE if directory   */
    st_bool_t          bExpanded;           /* Expanded in tree view  */
    struct dir_node_s *ptParent;            /* Parent node (NULL=root)*/
    struct dir_node_s *ptFirstChild;        /* First child node       */
    struct dir_node_s *ptNextSibling;       /* Next sibling           */
} dir_node_t;

/* ------------------------------------------------------------------
 * View context
 * ------------------------------------------------------------------ */

typedef struct dir_view_s
{
    gui_window_t    hWnd;                  /* GUI window handle       */
    renderer_t      hRenderer;             /* Renderer handle         */
    dir_node_t     *ptRoot;                /* Tree root node          */
    dir_node_t     *ptSelected;            /* Currently selected node */
    line_context_t *ptLineCtx;             /* Back-ref for selection  */
    int             iScrollOffset;         /* Vertical scroll in rows */
    char            szRootPath[ST_MAX_PATH];
} dir_view_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * dir_open() - Open the directory tree view.
 *
 * Parameters:
 *   szPath     [in]  : Root path to display (NULL = cwd).
 *   ptLineCtx  [in]  : Console context (selection is written here).
 *   pptView    [out] : Receives the created view context.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on failure.
 */
st_error_t dir_open(const char     *szPath,
                     line_context_t *ptLineCtx,
                     dir_view_t    **pptView);

/*
 * dir_close() - Close the view and release resources.
 *
 * Parameters:
 *   pptView [in/out] : View to close (set to NULL on return).
 *
 * Returns: ST_NO_ERROR.
 */
st_error_t dir_close(dir_view_t **pptView);

#endif /* DIR_H */
