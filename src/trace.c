/*
 * trace.c - ST4Ever trace and debug console implementation
 *
 * UC4.4: Added GUI_WND_TRACE window (D2D append-only scroll view).
 *   - trace_gui_open()  : opens GUI window + starts appending entries.
 *   - trace_close() : closes GUI window.
 *   - trace_log()   : writes to log file + GUI ring buffer (no stderr).
 *
 * Threading: trace_log() is called from any thread.
 *   The ring buffer (aLines/iHead/iCount) is protected by ptMutex.
 *   gui_invalidate() is called with a re-entrancy guard to prevent
 *   infinite recursion via LOG_TRACE inside gui_invalidate().
 */

#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>



/* D2D colour palette for the trace window */
static const renderer_color_t g_tv_clrBg    = {0.09f, 0.09f, 0.14f, 1.0f};
static const renderer_color_t g_tv_clrTrace = {0.45f, 0.45f, 0.45f, 1.0f};
static const renderer_color_t g_tv_clrInfo  = {0.00f, 0.85f, 0.85f, 1.0f};
static const renderer_color_t g_tv_clrError = {0.95f, 0.10f, 0.10f, 1.0f};
static const renderer_color_t g_tv_clrTodo  = {0.85f, 0.00f, 0.85f, 1.0f};
static const renderer_color_t g_tv_clrSel   = {0.20f, 0.20f, 0.35f, 1.0f};

/* ------------------------------------------------------------------
 * Module global context
 * ------------------------------------------------------------------ */

 static trace_context_t g_trace_ptCtx = {.ulMagic = 0xCAFEDECA, 
                                         .eObject = ST_TRACE_CTX};

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */
/*
 * trace_get_timestamp() - stamp incoming buffer with time
 *
 * Parameters:
 *   szBuf    [in] : Pointer to string
 *   uiBufLen [in] : length of string
 *
 * Returns:
 *   ST_ERROR if an error is detected
 *   ST_NO_ERROR on success
 */
static st_error_t trace_get_timestamp(char *szBuf, size_t uiBufLen)
{
    time_t     tNow;
    struct tm *pTm;

    // No LOG_TRACE - as we are currently writing a log message

    /* -- [TRACE]17. Reject NULL buffer or zero-length buffer -- */
    if (szBuf == NULL || uiBufLen == 0)
    {
        return ST_ERROR;
    }

    tNow = time(NULL);
    pTm  = localtime(&tNow);

    /* -- [TRACE]28. Add timestamp ??:??:?? if glibc does not work -- */
    if (pTm == NULL)
    {
        strncpy(szBuf, "??:??:??", uiBufLen - 1);
        szBuf[uiBufLen - 1] = '\0';
        return ST_ERROR;
    }

    /* -- [TRACE]18. Add timestamp %H:%M:%S at start of buffer -- */
    strftime(szBuf, uiBufLen, "%H:%M:%S", pTm);
    return ST_NO_ERROR;
}

/*
 * trace_level_label() - Set the tag according to the incoming level
 *
 * Parameters:
 *   eLevel   [in] : Type of logging message
 *
 * Returns:
 *   A 4-characters string representing the tag
 */
static const char *trace_level_label(log_level_t eLevel)
{
    // No LOG_TRACE - as we are currently writing a log message
    
    /* -- [TRACE]29. Tag logging message according to its type -- */
    switch (eLevel)
    {
        case LOG_LEVEL_TRACE: return "TRC ";
        case LOG_LEVEL_INFO:  return "INF ";
        case LOG_LEVEL_ERROR: return "ERR ";
        case LOG_LEVEL_TODO:  return "TODO";
        default:              return "??? ";
    }
}


/*
 * trace_level_color() - Set the text color according to the incoming level
 *
 * Parameters:
 *   eLevel   [in] : Type of logging message
 *
 * Returns:
 *   A rendering color of type renderer_color_t (RGB & Alpha format)
 */
