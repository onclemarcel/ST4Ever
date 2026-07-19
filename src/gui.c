/*
 * gui.c - ST4Ever GUI subsystem portable layer
 *
 * Implements:
 *   - gui_msg_queue_t : thread-safe circular event buffer (mutex-
 *                       protected, with spin-wait for blocking get).
 *   - Window list     : global registry of open gui_window_t handles.
 *   - Public gui_*    : lifecycle wrappers that delegate window
 *                       creation / close / size to the platform
 *                       backend (gui_platform_* in win_gui.c /
 *                       lx_gui.c).
 *
 * UC3.1: msg_queue + window lifecycle (open / close / shutdown).
 * TODO(UC3.2): renderer integration in GUI_EVT_PAINT handling.
 */

#include "gui_backend.h"
#include "trace.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Message queue internals
 * ------------------------------------------------------------------ */

struct gui_msg_queue_s
{
    gui_event_t   *aEvents;     /* heap-allocated circular buffer     */
    size_t         uiCapacity;  /* maximum number of events           */
    size_t         uiHead;      /* next-read index                    */
    size_t         uiTail;      /* next-write index                   */
    size_t         uiCount;     /* events currently in queue          */
    st_mutex_t    *ptMutex;     /* protects all fields above          */
};

/* ------------------------------------------------------------------
 * Window list (global registry of open windows)
 * ------------------------------------------------------------------ */

static gui_context_t  g_gui_ptCtx = {.ulMagic = OBJ_MAGIC, 
                                     .eObject = ST_GUI_CTX };

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

static st_error_t gui_list_add(struct gui_window_s *ptWnd)
{
    size_t uiIdx;

    /* -- [GUI]9. Lock the GUI context structure -- */
    if (platform_mutex_lock(g_gui_ptCtx.ptMutex) != ST_NO_ERROR)
    {
        return ST_ERROR;
    }

    /* -- [GUI]10. Return ST_ERROR if max number is reached -- */
    if (g_gui_ptCtx.uiWndCount >= GUI_MAX_WINDOWS)
    {
        platform_mutex_unlock(g_gui_ptCtx.ptMutex);
        LOG_ERROR("window list full (%d slots)", GUI_MAX_WINDOWS);
        return ST_ERROR;
    }

    /* -- [GUI]11. Create a new entry & increment number of windows -- */
    for (uiIdx = 0; uiIdx < GUI_MAX_WINDOWS; uiIdx++)
    {
        if (g_gui_ptCtx.aptWnd[uiIdx] == NULL)
        {
            g_gui_ptCtx.aptWnd[uiIdx] = ptWnd;
            g_gui_ptCtx.uiLastOpenWindow = uiIdx;
            g_gui_ptCtx.uiWndCount++;
            break;
        }
    }

    platform_mutex_unlock(g_gui_ptCtx.ptMutex);
    return ST_NO_ERROR;
}

static void gui_list_remove(struct gui_window_s *ptWnd)
{
    size_t uiIdx;

    if (platform_mutex_lock(g_gui_ptCtx.ptMutex) != ST_NO_ERROR)
    {
        return;
    }

    for (uiIdx = 0; uiIdx < GUI_MAX_WINDOWS; uiIdx++)
    {
        if (g_gui_ptCtx.aptWnd[uiIdx] == ptWnd)
        {
            g_gui_ptCtx.aptWnd[uiIdx] = NULL;
            if (g_gui_ptCtx.uiWndCount > 0)
            {
                g_gui_ptCtx.uiWndCount--;
            }
            break;
        }
    }

    platform_mutex_unlock(g_gui_ptCtx.ptMutex);
}

/* ------------------------------------------------------------------
 * Public GUI API
 * ------------------------------------------------------------------ */

