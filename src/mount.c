/*
 * mount.c - ST4Ever floppy disk emulation view
 *
 * UC18.1 : D2D view for mounted floppy content.
 *   Left panel  : A:\ header + scrollable file list (name + size).
 *   Right panel : disk properties (source, stats, hints).
 *   Keyboard    : UP/DOWN/PgUp/PgDn/Home/End navigate; DEL removes;
 *                 ESC closes.
 *   Mouse       : left-click selects; scroll wheel scrolls.
 *   Populates a blank image from a PC directory via file_list_dir()
 *   callback (MOUNT_SRC_DIR).
 *
 * UC18.2 : drag-and-drop from dir (P14) — mount_view_add_file().
 * UC19   : umount + optional save dialog.
 * UC20   : image creation from mounted content.
 */

#include "mount.h"
#include "image_msa.h"
#include "file.h"
#include "trace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------
 * Colors
 * ------------------------------------------------------------------ */

static const renderer_color_t g_mnt_clrBg      = {0.09f,0.09f,0.14f,1.0f};
static const renderer_color_t g_mnt_clrPropsBg = {0.12f,0.12f,0.19f,1.0f};
static const renderer_color_t g_mnt_clrSep     = {0.25f,0.25f,0.35f,1.0f};
static const renderer_color_t g_mnt_clrHdrBg   = {0.14f,0.14f,0.22f,1.0f};
static const renderer_color_t g_mnt_clrHeader  = {0.95f,0.90f,0.10f,1.0f};
static const renderer_color_t g_mnt_clrFile    = {0.80f,0.80f,0.80f,1.0f};
static const renderer_color_t g_mnt_clrSel     = {0.20f,0.30f,0.65f,1.0f};
static const renderer_color_t g_mnt_clrLabel   = {0.00f,0.85f,0.85f,1.0f};
static const renderer_color_t g_mnt_clrValue   = {0.80f,0.80f,0.80f,1.0f};
static const renderer_color_t g_mnt_clrDirty   = {0.95f,0.50f,0.10f,1.0f};
static const renderer_color_t g_mnt_clrHint    = {0.45f,0.45f,0.45f,1.0f};

/* ------------------------------------------------------------------
 * Internal: refresh entry list from image
 * ------------------------------------------------------------------ */

static void mount_refresh(mount_view_t *ptView)
{
    ptView->iEntryCount = 0;
    if (ptView->ptImg == NULL)
        return;
    image_st_list_root(ptView->ptImg,
                       ptView->aEntries, IST_RDE,
                       &ptView->iEntryCount);
}

/* ------------------------------------------------------------------
 * Internal: scroll to keep selection visible
 * ------------------------------------------------------------------ */

static void mount_scroll_to_sel(mount_view_t *ptView, int iVisRows)
{
    int iIdx = ptView->iSelectedEntry;
    int iMax;

    if (iVisRows <= 1 || iIdx < 0)
        return;
    iMax = ptView->iEntryCount - iVisRows;
    if (iMax < 0)
        iMax = 0;

    if (iIdx < ptView->iScrollOffset)
        ptView->iScrollOffset = iIdx;
    else if (iIdx >= ptView->iScrollOffset + iVisRows)
        ptView->iScrollOffset = iIdx - iVisRows + 1;

    if (ptView->iScrollOffset > iMax)
        ptView->iScrollOffset = iMax;
    if (ptView->iScrollOffset < 0)
        ptView->iScrollOffset = 0;
}

/* ------------------------------------------------------------------
 * Internal: render
 * ------------------------------------------------------------------ */

