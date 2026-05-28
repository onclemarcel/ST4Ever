/*
 * line.h - ST4Ever console line reader and command dispatcher
 *
 * Manages the interactive console loop: displays the prompt,
 * reads user input, parses the command and its arguments,
 * and dispatches to the appropriate command handler.
 *
 * Command syntax:
 *   <command> [arg1 [arg2 ...]]
 *   ^-- first word only, matched against the command table
 *              ^-- tab-completed against files/dirs (UC4)
 *
 * For UC1 the input is a plain fgets() read.
 * TODO(UC4): replace with the rich line editor (history, tab-complete,
 *            pre-completion ghost text, arrow-key navigation).
 */

#ifndef LINE_H
#define LINE_H

#include "common.h"

/* ------------------------------------------------------------------
 * Command identifiers
 * ------------------------------------------------------------------ */

typedef enum
{
    CMD_UNKNOWN  = -1,  /* Unrecognised command token                */
    CMD_NONE     =  0,  /* Empty input line                          */
    CMD_HELP,           /* h | help    | CTRL+H                      */
    CMD_QUIT,           /* q | quit    | CTRL+Q / CTRL+C             */
    CMD_DIR,            /* d | dir     | CTRL+D                      */
    CMD_LOAD,           /* l | load    | CTRL+L                      */
    CMD_EDIT,           /* e | edit    | CTRL+E                      */
    CMD_IMAGE,          /* i | image   | CTRL+I                      */
    CMD_MOUNT,          /* m | mount   | CTRL+M                      */
    CMD_UMOUNT,         /* u | umount  | CTRL+U                      */
    CMD_WHERE,          /* w | where   | CTRL+W                      */
    CMD_TRACE,          /* t | trace   | CTRL+T                      */
    CMD_EXECUTE,        /* x | execute | CTRL+X                      */
    CMD_COUNT           /* Sentinel - must be last                   */
} cmd_id_t;

/* ------------------------------------------------------------------
 * Parsed command
 * ------------------------------------------------------------------ */

#define LINE_MAX_ARGS   8   /* Maximum number of arguments per command */

typedef struct
{
    cmd_id_t  eCmd;                     /* Matched command identifier */
    char      szRaw[ST_MAX_CMD];        /* Verbatim input line        */
    int       iArgc;                    /* Token count (argv[0]=cmd)  */
    char     *aszArgv[LINE_MAX_ARGS];   /* Pointers into szArgBuf     */
    char      szArgBuf[ST_MAX_CMD];     /* Tokenisation working copy  */
} parsed_cmd_t;

/* ------------------------------------------------------------------
 * Console context
 * ------------------------------------------------------------------ */

typedef struct
{
    char      szCwd[ST_MAX_PATH];       /* Current working directory  */
    char      szSelected[ST_MAX_PATH];  /* Path selected via `dir`    */
    st_bool_t bRunning;                 /* ST_FALSE requests shutdown */
} line_context_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * line_init() - Initialise the console subsystem.
 *
 * Clears the context and captures the current working directory.
 *
 * Parameters:
 *   ptCtx [out] : Context structure to initialise (must not be NULL).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptCtx is NULL or getcwd() fails.
 */
st_error_t line_init(line_context_t *ptCtx);

/*
 * line_run() - Start the blocking interactive console loop.
 *
 * Returns when the user issues `quit`, signals EOF, or a fatal
 * error occurs.
 *
 * Parameters:
 *   ptCtx [in/out] : Initialised console context.
 *
 * Returns:
 *   ST_NO_ERROR on normal exit.
 *   ST_ERROR    on a fatal internal error.
 */
st_error_t line_run(line_context_t *ptCtx);

/*
 * line_shutdown() - Release console resources.
 *
 * Parameters:
 *   ptCtx [in/out] : Console context (zeroed on return).
 *
 * Returns:
 *   ST_NO_ERROR.
 */
st_error_t line_shutdown(line_context_t *ptCtx);

/*
 * line_print_msg() - Print a normal response message to stdout.
 *
 * Parameters:
 *   szFmt [in] : printf-style format string.
 *   ...        : Format arguments.
 */
void line_print_msg(const char *szFmt, ...);

/*
 * line_print_warning() - Print a warning message (yellow) to stdout.
 *
 * Parameters:
 *   szFmt [in] : printf-style format string.
 *   ...        : Format arguments.
 */
void line_print_warning(const char *szFmt, ...);

/*
 * line_print_error() - Print an error message (red) to stdout.
 *
 * Parameters:
 *   szFmt [in] : printf-style format string.
 *   ...        : Format arguments.
 */
void line_print_error(const char *szFmt, ...);

#endif /* LINE_H */