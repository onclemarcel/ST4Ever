/*
 * edit_txt.c - Text / source editor view
 *
 * UC8: full implementation.
 *   Line buffer  : heap array of heap strings; realloc on insert.
 *   Rendering    : Direct2D via renderer.h; monospace font;
 *                  gutter + text area; selection highlight; cursor bar.
 *   Input        : navigation, editing, selection, CTRL shortcuts,
 *                  clipboard via gui_clipboard_*().
 *   Save         : CTRL+S writes via file.h; title updated ([*] dirty).
 */

#include "edit_txt.h"
#include "file.h"
#include "trace.h"
#include "gui.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

/* ------------------------------------------------------------------
 * Internal constants
 * ------------------------------------------------------------------ */

#define LINE_ALLOC_INIT  256   /* initial aszLines capacity              */
#define LINE_ALLOC_STEP  256   /* growth step when full                  */
#define GUTTER_PAD       1     /* extra cells of right-padding in gutter */
#define CURSOR_BAR_W     2     /* cursor vertical bar width (px)         */

/* ------------------------------------------------------------------
 * Colours
 * ------------------------------------------------------------------ */

static const renderer_color_t g_clrBg       = RENDERER_COLOR_BLACK;
static const renderer_color_t g_clrCurLine  = { 0.10f, 0.10f, 0.16f, 1.0f };
static const renderer_color_t g_clrGutter   = { 0.08f, 0.08f, 0.12f, 1.0f };
static const renderer_color_t g_clrLineNo   = { 0.45f, 0.45f, 0.55f, 1.0f };
static const renderer_color_t g_clrText     = RENDERER_COLOR_LTGRAY;
static const renderer_color_t g_clrSel      = { 0.20f, 0.35f, 0.70f, 1.0f };
static const renderer_color_t g_clrCursor   = { 0.75f, 0.85f, 1.00f, 1.0f };

/* ------------------------------------------------------------------
 * Module-level state
 * ------------------------------------------------------------------ */

static st_bool_t g_etxt_bBackupEnabled = ST_TRUE;

/* ------------------------------------------------------------------
 * Forward declarations of all static helpers
 * ------------------------------------------------------------------ */

static void     etxt_free_lines(edit_txt_view_t *ptV);
static void     etxt_create_backup(edit_txt_view_t *ptV,
                                    const char      *szSrcPath);
static void     etxt_finalize_backup(edit_txt_view_t *ptV);
static void     etxt_undo_push(edit_txt_view_t *ptV);
static void     etxt_undo_pop(edit_txt_view_t *ptV);
static void     etxt_undo_free_all(edit_txt_view_t *ptV);
static st_error_t etxt_alloc_line_slot(edit_txt_view_t *ptV);
static st_error_t etxt_load(edit_txt_view_t *ptV, const char *szPath);
static st_error_t etxt_save(edit_txt_view_t *ptV);
static void     etxt_update_title(edit_txt_view_t *ptV);
static int      etxt_digits(int iN);
static void     etxt_recalc_gutter(edit_txt_view_t *ptV);
static int      etxt_display_len(const char *szLine, int iByteCol);
static int      etxt_byte_col_from_disp(const char *szLine, int iDisp);
static void     etxt_scroll_to_cursor(edit_txt_view_t *ptV);
static void     etxt_render(edit_txt_view_t *ptV);
static void     etxt_sel_order(const edit_txt_view_t *ptV,
                                edit_pos_t *ptStart,
                                edit_pos_t *ptEnd);
static st_bool_t etxt_has_sel(const edit_txt_view_t *ptV);
static void     etxt_sel_clear(edit_txt_view_t *ptV);
static void     etxt_sel_set_anchor(edit_txt_view_t *ptV);
static st_error_t etxt_del_sel(edit_txt_view_t *ptV);
static st_error_t etxt_copy_sel(edit_txt_view_t *ptV);
static st_error_t etxt_paste(edit_txt_view_t *ptV);
static st_error_t etxt_insert_char(edit_txt_view_t *ptV, char c);
static st_error_t etxt_insert_newline(edit_txt_view_t *ptV);
static st_error_t etxt_del_back(edit_txt_view_t *ptV);
static st_error_t etxt_del_fwd(edit_txt_view_t *ptV);
static void     etxt_move_word_left(edit_txt_view_t *ptV);
static void     etxt_move_word_right(edit_txt_view_t *ptV);
static void     etxt_clamp_col(edit_txt_view_t *ptV);
static void     etxt_handle_key(edit_txt_view_t *ptV,
                                 gui_key_t        eKey,
                                 char             cChar,
                                 st_u8_t          uiMods);
static void     etxt_event_callback(gui_window_t  hWnd,
                                     gui_event_t  *ptEvent,
                                     void         *pCtx);

/* ------------------------------------------------------------------
 * Line buffer helpers
 * ------------------------------------------------------------------ */

static void etxt_free_lines(edit_txt_view_t *ptV)
{
    int i;

    if (ptV->aszLines == NULL) return;
    for (i = 0; i < ptV->iLineCount; i++)
        free(ptV->aszLines[i]);
    free(ptV->aszLines);
    ptV->aszLines    = NULL;
    ptV->iLineCount  = 0;
    ptV->iLineAlloc  = 0;
}

/* ------------------------------------------------------------------
 * Undo ring buffer
 * ------------------------------------------------------------------ */

/* Deep-copy the current buffer into the next undo slot.
 * Consecutive printable-char inserts are grouped: push is skipped
 * when bUndoGroupInsert is already TRUE (caller sets it). */
static void etxt_undo_push(edit_txt_view_t *ptV)
{
    int    iSlot;
    int    i;
    char **aszCopy;

    iSlot = ptV->iUndoHead;

    /* Overwrite oldest entry when ring is full */
    if (ptV->iUndoCount == EDIT_TXT_UNDO_LEVELS)
    {
        if (ptV->aUndo[iSlot].aszLines != NULL)
        {
            for (i = 0; i < ptV->aUndo[iSlot].iLineCount; i++)
                free(ptV->aUndo[iSlot].aszLines[i]);
            free(ptV->aUndo[iSlot].aszLines);
            ptV->aUndo[iSlot].aszLines = NULL;
        }
    }
    else
    {
        ptV->iUndoCount++;
    }

    aszCopy = (char **)malloc((size_t)ptV->iLineCount * sizeof(char *));
    if (aszCopy == NULL)
    {
        LOG_ERROR("malloc failed for undo snapshot");
        ptV->iUndoCount--;
        return;
    }

    for (i = 0; i < ptV->iLineCount; i++)
    {
        size_t uiLen  = strlen(ptV->aszLines[i]);
        aszCopy[i]    = (char *)malloc(uiLen + 1);
        if (aszCopy[i] == NULL)
        {
            int j;
            LOG_ERROR("malloc failed for undo line %d", i);
            for (j = 0; j < i; j++) free(aszCopy[j]);
            free(aszCopy);
            ptV->iUndoCount--;
            return;
        }
        memcpy(aszCopy[i], ptV->aszLines[i], uiLen + 1);
    }

    ptV->aUndo[iSlot].aszLines   = aszCopy;
    ptV->aUndo[iSlot].iLineCount = ptV->iLineCount;
    ptV->aUndo[iSlot].tCursor    = ptV->tCursor;
    ptV->iUndoHead = (iSlot + 1) % EDIT_TXT_UNDO_LEVELS;
}

