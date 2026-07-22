/*
 * dir.c - ST4Ever directory tree view
 *
 * UC3.3: lazy-load N-ary tree + flat render list + ASCII tree style
 *        + D2D rendering + keyboard/mouse/scroll/selection.
 *
 * Threading model:
 *   dir_open() runs on the console thread:
 *     1. Allocates dir_view_t and aptFlat, loads root children (one
 *        level, dirs-first then files, FindFirst order).
 *     2. Calls gui_open_window() which spawns the window thread and
 *        blocks until the HWND is live (UC3.1 contract).
 *   The window thread calls dir_event_callback() for all GUI events.
 *   The console thread must not touch ptView after gui_open_window()
 *   and before dir_close() (which joins the window thread).
 *
 * ASCII tree (RENDERER_FONT_MONO, 4-char indent per depth level):
 *   ".." row always first.
 *   Each entry: <prefix><indicator><name>
 *   Prefix: depth×4 chars of continuation lines then 4-char connector.
 *   Indicator: "[+] " / "[-] " for dirs, "" for files.
 *
 * File selection (written to ptLineCtx->szSelected on ENTER/SPACE):
 *   Note: write from window thread; UC4+ will add proper locking.
 */

#include "dir.h"
#include "line.h"
#include "gui.h"
#include "trace.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef ST_PLATFORM_WINDOWS
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <direct.h>
#  define dir_getcwd _getcwd
#else
#  include <unistd.h>
#  include <dirent.h>
#  include <sys/stat.h>
#  define dir_getcwd getcwd
#endif

/* ------------------------------------------------------------------
 * Colour palette (static to avoid repeated compound-literal allocs)
 * ------------------------------------------------------------------ */

static const renderer_color_t g_dir_clrBg       = {0.09f, 0.09f, 0.14f, 1.0f};
static const renderer_color_t g_dir_clrSel      = {0.20f, 0.30f, 0.65f, 1.0f};
static const renderer_color_t g_dir_clrLastSel  = {0.04f, 0.28f, 0.10f, 1.0f};
static const renderer_color_t g_dir_clrMultiSel = {0.28f, 0.15f, 0.55f, 1.0f};
static const renderer_color_t g_dir_clrDir      = {0.00f, 0.85f, 0.85f, 1.0f};
static const renderer_color_t g_dir_clrFile     = {0.80f, 0.80f, 0.80f, 1.0f};
static const renderer_color_t g_dir_clrDD       = {0.95f, 0.90f, 0.10f, 1.0f};

/* ------------------------------------------------------------------
 * Global CPU Context - there is only one CPU in the ATARI ST
 * ------------------------------------------------------------------ */

 static dir_context_t g_dir_ptCtx = {.ulMagic = 0xCAFEDECA,
                                     .eObject = ST_DIR_CTX};

/* ------------------------------------------------------------------
 * Forward declarations (internal helpers)
 * ------------------------------------------------------------------ */

static void        dir_node_free_tree(dir_node_t *ptNode);
static st_error_t  dir_node_load_children(dir_node_t *ptNode,
                                           st_bool_t   bShowHidden);
static void        dir_node_reload_children(dir_node_t *ptNode,
                                             st_bool_t   bShowHidden);
static void        dir_collect_expanded(dir_node_t *ptNode,
                                         char       (*aaszPaths)[ST_MAX_PATH],
                                         int        *piCount,
                                         int         iMaxPaths);
static void        dir_reexpand_path(dir_node_t *ptRoot,
                                      const char *szPath,
                                      st_bool_t   bShowHidden);
static void        dir_refresh_tree(dir_view_t *ptView);

static void dir_flat_rebuild(dir_view_t *ptView);
static void dir_flat_rebuild_rec(dir_view_t *ptView,
                                  dir_node_t *ptNode,
                                  int         iDepth,
                                  st_bool_t   bLastSibling,
                                  st_u32_t    uiLastMask);

static void dir_build_prefix(const dir_flat_entry_t *ptEntry,
                               char                   *szBuf,
                               size_t                  uiMax);

static void dir_get_parent_path(const char *szPath,
                                  char       *szOut,
                                  size_t      uiMax);

static void dir_update_title(dir_view_t *ptView, gui_window_t hWnd);

static void dir_scroll_to_sel(dir_view_t *ptView);
static void dir_activate_sel(dir_view_t *ptView, gui_window_t hWnd);
static void dir_navigate_up(dir_view_t *ptView, gui_window_t hWnd);

/* P11: commit a file selection to the console + last-selected indicator
 * (shared by dir_activate_sel's ENTER path and dir_handle_key's SPACE) */
static void dir_commit_file_selection(dir_view_t       *ptView,
                                        const dir_node_t *ptNode);

/* P10: nav history helpers */
static void dir_nav_history_push(dir_view_t *ptView, const char *szNewPath);
static void dir_navigate_to(dir_view_t  *ptView, const char *szNewPath,
                              gui_window_t hWnd);

/* P14: multi-selection helpers */
static st_bool_t dir_is_multi_sel(const dir_view_t *ptView, const char *szPath);
static void      dir_toggle_multi_sel(dir_view_t *ptView, const char *szPath);

static void dir_render(dir_view_t *ptView);
static void dir_handle_key(dir_view_t  *ptView,
                             gui_window_t hWnd,
                             gui_key_t    eKey,
                             st_u8_t      uiMods);
static void dir_handle_click(dir_view_t  *ptView,
                               gui_window_t hWnd,
                               int          iX,
                               int          iY);
static void dir_handle_scroll(dir_view_t  *ptView,
                                gui_window_t hWnd,
                                int          iDelta);

static void dir_event_callback(gui_window_t  hWnd,
                                 gui_event_t  *ptEvent,
                                 void         *pUserCtx);

#define DIR_REFRESH_MAX_EXP  256  /* max expanded dirs to remember     */

/* ==================================================================
 * Tree node helpers
 * ================================================================== */

/*
 * dir_node_free_tree() - free allocated memory of dir_node_t structure,
 *                        recursively called for the children of the node
 *
 * Parameters:
 *   ptNode [IN] : the node and subsequent children to free
 *
 * Returns:
 *   Void - None
 *
 * This helper is currently called by:
 * - dir_node_load_children() - run when loading a tree
 * - dir_node_reload_children() - run when F5 or 'H' refresh the view
 * - dir_navigate_to() - run when navigating to new root
 * - dir_open() - run when opening a new 'dir' GUI window
 * - dir_close() - run when closing the view 
 */
static void dir_node_free_tree(dir_node_t *ptNode)
{
    dir_node_t *ptChild;
    dir_node_t *ptNext;

    // No LOG_TRACE - R22: called recursively for each node suppression

    if (ptNode == NULL)
    {
        return;
    }

    /* -- [DIR]45. Free a node tree, including its children recursively -- */
    ptChild = ptNode->ptFirstChild;
    while (ptChild != NULL)
    {
        ptNext = ptChild->ptNextSibling;
        dir_node_free_tree(ptChild);
        ptChild = ptNext;
    }
    mem_free(ptNode, &g_dir_ptCtx.uiFreeObjs[ST_DIR_NODE_T]);
}

/*
 * dir_node_load_children() - Scan the filesystem and attach children.
 *
 * Performs two passes: directories first, then files.  Within each
 * pass, entries are appended in FindFirstFile order (NTFS: typically
 * alphabetical).  Sets ptNode->bChildrenLoaded = ST_TRUE on return.
 *
 * Parameters:
 *   ptNode [IN]      : pointer to the node to load
 *   bShowHidden [IN] : optionally show/hide files/dirs beginning with '.'
 *
 * Returns:
 *   ST_NO_ERROR if successful, ST_ERROR if an error occurs
 *
 * A scan error (access denied, path not found) is non-fatal: the node
 * is marked loaded with zero children and ST_NO_ERROR is returned.
 */
