/*
 * edit_hex.c - Hex + ASCII + disassembly editor view
 *
 * UC9:  Data model (flat heap buffer, replace-mode), hex+ASCII rendering,
 *       full navigation, TAB to switch zones, CTRL+S save.
 * UC10: Disassembly panel on the right side of the window, synchronized
 *       bidirectionally with the hex cursor.  Cache of CACHE_LINES
 *       instructions around the cursor is rebuilt on each cursor move.
 *       TAB now cycles HEX → ASCII → DISASM → HEX.  F2 toggles panel.
 * UC24C: Info band at the bottom (2 rows): sector type + BPB layout on
 *       row 1, editable annotation note on row 2.  JSON annotation file
 *       (<basename>.json) loaded/saved alongside the image.  CTRL+N
 *       activates note editing; CTRL+S saves both hex file and JSON.
 */

#include "edit_hex.h"
#include "file.h"
#include "trace.h"
#include "gui.h"
#include "renderer.h"
#include "disassemble.h"
#include "sector_analyze.h"
#include "image_annot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------
 * Colours — hex+ASCII panel
 * ------------------------------------------------------------------ */

static const renderer_color_t g_clrBg      = { 0.06f, 0.06f, 0.08f, 1.0f };
static const renderer_color_t g_clrAddrBg  = { 0.09f, 0.09f, 0.13f, 1.0f };
static const renderer_color_t g_clrCurRow  = { 0.10f, 0.10f, 0.16f, 1.0f };
static const renderer_color_t g_clrAddr    = { 0.50f, 0.50f, 0.65f, 1.0f };
static const renderer_color_t g_clrHex     = RENDERER_COLOR_LTGRAY;
static const renderer_color_t g_clrAscii   = { 0.55f, 0.85f, 0.55f, 1.0f };
static const renderer_color_t g_clrDot     = { 0.28f, 0.28f, 0.32f, 1.0f };
static const renderer_color_t g_clrSep     = { 0.22f, 0.22f, 0.30f, 1.0f };
static const renderer_color_t g_clrCurHex  = { 0.20f, 0.40f, 0.90f, 1.0f };
static const renderer_color_t g_clrCurAsc  = { 0.80f, 0.65f, 0.10f, 1.0f };
static const renderer_color_t g_clrCurTxt  = RENDERER_COLOR_WHITE;

/* ------------------------------------------------------------------
 * Colours — disassembly panel (UC10)
 * ------------------------------------------------------------------ */

/* Valid 68000 instruction (will be non-DC.W after UC11-14)             */
static const renderer_color_t g_clrDisasm      = { 0.85f, 0.75f, 0.50f, 1.0f };
/* DC.W fallback (stub, dim)                                            */
static const renderer_color_t g_clrDisasmDC    = { 0.45f, 0.40f, 0.30f, 1.0f };
/* Highlighted instruction when cursor is in DISASM zone                */
static const renderer_color_t g_clrDisasmHL    = { 0.15f, 0.30f, 0.55f, 1.0f };
/* Panel separator background                                           */
static const renderer_color_t g_clrDisasmSepBg = { 0.09f, 0.09f, 0.13f, 1.0f };

/* ------------------------------------------------------------------
 * Colours — sector type tints (UC24B)
 * One entry per sector_type_t value [0..SECTOR_TYPE_COUNT-1].
 * Subtle dark hues overlaid on rows belonging to a classified sector.
 * SECTOR_UNKNOWN keeps the default background (no rect drawn).
 * ------------------------------------------------------------------ */

static const renderer_color_t g_aSecTint[SECTOR_TYPE_COUNT] = {
    { 0.06f, 0.06f, 0.08f, 1.0f }, /* UNKNOWN          - default bg   */
    { 0.14f, 0.06f, 0.24f, 1.0f }, /* BOOT_NOBOOT      - violet       */
    { 0.20f, 0.08f, 0.32f, 1.0f }, /* BOOT_BOOT        - vivid violet */
    { 0.05f, 0.08f, 0.26f, 1.0f }, /* FAT12            - blue         */
    { 0.05f, 0.18f, 0.20f, 1.0f }, /* DIRECTORY        - teal         */
    { 0.06f, 0.13f, 0.15f, 1.0f }, /* DIRECTORY_DELETED- muted teal   */
    { 0.05f, 0.20f, 0.07f, 1.0f }, /* CODE_DEMO        - green        */
    { 0.05f, 0.16f, 0.05f, 1.0f }, /* CODE_GEMDOS      - softer green */
    { 0.18f, 0.13f, 0.03f, 1.0f }, /* CODE_PACKER      - amber        */
    { 0.14f, 0.14f, 0.05f, 1.0f }, /* DATA_SINUS       - yellow       */
    { 0.14f, 0.11f, 0.09f, 1.0f }, /* DATA_ASCII       - warm gray    */
    { 0.10f, 0.10f, 0.16f, 1.0f }, /* DATA_BINARY      - cool blue-gray*/
    { 0.18f, 0.10f, 0.03f, 1.0f }, /* DATA_PACKED      - orange       */
    { 0.04f, 0.04f, 0.05f, 1.0f }, /* BSS_ZERO         - near-black   */
    { 0.24f, 0.05f, 0.05f, 1.0f }, /* UNFORMATTED      - dark red     */
    { 0.24f, 0.11f, 0.03f, 1.0f }, /* PROTECTION       - dark rust    */
};

/* ------------------------------------------------------------------
 * Colours — info band (UC24C)
 * ------------------------------------------------------------------ */

static const renderer_color_t g_clrBandBg   = { 0.08f, 0.08f, 0.12f,
                                                  1.0f };
static const renderer_color_t g_clrBandText = { 0.78f, 0.78f, 0.90f,
                                                  1.0f };
static const renderer_color_t g_clrBandNote = { 0.55f, 0.75f, 0.55f,
                                                  1.0f };
static const renderer_color_t g_clrBandNoteActive = { 0.90f, 0.90f,
                                                        0.40f, 1.0f };

/* ------------------------------------------------------------------
 * Hex nibble lookup
 * ------------------------------------------------------------------ */

static const char g_szNib[] = "0123456789ABCDEF";

/* ------------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------------ */

static st_error_t ehex_load(edit_hex_view_t *ptV, const char *szPath);
static void       ehex_classify_sectors(edit_hex_view_t *ptV);
static st_error_t ehex_save(edit_hex_view_t *ptV);
static void       ehex_update_title(edit_hex_view_t *ptV);
static void       ehex_recalc_layout(edit_hex_view_t *ptV);
static int        ehex_row_count(const edit_hex_view_t *ptV);
static int        ehex_hex_area_h(const edit_hex_view_t *ptV);
static void       ehex_scroll_to_cursor(edit_hex_view_t *ptV);
static void       ehex_build_hex_row(const edit_hex_view_t *ptV,
                                      int iRow, char *szOut);
static void       ehex_build_asc_row(const edit_hex_view_t *ptV,
                                      int iRow, char *szOut);
static void       ehex_band_sync(edit_hex_view_t *ptV);
static void       ehex_render_band(edit_hex_view_t *ptV);
static void       ehex_render(edit_hex_view_t *ptV);
static void       ehex_handle_key(edit_hex_view_t *ptV,
                                   gui_key_t eKey, char cChar,
                                   st_u8_t uiMods);
static void       ehex_handle_click(edit_hex_view_t *ptV,
                                     int iX, int iY);
static void       ehex_event_callback(gui_window_t hWnd,
                                       gui_event_t *ptEvent,
                                       void        *pCtx);

/* Disassembly helpers (UC10) */
static void       ehex_disasm_cache_update(edit_hex_view_t *ptV);
static void       ehex_disasm_find_cursor(edit_hex_view_t *ptV);
static void       ehex_disasm_scroll_to_cursor(edit_hex_view_t *ptV);
static void       ehex_disasm_render(edit_hex_view_t *ptV);

/* ------------------------------------------------------------------
 * File I/O
 * ------------------------------------------------------------------ */

