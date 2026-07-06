/*
 * exec_mem.c - ST4Ever Memory dump view during execution
 *
 * UC25B: 16-bytes/row hex+ASCII dump of ST RAM, PC row highlighted,
 * UP/DOWN/PgUp/PgDn navigation, HOME snaps to PC.
 */

#include "exec_mem.h"
#include "exec.h"
#include "trace.h"
#include "renderer.h"
#include "gui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Module global (one memory view at a time)
 * ------------------------------------------------------------------ */

static exec_mem_view_t *g_exec_mem_ptView = NULL;

/* ------------------------------------------------------------------
 * Colours
 * ------------------------------------------------------------------ */

static const renderer_color_t g_mem_clrBg    = RENDERER_COLOR_BLACK;
static const renderer_color_t g_mem_clrWhite = RENDERER_COLOR_WHITE;
static const renderer_color_t g_mem_clrGray  = RENDERER_COLOR_GRAY;
static const renderer_color_t g_mem_clrCyan  = RENDERER_COLOR_CYAN;
static const renderer_color_t g_mem_clrYellow = RENDERER_COLOR_YELLOW;
static const renderer_color_t g_mem_clrPCBg  =
    { 0.18f, 0.18f, 0.00f, 1.0f };

#define HINT_TEXT  \
    "[UP/DN] row  [PgUp/PgDn] page  [HOME] snap to PC  [ESC] close"

/* ------------------------------------------------------------------
 * Build one hex row: "$XXXXXX: XX XX XX XX  ...  cccccccccccccccc"
 * ------------------------------------------------------------------ */

static void exec_mem_build_row(char         *szOut,
                                int           iOutSize,
                                st_u32_t      uiAddr,
                                const st_u8_t *pRam,
                                st_u32_t      uiRamSize)
{
    char     szAscii[EXEC_MEM_BYTES_PER_ROW + 2];
    st_u32_t uiByteAddr;
    st_u8_t  uiByte;
    int      iOff;
    int      iCol;
    int      iRemain;

    iOff = snprintf(szOut, (size_t)iOutSize, "$%06X:", uiAddr);

    for (iCol = 0; iCol < EXEC_MEM_BYTES_PER_ROW; iCol++)
    {
        uiByteAddr = uiAddr + (st_u32_t)iCol;
        uiByte     = (uiByteAddr < uiRamSize)
                     ? pRam[uiByteAddr] : (st_u8_t)0;

        iRemain = iOutSize - iOff;
        if (iRemain <= 0) { break; }

        if ((iCol % 4) == 0)
        {
            iOff += snprintf(szOut + iOff, (size_t)iRemain, " ");
        }

        iRemain = iOutSize - iOff;
        if (iRemain > 0)
        {
            iOff += snprintf(szOut + iOff,
                             (size_t)iRemain, " %02X", uiByte);
        }

        szAscii[iCol] = (uiByte >= 0x20u && uiByte < 0x7Fu)
                        ? (char)uiByte : '.';
    }
    szAscii[EXEC_MEM_BYTES_PER_ROW] = '\0';

    iRemain = iOutSize - iOff;
    if (iRemain > 0)
    {
        snprintf(szOut + iOff, (size_t)iRemain, "  %s", szAscii);
    }
}

/* ------------------------------------------------------------------
 * Render
 * ------------------------------------------------------------------ */

