/*
 * lx_platform.c - Linux platform abstractions
 *
 * Implements portable mutex and thread primitives declared in
 * common.h using POSIX pthread_mutex_t and pthread_create.
 */

#include "../src/common.h"
#include "../src/trace.h"

#ifdef ST_PLATFORM_LINUX

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Mutex (pthread_mutex_t)
 * ------------------------------------------------------------------ */

st_error_t platform_mutex_create(st_mutex_t **pptMutex)
{
    pthread_mutex_t *pMtx;
    int              iRet;

    LOG_TRACE("pptMutex=%p", (void *)pptMutex);
    if (pptMutex == NULL) { LOG_ERROR("NULL"); return ST_ERROR; }

    *pptMutex = (st_mutex_t *)malloc(sizeof(st_mutex_t));
    if (*pptMutex == NULL) { LOG_ERROR("malloc failed"); return ST_ERROR; }

    pMtx = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (pMtx == NULL)
    {
        free(*pptMutex); *pptMutex = NULL;
        LOG_ERROR("malloc failed for pthread_mutex_t");
        return ST_ERROR;
    }

    iRet = pthread_mutex_init(pMtx, NULL);
    if (iRet != 0)
    {
        free(pMtx); free(*pptMutex); *pptMutex = NULL;
        LOG_ERROR("pthread_mutex_init failed: %d", iRet);
        return ST_ERROR;
    }

    (*pptMutex)->pHandle = pMtx;
    return ST_NO_ERROR;
}

st_error_t platform_mutex_lock(st_mutex_t *ptMutex)
{
    int iRet;
    LOG_TRACE("ptMutex=%p", (void *)ptMutex);
    if (ptMutex == NULL || ptMutex->pHandle == NULL)
    { LOG_ERROR("NULL"); return ST_ERROR; }
    iRet = pthread_mutex_lock((pthread_mutex_t *)ptMutex->pHandle);
    if (iRet != 0) { LOG_ERROR("pthread_mutex_lock: %d", iRet); return ST_ERROR; }
    return ST_NO_ERROR;
}

st_error_t platform_mutex_unlock(st_mutex_t *ptMutex)
{
    int iRet;
    LOG_TRACE("ptMutex=%p", (void *)ptMutex);
    if (ptMutex == NULL || ptMutex->pHandle == NULL)
    { LOG_ERROR("NULL"); return ST_ERROR; }
    iRet = pthread_mutex_unlock((pthread_mutex_t *)ptMutex->pHandle);
    if (iRet != 0) { LOG_ERROR("pthread_mutex_unlock: %d", iRet); return ST_ERROR; }
    return ST_NO_ERROR;
}

st_error_t platform_mutex_destroy(st_mutex_t **pptMutex)
{
    pthread_mutex_t *pMtx;
    LOG_TRACE("pptMutex=%p", (void *)pptMutex);
    if (pptMutex == NULL || *pptMutex == NULL)
    { LOG_ERROR("NULL"); return ST_ERROR; }
    pMtx = (pthread_mutex_t *)(*pptMutex)->pHandle;
    if (pMtx != NULL) { pthread_mutex_destroy(pMtx); free(pMtx); }
    free(*pptMutex); *pptMutex = NULL;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Thread (pthread)
 * ------------------------------------------------------------------ */

typedef struct { st_thread_fn pfnEntry; void *pArg; } lx_thread_arg_t;

static void *lx_thread_trampoline(void *pParam)
{
    lx_thread_arg_t *ptArg = (lx_thread_arg_t *)pParam;
    if (ptArg) { ptArg->pfnEntry(ptArg->pArg); free(ptArg); }
    return NULL;
}

st_error_t platform_thread_create(st_thread_t **pptThread,
                                    st_thread_fn   pfnEntry,
                                    void          *pArg)
{
    pthread_t       *pThr;
    lx_thread_arg_t *ptArg;
    int              iRet;

    LOG_TRACE("pptThread=%p", (void *)pptThread);
    if (pptThread == NULL || pfnEntry == NULL)
    { LOG_ERROR("NULL"); return ST_ERROR; }

    *pptThread = (st_thread_t *)malloc(sizeof(st_thread_t));
    if (*pptThread == NULL) { LOG_ERROR("malloc"); return ST_ERROR; }

    pThr = (pthread_t *)malloc(sizeof(pthread_t));
    ptArg = (lx_thread_arg_t *)malloc(sizeof(lx_thread_arg_t));
    if (pThr == NULL || ptArg == NULL)
    {
        free(pThr); free(ptArg); free(*pptThread); *pptThread = NULL;
        LOG_ERROR("malloc"); return ST_ERROR;
    }
    ptArg->pfnEntry = pfnEntry; ptArg->pArg = pArg;

    iRet = pthread_create(pThr, NULL, lx_thread_trampoline, ptArg);
    if (iRet != 0)
    {
        LOG_ERROR("pthread_create: %d", iRet);
        free(pThr); free(ptArg); free(*pptThread); *pptThread = NULL;
        return ST_ERROR;
    }

    (*pptThread)->pHandle = pThr;
    return ST_NO_ERROR;
}

st_error_t platform_thread_join(st_thread_t *ptThread)
{
    int iRet;
    LOG_TRACE("ptThread=%p", (void *)ptThread);
    if (ptThread == NULL || ptThread->pHandle == NULL)
    { LOG_ERROR("NULL"); return ST_ERROR; }
    iRet = pthread_join(*(pthread_t *)ptThread->pHandle, NULL);
    if (iRet != 0) { LOG_ERROR("pthread_join: %d", iRet); return ST_ERROR; }
    return ST_NO_ERROR;
}

st_error_t platform_thread_destroy(st_thread_t **pptThread)
{
    LOG_TRACE("pptThread=%p", (void *)pptThread);
    if (pptThread == NULL || *pptThread == NULL)
    { LOG_ERROR("NULL"); return ST_ERROR; }
    if ((*pptThread)->pHandle != NULL) { free((*pptThread)->pHandle); }
    free(*pptThread); *pptThread = NULL;
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_LINUX */
