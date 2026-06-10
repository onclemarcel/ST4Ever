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
#include "edit_hex.h"
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
static const renderer_color_t g_mnt_clrHint      = {0.45f,0.45f,0.45f,1.0f};
static const renderer_color_t g_mnt_clrBootY     = {0.20f,0.90f,0.20f,1.0f};
static const renderer_color_t g_mnt_clrBootN     = {0.55f,0.55f,0.55f,1.0f};
static const renderer_color_t g_mnt_clrStatusBg  = {0.11f,0.11f,0.18f,1.0f};
static const renderer_color_t g_mnt_clrStatusTx  = {0.70f,0.70f,0.70f,1.0f};
static const renderer_color_t g_mnt_clrDir       = {0.40f,0.80f,1.00f,1.0f};

/* Chunk size for progress reporting during file copy (P40) */
#define MOUNT_PROGRESS_CHUNK 65536u

#define MOUNT_BOOT_TMP  "/tmp/st4ever_boot.bin"
#define MOUNT_FILE_TMP  "/tmp/st4ever_mnt_file.bin"

/* ------------------------------------------------------------------
 * Internal: WD1772 bootability check
 * ------------------------------------------------------------------ */

/* P37: sum of the 256 LE16 words in the 512-byte bootsector must
 * equal 0x1234 (mod 0x10000) for the WD1772 to execute it. */
static st_bool_t mount_check_bootable(const st_u8_t *pBoot)
{
    st_u32_t uiSum;
    int      i;

    uiSum = 0;
    for (i = 0; i < 512; i += 2)
    {
        st_u16_t uiWord = (st_u16_t)( ((st_u16_t)pBoot[i]) |
                                       ((st_u16_t)pBoot[i + 1] << 8) );
        uiSum += uiWord;
    }
    return ((uiSum & 0xFFFFu) == 0x1234u) ? ST_TRUE : ST_FALSE;
}

/* P38: write the 512-byte bootsector to a temp file and open the
 * hex editor on it.  Stores the view handle in ptView->ptBootHexView.
 * Called from the window thread. */