/* Restore the most recent undo snapshot and free current buffer. */
static void etxt_undo_pop(edit_txt_view_t *ptV)
{
    int iSlot;
    int i;

    if (ptV->iUndoCount == 0) return;

    iSlot = (ptV->iUndoHead - 1 + EDIT_TXT_UNDO_LEVELS)
            % EDIT_TXT_UNDO_LEVELS;

    /* Free the live buffer */
    for (i = 0; i < ptV->iLineCount; i++)
        free(ptV->aszLines[i]);
    free(ptV->aszLines);

    /* Transfer snapshot ownership to live buffer */
    ptV->aszLines   = ptV->aUndo[iSlot].aszLines;
    ptV->iLineCount = ptV->aUndo[iSlot].iLineCount;
    ptV->iLineAlloc = ptV->aUndo[iSlot].iLineCount;
    ptV->tCursor    = ptV->aUndo[iSlot].tCursor;

    ptV->aUndo[iSlot].aszLines   = NULL;
    ptV->aUndo[iSlot].iLineCount = 0;
    ptV->iUndoHead  = iSlot;
    ptV->iUndoCount--;

    ptV->iSelAnchorLine   = -1;
    ptV->bUndoGroupInsert = ST_FALSE;
    ptV->bDirty           = ST_TRUE;
}

/* Release all entries in the undo ring (called on close). */
static void etxt_undo_free_all(edit_txt_view_t *ptV)
{
    int i;
    int j;

    for (i = 0; i < EDIT_TXT_UNDO_LEVELS; i++)
    {
        if (ptV->aUndo[i].aszLines != NULL)
        {
            for (j = 0; j < ptV->aUndo[i].iLineCount; j++)
                free(ptV->aUndo[i].aszLines[j]);
            free(ptV->aUndo[i].aszLines);
            ptV->aUndo[i].aszLines = NULL;
        }
    }
    ptV->iUndoHead  = 0;
    ptV->iUndoCount = 0;
}

