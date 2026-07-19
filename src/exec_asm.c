/*
 * exec_asm.c - ST4Ever Disassembly view during execution
 *
 * UC25B: forward disassembly from uiAsmBase using disasm_one(),
 * PC line highlighted yellow, UP/DOWN/PgUp/PgDn navigation,
 * F key snaps to PC.
 */

#include "exec_asm.h"
#include "exec.h"
#include "disassemble.h"
#include "trace.h"
#include "renderer.h"
#include "gui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Module global (one asm view at a time)
 * ------------------------------------------------------------------ */

static exec_asm_view_t *g_exec_asm_ptView = NULL;

/* ------------------------------------------------------------------
 * Colours
 * ------------------------------------------------------------------ */

static const renderer_color_t g_asm_clrBg    = RENDERER_COLOR_BLACK;
static const renderer_color_t g_asm_clrWhite = RENDERER_COLOR_WHITE;
static const renderer_color_t g_asm_clrGray  = RENDERER_COLOR_GRAY;
static const renderer_color_t g_asm_clrCyan  = RENDERER_COLOR_CYAN;
static const renderer_color_t g_asm_clrYellow = RENDERER_COLOR_YELLOW;
static const renderer_color_t g_asm_clrGreen  = RENDERER_COLOR_GREEN;
static const renderer_color_t g_asm_clrPCBg  =
    { 0.18f, 0.18f, 0.00f, 1.0f };

#define HINT_TEXT  \
    "[UP/DN] insn  [PgUp/PgDn] page  [F] snap to PC  [ESC] close"

/* ------------------------------------------------------------------
 * Find the instruction start that lands exactly at uiTarget.
 * Tries offsets -2, -4, -6, -8, -10 from uiTarget.
 * Returns the candidate address, or (uiTarget - 2) on failure.
 * ------------------------------------------------------------------ */

static st_u32_t exec_asm_prev_insn(const st_u8_t *pRam,
                                    st_u32_t       uiRamSize,
                                    st_u32_t       uiTarget)
{
    disasm_result_t tResult;
    st_u32_t        uiTry;
    st_u32_t        uiEnd;
    int             iDelta;

    for (iDelta = 2; iDelta <= 10; iDelta += 2)
    {
        if (uiTarget < (st_u32_t)iDelta) { break; }
        uiTry = uiTarget - (st_u32_t)iDelta;
        if (uiTry >= uiRamSize)         { continue; }

        memset(&tResult, 0, sizeof(tResult));
        disasm_one(pRam + uiTry,
                   (size_t)(uiRamSize - (size_t)uiTry),
                   uiTry, &tResult);
        uiEnd = uiTry + (st_u32_t)(tResult.iWordCount * 2);
        if (uiEnd == uiTarget)
        {
            return uiTry;
        }
    }

    return (uiTarget >= 2u) ? (uiTarget - 2u) : 0u;
}

/* ------------------------------------------------------------------
 * Build display line: "$XXXXXX  XXXX [XXXX]  text"
 * Returns next instruction address.
 * ------------------------------------------------------------------ */

static st_u32_t exec_asm_build_line(char           *szOut,
                                     int             iOutSize,
                                     st_u32_t        uiAddr,
                                     const st_u8_t  *pRam,
                                     st_u32_t        uiRamSize,
                                     disasm_result_t *ptResult)
{
    int      iOff;
    int      iRemain;
    int      iWord;

    memset(ptResult, 0, sizeof(*ptResult));

    if (uiAddr >= uiRamSize)
    {
        snprintf(szOut, (size_t)iOutSize, "$%06X  ----", uiAddr);
        ptResult->iWordCount = 1;
        return uiAddr + 2u;
    }

    disasm_one(pRam + uiAddr,
               (size_t)(uiRamSize - (size_t)uiAddr),
               uiAddr, ptResult);

    /* Address */
    iOff = snprintf(szOut, (size_t)iOutSize, "$%06X", uiAddr);

    /* Opcode words (max 3 displayed) */
    for (iWord = 0; iWord < ptResult->iWordCount && iWord < 3; iWord++)
    {
        iRemain = iOutSize - iOff;
        if (iRemain <= 0) { break; }
        iOff += snprintf(szOut + iOff, (size_t)iRemain,
                         " %04X", ptResult->auWords[iWord]);
    }

    /* Pad to fixed width (address=7 + up to 3 words×5 = 22 chars) */
    while (iOff < 22)
    {
        iRemain = iOutSize - iOff;
        if (iRemain <= 0) { break; }
        szOut[iOff++] = ' ';
    }

    /* Mnemonic + operands from szLine */
    iRemain = iOutSize - iOff;
    if (iRemain > 1)
    {
        snprintf(szOut + iOff, (size_t)iRemain, "  %s", ptResult->szLine);
    }

    return uiAddr + (st_u32_t)(ptResult->iWordCount * 2);
}

/* ------------------------------------------------------------------
 * Render
 * ------------------------------------------------------------------ */

