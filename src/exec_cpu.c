/*
 * exec_cpu.c - ST4Ever CPU 68000 register view
 *
 * UC25A: displays CPU register file snapshot during execution.
 */

#include "exec_cpu.h"
#include "exec.h"
#include "trace.h"
#include "renderer.h"
#include "gui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Module global (one CPU view at a time)
 * ------------------------------------------------------------------ */

static exec_cpu_view_t *g_exec_cpu_ptView = NULL;

/* ------------------------------------------------------------------
 * Colours
 * ------------------------------------------------------------------ */

static const renderer_color_t g_cpu_clrBg     = RENDERER_COLOR_BLACK;
static const renderer_color_t g_cpu_clrWhite  = RENDERER_COLOR_WHITE;
static const renderer_color_t g_cpu_clrCyan   = RENDERER_COLOR_CYAN;
static const renderer_color_t g_cpu_clrGreen  = RENDERER_COLOR_GREEN;
static const renderer_color_t g_cpu_clrYellow = RENDERER_COLOR_YELLOW;
static const renderer_color_t g_cpu_clrGray   = RENDERER_COLOR_GRAY;

/* ------------------------------------------------------------------
 * Render
 * ------------------------------------------------------------------ */

static void exec_cpu_render(exec_cpu_view_t *ptView)
{
    exec_state_t      *ptState;
    cpu_context_t      tSnap;
    exec_run_state_t   eRunState;
    int                iCellH;
    int                iCellW;
    int                iY;
    char               szBuf[128];
    char               szBuf2[128];
    renderer_rect_t    tRect;
    renderer_rect_t    tFull;
    int                i;
    int                iFlag;

    ptState = ptView->ptState;
    if (ptState == NULL || ptView->hRenderer == NULL)
    {
        return;
    }

    /* --- Snapshot under mutex --- */
    platform_mutex_lock(ptState->ptMutex);
    memcpy(&tSnap, ptState->tCpuSnap, sizeof(cpu_context_t));
    eRunState = ptState->eRunState;
    platform_mutex_unlock(ptState->ptMutex);

    ST_UNUSED(eRunState);

    renderer_get_font_metrics(ptView->hRenderer,
                              RENDERER_FONT_MONO,
                              &iCellW, &iCellH);
    ptView->iCellW = iCellW;
    ptView->iCellH = iCellH;

    tFull = (renderer_rect_t)RENDERER_RECT(0, 0,
                                            ptView->iWndWidth,
                                            ptView->iWndHeight);
    renderer_begin_draw(ptView->hRenderer, &g_cpu_clrBg);
    renderer_fill_rect(ptView->hRenderer, &tFull, &g_cpu_clrBg);

    iY = iCellH / 2;

    /* --- D0..D7  /  A0..A7 --- */
    for (i = 0; i < 8; i++)
    {
        int iXLeft  = iCellW;
        int iXRight = iCellW + iCellW * 20;

        snprintf(szBuf,  sizeof(szBuf),  "D%d: $%08X", i, tSnap.auDn[i]);
        snprintf(szBuf2, sizeof(szBuf2), "A%d: $%08X", i, tSnap.auAn[i]);

        tRect = (renderer_rect_t)RENDERER_RECT(iXLeft, iY,
                                                iCellW * 16, iCellH);
        renderer_draw_text(ptView->hRenderer, szBuf,
                           &tRect, &g_cpu_clrCyan,
                           RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

        tRect = (renderer_rect_t)RENDERER_RECT(iXRight, iY,
                                                iCellW * 16, iCellH);
        renderer_draw_text(ptView->hRenderer, szBuf2,
                           &tRect, &g_cpu_clrGreen,
                           RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
        iY += iCellH;
    }

    /* --- Separator --- */
    iY += iCellH / 4;
    renderer_draw_line(ptView->hRenderer,
                       0.0f, (float)iY,
                       (float)ptView->iWndWidth, (float)iY,
                       &g_cpu_clrGray, 1.0f);
    iY += 4;

    /* --- SSP + PC --- */
    snprintf(szBuf,  sizeof(szBuf),  "SSP: $%08X", tSnap.uiSSP);
    snprintf(szBuf2, sizeof(szBuf2), "PC:  $%06X", tSnap.uiPC);

    tRect = (renderer_rect_t)RENDERER_RECT(iCellW, iY,
                                            iCellW * 18, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, &g_cpu_clrWhite,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

    tRect = (renderer_rect_t)RENDERER_RECT(iCellW + iCellW * 20, iY,
                                            iCellW * 16, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf2,
                       &tRect, &g_cpu_clrYellow,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    iY += iCellH;

    /* --- Separator --- */
    iY += iCellH / 4;
    renderer_draw_line(ptView->hRenderer,
                       0.0f, (float)iY,
                       (float)ptView->iWndWidth, (float)iY,
                       &g_cpu_clrGray, 1.0f);
    iY += 4;

    /* --- SR --- */
    snprintf(szBuf, sizeof(szBuf), " SR: $%04X", tSnap.uiSR);
    tRect = (renderer_rect_t)RENDERER_RECT(iCellW, iY,
                                            iCellW * 16, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, &g_cpu_clrWhite,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    iY += iCellH;

    /* --- Individual flags --- */
    iFlag = (tSnap.uiSR & CPU_SR_T) ? 1 : 0;
    snprintf(szBuf, sizeof(szBuf),
             " Flags: T=%d S=%d I=%d  X=%d N=%d Z=%d V=%d C=%d",
             iFlag,
             (tSnap.uiSR & CPU_SR_S)  ? 1 : 0,
             (int)((tSnap.uiSR & CPU_SR_I_MASK) >> 8),
             (tSnap.uiSR & CPU_SR_X)  ? 1 : 0,
             (tSnap.uiSR & CPU_SR_N)  ? 1 : 0,
             (tSnap.uiSR & CPU_SR_Z)  ? 1 : 0,
             (tSnap.uiSR & CPU_SR_V)  ? 1 : 0,
             (tSnap.uiSR & CPU_SR_C)  ? 1 : 0);
    tRect = (renderer_rect_t)RENDERER_RECT(iCellW, iY,
                                            ptView->iWndWidth - iCellW,
                                            iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, &g_cpu_clrWhite,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

    renderer_end_draw(ptView->hRenderer);
}

/* ------------------------------------------------------------------
 * Event callback (runs in window thread)
 * ------------------------------------------------------------------ */

static void exec_cpu_event_cb(gui_window_t  hWnd,
                               gui_event_t  *ptEvent,
                               void         *pUserCtx)
{
    exec_cpu_view_t *ptView;

    if (ptEvent == NULL || pUserCtx == NULL)
    {
        return;
    }

    ptView = (exec_cpu_view_t *)pUserCtx;

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
            exec_cpu_render(ptView);
            break;

        case GUI_EVT_KEY_DOWN:
            if (ptEvent->uData.tKey.eKey == GUI_KEY_ESCAPE)
            {
                exec_quit_request();
                gui_request_close(hWnd);
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

st_error_t exec_cpu_open(exec_cpu_view_t **pptView)
{
    exec_cpu_view_t *ptView;
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

    ptView = (exec_cpu_view_t *)malloc(sizeof(exec_cpu_view_t));
    if (ptView == NULL)
    {
        LOG_ERROR("malloc failed for exec_cpu_view_t");
        return ST_ERROR;
    }
    memset(ptView, 0, sizeof(exec_cpu_view_t));
    ptView->ptState = ptState;

    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - CPU: %s", ptState->szProgName);

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle   = szTitle;
    tDesc.eType     = GUI_WND_EXEC_CPU;
    tDesc.iWidth    = EXEC_CPU_WND_W;
    tDesc.iHeight   = EXEC_CPU_WND_H;
    tDesc.pfnEvent  = exec_cpu_event_cb;
    tDesc.pUserCtx  = ptView;

    eResult = gui_open_window(&tDesc, &ptView->hWnd);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("gui_open_window failed");
        free(ptView);
        return ST_ERROR;
    }

    g_exec_cpu_ptView = ptView;
    *pptView = ptView;
    LOG_INFO("CPU register view opened");
    return ST_NO_ERROR;
}

st_error_t exec_cpu_close(exec_cpu_view_t **pptView)
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
    g_exec_cpu_ptView = NULL;

    LOG_INFO("CPU register view closed");
    return ST_NO_ERROR;
}
