/*
 * exec_screen.c - ST4Ever Atari ST screen emulation view
 *
 * UC27: displays the Shifter video output during execution.
 */

#include "exec_screen.h"
#include "exec.h"
#include "shifter.h"
#include "trace.h"
#include "renderer.h"
#include "gui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Module global (one screen view at a time)
 * ------------------------------------------------------------------ */

static exec_screen_view_t *g_exec_scr_ptView = NULL;

/* ------------------------------------------------------------------
 * Colours
 * ------------------------------------------------------------------ */

static const renderer_color_t g_scr_clrBg   = RENDERER_COLOR_BLACK;
static const renderer_color_t g_scr_clrGray = RENDERER_COLOR_GRAY;

/* ------------------------------------------------------------------
 * Render
 * ------------------------------------------------------------------ */

static void exec_screen_render(exec_screen_view_t *ptView)
{
    exec_state_t     *ptState;
    exec_run_state_t  eRunState;
    renderer_rect_t   tFull;
    renderer_rect_t   tDest;
    st_error_t        eResult;
    int               iSrcW;
    int               iSrcH;
    int               iWndW;
    int               iWndH;
    int               iDispW;
    int               iDispH;
    int               iOffX;
    int               iOffY;
    float             fScaleX;
    float             fScaleY;
    float             fScale;
    renderer_rect_t   tIdleRect;
    const char       *szIdle;

    ptState = ptView->ptState;
    if (ptState == NULL || ptView->hRenderer == NULL)
    {
        return;
    }

    /* Brief lock to read run state */
    platform_mutex_lock(ptState->ptMutex);
    eRunState = ptState->eRunState;
    platform_mutex_unlock(ptState->ptMutex);

    iWndW = ptView->iWndWidth;
    iWndH = ptView->iWndHeight;
    if (iWndW <= 0 || iWndH <= 0)
    {
        return;
    }

    tFull = (renderer_rect_t)RENDERER_RECT(0, 0, iWndW, iWndH);
    renderer_begin_draw(ptView->hRenderer, &g_scr_clrBg);
    renderer_fill_rect(ptView->hRenderer, &tFull, &g_scr_clrBg);

    /* Render Shifter frame — reads ptMachine directly (no mutex).
     * Race with exec thread is benign: worst case = slightly torn frame. */
    iSrcW   = 0;
    iSrcH   = 0;
    eResult = shifter_render(ptView->auPixels,
                             SHIFTER_MAX_PIXELS,
                             &iSrcW,
                             &iSrcH);

    if (eResult == ST_NO_ERROR && iSrcW > 0 && iSrcH > 0)
    {
        /* Aspect-ratio preserving scale to fit window */
        fScaleX = (float)iWndW / (float)iSrcW;
        fScaleY = (float)iWndH / (float)iSrcH;
        fScale  = (fScaleX < fScaleY) ? fScaleX : fScaleY;
        iDispW  = (int)((float)iSrcW * fScale);
        iDispH  = (int)((float)iSrcH * fScale);
        iOffX   = (iWndW - iDispW) / 2;
        iOffY   = (iWndH - iDispH) / 2;

        tDest = (renderer_rect_t)RENDERER_RECT(iOffX, iOffY,
                                               iDispW, iDispH);
        renderer_draw_bitmap(ptView->hRenderer,
                             (const st_u8_t *)ptView->auPixels,
                             iSrcW, iSrcH,
                             &tDest);
    }
    else
    {
        /* Session idle or machine not initialised: show hint text */
        szIdle    = (eRunState == EXEC_RUN_IDLE) ? "IDLE" : "HALTED";
        tIdleRect = (renderer_rect_t)RENDERER_RECT(0, iWndH / 2 - 10,
                                                   iWndW, 20);
        renderer_draw_text(ptView->hRenderer,
                           szIdle,
                           &tIdleRect,
                           &g_scr_clrGray,
                           RENDERER_FONT_UI,
                           RENDERER_ALIGN_CENTER);
    }

    /* Overlay resolution info in bottom-right corner */
    if (eResult == ST_NO_ERROR && iSrcW > 0)
    {
        char            szInfo[32];
        renderer_rect_t tInfoRect;

        snprintf(szInfo, sizeof(szInfo), "%dx%d", iSrcW, iSrcH);
        tInfoRect = (renderer_rect_t)RENDERER_RECT(iWndW - 80, iWndH - 18,
                                                   76, 16);
        renderer_draw_text(ptView->hRenderer,
                           szInfo,
                           &tInfoRect,
                           &g_scr_clrGray,
                           RENDERER_FONT_UI,
                           RENDERER_ALIGN_RIGHT);
    }

    renderer_end_draw(ptView->hRenderer);
}