st_u64_t gui_init(void)
{
    size_t     uiIdx;

    LOG_TRACE("Init GUI module");

    /* -- [GUI]1. Log Information if already initialised -- */
    if (g_gui_ptCtx.bInit == ST_TRUE)
    {
        LOG_INFO("Already initialised");
        return (st_u64_t)&g_gui_ptCtx;
    }

    /* -- [GUI]2. Initialize windows list -- */
    for (uiIdx = 0; uiIdx < GUI_MAX_WINDOWS; uiIdx++)
    {
        g_gui_ptCtx.aptWnd[uiIdx] = NULL;
    }
    g_gui_ptCtx.uiWndCount = 0;

    /* -- [GUI]3. Create mutex list -- */
    if (platform_mutex_create(&g_gui_ptCtx.ptMutex) != ST_NO_ERROR)
    {
        LOG_ERROR("platform_mutex_create failed");
        return ST_ERROR;
    }

    /* -- [GUI]4. Platform-specific GUI init -- */
    /* (Win32: RegisterClass, X11: XOpenDisplay) */
    g_gui_ptCtx.eGUIPtfInit = gui_platform_init();
    if (g_gui_ptCtx.eGUIPtfInit != ST_NO_ERROR)
    {
        platform_mutex_destroy(&g_gui_ptCtx.ptMutex);
        LOG_ERROR("gui_platform_init failed");
        return ST_ERROR;
    }

    /* -- [GUI]5. Init returns context sructure -- */
    g_gui_ptCtx.bInit = ST_TRUE;
    LOG_INFO("GUI Module initialised");
    return (st_u64_t)&g_gui_ptCtx;
}

st_error_t gui_open_window(const gui_wnd_desc_t *ptDesc,
                                 gui_window_t   *phWnd)
{
    struct gui_window_s *ptWnd;
    st_error_t           eResult;

    /* -- [GUI]6. NULL pointers leads to ST_ERROR and message in log file -- */
    LOG_TRACE("ptDesc=%p phWnd=%p", (void *)ptDesc, (void *)phWnd);

    if (ptDesc == NULL || phWnd == NULL)
    {
        LOG_ERROR("NULL parameter: ptDesc=%p phWnd=%p",
                  (void *)ptDesc, (void *)phWnd);
        return ST_ERROR;
    }

    /* -- [GUI]7. Opening a window without GUI initialized returns ST_ERROR -- */
    if (g_gui_ptCtx.bInit == ST_FALSE)
    {
        LOG_ERROR("gui_open_window called before gui_init");
        return ST_ERROR;
    }

    
    /* -- [GUI]8. Create an OS window & add it in the GUI context structure -- */
    *phWnd = NULL;

    ptWnd = (struct gui_window_s *)malloc(sizeof(struct gui_window_s));
    if (ptWnd == NULL)
    {
        LOG_ERROR("malloc failed for gui_window_s");
        return ST_ERROR;
    }
    memset(ptWnd, 0, sizeof(struct gui_window_s));
    ptWnd->tDesc  = *ptDesc;
    ptWnd->bOpen  = ST_FALSE;
    ptWnd->uiEventDelayMs = GUI_DEFAULT_EVENT_DELAY_MS;
    if (g_gui_ptCtx.bActiveSpies == ST_TRUE)
    {
        ptWnd->bActiveSpies = ST_TRUE;
    }
            
    /* Platform creates the OS window + spawns event thread */
    eResult = gui_platform_window_create(ptWnd);
    if (eResult != ST_NO_ERROR)
    {
        free(ptWnd);
        LOG_ERROR("gui_platform_window_create failed");
        return ST_ERROR;
    }

    eResult = gui_list_add(ptWnd);
    if (eResult != ST_NO_ERROR)
    {
        gui_platform_window_close(ptWnd);
        platform_thread_join(ptWnd->ptThread);
        platform_thread_destroy(&ptWnd->ptThread);
        free(ptWnd);
        return ST_ERROR;
    }

    *phWnd = (gui_window_t)ptWnd;
    LOG_INFO("window opened: title='%s'",
             ptDesc->szTitle ? ptDesc->szTitle : "(untitled)");
    return ST_NO_ERROR;
}