static st_error_t ehex_load(edit_hex_view_t *ptV, const char *szPath)
{
    file_stat_t tStat;
    file_t     *ptFile = NULL;
    st_u8_t    *pBuf   = NULL;
    size_t      uiRead = 0;

    memset(&tStat, 0, sizeof(tStat));
    if (file_stat(szPath, &tStat) != ST_NO_ERROR)
    {
        LOG_ERROR("file_stat failed for '%s'", szPath);
        return ST_ERROR;
    }
    if (!tStat.bExists)
    {
        LOG_ERROR("file not found: '%s'", szPath);
        return ST_ERROR;
    }
    if (tStat.bIsDir)
    {
        LOG_ERROR("'%s' is a directory", szPath);
        return ST_ERROR;
    }
    if (tStat.uiSize > EDIT_HEX_MAX_SIZE)
    {
        LOG_ERROR("file too large (%llu bytes, max %u)",
                  (unsigned long long)tStat.uiSize,
                  (unsigned)EDIT_HEX_MAX_SIZE);
        return ST_ERROR;
    }

    /* Allocate buffer; always at least 1 byte so pData != NULL        */
    pBuf = (st_u8_t *)malloc(tStat.uiSize > 0
                              ? (size_t)tStat.uiSize : 1u);
    if (pBuf == NULL)
    {
        LOG_ERROR("malloc failed for %llu bytes",
                  (unsigned long long)tStat.uiSize);
        return ST_ERROR;
    }

    if (tStat.uiSize > 0)
    {
        if (file_open(szPath, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
        {
            LOG_ERROR("file_open failed for '%s'", szPath);
            free(pBuf);
            return ST_ERROR;
        }
        if (file_read(ptFile, pBuf, (size_t)tStat.uiSize, &uiRead)
            != ST_NO_ERROR)
        {
            LOG_ERROR("file_read failed for '%s'", szPath);
            file_close(&ptFile);
            free(pBuf);
            return ST_ERROR;
        }
        file_close(&ptFile);
    }

    ptV->pData  = pBuf;
    ptV->uiSize = (size_t)tStat.uiSize;
    return ST_NO_ERROR;
}

/* Read 16-bit little-endian at offset off in buffer p */
#define EHEX_BPB16(p, off) \
    ((st_u16_t)((p)[(off)] | ((unsigned)(p)[(off)+1u] << 8)))

/* Read 32-bit big-endian at offset off in buffer p (PRG header) */
#define EHEX_PRG_BE32(p, off) \
    (((st_u32_t)(p)[(off)]     << 24) | ((st_u32_t)(p)[(off)+1u] << 16) \
   | ((st_u32_t)(p)[(off)+2u] <<  8) |  (st_u32_t)(p)[(off)+3u])

/*
 * If sector 0 contains a valid FAT12 BPB, assign structural sector
 * types directly (deterministic, no ML needed for boot/FAT/dir).
 * Returns ST_TRUE on success; data sectors stay SECTOR_UNKNOWN.
 */
static st_bool_t ehex_classify_by_bpb(edit_hex_view_t *ptV)
{
    const st_u8_t *p = ptV->pData;
    st_u16_t bps, res, rde, spf, spt, hds;
    st_u8_t  spc, nfat, media;
    int      iFat1, iRoot, iRootSecs;
    int      iSec, iFatCopy;
    st_u32_t uiSum;
    int      i;
    st_bool_t bBoot;

    bps   = EHEX_BPB16(p, 0x0Bu);
    spc   = p[0x0Du];
    res   = EHEX_BPB16(p, 0x0Eu);
    nfat  = p[0x10u];
    rde   = EHEX_BPB16(p, 0x11u);
    media = p[0x15u];
    spf   = EHEX_BPB16(p, 0x16u);
    spt   = EHEX_BPB16(p, 0x18u);
    hds   = EHEX_BPB16(p, 0x1Au);

    if (bps != 512u) return ST_FALSE;
    if (spc == 0u || (spc & (spc - 1u)) != 0u) return ST_FALSE;
    if (res < 1u) return ST_FALSE;
    if (nfat < 1u || nfat > 2u) return ST_FALSE;
    if (rde == 0u) return ST_FALSE;
    if (media < 0xF0u) return ST_FALSE;
    if (spf == 0u) return ST_FALSE;
    if (spt < 1u || spt > 11u) return ST_FALSE;
    if (hds < 1u || hds > 2u) return ST_FALSE;

    /* Sector ranges */
    iFat1     = (int)res;
    iRoot     = iFat1 + (int)nfat * (int)spf;
    iRootSecs = ((int)rde * 32 + 511) / 512;

    /* Bootsector: WD1772 checksum determines bootable flag */
    uiSum = 0u;
    for (i = 0; i < 256; i++)
        uiSum += ((st_u32_t)p[i * 2] << 8) | (st_u32_t)p[i * 2 + 1];
    bBoot = ((uiSum & 0xFFFFu) == 0x1234u) ? ST_TRUE : ST_FALSE;
    if (ptV->iSecCount > 0)
        ptV->aeSecType[0] = bBoot ? SECTOR_BOOTSECTOR_BOOT
                                  : SECTOR_BOOTSECTOR_NOBOOT;

    /* Reserved sectors beyond boot (uncommon, mark as boot) */
    for (iSec = 1; iSec < iFat1 && iSec < ptV->iSecCount; iSec++)
        ptV->aeSecType[iSec] = SECTOR_BOOTSECTOR_NOBOOT;

    /* FAT sectors (all copies) */
    for (iFatCopy = 0; iFatCopy < (int)nfat; iFatCopy++)
    {
        int iBase = iFat1 + iFatCopy * (int)spf;
        for (iSec = iBase;
             iSec < iBase + (int)spf && iSec < ptV->iSecCount;
             iSec++)
            ptV->aeSecType[iSec] = SECTOR_FAT12;
    }

    /* Root directory */
    for (iSec = iRoot;
         iSec < iRoot + iRootSecs && iSec < ptV->iSecCount;
         iSec++)
        ptV->aeSecType[iSec] = SECTOR_DIRECTORY;

    /* Cache BPB layout for info band (UC24C) */
    ptV->iBpbFat1Lba = iFat1;
    ptV->iBpbFat2Lba = ((int)nfat >= 2) ? iFat1 + (int)spf : -1;
    ptV->iBpbRootLba = iRoot;
    ptV->iBpbDataLba = iRoot + iRootSecs;

    LOG_INFO("BPB layout: boot=0 FAT1=%d..%d root=%d..%d data=%d..",
             iFat1, iFat1 + (int)spf - 1,
             iRoot, iRoot + iRootSecs - 1,
             iRoot + iRootSecs);
    return ST_TRUE;
}

/*
 * If sector iSec has PRG magic 0x601A and a valid header, classify
 * iSec and subsequent sectors by section (.text / .data / .symbol).
 * Only fires when the sector is still SECTOR_UNKNOWN.
 */
static void ehex_classify_prg_at(edit_hex_view_t *ptV, int iSec)
{
    const st_u8_t *p = ptV->pData + (size_t)iSec * SA_SECTOR_SIZE;
    st_u32_t       text_sz, data_sz, sym_sz;
    st_u32_t       text_end, data_end, sym_end;
    st_u32_t       uiTotal, uiFileOff;
    size_t         uiRemain;
    int            nSecs, s;

    text_sz  = EHEX_PRG_BE32(p,  2);
    data_sz  = EHEX_PRG_BE32(p,  6);
    sym_sz   = EHEX_PRG_BE32(p, 14);

    if (text_sz == 0u) return; /* empty .text is not a valid PRG */

    uiTotal  = 28u + text_sz + data_sz + sym_sz;
    uiRemain = ptV->uiSize - (size_t)iSec * SA_SECTOR_SIZE;
    if ((size_t)uiTotal > uiRemain) return; /* sizes corrupt */

    text_end = 28u + text_sz;
    data_end = text_end + data_sz;
    sym_end  = data_end + sym_sz;

    nSecs = (int)((uiTotal + 511u) / 512u);
    for (s = 0; s < nSecs && (iSec + s) < ptV->iSecCount; s++)
    {
        uiFileOff = (st_u32_t)s * 512u;
        if (uiFileOff < text_end)
            ptV->aeSecType[iSec + s] = SECTOR_CODE_DEMO;
        else if (uiFileOff < data_end)
            ptV->aeSecType[iSec + s] = SECTOR_DATA_BINARY;
        else if (uiFileOff < sym_end)
            ptV->aeSecType[iSec + s] = SECTOR_DATA_ASCII;
    }
}

/*
 * Returns ST_TRUE if the sector looks like a FAT12 directory cluster:
 * >= 10 of 16 slots valid (printable name, valid attr, or 0x00/0xE5)
 * AND at least 1 real non-zero entry (avoids mistaking BSS for dir).
 */
static st_bool_t ehex_sector_is_subdir(const st_u8_t *p)
{
    int       iValid = 0, iNonZero = 0;
    int       i, j;
    st_u8_t   uiF, uiA;
    st_bool_t bOk;

    for (i = 0; i < 16; i++)
    {
        uiF = p[i * 32];
        uiA = p[i * 32 + 11];

        if (uiF == 0x00u) { iValid++; continue; }
        if (uiF == 0xE5u) { iValid++; continue; }

        bOk = ST_TRUE;
        for (j = 0; j < 11 && bOk; j++)
        {
            st_u8_t c = p[i * 32 + j];
            bOk = (c >= 0x20u && c <= 0x7Eu) ? ST_TRUE : ST_FALSE;
        }
        if (bOk == ST_TRUE && (uiA & 0xC0u) == 0u)
        {
            iValid++;
            iNonZero++;
        }
    }
    return (iValid >= 10 && iNonZero >= 1) ? ST_TRUE : ST_FALSE;
}

/*
 * Classify every SA_SECTOR_SIZE-byte block in pData.
 * Phase 1: BPB-based layout (structural sectors — deterministic).
 * Phase 2: PRG magic scan (.text / .data / .symbol coloring).
 * Phase 3: Subdirectory scan (dir_density heuristic).
 * Phase 4: ML classifier for remaining SECTOR_UNKNOWN sectors.
 * Non-fatal: any failure simply leaves iSecCount == 0.
 */
static void ehex_classify_sectors(edit_hex_view_t *ptV)
{
    sector_db_t       *ptDb   = NULL;
    sector_features_t  tFeat;
    sector_match_t     tMatch;
    int                iCount = 0;
    int                iSec;
    int                iTotal;
    const st_u8_t     *pSector;
    st_bool_t          bBpb;

    if (ptV->pData == NULL || ptV->uiSize < SA_SECTOR_SIZE)
        return;

    iTotal = (int)(ptV->uiSize / SA_SECTOR_SIZE);
    if (iTotal <= 0) return;

    ptV->aeSecType = (sector_type_t *)malloc(
                      (size_t)iTotal * sizeof(sector_type_t));
    if (ptV->aeSecType == NULL)
    {
        LOG_ERROR("malloc failed for aeSecType (%d sectors)", iTotal);
        return;
    }
    for (iSec = 0; iSec < iTotal; iSec++)
        ptV->aeSecType[iSec] = SECTOR_UNKNOWN;
    ptV->iSecCount = iTotal;

    /* Phase 1 — BPB layout (reliable for disk images) */
    bBpb = ehex_classify_by_bpb(ptV);

    /* Phase 2 — PRG magic scan (Atari ST executables) */
    for (iSec = 0; iSec < iTotal; iSec++)
    {
        if (ptV->aeSecType[iSec] != SECTOR_UNKNOWN) continue;
        pSector = ptV->pData + (size_t)iSec * SA_SECTOR_SIZE;
        if (pSector[0] == 0x60u && pSector[1] == 0x1Au)
            ehex_classify_prg_at(ptV, iSec);
    }

    /* Phase 3 — Subdirectory scan (data-area dir clusters) */
    for (iSec = 0; iSec < iTotal; iSec++)
    {
        if (ptV->aeSecType[iSec] != SECTOR_UNKNOWN) continue;
        pSector = ptV->pData + (size_t)iSec * SA_SECTOR_SIZE;
        if (ehex_sector_is_subdir(pSector) == ST_TRUE)
            ptV->aeSecType[iSec] = SECTOR_DIRECTORY;
    }

    /* Phase 4 — ML classifier for remaining SECTOR_UNKNOWN */
    if (sector_db_create(&ptDb) != ST_NO_ERROR)
    {
        LOG_ERROR("sector_db_create failed — ML pass skipped");
        return;
    }
    sector_db_bootstrap_defaults(ptDb);
    sector_db_load(ptDb, "db/sector_db.bin"); /* ignore absence */

    for (iSec = 0; iSec < iTotal; iSec++)
    {
        if (ptV->aeSecType[iSec] != SECTOR_UNKNOWN) continue;

        pSector = ptV->pData + (size_t)iSec * SA_SECTOR_SIZE;
        memset(&tFeat, 0, sizeof(tFeat));
        if (sector_features_extract(pSector,
                                     (st_u32_t)iSec,
                                     &tFeat) == ST_NO_ERROR
        &&  sector_classify(ptDb, &tFeat, pSector,
                             &tMatch, 1, &iCount) == ST_NO_ERROR
        &&  iCount > 0)
        {
            ptV->aeSecType[iSec] = tMatch.eType;
        }
    }
    sector_db_destroy(&ptDb);
    LOG_INFO("sector tinting: %d sectors (BPB=%s)",
             iTotal, bBpb ? "yes" : "no");
}

static st_error_t ehex_save(edit_hex_view_t *ptV)
{
    file_t *ptFile = NULL;

    if (ptV->uiSize == 0) return ST_NO_ERROR;

    if (file_open(ptV->szPath, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
    {
        LOG_ERROR("file_open(WRITE) failed for '%s'", ptV->szPath);
        return ST_ERROR;
    }
    if (file_write(ptFile, ptV->pData, ptV->uiSize) != ST_NO_ERROR)
    {
        LOG_ERROR("file_write failed for '%s'", ptV->szPath);
        file_close(&ptFile);
        return ST_ERROR;
    }
    file_close(&ptFile);
    ptV->bDirty = ST_FALSE;
    LOG_INFO("saved '%s' (%zu bytes)", ptV->szPath, ptV->uiSize);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Title and layout
 * ------------------------------------------------------------------ */

static void ehex_update_title(edit_hex_view_t *ptV)
{
    char        szTitle[ST_MAX_PATH + 64];
    const char *szBase = ptV->szPath;
    const char *p;

    for (p = ptV->szPath; *p; p++)
    {
        if (*p == '/' || *p == '\\') szBase = p + 1;
    }
    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Hex: %s%s", szBase,
             ptV->bDirty ? " [*]" : "");
    gui_set_title(ptV->hWnd, szTitle);
}

static void ehex_recalc_layout(edit_hex_view_t *ptV)
{
    int iAsciiEnd;

    /* iAddrX = 0 (address column starts at left edge)                 */
    ptV->iAddrX  = 0;
    /* iHexX = after address column: (ADDR_CHARS + ADDR_PAD) cells     */
    ptV->iHexX   = (EDIT_HEX_ADDR_CHARS + EDIT_HEX_ADDR_PAD)
                   * ptV->iCellW;
    /* iAsciiX = after hex area + separator                            */
    ptV->iAsciiX = ptV->iHexX
                 + (EDIT_HEX_HEX_CHARS + EDIT_HEX_SEP_CHARS)
                 * ptV->iCellW;

    /* iDisasmX = after ASCII area + disasm separator                  */
    iAsciiEnd    = ptV->iAsciiX + EDIT_HEX_ASCII_CHARS * ptV->iCellW;
    if (ptV->bShowDisasm)
        ptV->iDisasmX = iAsciiEnd
                      + EDIT_HEX_DISASM_SEP_CHARS * ptV->iCellW;
    else
        ptV->iDisasmX = 0;
}

static int ehex_row_count(const edit_hex_view_t *ptV)
{
    if (ptV->uiSize == 0) return 1;
    return (int)((ptV->uiSize + EDIT_HEX_BYTES_PER_ROW - 1)
                 / EDIT_HEX_BYTES_PER_ROW);
}

/* Height of the hex+disasm area (window height minus info band). */
static int ehex_hex_area_h(const edit_hex_view_t *ptV)
{
    int iBandH = (ptV->iCellH > 0)
                 ? HEXED_BAND_ROWS * ptV->iCellH : 0;
    int iAvail = ptV->iWndHeight - iBandH;
    return (iAvail > 0) ? iAvail : ptV->iWndHeight;
}

static void ehex_scroll_to_cursor(edit_hex_view_t *ptV)
{
    int iCurRow;
    int iVisRows;

    iCurRow  = (int)(ptV->uiCursor / EDIT_HEX_BYTES_PER_ROW);
    iVisRows = (ptV->iCellH > 0)
               ? ehex_hex_area_h(ptV) / ptV->iCellH : 1;
    if (iVisRows < 1) iVisRows = 1;

    if (iCurRow < ptV->iScrollRow)
        ptV->iScrollRow = iCurRow;
    if (iCurRow >= ptV->iScrollRow + iVisRows)
        ptV->iScrollRow = iCurRow - iVisRows + 1;
}

/* ------------------------------------------------------------------
 * Row string builders
 * ------------------------------------------------------------------ */

/* szOut must be at least EDIT_HEX_HEX_CHARS+1 bytes */
static void ehex_build_hex_row(const edit_hex_view_t *ptV,
                                int iRow, char *szOut)
{
    int    i;
    size_t uiBase = (size_t)iRow * EDIT_HEX_BYTES_PER_ROW;

    memset(szOut, ' ', EDIT_HEX_HEX_CHARS);
    szOut[EDIT_HEX_HEX_CHARS] = '\0';

    for (i = 0; i < EDIT_HEX_BYTES_PER_ROW; i++)
    {
        /* Position in the string: col*3 + (col>=8 ? 1 : 0)            */
        int iPos = i * 3 + (i >= 8 ? 1 : 0);
        if (uiBase + (size_t)i < ptV->uiSize)
        {
            unsigned b      = ptV->pData[uiBase + (size_t)i];
            szOut[iPos]     = g_szNib[b >> 4];
            szOut[iPos + 1] = g_szNib[b & 0xF];
        }
        /* else: spaces already set */
    }
}

/* szOut must be at least EDIT_HEX_ASCII_CHARS+1 bytes */
static void ehex_build_asc_row(const edit_hex_view_t *ptV,
                                int iRow, char *szOut)
{
    int    i;
    size_t uiBase = (size_t)iRow * EDIT_HEX_BYTES_PER_ROW;

    for (i = 0; i < EDIT_HEX_BYTES_PER_ROW; i++)
    {
        if (uiBase + (size_t)i < ptV->uiSize)
        {
            unsigned b = ptV->pData[uiBase + (size_t)i];
            szOut[i] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
        }
        else
        {
            szOut[i] = ' ';
        }
    }
    szOut[EDIT_HEX_ASCII_CHARS] = '\0';
}

/* ------------------------------------------------------------------
 * Disassembly cache helpers (UC10)
 * ------------------------------------------------------------------ */

/*
 * Rebuild the disasm cache centered around uiCursor.  Called after
 * any cursor move.  Updates iDisasmCursorIdx via
 * ehex_disasm_find_cursor().
 */
static void ehex_disasm_cache_update(edit_hex_view_t *ptV)
{
    size_t uiStart;
    size_t uiAvail;
    size_t uiLines = 0;

    if (ptV->atDisasmCache == NULL) return;
    if (ptV->pData == NULL || ptV->uiSize < 2)
    {
        ptV->iDisasmCacheCount = 0;
        return;
    }

    /* Start PREBUF_BYTES before cursor, aligned to 2-byte boundary    */
    if (ptV->uiCursor > EDIT_HEX_DISASM_PREBUF_BYTES)
        uiStart = (ptV->uiCursor - EDIT_HEX_DISASM_PREBUF_BYTES) & ~1u;
    else
        uiStart = 0u;

    uiAvail               = ptV->uiSize - uiStart;
    ptV->uiDisasmCacheBase = (st_u32_t)uiStart;

    disasm_range(ptV->pData + uiStart,
                 uiAvail,
                 (st_u32_t)uiStart,
                 ptV->atDisasmCache,
                 (size_t)EDIT_HEX_DISASM_CACHE_LINES,
                 &uiLines);
    ptV->iDisasmCacheCount = (int)uiLines;

    ehex_disasm_find_cursor(ptV);
}

/*
 * Find the cache index of the instruction that contains uiCursor.
 * Updates iDisasmCursorIdx and calls ehex_disasm_scroll_to_cursor().
 */
static void ehex_disasm_find_cursor(edit_hex_view_t *ptV)
{
    int      i;
    st_u32_t uiCur  = (st_u32_t)ptV->uiCursor;
    int      iCount = ptV->iDisasmCacheCount;

    ptV->iDisasmCursorIdx = 0;
    if (iCount <= 0) return;

    for (i = 0; i < iCount - 1; i++)
    {
        st_u32_t uiNext = ptV->atDisasmCache[i + 1].uiAddr;
        if (ptV->atDisasmCache[i].uiAddr <= uiCur && uiCur < uiNext)
        {
            ptV->iDisasmCursorIdx = i;
            ehex_disasm_scroll_to_cursor(ptV);
            return;
        }
    }
    /* Cursor is at or past the last cached instruction */
    ptV->iDisasmCursorIdx = iCount - 1;
    ehex_disasm_scroll_to_cursor(ptV);
}

/*
 * Ensure the disasm cursor instruction is visible in the panel.
 */
static void ehex_disasm_scroll_to_cursor(edit_hex_view_t *ptV)
{
    int iVisRows;
    int iIdx = ptV->iDisasmCursorIdx;

    iVisRows = (ptV->iCellH > 0)
               ? ehex_hex_area_h(ptV) / ptV->iCellH : 1;
    if (iVisRows < 1) iVisRows = 1;

    if (iIdx < ptV->iDisasmScrollRow)
        ptV->iDisasmScrollRow = iIdx;
    if (iIdx >= ptV->iDisasmScrollRow + iVisRows)
        ptV->iDisasmScrollRow = iIdx - iVisRows + 1;
    if (ptV->iDisasmScrollRow < 0)
        ptV->iDisasmScrollRow = 0;
}

/*
 * Render the disassembly panel (separator + instruction lines).
 * Called from ehex_render() when bShowDisasm is TRUE.
 */
static void ehex_disasm_render(edit_hex_view_t *ptV)
{
    int             iVisRows;
    int             iRow;
    int             iAbsRow;
    float           fY;
    int             iSep2X;
    float           fPanelW;
    renderer_rect_t tRect;
    char            szText[DISASM_LINE_MAX];

    if (ptV->iDisasmX <= 0 || ptV->hRenderer == NULL) return;

    iSep2X   = ptV->iAsciiX + EDIT_HEX_ASCII_CHARS * ptV->iCellW;
    fPanelW  = (float)(EDIT_HEX_DISASM_PANEL_CHARS * ptV->iCellW);
    iVisRows = (ptV->iCellH > 0)
               ? ehex_hex_area_h(ptV) / ptV->iCellH : 1;
    if (iVisRows < 1) iVisRows = 1;

    /* Separator background */
    tRect.fX = (float)iSep2X;
    tRect.fY = 0.0f;
    tRect.fW = (float)(EDIT_HEX_DISASM_SEP_CHARS * ptV->iCellW);
    tRect.fH = (float)ptV->iWndHeight;
    renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrDisasmSepBg);

    /* Separator text " | " drawn on each row */

    for (iRow = 0; iRow < iVisRows; iRow++)
    {
        iAbsRow = ptV->iDisasmScrollRow + iRow;
        if (iAbsRow < 0 || iAbsRow >= ptV->iDisasmCacheCount)
            break;

        fY = (float)(iRow * ptV->iCellH);

        /* Separator text */
        tRect.fX = (float)iSep2X;
        tRect.fY = fY;
        tRect.fW = (float)(EDIT_HEX_DISASM_SEP_CHARS * ptV->iCellW);
        tRect.fH = (float)ptV->iCellH;
        renderer_draw_text(ptV->hRenderer, " | ", &tRect,
                           &g_clrSep, RENDERER_FONT_MONO,
                           RENDERER_ALIGN_LEFT);

        /* Highlight bar for cursor instruction */
        if (iAbsRow == ptV->iDisasmCursorIdx)
        {
            tRect.fX = (float)ptV->iDisasmX;
            tRect.fY = fY;
            tRect.fW = fPanelW;
            tRect.fH = (float)ptV->iCellH;
            renderer_fill_rect(ptV->hRenderer, &tRect,
                               (ptV->eZone == HEX_ZONE_DISASM)
                               ? &g_clrDisasmHL : &g_clrCurRow);
        }

        /* Instruction text: "$XXXXXX %-8s %s"                         */
        snprintf(szText, sizeof(szText), "$%06X %-8s %s",
                 (unsigned)ptV->atDisasmCache[iAbsRow].uiAddr,
                 ptV->atDisasmCache[iAbsRow].szMnemonic,
                 ptV->atDisasmCache[iAbsRow].szOperands);

        {
            const renderer_color_t *ptClr;

            if (iAbsRow == ptV->iDisasmCursorIdx
            &&  ptV->eZone == HEX_ZONE_DISASM)
                ptClr = &g_clrCurTxt;
            else if (ptV->atDisasmCache[iAbsRow].bValid)
                ptClr = &g_clrDisasm;
            else
                ptClr = &g_clrDisasmDC;

            tRect.fX = (float)ptV->iDisasmX;
            tRect.fY = fY;
            tRect.fW = fPanelW;
            tRect.fH = (float)ptV->iCellH;
            renderer_draw_text(ptV->hRenderer, szText, &tRect,
                               ptClr, RENDERER_FONT_MONO,
                               RENDERER_ALIGN_LEFT);
        }
    }
}

/* ------------------------------------------------------------------
 * Info band helpers (UC24C)
 * ------------------------------------------------------------------ */

/*
 * Sync szBandNote / iBandNotePos from ptAnnot for the current sector.
 * No-op when zone is BAND_NOTE (preserves the edit buffer).
 */
static void ehex_band_sync(edit_hex_view_t *ptV)
{
    int                   iCurSec;
    const annot_sector_t *ptSec;

    if (ptV->eZone == HEX_ZONE_BAND_NOTE)
        return;

    iCurSec = (int)(ptV->uiCursor / SA_SECTOR_SIZE);
    if (iCurSec == ptV->iBandLastSec)
        return;

    ptV->iBandLastSec = iCurSec;
    ptSec = image_annot_get_sector(ptV->ptAnnot, iCurSec);
    if (ptSec != NULL)
    {
        strncpy(ptV->szBandNote, ptSec->szNotes,
                ANNOT_NOTE_MAX - 1);
        ptV->szBandNote[ANNOT_NOTE_MAX - 1] = '\0';
    }
    else
    {
        ptV->szBandNote[0] = '\0';
    }
    ptV->iBandNotePos = (int)strlen(ptV->szBandNote);
}

/*
 * Render the two-row info band at the bottom of the window.
 *   Row 1: current sector LBA, type name, BPB layout if available.
 *   Row 2: annotation note (editable when zone is BAND_NOTE).
 */
static void ehex_render_band(edit_hex_view_t *ptV)
{
    renderer_rect_t         tRect;
    char                    szLine1[160];
    char                    szLine2[ANNOT_NOTE_MAX + 32];
    char                    szBuf[ANNOT_NOTE_MAX + 4];
    int                     iBandH;
    int                     iTop;
    int                     iCurSec;
    const char             *szTypeName;
    const annot_sector_t   *ptSec;
    int                     iPos;
    int                     iLen;

    if (ptV->hRenderer == NULL || ptV->iCellH <= 0)
        return;

    iBandH  = HEXED_BAND_ROWS * ptV->iCellH;
    iTop    = ptV->iWndHeight - iBandH;
    if (iTop < 0)
        iTop = 0;

    iCurSec = (int)(ptV->uiCursor / SA_SECTOR_SIZE);

    /* -- Background ------------------------------------------------- */
    tRect.fX = 0.0f;
    tRect.fY = (float)iTop;
    tRect.fW = (float)ptV->iWndWidth;
    tRect.fH = (float)iBandH;
    renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrBandBg);

    /* Separator line at top of band */
    tRect.fX = 0.0f;
    tRect.fY = (float)iTop;
    tRect.fW = (float)ptV->iWndWidth;
    tRect.fH = 1.0f;
    renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrSep);

    /* -- Row 1: sector info + BPB layout ----------------------------- */
    szTypeName = "—";
    if (ptV->iSecCount > 0
    &&  iCurSec >= 0 && iCurSec < ptV->iSecCount)
        szTypeName = sector_type_name(ptV->aeSecType[iCurSec]);

    if (ptV->iBpbFat1Lba >= 0)
    {
        if (ptV->iBpbFat2Lba >= 0)
            snprintf(szLine1, sizeof(szLine1),
                     " Sec %-4d  %-22s  "
                     "FAT1@%d  FAT2@%d  Root@%d  Data@%d",
                     iCurSec, szTypeName,
                     ptV->iBpbFat1Lba,
                     ptV->iBpbFat2Lba,
                     ptV->iBpbRootLba,
                     ptV->iBpbDataLba);
        else
            snprintf(szLine1, sizeof(szLine1),
                     " Sec %-4d  %-22s  "
                     "FAT1@%d  Root@%d  Data@%d",
                     iCurSec, szTypeName,
                     ptV->iBpbFat1Lba,
                     ptV->iBpbRootLba,
                     ptV->iBpbDataLba);
    }
    else
    {
        snprintf(szLine1, sizeof(szLine1),
                 " Sec %-4d  %s",
                 iCurSec, szTypeName);
    }

    tRect.fX = 0.0f;
    tRect.fY = (float)iTop;
    tRect.fW = (float)ptV->iWndWidth;
    tRect.fH = (float)ptV->iCellH;
    renderer_draw_text(ptV->hRenderer, szLine1, &tRect,
                       &g_clrBandText, RENDERER_FONT_MONO,
                       RENDERER_ALIGN_LEFT);

    /* -- Row 2: annotation note -------------------------------------- */
    ptSec = image_annot_get_sector(ptV->ptAnnot, iCurSec);

    if (ptV->eZone == HEX_ZONE_BAND_NOTE)
    {
        /* Show edit buffer with insertion cursor '|' */
        iPos = ptV->iBandNotePos;
        iLen = (int)strlen(ptV->szBandNote);
        if (iPos < 0)      iPos = 0;
        if (iPos > iLen)   iPos = iLen;

        memcpy(szBuf, ptV->szBandNote, (size_t)iPos);
        szBuf[iPos] = '|';
        memcpy(szBuf + iPos + 1,
               ptV->szBandNote + iPos,
               (size_t)(iLen - iPos) + 1u); /* +1 for NUL */

        snprintf(szLine2, sizeof(szLine2), " Note> %s", szBuf);
    }
    else if (ptSec != NULL && ptSec->szNotes[0] != '\0')
    {
        snprintf(szLine2, sizeof(szLine2),
                 " Note: %s", ptSec->szNotes);
    }
    else
    {
        snprintf(szLine2, sizeof(szLine2),
                 " Note: (none — CTRL+N or click to annotate)");
    }

    tRect.fX = 0.0f;
    tRect.fY = (float)(iTop + ptV->iCellH);
    tRect.fW = (float)ptV->iWndWidth;
    tRect.fH = (float)ptV->iCellH;
    renderer_draw_text(ptV->hRenderer, szLine2, &tRect,
                       (ptV->eZone == HEX_ZONE_BAND_NOTE)
                       ? &g_clrBandNoteActive : &g_clrBandNote,
                       RENDERER_FONT_MONO,
                       RENDERER_ALIGN_LEFT);
}

/* ------------------------------------------------------------------
 * Rendering
 * ------------------------------------------------------------------ */

static void ehex_render(edit_hex_view_t *ptV)
{
    int    iVisRows;
    int    iRowCount;
    int    iRow;
    int    iAbsRow;
    float  fY;
    char   szHex[EDIT_HEX_HEX_CHARS + 1];
    char   szAsc[EDIT_HEX_ASCII_CHARS + 1];
    char   szAddr[16];
    char   szCur[3];
    renderer_rect_t tRect;
    int    iCurRow;
    int    iCurCol;
    int    iHexCurX;
    int    iAscCurX;
    int    iSepX;

    if (ptV->hRenderer == NULL) return;

    iVisRows = (ptV->iCellH > 0)
               ? ehex_hex_area_h(ptV) / ptV->iCellH : 1;
    iRowCount = ehex_row_count(ptV);
    iCurRow   = (int)(ptV->uiCursor / EDIT_HEX_BYTES_PER_ROW);
    iCurCol   = (int)(ptV->uiCursor % EDIT_HEX_BYTES_PER_ROW);
    iSepX     = ptV->iHexX + EDIT_HEX_HEX_CHARS * ptV->iCellW;

    /* Hex cursor X: col*3 + (col>=8?1:0) + nibble → cell             */
    iHexCurX = ptV->iHexX
             + (iCurCol * 3 + (iCurCol >= 8 ? 1 : 0) + ptV->iNibble)
             * ptV->iCellW;
    /* ASCII cursor X */
    iAscCurX = ptV->iAsciiX + iCurCol * ptV->iCellW;

    renderer_begin_draw(ptV->hRenderer, &g_clrBg);

    /* Address gutter background */
    tRect.fX = 0.0f;
    tRect.fY = 0.0f;
    tRect.fW = (float)ptV->iHexX;
    tRect.fH = (float)ptV->iWndHeight;
    renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrAddrBg);

    /* Separator background between hex and ASCII */
    tRect.fX = (float)iSepX;
    tRect.fW = (float)(EDIT_HEX_SEP_CHARS * ptV->iCellW);
    tRect.fY = 0.0f;
    tRect.fH = (float)ptV->iWndHeight;
    renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrAddrBg);

    for (iRow = 0; iRow < iVisRows; iRow++)
    {
        iAbsRow = ptV->iScrollRow + iRow;
        if (iAbsRow >= iRowCount) break;

        fY = (float)(iRow * ptV->iCellH);

        /* Sector tint (UC24B) — drawn before cursor-row highlight       */
        if (ptV->iSecCount > 0)
        {
            int iSec = (iAbsRow * EDIT_HEX_BYTES_PER_ROW)
                       / (int)SA_SECTOR_SIZE;
            if (iSec < ptV->iSecCount
            &&  ptV->aeSecType[iSec] != SECTOR_UNKNOWN)
            {
                tRect.fX = (float)ptV->iHexX;
                tRect.fY = fY;
                tRect.fW = (float)(ptV->iAsciiX - ptV->iHexX
                                   + EDIT_HEX_ASCII_CHARS * ptV->iCellW);
                tRect.fH = (float)ptV->iCellH;
                renderer_fill_rect(ptV->hRenderer, &tRect,
                                   &g_aSecTint[ptV->aeSecType[iSec]]);
            }
        }

        /* Current-row highlight */
        if (iAbsRow == iCurRow)
        {
            tRect.fX = (float)ptV->iHexX;
            tRect.fY = fY;
            tRect.fW = (float)(ptV->iAsciiX - ptV->iHexX
                                + EDIT_HEX_ASCII_CHARS * ptV->iCellW);
            tRect.fH = (float)ptV->iCellH;
            renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrCurRow);
        }

        /* Address */
        snprintf(szAddr, sizeof(szAddr), "%06X:",
                 (unsigned)(iAbsRow * EDIT_HEX_BYTES_PER_ROW));
        tRect.fX = 0.0f;
        tRect.fY = fY;
        tRect.fW = (float)ptV->iHexX;
        tRect.fH = (float)ptV->iCellH;
        renderer_draw_text(ptV->hRenderer, szAddr, &tRect,
                           &g_clrAddr, RENDERER_FONT_MONO,
                           RENDERER_ALIGN_LEFT);

        /* Hex bytes */
        ehex_build_hex_row(ptV, iAbsRow, szHex);
        tRect.fX = (float)ptV->iHexX;
        tRect.fY = fY;
        tRect.fW = (float)(EDIT_HEX_HEX_CHARS * ptV->iCellW);
        tRect.fH = (float)ptV->iCellH;
        renderer_draw_text(ptV->hRenderer, szHex, &tRect,
                           &g_clrHex, RENDERER_FONT_MONO,
                           RENDERER_ALIGN_LEFT);

        /* Separator text " | " */
        tRect.fX = (float)iSepX;
        tRect.fW = (float)(EDIT_HEX_SEP_CHARS * ptV->iCellW);
        renderer_draw_text(ptV->hRenderer, " | ", &tRect,
                           &g_clrSep, RENDERER_FONT_MONO,
                           RENDERER_ALIGN_LEFT);

        /* ASCII chars — draw each char with its own color              */
        ehex_build_asc_row(ptV, iAbsRow, szAsc);
        {
            int    iC;
            size_t uiBase = (size_t)iAbsRow * EDIT_HEX_BYTES_PER_ROW;
            for (iC = 0; iC < EDIT_HEX_BYTES_PER_ROW; iC++)
            {
                char szOne[2] = { szAsc[iC], '\0' };
                const renderer_color_t *ptClr =
                    (uiBase + (size_t)iC < ptV->uiSize
                     && szAsc[iC] != '.')
                    ? &g_clrAscii : &g_clrDot;
                tRect.fX = (float)(ptV->iAsciiX + iC * ptV->iCellW);
                tRect.fY = fY;
                tRect.fW = (float)ptV->iCellW;
                tRect.fH = (float)ptV->iCellH;
                renderer_draw_text(ptV->hRenderer, szOne, &tRect,
                                   ptClr, RENDERER_FONT_MONO,
                                   RENDERER_ALIGN_LEFT);
            }
        }

        /* Cursor overlay on the current row                            */
        if (iAbsRow == iCurRow)
        {
            if (ptV->eZone == HEX_ZONE_HEX
            &&  ptV->uiCursor < ptV->uiSize)
            {
                /* Filled rect over cursor nibble */
                tRect.fX = (float)iHexCurX;
                tRect.fY = fY;
                tRect.fW = (float)ptV->iCellW;
                tRect.fH = (float)ptV->iCellH;
                renderer_fill_rect(ptV->hRenderer, &tRect,
                                   &g_clrCurHex);
                /* Re-draw nibble char in contrast color */
                if (ptV->uiCursor < ptV->uiSize)
                {
                    unsigned b = ptV->pData[ptV->uiCursor];
                    szCur[0] = (ptV->iNibble == 0)
                               ? g_szNib[b >> 4]
                               : g_szNib[b & 0xF];
                    szCur[1] = '\0';
                    renderer_draw_text(ptV->hRenderer, szCur, &tRect,
                                       &g_clrCurTxt,
                                       RENDERER_FONT_MONO,
                                       RENDERER_ALIGN_LEFT);
                }
            }

            if (ptV->eZone == HEX_ZONE_ASCII
            &&  ptV->uiCursor < ptV->uiSize)
            {
                unsigned b = ptV->pData[ptV->uiCursor];
                tRect.fX = (float)iAscCurX;
                tRect.fY = fY;
                tRect.fW = (float)ptV->iCellW;
                tRect.fH = (float)ptV->iCellH;
                renderer_fill_rect(ptV->hRenderer, &tRect,
                                   &g_clrCurAsc);
                szCur[0] = (b >= 0x20 && b < 0x7F)
                           ? (char)b : '.';
                szCur[1] = '\0';
                renderer_draw_text(ptV->hRenderer, szCur, &tRect,
                                   &g_clrCurTxt,
                                   RENDERER_FONT_MONO,
                                   RENDERER_ALIGN_LEFT);
            }
        }
    }

    /* Disassembly panel (UC10) */
    if (ptV->bShowDisasm)
        ehex_disasm_render(ptV);

    /* Info band (UC24C) */
    ehex_band_sync(ptV);
    ehex_render_band(ptV);

    renderer_end_draw(ptV->hRenderer);
}