static st_error_t dir_node_load_children(dir_node_t *ptNode,
                                          st_bool_t   bShowHidden)
{
    dir_node_t *ptLast;
    dir_node_t *ptChild;

    /* -- [DIR]48. A NULL node returns an error -- */
    if (ptNode == NULL)
    {
        LOG_ERROR("ptNode is NULL");
        return ST_ERROR;
    }

    ptLast = NULL;

#ifdef ST_PLATFORM_WINDOWS
    {
        WIN32_FIND_DATAA tFd;
        HANDLE           hFind;
        char             szPat[ST_MAX_PATH + 3]; /* +3 for "\\*\0" */
        st_bool_t        bIsDir;
        int              iPass;
        int              iPathLen;

        snprintf(szPat, sizeof(szPat), "%s\\*", ptNode->szPath);

        /* -- [DIR]35. Windows two-pass scan lists directories before files -- */
        /* Two passes: 0 = dirs, 1 = files */
        for (iPass = 0; iPass < 2; iPass++)
        {
            /* -- [DIR]49. Access denied or empty entries leaves the GUI empty -- */
            hFind = FindFirstFileA(szPat, &tFd);
            if (hFind == INVALID_HANDLE_VALUE)
            {
                break; /* access denied or no entries — non-fatal */
            }

            do
            {
                bIsDir = (tFd.dwFileAttributes
                          & FILE_ATTRIBUTE_DIRECTORY) != 0;

                if (strcmp(tFd.cFileName, ".")  == 0) continue;
                if (strcmp(tFd.cFileName, "..") == 0) continue;
                
                /* -- [DIR]9. P15: '.*' entries are skipped unless bShowHidden is set -- */
                if (bShowHidden == ST_FALSE
                &&  tFd.cFileName[0] == '.') continue;

                /* Pass 0: dirs only; pass 1: files only */
                if (iPass == 0 && !bIsDir) continue;
                if (iPass == 1 &&  bIsDir) continue;

                ptChild = (dir_node_t*)mem_alloc(ST_DIR_NODE_T, 
                                           &g_dir_ptCtx.uiAllocObjs[ST_DIR_NODE_T]);
                if (ptChild == NULL)
                {
                    FindClose(hFind);
                    ptNode->bChildrenLoaded = ST_TRUE;
                    return ST_ERROR;
                }

                /* -- [DIR]46. Entries with path too long are skipped -- */
                strncpy(ptChild->szName, tFd.cFileName,
                        ST_MAX_PATH - 1);
                iPathLen = snprintf(ptChild->szPath, ST_MAX_PATH,
                                    "%s\\%s",
                                    ptNode->szPath, tFd.cFileName);
                if (iPathLen < 0 || iPathLen >= (int)ST_MAX_PATH)
                {
                    /* Resulting path too long: skip this entry */
                    dir_node_free_tree(ptChild);
                    continue;
                }
                ptChild->bIsDir   = (st_bool_t)bIsDir;
                ptChild->ptParent = ptNode;
                ptChild->iDepth   = ptNode->iDepth + 1;

                if (ptNode->ptFirstChild == NULL)
                    ptNode->ptFirstChild = ptChild;
                else
                    ptLast->ptNextSibling = ptChild;
                ptLast = ptChild;
            }
            while (FindNextFileA(hFind, &tFd));

            FindClose(hFind);
        }
    }
#endif

    ptNode->bChildrenLoaded = ST_TRUE;
    return ST_NO_ERROR;
}

/*
 * dir_node_reload_children() - Free and rescan a node's direct children.
 *
 * Used by dir_refresh_tree() (P21 hidden toggle and P22 F5 refresh both
 * go through it to preserve expansion state).  The node is left
 * expanded only if it was already expanded; children come back with
 * bExpanded=ST_FALSE so sub-trees collapse implicitly.
 *
 * Parameters:
 *   ptNode [IN]      : pointer to the node to load
 *   bShowHidden [IN] : optionally show/hide files/dirs beginning with '.'
 *
 * Returns:
 *  void - None
 *
 */
static void dir_node_reload_children(dir_node_t *ptNode,
                                      st_bool_t   bShowHidden)
{
    dir_node_t *ptChild;
    dir_node_t *ptNext;

    /* -- [DIR]50. Do nothing if the input node is NULL -- */
    if (ptNode == NULL)
    {
        return;
    }

    /* -- [DIR]47. A reload frees all existing nodes and rescan the children -- */
    /* This helper is currently called by:
     * - dir_refresh_tree() - tested when F5 or 'H' refreshes the children
     */
    
     /* Free all existing children (recursively) */
    ptChild = ptNode->ptFirstChild;
    while (ptChild != NULL)
    {
        ptNext = ptChild->ptNextSibling;
        dir_node_free_tree(ptChild);
        ptChild = ptNext;
    }
    ptNode->ptFirstChild    = NULL;
    ptNode->bChildrenLoaded = ST_FALSE;

    /* Rescan */
    dir_node_load_children(ptNode, bShowHidden);
}

/*
 * dir_collect_expanded() - DFS walk: collect szPath of every expanded dir.
 *
 * Called before reloading the tree so we can restore expansion state.
 * Only expanded dirs (bExpanded==ST_TRUE) with loaded children are stored.
 *
 * Parameters:
 *   ptNode    [IN]   : pointer to the node to load
 *   aaszPaths [IN/OUT]  : arrays of expanded paths
 *   piCount   [IN/OUT]  : number of expanded paths
 *   iMaxPaths [IN]   : Max expanded paths to collect -  DIR_REFRESH_MAX_EXP
 *
 * Returns:
 *  void - None
 *
 */
static void dir_collect_expanded(dir_node_t *ptNode,
                                   char       (*aaszPaths)[ST_MAX_PATH],
                                   int        *piCount,
                                   int         iMaxPaths)
{
    dir_node_t *ptChild;

    /* -- [DIR]51. Do nothing in case of NULL parameter or negative iMaxPaths -- */
    if (ptNode == NULL || aaszPaths == NULL
    ||  piCount == NULL || iMaxPaths <= 0)
    {
        return;
    }

    /* -- [DIR]52. Collect the already expanded directories -- */
    /* This helper is currently called by:
     * - dir_refresh_tree() - tested when F5 or 'H' refreshes the children
     */
    for (ptChild = ptNode->ptFirstChild;
         ptChild != NULL;
         ptChild = ptChild->ptNextSibling)
    {
        if (!ptChild->bIsDir || !ptChild->bExpanded
        ||  !ptChild->bChildrenLoaded)
        {
            continue;
        }

        if (*piCount < iMaxPaths)
        {
            strncpy(aaszPaths[*piCount], ptChild->szPath, ST_MAX_PATH - 1);
            aaszPaths[*piCount][ST_MAX_PATH - 1] = '\0';
            (*piCount)++;
        }

        /* Recurse into expanded subtrees */
        dir_collect_expanded(ptChild, aaszPaths, piCount, iMaxPaths);
    }
}

/*
 * dir_reexpand_path() - Navigate the refreshed tree and expand one path.
 *
 * Splits szPath into components relative to ptRoot->szPath and walks
 * down the tree, loading children and setting bExpanded=ST_TRUE at
 * each level.  Silently stops if a component is not found (the dir
 * was deleted or renamed during the refresh).
 *
 * Parameters:
 *   ptRoot      [IN] : root of the freshly reloaded tree to walk
 *   szPath      [IN] : absolute path (under ptRoot->szPath) to re-expand
 *   bShowHidden [IN] : forwarded to dir_node_load_children() for any
 *                      lazy-load triggered while walking down
 *
 * Returns:
 *   Void - None
 */
static void dir_reexpand_path(dir_node_t *ptRoot,
                                const char *szPath,
                                st_bool_t   bShowHidden)
{
    const char *pRel;
    const char *pStart;
    const char *pSep;
    char        szComp[ST_MAX_PATH];
    size_t      uiLen;
    dir_node_t *ptNode;
    dir_node_t *ptChild;

    /* -- [DIR]53. dir_reexpand_path rejects a NULL ptRoot or szPath -- */
    if (ptRoot == NULL || szPath == NULL) return;

    uiLen = strlen(ptRoot->szPath);
    if (strncmp(szPath, ptRoot->szPath, uiLen) != 0) return;

    pRel = szPath + uiLen;
    if (*pRel == '/' || *pRel == '\\') pRel++;

    ptNode = ptRoot;
    pStart = pRel;

    /* -- [DIR]54. Reexpand the previously saved nodes -- */
    /* This helper is currently called by:
     *  - dir_refresh_tree() (Phase 3, [DIR]37) - tested when F5 or 'H'
     *    refreshes the view while visible_dir stays expanded
     *    (TC-DIR-067..076) */

    while (*pStart != '\0')
    {
        /* Advance to next path separator or end */
        pSep = pStart;
        while (*pSep != '\0' && *pSep != '/' && *pSep != '\\')
        {
            pSep++;
        }

        uiLen = (size_t)(pSep - pStart);
        if (uiLen == 0 || uiLen >= ST_MAX_PATH)
        {
            break;
        }

        memcpy(szComp, pStart, uiLen);
        szComp[uiLen] = '\0';

        /* Find child with matching name */
        ptChild = NULL;
        for (ptChild = ptNode->ptFirstChild;
             ptChild != NULL;
             ptChild = ptChild->ptNextSibling)
        {
            if (ptChild->bIsDir && strcmp(ptChild->szName, szComp) == 0)
            {
                break;
            }
        }

        if (ptChild == NULL) break; /* dir deleted or renamed */

        if (ptChild->bChildrenLoaded == ST_FALSE)
        {
            dir_node_load_children(ptChild, bShowHidden);
        }
        ptChild->bExpanded = ST_TRUE;

        ptNode = ptChild;
        pStart = (*pSep != '\0') ? pSep + 1 : pSep;
    }
}

