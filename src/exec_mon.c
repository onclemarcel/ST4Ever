/*
 * exec_mon.c - ST4Ever Execution monitor window
 *
 * UC25A: step/run/pause/stop/breakpoint controls + state display.
 */

#include "exec_mon.h"
#include "exec.h"
#include "trace.h"
#include "renderer.h"
#include "gui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Module global (one monitor view at a time)
 * ------------------------------------------------------------------ */

static exec_mon_view_t *g_exec_mon_ptView = NULL;

/* ------------------------------------------------------------------
 * Colours
 * ------------------------------------------------------------------ */

static const renderer_color_t g_mon_clrBg     = RENDERER_COLOR_BLACK;
static const renderer_color_t g_mon_clrWhite  = RENDERER_COLOR_WHITE;
static const renderer_color_t g_mon_clrGreen  = RENDERER_COLOR_GREEN;
static const renderer_color_t g_mon_clrYellow = RENDERER_COLOR_YELLOW;
static const renderer_color_t g_mon_clrRed    = RENDERER_COLOR_RED;
static const renderer_color_t g_mon_clrCyan   = RENDERER_COLOR_CYAN;
static const renderer_color_t g_mon_clrGray   = RENDERER_COLOR_GRAY;

/* ------------------------------------------------------------------
 * Render
 * ------------------------------------------------------------------ */