/* ------------------------------------------------------------------
 * Cursor movement helpers
 * ------------------------------------------------------------------ */

static void ehex_move(edit_hex_view_t *ptV, int iDelta)
{
    long long iNew = (long long)ptV->uiCursor + iDelta;
    if (iNew < 0) iNew = 0;
    if (ptV->uiSize > 0 && (size_t)iNew >= ptV->uiSize)
        iNew = (long long)(ptV->uiSize - 1);
    ptV->uiCursor = (size_t)iNew;
    ptV->iNibble  = 0;
}

/* ------------------------------------------------------------------
 * Key handler
 * ------------------------------------------------------------------ */

static void ehex_handle_key(edit_hex_view_t *ptV,
                             gui_key_t eKey, char cChar,
                             st_u8_t uiMods)
{
    int       bCtrl    = (uiMods & GUI_MOD_CTRL) != 0;
    int       iVisRows;
    int       iCount;
    int       iIdx;
    size_t    uiRowStart;
    size_t    uiRowEnd;
    int       iCurCol;

    iVisRows = (ptV->iCellH > 0)
               ? ehex_hex_area_h(ptV) / ptV->iCellH : 10;
    if (iVisRows < 1) iVisRows = 1;

    /* --- F2: toggle disasm panel (all zones) ---------------------- */
    if (eKey == GUI_KEY_F2)
    {
        ptV->bShowDisasm = (ptV->bShowDisasm == ST_TRUE)
                           ? ST_FALSE : ST_TRUE;
        if (ptV->eZone == HEX_ZONE_DISASM)
        {
            ptV->eZone   = HEX_ZONE_HEX;
            ptV->iNibble = 0;
        }
        ehex_recalc_layout(ptV);
        if (ptV->bShowDisasm)
            ehex_disasm_cache_update(ptV);
        gui_invalidate(ptV->hWnd);
        return;
    }

    /* --- CTRL shortcuts (all zones) ------------------------------- */
    if (bCtrl && eKey >= GUI_KEY_PRINTABLE)
    {
        int iLetter = (int)(eKey - GUI_KEY_PRINTABLE);
        if (iLetter == 'S' || iLetter == 's')
        {
            if (ptV->bDirty)
            {
                if (ehex_save(ptV) == ST_NO_ERROR)
                    ehex_update_title(ptV);
                else
                    LOG_ERROR("save failed for '%s'", ptV->szPath);
            }
            /* Save annotation when in note-edit zone */
            if (ptV->eZone == HEX_ZONE_BAND_NOTE
            &&  ptV->ptAnnot != NULL)
            {
                int        iSec  = (int)(ptV->uiCursor / SA_SECTOR_SIZE);
                const char *szTy = (ptV->iSecCount > 0
                                    && iSec < ptV->iSecCount)
                                   ? sector_type_name(
                                       ptV->aeSecType[iSec])
                                   : "";
                image_annot_set_sector(ptV->ptAnnot, iSec,
                                       szTy, ptV->szBandNote);
                if (image_annot_save(ptV->ptAnnot,
                                     ptV->szJsonPath) == ST_NO_ERROR)
                    LOG_INFO("annotation saved for sec %d", iSec);
                ptV->iBandLastSec = -1; /* force re-sync next render */
            }
            gui_invalidate(ptV->hWnd);
        }
        else if (iLetter == 'N' || iLetter == 'n')
        {
            /* CTRL+N: enter note-edit zone for current sector */
            ptV->eZone       = HEX_ZONE_BAND_NOTE;
            ptV->iBandLastSec = -1; /* force sync to load note */
            ehex_band_sync(ptV);
            /* After sync, switch zone back to editing */
            ptV->eZone       = HEX_ZONE_BAND_NOTE;
            ptV->iBandNotePos = (int)strlen(ptV->szBandNote);
            gui_invalidate(ptV->hWnd);
        }
        return;
    }

    /* --- BAND_NOTE zone: note text editing ----------------------- */
    if (ptV->eZone == HEX_ZONE_BAND_NOTE)
    {
        int iLen = (int)strlen(ptV->szBandNote);

        switch (eKey)
        {
        case GUI_KEY_LEFT:
            if (ptV->iBandNotePos > 0)
                ptV->iBandNotePos--;
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_RIGHT:
            if (ptV->iBandNotePos < iLen)
                ptV->iBandNotePos++;
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_HOME:
            ptV->iBandNotePos = 0;
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_END:
            ptV->iBandNotePos = iLen;
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_BACKSPACE:
            if (ptV->iBandNotePos > 0)
            {
                int iPos = ptV->iBandNotePos - 1;
                memmove(ptV->szBandNote + iPos,
                        ptV->szBandNote + iPos + 1,
                        (size_t)(iLen - iPos));
                ptV->iBandNotePos = iPos;
            }
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_DELETE:
            if (ptV->iBandNotePos < iLen)
            {
                int iPos = ptV->iBandNotePos;
                memmove(ptV->szBandNote + iPos,
                        ptV->szBandNote + iPos + 1,
                        (size_t)(iLen - iPos));
            }
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_ESCAPE:
        case GUI_KEY_TAB:
            ptV->eZone   = HEX_ZONE_HEX;
            ptV->iNibble = 0;
            gui_invalidate(ptV->hWnd);
            return;

        default:
            if (eKey >= GUI_KEY_PRINTABLE
            &&  iLen < ANNOT_NOTE_MAX - 1)
            {
                int iPos = ptV->iBandNotePos;
                memmove(ptV->szBandNote + iPos + 1,
                        ptV->szBandNote + iPos,
                        (size_t)(iLen - iPos + 1));
                ptV->szBandNote[iPos] = cChar;
                ptV->iBandNotePos++;
            }
            gui_invalidate(ptV->hWnd);
            return;
        }
    }

    /* --- DISASM zone navigation (read-only) ----------------------- */
    if (ptV->eZone == HEX_ZONE_DISASM)
    {
        iCount = ptV->iDisasmCacheCount;
        iIdx   = ptV->iDisasmCursorIdx;

        switch (eKey)
        {
        case GUI_KEY_UP:
            if (iIdx > 0)
            {
                ptV->iDisasmCursorIdx = iIdx - 1;
                ptV->uiCursor =
                    (size_t)ptV->atDisasmCache[iIdx - 1].uiAddr;
                ptV->iNibble = 0;
                ehex_scroll_to_cursor(ptV);
                ehex_disasm_scroll_to_cursor(ptV);
            }
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_DOWN:
            if (iIdx < iCount - 1)
            {
                ptV->iDisasmCursorIdx = iIdx + 1;
                ptV->uiCursor =
                    (size_t)ptV->atDisasmCache[iIdx + 1].uiAddr;
                ptV->iNibble = 0;
                ehex_scroll_to_cursor(ptV);
                ehex_disasm_scroll_to_cursor(ptV);
            }
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_PAGE_UP:
            {
                int iNew = iIdx - iVisRows;
                if (iNew < 0) iNew = 0;
                ptV->iDisasmCursorIdx = iNew;
                ptV->uiCursor =
                    (size_t)ptV->atDisasmCache[iNew].uiAddr;
                ptV->iNibble = 0;
                ehex_scroll_to_cursor(ptV);
                ehex_disasm_scroll_to_cursor(ptV);
                gui_invalidate(ptV->hWnd);
            }
            return;

        case GUI_KEY_PAGE_DOWN:
            {
                int iNew = iIdx + iVisRows;
                if (iNew >= iCount) iNew = iCount - 1;
                if (iNew < 0)       iNew = 0;
                ptV->iDisasmCursorIdx = iNew;
                ptV->uiCursor =
                    (size_t)ptV->atDisasmCache[iNew].uiAddr;
                ptV->iNibble = 0;
                ehex_scroll_to_cursor(ptV);
                ehex_disasm_scroll_to_cursor(ptV);
                gui_invalidate(ptV->hWnd);
            }
            return;

        case GUI_KEY_HOME:
            if (iCount > 0)
            {
                ptV->iDisasmCursorIdx = 0;
                ptV->uiCursor =
                    (size_t)ptV->atDisasmCache[0].uiAddr;
                ptV->iNibble = 0;
                /* Full rebuild to reach file start */
                ehex_disasm_cache_update(ptV);
                ehex_scroll_to_cursor(ptV);
                gui_invalidate(ptV->hWnd);
            }
            return;

        case GUI_KEY_END:
            if (ptV->uiSize > 0)
            {
                ptV->uiCursor = ptV->uiSize - 1;
                ptV->iNibble  = 0;
                ehex_disasm_cache_update(ptV);
                ehex_scroll_to_cursor(ptV);
                gui_invalidate(ptV->hWnd);
            }
            return;

        case GUI_KEY_TAB:
            ptV->eZone   = HEX_ZONE_HEX;
            ptV->iNibble = 0;
            gui_invalidate(ptV->hWnd);
            return;

        case GUI_KEY_ESCAPE:
            if (ptV->bDirty)
                LOG_INFO("hex edit: closed '%s' with unsaved changes",
                         ptV->szPath);
            gui_request_close(ptV->hWnd);
            return;

        default:
            return; /* disasm zone is read-only */
        }
    }

    /* --- HEX / ASCII zone navigation ------------------------------ */
    switch (eKey)
    {
    case GUI_KEY_LEFT:
        if (ptV->eZone == HEX_ZONE_HEX && ptV->iNibble == 1)
        {
            ptV->iNibble = 0;
        }
        else
        {
            if (ptV->uiCursor > 0) ptV->uiCursor--;
            ptV->iNibble = (ptV->eZone == HEX_ZONE_HEX) ? 1 : 0;
        }
        break;

    case GUI_KEY_RIGHT:
        if (ptV->eZone == HEX_ZONE_HEX && ptV->iNibble == 0)
        {
            ptV->iNibble = 1;
        }
        else
        {
            if (ptV->uiSize > 0
            &&  ptV->uiCursor < ptV->uiSize - 1)
                ptV->uiCursor++;
            ptV->iNibble = 0;
        }
        break;

    case GUI_KEY_UP:
        ehex_move(ptV, -EDIT_HEX_BYTES_PER_ROW);
        break;

    case GUI_KEY_DOWN:
        ehex_move(ptV, +EDIT_HEX_BYTES_PER_ROW);
        break;

    case GUI_KEY_HOME:
        if (bCtrl)
        {
            ptV->uiCursor = 0;
            ptV->iNibble  = 0;
        }
        else
        {
            uiRowStart = (ptV->uiCursor / EDIT_HEX_BYTES_PER_ROW)
                         * EDIT_HEX_BYTES_PER_ROW;
            ptV->uiCursor = uiRowStart;
            ptV->iNibble  = 0;
        }
        break;

    case GUI_KEY_END:
        if (bCtrl)
        {
            if (ptV->uiSize > 0)
                ptV->uiCursor = ptV->uiSize - 1;
            ptV->iNibble = (ptV->eZone == HEX_ZONE_HEX) ? 1 : 0;
        }
        else
        {
            uiRowStart = (ptV->uiCursor / EDIT_HEX_BYTES_PER_ROW)
                         * EDIT_HEX_BYTES_PER_ROW;
            uiRowEnd   = uiRowStart + EDIT_HEX_BYTES_PER_ROW - 1;
            if (uiRowEnd >= ptV->uiSize && ptV->uiSize > 0)
                uiRowEnd = ptV->uiSize - 1;
            ptV->uiCursor = uiRowEnd;
            ptV->iNibble  = (ptV->eZone == HEX_ZONE_HEX) ? 1 : 0;
        }
        break;

    case GUI_KEY_PAGE_UP:
        ehex_move(ptV, -(iVisRows * EDIT_HEX_BYTES_PER_ROW));
        break;

    case GUI_KEY_PAGE_DOWN:
        ehex_move(ptV, +(iVisRows * EDIT_HEX_BYTES_PER_ROW));
        break;

    case GUI_KEY_TAB:
        /* Cycle HEX → ASCII → DISASM (if visible) → HEX             */
        if (ptV->eZone == HEX_ZONE_HEX)
            ptV->eZone = HEX_ZONE_ASCII;
        else if (ptV->eZone == HEX_ZONE_ASCII && ptV->bShowDisasm)
            ptV->eZone = HEX_ZONE_DISASM;
        else
            ptV->eZone = HEX_ZONE_HEX;
        ptV->iNibble = 0;
        break;

    case GUI_KEY_ESCAPE:
        if (ptV->bDirty)
            LOG_INFO("hex edit: closed '%s' with unsaved changes",
                     ptV->szPath);
        gui_request_close(ptV->hWnd);
        return;

    default:
        if (eKey < GUI_KEY_PRINTABLE) return;  /* unhandled nav key */

        /* --- Editing ---------------------------------------------- */
        if (ptV->uiSize == 0) return;

        if (ptV->eZone == HEX_ZONE_HEX)
        {
            /* Accept 0-9 / A-F / a-f */
            int iNibVal = -1;
            if (cChar >= '0' && cChar <= '9')
                iNibVal = cChar - '0';
            else if (cChar >= 'A' && cChar <= 'F')
                iNibVal = 10 + cChar - 'A';
            else if (cChar >= 'a' && cChar <= 'f')
                iNibVal = 10 + cChar - 'a';
            if (iNibVal < 0) return;

            if (ptV->iNibble == 0)
                ptV->pData[ptV->uiCursor] =
                    (st_u8_t)((iNibVal << 4)
                              | (ptV->pData[ptV->uiCursor] & 0x0F));
            else
                ptV->pData[ptV->uiCursor] =
                    (st_u8_t)((ptV->pData[ptV->uiCursor] & 0xF0)
                              | iNibVal);

            ptV->bDirty = ST_TRUE;

            /* Advance: low nibble → next byte high nibble             */
            if (ptV->iNibble == 0)
            {
                ptV->iNibble = 1;
            }
            else
            {
                ptV->iNibble = 0;
                if (ptV->uiCursor < ptV->uiSize - 1)
                    ptV->uiCursor++;
            }
            /* Rebuild disasm after byte edit                          */
            if (ptV->bShowDisasm)
                ehex_disasm_cache_update(ptV);
        }
        else /* HEX_ZONE_ASCII */
        {
            /* Accept printable ASCII (0x20–0x7E) */
            if (cChar < 0x20 || cChar > 0x7E) return;

            iCurCol = (int)(ptV->uiCursor % EDIT_HEX_BYTES_PER_ROW);
            ptV->pData[ptV->uiCursor] = (st_u8_t)cChar;
            ptV->bDirty = ST_TRUE;

            if (ptV->uiCursor < ptV->uiSize - 1)
                ptV->uiCursor++;
            (void)iCurCol;
            /* Rebuild disasm after byte edit                          */
            if (ptV->bShowDisasm)
                ehex_disasm_cache_update(ptV);
        }
        break;
    }

    ehex_scroll_to_cursor(ptV);
    ehex_update_title(ptV);
    if (ptV->bShowDisasm)
        ehex_disasm_find_cursor(ptV);
    gui_invalidate(ptV->hWnd);
}