/* ------------------------------------------------------------------
 * Event callback
 * ------------------------------------------------------------------ */

static void exec_screen_event_cb(gui_window_t    hWnd,
                                  gui_event_t    *ptEvent,
                                  void           *pUserCtx)
{
    exec_screen_view_t *ptView;
    char                szTitle[ST_MAX_PATH + 64];

    ST_UNUSED(hWnd);

    ptView = (exec_screen_view_t *)pUserCtx;
    if (ptView == NULL || ptEvent == NULL)
    {
        return;
    }

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
                /* Update title with resolution */
                if (ptView->ptState != NULL)
                {
                    snprintf(szTitle, sizeof(szTitle),
                             "ST4Ever - Screen: %s [%dx%d]",
                             ptView->ptState->szProgName,
                             EXEC_SCR_WND_W, EXEC_SCR_WND_H);
                    gui_set_title(hWnd, szTitle);
                }
            }
            exec_screen_render(ptView);
            break;

        case GUI_EVT_KEY_DOWN:
            /* ESC closes the screen view */
            if (ptEvent->uData.tKey.cChar == 27)
            {
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

st_error_t exec_screen_open(exec_screen_view_t **pptView)
{
    exec_screen_view_t *ptView;
    exec_state_t       *ptState;
    gui_wnd_desc_t      tDesc;
    st_error_t          eResult;
    char                szTitle[ST_MAX_PATH + 64];

    LOG_TRACE("pptView=%p", (void *)pptView);

    if (pptView == NULL)
    {
        LOG_ERROR("NULL pptView");
        return ST_ERROR;
    }
    *pptView = NULL;

    ptState = exec_get_state();
    if (ptState == NULL)
    {
        LOG_ERROR("exec_get_state() returned NULL — exec_open not called");
        return ST_ERROR;
    }

    /* malloc: ~1 MB for auPixels inside the struct */
    ptView = (exec_screen_view_t *)calloc(1, sizeof(exec_screen_view_t));
    if (ptView == NULL)
    {
        LOG_ERROR("malloc failed for exec_screen_view_t");
        return ST_ERROR;
    }

    ptView->ptState    = ptState;
    ptView->iWndWidth  = EXEC_SCR_WND_W;
    ptView->iWndHeight = EXEC_SCR_WND_H;

    snprintf(szTitle, sizeof(szTitle),
             "ST4Ever - Screen: %s", ptState->szProgName);

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle   = szTitle;
    tDesc.eType     = GUI_WND_EXEC_SCR;
    tDesc.iWidth    = EXEC_SCR_WND_W;
    tDesc.iHeight   = EXEC_SCR_WND_H;
    tDesc.pfnEvent  = exec_screen_event_cb;
    tDesc.pUserCtx  = ptView;

    eResult = gui_open_window(&tDesc, &ptView->hWnd);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("gui_open_window failed");
        free(ptView);
        return ST_ERROR;
    }

    g_exec_scr_ptView = ptView;
    *pptView          = ptView;
    LOG_INFO("screen view opened: %s", ptState->szProgName);
    return ST_NO_ERROR;
}

st_error_t exec_screen_close(exec_screen_view_t **pptView)
{
    exec_screen_view_t *ptView;

    LOG_TRACE("pptView=%p", (void *)pptView);

    if (pptView == NULL)
    {
        LOG_ERROR("NULL pptView");
        return ST_ERROR;
    }

    ptView = *pptView;
    if (ptView == NULL)
    {
        return ST_NO_ERROR;
    }

    if (ptView->hWnd != NULL)
    {
        gui_close_window(ptView->hWnd);
    }

    free(ptView);
    *pptView = NULL;

    if (g_exec_scr_ptView == ptView)
    {
        g_exec_scr_ptView = NULL;
    }

    LOG_INFO("screen view closed");
    return ST_NO_ERROR;
}