/*
 * dir_refresh_tree() - Reload root children while preserving expansion state.
 *
 * Phase 1: collect all currently expanded paths (DFS).
 * Phase 2: reload root's direct children (frees old tree).
 * Phase 3: re-expand each saved path by navigating the new tree.
 *
 * Expanded dirs that no longer exist after the reload are silently
 * ignored (dir_reexpand_path is a no-op when a component is missing).
 */


/* -- [DIR]37. dir_refresh_tree: reloads the root's children while preserving previously expanded subtree paths (3-phase: collect / reload / re-expand) -- */
static void dir_refresh_tree(dir_view_t *ptView)
{
    static char aaszExpanded[DIR_REFRESH_MAX_EXP][ST_MAX_PATH];
    int         iExpCount;
    int         iIdx;

    if (ptView == NULL || ptView->ptRoot == NULL)
    {
        return;
    }

    /* Phase 1: save expanded paths */
    iExpCount = 0;
    dir_collect_expanded(ptView->ptRoot, aaszExpanded,
                          &iExpCount, DIR_REFRESH_MAX_EXP);

    /* Phase 2: reload root's direct children */
    dir_node_reload_children(ptView->ptRoot, ptView->bShowHidden);

    /* Phase 3: re-expand previously expanded dirs */
    for (iIdx = 0; iIdx < iExpCount; iIdx++)
    {
        dir_reexpand_path(ptView->ptRoot,
                           aaszExpanded[iIdx],
                           ptView->bShowHidden);
    }
}

/* ==================================================================
 * Flat list
 * ================================================================== */

/* -- [DIR]36. dir_flat_rebuild: rebuilds the flat render list from the currently expanded tree, skipping collapsed subtrees -- */
static void dir_flat_rebuild(dir_view_t *ptView)
{
    if (ptView == NULL || ptView->aptFlat == NULL) return;
    ptView->iFlatCount = 0;
    dir_flat_rebuild_rec(ptView, ptView->ptRoot, -1, ST_FALSE, 0u);
}

/*
 * dir_flat_rebuild_rec() - Depth-first recursion for dir_flat_rebuild().
 *
 * Parameters:
 *   ptView       [IN/OUT] : view whose aptFlat[]/iFlatCount is appended to
 *   ptNode       [IN]     : node to visit (root is called with iDepth=-1
 *                           so the root itself is skipped, not recorded)
 *   iDepth       [IN]     : depth of ptNode in the tree (0 = root's child)
 *   bLastSibling [IN]     : ST_TRUE if ptNode is the last child of its parent
 *   uiLastMask   [IN]     : bit i set = ancestor at depth i was a last child
 *                           (used by dir_build_prefix for continuation bars)
 *
 * Returns:
 *   Void - None
 */
/* -- [DIR]55. dir_flat_rebuild_rec recursively walks the expanded tree in depth-first order, skipping collapsed subtrees, and records ASCII prefix bookkeeping per visible entry -- */
/* This helper is currently called by:
 *  - dir_flat_rebuild() ([DIR]36) - tested whenever iFlatCount changes
 *    across an expand/collapse/refresh (TC-DIR-033..036, 067..076) */
static void dir_flat_rebuild_rec(dir_view_t *ptView,
                                  dir_node_t *ptNode,
                                  int         iDepth,
                                  st_bool_t   bLastSibling,
                                  st_u32_t    uiLastMask)
{
    dir_node_t       *ptChild;
    dir_node_t       *ptLastChild;
    dir_flat_entry_t *ptEntry;
    st_u32_t          uiChildMask;

    if (ptView->iFlatCount >= DIR_FLAT_MAX) return;

    /* Add to flat list — skip root node itself (depth == -1) */
    if (iDepth >= 0)
    {
        ptEntry               = &ptView->aptFlat[ptView->iFlatCount];
        ptEntry->ptNode       = ptNode;
        ptEntry->iDepth       = iDepth;
        ptEntry->bLastSibling = bLastSibling;
        ptEntry->uiLastMask   = uiLastMask;
        ptView->iFlatCount++;
    }

    /* Recurse only into expanded dirs with loaded children */
    if (!ptNode->bIsDir
    ||  ptNode->bExpanded      == ST_FALSE
    ||  ptNode->bChildrenLoaded == ST_FALSE)
    {
        return;
    }

    /* Find last child (needed for bLastSibling flag) */
    ptLastChild = NULL;
    for (ptChild = ptNode->ptFirstChild; ptChild != NULL;
         ptChild = ptChild->ptNextSibling)
    {
        ptLastChild = ptChild;
    }
    if (ptLastChild == NULL) return;

    /*
     * uiLastMask for children: set bit at current depth if this node
     * is last (means no vertical continuation line from this level).
     */
    if (iDepth >= 0)
    {
        uiChildMask = uiLastMask
                    | (bLastSibling ? (1u << (unsigned)iDepth) : 0u);
    }
    else
    {
        uiChildMask = 0u; /* root has no depth bit */
    }

    for (ptChild = ptNode->ptFirstChild; ptChild != NULL;
         ptChild = ptChild->ptNextSibling)
    {
        dir_flat_rebuild_rec(ptView,
                             ptChild,
                             iDepth + 1,
                             (st_bool_t)(ptChild == ptLastChild),
                             uiChildMask);
    }
}

/* ==================================================================
 * ASCII prefix generation
 * ================================================================== */

/*
 * dir_build_prefix() - Build the ASCII tree prefix for a flat entry.
 *
 * Result format example for depth 2, not-last, parent-0 was not-last,
 * parent-1 was last:
 *   "|       +-- "   (8 + 4 = 12 chars, but depth=2 → "|   " "    " "+-- ")
 *   Actually: depth=2 → loop i=0,1 then connector:
 *     i=0: bit0 of uiLastMask=0 → "|   "
 *     i=1: bit1 of uiLastMask=1 → "    "
 *     connector: "+-- "
 *   Result: "|       +-- "  (12 chars before name)
 *
 * Parameters:
 *   ptEntry [IN]  : flat entry (iDepth/bLastSibling/uiLastMask) to render
 *   szBuf   [OUT] : buffer receiving the NUL-terminated prefix string
 *   uiMax   [IN]  : size of szBuf in bytes
 *
 * Returns:
 *   Void - None
 */
/* -- [DIR]56. dir_build_prefix builds the ASCII tree connector prefix (vertical continuation bars + branch connector) for one flat entry -- */
static void dir_build_prefix(const dir_flat_entry_t *ptEntry,
                               char                   *szBuf,
                               size_t                  uiMax)
{
    int i;

    szBuf[0] = '\0';

    /* Continuation lines for each ancestor level */
    for (i = 0; i < ptEntry->iDepth; i++)
    {
        if (ptEntry->uiLastMask & (1u << (unsigned)i))
            strncat(szBuf, "    ", uiMax - strlen(szBuf) - 1);
        else
            strncat(szBuf, "|   ", uiMax - strlen(szBuf) - 1);
    }

    /* Connector to this entry */
    if (ptEntry->bLastSibling)
        strncat(szBuf, "\\-- ", uiMax - strlen(szBuf) - 1);
    else
        strncat(szBuf, "+-- ", uiMax - strlen(szBuf) - 1);
}

/* ==================================================================
 * Path helpers
 * ================================================================== */