/* ------------------------------------------------------------------
 * Mouse click
 * ------------------------------------------------------------------ */

static void ehex_handle_click(edit_hex_view_t *ptV, int iX, int iY)
{
    int    iRow;
    int    iAbsRow;
    int    iCol;
    int    iRelX;
    size_t uiOffset;
    int    iPos;
    int    i;
    int    iBandH;
    int    iBandTop;
    int    iBandNoteY;

    if (ptV->iCellH <= 0 || ptV->iCellW <= 0) return;

    /* Click in the info band (UC24C) -------------------------------- */
    iBandH     = HEXED_BAND_ROWS * ptV->iCellH;
    iBandTop   = ptV->iWndHeight - iBandH;
    iBandNoteY = iBandTop + ptV->iCellH;
    if (ptV->iCellH > 0 && iY >= iBandTop)
    {
        if (iY >= iBandNoteY)
        {
            /* Click on note row: enter BAND_NOTE zone */
            ptV->iBandLastSec = -1; /* force sync */
            ptV->eZone        = HEX_ZONE_HEX; /* allow sync */
            ehex_band_sync(ptV);
            ptV->eZone        = HEX_ZONE_BAND_NOTE;
            ptV->iBandNotePos = (int)strlen(ptV->szBandNote);
            gui_invalidate(ptV->hWnd);
        }
        return; /* click in band → no hex cursor change */
    }

    iRow    = iY / ptV->iCellH;

    /* Click in disasm panel (UC10) */
    if (ptV->bShowDisasm && ptV->iDisasmX > 0 && iX >= ptV->iDisasmX)
    {
        int iAbsDisasm = ptV->iDisasmScrollRow + iRow;
        if (iAbsDisasm >= 0 && iAbsDisasm < ptV->iDisasmCacheCount)
        {
            ptV->iDisasmCursorIdx = iAbsDisasm;
            ptV->uiCursor =
                (size_t)ptV->atDisasmCache[iAbsDisasm].uiAddr;
            ptV->iNibble = 0;
            ptV->eZone   = HEX_ZONE_DISASM;
            ehex_scroll_to_cursor(ptV);
            gui_invalidate(ptV->hWnd);
        }
        return;
    }

    iAbsRow = ptV->iScrollRow + iRow;
    if (iAbsRow >= ehex_row_count(ptV)) return;

    if (iX >= ptV->iAsciiX)
    {
        /* Click in ASCII zone */
        iCol = (iX - ptV->iAsciiX) / ptV->iCellW;
        if (iCol < 0) iCol = 0;
        if (iCol >= EDIT_HEX_BYTES_PER_ROW)
            iCol = EDIT_HEX_BYTES_PER_ROW - 1;
        uiOffset = (size_t)iAbsRow * EDIT_HEX_BYTES_PER_ROW
                 + (size_t)iCol;
        if (uiOffset >= ptV->uiSize && ptV->uiSize > 0)
            uiOffset = ptV->uiSize - 1;
        ptV->uiCursor = uiOffset;
        ptV->iNibble  = 0;
        ptV->eZone    = HEX_ZONE_ASCII;
    }
    else if (iX >= ptV->iHexX)
    {
        /* Click in HEX zone: map char column → byte + nibble          */
        iRelX = (iX - ptV->iHexX) / ptV->iCellW;
        /* iRelX is a character offset within the hex string           */
        /* find which byte and nibble                                  */
        iCol = 0;
        for (i = 0; i < EDIT_HEX_BYTES_PER_ROW; i++)
        {
            iPos = i * 3 + (i >= 8 ? 1 : 0);
            if (iRelX >= iPos && iRelX < iPos + 2)
            {
                iCol = i;
                uiOffset = (size_t)iAbsRow * EDIT_HEX_BYTES_PER_ROW
                         + (size_t)iCol;
                if (uiOffset >= ptV->uiSize && ptV->uiSize > 0)
                    uiOffset = ptV->uiSize - 1;
                ptV->uiCursor = uiOffset;
                ptV->iNibble  = (iRelX == iPos) ? 0 : 1;
                ptV->eZone    = HEX_ZONE_HEX;
                if (ptV->bShowDisasm)
                    ehex_disasm_find_cursor(ptV);
                gui_invalidate(ptV->hWnd);
                return;
            }
        }
        /* Click on a space between bytes: nearest byte                 */
        iCol = iRelX / 3;
        if (iCol >= EDIT_HEX_BYTES_PER_ROW)
            iCol = EDIT_HEX_BYTES_PER_ROW - 1;
        uiOffset = (size_t)iAbsRow * EDIT_HEX_BYTES_PER_ROW
                 + (size_t)iCol;
        if (uiOffset >= ptV->uiSize && ptV->uiSize > 0)
            uiOffset = ptV->uiSize - 1;
        ptV->uiCursor = uiOffset;
        ptV->iNibble  = 0;
        ptV->eZone    = HEX_ZONE_HEX;
    }

    if (ptV->bShowDisasm)
        ehex_disasm_find_cursor(ptV);
    gui_invalidate(ptV->hWnd);
}

