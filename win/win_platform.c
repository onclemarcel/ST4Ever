/*
 * win_platform.c - ST4Ever Windows platform abstractions
 *
 * Implements portable mutex and thread primitives declared in
 * common.h using Win32 CRITICAL_SECTION and CreateThread.
 *
 * TODO(UC4): Implement platform_thread_create/join/destroy.
 * TODO(UC3): Mutex used by gui_msg_queue (implement with UC3).
 */

#include "../src/common.h"
#include "../src/trace.h"

#ifdef ST_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Mutex
 * ------------------------------------------------------------------ */

st_error_t platform_mutex_create(st_mutex_t **pptMutex)
{
    CRITICAL_SECTION *pCs;

    LOG_TRACE("pptMutex=%p", (void *)pptMutex);
    if (pptMutex == NULL)
    {
        LOG_ERROR("NULL pptMutex");
        return ST_ERROR;
    }

    *pptMutex = (st_mutex_t *)malloc(sizeof(st_mutex_t));
    if (*pptMutex == NULL)
    {
        LOG_ERROR("malloc failed for st_mutex_t");
        return ST_ERROR;
    }

    pCs = (CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
    if (pCs == NULL)
    {
        LOG_ERROR("malloc failed for CRITICAL_SECTION");
        free(*pptMutex);
        *pptMutex = NULL;
        return ST_ERROR;
    }

    InitializeCriticalSection(pCs);
    (*pptMutex)->pHandle = pCs;

    return ST_NO_ERROR;
}

st_error_t platform_mutex_lock(st_mutex_t *ptMutex)
{
    LOG_TRACE("ptMutex=%p", (void *)ptMutex);
    if (ptMutex == NULL || ptMutex->pHandle == NULL)
    {
        LOG_ERROR("NULL ptMutex or pHandle");
        return ST_ERROR;
    }
    EnterCriticalSection((CRITICAL_SECTION *)ptMutex->pHandle);
    return ST_NO_ERROR;
}

st_error_t platform_mutex_unlock(st_mutex_t *ptMutex)
{
    LOG_TRACE("ptMutex=%p", (void *)ptMutex);
    if (ptMutex == NULL || ptMutex->pHandle == NULL)
    {
        LOG_ERROR("NULL ptMutex or pHandle");
        return ST_ERROR;
    }
    LeaveCriticalSection((CRITICAL_SECTION *)ptMutex->pHandle);
    return ST_NO_ERROR;
}

st_error_t platform_mutex_destroy(st_mutex_t **pptMutex)
{
    CRITICAL_SECTION *pCs;

    LOG_TRACE("pptMutex=%p", (void *)pptMutex);
    if (pptMutex == NULL || *pptMutex == NULL)
    {
        LOG_ERROR("NULL pptMutex or *pptMutex");
        return ST_ERROR;
    }

    pCs = (CRITICAL_SECTION *)(*pptMutex)->pHandle;
    if (pCs != NULL)
    {
        DeleteCriticalSection(pCs);
        free(pCs);
    }
    free(*pptMutex);
    *pptMutex = NULL;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Thread - internal trampoline
 * ------------------------------------------------------------------ */

typedef struct
{
    st_thread_fn  pfnEntry;
    void         *pArg;
} win_thread_arg_t;

static DWORD WINAPI win_thread_trampoline(LPVOID lpParam)
{
    win_thread_arg_t *ptArg;

    ptArg = (win_thread_arg_t *)lpParam;
    if (ptArg == NULL) { return 1; }

    ptArg->pfnEntry(ptArg->pArg);
    free(ptArg);
    return 0;
}

st_error_t platform_thread_create(st_thread_t  **pptThread,
                                    st_thread_fn   pfnEntry,
                                    void          *pArg)
{
    win_thread_arg_t *ptArg;
    HANDLE            hThread;

    LOG_TRACE("pptThread=%p pfnEntry=%p pArg=%p",
              (void *)pptThread, (void *)(size_t)pfnEntry, pArg);
    if (pptThread == NULL || pfnEntry == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    *pptThread = (st_thread_t *)malloc(sizeof(st_thread_t));
    if (*pptThread == NULL)
    {
        LOG_ERROR("malloc failed for st_thread_t");
        return ST_ERROR;
    }

    ptArg = (win_thread_arg_t *)malloc(sizeof(win_thread_arg_t));
    if (ptArg == NULL)
    {
        LOG_ERROR("malloc failed for thread arg");
        free(*pptThread);
        *pptThread = NULL;
        return ST_ERROR;
    }
    ptArg->pfnEntry = pfnEntry;
    ptArg->pArg     = pArg;

    hThread = CreateThread(NULL, 0, win_thread_trampoline, ptArg,
                            0, NULL);
    if (hThread == NULL)
    {
        LOG_ERROR("CreateThread failed: %lu", GetLastError());
        free(ptArg);
        free(*pptThread);
        *pptThread = NULL;
        return ST_ERROR;
    }

    (*pptThread)->pHandle = hThread;
    LOG_INFO("thread created (HANDLE=%p)", hThread);
    return ST_NO_ERROR;
}

st_error_t platform_thread_join(st_thread_t *ptThread)
{
    DWORD dwResult;

    LOG_TRACE("ptThread=%p", (void *)ptThread);
    if (ptThread == NULL || ptThread->pHandle == NULL)
    {
        LOG_ERROR("NULL ptThread or pHandle");
        return ST_ERROR;
    }

    dwResult = WaitForSingleObject((HANDLE)ptThread->pHandle,
                                    INFINITE);
    if (dwResult != WAIT_OBJECT_0)
    {
        LOG_ERROR("WaitForSingleObject failed: %lu GetLastError=%lu",
                  dwResult, GetLastError());
        return ST_ERROR;
    }

    return ST_NO_ERROR;
}

st_error_t platform_thread_destroy(st_thread_t **pptThread)
{
    LOG_TRACE("pptThread=%p", (void *)pptThread);
    if (pptThread == NULL || *pptThread == NULL)
    {
        LOG_ERROR("NULL pptThread or *pptThread");
        return ST_ERROR;
    }

    if ((*pptThread)->pHandle != NULL)
    {
        CloseHandle((HANDLE)(*pptThread)->pHandle);
    }
    free(*pptThread);
    *pptThread = NULL;
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_WINDOWS */
