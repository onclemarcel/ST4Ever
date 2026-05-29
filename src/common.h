/*
 * common.h - ST4Ever foundational types, return codes and macros
 *
 * Included by all source files. Defines the type conventions,
 * error propagation model, platform detection, utility macros,
 * and the portable mutex / thread abstractions used throughout
 * the project.
 *
 * Platform-specific implementations of st_mutex_t and st_thread_t
 * live in win/win_platform.c (Windows) and linux/lx_platform.c
 * (Linux).
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Return codes
 * ------------------------------------------------------------------ */

typedef enum
{
    ST_NO_ERROR = 0,   /* Operation completed successfully            */
    ST_ERROR    = 1    /* Operation failed - see trace log for detail */
} st_error_t;

/* ------------------------------------------------------------------
 * Boolean
 * ------------------------------------------------------------------ */

typedef enum
{
    ST_FALSE = 0,
    ST_TRUE  = 1
} st_bool_t;

/* ------------------------------------------------------------------
 * Platform detection
 * ------------------------------------------------------------------ */

#if defined(_WIN32) || defined(_WIN64)
    #define ST_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define ST_PLATFORM_LINUX
#else
    #error "Unsupported platform - only Windows (MSYS2) and Linux supported"
#endif

/* ------------------------------------------------------------------
 * Fixed-width integer aliases
 * ------------------------------------------------------------------ */

typedef uint8_t  st_u8_t;    /* Unsigned  8-bit */
typedef uint16_t st_u16_t;   /* Unsigned 16-bit */
typedef uint32_t st_u32_t;   /* Unsigned 32-bit */
typedef uint64_t st_u64_t;   /* Unsigned 64-bit */
typedef int8_t   st_i8_t;    /* Signed    8-bit */
typedef int16_t  st_i16_t;   /* Signed   16-bit */
typedef int32_t  st_i32_t;   /* Signed   32-bit */
typedef int64_t  st_i64_t;   /* Signed   64-bit */

/* ------------------------------------------------------------------
 * Utility macros
 * ------------------------------------------------------------------ */

/* Suppress unused-parameter warnings */
#define ST_UNUSED(x)        ((void)(x))

/* Number of elements in a statically-allocated array */
#define ST_ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))

/* Safe minimum / maximum (evaluate each argument once) */
#define ST_MIN(a, b)        ((a) < (b) ? (a) : (b))
#define ST_MAX(a, b)        ((a) > (b) ? (a) : (b))

/* ------------------------------------------------------------------
 * String / buffer size constants
 * ------------------------------------------------------------------ */

#define ST_MAX_PATH         512    /* Maximum file path length        */
#define ST_MAX_CMD          1024   /* Maximum console command length  */
#define ST_MAX_MSG          2048   /* Maximum formatted message length*/

/* ------------------------------------------------------------------
 * Application identity
 * ------------------------------------------------------------------ */

#define ST_APP_NAME         "ST4Ever"
#define ST_APP_VERSION      "0.1.0-UC1"
#define ST_APP_DESC         "Atari ST Revival Engine"

/* ------------------------------------------------------------------
 * Portable mutex abstraction
 *
 * The implementation is a thin opaque wrapper so that no platform
 * headers (windows.h, pthread.h) need to be included here.
 *
 * Implemented in:
 *   win/win_platform.c  (uses CRITICAL_SECTION)
 *   linux/lx_platform.c (uses pthread_mutex_t)
 * ------------------------------------------------------------------ */

typedef struct
{
    void *pHandle;   /* Platform-specific mutex handle (opaque)     */
} st_mutex_t;

/*
 * platform_mutex_create() - Allocate and initialise a mutex.
 *
 * Parameters:
 *   pptMutex [out] : Receives pointer to the created mutex.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on allocation or system call failure.
 */
st_error_t platform_mutex_create(st_mutex_t **pptMutex);

/*
 * platform_mutex_lock() - Acquire the mutex (blocking).
 *
 * Parameters:
 *   ptMutex [in] : Mutex to lock.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptMutex is NULL or the system call fails.
 */
st_error_t platform_mutex_lock(st_mutex_t *ptMutex);

/*
 * platform_mutex_unlock() - Release the mutex.
 *
 * Parameters:
 *   ptMutex [in] : Mutex to unlock.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptMutex is NULL or the system call fails.
 */
st_error_t platform_mutex_unlock(st_mutex_t *ptMutex);

/*
 * platform_mutex_destroy() - Release mutex resources.
 *
 * The pointer *pptMutex is set to NULL on success.
 *
 * Parameters:
 *   pptMutex [in/out] : Mutex to destroy.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptMutex or *pptMutex is NULL.
 */
st_error_t platform_mutex_destroy(st_mutex_t **pptMutex);

/* ------------------------------------------------------------------
 * Portable thread abstraction
 *
 * Implemented in:
 *   win/win_platform.c  (uses CreateThread / HANDLE)
 *   linux/lx_platform.c (uses pthread_create)
 * ------------------------------------------------------------------ */

typedef struct
{
    void *pHandle;   /* Platform-specific thread handle (opaque)    */
} st_thread_t;

/* Thread entry point type */
typedef void (*st_thread_fn)(void *pArg);

/*
 * platform_thread_create() - Spawn a new thread.
 *
 * Parameters:
 *   pptThread [out] : Receives pointer to the created thread object.
 *   pfnEntry  [in]  : Thread function (void fn(void*)).
 *   pArg      [in]  : Argument forwarded to pfnEntry.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on failure.
 */
st_error_t platform_thread_create(st_thread_t   **pptThread,
                                   st_thread_fn    pfnEntry,
                                   void           *pArg);

/*
 * platform_thread_join() - Wait for a thread to finish.
 *
 * Parameters:
 *   ptThread [in] : Thread to join.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptThread is NULL or the system call fails.
 */
st_error_t platform_thread_join(st_thread_t *ptThread);

/*
 * platform_thread_destroy() - Release thread object resources.
 *
 * Must be called after platform_thread_join(). Sets *pptThread to NULL.
 *
 * Parameters:
 *   pptThread [in/out] : Thread to destroy.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptThread or *pptThread is NULL.
 */
st_error_t platform_thread_destroy(st_thread_t **pptThread);

#endif /* COMMON_H */