/*
 * dir_get_parent_path() - Compute the parent directory of szPath.
 *
 * Parameters:
 *   szPath [IN]  : path to compute the parent of
 *   szOut  [OUT] : buffer receiving the NUL-terminated parent path
 *   uiMax  [IN]  : size of szOut in bytes
 *
 * Returns:
 *   Void - None (szOut == szPath's trailing component stripped; szOut
 *   is left equal to szPath when szPath is already a filesystem root -
 *   callers such as dir_navigate_up() detect that by comparing the two)
 */
/* -- [DIR]57. dir_get_parent_path strips the trailing separator and truncates at the last path separator to compute the parent directory -- */
/* This helper is currently called by:
 *  - dir_navigate_up() ([DIR]62) - tested via ENTER on the ".." row
 *    (TC-DIR-077..080), which always goes from a directory nested
 *    several levels under the project root to its immediate parent */
static void dir_get_parent_path(const char *szPath,
                                  char       *szOut,
                                  size_t      uiMax)
{
    char  *pLastSep;
    char  *p;
    size_t uiLen;

    strncpy(szOut, szPath, uiMax - 1);
    szOut[uiMax - 1] = '\0';

    /* Strip trailing separator */
    uiLen = strlen(szOut);
    while (uiLen > 1
    &&    (szOut[uiLen - 1] == '/' || szOut[uiLen - 1] == '\\'))
    {
        szOut[--uiLen] = '\0';
    }

    /* Find last separator */
    pLastSep = NULL;
    for (p = szOut; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\') pLastSep = p;
    }

    if (pLastSep == NULL) return; /* already at root */

    /* -- [DIR]58. dir_get_parent_path: drive-root (C:\) and Unix-root (/) special cases stop the parent computation at the filesystem root -- */
#ifdef ST_PLATFORM_WINDOWS
    /* "C:\folder" → "C:\" — keep trailing separator at drive root */
    if (pLastSep == szOut + 2 && szOut[1] == ':')
    {
        szOut[3] = '\0'; /* keep "C:\" */
        return;
    }
#endif

    if (pLastSep == szOut)
    {
        szOut[1] = '\0'; /* Unix root "/" */
        return;
    }

    *pLastSep = '\0';
}

/*
 * dir_update_title() - Refresh the window title from the current root (R18).
 *
 * Parameters:
 *   ptView [IN] : view whose szRootPath supplies the title text
 *   hWnd   [IN] : window whose title bar is updated
 *
 * Returns:
 *   Void - None
 */
/* -- [DIR]59. dir_update_title sets the window title to 'ST4Ever - Dir: <root path>' (R18) -- */
static void dir_update_title(dir_view_t *ptView, gui_window_t hWnd)
{
    char szTitle[ST_MAX_PATH + 32];

    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Dir: %s", ptView->szRootPath);
    gui_set_title(hWnd, szTitle);
}

/* ==================================================================
 * Navigation / selection helpers  (called from window thread)
 * ================================================================== */

/*
 * dir_scroll_to_sel() - Scroll just enough to keep iSelectedFlat visible.
 *
 * Parameters:
 *   ptView [IN/OUT] : view whose iScrollOffset is adjusted in place
 *
 * Returns:
 *   Void - None
 */
static void dir_scroll_to_sel(dir_view_t *ptView)
{
    int iSelRow;
    int iVisRows;
    int iMaxScroll;
    int iTotalRows;

    /* -- [DIR]60. dir_scroll_to_sel: a non-positive iCellH (renderer not yet created) is a silent no-op -- */
    if (ptView->iCellH <= 0) return;

    iTotalRows = ptView->iFlatCount + 1; /* +1 for ".." */
    iVisRows   = ptView->iWndHeight / ptView->iCellH;
    if (iVisRows < 1) iVisRows = 1;
    iMaxScroll = iTotalRows - iVisRows;
    if (iMaxScroll < 0) iMaxScroll = 0;

    /* Convert iSelectedFlat to row index */
    iSelRow = (ptView->iSelectedFlat == -1) ? 0
                                             : ptView->iSelectedFlat + 1;

    /* -- [DIR]61. dir_scroll_to_sel scrolls iScrollOffset up or down just enough to keep the selected row visible -- */
    if (iSelRow < ptView->iScrollOffset)
    {
        ptView->iScrollOffset = iSelRow;
    }
    else if (iSelRow >= ptView->iScrollOffset + iVisRows)
    {
        ptView->iScrollOffset = iSelRow - iVisRows + 1;
    }

    if (ptView->iScrollOffset > iMaxScroll)
        ptView->iScrollOffset = iMaxScroll;
    if (ptView->iScrollOffset < 0)
        ptView->iScrollOffset = 0;
}

/* -- [DIR]39. dir_nav_history_push: appends a new path to the navigation history, truncating any forward entries -- */
/* P10: push szNewPath onto the navigation history, truncating forward
 * entries.  If the history is full, the oldest entry is evicted. */
static void dir_nav_history_push(dir_view_t *ptView, const char *szNewPath)
{
    int iNewHead;
    int i;

    /* Truncate forward history beyond current head */
    iNewHead = ptView->iNavHistHead + 1;

    if (iNewHead >= DIR_NAV_HIST_MAX)
    {
        /* Evict oldest entry (shift the ring left by one) */
        for (i = 0; i < DIR_NAV_HIST_MAX - 1; i++)
        {
            strncpy(ptView->aszNavHistory[i],
                    ptView->aszNavHistory[i + 1], ST_MAX_PATH - 1);
            ptView->aszNavHistory[i][ST_MAX_PATH - 1] = '\0';
        }
        iNewHead = DIR_NAV_HIST_MAX - 1;
    }

    strncpy(ptView->aszNavHistory[iNewHead], szNewPath, ST_MAX_PATH - 1);
    ptView->aszNavHistory[iNewHead][ST_MAX_PATH - 1] = '\0';
    ptView->iNavHistHead  = iNewHead;
    ptView->iNavHistCount = iNewHead + 1;
}

static void dir_navigate_to(dir_view_t  *ptView,
                              const char  *szNewPath,
                              gui_window_t hWnd)
{
    dir_node_t *ptNewRoot;

    /* -- [DIR]38. Navigates the tree root to a new path and free the old tree -- */
    ptNewRoot = (dir_node_t*)mem_alloc(ST_DIR_NODE_T, 
                                        &g_dir_ptCtx.uiAllocObjs[ST_DIR_NODE_T]);
    if (ptNewRoot == NULL)
    {
        // LOG_ERROR managed by mem_alloc()
        return;
    }

    strncpy(ptNewRoot->szPath, szNewPath, ST_MAX_PATH - 1);
    strncpy(ptNewRoot->szName, szNewPath, ST_MAX_PATH - 1);
    ptNewRoot->bIsDir          = ST_TRUE;
    ptNewRoot->bExpanded       = ST_TRUE;
    ptNewRoot->bChildrenLoaded = ST_FALSE;
    ptNewRoot->iDepth          = -1;

    dir_node_load_children(ptNewRoot, ptView->bShowHidden);

    /* Free old tree only after new one is ready */
    dir_node_free_tree(ptView->ptRoot);
    ptView->ptRoot = ptNewRoot;

    strncpy(ptView->szRootPath, szNewPath, ST_MAX_PATH - 1);
    ptView->szRootPath[ST_MAX_PATH - 1] = '\0';

    ptView->iFlatCount    = 0;
    ptView->iScrollOffset = 0;
    ptView->iSelectedFlat = -1;
    dir_flat_rebuild(ptView);

    dir_update_title(ptView, hWnd);
}

/*
 * dir_navigate_up() - Navigate to the parent of the current root ("..").
 *
 * Parameters:
 *   ptView [IN/OUT] : view to navigate; unchanged if already at root
 *   hWnd   [IN]     : window handle forwarded to dir_navigate_to()
 *
 * Returns:
 *   Void - None
 */
/* -- [DIR]62. dir_navigate_up computes the parent path and navigates the tree to it, unless already at the filesystem root -- */
/* This helper is currently called by:
 *  - dir_activate_sel() ([DIR]10) - tested via ENTER on the ".." row
 *    (TC-DIR-077..080) */
static void dir_navigate_up(dir_view_t *ptView, gui_window_t hWnd)
{
    char szParent[ST_MAX_PATH];

    dir_get_parent_path(ptView->szRootPath, szParent, sizeof(szParent));
    if (strcmp(szParent, ptView->szRootPath) == 0)
        return; /* already at filesystem root */

    dir_nav_history_push(ptView, szParent);
    dir_navigate_to(ptView, szParent, hWnd);

    LOG_INFO("dir: navigated up to '%s' (%d entries)",
             szParent, ptView->iFlatCount);
}