/* Grow aszLines array by LINE_ALLOC_STEP if full. */
static st_error_t etxt_alloc_line_slot(edit_txt_view_t *ptV)
{
    char **aszNew;
    int    iNewAlloc;

    if (ptV->iLineCount < ptV->iLineAlloc) return ST_NO_ERROR;

    iNewAlloc = ptV->iLineAlloc + LINE_ALLOC_STEP;
    aszNew    = (char **)realloc(ptV->aszLines,
                                  (size_t)iNewAlloc * sizeof(char *));
    if (aszNew == NULL)
    {
        LOG_ERROR("realloc failed for aszLines (%d slots)", iNewAlloc);
        return ST_ERROR;
    }
    ptV->aszLines   = aszNew;
    ptV->iLineAlloc = iNewAlloc;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * File load / save
 * ------------------------------------------------------------------ */

static st_error_t etxt_load(edit_txt_view_t *ptV, const char *szPath)
{
    file_t    *ptFile   = NULL;
    char      *szBuf    = NULL;
    char      *szEmpty  = NULL;
    size_t     uiBufSz  = 0;
    size_t     uiRead   = 0;
    char      *p;
    char      *pLine;
    char      *szDup;
    file_stat_t tStat;
    st_error_t  eRet    = ST_ERROR;

    /* Determine file size for single-allocation read */
    memset(&tStat, 0, sizeof(tStat));
    if (file_stat(szPath, &tStat) != ST_NO_ERROR)
    {
        LOG_ERROR("file_stat failed for '%s'", szPath);
        goto done;
    }
    uiBufSz = (size_t)tStat.uiSize;

    /* Create timestamped backup before touching the file */
    etxt_create_backup(ptV, szPath);

    /* Allocate initial line array */
    ptV->aszLines   = (char **)malloc(LINE_ALLOC_INIT * sizeof(char *));
    ptV->iLineAlloc = LINE_ALLOC_INIT;
    ptV->iLineCount = 0;
    if (ptV->aszLines == NULL)
    {
        LOG_ERROR("malloc failed for aszLines");
        goto done;
    }

    if (uiBufSz == 0)
    {
        /* Empty file: one empty line */
        szEmpty = (char *)malloc(1);
        if (szEmpty == NULL) { LOG_ERROR("malloc failed"); goto done; }
        szEmpty[0] = '\0';
        ptV->aszLines[0] = szEmpty;
        ptV->iLineCount  = 1;
        eRet = ST_NO_ERROR;
        goto done;
    }

    szBuf = (char *)malloc(uiBufSz + 1);
    if (szBuf == NULL)
    {
        LOG_ERROR("malloc failed for file buffer (%zu bytes)", uiBufSz);
        goto done;
    }

    if (file_open(szPath, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
    {
        LOG_ERROR("file_open failed for '%s'", szPath);
        goto done;
    }
    if (file_read(ptFile, szBuf, uiBufSz, &uiRead)
        != ST_NO_ERROR)
    {
        LOG_ERROR("file_read failed for '%s'", szPath);
        goto done;
    }
    szBuf[uiRead] = '\0';
    file_close(&ptFile);

    /* Split by newlines into aszLines.
     *
     * BUG FIX: for LF-only files (no \r before \n), the else branch
     * sets *p = '\0' to null-terminate the line for strlen().  Then
     * the original "if (*p == '\0') break" would fire immediately,
     * leaving only the first line.  We save the original character
     * (bEndOfBuf) before any modification to distinguish a real end-
     * of-buffer '\0' from a '\n' that was replaced in-place.
     */
    pLine = szBuf;
    for (p = szBuf; ; p++)
    {
        if (*p == '\n' || *p == '\0')
        {
            int       iLen;
            st_bool_t bEndOfBuf = (*p == '\0');

            /* Strip trailing \r, or null-terminate at \n */
            if (p > pLine && *(p - 1) == '\r')
                *(p - 1) = '\0';
            else
                *p = '\0';

            iLen = (int)strlen(pLine);
            if (iLen > EDIT_TXT_MAX_LINE_LEN - 1)
            {
                iLen = EDIT_TXT_MAX_LINE_LEN - 1;
                pLine[iLen] = '\0';
            }

            if (etxt_alloc_line_slot(ptV) != ST_NO_ERROR) goto done;

            szDup = (char *)malloc((size_t)iLen + 1);
            if (szDup == NULL) { LOG_ERROR("malloc failed"); goto done; }
            memcpy(szDup, pLine, (size_t)iLen + 1);
            ptV->aszLines[ptV->iLineCount++] = szDup;

            if (bEndOfBuf) break;
            pLine = p + 1;
        }
    }

    /* Guarantee at least one line */
    if (ptV->iLineCount == 0)
    {
        szEmpty = (char *)malloc(1);
        if (szEmpty == NULL) { LOG_ERROR("malloc failed"); goto done; }
        szEmpty[0] = '\0';
        ptV->aszLines[0] = szEmpty;
        ptV->iLineCount  = 1;
    }

    eRet = ST_NO_ERROR;

done:
    if (ptFile != NULL) file_close(&ptFile);
    free(szBuf);
    if (eRet != ST_NO_ERROR)
    {
        etxt_free_lines(ptV);
    }
    return eRet;
}

static st_error_t etxt_save(edit_txt_view_t *ptV)
{
    file_t    *ptFile = NULL;
    int        i;
    size_t     uiLen;
    st_error_t eRet  = ST_ERROR;

    if (file_open(ptV->szPath, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
    {
        LOG_ERROR("file_open(WRITE) failed for '%s'", ptV->szPath);
        return ST_ERROR;
    }

    for (i = 0; i < ptV->iLineCount; i++)
    {
        uiLen = strlen(ptV->aszLines[i]);
        if (uiLen > 0)
        {
            if (file_write(ptFile, ptV->aszLines[i], uiLen)
                != ST_NO_ERROR)
            {
                LOG_ERROR("file_write failed at line %d", i);
                goto done;
            }
        }
        /* Write newline after every line except the last */
        if (i < ptV->iLineCount - 1)
        {
            if (file_write(ptFile, "\n", 1) != ST_NO_ERROR)
            {
                LOG_ERROR("file_write newline failed at line %d", i);
                goto done;
            }
        }
    }

    ptV->bDirty = ST_FALSE;
    eRet = ST_NO_ERROR;
    LOG_INFO("saved '%s' (%d lines)", ptV->szPath, ptV->iLineCount);

done:
    file_close(&ptFile);
    return eRet;
}

/* ------------------------------------------------------------------
 * Backup helpers
 * ------------------------------------------------------------------ */

/* Create a timestamped copy of szSrcPath before editing begins.
 * Stores the backup path in ptV->szBackupPath, or "" on failure/skip.
 * Uses raw fopen/fread to avoid dependency on file_t lifecycle. */
static void etxt_create_backup(edit_txt_view_t *ptV,
                                const char      *szSrcPath)
{
    time_t      tNow;
    struct tm  *ptTm;
    char        szTimestamp[32];
    FILE       *pSrc;
    FILE       *pDst;
    char        aBuf[4096];
    size_t      uiGot;

    ptV->szBackupPath[0] = '\0';
    if (!g_etxt_bBackupEnabled)   return;
    if (szSrcPath == NULL)        return;

    tNow = time(NULL);
    ptTm = localtime(&tNow);
    if (ptTm == NULL)
    {
        fprintf(stderr, "edit backup: localtime failed\n");
        return;
    }
    strftime(szTimestamp, sizeof(szTimestamp), "%Y%m%d_%H%M%S", ptTm);

    /* "<original_path>_YYYYMMDD_HHMMSS.bak" */
    snprintf(ptV->szBackupPath, sizeof(ptV->szBackupPath),
             "%s_%s.bak", szSrcPath, szTimestamp);

    pSrc = fopen(szSrcPath, "rb");
    if (pSrc == NULL)
    {
        /* New (not yet saved) file — no backup needed */
        ptV->szBackupPath[0] = '\0';
        return;
    }

    pDst = fopen(ptV->szBackupPath, "wb");
    if (pDst == NULL)
    {
        LOG_ERROR("backup: cannot create '%s': %s",
                  ptV->szBackupPath, strerror(errno));
        fclose(pSrc);
        ptV->szBackupPath[0] = '\0';
        return;
    }

    while ((uiGot = fread(aBuf, 1, sizeof(aBuf), pSrc)) > 0)
        fwrite(aBuf, 1, uiGot, pDst);

    fclose(pSrc);
    fclose(pDst);
    LOG_INFO("backup created: '%s'", ptV->szBackupPath);
}

/* Compare current on-disk file with backup and delete backup if
 * they are byte-for-byte identical (meaning nothing was persisted). */
static void etxt_finalize_backup(edit_txt_view_t *ptV)
{
    FILE   *pFile;
    FILE   *pBak;
    char    aBufFile[4096];
    char    aBufBak[4096];
    size_t  uiGotF;
    size_t  uiGotB;
    int     bSame;

    if (ptV->szBackupPath[0] == '\0') return;

    pFile = fopen(ptV->szPath,       "rb");
    pBak  = fopen(ptV->szBackupPath, "rb");

    if (pFile == NULL || pBak == NULL)
    {
        /* Cannot compare — keep backup for safety */
        if (pFile) fclose(pFile);
        if (pBak)  fclose(pBak);
        return;
    }

    bSame = 1;
    for (;;)
    {
        uiGotF = fread(aBufFile, 1, sizeof(aBufFile), pFile);
        uiGotB = fread(aBufBak,  1, sizeof(aBufBak),  pBak);
        if (uiGotF != uiGotB
         || (uiGotF > 0 && memcmp(aBufFile, aBufBak, uiGotF) != 0))
        {
            bSame = 0;
            break;
        }
        if (uiGotF == 0) break;   /* simultaneous EOF */
    }
    fclose(pFile);
    fclose(pBak);

    if (bSame)
    {
        remove(ptV->szBackupPath);
        LOG_INFO("backup removed (file unchanged): '%s'",
                 ptV->szBackupPath);
    }
    else
    {
        LOG_INFO("backup kept (file was modified): '%s'",
                 ptV->szBackupPath);
    }
    ptV->szBackupPath[0] = '\0';
}

/* ------------------------------------------------------------------
 * Title and layout
 * ------------------------------------------------------------------ */

static void etxt_update_title(edit_txt_view_t *ptV)
{
    char szTitle[ST_MAX_PATH + 64];
    const char *szBase;
    const char *p;

    /* Basename extraction */
    szBase = ptV->szPath;
    for (p = ptV->szPath; *p; p++)
    {
        if (*p == '/' || *p == '\\')
            szBase = p + 1;
    }

    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Edit: %s%s",
             szBase,
             ptV->bDirty ? " [*]" : "");
    gui_set_title(ptV->hWnd, szTitle);
}

static int etxt_digits(int iN)
{
    int iD = 1;
    while (iN >= 10) { iN /= 10; iD++; }
    return iD;
}

static void etxt_recalc_gutter(edit_txt_view_t *ptV)
{
    ptV->iGutterW = (etxt_digits(ptV->iLineCount) + GUTTER_PAD)
                    * ptV->iCellW;
}

/* ------------------------------------------------------------------
 * Display-column helpers (tab expansion)
 * ------------------------------------------------------------------ */

/* Returns the display column of byte position iByteCol in szLine.
 * Tabs count as the number of spaces to the next tab stop. */
static int etxt_display_len(const char *szLine, int iByteCol)
{
    int iDisp = 0;
    int i;

    for (i = 0; i < iByteCol && szLine[i] != '\0'; i++)
    {
        if (szLine[i] == '\t')
            iDisp = (iDisp / EDIT_TXT_TAB_WIDTH + 1) * EDIT_TXT_TAB_WIDTH;
        else
            iDisp++;
    }
    return iDisp;
}

/* Returns the byte column closest to display column iDisp in szLine. */
static int etxt_byte_col_from_disp(const char *szLine, int iDisp)
{
    int iCurDisp = 0;
    int i        = 0;
    int iNext;

    while (szLine[i] != '\0')
    {
        if (iCurDisp >= iDisp) break;
        if (szLine[i] == '\t')
            iNext = (iCurDisp / EDIT_TXT_TAB_WIDTH + 1) * EDIT_TXT_TAB_WIDTH;
        else
            iNext = iCurDisp + 1;
        if (iNext > iDisp) break;
        iCurDisp = iNext;
        i++;
    }
    return i;
}

/* ------------------------------------------------------------------
 * Scroll
 * ------------------------------------------------------------------ */

static void etxt_scroll_to_cursor(edit_txt_view_t *ptV)
{
    int iVisRows;
    int iVisCols;
    int iCurDisp;

    iVisRows = (ptV->iCellH > 0) ? ptV->iWndHeight / ptV->iCellH : 1;
    iVisCols = (ptV->iCellW > 0)
               ? (ptV->iWndWidth - ptV->iGutterW) / ptV->iCellW : 1;
    if (iVisRows < 1) iVisRows = 1;
    if (iVisCols < 1) iVisCols = 1;

    /* Vertical */
    if (ptV->tCursor.iLine < ptV->iScrollLine)
        ptV->iScrollLine = ptV->tCursor.iLine;
    if (ptV->tCursor.iLine >= ptV->iScrollLine + iVisRows)
        ptV->iScrollLine = ptV->tCursor.iLine - iVisRows + 1;

    /* Horizontal */
    iCurDisp = etxt_display_len(
        ptV->aszLines[ptV->tCursor.iLine], ptV->tCursor.iCol);
    if (iCurDisp < ptV->iScrollCol)
        ptV->iScrollCol = iCurDisp;
    if (iCurDisp >= ptV->iScrollCol + iVisCols)
        ptV->iScrollCol = iCurDisp - iVisCols + 1;
}

/* ------------------------------------------------------------------
 * Rendering
 * ------------------------------------------------------------------ */

static void etxt_render(edit_txt_view_t *ptV)
{
    int    iVisRows;
    int    iRow;
    int    iLine;
    float  fY;
    float  fX;
    char   szExpanded[EDIT_TXT_MAX_LINE_LEN * EDIT_TXT_TAB_WIDTH + 1];
    char   szLineNo[16];
    int    iExpLen;
    int    iGutDigits;
    renderer_rect_t tRect;

    if (ptV->hRenderer == NULL) return;

    iVisRows   = (ptV->iCellH > 0) ? ptV->iWndHeight / ptV->iCellH : 1;
    iGutDigits = etxt_digits(ptV->iLineCount);

    renderer_begin_draw(ptV->hRenderer, &g_clrBg);

    /* Gutter background covers the full-height paint below */

    /* Gutter background */
    tRect.fX = 0.0f;
    tRect.fY = 0.0f;
    tRect.fW = (float)ptV->iGutterW;
    tRect.fH = (float)ptV->iWndHeight;
    renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrGutter);

    for (iRow = 0; iRow < iVisRows; iRow++)
    {
        edit_pos_t tSelStart;
        edit_pos_t tSelEnd;
        st_bool_t  bHasSel;
        int        iSelStartDisp;
        int        iSelEndDisp;

        iLine = ptV->iScrollLine + iRow;
        if (iLine >= ptV->iLineCount) break;
        fY = (float)(iRow * ptV->iCellH);

        /* Current-line highlight */
        if (iLine == ptV->tCursor.iLine)
        {
            tRect.fX = (float)ptV->iGutterW;
            tRect.fY = fY;
            tRect.fW = (float)(ptV->iWndWidth - ptV->iGutterW);
            tRect.fH = (float)ptV->iCellH;
            renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrCurLine);
        }

        /* Selection on this line */
        bHasSel = etxt_has_sel(ptV);
        if (bHasSel)
        {
            etxt_sel_order(ptV, &tSelStart, &tSelEnd);
            if (iLine >= tSelStart.iLine && iLine <= tSelEnd.iLine)
            {
                const char *szL = ptV->aszLines[iLine];
                int iLineLen    = (int)strlen(szL);
                int iS, iE;

                iS = (iLine == tSelStart.iLine)
                     ? tSelStart.iCol : 0;
                iE = (iLine == tSelEnd.iLine)
                     ? tSelEnd.iCol   : iLineLen;

                iSelStartDisp = etxt_display_len(szL, iS)
                                - ptV->iScrollCol;
                iSelEndDisp   = etxt_display_len(szL, iE)
                                - ptV->iScrollCol;

                /* Extend to end of line for multi-line selection */
                if (iLine < tSelEnd.iLine)
                {
                    int iTot = etxt_display_len(szL, iLineLen);
                    iSelEndDisp = (iTot - ptV->iScrollCol) + 1;
                }

                if (iSelEndDisp > iSelStartDisp)
                {
                    float fSx = (float)(ptV->iGutterW
                                 + iSelStartDisp * ptV->iCellW);
                    float fSw = (float)((iSelEndDisp - iSelStartDisp)
                                         * ptV->iCellW);
                    if (fSx < (float)ptV->iGutterW)
                    {
                        fSw -= ((float)ptV->iGutterW - fSx);
                        fSx  = (float)ptV->iGutterW;
                    }
                    if (fSw > 0.0f)
                    {
                        tRect.fX = fSx;
                        tRect.fY = fY;
                        tRect.fW = fSw;
                        tRect.fH = (float)ptV->iCellH;
                        renderer_fill_rect(ptV->hRenderer, &tRect,
                                           &g_clrSel);
                    }
                }
            }
        }

        /* Line number */
        snprintf(szLineNo, sizeof(szLineNo),
                 "%*d", iGutDigits, iLine + 1);
        tRect.fX = 0.0f;
        tRect.fY = fY;
        tRect.fW = (float)(ptV->iGutterW - ptV->iCellW / 2);
        tRect.fH = (float)ptV->iCellH;
        renderer_draw_text(ptV->hRenderer, szLineNo, &tRect,
                           &g_clrLineNo, RENDERER_FONT_MONO,
                           RENDERER_ALIGN_RIGHT);

        /* Text — expand tabs, clip to iScrollCol */
        {
            const char *szL  = ptV->aszLines[iLine];
            int         iInB = 0;
            int         iInD = 0;
            int         iOutD = 0;
            int         iBufPos = 0;
            int         iLenL;

            iLenL = (int)strlen(szL);
            /* Build visible portion starting at iScrollCol */
            while (szL[iInB] != '\0' && iBufPos < (int)sizeof(szExpanded) - 1)
            {
                int iAdvD;
                if (szL[iInB] == '\t')
                    iAdvD = (iInD / EDIT_TXT_TAB_WIDTH + 1)
                            * EDIT_TXT_TAB_WIDTH - iInD;
                else
                    iAdvD = 1;

                if (iInD + iAdvD > ptV->iScrollCol)
                {
                    /* Partially or fully visible */
                    int iStartD = (iInD < ptV->iScrollCol)
                                   ? ptV->iScrollCol - iInD : 0;
                    int k;
                    for (k = iStartD; k < iAdvD; k++)
                    {
                        if (iBufPos < (int)sizeof(szExpanded) - 1)
                            szExpanded[iBufPos++] = ' ';
                    }
                }
                iInD += iAdvD;
                if (szL[iInB] != '\t' && iInD > ptV->iScrollCol)
                {
                    /* Was a non-tab: replace the last ' ' with actual char */
                    if (iBufPos > 0)
                        szExpanded[iBufPos - 1] = szL[iInB];
                }
                iInB++;
                (void)iLenL; /* suppress unused warning */
            }
            szExpanded[iBufPos] = '\0';
            iExpLen = iBufPos;
            (void)iOutD; /* suppress unused warning */
        }

        if (iExpLen > 0)
        {
            fX = (float)ptV->iGutterW;
            tRect.fX = fX;
            tRect.fY = fY;
            tRect.fW = (float)(ptV->iWndWidth - ptV->iGutterW);
            tRect.fH = (float)ptV->iCellH;
            renderer_draw_text(ptV->hRenderer, szExpanded, &tRect,
                               &g_clrText, RENDERER_FONT_MONO,
                               RENDERER_ALIGN_LEFT);
        }

        /* Cursor on this line */
        if (iLine == ptV->tCursor.iLine)
        {
            int iCurDisp = etxt_display_len(
                ptV->aszLines[iLine], ptV->tCursor.iCol)
                - ptV->iScrollCol;
            float fCx = (float)(ptV->iGutterW
                                 + iCurDisp * ptV->iCellW);
            tRect.fX = fCx;
            tRect.fY = fY;
            tRect.fW = (float)CURSOR_BAR_W;
            tRect.fH = (float)ptV->iCellH;
            renderer_fill_rect(ptV->hRenderer, &tRect, &g_clrCursor);
        }
    }

    renderer_end_draw(ptV->hRenderer);
}

/* ------------------------------------------------------------------
 * Selection helpers
 * ------------------------------------------------------------------ */

static void etxt_sel_order(const edit_txt_view_t *ptV,
                            edit_pos_t            *ptStart,
                            edit_pos_t            *ptEnd)
{
    int bBefore;

    bBefore = (ptV->iSelAnchorLine < ptV->tCursor.iLine)
           || (ptV->iSelAnchorLine == ptV->tCursor.iLine
               && ptV->iSelAnchorCol <= ptV->tCursor.iCol);

    if (bBefore)
    {
        ptStart->iLine = ptV->iSelAnchorLine;
        ptStart->iCol  = ptV->iSelAnchorCol;
        ptEnd->iLine   = ptV->tCursor.iLine;
        ptEnd->iCol    = ptV->tCursor.iCol;
    }
    else
    {
        ptStart->iLine = ptV->tCursor.iLine;
        ptStart->iCol  = ptV->tCursor.iCol;
        ptEnd->iLine   = ptV->iSelAnchorLine;
        ptEnd->iCol    = ptV->iSelAnchorCol;
    }
}

static st_bool_t etxt_has_sel(const edit_txt_view_t *ptV)
{
    if (ptV->iSelAnchorLine < 0) return ST_FALSE;
    return (st_bool_t)(ptV->iSelAnchorLine != ptV->tCursor.iLine
                    || ptV->iSelAnchorCol  != ptV->tCursor.iCol);
}

static void etxt_sel_clear(edit_txt_view_t *ptV)
{
    ptV->iSelAnchorLine = -1;
    ptV->iSelAnchorCol  =  0;
}

static void etxt_sel_set_anchor(edit_txt_view_t *ptV)
{
    ptV->iSelAnchorLine = ptV->tCursor.iLine;
    ptV->iSelAnchorCol  = ptV->tCursor.iCol;
}

/* Delete the selected text; cursor moves to selection start. */
static st_error_t etxt_del_sel(edit_txt_view_t *ptV)
{
    edit_pos_t  tS;
    edit_pos_t  tE;
    char       *szFirst;
    char       *szLast;
    char       *szMerge;
    int         iPreLen;
    int         iSufLen;
    int         iMergeLen;
    int         iDel;
    int         i;

    if (!etxt_has_sel(ptV)) return ST_NO_ERROR;
    etxt_sel_order(ptV, &tS, &tE);

    if (tS.iLine == tE.iLine)
    {
        /* Same-line deletion */
        szFirst = ptV->aszLines[tS.iLine];
        iDel    = tE.iCol - tS.iCol;
        memmove(szFirst + tS.iCol,
                szFirst + tE.iCol,
                strlen(szFirst + tE.iCol) + 1);
    }
    else
    {
        /* Multi-line: merge prefix of first line + suffix of last */
        szFirst   = ptV->aszLines[tS.iLine];
        szLast    = ptV->aszLines[tE.iLine];
        iPreLen   = tS.iCol;
        iSufLen   = (int)strlen(szLast) - tE.iCol;
        iMergeLen = iPreLen + iSufLen;
        szMerge   = (char *)malloc((size_t)iMergeLen + 1);
        if (szMerge == NULL) { LOG_ERROR("malloc failed"); return ST_ERROR; }

        memcpy(szMerge, szFirst, (size_t)iPreLen);
        memcpy(szMerge + iPreLen,
               szLast + tE.iCol, (size_t)iSufLen + 1);

        free(ptV->aszLines[tS.iLine]);
        ptV->aszLines[tS.iLine] = szMerge;

        /* Free intermediate + last line, close the gap */
        for (i = tS.iLine + 1; i <= tE.iLine; i++)
            free(ptV->aszLines[i]);

        iDel = tE.iLine - tS.iLine;
        memmove(ptV->aszLines + tS.iLine + 1,
                ptV->aszLines + tE.iLine + 1,
                (size_t)(ptV->iLineCount - tE.iLine - 1)
                * sizeof(char *));
        ptV->iLineCount -= iDel;
    }

    ptV->tCursor = tS;
    etxt_sel_clear(ptV);
    ptV->bDirty = ST_TRUE;
    (void)iDel;
    return ST_NO_ERROR;
}

/* Copy selection to clipboard. */
static st_error_t etxt_copy_sel(edit_txt_view_t *ptV)
{
    char       szBuf[EDIT_TXT_CLIP_MAX];
    edit_pos_t tS;
    edit_pos_t tE;
    int        iPos = 0;
    int        iLine;
    int        iLen;

    if (!etxt_has_sel(ptV)) return ST_NO_ERROR;
    etxt_sel_order(ptV, &tS, &tE);

    if (tS.iLine == tE.iLine)
    {
        iLen = tE.iCol - tS.iCol;
        if (iLen > 0 && iPos + iLen < EDIT_TXT_CLIP_MAX)
        {
            memcpy(szBuf, ptV->aszLines[tS.iLine] + tS.iCol,
                   (size_t)iLen);
            iPos += iLen;
        }
    }
    else
    {
        /* First line: from tS.iCol to end */
        iLen = (int)strlen(ptV->aszLines[tS.iLine]) - tS.iCol;
        if (iLen > 0 && iPos + iLen < EDIT_TXT_CLIP_MAX)
        {
            memcpy(szBuf + iPos,
                   ptV->aszLines[tS.iLine] + tS.iCol,
                   (size_t)iLen);
            iPos += iLen;
        }
        if (iPos < EDIT_TXT_CLIP_MAX) szBuf[iPos++] = '\n';

        /* Middle lines */
        for (iLine = tS.iLine + 1; iLine < tE.iLine; iLine++)
        {
            iLen = (int)strlen(ptV->aszLines[iLine]);
            if (iPos + iLen + 1 >= EDIT_TXT_CLIP_MAX) break;
            memcpy(szBuf + iPos, ptV->aszLines[iLine],
                   (size_t)iLen);
            iPos += iLen;
            szBuf[iPos++] = '\n';
        }

        /* Last line: from 0 to tE.iCol */
        if (tE.iCol > 0 && iPos + tE.iCol < EDIT_TXT_CLIP_MAX)
        {
            memcpy(szBuf + iPos, ptV->aszLines[tE.iLine],
                   (size_t)tE.iCol);
            iPos += tE.iCol;
        }
    }

    szBuf[iPos] = '\0';
    if (iPos > 0)
        gui_clipboard_set_text(szBuf);
    return ST_NO_ERROR;
}

/* Paste clipboard text at cursor. */
static st_error_t etxt_paste(edit_txt_view_t *ptV)
{
    char  szClip[EDIT_TXT_CLIP_MAX];
    char *p;

    if (gui_clipboard_get_text(szClip, sizeof(szClip)) != ST_NO_ERROR)
        return ST_NO_ERROR;

    if (etxt_has_sel(ptV))
    {
        if (etxt_del_sel(ptV) != ST_NO_ERROR) return ST_ERROR;
    }

    for (p = szClip; *p != '\0'; p++)
    {
        if (*p == '\r') continue;
        if (*p == '\n')
        {
            if (etxt_insert_newline(ptV) != ST_NO_ERROR)
                return ST_ERROR;
        }
        else
        {
            if (etxt_insert_char(ptV, *p) != ST_NO_ERROR)
                return ST_ERROR;
        }
    }
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Edit operations
 * ------------------------------------------------------------------ */

static st_error_t etxt_insert_char(edit_txt_view_t *ptV, char c)
{
    char  *szLine;
    char  *szNew;
    int    iLen;

    szLine = ptV->aszLines[ptV->tCursor.iLine];
    iLen   = (int)strlen(szLine);
    if (iLen >= EDIT_TXT_MAX_LINE_LEN - 1)
        return ST_NO_ERROR;  /* line full: silently ignore */

    szNew = (char *)realloc(szLine, (size_t)iLen + 2);
    if (szNew == NULL)
    {
        LOG_ERROR("realloc failed inserting char");
        return ST_ERROR;
    }
    ptV->aszLines[ptV->tCursor.iLine] = szNew;

    memmove(szNew + ptV->tCursor.iCol + 1,
            szNew + ptV->tCursor.iCol,
            (size_t)(iLen - ptV->tCursor.iCol) + 1);
    szNew[ptV->tCursor.iCol] = c;
    ptV->tCursor.iCol++;
    ptV->bDirty = ST_TRUE;
    return ST_NO_ERROR;
}

static st_error_t etxt_insert_newline(edit_txt_view_t *ptV)
{
    char *szLine;
    char *szNewLine;
    int   iSufLen;
    int   iLine;

    if (etxt_alloc_line_slot(ptV) != ST_NO_ERROR) return ST_ERROR;

    szLine   = ptV->aszLines[ptV->tCursor.iLine];
    iSufLen  = (int)strlen(szLine) - ptV->tCursor.iCol;
    szNewLine = (char *)malloc((size_t)iSufLen + 1);
    if (szNewLine == NULL) { LOG_ERROR("malloc failed"); return ST_ERROR; }
    memcpy(szNewLine, szLine + ptV->tCursor.iCol, (size_t)iSufLen + 1);
    szLine[ptV->tCursor.iCol] = '\0';  /* truncate current line */

    /* Realloc current line (shrink) — ignore failure, just wastes mem */
    {
        char *szShrunk = (char *)realloc(szLine,
                                          (size_t)ptV->tCursor.iCol + 1);
        if (szShrunk != NULL)
            ptV->aszLines[ptV->tCursor.iLine] = szShrunk;
    }

    /* Insert new line after current */
    iLine = ptV->tCursor.iLine + 1;
    memmove(ptV->aszLines + iLine + 1,
            ptV->aszLines + iLine,
            (size_t)(ptV->iLineCount - iLine) * sizeof(char *));
    ptV->aszLines[iLine] = szNewLine;
    ptV->iLineCount++;

    ptV->tCursor.iLine = iLine;
    ptV->tCursor.iCol  = 0;
    ptV->bDirty = ST_TRUE;
    return ST_NO_ERROR;
}

static st_error_t etxt_del_back(edit_txt_view_t *ptV)
{
    char *szLine;
    char *szPrev;
    char *szMerge;
    int   iPrevLen;
    int   iLen;

    if (ptV->tCursor.iCol > 0)
    {
        szLine = ptV->aszLines[ptV->tCursor.iLine];
        memmove(szLine + ptV->tCursor.iCol - 1,
                szLine + ptV->tCursor.iCol,
                strlen(szLine + ptV->tCursor.iCol) + 1);
        ptV->tCursor.iCol--;
        ptV->bDirty = ST_TRUE;
    }
    else if (ptV->tCursor.iLine > 0)
    {
        /* Merge with previous line */
        szPrev    = ptV->aszLines[ptV->tCursor.iLine - 1];
        szLine    = ptV->aszLines[ptV->tCursor.iLine];
        iPrevLen  = (int)strlen(szPrev);
        iLen      = (int)strlen(szLine);
        szMerge   = (char *)malloc((size_t)(iPrevLen + iLen) + 1);
        if (szMerge == NULL) { LOG_ERROR("malloc failed"); return ST_ERROR; }
        memcpy(szMerge, szPrev, (size_t)iPrevLen);
        memcpy(szMerge + iPrevLen, szLine, (size_t)iLen + 1);

        free(ptV->aszLines[ptV->tCursor.iLine - 1]);
        free(ptV->aszLines[ptV->tCursor.iLine]);
        ptV->aszLines[ptV->tCursor.iLine - 1] = szMerge;
        memmove(ptV->aszLines + ptV->tCursor.iLine,
                ptV->aszLines + ptV->tCursor.iLine + 1,
                (size_t)(ptV->iLineCount - ptV->tCursor.iLine - 1)
                * sizeof(char *));
        ptV->iLineCount--;
        ptV->tCursor.iLine--;
        ptV->tCursor.iCol = iPrevLen;
        ptV->bDirty = ST_TRUE;
    }
    return ST_NO_ERROR;
}

static st_error_t etxt_del_fwd(edit_txt_view_t *ptV)
{
    char *szLine;
    char *szNext;
    char *szMerge;
    int   iLen;
    int   iNextLen;

    szLine = ptV->aszLines[ptV->tCursor.iLine];
    iLen   = (int)strlen(szLine);

    if (ptV->tCursor.iCol < iLen)
    {
        memmove(szLine + ptV->tCursor.iCol,
                szLine + ptV->tCursor.iCol + 1,
                (size_t)(iLen - ptV->tCursor.iCol));
        ptV->bDirty = ST_TRUE;
    }
    else if (ptV->tCursor.iLine < ptV->iLineCount - 1)
    {
        szNext   = ptV->aszLines[ptV->tCursor.iLine + 1];
        iNextLen = (int)strlen(szNext);
        szMerge  = (char *)malloc((size_t)(iLen + iNextLen) + 1);
        if (szMerge == NULL) { LOG_ERROR("malloc failed"); return ST_ERROR; }
        memcpy(szMerge, szLine, (size_t)iLen);
        memcpy(szMerge + iLen, szNext, (size_t)iNextLen + 1);

        free(ptV->aszLines[ptV->tCursor.iLine]);
        free(ptV->aszLines[ptV->tCursor.iLine + 1]);
        ptV->aszLines[ptV->tCursor.iLine] = szMerge;
        memmove(ptV->aszLines + ptV->tCursor.iLine + 1,
                ptV->aszLines + ptV->tCursor.iLine + 2,
                (size_t)(ptV->iLineCount - ptV->tCursor.iLine - 2)
                * sizeof(char *));
        ptV->iLineCount--;
        ptV->bDirty = ST_TRUE;
    }
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Word navigation helpers
 * ------------------------------------------------------------------ */

static int etxt_is_word_char(char c)
{
    return (isalnum((unsigned char)c) || c == '_');
}

static void etxt_move_word_left(edit_txt_view_t *ptV)
{
    const char *szLine = ptV->aszLines[ptV->tCursor.iLine];
    int         iCol   = ptV->tCursor.iCol;

    if (iCol == 0)
    {
        if (ptV->tCursor.iLine > 0)
        {
            ptV->tCursor.iLine--;
            ptV->tCursor.iCol =
                (int)strlen(ptV->aszLines[ptV->tCursor.iLine]);
        }
        return;
    }
    iCol--;
    while (iCol > 0 && !etxt_is_word_char(szLine[iCol - 1]))
        iCol--;
    while (iCol > 0 && etxt_is_word_char(szLine[iCol - 1]))
        iCol--;
    ptV->tCursor.iCol = iCol;
}

static void etxt_move_word_right(edit_txt_view_t *ptV)
{
    const char *szLine = ptV->aszLines[ptV->tCursor.iLine];
    int         iLen   = (int)strlen(szLine);
    int         iCol   = ptV->tCursor.iCol;

    if (iCol >= iLen)
    {
        if (ptV->tCursor.iLine < ptV->iLineCount - 1)
        {
            ptV->tCursor.iLine++;
            ptV->tCursor.iCol = 0;
        }
        return;
    }
    while (iCol < iLen && !etxt_is_word_char(szLine[iCol])) iCol++;
    while (iCol < iLen &&  etxt_is_word_char(szLine[iCol])) iCol++;
    ptV->tCursor.iCol = iCol;
}

static void etxt_clamp_col(edit_txt_view_t *ptV)
{
    int iLen = (int)strlen(ptV->aszLines[ptV->tCursor.iLine]);
    if (ptV->tCursor.iCol > iLen)
        ptV->tCursor.iCol = iLen;
}

/* ------------------------------------------------------------------
 * Key handler
 * ------------------------------------------------------------------ */

static void etxt_handle_key(edit_txt_view_t *ptV,
                             gui_key_t        eKey,
                             char             cChar,
                             st_u8_t          uiMods)
{
    int       iVisRows;
    int       bShift;
    int       bCtrl;
    int       iLen;

    bShift = (uiMods & GUI_MOD_SHIFT) != 0;
    bCtrl  = (uiMods & GUI_MOD_CTRL)  != 0;

    iVisRows = (ptV->iCellH > 0) ? ptV->iWndHeight / ptV->iCellH : 10;
    if (iVisRows < 1) iVisRows = 1;

    /* CTRL+letter shortcuts (forwarded as GUI_KEY_PRINTABLE+'A'..'Z') */
    if (bCtrl && eKey >= GUI_KEY_PRINTABLE)
    {
        int iLetter = (int)(eKey - GUI_KEY_PRINTABLE);
        switch (iLetter)
        {
        case 'A':
            /* Select all — navigation, no buffer change */
            ptV->bUndoGroupInsert = ST_FALSE;
            ptV->iSelAnchorLine   = 0;
            ptV->iSelAnchorCol    = 0;
            ptV->tCursor.iLine    = ptV->iLineCount - 1;
            ptV->tCursor.iCol     =
                (int)strlen(ptV->aszLines[ptV->iLineCount - 1]);
            break;
        case 'C':
            ptV->bUndoGroupInsert = ST_FALSE;
            etxt_copy_sel(ptV);
            return;
        case 'X':
            ptV->bUndoGroupInsert = ST_FALSE;
            etxt_copy_sel(ptV);
            if (etxt_has_sel(ptV))
            {
                etxt_undo_push(ptV);
                etxt_del_sel(ptV);
                etxt_recalc_gutter(ptV);
            }
            break;
        case 'V':
            ptV->bUndoGroupInsert = ST_FALSE;
            etxt_undo_push(ptV);
            if (etxt_has_sel(ptV)) etxt_del_sel(ptV);
            etxt_paste(ptV);
            etxt_recalc_gutter(ptV);
            break;
        case 'S':
            ptV->bUndoGroupInsert = ST_FALSE;
            if (ptV->bDirty)
            {
                if (etxt_save(ptV) == ST_NO_ERROR)
                    etxt_update_title(ptV);
                else
                    LOG_ERROR("save failed for '%s'", ptV->szPath);
            }
            return;
        case 'Z':
            etxt_undo_pop(ptV);
            etxt_recalc_gutter(ptV);
            break;
        default:
            return;
        }
        etxt_scroll_to_cursor(ptV);
        gui_invalidate(ptV->hWnd);
        return;
    }

    /* Navigation with optional SHIFT to extend selection */
    switch (eKey)
    {
    case GUI_KEY_LEFT:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (!bShift) etxt_sel_clear(ptV);
        else if (!etxt_has_sel(ptV) && !bShift)
            ; /* nothing */
        else if (bShift && ptV->iSelAnchorLine < 0)
            etxt_sel_set_anchor(ptV);

        if (bShift && ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
        if (!bShift) etxt_sel_clear(ptV);

        if (bCtrl)
            etxt_move_word_left(ptV);
        else if (ptV->tCursor.iCol > 0)
            ptV->tCursor.iCol--;
        else if (ptV->tCursor.iLine > 0)
        {
            ptV->tCursor.iLine--;
            ptV->tCursor.iCol =
                (int)strlen(ptV->aszLines[ptV->tCursor.iLine]);
        }
        break;

    case GUI_KEY_RIGHT:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (bShift && ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
        if (!bShift) etxt_sel_clear(ptV);

        if (bCtrl)
            etxt_move_word_right(ptV);
        else
        {
            iLen = (int)strlen(ptV->aszLines[ptV->tCursor.iLine]);
            if (ptV->tCursor.iCol < iLen)
                ptV->tCursor.iCol++;
            else if (ptV->tCursor.iLine < ptV->iLineCount - 1)
            {
                ptV->tCursor.iLine++;
                ptV->tCursor.iCol = 0;
            }
        }
        break;

    case GUI_KEY_UP:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (bShift && ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
        if (!bShift) etxt_sel_clear(ptV);
        if (ptV->tCursor.iLine > 0)
        {
            ptV->tCursor.iLine--;
            etxt_clamp_col(ptV);
        }
        break;

    case GUI_KEY_DOWN:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (bShift && ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
        if (!bShift) etxt_sel_clear(ptV);
        if (ptV->tCursor.iLine < ptV->iLineCount - 1)
        {
            ptV->tCursor.iLine++;
            etxt_clamp_col(ptV);
        }
        break;

    case GUI_KEY_HOME:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (bShift && ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
        if (!bShift) etxt_sel_clear(ptV);
        if (bCtrl)
        {
            ptV->tCursor.iLine = 0;
            ptV->tCursor.iCol  = 0;
        }
        else
            ptV->tCursor.iCol = 0;
        break;

    case GUI_KEY_END:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (bShift && ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
        if (!bShift) etxt_sel_clear(ptV);
        if (bCtrl)
        {
            ptV->tCursor.iLine = ptV->iLineCount - 1;
            ptV->tCursor.iCol  =
                (int)strlen(ptV->aszLines[ptV->tCursor.iLine]);
        }
        else
            ptV->tCursor.iCol =
                (int)strlen(ptV->aszLines[ptV->tCursor.iLine]);
        break;

    case GUI_KEY_PAGE_UP:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (bShift && ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
        if (!bShift) etxt_sel_clear(ptV);
        ptV->tCursor.iLine -= iVisRows;
        if (ptV->tCursor.iLine < 0) ptV->tCursor.iLine = 0;
        etxt_clamp_col(ptV);
        break;

    case GUI_KEY_PAGE_DOWN:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (bShift && ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
        if (!bShift) etxt_sel_clear(ptV);
        ptV->tCursor.iLine += iVisRows;
        if (ptV->tCursor.iLine >= ptV->iLineCount)
            ptV->tCursor.iLine = ptV->iLineCount - 1;
        etxt_clamp_col(ptV);
        break;

    case GUI_KEY_ENTER:
        ptV->bUndoGroupInsert = ST_FALSE;
        etxt_undo_push(ptV);
        if (etxt_has_sel(ptV)) etxt_del_sel(ptV);
        etxt_insert_newline(ptV);
        etxt_recalc_gutter(ptV);
        break;

    case GUI_KEY_TAB:
        /* Tab treated as an insert group-breaker (structural indent) */
        ptV->bUndoGroupInsert = ST_FALSE;
        etxt_undo_push(ptV);
        if (etxt_has_sel(ptV)) etxt_del_sel(ptV);
        etxt_insert_char(ptV, '\t');
        ptV->bUndoGroupInsert = ST_FALSE;
        break;

    case GUI_KEY_BACKSPACE:
        ptV->bUndoGroupInsert = ST_FALSE;
        etxt_undo_push(ptV);
        if (etxt_has_sel(ptV))
        {
            etxt_del_sel(ptV);
            etxt_recalc_gutter(ptV);
        }
        else
        {
            etxt_del_back(ptV);
            etxt_recalc_gutter(ptV);
        }
        break;

    case GUI_KEY_DELETE:
        ptV->bUndoGroupInsert = ST_FALSE;
        etxt_undo_push(ptV);
        if (etxt_has_sel(ptV))
        {
            etxt_del_sel(ptV);
            etxt_recalc_gutter(ptV);
        }
        else
        {
            etxt_del_fwd(ptV);
            etxt_recalc_gutter(ptV);
        }
        break;

    case GUI_KEY_ESCAPE:
        ptV->bUndoGroupInsert = ST_FALSE;
        if (ptV->bDirty)
            LOG_INFO("edit: closed '%s' with unsaved changes",
                     ptV->szPath);
        gui_request_close(ptV->hWnd);
        return;

    default:
        if (eKey >= GUI_KEY_PRINTABLE && !bCtrl)
        {
            /* Group consecutive inserts: push only on first of a run */
            if (!ptV->bUndoGroupInsert)
                etxt_undo_push(ptV);
            ptV->bUndoGroupInsert = ST_TRUE;
            if (etxt_has_sel(ptV))
            {
                etxt_del_sel(ptV);
                ptV->bUndoGroupInsert = ST_FALSE; /* sel+insert = one */
            }
            etxt_insert_char(ptV, cChar);
            etxt_recalc_gutter(ptV);
        }
        else
        {
            return;  /* unhandled key: no redraw */
        }
        break;
    }

    etxt_scroll_to_cursor(ptV);
    etxt_update_title(ptV);
    gui_invalidate(ptV->hWnd);
}

/* ------------------------------------------------------------------
 * Mouse click: set cursor
 * ------------------------------------------------------------------ */

static void etxt_handle_click(edit_txt_view_t *ptV, int iX, int iY,
                               st_u8_t uiMods)
{
    int iLine;
    int iDispCol;
    int iByteCol;

    iLine = ptV->iScrollLine + iY / ptV->iCellH;
    if (iLine >= ptV->iLineCount)
        iLine = ptV->iLineCount - 1;
    if (iLine < 0) iLine = 0;

    iDispCol = ptV->iScrollCol + (iX - ptV->iGutterW) / ptV->iCellW;
    if (iDispCol < 0) iDispCol = 0;

    iByteCol = etxt_byte_col_from_disp(ptV->aszLines[iLine], iDispCol);

    if (uiMods & GUI_MOD_SHIFT)
    {
        if (ptV->iSelAnchorLine < 0) etxt_sel_set_anchor(ptV);
    }
    else
        etxt_sel_clear(ptV);

    ptV->tCursor.iLine = iLine;
    ptV->tCursor.iCol  = iByteCol;
    gui_invalidate(ptV->hWnd);
}

/* ------------------------------------------------------------------
 * GUI event callback
 * ------------------------------------------------------------------ */

static void etxt_event_callback(gui_window_t  hWnd,
                                 gui_event_t  *ptEvent,
                                 void         *pCtx)
{
    edit_txt_view_t *ptV = (edit_txt_view_t *)pCtx;
    int              iVisRows;

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
                                          &ptV->iCellW, &ptV->iCellH);
                etxt_recalc_gutter(ptV);
            }
        }
        if (ptV->hRenderer != NULL)
        {
            etxt_update_title(ptV);
            etxt_render(ptV);
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
        etxt_handle_key(ptV,
                         ptEvent->uData.tKey.eKey,
                         ptEvent->uData.tKey.cChar,
                         ptEvent->uData.tKey.uiMods);
        break;

    case GUI_EVT_MOUSE_DOWN:
        if (ptEvent->uData.tMouse.eBtn == GUI_MOUSE_LEFT
        &&  ptV->iCellH > 0 && ptV->iCellW > 0)
        {
            etxt_handle_click(ptV,
                               ptEvent->uData.tMouse.iX,
                               ptEvent->uData.tMouse.iY,
                               0);
        }
        break;

    case GUI_EVT_SCROLL:
        iVisRows = (ptV->iCellH > 0)
                   ? ptV->iWndHeight / ptV->iCellH : 1;
        ptV->iScrollLine -= ptEvent->uData.tScroll.iDelta;
        if (ptV->iScrollLine < 0)
            ptV->iScrollLine = 0;
        if (ptV->iScrollLine > ptV->iLineCount - 1)
            ptV->iScrollLine = ptV->iLineCount - 1;
        (void)iVisRows;
        gui_invalidate(hWnd);
        break;

    case GUI_EVT_CLOSE:
        etxt_finalize_backup(ptV);
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

st_error_t edit_txt_open(const char       *szPath,
                          line_context_t   *ptLineCtx,
                          edit_txt_view_t **pptView)
{
    edit_txt_view_t *ptV = NULL;
    gui_wnd_desc_t   tDesc;
    st_error_t       eRet = ST_ERROR;

    LOG_TRACE("szPath=%s ptLineCtx=%p pptView=%p",
              szPath ? szPath : "(null)",
              (void *)ptLineCtx, (void *)pptView);

    if (szPath == NULL || ptLineCtx == NULL || pptView == NULL)
    {
        LOG_ERROR("NULL parameter: szPath=%p ptLineCtx=%p pptView=%p",
                  (void *)szPath,
                  (void *)ptLineCtx, (void *)pptView);
        return ST_ERROR;
    }

    *pptView = NULL;

    ptV = (edit_txt_view_t *)malloc(sizeof(*ptV));
    if (ptV == NULL)
    {
        LOG_ERROR("malloc failed for edit_txt_view_t");
        return ST_ERROR;
    }
    memset(ptV, 0, sizeof(*ptV));

    strncpy(ptV->szPath, szPath, ST_MAX_PATH - 1);
    ptV->szPath[ST_MAX_PATH - 1] = '\0';
    ptV->ptLineCtx    = ptLineCtx;
    ptV->iSelAnchorLine = -1;
    ptV->bDirty       = ST_FALSE;

    if (etxt_load(ptV, szPath) != ST_NO_ERROR)
    {
        LOG_ERROR("failed to load '%s'", szPath);
        free(ptV);
        return ST_ERROR;
    }

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle  = "ST4Ever - Edit";
    tDesc.eType    = GUI_WND_EDIT_TXT;
    tDesc.pfnEvent = etxt_event_callback;
    tDesc.pUserCtx = ptV;

    if (gui_open_window(&tDesc, &ptV->hWnd) != ST_NO_ERROR)
    {
        LOG_ERROR("gui_open_window failed for '%s'", szPath);
        etxt_free_lines(ptV);
        free(ptV);
        return ST_ERROR;
    }

    *pptView = ptV;
    eRet = ST_NO_ERROR;
    LOG_INFO("edit_txt opened '%s' (%d lines)", szPath, ptV->iLineCount);
    return eRet;
}

st_error_t edit_txt_close(edit_txt_view_t **pptView)
{
    edit_txt_view_t *ptV;

    LOG_TRACE("pptView=%p", (void *)pptView);

    if (pptView == NULL)
    {
        LOG_ERROR("NULL parameter: pptView=%p", (void *)pptView);
        return ST_ERROR;
    }
    if (*pptView == NULL) return ST_NO_ERROR;

    ptV = *pptView;

    gui_close_window(ptV->hWnd);
    etxt_undo_free_all(ptV);
    etxt_free_lines(ptV);
    free(ptV);
    *pptView = NULL;

    LOG_INFO("edit_txt view closed");
    return ST_NO_ERROR;
}

void edit_txt_set_backup(st_bool_t bEnabled)
{
    g_etxt_bBackupEnabled = bEnabled;
    LOG_INFO("edit backup: %s", bEnabled ? "on" : "off");
}

st_bool_t edit_txt_get_backup(void)
{
    return g_etxt_bBackupEnabled;
}