/* ------------------------------------------------------------------
 * GUI event callback
 * ------------------------------------------------------------------ */

static void ehex_event_callback(gui_window_t  hWnd,
                                 gui_event_t  *ptEvent,
                                 void         *pCtx)
{
    edit_hex_view_t *ptV      = (edit_hex_view_t *)pCtx;
    int              iVisRows;
    int              iRowCount;

    if (ptV == NULL || ptEvent == NULL) return;

    switch (ptEvent->eType)
    {
    case GUI_EVT_PAINT:
        if (ptV->hRenderer == NULL)
        {
            renderer_create(hWnd, &ptV->hRenderer);
            if (ptV->hRenderer != NULL)
            {
                renderer_get_font_metrics(ptV->hRenderer,
                                          RENDERER_FONT_MONO,
                                          &ptV->iCellW,
                                          &ptV->iCellH);
                ehex_recalc_layout(ptV);
                /* Build initial disasm cache once cells are known     */
                if (ptV->bShowDisasm)
                    ehex_disasm_cache_update(ptV);
            }
        }
        if (ptV->hRenderer != NULL)
        {
            ehex_update_title(ptV);
            ehex_render(ptV);
        }
        break;

    case GUI_EVT_RESIZE:
        ptV->iWndWidth  = ptEvent->uData.tResize.iWidth;
        ptV->iWndHeight = ptEvent->uData.tResize.iHeight;
        if (ptV->hRenderer != NULL)
            renderer_resize(ptV->hRenderer,
                            ptV->iWndWidth, ptV->iWndHeight);
        gui_invalidate(hWnd);
        break;

    case GUI_EVT_KEY_DOWN:
        ehex_handle_key(ptV,
                         ptEvent->uData.tKey.eKey,
                         ptEvent->uData.tKey.cChar,
                         ptEvent->uData.tKey.uiMods);
        break;

    case GUI_EVT_MOUSE_DOWN:
        if (ptEvent->uData.tMouse.eBtn == GUI_MOUSE_LEFT
        &&  ptV->iCellH > 0)
        {
            ehex_handle_click(ptV,
                               ptEvent->uData.tMouse.iX,
                               ptEvent->uData.tMouse.iY);
        }
        break;

    case GUI_EVT_SCROLL:
        if (ptV->eZone == HEX_ZONE_DISASM && ptV->bShowDisasm)
        {
            /* Scroll disasm panel when cursor is in disasm zone       */
            ptV->iDisasmScrollRow -= ptEvent->uData.tScroll.iDelta;
            if (ptV->iDisasmScrollRow < 0)
                ptV->iDisasmScrollRow = 0;
            if (ptV->iDisasmCacheCount > 0
            &&  ptV->iDisasmScrollRow >= ptV->iDisasmCacheCount)
                ptV->iDisasmScrollRow = ptV->iDisasmCacheCount - 1;
        }
        else
        {
            /* Scroll hex panel */
            iRowCount = ehex_row_count(ptV);
            iVisRows  = (ptV->iCellH > 0)
                        ? ptV->iWndHeight / ptV->iCellH : 1;
            ptV->iScrollRow -= ptEvent->uData.tScroll.iDelta;
            if (ptV->iScrollRow < 0) ptV->iScrollRow = 0;
            if (ptV->iScrollRow > iRowCount - 1)
                ptV->iScrollRow = iRowCount - 1;
            (void)iVisRows;
        }
        gui_invalidate(hWnd);
        break;

    case GUI_EVT_CLOSE:
        if (ptV->hRenderer != NULL)
        {
            renderer_destroy(&ptV->hRenderer);
            ptV->hRenderer = NULL;
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_error_t edit_hex_open(const char       *szPath,
                          line_context_t   *ptLineCtx,
                          edit_hex_view_t **pptView)
{
    edit_hex_view_t *ptV = NULL;
    gui_wnd_desc_t   tDesc;
    st_error_t       eRet = ST_ERROR;

    LOG_TRACE("szPath=%s ptLineCtx=%p pptView=%p",
              szPath ? szPath : "(null)",
              (void *)ptLineCtx, (void *)pptView);

    if (szPath == NULL || ptLineCtx == NULL || pptView == NULL)
    {
        LOG_ERROR("NULL parameter: szPath=%p ptLineCtx=%p pptView=%p",
                  (void *)szPath, (void *)ptLineCtx,
                  (void *)pptView);
        return ST_ERROR;
    }

    *pptView = NULL;

    ptV = (edit_hex_view_t *)malloc(sizeof(*ptV));
    if (ptV == NULL)
    {
        LOG_ERROR("malloc failed for edit_hex_view_t");
        return ST_ERROR;
    }
    memset(ptV, 0, sizeof(*ptV));

    strncpy(ptV->szPath, szPath, ST_MAX_PATH - 1);
    ptV->szPath[ST_MAX_PATH - 1] = '\0';
    ptV->ptLineCtx    = ptLineCtx;
    ptV->eZone        = HEX_ZONE_HEX;

    /* Initialise BPB cache and band state (UC24C) */
    ptV->iBpbFat1Lba  = -1;
    ptV->iBpbFat2Lba  = -1;
    ptV->iBpbRootLba  = -1;
    ptV->iBpbDataLba  = -1;
    ptV->iBandLastSec = -1;

    if (ehex_load(ptV, szPath) != ST_NO_ERROR)
    {
        LOG_ERROR("failed to load '%s'", szPath);
        free(ptV);
        return ST_ERROR;
    }

    /* Classify sectors for tinting (UC24B) and BPB cache (UC24C) */
    ehex_classify_sectors(ptV);

    /* Load JSON annotation (UC24C); absent file is non-fatal */
    image_annot_json_path(szPath, ptV->szJsonPath,
                          sizeof(ptV->szJsonPath));
    if (image_annot_create(&ptV->ptAnnot) == ST_NO_ERROR)
    {
        const char *pBase = szPath;
        const char *p;
        for (p = szPath; *p; p++)
            if (*p == '/' || *p == '\\') pBase = p + 1;
        strncpy(ptV->ptAnnot->szFilename, pBase, ST_MAX_PATH - 1);
        image_annot_load(ptV->szJsonPath, ptV->ptAnnot); /* non-fatal */
    }

    /* Allocate disasm cache (UC10) */
    ptV->atDisasmCache = (disasm_result_t *)malloc(
        sizeof(disasm_result_t) * EDIT_HEX_DISASM_CACHE_LINES);
    if (ptV->atDisasmCache == NULL)
    {
        LOG_ERROR("malloc failed for disasm cache (%zu bytes), "
                  "panel disabled",
                  sizeof(disasm_result_t) * EDIT_HEX_DISASM_CACHE_LINES);
        ptV->bShowDisasm = ST_FALSE;
    }
    else
    {
        ptV->bShowDisasm = ST_TRUE;
    }

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle  = "ST4Ever - Hex";
    tDesc.eType    = GUI_WND_EDIT_HEX;
    tDesc.pfnEvent = ehex_event_callback;
    tDesc.pUserCtx = ptV;
    tDesc.iWidth   = ptV->bShowDisasm
                     ? EDIT_HEX_WND_WIDTH_DISASM
                     : EDIT_HEX_WND_WIDTH_STD;
    tDesc.iHeight  = EDIT_HEX_WND_HEIGHT;

    if (gui_open_window(&tDesc, &ptV->hWnd) != ST_NO_ERROR)
    {
        LOG_ERROR("gui_open_window failed for '%s'", szPath);
        free(ptV->atDisasmCache);
        free(ptV->aeSecType);
        free(ptV->pData);
        free(ptV);
        return ST_ERROR;
    }

    *pptView = ptV;
    eRet = ST_NO_ERROR;
    LOG_INFO("edit_hex opened '%s' (%zu bytes, disasm=%s)",
             szPath, ptV->uiSize,
             ptV->bShowDisasm ? "on" : "off");
    return eRet;
}

st_error_t edit_hex_close(edit_hex_view_t **pptView)
{
    edit_hex_view_t *ptV;

    LOG_TRACE("pptView=%p", (void *)pptView);

    if (pptView == NULL)
    {
        LOG_ERROR("NULL parameter: pptView=%p", (void *)pptView);
        return ST_ERROR;
    }
    if (*pptView == NULL) return ST_NO_ERROR;

    ptV = *pptView;
    gui_close_window(ptV->hWnd);
    free(ptV->atDisasmCache);
    ptV->atDisasmCache = NULL;
    free(ptV->aeSecType);
    ptV->aeSecType = NULL;
    image_annot_destroy(&ptV->ptAnnot);
    free(ptV->pData);
    ptV->pData = NULL;
    free(ptV);
    *pptView = NULL;

    LOG_INFO("edit_hex view closed");
    return ST_NO_ERROR;
}