/*
 * dir_is_multi_sel() - Report whether szPath is in the multi-selection set.
 *
 * Parameters:
 *   ptView [IN] : view whose aszMultiSel[]/iMultiSelCount is searched
 *   szPath [IN] : path to look up
 *
 * Returns:
 *   ST_TRUE  if szPath is currently multi-selected
 *   ST_FALSE otherwise
 */
/* -- [DIR]63. dir_is_multi_sel reports whether szPath is currently present in the multi-selection set -- */
/* This helper is currently called by:
 *  - dir_render() ([DIR]25) - tested via the purple background layer
 *    assertions (TC-DIR-087, TC-DIR-090) */
/* P14: check if szPath is in the multi-selection set */
static st_bool_t dir_is_multi_sel(const dir_view_t *ptView, const char *szPath)
{
    int i;
    for (i = 0; i < ptView->iMultiSelCount; i++)
    {
        if (strcmp(ptView->aszMultiSel[i], szPath) == 0)
            return ST_TRUE;
    }
    return ST_FALSE;
}

/* -- [DIR]40. dir_toggle_multi_sel: adds a path to the multi-selection set, or removes it if already present -- */
/* P14: toggle szPath in/out of the multi-selection set */
static void dir_toggle_multi_sel(dir_view_t *ptView, const char *szPath)
{
    int i;
    int j;

    for (i = 0; i < ptView->iMultiSelCount; i++)
    {
        if (strcmp(ptView->aszMultiSel[i], szPath) == 0)
        {
            /* Remove by shifting remaining entries down */
            for (j = i; j < ptView->iMultiSelCount - 1; j++)
            {
                strncpy(ptView->aszMultiSel[j],
                        ptView->aszMultiSel[j + 1], ST_MAX_PATH - 1);
                ptView->aszMultiSel[j][ST_MAX_PATH - 1] = '\0';
            }
            ptView->iMultiSelCount--;
            return;
        }
    }

    if (ptView->iMultiSelCount < DIR_MULTI_SEL_MAX)
    {
        strncpy(ptView->aszMultiSel[ptView->iMultiSelCount],
                szPath, ST_MAX_PATH - 1);
        ptView->aszMultiSel[ptView->iMultiSelCount][ST_MAX_PATH - 1] = '\0';
        ptView->iMultiSelCount++;
    }
}

/* -- [DIR]31. writes selected file path to the console selection -- */
/* P11: commit a file selection to the console + last-selected indicator
 * (shared by dir_activate_sel's ENTER path and dir_handle_key's SPACE) */
static void dir_commit_file_selection(dir_view_t       *ptView,
                                        const dir_node_t *ptNode)
{
    if (!ptView->bIsLineRunning) return;

    line_set_selected(ptNode->szPath);
    strncpy(ptView->szLastSelected, ptNode->szPath, ST_MAX_PATH - 1);
    ptView->szLastSelected[ST_MAX_PATH - 1] = '\0';
    LOG_INFO("dir: selected '%s'", ptNode->szPath);
}

static void dir_activate_sel(dir_view_t *ptView, gui_window_t hWnd)
{
    dir_flat_entry_t *ptEntry;
    dir_node_t       *ptNode;

    /* -- [DIR]64. dir_activate_sel: iSelectedFlat == -2 ('nothing selected') is a silent no-op -- */
    if (ptView->iSelectedFlat == -2) return; /* nothing selected */

    /* -- [DIR]10. dir_activate_sel: ".." row navigates to the parent directory -- */
    if (ptView->iSelectedFlat == -1)
    {
        dir_navigate_up(ptView, hWnd);
        return;
    }

    ptEntry = &ptView->aptFlat[ptView->iSelectedFlat];
    ptNode  = ptEntry->ptNode;

    /* -- [DIR]11. dir_activate_sel: directory entry toggles expand/collapse -- */
    if (ptNode->bIsDir)
    {
        if (ptNode->bExpanded == ST_TRUE)
        {
            ptNode->bExpanded = ST_FALSE;
        }
        else
        {
            if (ptNode->bChildrenLoaded == ST_FALSE)
            {
                dir_node_load_children(ptNode, ptView->bShowHidden);
            }
            ptNode->bExpanded = ST_TRUE;
        }
        ptView->iFlatCount = 0;
        dir_flat_rebuild(ptView);
        dir_scroll_to_sel(ptView);
    }
    else
    {
        /* -- [DIR]12. dir_activate_sel: file entry commits selection (P11) -- */
        /* File: update console selection (mutex-safe, UC4.3) */
        dir_commit_file_selection(ptView, ptNode);
    }
}

/* ==================================================================
 * Rendering
 * ================================================================== */

static void dir_render(dir_view_t *ptView)
{
    renderer_rect_t   tRect;
    char              szLine[ST_MAX_PATH + 320]; /* prefix+deco+name */
    char              szPrefix[256];
    int               iRow;
    int               iVisRows;
    int               iTotalRows;
    int               iY;
    int               iEntryIdx;
    st_bool_t         bSelected;
    st_bool_t         bLastSel;
    dir_flat_entry_t *ptEntry;

    /* -- [DIR]65. dir_render: a NULL hRenderer (D2D creation failure) or non-positive iCellH is a silent no-op -- */
    if (ptView->hRenderer == NULL || ptView->iCellH <= 0) return;

    renderer_begin_draw(ptView->hRenderer, &g_dir_clrBg);

    iTotalRows = ptView->iFlatCount + 1;
    iVisRows   = ptView->iWndHeight / ptView->iCellH;

    for (iRow = ptView->iScrollOffset;
         iRow < iTotalRows
         && (iRow - ptView->iScrollOffset) < iVisRows;
         iRow++)
    {
        iY        = (iRow - ptView->iScrollOffset) * ptView->iCellH;
        bSelected = ST_FALSE;
        bLastSel  = ST_FALSE;

        if (iRow == 0 && ptView->iSelectedFlat == -1)
            bSelected = ST_TRUE;
        else if (iRow > 0 && ptView->iSelectedFlat == (iRow - 1))
            bSelected = ST_TRUE;

        /* P11: secondary background for last committed selection */
        if (iRow > 0 && ptView->szLastSelected[0] != '\0')
        {
            ptEntry = &ptView->aptFlat[iRow - 1];
            if (strcmp(ptEntry->ptNode->szPath,
                       ptView->szLastSelected) == 0)
            {
                bLastSel = ST_TRUE;
            }
        }

        /* -- [DIR]24. dir_render: P11 green background marks the last committed selection -- */
        /* Layer 1 (bottom): P11 green — last committed selection */
        if (bLastSel == ST_TRUE)
        {
            tRect.fX = 0.0f;
            tRect.fY = (float)iY;
            tRect.fW = (float)ptView->iWndWidth;
            tRect.fH = (float)ptView->iCellH;
            renderer_fill_rect(ptView->hRenderer, &tRect,
                               &g_dir_clrLastSel);
        }

        /* -- [DIR]25. dir_render: P14 purple background marks multi-selected files -- */
        /* Layer 2 (middle): P14 purple — multi-selected files */
        if (iRow > 0)
        {
            dir_flat_entry_t *ptFE = &ptView->aptFlat[iRow - 1];
            if (!ptFE->ptNode->bIsDir
            &&  dir_is_multi_sel(ptView, ptFE->ptNode->szPath))
            {
                tRect.fX = 0.0f;
                tRect.fY = (float)iY;
                tRect.fW = (float)ptView->iWndWidth;
                tRect.fH = (float)ptView->iCellH;
                renderer_fill_rect(ptView->hRenderer, &tRect,
                                   &g_dir_clrMultiSel);
            }
        }

        /* -- [DIR]26. dir_render: blue background marks the keyboard cursor selection -- */
        /* Layer 3 (top): blue — keyboard/cursor selection */
        if (bSelected == ST_TRUE)
        {
            tRect.fX = 0.0f;
            tRect.fY = (float)iY;
            tRect.fW = (float)ptView->iWndWidth;
            tRect.fH = (float)ptView->iCellH;
            renderer_fill_rect(ptView->hRenderer, &tRect, &g_dir_clrSel);
        }

        tRect.fX = 4.0f;
        tRect.fY = (float)iY;
        tRect.fW = (float)(ptView->iWndWidth - 4);
        tRect.fH = (float)ptView->iCellH;

        /* -- [DIR]27. dir_render: rows are drawn as ".." / "[+/-] dir/" / file name -- */
        if (iRow == 0)
        {
            renderer_draw_text(ptView->hRenderer, "..", &tRect,
                               &g_dir_clrDD, RENDERER_FONT_MONO,
                               RENDERER_ALIGN_LEFT);
        }
        else
        {
            iEntryIdx = iRow - 1;
            ptEntry   = &ptView->aptFlat[iEntryIdx];
            dir_build_prefix(ptEntry, szPrefix, sizeof(szPrefix));

            if (ptEntry->ptNode->bIsDir)
            {
                snprintf(szLine, sizeof(szLine), "%s[%c] %s/",
                         szPrefix,
                         ptEntry->ptNode->bExpanded ? '-' : '+',
                         ptEntry->ptNode->szName);
                renderer_draw_text(ptView->hRenderer, szLine, &tRect,
                                   &g_dir_clrDir, RENDERER_FONT_MONO,
                                   RENDERER_ALIGN_LEFT);
            }
            else
            {
                snprintf(szLine, sizeof(szLine), "%s%s",
                         szPrefix, ptEntry->ptNode->szName);
                renderer_draw_text(ptView->hRenderer, szLine, &tRect,
                                   &g_dir_clrFile, RENDERER_FONT_MONO,
                                   RENDERER_ALIGN_LEFT);
            }
        }
    }

    renderer_end_draw(ptView->hRenderer);
}