static const renderer_color_t *trace_level_color(log_level_t eLevel)
{
    // No LOG_TRACE - as we are currently writing a log message

    /* -- [TRACE]30. Set message color according to its type -- */
    switch (eLevel)
    {
        case LOG_LEVEL_TRACE: return &g_tv_clrTrace;
        case LOG_LEVEL_INFO:  return &g_tv_clrInfo;
        case LOG_LEVEL_ERROR: return &g_tv_clrError;
        case LOG_LEVEL_TODO:  return &g_tv_clrTodo;
        default:              return &g_tv_clrInfo;
    }
}

/*
 * trace_flush_compact() - Write the compaction logging message
 *
 * Parameters:
 *   void   
 *
 * Returns:
 *   ST_ERROR in case of detected error
 *   ST_NO_ERROR in case of success
 */
static st_error_t trace_flush_compact(void)
{
    char szTime[16];
    st_error_t eR;

    // No LOG_TRACE - as we are currently writing a log message

    /* -- [TRACE]31. If no need for compaction, return with no error -- */
    if (g_trace_ptCtx.iCompactCount <= 1)
    {
        g_trace_ptCtx.iCompactCount = 0;
        g_trace_ptCtx.szLastFunc[0] = '\0';
        return ST_NO_ERROR;
    }

    /* -- [TRACE]32. Send the compaction message in trace file -- */
    eR = trace_get_timestamp(szTime, sizeof(szTime));

    if (g_trace_ptCtx.pFile != NULL)
    {
        fprintf(g_trace_ptCtx.pFile,
                "%s [%s] %s [x%d]\n",
                szTime,
                trace_level_label(LOG_LEVEL_TRACE),
                g_trace_ptCtx.szLastFunc,
                g_trace_ptCtx.iCompactCount);
        fflush(g_trace_ptCtx.pFile);
    }
    else
    {
        eR = ST_ERROR;
    }

    g_trace_ptCtx.iCompactCount = 0;
    g_trace_ptCtx.szLastFunc[0] = '\0';

    return eR;
}

/* ------------------------------------------------------------------
 * GUI trace view — ring buffer append (caller holds ptMutex)
 * ------------------------------------------------------------------ */
/*
 * trace_view_append_locked() - Add new lines in GUI view and apply
 *                              an auto-scroll strategy to keep new 
 *                              line visible
 *
 * Parameters:
 *   ptView [in] : the pointer to the GUI view to be updated
 *   eLevel [in] : the log message level when this line is written
 *   szLine [in] : the line itself
 *
 * Returns:
 *   ST_ERROR in case of detected error
 *   ST_NO_ERROR in case of success
 */
