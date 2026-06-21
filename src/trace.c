/*
 * trace.c - ST4Ever trace and debug console implementation
 *
 * UC4.4: Added GUI_WND_TRACE window (D2D append-only scroll view).
 *   - trace_open()  : opens GUI window + starts appending entries.
 *   - trace_close() : closes GUI window.
 *   - trace_log()   : writes to log file + GUI ring buffer (no stderr).
 *
 * Threading: trace_log() is called from any thread.
 *   The ring buffer (aLines/iHead/iCount) is protected by ptMutex.
 *   gui_invalidate() is called with a re-entrancy guard to prevent
 *   infinite recursion via LOG_TRACE inside gui_invalidate().
 */

#include "trace.h"
#include "gui.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>


/* ------------------------------------------------------------------
 * Internal constants
 * ------------------------------------------------------------------ */

#define TRACE_COMPACT_FUNCLEN   80   /* max func name stored for compact */

/* GUI trace window ring buffer.
 * LINE_LEN covers full formatted output: prefix (~60) + ST_MAX_MSG (2048).
 * 200 lines * ~2112 bytes = ~422 KB heap — acceptable for a debug view. */
#define TRACE_VIEW_MAX_LINES    200
#define TRACE_VIEW_LINE_LEN     (ST_MAX_MSG + 64)

/* ------------------------------------------------------------------
 * GUI trace view types  (internal to trace.c)
 * ------------------------------------------------------------------ */

typedef struct
{
    char        szText[TRACE_VIEW_LINE_LEN];
    log_level_t eLevel;
} trace_view_line_t;

typedef struct
{
    gui_window_t        hWnd;
    renderer_t          hRenderer;
    trace_view_line_t   aLines[TRACE_VIEW_MAX_LINES]; /* ring buffer    */
    int                 iHead;       /* index of oldest stored line      */
    int                 iCount;      /* number of lines currently stored */
    int                 iScrollOff;  /* index of first visible line      */
    st_bool_t           bAutoScroll; /* ST_TRUE = follow newest line     */
    int                 iWndWidth;
    int                 iWndHeight;
    int                 iCellW;
    int                 iCellH;
    st_mutex_t         *ptMutex;     /* protects aLines / iHead / iCount */
    log_level_t         eViewMinLevel; /* P28: GUI display filter level  */
} trace_view_t;

/* D2D colour palette for the trace window */
static const renderer_color_t g_tv_clrBg    = {0.09f, 0.09f, 0.14f, 1.0f};
static const renderer_color_t g_tv_clrTrace = {0.45f, 0.45f, 0.45f, 1.0f};
static const renderer_color_t g_tv_clrInfo  = {0.00f, 0.85f, 0.85f, 1.0f};
static const renderer_color_t g_tv_clrError = {0.95f, 0.10f, 0.10f, 1.0f};
static const renderer_color_t g_tv_clrTodo  = {0.85f, 0.00f, 0.85f, 1.0f};
static const renderer_color_t g_tv_clrSel   = {0.20f, 0.20f, 0.35f, 1.0f};

/* ------------------------------------------------------------------
 * Module globals  (g_trace_ prefix, static = module-private)
 * ------------------------------------------------------------------ */

static st_bool_t   g_trace_bInitialised  = ST_FALSE;
static st_bool_t   g_trace_bOpen         = ST_FALSE;
static st_bool_t   g_trace_bTraceEnabled = ST_TRUE;
static FILE       *g_trace_pFile         = NULL;

/* Compaction state for consecutive LOG_TRACE from the same function */
static char        g_trace_szLastFunc[TRACE_COMPACT_FUNCLEN] = {'\0'};
static int         g_trace_iCompactCount                     = 0;

/* GUI trace window state */
static trace_view_t *g_trace_ptView       = NULL;
static gui_window_t  g_trace_hWnd         = NULL;

/* P28: persistent display filter — survives close/reopen cycles */
static log_level_t   g_trace_eViewMinLevel = LOG_LEVEL_TRACE;

/* Re-entrancy guard: prevents gui_invalidate → LOG_TRACE → gui_invalidate
 * infinite loop.  Only blocks the invalidate call, not the append. */
static int           g_trace_bInNotify    = 0;

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

static void trace_get_timestamp(char *szBuf, size_t uiBufLen)
{
    time_t     tNow;
    struct tm *pTm;

    if (szBuf == NULL || uiBufLen == 0)
    {
        return;
    }

    tNow = time(NULL);
    pTm  = localtime(&tNow);

    if (pTm == NULL)
    {
        strncpy(szBuf, "??:??:??", uiBufLen - 1);
        szBuf[uiBufLen - 1] = '\0';
        return;
    }

    strftime(szBuf, uiBufLen, "%H:%M:%S", pTm);
}

