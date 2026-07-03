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
 * When the trace console is open, output is written to:
 *   - st4ever_trace.log  (always, when initialized)
 *   - stderr with ANSI colours (always, as a fallback / CI-friendly copy)
 *   - the GUI trace window (GUI_WND_TRACE, UC4.4): D2D append-only scroll
 *     view with per-level colours.  Opened via gui_open_window() from
 *     trace_gui_open(); closed via gui_close_window() from trace_close().
 *
 * Threading: trace_log() is called from any thread.  The GUI window
 * notification (gui_invalidate) is guarded against re-entrancy.
 */

#ifndef TRACE_H
#define TRACE_H

#include "common.h"
#include "gui.h"
#include "renderer.h"

#define TRACE_LOGFILE           "st4ever_trace.log"

/* ------------------------------------------------------------------
 * Log levels
 * ------------------------------------------------------------------ */
typedef enum log_level_s
{
    LOG_LEVEL_TRACE = 0,  /* Function entry / parameter trace        */
    LOG_LEVEL_INFO  = 1,  /* Functional progress info                */
    LOG_LEVEL_ERROR = 2,  /* Internal error with source location     */
    LOG_LEVEL_TODO  = 3   /* Stub / future implementation marker     */
} log_level_t;

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
 * Trace Context
 * ------------------------------------------------------------------ */

typedef struct trace_context_s
{
    st_u32_t      ulMagic;                 /* Magic ST4Ever OO-like tag */
    st_object_t   eObject;                 /* Object type for tests     */
    
    st_bool_t     bInitialised;
    st_bool_t     bOpen;
    st_bool_t     bGUITraceEnabled;
    FILE         *pFile;

/* Compaction state for consecutive LOG_TRACE from the same function */
    char          szLastFunc[TRACE_COMPACT_FUNCLEN];
    int           iCompactCount;

/* GUI trace window state */
    trace_view_t *ptView;
    log_level_t   eViewMinLevel;   /* Persistent view level     */

} trace_context_t;


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
 *   Value of the global trace_context_t structure pointer on success.
 * 
 */
st_u64_t trace_init();

/*
 * trace_gui_open() - Open / show the trace console.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if not initialised.
 */
st_error_t trace_gui_open(void);

/*
 * trace_close() - Close / hide the trace console.
 *
 * Returns:
 *   ST_NO_ERROR always (safe to call when already closed).
 */
st_error_t trace_gui_close(void);

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
 * trace_clear() - Erase all lines from the GUI ring buffer.
 *
 * The log file is not affected.  Safe to call when the trace window
 * is closed (no-op).
 *
 * Returns:
 *   ST_NO_ERROR always.
 */
st_error_t trace_clear(void);

/*
 * trace_set_view_level() - Set the minimum log level shown in the GUI
 * trace window (P28 display filter).
 *
 * @req REQ-CON-030
 *
 * Entries below eMinLevel are still appended to the ring buffer and
 * written to the log file; they are simply not rendered.
 *
 * Parameters:
 *   eMinLevel [in] : Minimum visible level (LOG_LEVEL_TRACE..TODO).
 *
 * Returns:
 *   ST_NO_ERROR always.
 */
st_error_t trace_set_view_level(log_level_t eMinLevel);

/*
 * trace_get_view_level() - Return the current GUI display filter level.
 */
log_level_t trace_get_view_level(void);

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