static void mount_render(mount_view_t *ptView)
{
    renderer_rect_t tRect;
    renderer_rect_t tText;
    char            szLine[128];
    int             iPropsX;
    int             iRow;
    int             iIdx;
    int             iVisRows;
    int             iMaxScroll;
    st_u32_t        uiFreeBytes;
    int             iPr;          /* property row counter */
    char            szShort[42];
    const char     *szSrc;
    const char     *szP;
    size_t          uiL;

    if (ptView->hRenderer == NULL || ptView->iCellH == 0)
        return;

    /* Rows available for files (below the header row) */
    iVisRows = (ptView->iWndHeight / ptView->iCellH) - 1;
    if (iVisRows < 1)
        iVisRows = 1;

    /* Clamp scroll */
    iMaxScroll = ptView->iEntryCount - iVisRows;
    if (iMaxScroll < 0)  iMaxScroll = 0;
    if (ptView->iScrollOffset > iMaxScroll)
        ptView->iScrollOffset = iMaxScroll;
    if (ptView->iScrollOffset < 0)
        ptView->iScrollOffset = 0;

    iPropsX = MOUNT_LIST_WIDTH + 4;

    renderer_begin_draw(ptView->hRenderer, &g_mnt_clrBg);

    /* --- Left panel background --- */
    tRect.fX = 0.0f;
    tRect.fY = 0.0f;
    tRect.fW = (float)MOUNT_LIST_WIDTH;
    tRect.fH = (float)ptView->iWndHeight;
    renderer_fill_rect(ptView->hRenderer, &tRect, &g_mnt_clrBg);

    /* --- Right panel background --- */
    tRect.fX = (float)MOUNT_LIST_WIDTH;
    tRect.fW = (float)(ptView->iWndWidth - MOUNT_LIST_WIDTH);
    renderer_fill_rect(ptView->hRenderer, &tRect, &g_mnt_clrPropsBg);

    /* --- Vertical separator --- */
    renderer_draw_line(ptView->hRenderer,
                       (float)MOUNT_LIST_WIDTH, 0.0f,
                       (float)MOUNT_LIST_WIDTH, (float)ptView->iWndHeight,
                       &g_mnt_clrSep, 1.0f);

    /* === Header row === */
    tRect.fX = 0.0f;
    tRect.fY = 0.0f;
    tRect.fW = (float)MOUNT_LIST_WIDTH;
    tRect.fH = (float)ptView->iCellH;
    renderer_fill_rect(ptView->hRenderer, &tRect, &g_mnt_clrHdrBg);

    switch (ptView->eSrc)
    {
    case MOUNT_SRC_ST:  szSrc = ".ST";  break;
    case MOUNT_SRC_MSA: szSrc = ".MSA"; break;
    default:            szSrc = "DIR";  break;
    }
    snprintf(szLine, sizeof(szLine),
             " A:\\  [%s]  %d / %d file%s",
             szSrc, ptView->iEntryCount, IST_RDE,
             ptView->iEntryCount == 1 ? "" : "s");
    tText.fX = 4.0f;
    tText.fY = 0.0f;
    tText.fW = (float)(MOUNT_LIST_WIDTH - 8);
    tText.fH = (float)ptView->iCellH;
    renderer_draw_text(ptView->hRenderer, szLine,
                       &tText, &g_mnt_clrHeader,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

    /* === File rows === */
    for (iIdx = ptView->iScrollOffset;
         iIdx < ptView->iEntryCount;
         iIdx++)
    {
        iRow = 1 + (iIdx - ptView->iScrollOffset);
        if (iRow > iVisRows)
            break;

        /* Selection highlight */
        if (iIdx == ptView->iSelectedEntry)
        {
            tRect.fX = 0.0f;
            tRect.fY = (float)(iRow * ptView->iCellH);
            tRect.fW = (float)MOUNT_LIST_WIDTH;
            tRect.fH = (float)ptView->iCellH;
            renderer_fill_rect(ptView->hRenderer, &tRect, &g_mnt_clrSel);
        }

        snprintf(szLine, sizeof(szLine),
                 "  %-12s   %7lu",
                 ptView->aEntries[iIdx].szName,
                 (unsigned long)ptView->aEntries[iIdx].uiSize);
        tText.fX = 4.0f;
        tText.fY = (float)(iRow * ptView->iCellH);
        tText.fW = (float)(MOUNT_LIST_WIDTH - 8);
        tText.fH = (float)ptView->iCellH;
        renderer_draw_text(ptView->hRenderer, szLine,
                           &tText, &g_mnt_clrFile,
                           RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    }

    /* === Right panel: disk properties === */
    iPr = 0;

/* Helper macro: draw one property row with a given colour */
#define PROP_ROW(txt, clr) do { \
    tText.fX = (float)(iPropsX); \
    tText.fY = (float)(iPr * ptView->iCellH); \
    tText.fW = (float)(ptView->iWndWidth - iPropsX - 4); \
    tText.fH = (float)ptView->iCellH; \
    renderer_draw_text(ptView->hRenderer, (txt), \
                       &tText, (clr), \
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT); \
    iPr++; \
} while (0)

    PROP_ROW("Disk Properties", &g_mnt_clrLabel);
    iPr++;  /* blank */

    PROP_ROW("Source:", &g_mnt_clrLabel);
    switch (ptView->eSrc)
    {
    case MOUNT_SRC_ST:  PROP_ROW("  .st image",  &g_mnt_clrValue); break;
    case MOUNT_SRC_MSA: PROP_ROW("  .msa image", &g_mnt_clrValue); break;
    default:            PROP_ROW("  directory",  &g_mnt_clrValue); break;
    }

    PROP_ROW("Path:", &g_mnt_clrLabel);
    szP = ptView->szSrcPath;
    uiL = strlen(szP);
    if (uiL > 37)
    {
        snprintf(szShort, sizeof(szShort), "...%s", szP + uiL - 34);
        szP = szShort;
    }
    PROP_ROW(szP, &g_mnt_clrValue);
    iPr++;  /* blank */

    PROP_ROW("Files:", &g_mnt_clrLabel);
    snprintf(szLine, sizeof(szLine), "  %d / %d",
             ptView->iEntryCount, IST_RDE);
    PROP_ROW(szLine, &g_mnt_clrValue);

    uiFreeBytes = 0;
    if (ptView->ptImg != NULL)
        image_st_free_bytes(ptView->ptImg, &uiFreeBytes);

    PROP_ROW("Free:", &g_mnt_clrLabel);
    snprintf(szLine, sizeof(szLine), "  %u KB",
             (unsigned)(uiFreeBytes / 1024u));
    PROP_ROW(szLine, &g_mnt_clrValue);

    PROP_ROW("Total:", &g_mnt_clrLabel);
    snprintf(szLine, sizeof(szLine), "  %u KB",
             (unsigned)(IST_DISK_SIZE / 1024u));
    PROP_ROW(szLine, &g_mnt_clrValue);

    PROP_ROW("Modified:", &g_mnt_clrLabel);
    if (ptView->bDirty)
        PROP_ROW("  Yes", &g_mnt_clrDirty);
    else
        PROP_ROW("  No", &g_mnt_clrValue);

    iPr++;  /* blank */
    PROP_ROW("[UP/DOWN] Navigate", &g_mnt_clrHint);
    PROP_ROW("[DEL]     Remove",   &g_mnt_clrHint);
    PROP_ROW("[ESC]     Close",    &g_mnt_clrHint);

    if (ptView->iSelectedEntry >= 0 &&
        ptView->iSelectedEntry < ptView->iEntryCount)
    {
        iPr++;
        snprintf(szLine, sizeof(szLine), "Sel: %s",
                 ptView->aEntries[ptView->iSelectedEntry].szName);
        PROP_ROW(szLine, &g_mnt_clrHint);
    }

#undef PROP_ROW

    renderer_end_draw(ptView->hRenderer);
}

/* ------------------------------------------------------------------
 * Internal: keyboard handler
 * ------------------------------------------------------------------ */

static void mount_handle_key(mount_view_t *ptView,
                              gui_window_t  hWnd,
                              gui_key_t     eKey)
{
    int iVisRows;
    int iOld;

    if (ptView->iCellH == 0)
        return;
    iVisRows = (ptView->iWndHeight / ptView->iCellH) - 1;
    if (iVisRows < 1)
        iVisRows = 1;
    iOld = ptView->iSelectedEntry;

    switch (eKey)
    {
    case GUI_KEY_UP:
        if (ptView->iEntryCount > 0)
        {
            if (ptView->iSelectedEntry > 0)
                ptView->iSelectedEntry--;
            else if (ptView->iSelectedEntry < 0)
                ptView->iSelectedEntry = 0;
        }
        break;

    case GUI_KEY_DOWN:
        if (ptView->iSelectedEntry < ptView->iEntryCount - 1)
            ptView->iSelectedEntry++;
        break;

    case GUI_KEY_HOME:
        ptView->iScrollOffset  = 0;
        ptView->iSelectedEntry = (ptView->iEntryCount > 0) ? 0 : -1;
        break;

    case GUI_KEY_END:
        ptView->iSelectedEntry = ptView->iEntryCount - 1;
        break;

    case GUI_KEY_PAGE_UP:
        ptView->iSelectedEntry -= iVisRows;
        if (ptView->iSelectedEntry < 0)
            ptView->iSelectedEntry =
                (ptView->iEntryCount > 0) ? 0 : -1;
        break;

    case GUI_KEY_PAGE_DOWN:
        ptView->iSelectedEntry += iVisRows;
        if (ptView->iSelectedEntry >= ptView->iEntryCount)
            ptView->iSelectedEntry = ptView->iEntryCount - 1;
        break;

    case GUI_KEY_DELETE:
        if (ptView->iSelectedEntry >= 0 &&
            ptView->iSelectedEntry < ptView->iEntryCount &&
            ptView->ptImg != NULL)
        {
            if (image_st_delete_file(
                    ptView->ptImg,
                    ptView->aEntries[ptView->iSelectedEntry].szName)
                == ST_NO_ERROR)
            {
                ptView->bDirty = ST_TRUE;
                mount_refresh(ptView);
                if (ptView->iSelectedEntry >= ptView->iEntryCount)
                    ptView->iSelectedEntry = ptView->iEntryCount - 1;
                LOG_INFO("deleted '%s' from disk",
                         ptView->aEntries[
                             ptView->iSelectedEntry >= 0
                             ? ptView->iSelectedEntry : 0].szName);
            }
        }
        gui_invalidate(hWnd);
        return;

    case GUI_KEY_ESCAPE:
        gui_request_close(hWnd);
        return;

    default:
        return;
    }

    if (ptView->iSelectedEntry != iOld)
        mount_scroll_to_sel(ptView, iVisRows);
    gui_invalidate(hWnd);
}

/* ------------------------------------------------------------------
 * Internal: mouse click handler
 * ------------------------------------------------------------------ */

static void mount_handle_click(mount_view_t *ptView,
                                gui_window_t  hWnd,
                                int           iX,
                                int           iY)
{
    int iRow;
    int iIdx;
    int iVisRows;

    if (iX >= MOUNT_LIST_WIDTH || ptView->iCellH == 0)
        return;

    iRow = iY / ptView->iCellH;
    if (iRow == 0)
        return;   /* header, no file selection */

    iVisRows = (ptView->iWndHeight / ptView->iCellH) - 1;
    if (iRow > iVisRows)
        return;

    iIdx = ptView->iScrollOffset + (iRow - 1);
    if (iIdx < 0 || iIdx >= ptView->iEntryCount)
        return;

    ptView->iSelectedEntry = iIdx;
    gui_invalidate(hWnd);
}

/* ------------------------------------------------------------------
 * Internal: scroll handler
 * ------------------------------------------------------------------ */

static void mount_handle_scroll(mount_view_t *ptView,
                                 gui_window_t  hWnd,
                                 int           iDelta)
{
    int iMax;
    int iVisRows;

    if (ptView->iCellH == 0)
        return;
    iVisRows = (ptView->iWndHeight / ptView->iCellH) - 1;
    if (iVisRows < 1)
        iVisRows = 1;
    iMax = ptView->iEntryCount - iVisRows;
    if (iMax < 0)
        iMax = 0;

    ptView->iScrollOffset -= iDelta;
    if (ptView->iScrollOffset < 0)
        ptView->iScrollOffset = 0;
    if (ptView->iScrollOffset > iMax)
        ptView->iScrollOffset = iMax;

    gui_invalidate(hWnd);
}

/* ------------------------------------------------------------------
 * Internal: event callback (window thread)
 * ------------------------------------------------------------------ */

static void mount_event_callback(gui_window_t  hWnd,
                                  gui_event_t  *ptEvent,
                                  void         *pUserCtx)
{
    mount_view_t *ptView = (mount_view_t *)pUserCtx;

    if (ptView == NULL || ptEvent == NULL)
        return;

    switch (ptEvent->eType)
    {
    case GUI_EVT_PAINT:
        if (ptView->hRenderer == NULL)
        {
            if (renderer_create(hWnd, &ptView->hRenderer) != ST_NO_ERROR)
                break;
            renderer_get_font_metrics(ptView->hRenderer,
                                      RENDERER_FONT_MONO,
                                      &ptView->iCellW,
                                      &ptView->iCellH);
            gui_get_size(hWnd,
                         &ptView->iWndWidth,
                         &ptView->iWndHeight);
        }
        mount_render(ptView);
        break;

    case GUI_EVT_RESIZE:
        ptView->iWndWidth  = ptEvent->uData.tResize.iWidth;
        ptView->iWndHeight = ptEvent->uData.tResize.iHeight;
        if (ptView->hRenderer != NULL)
            renderer_resize(ptView->hRenderer,
                            ptView->iWndWidth,
                            ptView->iWndHeight);
        break;

    case GUI_EVT_KEY_DOWN:
        mount_handle_key(ptView, hWnd, ptEvent->uData.tKey.eKey);
        break;

    case GUI_EVT_MOUSE_DOWN:
        if (ptEvent->uData.tMouse.eBtn == GUI_MOUSE_LEFT)
            mount_handle_click(ptView, hWnd,
                               ptEvent->uData.tMouse.iX,
                               ptEvent->uData.tMouse.iY);
        break;

    case GUI_EVT_SCROLL:
        mount_handle_scroll(ptView, hWnd,
                            ptEvent->uData.tScroll.iDelta);
        break;

    case GUI_EVT_CLOSE:
        if (ptView->hRenderer != NULL)
        {
            renderer_destroy(&ptView->hRenderer);
            ptView->hRenderer = NULL;
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------
 * Internal: directory population callback
 * ------------------------------------------------------------------ */

typedef struct mount_dir_ctx_s
{
    mount_view_t *ptView;
    int           iAdded;
    int           iSkipped;
} mount_dir_ctx_t;

static void mount_dir_cb(const char        *szFull,
                          const char        *szName,
                          const file_stat_t *ptStat,
                          void              *pCtx)
{
    mount_dir_ctx_t *ptCbCtx = (mount_dir_ctx_t *)pCtx;
    file_t          *ptFile  = NULL;
    st_u8_t         *pBuf    = NULL;
    size_t           uiRead  = 0;

    if (ptStat->bIsDir)
        return;  /* skip subdirectories */

    if (ptStat->uiSize == 0)
    {
        if (image_st_write_file(ptCbCtx->ptView->ptImg,
                                szName, NULL, 0) == ST_NO_ERROR)
            ptCbCtx->iAdded++;
        else
        {
            LOG_INFO("dir mount: skipped '%s' (invalid 8.3 name)", szName);
            ptCbCtx->iSkipped++;
        }
        return;
    }

    pBuf = (st_u8_t *)malloc((size_t)ptStat->uiSize);
    if (pBuf == NULL)
    {
        LOG_ERROR("malloc failed for '%s' (%u bytes)",
                  szName, (unsigned)ptStat->uiSize);
        ptCbCtx->iSkipped++;
        return;
    }

    if (file_open(szFull, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
    {
        free(pBuf);
        ptCbCtx->iSkipped++;
        return;
    }
    file_read(ptFile, pBuf, (size_t)ptStat->uiSize, &uiRead);
    file_close(&ptFile);

    if (image_st_write_file(ptCbCtx->ptView->ptImg,
                             szName, pBuf, (st_u32_t)uiRead) == ST_NO_ERROR)
        ptCbCtx->iAdded++;
    else
    {
        LOG_INFO("dir mount: skipped '%s' (invalid name or disk full)",
                 szName);
        ptCbCtx->iSkipped++;
    }
    free(pBuf);
}

/* ==================================================================
 * Public API
 * ================================================================== */

st_error_t mount_view_open(const char     *szPath,
                             line_context_t *ptLineCtx,
                             mount_view_t  **pptView)
{
    mount_view_t    *ptView = NULL;
    gui_wnd_desc_t   tDesc;
    file_stat_t      tStat;
    mount_dir_ctx_t  tCbCtx;
    char             szTitle[ST_MAX_PATH + 40];
    char             szEffPath[ST_MAX_PATH];
    const char      *szSrcLabel;

    LOG_TRACE("szPath=%s pptView=%p",
              szPath ? szPath : "NULL", (void *)pptView);

    if (ptLineCtx == NULL || pptView == NULL)
    {
        LOG_ERROR("NULL parameter: ptLineCtx=%p pptView=%p",
                  (void *)ptLineCtx, (void *)pptView);
        return ST_ERROR;
    }
    *pptView = NULL;

    /* Resolve effective path */
    if (szPath == NULL || szPath[0] == '\0')
        strncpy(szEffPath, ptLineCtx->szCwd, ST_MAX_PATH - 1);
    else
        strncpy(szEffPath, szPath, ST_MAX_PATH - 1);
    szEffPath[ST_MAX_PATH - 1] = '\0';

    /* Stat the source */
    memset(&tStat, 0, sizeof(tStat));
    if (file_stat(szEffPath, &tStat) != ST_NO_ERROR || !tStat.bExists)
    {
        LOG_ERROR("source not found: '%s'", szEffPath);
        return ST_ERROR;
    }

    /* Allocate view */
    ptView = (mount_view_t *)malloc(sizeof(mount_view_t));
    if (ptView == NULL)
    {
        LOG_ERROR("malloc failed for mount_view_t");
        return ST_ERROR;
    }
    memset(ptView, 0, sizeof(mount_view_t));
    ptView->iSelectedEntry = -1;
    strncpy(ptView->szSrcPath, szEffPath, ST_MAX_PATH - 1);

    /* Load or create the disk image */
    if (tStat.bIsDir)
    {
        ptView->eSrc = MOUNT_SRC_DIR;
        if (image_st_create(&ptView->ptImg) != ST_NO_ERROR)
        {
            free(ptView);
            return ST_ERROR;
        }
        memset(&tCbCtx, 0, sizeof(tCbCtx));
        tCbCtx.ptView = ptView;
        file_list_dir(szEffPath, ST_FALSE, mount_dir_cb, &tCbCtx);
        if (tCbCtx.iSkipped > 0)
            LOG_INFO("dir mount: %d added, %d skipped",
                     tCbCtx.iAdded, tCbCtx.iSkipped);
        szSrcLabel = "Dir";
    }
    else if (strcmp(tStat.szExt, "st") == 0)
    {
        ptView->eSrc = MOUNT_SRC_ST;
        if (image_st_load(szEffPath, &ptView->ptImg) != ST_NO_ERROR)
        {
            free(ptView);
            return ST_ERROR;
        }
        szSrcLabel = "ST";
    }
    else if (strcmp(tStat.szExt, "msa") == 0)
    {
        ptView->eSrc = MOUNT_SRC_MSA;
        if (image_msa_load(szEffPath, &ptView->ptImg) != ST_NO_ERROR)
        {
            free(ptView);
            return ST_ERROR;
        }
        szSrcLabel = "MSA";
    }
    else
    {
        LOG_ERROR("unsupported source for mount: '%s'", szEffPath);
        free(ptView);
        return ST_ERROR;
    }

    /* Populate entry list */
    mount_refresh(ptView);

    /* Open the GUI window */
    memset(&tDesc, 0, sizeof(tDesc));
    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Mount: A:\\ [%s]", szSrcLabel);
    tDesc.szTitle  = szTitle;
    tDesc.eType    = GUI_WND_MOUNT;
    tDesc.iWidth   = MOUNT_WND_WIDTH;
    tDesc.iHeight  = MOUNT_WND_HEIGHT;
    tDesc.pfnEvent = mount_event_callback;
    tDesc.pUserCtx = ptView;

    if (gui_open_window(&tDesc, &ptView->hWnd) != ST_NO_ERROR)
    {
        image_st_close(&ptView->ptImg);
        free(ptView);
        return ST_ERROR;
    }

    LOG_INFO("mount view opened: '%s' (%d files)", szEffPath,
             ptView->iEntryCount);
    *pptView = ptView;
    return ST_NO_ERROR;
}

st_error_t mount_view_close(mount_view_t **pptView)
{
    mount_view_t *ptView;

    if (pptView == NULL)
    {
        LOG_ERROR("pptView is NULL");
        return ST_ERROR;
    }
    if (*pptView == NULL)
        return ST_NO_ERROR;

    ptView = *pptView;
    LOG_TRACE("ptView=%p", (void *)ptView);

    if (ptView->hWnd != NULL)
    {
        gui_close_window(ptView->hWnd);
        ptView->hWnd = NULL;
    }

    if (ptView->ptImg != NULL)
        image_st_close(&ptView->ptImg);

    free(ptView);
    *pptView = NULL;
    LOG_INFO("mount view closed");
    return ST_NO_ERROR;
}

st_error_t mount_view_add_file(mount_view_t *ptView,
                                const char   *szSrcPath)
{
    file_stat_t  tStat;
    file_t      *ptFile  = NULL;
    st_u8_t     *pBuf    = NULL;
    const char  *szName;
    size_t       uiRead  = 0;
    st_error_t   eResult;

    LOG_TRACE("ptView=%p szSrcPath=%s",
              (void *)ptView, szSrcPath ? szSrcPath : "NULL");

    if (ptView == NULL || szSrcPath == NULL)
    {
        LOG_ERROR("NULL parameter: ptView=%p szSrcPath=%p",
                  (void *)ptView, (void *)szSrcPath);
        return ST_ERROR;
    }
    if (ptView->ptImg == NULL)
    {
        LOG_ERROR("add_file: no disk image loaded");
        return ST_ERROR;
    }

    memset(&tStat, 0, sizeof(tStat));
    if (file_stat(szSrcPath, &tStat) != ST_NO_ERROR || !tStat.bExists)
    {
        LOG_ERROR("file not found: '%s'", szSrcPath);
        return ST_ERROR;
    }
    if (tStat.bIsDir)
    {
        LOG_ERROR("cannot add directory: '%s'", szSrcPath);
        return ST_ERROR;
    }

    /* Basename for the on-disk name */
    szName = szSrcPath + strlen(szSrcPath);
    while (szName > szSrcPath &&
           szName[-1] != '/' && szName[-1] != '\\')
        szName--;

    if (tStat.uiSize == 0)
    {
        eResult = image_st_write_file(ptView->ptImg, szName, NULL, 0);
    }
    else
    {
        pBuf = (st_u8_t *)malloc((size_t)tStat.uiSize);
        if (pBuf == NULL)
        {
            LOG_ERROR("malloc failed for '%s'", szName);
            return ST_ERROR;
        }
        if (file_open(szSrcPath, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
        {
            free(pBuf);
            return ST_ERROR;
        }
        file_read(ptFile, pBuf, (size_t)tStat.uiSize, &uiRead);
        file_close(&ptFile);
        eResult = image_st_write_file(ptView->ptImg, szName,
                                       pBuf, (st_u32_t)uiRead);
        free(pBuf);
    }

    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("write to disk failed for '%s'", szName);
        return ST_ERROR;
    }

    ptView->bDirty = ST_TRUE;
    mount_refresh(ptView);
    if (ptView->hWnd != NULL)
        gui_invalidate(ptView->hWnd);

    LOG_INFO("added '%s' to disk (%d files total)",
             szName, ptView->iEntryCount);
    return ST_NO_ERROR;
}