/* ==================================================================
 * Event handlers  (all run on the window thread)
 * ================================================================== */

static void dir_handle_key(dir_view_t  *ptView,
                             gui_window_t hWnd,
                             gui_key_t    eKey,
                             st_u8_t      uiMods)
{
    int       iVisRows;
    int       iTotalRows;
    int       iMaxScroll;
    int       iMaxSel;
    st_bool_t bRedraw;

    /* -- [DIR]66. dir_handle_key: a non-positive iCellH (renderer not yet created) is a silent no-op -- */
    if (ptView->iCellH <= 0) return;

    iTotalRows = ptView->iFlatCount + 1;
    iVisRows   = ptView->iWndHeight / ptView->iCellH;
    if (iVisRows < 1) iVisRows = 1;
    iMaxScroll = iTotalRows - iVisRows;
    if (iMaxScroll < 0) iMaxScroll = 0;
    iMaxSel    = ptView->iFlatCount - 1; /* max flat index */
    bRedraw    = ST_FALSE;

    switch (eKey)
    {
    /* -- [DIR]13. dir_handle_key: UP/DOWN move the cursor selection -- */
    case GUI_KEY_UP:
        if (ptView->iSelectedFlat > -1)
        {
            ptView->iSelectedFlat--;
            dir_scroll_to_sel(ptView);
            bRedraw = ST_TRUE;
        }
        /* -- [DIR]67. dir_handle_key: UP recovers from the dead iSelectedFlat == -2 sentinel by selecting the last entry -- */
        else if (ptView->iSelectedFlat == -2 && iTotalRows > 0)
        {
            ptView->iSelectedFlat = iMaxSel;
            dir_scroll_to_sel(ptView);
            bRedraw = ST_TRUE;
        }
        break;

    case GUI_KEY_DOWN:
        if (ptView->iSelectedFlat < iMaxSel)
        {
            ptView->iSelectedFlat++;
            dir_scroll_to_sel(ptView);
            bRedraw = ST_TRUE;
        }
        /* -- [DIR]68. dir_handle_key: DOWN recovers from the dead iSelectedFlat == -2 sentinel by selecting the '..' row -- */
        else if (ptView->iSelectedFlat == -2)
        {
            ptView->iSelectedFlat = -1; /* ".." */
            dir_scroll_to_sel(ptView);
            bRedraw = ST_TRUE;
        }
        break;

    /* -- [DIR]69. dir_handle_key: PAGE_UP scrolls iScrollOffset up by one page -- */
    case GUI_KEY_PAGE_UP:
        ptView->iScrollOffset -= iVisRows;
        if (ptView->iScrollOffset < 0) ptView->iScrollOffset = 0;
        bRedraw = ST_TRUE;
        break;

    /* -- [DIR]70. dir_handle_key: PAGE_DOWN scrolls iScrollOffset down by one page, clamped to iMaxScroll -- */
    case GUI_KEY_PAGE_DOWN:
        ptView->iScrollOffset += iVisRows;
        if (ptView->iScrollOffset > iMaxScroll)
            ptView->iScrollOffset = iMaxScroll;
        bRedraw = ST_TRUE;
        break;

    /* -- [DIR]71. dir_handle_key: HOME resets the scroll offset and selects the '..' row -- */
    case GUI_KEY_HOME:
        ptView->iScrollOffset = 0;
        ptView->iSelectedFlat = -1;
        bRedraw = ST_TRUE;
        break;

    /* -- [DIR]72. dir_handle_key: END scrolls to the last page and selects the last flat entry -- */
    case GUI_KEY_END:
        ptView->iScrollOffset = iMaxScroll;
        ptView->iSelectedFlat = iMaxSel;
        bRedraw = ST_TRUE;
        break;

    /* -- [DIR]14. dir_handle_key: ENTER activates the current selection -- */
    case GUI_KEY_ENTER:
        /* P13: ENTER = action (expand/collapse dir, select file, nav up) */
        dir_activate_sel(ptView, hWnd);
        bRedraw = ST_TRUE;
        break;

    case GUI_KEY_SPACE:
        if (ptView->iSelectedFlat >= 0)
        {
            dir_node_t *ptNode;
            ptNode = ptView->aptFlat[ptView->iSelectedFlat].ptNode;
            if (uiMods & GUI_MOD_CTRL)
            {
                /* -- [DIR]16. dir_handle_key: CTRL+SPACE toggles multi-selection on files (P14/P60) -- */
                /* P14: CTRL+SPACE = toggle multi-selection (files only).
                 * P60: starting multi-sel clears single selection first. */
                if (!ptNode->bIsDir)
                {
                    if (ptView->szLastSelected[0] != '\0')
                    {
                        ptView->szLastSelected[0] = '\0';
                        if (ptView->bIsLineRunning)
                            line_set_selected("");
                    }
                    dir_toggle_multi_sel(ptView, ptNode->szPath);
                    bRedraw = ST_TRUE;
                    LOG_INFO("dir: multi-sel toggle '%s' (count=%d)",
                             ptNode->szPath, ptView->iMultiSelCount);
                }
            }
            else
            {
                /* -- [DIR]15. dir_handle_key: SPACE selects and clears multi-selection (P13/P60) -- */
                /* P13: SPACE = pure selection — update szSelected, no expand.
                 * P60: clear multi-selection before setting single select. */
                if (ptView->iMultiSelCount > 0)
                {
                    memset(ptView->aszMultiSel, 0,
                           sizeof(ptView->aszMultiSel));
                    ptView->iMultiSelCount = 0;
                    LOG_INFO("dir: multi-sel cleared on single SPACE");
                }

                /* -- [DIR]32. dir_handle_key: SPACE on a selected entry toggles deselection (P70) -- */
                /* P70: pressing SPACE again on the entry currently holding
                 * the single (green) selection clears it instead of
                 * re-committing the same path. */
                if (ptView->szLastSelected[0] != '\0'
                &&  strcmp(ptView->szLastSelected, ptNode->szPath) == 0)
                {
                    ptView->szLastSelected[0] = '\0';
                    if (ptView->bIsLineRunning)
                        line_set_selected("");
                    LOG_INFO("dir: deselected '%s' (SPACE toggle)",
                             ptNode->szPath);
                }
                else
                {
                    dir_commit_file_selection(ptView, ptNode);
                }
                /* BUG-08: refresh immediately so green highlight is visible */
                bRedraw = ST_TRUE;
            }
        }
        break;

    case GUI_KEY_LEFT:
        if (uiMods & GUI_MOD_ALT)
        {
            /* -- [DIR]18. dir_handle_key: ALT+LEFT/RIGHT navigate the history stack (P10) -- */
            /* P10: ALT+← = navigate history back */
            if (ptView->iNavHistHead > 0)
            {
                ptView->iNavHistHead--;
                dir_navigate_to(ptView,
                                ptView->aszNavHistory[ptView->iNavHistHead],
                                hWnd);
                bRedraw = ST_TRUE;
                LOG_INFO("dir: history back to '%s'",
                         ptView->aszNavHistory[ptView->iNavHistHead]);
            }
        }
        else
        {
            /* -- [DIR]17. dir_handle_key: LEFT/RIGHT collapse/expand the selected directory (P12) -- */
            /* P12: collapse expanded directory */
            if (ptView->iSelectedFlat >= 0)
            {
                dir_node_t *ptNode;
                ptNode = ptView->aptFlat[ptView->iSelectedFlat].ptNode;
                if (ptNode->bIsDir == ST_TRUE
                &&  ptNode->bExpanded == ST_TRUE)
                {
                    ptNode->bExpanded  = ST_FALSE;
                    ptView->iFlatCount = 0;
                    dir_flat_rebuild(ptView);
                    dir_scroll_to_sel(ptView);
                    bRedraw = ST_TRUE;
                }
            }
        }
        break;

    case GUI_KEY_RIGHT:
        if (uiMods & GUI_MOD_ALT)
        {
            /* P10: ALT+→ = navigate history forward */
            if (ptView->iNavHistHead < ptView->iNavHistCount - 1)
            {
                ptView->iNavHistHead++;
                dir_navigate_to(ptView,
                                ptView->aszNavHistory[ptView->iNavHistHead],
                                hWnd);
                bRedraw = ST_TRUE;
                LOG_INFO("dir: history forward to '%s'",
                         ptView->aszNavHistory[ptView->iNavHistHead]);
            }
        }
        else
        {
            /* P12: expand collapsed directory (lazy-load children if needed) */
            if (ptView->iSelectedFlat >= 0)
            {
                dir_node_t *ptNode;
                ptNode = ptView->aptFlat[ptView->iSelectedFlat].ptNode;
                if (ptNode->bIsDir == ST_TRUE
                &&  ptNode->bExpanded == ST_FALSE)
                {
                    if (ptNode->bChildrenLoaded == ST_FALSE)
                    {
                        dir_node_load_children(ptNode, ptView->bShowHidden);
                    }
                    ptNode->bExpanded  = ST_TRUE;
                    ptView->iFlatCount = 0;
                    dir_flat_rebuild(ptView);
                    dir_scroll_to_sel(ptView);
                    bRedraw = ST_TRUE;
                }
            }
        }
        break;

    /* -- [DIR]19. dir_handle_key: ESCAPE requests a non-blocking window close (P9) -- */
    case GUI_KEY_ESCAPE:
        /* P9: ESC closes the view non-blocking from the window thread */
        gui_request_close(hWnd);
        break;

    /* -- [DIR]20. dir_handle_key: F5 refreshes the tree preserving expansion (P22) -- */
    case GUI_KEY_F5:
        /* P22: F5 = refresh while preserving current expansion state */
        dir_refresh_tree(ptView);
        dir_flat_rebuild(ptView);
        ptView->iScrollOffset = 0;
        if (ptView->iSelectedFlat >= ptView->iFlatCount)
        {
            ptView->iSelectedFlat = ptView->iFlatCount - 1;
        }
        bRedraw = ST_TRUE;
        LOG_INFO("dir: refreshed '%s' (%d entries)",
                 ptView->szRootPath, ptView->iFlatCount);
        break;

    /* -- [DIR]21. dir_handle_key: 'H'/'h' toggles hidden-file visibility (P21) -- */
    default:
        /* P21: 'H'/'h' toggles hidden file visibility */
        if (eKey >= GUI_KEY_PRINTABLE)
        {
            char cChar;
            cChar = (char)((int)eKey - (int)GUI_KEY_PRINTABLE);
            if (cChar == 'H' || cChar == 'h')
            {
                ptView->bShowHidden =
                    (ptView->bShowHidden == ST_TRUE)
                    ? ST_FALSE : ST_TRUE;

                /* P21/P22: reuse F5's expansion-preserving reload so
                 * toggling hidden files does not collapse open dirs */
                dir_refresh_tree(ptView);
                dir_flat_rebuild(ptView);
                ptView->iScrollOffset = 0;
                if (ptView->iSelectedFlat >= ptView->iFlatCount)
                {
                    ptView->iSelectedFlat = ptView->iFlatCount - 1;
                }
                bRedraw = ST_TRUE;
                LOG_INFO("dir: hidden files %s",
                         ptView->bShowHidden ? "shown" : "hidden");
            }
        }
        break;
    }

    if (bRedraw == ST_TRUE)
    {
        gui_invalidate(hWnd);
    }
}

