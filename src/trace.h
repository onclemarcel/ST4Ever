/*
 * trace.h - ST4Ever trace and debug console
 *
 * Provides four log levels for runtime diagnostics:
 *
 *   LOG_TRACE  function entry with key parameter values.
 *              Compacted: consecutive calls from the same function
 *              are collapsed into a single entry with a repeat count
 *              to prevent log flooding in tight loops.
 *
 *   LOG_INFO   functional state information useful for following
 *              application progress at a higher level.
 *
 *   LOG_ERROR  internal errors: NULL pointers, failed system calls,
 *              unexpected state. Always identifies the source location.
 *
 *   LOG_TODO   marks stub functions or code paths not yet implemented.
 *              Helps track work remaining across the codebase.
 *
 * For UC1 the trace output is written to:
 *   - st4ever_trace.log  (always, when initialized)
 *   - stderr with ANSI colours (when trace console is open)
 *
 * TODO(UC3): Replace the stderr output with a dedicated Win32 GDI window (Windows)
 * or X11 window (Linux) opened by gui_create_trace_window().
 */

#ifndef TRACE_H
#define TRACE_H

#include "common.h"

/* ------------------------------------------------------------------
 * Log levels
 * ------------------------------------------------------------------ */

typedef enum
{
    LOG_LEVEL_TRACE = 0,  /* Function entry / parameter trace        */
    LOG_LEVEL_INFO  = 1,  /* Functional progress info                */
    LOG_LEVEL_ERROR = 2,  /* Internal error with source location     */
    LOG_LEVEL_TODO  = 3   /* Stub / future implementation marker     */
} log_level_t;

/* ------------------------------------------------------------------
 * Log macros  (always use these, never call trace_log() directly)
 * ------------------------------------------------------------------ */

#define LOG_TRACE(fmt, ...) \
    trace_log(LOG_LEVEL_TRACE, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    trace_log(LOG_LEVEL_INFO,  __func__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    trace_log(LOG_LEVEL_ERROR, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_TODO(fmt, ...) \
    trace_log(LOG_LEVEL_TODO,  __func__, __LINE__, fmt, ##__VA_ARGS__)

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * trace_init() - Initialise the trace subsystem.
 *
 * Opens the log file and optionally shows the trace console.
 * Must be called before any LOG_* macro.
 *
 * Parameters:
 *   bOpen [in] : ST_TRUE to open the trace console immediately.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_NO_ERROR if already initialised (idempotent - logs a warning to
 *               stderr, leaves the subsystem unchanged).
 */
st_error_t trace_init(st_bool_t bOpen);

/*
 * trace_open() - Open / show the trace console.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if not initialised.
 */
st_error_t trace_open(void);

/*
 * trace_close() - Close / hide the trace console.
 *
 * Returns:
 *   ST_NO_ERROR always (safe to call when already closed).
 */
st_error_t trace_close(void);

/*
 * trace_set_trace_enabled() - Enable or disable LOG_TRACE output.
 *
 * LOG_INFO, LOG_ERROR and LOG_TODO are unaffected.
 *
 * Parameters:
 *   bEnabled [in] : ST_TRUE to enable, ST_FALSE to suppress.
 *
 * Returns:
 *   ST_NO_ERROR.
 */
st_error_t trace_set_trace_enabled(st_bool_t bEnabled);

/*
 * trace_is_trace_enabled() - Query whether LOG_TRACE is active.
 *
 * Returns:
 *   ST_TRUE if LOG_TRACE output is enabled, ST_FALSE otherwise.
 */
st_bool_t trace_is_trace_enabled(void);

/*
 * trace_is_open() - Query whether the trace console is visible.
 *
 * Returns:
 *   ST_TRUE if the trace console is open, ST_FALSE otherwise.
 */
st_bool_t trace_is_open(void);

/*
 * trace_log() - Emit one log entry.  Use the LOG_* macros instead.
 *
 * Parameters:
 *   eLevel [in] : Log level.
 *   szFunc [in] : Calling function name (__func__).
 *   iLine  [in] : Source line number (__LINE__).
 *   szFmt  [in] : printf-style format string.
 *   ...         : Format arguments.
 */
void trace_log(log_level_t  eLevel,
               const char  *szFunc,
               int          iLine,
               const char  *szFmt,
               ...);

/*
 * trace_shutdown() - Flush pending entries, close log file, free state.
 *
 * Safe to call if trace_init() was never called.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if fclose() fails (logged to stderr).
 */
st_error_t trace_shutdown(void);

#endif /* TRACE_H */