static const char *trace_level_label(log_level_t eLevel)
{
    switch (eLevel)
    {
        case LOG_LEVEL_TRACE: return "TRC ";
        case LOG_LEVEL_INFO:  return "INF ";
        case LOG_LEVEL_ERROR: return "ERR ";
        case LOG_LEVEL_TODO:  return "TODO";
        default:              return "??? ";
    }
}


static const renderer_color_t *trace_level_color(log_level_t eLevel)
{
    switch (eLevel)
    {
        case LOG_LEVEL_TRACE: return &g_tv_clrTrace;
        case LOG_LEVEL_INFO:  return &g_tv_clrInfo;
        case LOG_LEVEL_ERROR: return &g_tv_clrError;
        case LOG_LEVEL_TODO:  return &g_tv_clrTodo;
        default:              return &g_tv_clrInfo;
    }
}

static void trace_flush_compact(void)
{
    char szTime[16];

    if (g_trace_iCompactCount <= 1)
    {
        g_trace_iCompactCount = 0;
        g_trace_szLastFunc[0] = '\0';
        return;
    }

    trace_get_timestamp(szTime, sizeof(szTime));

    /* stderr omitted — same rationale as trace_log(). */

    if (g_trace_pFile != NULL)
    {
        fprintf(g_trace_pFile,
                "%s [%s] %s [x%d]\n",
                szTime,
                trace_level_label(LOG_LEVEL_TRACE),
                g_trace_szLastFunc,
                g_trace_iCompactCount);
        fflush(g_trace_pFile);
    }

    g_trace_iCompactCount = 0;
    g_trace_szLastFunc[0] = '\0';
}

/* ------------------------------------------------------------------
 * GUI trace view — ring buffer append (caller holds ptMutex)
 * ------------------------------------------------------------------ */

static void trace_view_append_locked(trace_view_t *ptView,
                                      log_level_t   eLevel,
                                      const char   *szLine)
{
    int iInsert;

    if (ptView == NULL || szLine == NULL)
    {
        return;
    }

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

    strncpy(ptView->aLines[iInsert].szText, szLine,
            TRACE_VIEW_LINE_LEN - 1);
    ptView->aLines[iInsert].szText[TRACE_VIEW_LINE_LEN - 1] = '\0';
    ptView->aLines[iInsert].eLevel = eLevel;

    /* Auto-scroll: keep the newest line visible */
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
}

/* ------------------------------------------------------------------
 * GUI trace view — rendering
 * ------------------------------------------------------------------ */

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

    iVisRows = ptView->iWndHeight / ptView->iCellH;
    if (iVisRows < 1) iVisRows = 1;

    /* Take a consistent snapshot of count/head under mutex */
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

        /* P28: skip lines below the view minimum level */
        if (eLevel < ptView->eViewMinLevel)
        {
            continue;
        }

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

static void trace_view_scroll(trace_view_t *ptView, int iDelta)
{
    int iVisRows;
    int iMaxOff;

    if (ptView == NULL) return;

    iVisRows = (ptView->iCellH > 0)
             ? (ptView->iWndHeight / ptView->iCellH)
             : 25;
    if (iVisRows < 1) iVisRows = 1;

    iMaxOff = ptView->iCount - iVisRows;
    if (iMaxOff < 0) iMaxOff = 0;

    ptView->iScrollOff -= iDelta;  /* positive iDelta = scroll up */
    if (ptView->iScrollOff < 0)       ptView->iScrollOff = 0;
    if (ptView->iScrollOff > iMaxOff) ptView->iScrollOff = iMaxOff;

    /* If user scrolled to bottom, re-enable auto-scroll */
    ptView->bAutoScroll = (ptView->iScrollOff >= iMaxOff) ? ST_TRUE : ST_FALSE;
}