static void exec_mem_render(exec_mem_view_t *ptView)
{
    exec_state_t          *ptState;
    st_u32_t               uiPC;
    st_u32_t               uiBase;
    st_u32_t               uiAddr;
    st_u32_t               uiRowEnd;
    st_u32_t               uiRamSize;
    const renderer_color_t *pclrTxt;
    int                    iCellW;
    int                    iCellH;
    int                    iW;
    int                    iH;
    int                    iY;
    int                    iRow;
    int                    iRowsVisible;
    int                    iMarginX;
    renderer_rect_t        tFull;
    renderer_rect_t        tRect;
    char                   szLine[128];

    ptState = ptView->ptState;
    if (ptState == NULL || ptView->hRenderer == NULL)
    {
        return;
    }

    renderer_get_font_metrics(ptView->hRenderer, RENDERER_FONT_MONO,
                              &iCellW, &iCellH);
    ptView->iCellW = iCellW;
    ptView->iCellH = iCellH;

    iW = ptView->iWndWidth;
    iH = ptView->iWndHeight;

    tFull = (renderer_rect_t)RENDERER_RECT(0, 0, iW, iH);

    renderer_begin_draw(ptView->hRenderer, &g_mem_clrBg);
    renderer_fill_rect(ptView->hRenderer, &tFull, &g_mem_clrBg);

    /* Snapshot PC under mutex */
    platform_mutex_lock(ptState->ptMutex);
    uiPC = ptState->tCpuSnap->uiPC;
    platform_mutex_unlock(ptState->ptMutex);

    uiRamSize = (st_u32_t)ST_RAM_SIZE;
    uiBase    = ptView->uiMemBase;
    iMarginX  = iCellW;

    if (iCellH <= 0) { iCellH = 16; }
    iRowsVisible = (iH - EXEC_MEM_HINT_H) / iCellH;
    if (iRowsVisible < 1) { iRowsVisible = 1; }

    /* Auto-snap: if PC is not visible, snap to its row */
    {
        st_u32_t uiVisibleEnd;
        uiVisibleEnd = uiBase
                       + (st_u32_t)(iRowsVisible * EXEC_MEM_BYTES_PER_ROW);
        if (uiPC < uiBase || uiPC >= uiVisibleEnd)
        {
            uiBase = uiPC & ~((st_u32_t)(EXEC_MEM_BYTES_PER_ROW - 1));
            ptView->uiMemBase = uiBase;
        }
    }

    iY = 0;
    for (iRow = 0; iRow < iRowsVisible; iRow++)
    {
        uiAddr = uiBase + (st_u32_t)(iRow * EXEC_MEM_BYTES_PER_ROW);

        if (uiAddr >= uiRamSize)
        {
            break;
        }

        exec_mem_build_row(szLine, (int)sizeof(szLine),
                           uiAddr,
                           ptState->ptMachine->aRam,
                           uiRamSize);

        uiRowEnd = uiAddr + (st_u32_t)EXEC_MEM_BYTES_PER_ROW;
        if (uiPC >= uiAddr && uiPC < uiRowEnd)
        {
            /* PC row: highlight background, yellow text */
            tRect    = (renderer_rect_t)RENDERER_RECT(0, iY, iW, iCellH);
            renderer_fill_rect(ptView->hRenderer, &tRect, &g_mem_clrPCBg);
            pclrTxt  = &g_mem_clrYellow;
        }
        else
        {
            pclrTxt = &g_mem_clrWhite;
        }

        tRect = (renderer_rect_t)RENDERER_RECT(iMarginX, iY,
                                                iW - iMarginX, iCellH);
        renderer_draw_text(ptView->hRenderer, szLine,
                           &tRect, pclrTxt,
                           RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
        iY += iCellH;
    }

    /* Hint bar */
    renderer_draw_line(ptView->hRenderer,
                       0.0f, (float)(iH - EXEC_MEM_HINT_H),
                       (float)iW, (float)(iH - EXEC_MEM_HINT_H),
                       &g_mem_clrGray, 1.0f);

    tRect = (renderer_rect_t)RENDERER_RECT(iMarginX,
                                            iH - EXEC_MEM_HINT_H + 2,
                                            iW - iMarginX * 2,
                                            EXEC_MEM_HINT_H - 2);
    renderer_draw_text(ptView->hRenderer, HINT_TEXT,
                       &tRect, &g_mem_clrCyan,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

    renderer_end_draw(ptView->hRenderer);
}

/* ------------------------------------------------------------------
 * Event callback (runs in window thread)
 * ------------------------------------------------------------------ */

static void exec_mem_event_cb(gui_window_t  hWnd,
                               gui_event_t  *ptEvent,
                               void         *pUserCtx)
{
    exec_mem_view_t *ptView;
    int              iCellH;
    int              iRowsPerPage;
    st_u32_t         uiPC;

    if (ptEvent == NULL || pUserCtx == NULL)
    {
        return;
    }

    ptView = (exec_mem_view_t *)pUserCtx;

    switch (ptEvent->eType)
    {
        case GUI_EVT_RESIZE:
            ptView->iWndWidth  = ptEvent->uData.tResize.iWidth;
            ptView->iWndHeight = ptEvent->uData.tResize.iHeight;
            if (ptView->hRenderer != NULL)
            {
                renderer_resize(ptView->hRenderer,
                                ptView->iWndWidth,
                                ptView->iWndHeight);
            }
            break;

        case GUI_EVT_PAINT:
            if (ptView->hRenderer == NULL)
            {
                renderer_create(hWnd, &ptView->hRenderer);
                gui_get_size(hWnd,
                             &ptView->iWndWidth,
                             &ptView->iWndHeight);
            }
            exec_mem_render(ptView);
            break;

        case GUI_EVT_KEY_DOWN:
            iCellH = (ptView->iCellH > 0) ? ptView->iCellH : 16;
            iRowsPerPage = (ptView->iWndHeight - EXEC_MEM_HINT_H)
                           / iCellH;
            if (iRowsPerPage < 1) { iRowsPerPage = 1; }

            switch (ptEvent->uData.tKey.eKey)
            {
                case GUI_KEY_UP:
                    if (ptView->uiMemBase >= (st_u32_t)EXEC_MEM_BYTES_PER_ROW)
                    {
                        ptView->uiMemBase -=
                            (st_u32_t)EXEC_MEM_BYTES_PER_ROW;
                    }
                    else
                    {
                        ptView->uiMemBase = 0;
                    }
                    gui_invalidate(hWnd);
                    break;

                case GUI_KEY_DOWN:
                    ptView->uiMemBase +=
                        (st_u32_t)EXEC_MEM_BYTES_PER_ROW;
                    if (ptView->uiMemBase >= (st_u32_t)ST_RAM_SIZE)
                    {
                        ptView->uiMemBase =
                            (st_u32_t)(ST_RAM_SIZE - EXEC_MEM_BYTES_PER_ROW);
                    }
                    gui_invalidate(hWnd);
                    break;

                case GUI_KEY_PAGE_UP:
                {
                    st_u32_t uiStep = (st_u32_t)(iRowsPerPage
                                       * EXEC_MEM_BYTES_PER_ROW);
                    if (ptView->uiMemBase >= uiStep)
                    {
                        ptView->uiMemBase -= uiStep;
                    }
                    else
                    {
                        ptView->uiMemBase = 0;
                    }
                    gui_invalidate(hWnd);
                    break;
                }

                case GUI_KEY_PAGE_DOWN:
                {
                    st_u32_t uiStep = (st_u32_t)(iRowsPerPage
                                       * EXEC_MEM_BYTES_PER_ROW);
                    ptView->uiMemBase += uiStep;
                    if (ptView->uiMemBase >= (st_u32_t)ST_RAM_SIZE)
                    {
                        ptView->uiMemBase =
                            (st_u32_t)(ST_RAM_SIZE - EXEC_MEM_BYTES_PER_ROW);
                    }
                    gui_invalidate(hWnd);
                    break;
                }

                case GUI_KEY_HOME:
                    if (ptView->ptState != NULL)
                    {
                        platform_mutex_lock(ptView->ptState->ptMutex);
                        uiPC = ptView->ptState->tCpuSnap->uiPC;
                        platform_mutex_unlock(ptView->ptState->ptMutex);
                        ptView->uiMemBase = uiPC
                            & ~((st_u32_t)(EXEC_MEM_BYTES_PER_ROW - 1));
                        gui_invalidate(hWnd);
                    }
                    break;

                case GUI_KEY_ESCAPE:
                    gui_request_close(hWnd);
                    break;

                default:
                    break;
            }
            break;

        case GUI_EVT_CLOSE:
            if (ptView->hRenderer != NULL)
            {
                renderer_destroy(&ptView->hRenderer);
            }
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------
 * Open / close
 * ------------------------------------------------------------------ */

st_error_t exec_mem_open(exec_mem_view_t **pptView)
{
    exec_mem_view_t *ptView;
    gui_wnd_desc_t   tDesc;
    st_error_t       eResult;
    char             szTitle[ST_MAX_PATH + 64];
    exec_state_t    *ptState;

    LOG_TRACE("pptView=%p", (void *)pptView);

    if (pptView == NULL)
    {
        LOG_ERROR("NULL parameter: pptView");
        return ST_ERROR;
    }

    *pptView = NULL;
    ptState  = exec_get_state();
    if (ptState == NULL)
    {
        LOG_ERROR("exec_get_state() returned NULL");
        return ST_ERROR;
    }

    ptView = (exec_mem_view_t *)malloc(sizeof(exec_mem_view_t));
    if (ptView == NULL)
    {
        LOG_ERROR("malloc failed for exec_mem_view_t");
        return ST_ERROR;
    }
    memset(ptView, 0, sizeof(exec_mem_view_t));
    ptView->ptState = ptState;

    /* Set initial view position to PC */
    platform_mutex_lock(ptState->ptMutex);
    ptView->uiMemBase = ptState->tCpuSnap->uiPC
                        & ~((st_u32_t)(EXEC_MEM_BYTES_PER_ROW - 1));
    platform_mutex_unlock(ptState->ptMutex);

    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Memory: %s", ptState->szProgName);

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle   = szTitle;
    tDesc.eType     = GUI_WND_EXEC_MEM;
    tDesc.iWidth    = EXEC_MEM_WND_W;
    tDesc.iHeight   = EXEC_MEM_WND_H;
    tDesc.pfnEvent  = exec_mem_event_cb;
    tDesc.pUserCtx  = ptView;

    eResult = gui_open_window(&tDesc, &ptView->hWnd);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("gui_open_window failed for memory view");
        free(ptView);
        return ST_ERROR;
    }

    g_exec_mem_ptView = ptView;
    *pptView = ptView;
    LOG_INFO("memory dump view opened");
    return ST_NO_ERROR;
}

st_error_t exec_mem_close(exec_mem_view_t **pptView)
{
    LOG_TRACE("pptView=%p", (void *)pptView);

    if (pptView == NULL)
    {
        LOG_ERROR("NULL parameter: pptView");
        return ST_ERROR;
    }

    if (*pptView == NULL)
    {
        return ST_NO_ERROR;
    }

    if ((*pptView)->hWnd != NULL)
    {
        gui_close_window((*pptView)->hWnd);
    }

    free(*pptView);
    *pptView = NULL;
    g_exec_mem_ptView = NULL;

    LOG_INFO("memory dump view closed");
    return ST_NO_ERROR;
}