static st_error_t trace_view_append_locked(trace_view_t *ptView,
                                      log_level_t  eLevel,
                                      const char  *szLine)
{
    int iInsert;

    // No LOG_TRACE - as we are currently writing a log message in GUI

    /* -- [TRACE]33. Returns an error if NULL parameter is received -- */
    if (ptView == NULL || szLine == NULL)
    {
        return ST_ERROR;
    }

    /* -- [TRACE]34. Manage the ring buffer of trace lines -- */
    if (ptView->iCount < TRACE_VIEW_MAX_LINES)
    {
        /* Buffer not yet full: append after tail */
        iInsert = (ptView->iHead + ptView->iCount) % TRACE_VIEW_MAX_LINES;
        ptView->iCount++;
    }
    else
    {
        /* Buffer full: overwrite oldest (advance head) */
        iInsert = ptView->iHead;
        ptView->iHead = (ptView->iHead + 1) % TRACE_VIEW_MAX_LINES;
        /* Keep scroll offset in sync: oldest line was scrolled away */
        if (ptView->iScrollOff > 0)
        {
            ptView->iScrollOff--;
        }
    }

    /* -- [TRACE]35. Insert the new line given in input -- */
    strncpy(ptView->aLines[iInsert].szText, szLine,
            TRACE_VIEW_LINE_LEN - 1);
    ptView->aLines[iInsert].szText[TRACE_VIEW_LINE_LEN - 1] = '\0';
    ptView->aLines[iInsert].eLevel = eLevel;

    /* -- [TRACE]36. Apply auto-scroll strategy for new line visiblity -- */
    if (ptView->bAutoScroll == ST_TRUE)
    {
        int iVisRows;
        iVisRows = (ptView->iCellH > 0)
                 ? (ptView->iWndHeight / ptView->iCellH)
                 : 25;
        if (iVisRows < 1) iVisRows = 1;
        ptView->iScrollOff = ptView->iCount - iVisRows;
        if (ptView->iScrollOff < 0) ptView->iScrollOff = 0;
    }

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * GUI trace view — rendering
 * ------------------------------------------------------------------ */
/*
 * trace_render() - Trace GUI rendering function
 *
 * Parameters:
 *   ptView [in] : the pointer to the GUI view to be updated
 *
 * Returns:
 *   void
 */
static void trace_render(trace_view_t *ptView)
{
    renderer_color_t  tBg;
    renderer_rect_t   tFull;
    renderer_rect_t   tLine;
    int               iVisRows;
    int               iRow;
    int               iLineIdx;
    int               iAbsIdx;
    float             fY;
    char              szText[TRACE_VIEW_LINE_LEN];
    log_level_t       eLevel;
    int               iSnapshot;
    int               iHeadSnapshot;

    // No LOG_TRACE - R22: called on every GUI_EVT_PAINT repaint

    /* -- [TRACE]38. Ignore paint request with no view or no D2D renderer -- */
    if (ptView == NULL || ptView->hRenderer == NULL)
    {
        return;
    }

    tBg   = g_tv_clrBg;
    tFull = (renderer_rect_t)RENDERER_RECT(0, 0,
                                            ptView->iWndWidth,
                                            ptView->iWndHeight);

    renderer_begin_draw(ptView->hRenderer, &tBg);
    renderer_fill_rect(ptView->hRenderer, &tFull, &tBg);

    if (ptView->iCellH <= 0)
    {
        renderer_end_draw(ptView->hRenderer);
        return;
    }

    /* -- [TRACE]39. Compute visible row count from cell height, with a
     *               sane fallback when no font metrics are available -- */
    iVisRows = ptView->iWndHeight / ptView->iCellH;
    if (iVisRows < 1) iVisRows = 1;

    /* -- [TRACE]40. Snapshot ring buffer head/count under mutex, for a
     *               tearing-free render pass -- */
    if (platform_mutex_lock(ptView->ptMutex) != ST_NO_ERROR)
    {
        renderer_end_draw(ptView->hRenderer);
        return;
    }
    iSnapshot    = ptView->iCount;
    iHeadSnapshot = ptView->iHead;
    platform_mutex_unlock(ptView->ptMutex);

    /* iRow = screen row; iLineIdx = ring-buffer index (walks forward,
     * skipping lines filtered out by eViewMinLevel — P28). */
    iRow     = 0;
    iLineIdx = ptView->iScrollOff;

    while (iRow < iVisRows && iLineIdx < iSnapshot)
    {
        iAbsIdx = (iHeadSnapshot + iLineIdx) % TRACE_VIEW_MAX_LINES;
        iLineIdx++;

        /* Copy under mutex to avoid tearing */
        if (platform_mutex_lock(ptView->ptMutex) != ST_NO_ERROR)
        {
            break;
        }
        strncpy(szText, ptView->aLines[iAbsIdx].szText,
                TRACE_VIEW_LINE_LEN - 1);
        szText[TRACE_VIEW_LINE_LEN - 1] = '\0';
        eLevel = ptView->aLines[iAbsIdx].eLevel;
        platform_mutex_unlock(ptView->ptMutex);

        /* -- [TRACE]41. Skip lines below the GUI display minimum level
         *               (P28 filter) without consuming a screen row -- */
        if (eLevel < ptView->eViewMinLevel)
        {
            continue;
        }

        /* -- [TRACE]42. Draw the alternating row highlight and the
         *               level-coloured text for each visible line -- */
        fY = (float)(iRow * ptView->iCellH);

        /* Subtle row highlight for readability (alternating) */
        if ((iRow & 1) == 0)
        {
            tLine = (renderer_rect_t)RENDERER_RECT(0, (int)fY,
                                                    ptView->iWndWidth,
                                                    ptView->iCellH);
            renderer_fill_rect(ptView->hRenderer, &tLine, &g_tv_clrSel);
        }

        /* Text */
        tLine = (renderer_rect_t)RENDERER_RECT(4, (int)fY,
                                                ptView->iWndWidth - 4,
                                                ptView->iCellH);
        renderer_draw_text(ptView->hRenderer,
                           szText,
                           &tLine,
                           trace_level_color(eLevel),
                           RENDERER_FONT_MONO,
                           RENDERER_ALIGN_LEFT);
        iRow++;
    }

    renderer_end_draw(ptView->hRenderer);
}

/* ------------------------------------------------------------------
 * GUI trace view — scroll helpers
 * ------------------------------------------------------------------ */

/*
 * trace_view_scroll() - Move the scroll offset by iDelta lines, clamped
 *                        to the ring buffer bounds
 *
 * Parameters:
 *   ptView [in] : the pointer to the GUI view to be updated
 *   iDelta [in] : positive = scroll up (toward older lines)
 *
 * Returns:
 *   void
 */
static void trace_view_scroll(trace_view_t *ptView, int iDelta)
{
    int iVisRows;
    int iMaxOff;

    // No LOG_TRACE - R22: called on every mouse-wheel / key-repeat scroll

    /* -- [TRACE]43. Reject NULL view - do nothing -- */
    if (ptView == NULL) return;

    iVisRows = (ptView->iCellH > 0)
             ? (ptView->iWndHeight / ptView->iCellH)
             : 25;
    if (iVisRows < 1) iVisRows = 1;

    iMaxOff = ptView->iCount - iVisRows;
    if (iMaxOff < 0) iMaxOff = 0;

    /* -- [TRACE]44. Clamp the scroll offset between the top and the
     *               bottom of the ring buffer -- */
    ptView->iScrollOff -= iDelta;  /* positive iDelta = scroll up */
    if (ptView->iScrollOff < 0)       ptView->iScrollOff = 0;
    if (ptView->iScrollOff > iMaxOff) ptView->iScrollOff = iMaxOff;

    /* -- [TRACE]45. Re-enable auto-scroll once the user has scrolled
     *               back down to the newest line -- */
    ptView->bAutoScroll = (ptView->iScrollOff >= iMaxOff) ? ST_TRUE : ST_FALSE;
}

/*
 * trace_view_page() - Move the scroll offset by one full page
 *
 * Parameters:
 *   ptView [in] : the pointer to the GUI view to be updated
 *   iSign  [in] : +1 = page up, -1 = page down
 *
 * Returns:
 *   void
 */
static void trace_view_page(trace_view_t *ptView, int iSign)
{
    int iVisRows;

    // No LOG_TRACE - R22: delegates to trace_view_scroll(), called on
    // every Page Up/Down key-repeat

    /* -- [TRACE]46. Reject NULL view - do nothing -- */
    if (ptView == NULL) return;

    iVisRows = (ptView->iCellH > 0)
             ? (ptView->iWndHeight / ptView->iCellH)
             : 25;
    if (iVisRows < 1) iVisRows = 1;

    /* -- [TRACE]47. Scroll by a full page (iSign x visible rows) -- */
    trace_view_scroll(ptView, iSign * iVisRows);
}

/* ------------------------------------------------------------------
 * GUI trace view — event callback (called from the window thread)
 * ------------------------------------------------------------------ */

static void trace_event_callback(gui_window_t  hWnd,
                                   gui_event_t  *ptEvent,
                                   void         *pUserCtx)
{
    trace_view_t *ptView;

    // No LOG_TRACE - R22: dispatched for every GUI event (paint/resize/
    // scroll/key/close), several times per second while the window is live

    /* -- [TRACE]37. Reject any NULL pointer - Do nothing -- */
    if (ptEvent == NULL || pUserCtx == NULL) return;

    ptView = (trace_view_t *)pUserCtx;

    switch (ptEvent->eType)
    {
    /* -- [TRACE]48. GUI_EVT_PAINT - lazily create the D2D renderer on
     *               first paint, then render the visible lines -- */
    case GUI_EVT_PAINT:
        /* Lazy renderer creation: HWND is live at first paint */
        if (ptView->hRenderer == NULL)
        {
            if (renderer_create(hWnd, &ptView->hRenderer) == ST_NO_ERROR)
            {
                renderer_get_font_metrics(ptView->hRenderer,
                                          RENDERER_FONT_MONO,
                                          &ptView->iCellW,
                                          &ptView->iCellH);
                gui_get_size(hWnd,
                             &ptView->iWndWidth,
                             &ptView->iWndHeight);
            }
        }
        trace_render(ptView);
        break;

    /* -- [TRACE]49. GUI_EVT_RESIZE - resize the D2D renderer to the new
     *               window dimensions -- */
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

    /* -- [TRACE]50. GUI_EVT_SCROLL - mouse wheel scroll -- */
    case GUI_EVT_SCROLL:
        trace_view_scroll(ptView, ptEvent->uData.tScroll.iDelta);
        gui_invalidate(hWnd);
        break;

    /* -- [TRACE]51. GUI_EVT_KEY_DOWN - keyboard navigation
     *               (ESC/arrows/page/home/end) -- */
    case GUI_EVT_KEY_DOWN:
        switch (ptEvent->uData.tKey.eKey)
        {
        case GUI_KEY_ESCAPE:
            gui_request_close(hWnd);
            break;
        case GUI_KEY_UP:
            trace_view_scroll(ptView, 1);
            gui_invalidate(hWnd);
            break;
        case GUI_KEY_DOWN:
            trace_view_scroll(ptView, -1);
            gui_invalidate(hWnd);
            break;
        case GUI_KEY_PAGE_UP:
            trace_view_page(ptView, 1);
            gui_invalidate(hWnd);
            break;
        case GUI_KEY_PAGE_DOWN:
            trace_view_page(ptView, -1);
            gui_invalidate(hWnd);
            break;
        case GUI_KEY_HOME:
            ptView->iScrollOff = 0;
            ptView->bAutoScroll = ST_FALSE;
            gui_invalidate(hWnd);
            break;
        case GUI_KEY_END:
            /* large negative delta → iScrollOff += (iCount+1) → clamped
             * to iMaxOff by trace_view_scroll; bAutoScroll set to ST_TRUE
             * automatically when iScrollOff reaches iMaxOff. */
            trace_view_scroll(ptView, -(ptView->iCount + 1));
            gui_invalidate(hWnd);
            break;
        default:
            break;
        }
        break;

    /* -- [TRACE]52. GUI_EVT_CLOSE - destroy the D2D renderer -- */
    case GUI_EVT_CLOSE:
        if (ptView->hRenderer != NULL)
        {
            renderer_destroy(&ptView->hRenderer);
        }
        break;

    /* -- [TRACE]53. default - ignore any unhandled event type -- */
    default:
        break;
    }
}

/* ------------------------------------------------------------------
 * Public API (see description in trace.h)
 * ------------------------------------------------------------------ */
///////////////////////////////////////////////////////////////////////////////
//
st_u64_t trace_init()
{
    LOG_TRACE("Initializing the Trace Module");

    /* -- [TRACE]1. Log Information if already initialised -- */
    if (g_trace_ptCtx.bInitialised == ST_TRUE)
    {
        LOG_INFO("Already initialized");
        return (st_u64_t)&g_trace_ptCtx;
    }

    /* -- [TRACE]2. If not initialized, open TRACE_LOG file for writing -- */
    g_trace_ptCtx.pFile = fopen(TRACE_LOGFILE, "w");
    if (g_trace_ptCtx.pFile == NULL)
    {
        fprintf(stderr,
                "[trace_init] WARNING: cannot open '%s' - "
                "file logging disabled\n",
                TRACE_LOGFILE);
        return (st_u64_t)&g_trace_ptCtx;
    }

    /* -- [TRACE]3. Init trace context structure -- */
    g_trace_ptCtx.bInitialised  = ST_TRUE;
    g_trace_ptCtx.bGUITraceEnabled = ST_FALSE;
    
    /* -- [TRACE]4. Init returns context sructure -- */
    return (st_u64_t)&g_trace_ptCtx;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t trace_gui_open(void)
{
    trace_view_t  *ptView;
    gui_wnd_desc_t tDesc;
    st_error_t     eResult;

    LOG_TRACE("Opening the Trace Module GUI View");

    /* -- [TRACE]11. GUI must not be open when trace module is not initialized -- */
    if (g_trace_ptCtx.bInitialised == ST_FALSE)
    {
        fprintf(stderr,
                "[trace_gui_open] ERROR: trace module not initialised\n");
        return ST_ERROR;
    }

    /* -- [TRACE]12. Calling trace_gui_open() when GUI is open is harmless -- */
    if (g_trace_ptCtx.bOpen == ST_TRUE)
    {
        return ST_NO_ERROR;
    }

    g_trace_ptCtx.bOpen = ST_TRUE;

    /* -- [TRACE]13. Allocate a new trace_view structure -- */
    ptView = (trace_view_t *)malloc(sizeof(trace_view_t));
    if (ptView == NULL)
    {
        LOG_ERROR("malloc failed for trace_view_t");
        return ST_ERROR;
    }
    memset(ptView, 0, sizeof(trace_view_t));
    ptView->bAutoScroll   = ST_TRUE;
    ptView->eViewMinLevel = g_trace_ptCtx.eViewMinLevel; /* P28: persistent filter */

    eResult = platform_mutex_create(&ptView->ptMutex);
    if (eResult != ST_NO_ERROR)
    {
        free(ptView);
        LOG_ERROR("platform_mutex_create failed");
        return ST_ERROR;
    }

    /* -- [TRACE]14. Open the GUI window -- */
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle  = "ST4Ever - Trace";
    tDesc.eType    = GUI_WND_TRACE;
    tDesc.pfnEvent = trace_event_callback;
    tDesc.pUserCtx = ptView;

    eResult = gui_open_window(&tDesc, &ptView->hWnd);
    if (eResult != ST_NO_ERROR)
    {
        /* Non-fatal: GUI not available (headless / gui_init not called).
         * Trace continues with stderr + log file output only. */
        LOG_ERROR("gui_open_window failed - GUI not available - file log only");
        platform_mutex_destroy(&ptView->ptMutex);
        free(ptView);
        ptView = NULL;
    }
    
    g_trace_ptCtx.ptView = ptView;

    LOG_INFO("Trace gui is open (bOpen=%d, ptView=%p)", 
               g_trace_ptCtx.bOpen, (void*)g_trace_ptCtx.ptView);

    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t trace_gui_close(void)
{
    LOG_TRACE("Closing the trace window");
	
	/* -- [TRACE]15. Do not close if trace module is not initialized -- */
    if (g_trace_ptCtx.bInitialised == ST_FALSE)
    {
        return ST_NO_ERROR;
    }

    g_trace_ptCtx.bOpen = ST_FALSE;

    /* -- [TRACE]16. Close the GUI window -- */
    if (g_trace_ptCtx.ptView != NULL)
    {
		if (g_trace_ptCtx.ptView->hWnd != NULL)
		{
			gui_close_window(g_trace_ptCtx.ptView->hWnd);
			g_trace_ptCtx.ptView->hWnd = NULL;
		}

		platform_mutex_destroy(&g_trace_ptCtx.ptView->ptMutex);
        free(g_trace_ptCtx.ptView);
        g_trace_ptCtx.ptView = NULL;
    }

	LOG_INFO("Trace gui is closed (bOpen=%d, ptView=%p)", 
               g_trace_ptCtx.bOpen, (void*)g_trace_ptCtx.ptView);
    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
void trace_log(log_level_t  eLevel,
               const char  *szFunc,
               int          iLine,
               const char  *szFmt,
               ...)
{
    char    szTime[16];
    char    szMsg[ST_MAX_MSG];
    char    szLine[TRACE_VIEW_LINE_LEN];
    va_list vaArgs;
    int     bCompacted;

    // No LOG_TRACE here - we are in the function producing the logs

    /* -- [TRACE]5. Trace Context must be initialized before logging -- */
    if (g_trace_ptCtx.bInitialised == ST_FALSE)
    {
        return;
    }

    /* -- [TRACE]6. LOG_TRACE is compacted when called consecutively -- */
    /* --- Compaction logic for LOG_TRACE - based on function name  --- */
    bCompacted = 0;

    if (eLevel == LOG_LEVEL_TRACE && szFunc != NULL)
    {
        if (g_trace_ptCtx.szLastFunc[0] != '\0'
        &&  strncmp(g_trace_ptCtx.szLastFunc,
                    szFunc,
                    TRACE_COMPACT_FUNCLEN - 1) == 0)
        {
            g_trace_ptCtx.iCompactCount++;
            bCompacted = 1;
        }
        else
        {
            trace_flush_compact();
            strncpy(g_trace_ptCtx.szLastFunc,
                    szFunc,
                    TRACE_COMPACT_FUNCLEN - 1);
            g_trace_ptCtx.szLastFunc[TRACE_COMPACT_FUNCLEN - 1] = '\0';
            g_trace_ptCtx.iCompactCount = 1;
        }
    }
    else
    {
        trace_flush_compact();
    }

    if (bCompacted)
    {
        return;
    }

    /* -- [TRACE]7. Format logging messages -- */
	/* format : timestamp [TAG] function:line Message */
    va_start(vaArgs, szFmt);
    vsnprintf(szMsg, sizeof(szMsg), szFmt, vaArgs);
    va_end(vaArgs);

    trace_get_timestamp(szTime, sizeof(szTime));

    /* stderr intentionally omitted from trace_log():
     * when the trace console is open (bOpen=TRUE) the terminal must stay
     * clean — the GUI window is the output.  Preliminary traces during GUI
     * startup go to the log file only.  Real init errors use fprintf()
     * directly in trace_gui_open()/trace_init(), bypassing this path. */

    /* -- [TRACE]8. Log in file -- */
    /* ---- Write to log file (plain text) ---- */
    if (g_trace_ptCtx.pFile != NULL)
    {
        fprintf(g_trace_ptCtx.pFile,
                "%s [%s] %s:%d  %s\n",
                szTime,
                trace_level_label(eLevel),
                szFunc != NULL ? szFunc : "?",
                iLine,
                szMsg);
        fflush(g_trace_ptCtx.pFile);
    }

	/* -- [TRACE]9. LOG_TRACE is filtered from GUI unless requested -- */
    if (eLevel == LOG_LEVEL_TRACE
    &&  g_trace_ptCtx.bGUITraceEnabled == ST_FALSE)
    {
        return;
    }

    /* -- [TRACE]10. Log in GUI when available -- */
    /* ---- Append to GUI trace view (thread-safe) ---- */
    if (g_trace_ptCtx.ptView != NULL)
    {
        snprintf(szLine, sizeof(szLine),
                 "%s [%s] %s:%d  %s",
                 szTime,
                 trace_level_label(eLevel),
                 szFunc != NULL ? szFunc : "?",
                 iLine,
                 szMsg);

        if (platform_mutex_lock(g_trace_ptCtx.ptView->ptMutex) == ST_NO_ERROR)
        {
            trace_view_append_locked(g_trace_ptCtx.ptView, eLevel, szLine);
            platform_mutex_unlock(g_trace_ptCtx.ptView->ptMutex);
        }

        gui_invalidate(g_trace_ptCtx.ptView->hWnd);
	}
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t trace_clear(void)
{
    LOG_TRACE("ptView=%p", g_trace_ptCtx.ptView);
    
    /* -- [TRACE]19. Do nothing if no trace view window is open -- */
    if (g_trace_ptCtx.ptView == NULL)
    {
        return ST_NO_ERROR; /* no window open — no-op */
    }

    /* -- [TRACE]20. Reset the ring buffer under mutex protection -- */
    if (platform_mutex_lock(g_trace_ptCtx.ptView->ptMutex) == ST_NO_ERROR)
    {
        g_trace_ptCtx.ptView->iHead      = 0;
        g_trace_ptCtx.ptView->iCount     = 0;
        g_trace_ptCtx.ptView->iScrollOff = 0;
        platform_mutex_unlock(g_trace_ptCtx.ptView->ptMutex);
    }

    /* -- [TRACE]21. Invalidate the window for next redraw -- */
    if (g_trace_ptCtx.ptView->hWnd != NULL)
    {
        gui_invalidate(g_trace_ptCtx.ptView->hWnd);
    }

    LOG_INFO("trace: buffer cleared");
    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t trace_set_view_level(log_level_t eMinLevel)
{
    LOG_TRACE("Changing trace level to %d", eMinLevel);

    /* -- [TRACE]22. Store the new minimum level globally -- */
    g_trace_ptCtx.eViewMinLevel = eMinLevel;

    /* -- [TRACE]23. Propagate the new level to the open view and
     *               invalidate it -- */
    if (g_trace_ptCtx.ptView != NULL)
    {
        g_trace_ptCtx.ptView->eViewMinLevel = eMinLevel;
        if (g_trace_ptCtx.ptView->hWnd != NULL)
        {
            gui_invalidate(g_trace_ptCtx.ptView->hWnd);
        }
    }

    LOG_INFO("trace: view level set to %d", (int)eMinLevel);
    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t trace_shutdown(void)
{
    LOG_TRACE("Closing trace context");

	st_error_t eResult;

    /* -- [TRACE]24. Do nothing if trace module is not initialized -- */
    if (g_trace_ptCtx.bInitialised == ST_FALSE)
    {
        return ST_NO_ERROR;
    }

    /* -- [TRACE]25. Close the GUI window if still open -- */
    if (g_trace_ptCtx.bOpen == ST_TRUE)
    {
        if (gui_is_initialized()) trace_gui_close();
    }

    eResult = ST_NO_ERROR;

    LOG_INFO("%s %s shutting down - trace closing - bye folks",
             ST_APP_NAME, ST_APP_VERSION);

    /* -- [TRACE]26. Close the trace log file -- */
    if (g_trace_ptCtx.pFile != NULL)
    {
        if (fclose(g_trace_ptCtx.pFile) != 0)
        {
            fprintf(stderr,
                    "[trace_shutdown] ERROR: fclose() failed\n");
            eResult = ST_ERROR;
        }
        g_trace_ptCtx.pFile = NULL;
    }

    /* -- [TRACE]27. Reset trace context state to uninitialized -- */
    memset(&g_trace_ptCtx, 0, sizeof(g_trace_ptCtx));
    g_trace_ptCtx.ulMagic = 0xCAFEDECA;
    g_trace_ptCtx.eObject = ST_TRACE_CTX;

    return eResult;
}

/******************************************************************************
 * Getters
 *****************************************************************************/

log_level_t trace_get_view_level(void)
{
    LOG_TRACE("Get Request for eViewMinLevel=%d", g_trace_ptCtx.eViewMinLevel);
    
    return g_trace_ptCtx.eViewMinLevel;
}

st_bool_t trace_is_open(void)
{
    LOG_TRACE("Get Request for bOpen=%d", g_trace_ptCtx.bOpen);
    
    return g_trace_ptCtx.bOpen;
}

st_bool_t trace_is_trace_enabled(void)
{
    LOG_TRACE("Get Request for bGUITraceEnabled=%d", g_trace_ptCtx.bGUITraceEnabled);

    return g_trace_ptCtx.bGUITraceEnabled;
}

/******************************************************************************
 * Setters
 *****************************************************************************/

void trace_set_trace_enabled(st_bool_t bEnabled)
{
    LOG_TRACE("Set Request for bGUITraceEnabled=%d", bEnabled);

    g_trace_ptCtx.bGUITraceEnabled = bEnabled;
}