st_error_t gui_close_window(gui_window_t hWnd)
{
    struct gui_window_s *ptWnd;

    LOG_TRACE("hWnd=%p", (void *)hWnd);

    if (!g_gui_ptCtx.bInit)
    {
        LOG_INFO("Calling gui_close_window() while gui has been shut down");
        return ST_ERROR;
    }

    if (hWnd == NULL)
    {
        LOG_ERROR("NULL hWnd");
        return ST_ERROR;
    }

    ptWnd = (struct gui_window_s *)hWnd;

    /* Ask the window thread to exit */
    if (ptWnd->bOpen == ST_TRUE)
    {
        gui_platform_window_close(ptWnd);
    }

    /* Join and destroy the thread */
    if (ptWnd->ptThread != NULL)
    {
        platform_thread_join(ptWnd->ptThread);
        platform_thread_destroy(&ptWnd->ptThread);
    }

    gui_list_remove(ptWnd);
    free(ptWnd);

    LOG_INFO("window closed");
    return ST_NO_ERROR;
}

st_error_t gui_request_close(gui_window_t hWnd)
{
    struct gui_window_s *ptWnd;

    LOG_TRACE("hWnd=%p", (void *)hWnd);

    if (hWnd == NULL)
    {
        LOG_ERROR("NULL hWnd");
        return ST_ERROR;
    }

    ptWnd = (struct gui_window_s *)hWnd;

    if (ptWnd->bOpen == ST_TRUE)
    {
        gui_platform_window_close(ptWnd);
    }

    LOG_INFO("close requested for window '%s'",
             ptWnd->tDesc.szTitle ? ptWnd->tDesc.szTitle : "(untitled)");
    return ST_NO_ERROR;
}

st_error_t gui_find_window_by_type(gui_wnd_type_t  eType,
                                    gui_window_t   *phWnd)
{
    size_t               uiIdx;
    struct gui_window_s *ptWnd;

    if (phWnd == NULL)
    {
        LOG_ERROR("phWnd is NULL");
        return ST_ERROR;
    }
    *phWnd = NULL;

    if (g_gui_ptCtx.ptMutex == NULL)
        return ST_NO_ERROR;

    if (platform_mutex_lock(g_gui_ptCtx.ptMutex) != ST_NO_ERROR)
        return ST_NO_ERROR;

    for (uiIdx = 0; uiIdx < GUI_MAX_WINDOWS; uiIdx++)
    {
        ptWnd = g_gui_ptCtx.aptWnd[uiIdx];
        if (ptWnd != NULL && ptWnd->bOpen == ST_TRUE &&
            ptWnd->tDesc.eType == eType)
        {
            *phWnd = (gui_window_t)ptWnd;
            break;
        }
    }

    platform_mutex_unlock(g_gui_ptCtx.ptMutex);
    return ST_NO_ERROR;
}

st_error_t gui_invalidate(gui_window_t hWnd)
{
    struct gui_window_s *ptWnd;

    if (hWnd == NULL)
    {
        LOG_ERROR("NULL hWnd");
        return ST_ERROR;
    }

    ptWnd = (struct gui_window_s *)hWnd;
    return gui_platform_window_invalidate(ptWnd);
}

st_error_t gui_handle_resize_event(gui_window_t       hWnd,
                                    const gui_event_t *ptEvent,
                                    int               *piWndWidth,
                                    int               *piWndHeight,
                                    void              *hRenderer)
{
    if (hWnd == NULL || ptEvent == NULL
    ||  piWndWidth == NULL || piWndHeight == NULL)
    {
        LOG_ERROR("NULL parameter: hWnd=%p ptEvent=%p piWndWidth=%p "
                  "piWndHeight=%p", (void *)hWnd, (void *)ptEvent,
                  (void *)piWndWidth, (void *)piWndHeight);
        return ST_ERROR;
    }

    *piWndWidth  = ptEvent->uData.tResize.iWidth;
    *piWndHeight = ptEvent->uData.tResize.iHeight;

    if (hRenderer != NULL)
    {
        renderer_resize((renderer_t)hRenderer, *piWndWidth, *piWndHeight);
    }

    gui_invalidate(hWnd);
    return ST_NO_ERROR;
}