static void trace_view_page(trace_view_t *ptView, int iSign)
{
    int iVisRows;

    if (ptView == NULL) return;

    iVisRows = (ptView->iCellH > 0)
             ? (ptView->iWndHeight / ptView->iCellH)
             : 25;
    if (iVisRows < 1) iVisRows = 1;

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

    if (ptEvent == NULL || pUserCtx == NULL) return;

    ptView = (trace_view_t *)pUserCtx;

    switch (ptEvent->eType)
    {
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

    case GUI_EVT_SCROLL:
        trace_view_scroll(ptView, ptEvent->uData.tScroll.iDelta);
        gui_invalidate(hWnd);
        break;

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
 * Public API
 * ------------------------------------------------------------------ */

st_error_t trace_init(st_bool_t bOpen)
{
    
    if (g_trace_bInitialised == ST_TRUE)
    {
        fprintf(stderr, "[trace_init] WARNING: called more than once\n");
        return ST_NO_ERROR;
    }

    g_trace_pFile = fopen(TRACE_LOGFILE, "w");
    if (g_trace_pFile == NULL)
    {
        fprintf(stderr,
                "[trace_init] WARNING: cannot open '%s' - "
                "file logging disabled\n",
                TRACE_LOGFILE);
    }

    g_trace_bInitialised  = ST_TRUE;
    g_trace_bTraceEnabled = ST_TRUE;
    g_trace_iCompactCount = 0;
    g_trace_szLastFunc[0] = '\0';

    if (bOpen == ST_TRUE)
    {
        return trace_open();
    }

    return ST_NO_ERROR;
}

st_error_t trace_open(void)
{
    trace_view_t  *ptView;
    gui_wnd_desc_t tDesc;
    st_error_t     eResult;

    if (g_trace_bInitialised == ST_FALSE)
    {
        fprintf(stderr,
                "[trace_open] ERROR: trace not initialised\n");
        return ST_ERROR;
    }

    /* Idempotent: already open */
    if (g_trace_bOpen == ST_TRUE)
    {
        return ST_NO_ERROR;
    }

    g_trace_bOpen = ST_TRUE;

    /* Allocate and initialise the GUI view */
    ptView = (trace_view_t *)malloc(sizeof(trace_view_t));
    if (ptView == NULL)
    {
        fprintf(stderr,
                "[trace_open] ERROR: malloc failed for trace_view_t\n");
        return ST_ERROR;
    }
    memset(ptView, 0, sizeof(trace_view_t));
    ptView->bAutoScroll   = ST_TRUE;
    ptView->eViewMinLevel = g_trace_eViewMinLevel; /* P28: persistent filter */

    eResult = platform_mutex_create(&ptView->ptMutex);
    if (eResult != ST_NO_ERROR)
    {
        free(ptView);
        fprintf(stderr, "[trace_open] ERROR: platform_mutex_create failed\n");
        return ST_ERROR;
    }

    /* Open the GUI window */
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
        fprintf(stderr,
                "[trace_open] WARNING: gui_open_window failed - "
                "GUI trace window unavailable (stderr/log only)\n");
        platform_mutex_destroy(&ptView->ptMutex);
        free(ptView);
        ptView = NULL;
    }

    g_trace_ptView = ptView;
    g_trace_hWnd   = (ptView != NULL) ? ptView->hWnd : NULL;

    LOG_INFO("Trace console opened");
    return ST_NO_ERROR;
}

st_error_t trace_close(void)
{
    if (g_trace_bInitialised == ST_FALSE)
    {
        return ST_NO_ERROR;
    }

    trace_flush_compact();

    LOG_INFO("Trace console closed");
    g_trace_bOpen = ST_FALSE;

    /* Close the GUI window and free the view */
    if (g_trace_hWnd != NULL)
    {
        gui_close_window(g_trace_hWnd);
        g_trace_hWnd = NULL;
    }

    if (g_trace_ptView != NULL)
    {
        platform_mutex_destroy(&g_trace_ptView->ptMutex);
        free(g_trace_ptView);
        g_trace_ptView = NULL;
    }

    return ST_NO_ERROR;
}

st_error_t trace_set_trace_enabled(st_bool_t bEnabled)
{
    trace_flush_compact();
    g_trace_bTraceEnabled = bEnabled;
    LOG_INFO("LOG_TRACE %s", bEnabled == ST_TRUE ? "enabled" : "disabled");
    return ST_NO_ERROR;
}

st_bool_t trace_is_trace_enabled(void)
{
    return g_trace_bTraceEnabled;
}

st_bool_t trace_is_open(void)
{
    return g_trace_bOpen;
}

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

    if (g_trace_bInitialised == ST_FALSE)
    {
        return;
    }

    if (eLevel == LOG_LEVEL_TRACE
    &&  g_trace_bTraceEnabled == ST_FALSE)
    {
        return;
    }

    if (g_trace_bOpen == ST_FALSE && g_trace_pFile == NULL)
    {
        return;
    }

    /* ---- Compaction logic for LOG_TRACE ---- */
    bCompacted = 0;

    if (eLevel == LOG_LEVEL_TRACE && szFunc != NULL)
    {
        if (g_trace_szLastFunc[0] != '\0'
        &&  strncmp(g_trace_szLastFunc,
                    szFunc,
                    TRACE_COMPACT_FUNCLEN - 1) == 0)
        {
            g_trace_iCompactCount++;
            bCompacted = 1;
        }
        else
        {
            trace_flush_compact();
            strncpy(g_trace_szLastFunc,
                    szFunc,
                    TRACE_COMPACT_FUNCLEN - 1);
            g_trace_szLastFunc[TRACE_COMPACT_FUNCLEN - 1] = '\0';
            g_trace_iCompactCount = 1;
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

    /* ---- Format the message ---- */
    va_start(vaArgs, szFmt);
    vsnprintf(szMsg, sizeof(szMsg), szFmt, vaArgs);
    va_end(vaArgs);

    trace_get_timestamp(szTime, sizeof(szTime));

    /* stderr intentionally omitted from trace_log():
     * when the trace console is open (bOpen=TRUE) the terminal must stay
     * clean — the GUI window is the output.  Preliminary traces during GUI
     * startup go to the log file only.  Real init errors use fprintf()
     * directly in trace_open()/trace_init(), bypassing this path. */

    /* ---- Write to log file (plain text) ---- */
    if (g_trace_pFile != NULL)
    {
        fprintf(g_trace_pFile,
                "%s [%s] %s:%d  %s\n",
                szTime,
                trace_level_label(eLevel),
                szFunc != NULL ? szFunc : "?",
                iLine,
                szMsg);
        fflush(g_trace_pFile);
    }

    /* ---- Append to GUI trace view (thread-safe) ---- */
    /* Guard covers the entire GUI section: platform_mutex_lock calls
     * LOG_TRACE which re-enters trace_log.  Without this guard the
     * chain trace_log → mutex_lock → LOG_TRACE → trace_log → mutex_lock
     * recurses infinitely and overflows the stack. */
    if (g_trace_ptView != NULL && g_trace_hWnd != NULL
    &&  !g_trace_bInNotify)
    {
        g_trace_bInNotify = 1;

        snprintf(szLine, sizeof(szLine),
                 "%s [%s] %s:%d  %s",
                 szTime,
                 trace_level_label(eLevel),
                 szFunc != NULL ? szFunc : "?",
                 iLine,
                 szMsg);

        if (platform_mutex_lock(g_trace_ptView->ptMutex) == ST_NO_ERROR)
        {
            trace_view_append_locked(g_trace_ptView, eLevel, szLine);
            platform_mutex_unlock(g_trace_ptView->ptMutex);
        }

        gui_invalidate(g_trace_hWnd);

        g_trace_bInNotify = 0;
    }
}

st_error_t trace_clear(void)
{
    if (g_trace_ptView == NULL)
    {
        return ST_NO_ERROR; /* no window open — no-op */
    }

    if (platform_mutex_lock(g_trace_ptView->ptMutex) == ST_NO_ERROR)
    {
        g_trace_ptView->iHead      = 0;
        g_trace_ptView->iCount     = 0;
        g_trace_ptView->iScrollOff = 0;
        platform_mutex_unlock(g_trace_ptView->ptMutex);
    }

    if (g_trace_hWnd != NULL && !g_trace_bInNotify)
    {
        g_trace_bInNotify = 1;
        gui_invalidate(g_trace_hWnd);
        g_trace_bInNotify = 0;
    }

    LOG_INFO("trace: buffer cleared");
    return ST_NO_ERROR;
}

st_error_t trace_set_view_level(log_level_t eMinLevel)
{
    g_trace_eViewMinLevel = eMinLevel;

    if (g_trace_ptView != NULL)
    {
        g_trace_ptView->eViewMinLevel = eMinLevel;
        if (g_trace_hWnd != NULL && !g_trace_bInNotify)
        {
            g_trace_bInNotify = 1;
            gui_invalidate(g_trace_hWnd);
            g_trace_bInNotify = 0;
        }
    }

    LOG_INFO("trace: view level set to %d", (int)eMinLevel);
    return ST_NO_ERROR;
}

log_level_t trace_get_view_level(void)
{
    return g_trace_eViewMinLevel;
}

st_error_t trace_shutdown(void)
{
    st_error_t eResult;

    if (g_trace_bInitialised == ST_FALSE)
    {
        return ST_NO_ERROR;
    }

    LOG_INFO("%s %s shutting down - trace closing",
             ST_APP_NAME, ST_APP_VERSION);
    trace_flush_compact();

    /* Close GUI window if still open */
    if (g_trace_bOpen == ST_TRUE)
    {
        trace_close();
    }

    eResult = ST_NO_ERROR;

    if (g_trace_pFile != NULL)
    {
        if (fclose(g_trace_pFile) != 0)
        {
            fprintf(stderr,
                    "[trace_shutdown] ERROR: fclose() failed\n");
            eResult = ST_ERROR;
        }
        g_trace_pFile = NULL;
    }

    g_trace_bInitialised  = ST_FALSE;
    g_trace_bOpen         = ST_FALSE;
    g_trace_bTraceEnabled = ST_TRUE;
    g_trace_iCompactCount = 0;
    g_trace_szLastFunc[0] = '\0';

    return eResult;
}