static void exec_asm_render(exec_asm_view_t *ptView)
{
    exec_state_t          *ptState;
    st_u32_t               uiPC;
    st_u32_t               uiAddr;
    st_u32_t               uiBase;
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
    disasm_result_t        tResult;
    st_bool_t              bPCVisible;
    st_u32_t               uiNextAddr;

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

    renderer_begin_draw(ptView->hRenderer, &g_asm_clrBg);
    renderer_fill_rect(ptView->hRenderer, &tFull, &g_asm_clrBg);

    /* Snapshot PC under mutex */
    platform_mutex_lock(ptState->ptMutex);
    uiPC = ptState->tCpuSnap->uiPC;
    platform_mutex_unlock(ptState->ptMutex);

    uiRamSize = (st_u32_t)ST_RAM_SIZE;
    uiBase    = ptView->uiAsmBase;
    iMarginX  = iCellW;

    if (iCellH <= 0) { iCellH = 16; }
    iRowsVisible = (iH - EXEC_ASM_HINT_H) / iCellH;
    if (iRowsVisible < 1) { iRowsVisible = 1; }

    /* Check if PC is visible in current range (forward scan) */
    bPCVisible = ST_FALSE;
    {
        st_u32_t uiScan = uiBase;
        int      i;
        for (i = 0; i < iRowsVisible; i++)
        {
            if (uiScan == uiPC)
            {
                bPCVisible = ST_TRUE;
                break;
            }
            if (uiScan >= uiRamSize) { break; }
            memset(&tResult, 0, sizeof(tResult));
            disasm_one(ptState->ptMachine->aRam + uiScan,
                       (size_t)(uiRamSize - (size_t)uiScan),
                       uiScan, &tResult);
            uiScan += (st_u32_t)(tResult.iWordCount * 2);
        }
    }

    /* Auto-snap if PC is not visible */
    if (!bPCVisible)
    {
        uiBase = uiPC;
        ptView->uiAsmBase = uiBase;
    }

    /* Render instructions forward from uiBase */
    iY    = 0;
    uiAddr = uiBase;

    for (iRow = 0; iRow < iRowsVisible; iRow++)
    {
        if (uiAddr >= uiRamSize) { break; }

        uiNextAddr = exec_asm_build_line(szLine, (int)sizeof(szLine),
                                         uiAddr,
                                         ptState->ptMachine->aRam,
                                         uiRamSize, &tResult);

        if (uiAddr == uiPC)
        {
            tRect   = (renderer_rect_t)RENDERER_RECT(0, iY, iW, iCellH);
            renderer_fill_rect(ptView->hRenderer, &tRect, &g_asm_clrPCBg);
            pclrTxt = &g_asm_clrYellow;
        }
        else if (tResult.bValid)
        {
            pclrTxt = &g_asm_clrWhite;
        }
        else
        {
            pclrTxt = &g_asm_clrGreen;  /* DC.W fallback in green */
        }

        tRect = (renderer_rect_t)RENDERER_RECT(iMarginX, iY,
                                                iW - iMarginX, iCellH);
        renderer_draw_text(ptView->hRenderer, szLine,
                           &tRect, pclrTxt,
                           RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
        iY    += iCellH;
        uiAddr = uiNextAddr;
    }

    /* Hint bar */
    renderer_draw_line(ptView->hRenderer,
                       0.0f, (float)(iH - EXEC_ASM_HINT_H),
                       (float)iW, (float)(iH - EXEC_ASM_HINT_H),
                       &g_asm_clrGray, 1.0f);

    tRect = (renderer_rect_t)RENDERER_RECT(iMarginX,
                                            iH - EXEC_ASM_HINT_H + 2,
                                            iW - iMarginX * 2,
                                            EXEC_ASM_HINT_H - 2);
    renderer_draw_text(ptView->hRenderer, HINT_TEXT,
                       &tRect, &g_asm_clrCyan,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

    renderer_end_draw(ptView->hRenderer);
}

/* ------------------------------------------------------------------
 * Event callback (runs in window thread)
 * ------------------------------------------------------------------ */

static void exec_asm_event_cb(gui_window_t  hWnd,
                               gui_event_t  *ptEvent,
                               void         *pUserCtx)
{
    exec_asm_view_t  *ptView;
    exec_state_t     *ptState;
    char              cCh;
    int               iCellH;
    int               iRowsPerPage;
    disasm_result_t   tResult;
    st_u32_t          uiPC;
    int               i;

    if (ptEvent == NULL || pUserCtx == NULL)
    {
        return;
    }

    ptView  = (exec_asm_view_t *)pUserCtx;
    ptState = ptView->ptState;

    switch (ptEvent->eType)
    {
        case GUI_EVT_RESIZE:
            gui_handle_resize_event(hWnd, ptEvent,
                                     &ptView->iWndWidth, &ptView->iWndHeight,
                                     ptView->hRenderer);
            break;

        case GUI_EVT_PAINT:
            if (ptView->hRenderer == NULL)
            {
                renderer_create(hWnd, &ptView->hRenderer);
                gui_get_size(hWnd,
                             &ptView->iWndWidth,
                             &ptView->iWndHeight);
            }
            exec_asm_render(ptView);
            break;

        case GUI_EVT_KEY_DOWN:
            if (ptState == NULL)
            {
                break;
            }

            iCellH = (ptView->iCellH > 0) ? ptView->iCellH : 16;
            iRowsPerPage = (ptView->iWndHeight - EXEC_ASM_HINT_H)
                           / iCellH;
            if (iRowsPerPage < 1) { iRowsPerPage = 1; }

            cCh = ptEvent->uData.tKey.cChar;

            switch (ptEvent->uData.tKey.eKey)
            {
                case GUI_KEY_UP:
                    /* Find instruction that ends at uiAsmBase */
                    ptView->uiAsmBase =
                        exec_asm_prev_insn(
                            ptState->ptMachine->aRam,
                            (st_u32_t)ST_RAM_SIZE,
                            ptView->uiAsmBase);
                    gui_invalidate(hWnd);
                    break;

                case GUI_KEY_DOWN:
                    /* Advance by one instruction */
                    if (ptView->uiAsmBase < (st_u32_t)ST_RAM_SIZE)
                    {
                        memset(&tResult, 0, sizeof(tResult));
                        disasm_one(
                            ptState->ptMachine->aRam
                                + ptView->uiAsmBase,
                            (size_t)(ST_RAM_SIZE
                                     - (size_t)ptView->uiAsmBase),
                            ptView->uiAsmBase, &tResult);
                        ptView->uiAsmBase +=
                            (st_u32_t)(tResult.iWordCount * 2);
                    }
                    gui_invalidate(hWnd);
                    break;

                case GUI_KEY_PAGE_UP:
                    for (i = 0; i < iRowsPerPage; i++)
                    {
                        ptView->uiAsmBase =
                            exec_asm_prev_insn(
                                ptState->ptMachine->aRam,
                                (st_u32_t)ST_RAM_SIZE,
                                ptView->uiAsmBase);
                    }
                    gui_invalidate(hWnd);
                    break;

                case GUI_KEY_PAGE_DOWN:
                    for (i = 0; i < iRowsPerPage; i++)
                    {
                        if (ptView->uiAsmBase >= (st_u32_t)ST_RAM_SIZE)
                        {
                            break;
                        }
                        memset(&tResult, 0, sizeof(tResult));
                        disasm_one(
                            ptState->ptMachine->aRam
                                + ptView->uiAsmBase,
                            (size_t)(ST_RAM_SIZE
                                     - (size_t)ptView->uiAsmBase),
                            ptView->uiAsmBase, &tResult);
                        ptView->uiAsmBase +=
                            (st_u32_t)(tResult.iWordCount * 2);
                    }
                    gui_invalidate(hWnd);
                    break;

                case GUI_KEY_ESCAPE:
                    gui_request_close(hWnd);
                    break;

                default:
                    /* 'f'/'F' key: snap to PC */
                    if (cCh == 'f' || cCh == 'F')
                    {
                        platform_mutex_lock(ptState->ptMutex);
                        uiPC = ptState->tCpuSnap->uiPC;
                        platform_mutex_unlock(ptState->ptMutex);
                        ptView->uiAsmBase = uiPC;
                        gui_invalidate(hWnd);
                    }
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

st_error_t exec_asm_open(exec_asm_view_t **pptView)
{
    exec_asm_view_t *ptView;
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

    ptView = (exec_asm_view_t *)malloc(sizeof(exec_asm_view_t));
    if (ptView == NULL)
    {
        LOG_ERROR("malloc failed for exec_asm_view_t");
        return ST_ERROR;
    }
    memset(ptView, 0, sizeof(exec_asm_view_t));
    ptView->ptState = ptState;

    /* Set initial view position to PC */
    platform_mutex_lock(ptState->ptMutex);
    ptView->uiAsmBase = ptState->tCpuSnap->uiPC;
    platform_mutex_unlock(ptState->ptMutex);

    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Disasm: %s", ptState->szProgName);

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle   = szTitle;
    tDesc.eType     = GUI_WND_EXEC_ASM;
    tDesc.iWidth    = EXEC_ASM_WND_W;
    tDesc.iHeight   = EXEC_ASM_WND_H;
    tDesc.pfnEvent  = exec_asm_event_cb;
    tDesc.pUserCtx  = ptView;

    eResult = gui_open_window(&tDesc, &ptView->hWnd);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("gui_open_window failed for disassembly view");
        free(ptView);
        return ST_ERROR;
    }

    g_exec_asm_ptView = ptView;
    *pptView = ptView;
    LOG_INFO("disassembly view opened");
    return ST_NO_ERROR;
}

st_error_t exec_asm_close(exec_asm_view_t **pptView)
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
    g_exec_asm_ptView = NULL;

    LOG_INFO("disassembly view closed");
    return ST_NO_ERROR;
}