static void mount_open_bootsector(mount_view_t *ptView)
{
    st_u8_t          *pDisk    = NULL;
    file_t           *ptFile   = NULL;
    edit_hex_view_t **pptHexV;
    char              szTitle[80];
    st_bool_t         bBoot;
    st_u16_t          uiSPT;
    st_u16_t          uiHeads;
    st_u16_t          uiTracks;

    if (ptView->ptImg == NULL)
        return;
    if (image_st_get_disk(ptView->ptImg, &pDisk) != ST_NO_ERROR ||
        pDisk == NULL)
        return;

    /* Close existing bootsector view if any */
    pptHexV = (edit_hex_view_t **)&ptView->ptBootHexView;
    if (*pptHexV != NULL)
    {
        edit_hex_close(pptHexV);
        ptView->ptBootHexView = NULL;
    }

    /* Write bootsector bytes to temp file */
    if (file_open(MOUNT_BOOT_TMP, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
    {
        LOG_ERROR("mount: cannot write bootsector temp file '%s'",
                  MOUNT_BOOT_TMP);
        return;
    }
    if (file_write(ptFile, pDisk, 512) != ST_NO_ERROR)
    {
        file_close(&ptFile);
        LOG_ERROR("mount: bootsector write failed");
        return;
    }
    file_close(&ptFile);

    /* Heuristic title info */
    bBoot  = mount_check_bootable(pDisk);
    /* BPB: SPT @0x18 LE16, HEADS @0x1A LE16, TS @0x13 LE16 */
    uiSPT    = (st_u16_t)( (st_u16_t)pDisk[0x18] |
                            ((st_u16_t)pDisk[0x19] << 8) );
    uiHeads  = (st_u16_t)( (st_u16_t)pDisk[0x1A] |
                            ((st_u16_t)pDisk[0x1B] << 8) );
    uiTracks = (st_u16_t)( (st_u16_t)pDisk[0x13] |
                            ((st_u16_t)pDisk[0x14] << 8) );
    if (uiSPT == 0)    uiSPT    = IST_SPT;
    if (uiHeads == 0)  uiHeads  = IST_HEADS;
    if (uiTracks == 0) uiTracks = IST_TRACKS;

    snprintf(szTitle, sizeof(szTitle),
             "bootsector [H%u/S%u/T%u %uKo - %s]",
             (unsigned)uiHeads, (unsigned)uiSPT, (unsigned)uiTracks,
             (unsigned)(((st_u32_t)uiHeads * uiSPT * uiTracks * IST_BPS)
                        / 1024u),
             bBoot ? "bootable" : "not bootable");

    if (ptView->ptLineCtx != NULL)
    {
        if (edit_hex_open(MOUNT_BOOT_TMP, ptView->ptLineCtx,
                          (edit_hex_view_t **)&ptView->ptBootHexView)
            != ST_NO_ERROR)
        {
            LOG_ERROR("mount: edit_hex_open bootsector failed");
            ptView->ptBootHexView = NULL;
            return;
        }
        /* Override window title with bootsector info */
        if (ptView->ptBootHexView != NULL)
        {
            edit_hex_view_t *ptHex;
            ptHex = (edit_hex_view_t *)ptView->ptBootHexView;
            gui_set_title(ptHex->hWnd, szTitle);
        }
    }
}

/* P41: extract the selected file to a temp file and open the hex
 * editor on it.  Stores the view handle in ptView->ptFileHexView.
 * Called from the window thread on ENTER key. */
static void mount_open_file_hex(mount_view_t *ptView)
{
    image_st_dirent_t *ptEntry;
    st_u8_t           *pBuf    = NULL;
    file_t            *ptFile  = NULL;
    edit_hex_view_t  **pptHexV;
    char               szTitle[128];

    if (ptView->ptImg == NULL ||
        ptView->iSelectedEntry < 0 ||
        ptView->iSelectedEntry >= ptView->iEntryCount)
        return;

    ptEntry = &ptView->aEntries[ptView->iSelectedEntry];

    /* Close existing file hex viewer if any */
    pptHexV = (edit_hex_view_t **)&ptView->ptFileHexView;
    if (*pptHexV != NULL)
    {
        edit_hex_close(pptHexV);
        ptView->ptFileHexView = NULL;
    }

    if (ptEntry->uiSize == 0)
    {
        LOG_INFO("mount: selected file '%s' is empty, nothing to open",
                 ptEntry->szName);
        return;
    }

    pBuf = (st_u8_t *)malloc((size_t)ptEntry->uiSize);
    if (pBuf == NULL)
    {
        LOG_ERROR("mount_open_file_hex: malloc failed for '%s'",
                  ptEntry->szName);
        return;
    }

    if (image_st_read_file(ptView->ptImg,
                            ptEntry->uiCluster,
                            ptEntry->uiSize,
                            pBuf,
                            ptEntry->uiSize) != ST_NO_ERROR)
    {
        LOG_ERROR("mount_open_file_hex: read failed for '%s'",
                  ptEntry->szName);
        free(pBuf);
        return;
    }

    /* Write to temp file */
    if (file_open(MOUNT_FILE_TMP, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
    {
        LOG_ERROR("mount_open_file_hex: cannot write temp file '%s'",
                  MOUNT_FILE_TMP);
        free(pBuf);
        return;
    }
    if (file_write(ptFile, pBuf, ptEntry->uiSize) != ST_NO_ERROR)
    {
        LOG_ERROR("mount_open_file_hex: write failed");
        file_close(&ptFile);
        free(pBuf);
        return;
    }
    file_close(&ptFile);
    free(pBuf);

    snprintf(szTitle, sizeof(szTitle),
             "A:\\ %s  [%u bytes]", ptEntry->szName,
             (unsigned)ptEntry->uiSize);

    if (ptView->ptLineCtx != NULL)
    {
        if (edit_hex_open(MOUNT_FILE_TMP, ptView->ptLineCtx,
                          (edit_hex_view_t **)&ptView->ptFileHexView)
            != ST_NO_ERROR)
        {
            LOG_ERROR("mount_open_file_hex: edit_hex_open failed");
            ptView->ptFileHexView = NULL;
            return;
        }
        if (ptView->ptFileHexView != NULL)
        {
            edit_hex_view_t *ptHex;
            ptHex = (edit_hex_view_t *)ptView->ptFileHexView;
            gui_set_title(ptHex->hWnd, szTitle);
        }
    }
}

/* ------------------------------------------------------------------
 * Internal: refresh entry list from image
 * ------------------------------------------------------------------ */

static void mount_refresh(mount_view_t *ptView)
{
    ptView->iEntryCount = 0;
    if (ptView->ptImg == NULL)
        return;
    /* UC24F: list current dir (NULL/empty = root) */
    image_st_list_dir(ptView->ptImg,
                      ptView->szCurDir[0] ? ptView->szCurDir : NULL,
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

    /* Rows available for files (header takes row 0, status bar takes last) */
    iVisRows = (ptView->iWndHeight / ptView->iCellH) - 2;
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
    if (ptView->szCurDir[0] != '\0')
        snprintf(szLine, sizeof(szLine),
                 " A:\\%s  [%s]  %d entr%s",
                 ptView->szCurDir, szSrc, ptView->iEntryCount,
                 ptView->iEntryCount == 1 ? "y" : "ies");
    else
        snprintf(szLine, sizeof(szLine),
                 " A:\\  [%s]  %d file%s",
                 szSrc, ptView->iEntryCount,
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

        if (ptView->aEntries[iIdx].bIsDir)
            snprintf(szLine, sizeof(szLine),
                     "  %-12s  [DIR]",
                     ptView->aEntries[iIdx].szName);
        else
            snprintf(szLine, sizeof(szLine),
                     "  %-12s   %7lu",
                     ptView->aEntries[iIdx].szName,
                     (unsigned long)ptView->aEntries[iIdx].uiSize);
        tText.fX = 4.0f;
        tText.fY = (float)(iRow * ptView->iCellH);
        tText.fW = (float)(MOUNT_LIST_WIDTH - 8);
        tText.fH = (float)ptView->iCellH;
        renderer_draw_text(ptView->hRenderer, szLine,
                           &tText,
                           ptView->aEntries[iIdx].bIsDir
                               ? &g_mnt_clrDir : &g_mnt_clrFile,
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

    /* P34: BPB geometry (read-only) */
    {
        st_u8_t  *pDisk = NULL;
        st_u16_t  uiSPT    = IST_SPT;
        st_u16_t  uiHeads  = IST_HEADS;
        st_u16_t  uiTracks = IST_TRACKS;
        st_u16_t  uiRDE    = IST_RDE;

        if (ptView->eSrc != MOUNT_SRC_DIR &&
            ptView->ptImg != NULL &&
            image_st_get_disk(ptView->ptImg, &pDisk) == ST_NO_ERROR &&
            pDisk != NULL)
        {
            st_u16_t v;
            st_u16_t uiTS;  /* BPB @0x13: Total Sectors (not Tracks!) */
            v = (st_u16_t)( (st_u16_t)pDisk[0x18] |
                             ((st_u16_t)pDisk[0x19] << 8) );
            if (v != 0) uiSPT = v;
            v = (st_u16_t)( (st_u16_t)pDisk[0x1A] |
                             ((st_u16_t)pDisk[0x1B] << 8) );
            if (v != 0) uiHeads = v;
            uiTS = (st_u16_t)( (st_u16_t)pDisk[0x13] |
                                ((st_u16_t)pDisk[0x14] << 8) );
            if (uiTS != 0 && uiSPT != 0 && uiHeads != 0)
                uiTracks = (st_u16_t)(uiTS / ((st_u32_t)uiSPT * uiHeads));
            v = (st_u16_t)( (st_u16_t)pDisk[0x11] |
                             ((st_u16_t)pDisk[0x12] << 8) );
            if (v != 0) uiRDE = v;

            PROP_ROW("Geometry:", &g_mnt_clrLabel);
            snprintf(szLine, sizeof(szLine), "  H%u/S%u/T%u  %uKo",
                     (unsigned)uiHeads, (unsigned)uiSPT,
                     (unsigned)uiTracks,
                     (unsigned)(((st_u32_t)uiHeads * uiSPT * uiTracks *
                                 IST_BPS) / 1024u));
            PROP_ROW(szLine, &g_mnt_clrValue);

            PROP_ROW("Bootable:", &g_mnt_clrLabel);
            if (mount_check_bootable(pDisk))
                PROP_ROW("  Yes", &g_mnt_clrBootY);
            else
                PROP_ROW("  No",  &g_mnt_clrBootN);

            /* P36: Files with root dir capacity from BPB */
            PROP_ROW("Files:", &g_mnt_clrLabel);
            snprintf(szLine, sizeof(szLine), "  %d / %u  (root)",
                     ptView->iEntryCount, (unsigned)uiRDE);
            PROP_ROW(szLine, &g_mnt_clrValue);
        }
        else if (ptView->eSrc == MOUNT_SRC_DIR)
        {
            /* BUG-06: local directory mount — no real bootsector */
            PROP_ROW("Geometry:", &g_mnt_clrLabel);
            PROP_ROW("  —", &g_mnt_clrValue);
            PROP_ROW("Bootable:", &g_mnt_clrLabel);
            PROP_ROW("  —", &g_mnt_clrValue);
            PROP_ROW("Files:", &g_mnt_clrLabel);
            snprintf(szLine, sizeof(szLine), "  %d", ptView->iEntryCount);
            PROP_ROW(szLine, &g_mnt_clrValue);
            /* BUG-04: warn about files skipped (8.3 name limit / disk full) */
            if (ptView->iNotImported > 0)
            {
                PROP_ROW("Skipped:", &g_mnt_clrLabel);
                snprintf(szLine, sizeof(szLine), "  %d (8.3 limit)",
                         ptView->iNotImported);
                PROP_ROW(szLine, &g_mnt_clrDirty);
            }
        }
        else
        {
            PROP_ROW("Files:", &g_mnt_clrLabel);
            snprintf(szLine, sizeof(szLine), "  %d / %d",
                     ptView->iEntryCount, IST_RDE);
            PROP_ROW(szLine, &g_mnt_clrValue);
        }
    }

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
    PROP_ROW("[ENTER]   Open hex", &g_mnt_clrHint);
    PROP_ROW("[DEL]     Remove",   &g_mnt_clrHint);
    PROP_ROW("[B]       Bootsector", &g_mnt_clrHint);
    PROP_ROW("[F]       Fix boot", &g_mnt_clrHint);
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

    /* === Status bar (P39) — bottom row across full window width === */
    {
        int      iSbRow;
        char     szStatus[128];
        st_u32_t uiSbFree = 0;
        int      iPos     = 0;

        iSbRow = (ptView->iWndHeight / ptView->iCellH) - 1;

        /* Background */
        tRect.fX = 0.0f;
        tRect.fY = (float)(iSbRow * ptView->iCellH);
        tRect.fW = (float)ptView->iWndWidth;
        tRect.fH = (float)ptView->iCellH;
        renderer_fill_rect(ptView->hRenderer, &tRect, &g_mnt_clrStatusBg);

        /* Build status text */
        if (ptView->ptImg != NULL)
            image_st_free_bytes(ptView->ptImg, &uiSbFree);
        iPos += snprintf(szStatus + iPos, sizeof(szStatus) - (size_t)iPos,
                         "  Free: %u KB  |  %d file%s",
                         (unsigned)(uiSbFree / 1024u),
                         ptView->iEntryCount,
                         ptView->iEntryCount == 1 ? "" : "s");
        if (ptView->bDirty)
            snprintf(szStatus + iPos, sizeof(szStatus) - (size_t)iPos,
                     "  [*] unsaved");

        tText.fX = 4.0f;
        tText.fY = (float)(iSbRow * ptView->iCellH);
        tText.fW = (float)(ptView->iWndWidth - 8);
        tText.fH = (float)ptView->iCellH;
        renderer_draw_text(ptView->hRenderer, szStatus,
                           &tText, &g_mnt_clrStatusTx,
                           RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    }

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
    iVisRows = (ptView->iWndHeight / ptView->iCellH) - 2;
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

    case GUI_KEY_ENTER:
        /* UC24F: navigate into directory or open file in hex (P41) */
        if (ptView->iSelectedEntry >= 0 &&
            ptView->iSelectedEntry < ptView->iEntryCount &&
            ptView->aEntries[ptView->iSelectedEntry].bIsDir)
        {
            if (ptView->iNavDepth < 7)
            {
                strncpy(ptView->aszNavStack[ptView->iNavDepth],
                        ptView->szCurDir, IST_NAME_MAX - 1);
                ptView->aszNavStack[ptView->iNavDepth][IST_NAME_MAX-1] = '\0';
                ptView->iNavDepth++;
            }
            strncpy(ptView->szCurDir,
                    ptView->aEntries[ptView->iSelectedEntry].szName,
                    IST_NAME_MAX - 1);
            ptView->szCurDir[IST_NAME_MAX - 1] = '\0';
            ptView->iSelectedEntry = -1;
            ptView->iScrollOffset  = 0;
            mount_refresh(ptView);
            gui_invalidate(hWnd);
            return;
        }
        mount_open_file_hex(ptView);
        return;

    case GUI_KEY_LEFT:
    case GUI_KEY_BACKSPACE:
        /* UC24F: navigate up to parent directory */
        if (ptView->iNavDepth > 0)
        {
            ptView->iNavDepth--;
            strncpy(ptView->szCurDir,
                    ptView->aszNavStack[ptView->iNavDepth],
                    IST_NAME_MAX - 1);
            ptView->szCurDir[IST_NAME_MAX - 1] = '\0';
            ptView->iSelectedEntry = -1;
            ptView->iScrollOffset  = 0;
            mount_refresh(ptView);
            gui_invalidate(hWnd);
        }
        return;

    default:
        /* P38: 'B'/'b' opens bootsector; P37: 'F'/'f' makes bootable */
        if (eKey >= GUI_KEY_PRINTABLE)
        {
            char cCh;
            cCh = (char)((int)eKey - (int)GUI_KEY_PRINTABLE);
            if (cCh == 'B' || cCh == 'b')
            {
                mount_open_bootsector(ptView);
            }
            else if (cCh == 'F' || cCh == 'f')
            {
                if (ptView->ptImg != NULL)
                {
                    if (mount_make_bootable(ptView->ptImg) == ST_NO_ERROR)
                    {
                        ptView->bDirty = ST_TRUE;
                        LOG_INFO("bootsector checksum fixed - disk is now bootable");
                        gui_invalidate(hWnd);
                    }
                }
            }
        }
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

    iVisRows = (ptView->iWndHeight / ptView->iCellH) - 2;
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
    iVisRows = (ptView->iWndHeight / ptView->iCellH) - 2;
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
    char          szDirName[IST_NAME_MAX]; /* "" = root, else subdir   */
} mount_dir_ctx_t;

static void mount_dir_cb(const char        *szFull,
                          const char        *szName,
                          const file_stat_t *ptStat,
                          void              *pCtx)
{
    mount_dir_ctx_t  tSub;
    mount_dir_ctx_t *ptCbCtx  = (mount_dir_ctx_t *)pCtx;
    file_t          *ptFile   = NULL;
    st_u8_t         *pBuf     = NULL;
    size_t           uiRead   = 0;
    st_error_t       eRes;

    if (ptStat->bIsDir)
    {
        /* Recurse into subdirs 1 level deep (root dirs only, UC24F P53) */
        if (ptCbCtx->szDirName[0] == '\0')
        {
            if (image_st_mkdir(ptCbCtx->ptView->ptImg,
                               szName) == ST_NO_ERROR)
            {
                memset(&tSub, 0, sizeof(tSub));
                tSub.ptView = ptCbCtx->ptView;
                strncpy(tSub.szDirName, szName, IST_NAME_MAX - 1);
                tSub.szDirName[IST_NAME_MAX - 1] = '\0';
                file_list_dir(szFull, ST_FALSE, mount_dir_cb, &tSub);
                ptCbCtx->iAdded   += tSub.iAdded;
                ptCbCtx->iSkipped += tSub.iSkipped;
            }
            else
            {
                LOG_INFO("dir mount: skipped subdir '%s' (name or disk)", szName);
                ptCbCtx->iSkipped++;
            }
        }
        /* else: skip nested sub-subdirs (Atari ST 1-level practical limit) */
        return;
    }

    if (ptStat->uiSize == 0)
    {
        if (ptCbCtx->szDirName[0] == '\0')
            eRes = image_st_write_file(ptCbCtx->ptView->ptImg,
                                        szName, NULL, 0);
        else
            eRes = image_st_write_file_in_dir(ptCbCtx->ptView->ptImg,
                                               ptCbCtx->szDirName,
                                               szName, NULL, 0);
        if (eRes == ST_NO_ERROR)
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

    if (ptCbCtx->szDirName[0] == '\0')
        eRes = image_st_write_file(ptCbCtx->ptView->ptImg,
                                    szName, pBuf, (st_u32_t)uiRead);
    else
        eRes = image_st_write_file_in_dir(ptCbCtx->ptView->ptImg,
                                           ptCbCtx->szDirName,
                                           szName, pBuf, (st_u32_t)uiRead);
    if (eRes == ST_NO_ERROR)
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

st_error_t mount_save_image(mount_view_t    *ptView,
                              mount_save_fmt_t eFmt,
                              const char      *szOutPath)
{
    image_st_dirent_t  aEntries[IST_RDE];
    st_u8_t           *pBuf    = NULL;
    file_t            *ptFile  = NULL;
    char               szFilePath[ST_MAX_PATH];
    int                iCount  = 0;
    int                i;
    st_error_t         eResult = ST_NO_ERROR;

    if (ptView == NULL || szOutPath == NULL)
    {
        LOG_ERROR("NULL parameter: ptView=%p szOutPath=%p",
                  (void *)ptView, (void *)szOutPath);
        return ST_ERROR;
    }
    if (ptView->ptImg == NULL)
    {
        LOG_ERROR("mount_save_image: no disk image loaded");
        return ST_ERROR;
    }

    switch (eFmt)
    {
    case MOUNT_SAVE_ST:
        eResult = image_st_save(ptView->ptImg, szOutPath);
        break;

    case MOUNT_SAVE_MSA:
        eResult = image_msa_save(ptView->ptImg, szOutPath);
        break;

    case MOUNT_SAVE_DIR:
    {
        image_st_dirent_t  aSubEntries[IST_RDE];
        char               szSubDir[ST_MAX_PATH];
        int                iSubCount;
        int                j;

        if (file_mkdir(szOutPath) != ST_NO_ERROR)
        {
            LOG_ERROR("mount_save_image: cannot create dir '%s'", szOutPath);
            return ST_ERROR;
        }
        if (image_st_list_root(ptView->ptImg, aEntries, IST_RDE,
                                &iCount) != ST_NO_ERROR)
        {
            LOG_ERROR("mount_save_image: list_root failed");
            return ST_ERROR;
        }
        for (i = 0; i < iCount; i++)
        {
            if (aEntries[i].bIsDir)
            {
                /* UC24F: extract subdirectory */
                snprintf(szSubDir, sizeof(szSubDir),
                         "%s/%s", szOutPath, aEntries[i].szName);
                if (file_mkdir(szSubDir) != ST_NO_ERROR)
                {
                    LOG_ERROR("mount_save_image: cannot create '%s'",
                              szSubDir);
                    eResult = ST_ERROR;
                    continue;
                }
                iSubCount = 0;
                if (image_st_list_dir(ptView->ptImg, aEntries[i].szName,
                                       aSubEntries, IST_RDE,
                                       &iSubCount) != ST_NO_ERROR)
                    continue;
                for (j = 0; j < iSubCount; j++)
                {
                    if (aSubEntries[j].bIsDir || aSubEntries[j].uiSize == 0)
                        continue;
                    pBuf = (st_u8_t *)malloc(
                               (size_t)aSubEntries[j].uiSize);
                    if (pBuf == NULL)
                    {
                        LOG_ERROR("malloc failed for '%s'",
                                  aSubEntries[j].szName);
                        eResult = ST_ERROR;
                        continue;
                    }
                    if (image_st_read_file(ptView->ptImg,
                                           aSubEntries[j].uiCluster,
                                           aSubEntries[j].uiSize,
                                           pBuf,
                                           aSubEntries[j].uiSize)
                        != ST_NO_ERROR)
                    {
                        LOG_ERROR("read_file failed for '%s'",
                                  aSubEntries[j].szName);
                        free(pBuf);
                        pBuf    = NULL;
                        eResult = ST_ERROR;
                        continue;
                    }
                    snprintf(szFilePath, sizeof(szFilePath),
                             "%s/%s", szSubDir, aSubEntries[j].szName);
                    if (file_open(szFilePath, FILE_MODE_WRITE,
                                   &ptFile) != ST_NO_ERROR)
                    {
                        LOG_ERROR("cannot open '%s' for write", szFilePath);
                        free(pBuf);
                        pBuf    = NULL;
                        eResult = ST_ERROR;
                        continue;
                    }
                    if (file_write(ptFile, pBuf,
                                    aSubEntries[j].uiSize) != ST_NO_ERROR)
                    {
                        LOG_ERROR("write failed for '%s'", szFilePath);
                        eResult = ST_ERROR;
                    }
                    file_close(&ptFile);
                    free(pBuf);
                    pBuf = NULL;
                    LOG_INFO("mount_save_image: extracted '%s/%s'",
                             aEntries[i].szName, aSubEntries[j].szName);
                }
                continue;
            }

            if (aEntries[i].uiSize == 0)
                continue;

            pBuf = (st_u8_t *)malloc((size_t)aEntries[i].uiSize);
            if (pBuf == NULL)
            {
                LOG_ERROR("malloc failed for '%s'", aEntries[i].szName);
                eResult = ST_ERROR;
                continue;
            }
            if (image_st_read_file(ptView->ptImg,
                                    aEntries[i].uiCluster,
                                    aEntries[i].uiSize,
                                    pBuf,
                                    aEntries[i].uiSize) != ST_NO_ERROR)
            {
                LOG_ERROR("read_file failed for '%s'", aEntries[i].szName);
                free(pBuf);
                pBuf    = NULL;
                eResult = ST_ERROR;
                continue;
            }
            snprintf(szFilePath, sizeof(szFilePath), "%s/%s",
                     szOutPath, aEntries[i].szName);
            if (file_open(szFilePath, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
            {
                LOG_ERROR("cannot open '%s' for write", szFilePath);
                free(pBuf);
                pBuf    = NULL;
                eResult = ST_ERROR;
                continue;
            }
            if (file_write(ptFile, pBuf, aEntries[i].uiSize) != ST_NO_ERROR)
            {
                LOG_ERROR("write failed for '%s'", szFilePath);
                eResult = ST_ERROR;
            }
            file_close(&ptFile);
            free(pBuf);
            pBuf = NULL;
            LOG_INFO("mount_save_image: extracted '%s'", aEntries[i].szName);
        }
        break;
    }

    default:
        LOG_ERROR("mount_save_image: unknown format %d", (int)eFmt);
        return ST_ERROR;
    }

    if (eResult == ST_NO_ERROR)
    {
        ptView->bDirty = ST_FALSE;
        LOG_INFO("mount_save_image: saved to '%s'", szOutPath);
    }
    return eResult;
}

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
    ptView->ptLineCtx      = ptLineCtx;
    ptView->szCurDir[0]    = '\0';
    ptView->iNavDepth      = 0;
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
        ptView->iNotImported = tCbCtx.iSkipped;
        if (tCbCtx.iSkipped > 0)
            LOG_INFO("dir mount: %d added, %d skipped (8.3 limit or full)",
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

    /* P38: close bootsector hex viewer if open */
    if (ptView->ptBootHexView != NULL)
    {
        edit_hex_view_t *ptHex;
        ptHex = (edit_hex_view_t *)ptView->ptBootHexView;
        edit_hex_close(&ptHex);
        ptView->ptBootHexView = NULL;
    }

    /* P41: close selected-file hex viewer if open */
    if (ptView->ptFileHexView != NULL)
    {
        edit_hex_view_t *ptHex;
        ptHex = (edit_hex_view_t *)ptView->ptFileHexView;
        edit_hex_close(&ptHex);
        ptView->ptFileHexView = NULL;
    }

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
    file_t      *ptFile      = NULL;
    st_u8_t     *pBuf        = NULL;
    const char  *szName;
    size_t       uiRead      = 0;
    size_t       uiChunkRead = 0;
    size_t       uiOffset    = 0;
    size_t       uiChunk;
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

        /* P40: read in chunks and report progress per chunk */
        uiOffset = 0;
        while (uiOffset < (size_t)tStat.uiSize)
        {
            uiChunk = (size_t)tStat.uiSize - uiOffset;
            if (uiChunk > MOUNT_PROGRESS_CHUNK)
                uiChunk = MOUNT_PROGRESS_CHUNK;
            uiChunkRead = 0;
            if (file_read(ptFile, pBuf + uiOffset,
                          uiChunk, &uiChunkRead) != ST_NO_ERROR)
                break;
            if (uiChunkRead == 0)
                break;
            uiOffset += uiChunkRead;
            if (uiOffset < (size_t)tStat.uiSize &&
                ptView->hWnd != NULL)
            {
                LOG_INFO("copying '%s': %u%%", szName,
                         (unsigned)((uiOffset * 100u) /
                                    (size_t)tStat.uiSize));
                gui_invalidate(ptView->hWnd);
            }
        }
        uiRead = uiOffset;
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

st_bool_t mount_is_bootable(const st_u8_t *pBootSect)
{
    if (pBootSect == NULL)
        return ST_FALSE;
    return mount_check_bootable(pBootSect);
}

st_error_t mount_make_bootable(image_st_t *ptImg)
{
    st_u8_t  *pDisk   = NULL;
    st_u32_t  uiSum   = 0;
    st_u16_t  uiWord0;
    st_u16_t  uiNewWord0;
    int       i;

    if (ptImg == NULL)
    {
        LOG_ERROR("mount_make_bootable: ptImg is NULL");
        return ST_ERROR;
    }
    if (image_st_get_disk(ptImg, &pDisk) != ST_NO_ERROR || pDisk == NULL)
    {
        LOG_ERROR("mount_make_bootable: cannot get disk buffer");
        return ST_ERROR;
    }

    /* Compute current sum of all 256 LE16 words */
    for (i = 0; i < 512; i += 2)
    {
        st_u16_t uiW = (st_u16_t)( (st_u16_t)pDisk[i] |
                                     ((st_u16_t)pDisk[i + 1] << 8) );
        uiSum += uiW;
    }
    uiSum &= 0xFFFFu;

    /* Current word[0] */
    uiWord0 = (st_u16_t)( (st_u16_t)pDisk[0] |
                            ((st_u16_t)pDisk[1] << 8) );

    /* new_word0: adjust so (sum - word0 + new_word0) mod 0x10000 == 0x1234 */
    uiNewWord0 = (st_u16_t)((0x1234u - (uiSum - uiWord0)) & 0xFFFFu);

    /* Write back LE16 */
    pDisk[0] = (st_u8_t)(uiNewWord0 & 0xFFu);
    pDisk[1] = (st_u8_t)((uiNewWord0 >> 8) & 0xFFu);

    LOG_INFO("mount_make_bootable: checksum fixed (word[0]=$%04X)", uiNewWord0);
    return ST_NO_ERROR;
}
