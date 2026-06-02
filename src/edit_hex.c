/*
 * edit_hex.c - Hex + ASCII editor view
 *
 * UC9: full implementation.
 *   Data model : flat heap byte array (replace-mode, fixed size).
 *   Rendering  : Direct2D; per-row: address | hex pairs | ASCII.
 *   Input      : nibble edit (hex zone), byte edit (ASCII zone),
 *                full navigation, TAB to switch zone, CTRL+S save.
 */

#include "edit_hex.h"
#include "file.h"
#include "trace.h"
#include "gui.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------
 * Colours
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
 * Hex nibble lookup
 * ------------------------------------------------------------------ */

static const char g_szNib[] = "0123456789ABCDEF";

/* ------------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------------ */

static st_error_t ehex_load(edit_hex_view_t *ptV, const char *szPath);
static st_error_t ehex_save(edit_hex_view_t *ptV);
static void       ehex_update_title(edit_hex_view_t *ptV);
static void       ehex_recalc_layout(edit_hex_view_t *ptV);
static int        ehex_row_count(const edit_hex_view_t *ptV);
static void       ehex_scroll_to_cursor(edit_hex_view_t *ptV);
static void       ehex_build_hex_row(const edit_hex_view_t *ptV,
                                      int iRow, char *szOut);
static void       ehex_build_asc_row(const edit_hex_view_t *ptV,
                                      int iRow, char *szOut);
static void       ehex_render(edit_hex_view_t *ptV);
static void       ehex_handle_key(edit_hex_view_t *ptV,
                                   gui_key_t eKey, char cChar,
                                   st_u8_t uiMods);
static void       ehex_handle_click(edit_hex_view_t *ptV,
                                     int iX, int iY);
static void       ehex_event_callback(gui_window_t hWnd,
                                       gui_event_t *ptEvent,
                                       void        *pCtx);

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
    /* iAddrX = 0 (address column starts at left edge)                 */
    ptV->iAddrX   = 0;
    /* iHexX = after address column: (ADDR_CHARS + ADDR_PAD) cells     */
    ptV->iHexX    = (EDIT_HEX_ADDR_CHARS + EDIT_HEX_ADDR_PAD)
                    * ptV->iCellW;
    /* iAsciiX = after hex area + separator                            */
    ptV->iAsciiX  = ptV->iHexX
                  + (EDIT_HEX_HEX_CHARS + EDIT_HEX_SEP_CHARS)
                  * ptV->iCellW;
}

static int ehex_row_count(const edit_hex_view_t *ptV)
{
    if (ptV->uiSize == 0) return 1;
    return (int)((ptV->uiSize + EDIT_HEX_BYTES_PER_ROW - 1)
                 / EDIT_HEX_BYTES_PER_ROW);
}

static void ehex_scroll_to_cursor(edit_hex_view_t *ptV)
{
    int iCurRow;
    int iVisRows;

    iCurRow  = (int)(ptV->uiCursor / EDIT_HEX_BYTES_PER_ROW);
    iVisRows = (ptV->iCellH > 0) ? ptV->iWndHeight / ptV->iCellH : 1;
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
            unsigned b     = ptV->pData[uiBase + (size_t)i];
            szOut[iPos]    = g_szNib[b >> 4];
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

    iVisRows = (ptV->iCellH > 0) ? ptV->iWndHeight / ptV->iCellH : 1;
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

    /* Separator background */
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
    size_t    uiRowStart;
    size_t    uiRowEnd;
    int       iCurCol;

    iVisRows = (ptV->iCellH > 0) ? ptV->iWndHeight / ptV->iCellH : 10;
    if (iVisRows < 1) iVisRows = 1;

    /* --- CTRL shortcuts ------------------------------------------- */
    if (bCtrl && eKey >= GUI_KEY_PRINTABLE)
    {
        int iLetter = (int)(eKey - GUI_KEY_PRINTABLE);
        if (iLetter == 'S')
        {
            if (ptV->bDirty)
            {
                if (ehex_save(ptV) == ST_NO_ERROR)
                    ehex_update_title(ptV);
                else
                    LOG_ERROR("save failed for '%s'", ptV->szPath);
            }
        }
        return;
    }

    /* --- Navigation ----------------------------------------------- */
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
        ptV->eZone   = (ptV->eZone == HEX_ZONE_HEX)
                       ? HEX_ZONE_ASCII : HEX_ZONE_HEX;
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
        }
        break;
    }

    ehex_scroll_to_cursor(ptV);
    ehex_update_title(ptV);
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

    if (ptV->iCellH <= 0 || ptV->iCellW <= 0) return;

    iRow    = iY / ptV->iCellH;
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

    gui_invalidate(ptV->hWnd);
}

/* ------------------------------------------------------------------
 * GUI event callback
 * ------------------------------------------------------------------ */

static void ehex_event_callback(gui_window_t  hWnd,
                                 gui_event_t  *ptEvent,
                                 void         *pCtx)
{
    edit_hex_view_t *ptV    = (edit_hex_view_t *)pCtx;
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
        iRowCount = ehex_row_count(ptV);
        iVisRows  = (ptV->iCellH > 0)
                    ? ptV->iWndHeight / ptV->iCellH : 1;
        ptV->iScrollRow -= ptEvent->uData.tScroll.iDelta;
        if (ptV->iScrollRow < 0) ptV->iScrollRow = 0;
        if (ptV->iScrollRow > iRowCount - 1)
            ptV->iScrollRow = iRowCount - 1;
        (void)iVisRows;
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
    ptV->ptLineCtx = ptLineCtx;
    ptV->eZone     = HEX_ZONE_HEX;

    if (ehex_load(ptV, szPath) != ST_NO_ERROR)
    {
        LOG_ERROR("failed to load '%s'", szPath);
        free(ptV);
        return ST_ERROR;
    }

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle  = "ST4Ever - Hex";
    tDesc.eType    = GUI_WND_EDIT_HEX;
    tDesc.pfnEvent = ehex_event_callback;
    tDesc.pUserCtx = ptV;

    if (gui_open_window(&tDesc, &ptV->hWnd) != ST_NO_ERROR)
    {
        LOG_ERROR("gui_open_window failed for '%s'", szPath);
        free(ptV->pData);
        free(ptV);
        return ST_ERROR;
    }

    *pptView = ptV;
    eRet = ST_NO_ERROR;
    LOG_INFO("edit_hex opened '%s' (%zu bytes)", szPath, ptV->uiSize);
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
    free(ptV->pData);
    ptV->pData = NULL;
    free(ptV);
    *pptView = NULL;

    LOG_INFO("edit_hex view closed");
    return ST_NO_ERROR;
}