st_error_t gui_get_size(gui_window_t  hWnd,
                         int          *piWidth,
                         int          *piHeight)
{
    struct gui_window_s *ptWnd;

    if (hWnd == NULL || piWidth == NULL || piHeight == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    ptWnd = (struct gui_window_s *)hWnd;
    return gui_platform_window_get_size(ptWnd, piWidth, piHeight);
}

st_error_t gui_shutdown(void)
{
    size_t               uiIdx;
    struct gui_window_s *ptWnd;

    LOG_TRACE("(void)");

    if (g_gui_ptCtx.bInit == ST_FALSE)
    {
        return ST_NO_ERROR;
    }

    /* Close all open windows */
    for (uiIdx = 0; uiIdx < GUI_MAX_WINDOWS; uiIdx++)
    {
        ptWnd = g_gui_ptCtx.aptWnd[uiIdx];
        if (ptWnd != NULL)
        {
            if (ptWnd->bOpen == ST_TRUE)
            {
                gui_platform_window_close(ptWnd);
            }
            if (ptWnd->ptThread != NULL)
            {
                platform_thread_join(ptWnd->ptThread);
                platform_thread_destroy(&ptWnd->ptThread);
            }
            g_gui_ptCtx.aptWnd[uiIdx] = NULL;
            free(ptWnd);
        }
    }
    g_gui_ptCtx.uiWndCount = 0;

    gui_platform_shutdown();

    if (g_gui_ptCtx.ptMutex != NULL)
    {
        platform_mutex_destroy(&g_gui_ptCtx.ptMutex);
    }

    memset(&g_gui_ptCtx, 0, sizeof(g_gui_ptCtx));
    g_gui_ptCtx.ulMagic = OBJ_MAGIC;
    g_gui_ptCtx.eObject = ST_GUI_CTX;
    LOG_INFO("GUI subsystem shutdown complete");
    return ST_NO_ERROR;
}

st_error_t gui_set_title(gui_window_t hWnd, const char *szTitle)
{
    struct gui_window_s *ptWnd;

    LOG_TRACE("hWnd=%p szTitle=%p", (void *)hWnd, (void *)szTitle);

    if (hWnd == NULL || szTitle == NULL)
    {
        LOG_ERROR("NULL parameter: hWnd=%p szTitle=%p",
                  (void *)hWnd, (void *)szTitle);
        return ST_ERROR;
    }

    ptWnd = (struct gui_window_s *)hWnd;
    return gui_platform_window_set_title(ptWnd, szTitle);
}

/* ------------------------------------------------------------------
 * Message queue API
 * ------------------------------------------------------------------ */

st_error_t gui_msg_create(gui_msg_queue_t *pphQueue,
                           size_t           uiCapacity)
{
    struct gui_msg_queue_s *ptQ;
    st_error_t              eResult;

    LOG_TRACE("pphQueue=%p uiCapacity=%zu",
              (void *)pphQueue, uiCapacity);

    if (pphQueue == NULL)
    {
        LOG_ERROR("NULL pphQueue");
        return ST_ERROR;
    }

    if (uiCapacity == 0)
    {
        LOG_ERROR("uiCapacity must be > 0");
        return ST_ERROR;
    }

    *pphQueue = NULL;

    ptQ = (struct gui_msg_queue_s *)malloc(
              sizeof(struct gui_msg_queue_s));
    if (ptQ == NULL)
    {
        LOG_ERROR("malloc failed for gui_msg_queue_s");
        return ST_ERROR;
    }
    memset(ptQ, 0, sizeof(struct gui_msg_queue_s));

    ptQ->aEvents = (gui_event_t *)malloc(
                       uiCapacity * sizeof(gui_event_t));
    if (ptQ->aEvents == NULL)
    {
        LOG_ERROR("malloc failed for event buffer");
        free(ptQ);
        return ST_ERROR;
    }

    eResult = platform_mutex_create(&ptQ->ptMutex);
    if (eResult != ST_NO_ERROR)
    {
        free(ptQ->aEvents);
        free(ptQ);
        LOG_ERROR("platform_mutex_create failed");
        return ST_ERROR;
    }

    ptQ->uiCapacity = uiCapacity;
    ptQ->uiHead     = 0;
    ptQ->uiTail     = 0;
    ptQ->uiCount    = 0;

    *pphQueue = (gui_msg_queue_t)ptQ;
    return ST_NO_ERROR;
}

st_error_t gui_msg_post(gui_msg_queue_t    hQueue,
                         const gui_event_t *ptEvent)
{
    struct gui_msg_queue_s *ptQ;
    st_error_t              eResult;

    if (hQueue == NULL || ptEvent == NULL)
    {
        LOG_ERROR("NULL parameter: hQueue=%p ptEvent=%p",
                  (void *)hQueue, (void *)ptEvent);
        return ST_ERROR;
    }

    ptQ = (struct gui_msg_queue_s *)hQueue;

    eResult = platform_mutex_lock(ptQ->ptMutex);
    if (eResult != ST_NO_ERROR)
    {
        return ST_ERROR;
    }

    if (ptQ->uiCount >= ptQ->uiCapacity)
    {
        platform_mutex_unlock(ptQ->ptMutex);
        LOG_ERROR("queue full (capacity=%zu)", ptQ->uiCapacity);
        return ST_ERROR;
    }

    ptQ->aEvents[ptQ->uiTail] = *ptEvent;
    ptQ->uiTail = (ptQ->uiTail + 1) % ptQ->uiCapacity;
    ptQ->uiCount++;

    platform_mutex_unlock(ptQ->ptMutex);
    return ST_NO_ERROR;
}

st_error_t gui_msg_get(gui_msg_queue_t  hQueue,
                        gui_event_t     *ptEvent,
                        st_bool_t        bBlock)
{
    struct gui_msg_queue_s *ptQ;
    st_error_t              eResult;
    st_bool_t               bGot;

    if (hQueue == NULL || ptEvent == NULL)
    {
        LOG_ERROR("NULL parameter: hQueue=%p ptEvent=%p",
                  (void *)hQueue, (void *)ptEvent);
        return ST_ERROR;
    }

    ptQ  = (struct gui_msg_queue_s *)hQueue;
    bGot = ST_FALSE;

    do
    {
        eResult = platform_mutex_lock(ptQ->ptMutex);
        if (eResult != ST_NO_ERROR)
        {
            return ST_ERROR;
        }

        if (ptQ->uiCount > 0)
        {
            *ptEvent = ptQ->aEvents[ptQ->uiHead];
            ptQ->uiHead = (ptQ->uiHead + 1) % ptQ->uiCapacity;
            ptQ->uiCount--;
            bGot = ST_TRUE;
        }

        platform_mutex_unlock(ptQ->ptMutex);

        if (bGot == ST_FALSE && bBlock == ST_TRUE)
        {
            /* Spin-wait: yield for 1 ms before retry.
             * TODO(UC4): replace with condition variable / Win32 Event. */
            platform_sleep_ms(1);
        }

    } while (bGot == ST_FALSE && bBlock == ST_TRUE);

    return (bGot == ST_TRUE) ? ST_NO_ERROR : ST_ERROR;
}

st_error_t gui_msg_destroy(gui_msg_queue_t *pphQueue)
{
    struct gui_msg_queue_s *ptQ;

    LOG_TRACE("pphQueue=%p", (void *)pphQueue);

    if (pphQueue == NULL || *pphQueue == NULL)
    {
        LOG_ERROR("NULL pphQueue or *pphQueue");
        return ST_ERROR;
    }

    ptQ = (struct gui_msg_queue_s *)*pphQueue;

    if (ptQ->ptMutex != NULL)
    {
        platform_mutex_destroy(&ptQ->ptMutex);
    }

    free(ptQ->aEvents);
    free(ptQ);
    *pphQueue = NULL;

    return ST_NO_ERROR;
}

st_bool_t gui_is_initialized()
{
    return g_gui_ptCtx.bInit;
}