static void dir_handle_click(dir_view_t  *ptView,
                               gui_window_t hWnd,
                               int          iX,
                               int          iY)
{
    int               iClickRow;
    dir_flat_entry_t *ptEntry;

    /* -- [DIR]22. dir_handle_click: left-click selects a row and expands directories -- */
    ST_UNUSED(iX);

    /* -- [DIR]73. dir_handle_click: a non-positive iCellH (renderer not yet created) is a silent no-op -- */
    if (ptView->iCellH <= 0) return;

    iClickRow = iY / ptView->iCellH + ptView->iScrollOffset;

    if (iClickRow == 0)
    {
        ptView->iSelectedFlat = -1;
    }
    else if (iClickRow <= ptView->iFlatCount)
    {
        ptView->iSelectedFlat = iClickRow - 1;
        ptEntry = &ptView->aptFlat[ptView->iSelectedFlat];
        if (ptEntry->ptNode->bIsDir)
        {
            dir_activate_sel(ptView, hWnd);
        }
    }
    else
    {
        return; /* click below content */
    }

    gui_invalidate(hWnd);
}

static void dir_handle_scroll(dir_view_t  *ptView,
                                gui_window_t hWnd,
                                int          iDelta)
{
    int iVisRows;
    int iMaxScroll;
    int iTotalRows;

    /* -- [DIR]74. dir_handle_scroll: a non-positive iCellH (renderer not yet created) is a silent no-op -- */
    if (ptView->iCellH <= 0) return;

    iVisRows   = ptView->iWndHeight / ptView->iCellH;
    if (iVisRows < 1) iVisRows = 1;
    iTotalRows = ptView->iFlatCount + 1;
    iMaxScroll = iTotalRows - iVisRows;
    if (iMaxScroll < 0) iMaxScroll = 0;

    /* -- [DIR]23. dir_handle_scroll: mouse wheel adjusts iScrollOffset within bounds -- */
    /* iDelta > 0 = scroll up (towards beginning), < 0 = scroll down */
    ptView->iScrollOffset -= iDelta;

    if (ptView->iScrollOffset < 0)          ptView->iScrollOffset = 0;
    if (ptView->iScrollOffset > iMaxScroll) ptView->iScrollOffset = iMaxScroll;

    gui_invalidate(hWnd);
}

/* ==================================================================
 * GUI event callback  (called from the window thread for all events)
 * ================================================================== */

static void dir_event_callback(gui_window_t  hWnd,
                                 gui_event_t  *ptEvent,
                                 void         *pUserCtx)
{
    dir_view_t *ptView;

    /* -- [DIR]75. dir_event_callback: a NULL ptEvent or pUserCtx is a silent no-op -- */
    if (ptEvent == NULL || pUserCtx == NULL) return;

    ptView = (dir_view_t *)pUserCtx;

    switch (ptEvent->eType)
    {
    case GUI_EVT_PAINT:
        /* -- [DIR]28. Dir GUI react on GUI_EVT_PAINT, first event creates renderer -- */
        /* Create renderer on first paint (HWND live at this point) */
        if (ptView->hRenderer == NULL)
        {
            if (renderer_create(hWnd, &ptView->hRenderer) == ST_NO_ERROR)
            {
                renderer_get_font_metrics(ptView->hRenderer,
                                          RENDERER_FONT_MONO,
                                          &ptView->iCellW,
                                          &ptView->iCellH);
                gui_get_size(hWnd,
                             &ptView->iWndWidth,
                             &ptView->iWndHeight);
            }
        }
        dir_render(ptView);
        break;

    case GUI_EVT_RESIZE:
        /* -- [DIR]29. Dir GUI react on GUI_EVT_RESIZE -- */
        gui_handle_resize_event(hWnd, ptEvent,
                                 &ptView->iWndWidth, &ptView->iWndHeight,
                                 ptView->hRenderer);
        break;

    case GUI_EVT_KEY_DOWN:
        /* -- [DIR]30. Dir GUI react on GUI_EVT_KEY_DOWN and dispatch -- */
        dir_handle_key(ptView, hWnd,
                       ptEvent->uData.tKey.eKey,
                       ptEvent->uData.tKey.uiMods);
        break;

    case GUI_EVT_MOUSE_DOWN:
        /* -- [DIR]33. Dir GUI react on GUI_EVT_MOUSE_DOWN and dispatch -- */
        if (ptEvent->uData.tMouse.eBtn == GUI_MOUSE_LEFT)
        {
            dir_handle_click(ptView, hWnd,
                             ptEvent->uData.tMouse.iX,
                             ptEvent->uData.tMouse.iY);
        }
        break;

    case GUI_EVT_SCROLL:
        /* -- [DIR]34. Dir GUI react on GUI_EVT_SCROLL and dispatch -- */
        dir_handle_scroll(ptView, hWnd, ptEvent->uData.tScroll.iDelta);
        break;

    /* -- [DIR]76. Dir GUI react on GUI_EVT_CLOSE and release the renderer -- */
    case GUI_EVT_CLOSE:
        if (ptView->hRenderer != NULL)
        {
            renderer_destroy(&ptView->hRenderer);
        }
        break;

    /* -- [DIR]77. Dir GUI: any other/unhandled gui_event_t type is silently ignored -- */
    default:
        break;
    }
}

