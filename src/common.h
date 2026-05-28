/*
 * common.h - ST4Ever foundational types, return codes and macros
 *
 * Included by all source files. Defines the type conventions,
 * error propagation model, platform detection and utility macros
 * used throughout the project.
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

#endif /* COMMON_H */