static void exec_mon_render(exec_mon_view_t *ptView)
{
    exec_state_t      *ptState;
    exec_run_state_t   eRunState;
    cpu_context_t      tSnap;
    cpu_step_result_t  tResult;
    char               szNextDisasm[DISASM_LINE_MAX];
    st_u32_t           auBP[EXEC_BP_MAX];
    int                iBPCount;
    int                iCellH;
    int                iCellW;
    int                iW;
    int                iH;
    int                iY;
    int                iX;
    char               szBuf[256];
    int                i;
    renderer_rect_t    tFull;
    renderer_rect_t    tRect;
    const renderer_color_t *pClrState;
    const char        *szStateName;

    ptState = ptView->ptState;
    if (ptState == NULL || ptView->hRenderer == NULL)
    {
        return;
    }

    /* --- Snapshot state under mutex --- */
    platform_mutex_lock(ptState->ptMutex);
    eRunState = ptState->eRunState;
    memcpy(&tSnap,   ptState->tCpuSnap,    sizeof(cpu_context_t));
    memcpy(&tResult, &ptState->tLastResult, sizeof(cpu_step_result_t));
    memcpy(auBP, ptState->auBP, sizeof(auBP));
    iBPCount = ptState->iBPCount;
    strncpy(szNextDisasm, ptState->szNextDisasm, DISASM_LINE_MAX - 1);
    szNextDisasm[DISASM_LINE_MAX - 1] = '\0';
    platform_mutex_unlock(ptState->ptMutex);

    ST_UNUSED(tResult);

    /* --- Font metrics --- */
    renderer_get_font_metrics(ptView->hRenderer,
                              RENDERER_FONT_MONO,
                              &iCellW, &iCellH);
    ptView->iCellW = iCellW;
    ptView->iCellH = iCellH;
    iW = ptView->iWndWidth;
    iH = ptView->iWndHeight;

    tFull = (renderer_rect_t)RENDERER_RECT(0, 0, iW, iH);
    renderer_begin_draw(ptView->hRenderer, &g_mon_clrBg);
    renderer_fill_rect(ptView->hRenderer, &tFull, &g_mon_clrBg);

    /* --- Header row: program name + state badge --- */
    switch (eRunState)
    {
        case EXEC_RUN_RUNNING: szStateName = "RUNNING"; pClrState = &g_mon_clrGreen;  break;
        case EXEC_RUN_HALTED:  szStateName = "HALTED";  pClrState = &g_mon_clrRed;    break;
        default:               szStateName = "PAUSED";  pClrState = &g_mon_clrYellow; break;
    }

    iX = iCellW;
    iY = iCellH / 2;

    /* Program name */
    snprintf(szBuf, sizeof(szBuf), "%s", ptState->szProgName);
    tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, &g_mon_clrWhite,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

    /* State badge on the right */
    snprintf(szBuf, sizeof(szBuf), "%s", szStateName);
    tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, pClrState,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_RIGHT);

    /* --- Separator after header --- */
    iY += iCellH + iCellH / 2;
    renderer_draw_line(ptView->hRenderer,
                       0.0f, (float)iY,
                       (float)iW, (float)iY,
                       &g_mon_clrGray, 1.0f);
    iY += 4;

    /* --- PC + next instruction --- */
    snprintf(szBuf, sizeof(szBuf), "  PC: $%06X", tSnap.uiPC);
    tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, &g_mon_clrCyan,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    iY += iCellH;

    if (szNextDisasm[0] != '\0')
    {
        snprintf(szBuf, sizeof(szBuf), "      %s", szNextDisasm);
    }
    else
    {
        snprintf(szBuf, sizeof(szBuf), "      (out of range)");
    }
    tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, &g_mon_clrGreen,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    iY += iCellH;

    /* --- Instruction and cycle counters --- */
    snprintf(szBuf, sizeof(szBuf),
             "  Instr: %llu   Cycles: %llu",
             (unsigned long long)tSnap.ulInstrCount,
             (unsigned long long)tSnap.ulCycleCount);
    tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, &g_mon_clrWhite,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    iY += iCellH + iCellH / 2;

    /* --- Separator before breakpoints --- */
    renderer_draw_line(ptView->hRenderer,
                       0.0f, (float)iY,
                       (float)iW, (float)iY,
                       &g_mon_clrGray, 1.0f);
    iY += 4;

    /* --- Breakpoints --- */
    snprintf(szBuf, sizeof(szBuf),
             "  Breakpoints (%d/%d):", iBPCount, EXEC_BP_MAX);
    tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
    renderer_draw_text(ptView->hRenderer, szBuf,
                       &tRect, &g_mon_clrWhite,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    iY += iCellH;

    if (iBPCount == 0)
    {
        tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
        renderer_draw_text(ptView->hRenderer, "    (none)",
                           &tRect, &g_mon_clrGray,
                           RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
        iY += iCellH;
    }
    else
    {
        for (i = 0; i < iBPCount; i++)
        {
            const renderer_color_t *pClr = &g_mon_clrCyan;
            if (auBP[i] == tSnap.uiPC)
            {
                snprintf(szBuf, sizeof(szBuf),
                         "    [%d] $%06X  <- PC", i, auBP[i]);
                pClr = &g_mon_clrYellow;
            }
            else
            {
                snprintf(szBuf, sizeof(szBuf),
                         "    [%d] $%06X", i, auBP[i]);
            }
            tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
            renderer_draw_text(ptView->hRenderer, szBuf,
                               &tRect, pClr,
                               RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
            iY += iCellH;
        }
    }

    /* --- Key hints at bottom --- */
    iY = iH - 2 * iCellH - iCellH / 2;
    renderer_draw_line(ptView->hRenderer,
                       0.0f, (float)iY,
                       (float)iW, (float)iY,
                       &g_mon_clrGray, 1.0f);
    iY += 4;

    tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
    renderer_draw_text(ptView->hRenderer,
                       "  [F5] Step  [F6] Run/Pause  "
                       "[F7] Reset  [F8/ESC] Quit",
                       &tRect, &g_mon_clrGray,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);
    iY += iCellH;

    tRect = (renderer_rect_t)RENDERER_RECT(iX, iY, iW - iX, iCellH);
    renderer_draw_text(ptView->hRenderer,
                       "  [B] Toggle BP at PC   "
                       "[C] Clear all BPs",
                       &tRect, &g_mon_clrGray,
                       RENDERER_FONT_MONO, RENDERER_ALIGN_LEFT);

    renderer_end_draw(ptView->hRenderer);
}

/* ------------------------------------------------------------------
 * Event callback (runs in window thread)
 * ------------------------------------------------------------------ */

static void exec_mon_event_cb(gui_window_t  hWnd,
                               gui_event_t  *ptEvent,
                               void         *pUserCtx)
{
    exec_mon_view_t   *ptView;
    exec_state_t      *ptState;
    exec_run_state_t   eRunState;
    char               cCh;

    if (ptEvent == NULL || pUserCtx == NULL)
    {
        return;
    }

    ptView  = (exec_mon_view_t *)pUserCtx;
    ptState = ptView->ptState;

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
            exec_mon_render(ptView);
            break;

        case GUI_EVT_KEY_DOWN:
            if (ptState == NULL)
            {
                break;
            }
            cCh = ptEvent->uData.tKey.cChar;
            switch (ptEvent->uData.tKey.eKey)
            {
                case GUI_KEY_F5:
                    exec_step_request();
                    break;

                case GUI_KEY_F6:
                    platform_mutex_lock(ptState->ptMutex);
                    eRunState = ptState->eRunState;
                    platform_mutex_unlock(ptState->ptMutex);
                    if (eRunState == EXEC_RUN_RUNNING)
                    {
                        exec_pause_request();
                    }
                    else
                    {
                        exec_run_request();
                    }
                    break;

                case GUI_KEY_F7:
                    exec_stop_request();
                    break;

                case GUI_KEY_F8:
                case GUI_KEY_ESCAPE:
                {
                    gui_window_t hCpuWnd;
                    exec_quit_request();
                    platform_mutex_lock(ptState->ptMutex);
                    hCpuWnd = ptState->hCpuWnd;
                    platform_mutex_unlock(ptState->ptMutex);
                    if (hCpuWnd != NULL)
                    {
                        gui_request_close(hCpuWnd);
                    }
                    gui_request_close(hWnd);
                    break;
                }

                default:
                    if (cCh == 'b' || cCh == 'B')
                    {
                        st_u32_t uiPC;
                        platform_mutex_lock(ptState->ptMutex);
                        uiPC = ptState->tCpuSnap->uiPC;
                        platform_mutex_unlock(ptState->ptMutex);
                        exec_bp_toggle(uiPC);
                        if (hWnd != NULL)
                        {
                            gui_invalidate(hWnd);
                        }
                    }
                    else if (cCh == 'c' || cCh == 'C')
                    {
                        exec_bp_clear();
                        if (hWnd != NULL)
                        {
                            gui_invalidate(hWnd);
                        }
                    }
                    break;
            }
            break;

        case GUI_EVT_CLOSE:
        {
            gui_window_t hCpuWnd;
            exec_quit_request();
            if (ptState != NULL)
            {
                platform_mutex_lock(ptState->ptMutex);
                hCpuWnd = ptState->hCpuWnd;
                platform_mutex_unlock(ptState->ptMutex);
                if (hCpuWnd != NULL)
                {
                    gui_request_close(hCpuWnd);
                }
            }
            if (ptView->hRenderer != NULL)
            {
                renderer_destroy(&ptView->hRenderer);
            }
            break;
        }

        default:
            break;
    }
}

/* ------------------------------------------------------------------
 * Open / close
 * ------------------------------------------------------------------ */

st_error_t exec_mon_open(exec_mon_view_t **pptView)
{
    exec_mon_view_t *ptView;
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

    ptView = (exec_mon_view_t *)malloc(sizeof(exec_mon_view_t));
    if (ptView == NULL)
    {
        LOG_ERROR("malloc failed for exec_mon_view_t");
        return ST_ERROR;
    }
    memset(ptView, 0, sizeof(exec_mon_view_t));
    ptView->ptState = ptState;

    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Monitor: %s", ptState->szProgName);

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle   = szTitle;
    tDesc.eType     = GUI_WND_EXEC_MON;
    tDesc.iWidth    = EXEC_MON_WND_W;
    tDesc.iHeight   = EXEC_MON_WND_H;
    tDesc.pfnEvent  = exec_mon_event_cb;
    tDesc.pUserCtx  = ptView;

    eResult = gui_open_window(&tDesc, &ptView->hWnd);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("gui_open_window failed");
        free(ptView);
        return ST_ERROR;
    }

    g_exec_mon_ptView = ptView;
    *pptView = ptView;
    LOG_INFO("execution monitor opened");
    return ST_NO_ERROR;
}

st_error_t exec_mon_close(exec_mon_view_t **pptView)
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
    g_exec_mon_ptView = NULL;

    LOG_INFO("execution monitor closed");
    return ST_NO_ERROR;
}