/* ==================================================================
 * Public API
 * ================================================================== */

st_u64_t dir_open(const char     *szPath,
                     st_bool_t      bIsLineRunning,
                     st_bool_t      bShowHidden,
                     dir_view_t   **pptView)
{
    dir_view_t    *ptView;
    gui_wnd_desc_t tDesc;
    st_error_t     eResult;
    char           szCwd[ST_MAX_PATH];
    char           szTitle[ST_MAX_PATH + 32];
    const char    *szRoot;

    LOG_TRACE("szPath=%p bIsLineRunning=%d bShowHidden=%d pptView=%p",
              (void *)szPath, (int)bIsLineRunning,
              (int)bShowHidden, (void *)pptView);

    /* -- [DIR]1. dir_open rejects NULL pptView -- */
    if (pptView == NULL)
    {
        LOG_ERROR("NULL parameter: pptView=%p", (void *)pptView);
        return ST_ERROR;
    }

    *pptView = NULL;

    /* -- [DIR]2. dir_open requires an initialized line context (bIsLineRunning) -- */
    if (!bIsLineRunning)
    {
        LOG_ERROR("Line context not initialized - call line_init() first");
        return ST_ERROR;
    }

    /* -- [DIR]3. dir_open resolves the root path to cwd when szPath is empty -- */
    /* Resolve root path */
    if (szPath == NULL || szPath[0] == '\0')
    {
        if (dir_getcwd(szCwd, sizeof(szCwd)) == NULL)
        {
            strncpy(szCwd, ".", sizeof(szCwd) - 1);
            szCwd[sizeof(szCwd) - 1] = '\0';
        }
        szRoot = szCwd;
    }
    else
    {
        szRoot = szPath;
    }

    /* -- [DIR]42. dir_open allocates & populates the GUI dir_view_t structure -- */
    /* Allocate view */
    ptView = (dir_view_t *)malloc(sizeof(dir_view_t));
    if (ptView == NULL)
    {
        LOG_ERROR("malloc failed for dir_view_t");
        return ST_ERROR;
    }
    memset(ptView, 0, sizeof(dir_view_t));

    /* -- [DIR]43. dir_open allocates & populates a flat list of entries -- */
    ptView->aptFlat = (dir_flat_entry_t *)malloc(
                          DIR_FLAT_MAX * sizeof(dir_flat_entry_t));
    if (ptView->aptFlat == NULL)
    {
        LOG_ERROR("malloc failed for aptFlat (%d entries)", DIR_FLAT_MAX);
        free(ptView);
        return ST_ERROR;
    }

    strncpy(ptView->szRootPath, szRoot, ST_MAX_PATH - 1);
    ptView->szRootPath[ST_MAX_PATH - 1] = '\0';
    ptView->bIsLineRunning= bIsLineRunning;
    ptView->bShowHidden   = bShowHidden;
    ptView->iSelectedFlat = -1;   /* ".." selected by default */
    ptView->iCellH        = 20;   /* overridden on first PAINT */
    ptView->iCellW        = 10;

    /* P10: seed navigation history with the initial root */
    strncpy(ptView->aszNavHistory[0], szRoot, ST_MAX_PATH - 1);
    ptView->aszNavHistory[0][ST_MAX_PATH - 1] = '\0';
    ptView->iNavHistHead  = 0;
    ptView->iNavHistCount = 1;

    /* -- [DIR]44. dir_open allocates and populates the root node -- */
    /* Create and populate root node */
    ptView->ptRoot = (dir_node_t*)mem_alloc(ST_DIR_NODE_T, 
                                           &g_dir_ptCtx.uiAllocObjs[ST_DIR_NODE_T]);   
    if (ptView->ptRoot == NULL)
    {
        free(ptView->aptFlat);
        free(ptView);
        return ST_ERROR;
    }

    strncpy(ptView->ptRoot->szPath, szRoot, ST_MAX_PATH - 1);
    strncpy(ptView->ptRoot->szName, szRoot, ST_MAX_PATH - 1);
    ptView->ptRoot->bIsDir    = ST_TRUE;
    ptView->ptRoot->bExpanded = ST_TRUE;
    ptView->ptRoot->iDepth    = -1;

    /* -- [DIR]4. dir_open loads the root's children honoring bShowHidden -- */
    eResult = dir_node_load_children(ptView->ptRoot, ptView->bShowHidden);
    if (eResult != ST_NO_ERROR)
    {
        dir_node_free_tree(ptView->ptRoot);
        free(ptView->aptFlat);
        free(ptView);
        return ST_ERROR;
    }

    dir_flat_rebuild(ptView);

    /* -- [DIR]5. dir_open opens the GUI window and returns the populated view -- */
    /* Open window (blocks until HWND is live, per UC3.1 contract) */
    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Dir: %s", szRoot);
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle  = szTitle;
    tDesc.eType    = GUI_WND_DIR;
    tDesc.pfnEvent = dir_event_callback;
    tDesc.pUserCtx = ptView;

    eResult = gui_open_window(&tDesc, &ptView->hWnd);
    if (eResult != ST_NO_ERROR)
    {
        dir_node_free_tree(ptView->ptRoot);
        free(ptView->aptFlat);
        free(ptView);
        return ST_ERROR;
    }

    *pptView = ptView;
    g_dir_ptCtx.uiNbViewOpen = g_dir_ptCtx.uiNbViewOpen + 1;
    LOG_INFO("dir view opened: '%s' (%d flat entries)",
             szRoot, ptView->iFlatCount);
    return (st_u64_t)&g_dir_ptCtx;
}

st_u64_t dir_close(dir_view_t **pptView)
{
    dir_view_t *ptView;

    LOG_TRACE("pptView=%p", (void *)pptView);

    /* -- [DIR]6. dir_close rejects NULL pptView -- */
    if (pptView == NULL)
    {
        LOG_ERROR("pptView is NULL");
        return ST_ERROR;
    }

    ptView   = *pptView;
    *pptView = NULL;

    /* -- [DIR]7. dir_close is idempotent when *pptView is already NULL -- */
    if (ptView == NULL)
    {
        return (st_u64_t)&g_dir_ptCtx; /* idempotent */
    }

    /* -- [DIR]8. dir_close releases the window, tree and flat list -- */
    /* Close window: posts WM_CLOSE then joins the thread.
     * GUI_EVT_CLOSE callback (window thread) destroys the renderer
     * before the thread exits. */
    if (ptView->hWnd != NULL)
    {
        gui_close_window(ptView->hWnd);
        ptView->hWnd = NULL;
    }

    /* Renderer already destroyed in GUI_EVT_CLOSE callback */
    if (ptView->ptRoot != NULL)
    {
        dir_node_free_tree(ptView->ptRoot);
        ptView->ptRoot = NULL;
    }

    if (ptView->aptFlat != NULL)
    {
        free(ptView->aptFlat);
        ptView->aptFlat = NULL;
    }

    free(ptView);
    g_dir_ptCtx.uiNbViewOpen = g_dir_ptCtx.uiNbViewOpen - 1;
    LOG_INFO("dir view closed");
    return (st_u64_t)&g_dir_ptCtx;
}
