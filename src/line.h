/*
 * line.h - ST4Ever console line reader and command dispatcher
 *
 * Manages the interactive console loop: displays the prompt,
 * reads user input, parses the command and its arguments,
 * and dispatches to the appropriate command handler.
 *
 * Command syntax:
 *   <command> [arg1 [arg2 ...]]
 *   ^-- first word only, matched against the command table.
 *              ^-- tab-completed against files/dirs (UC4.3).
 *
 * Rich line editor (UC4.2/UC4.3):
 *   - cursor      : LEFT/RIGHT/Home/End
 *   - deletion    : Backspace/Delete
 *   - history     : UP/DOWN circular ring, persisted in
 *                   ~/.st4ever_history between sessions
 *   - completion  : TAB cycles commands (1st word) or paths
 *   - ghost text  : dim completion suffix shown at cursor
 *   - prompt      : contextual — shows trace and selection state
 *   - colors      : ANSI output toggled by `colors on/off`
 *   - script      : non-interactive batch mode via --script file
 *
 * CTRL conflict notes (UC4.3 / UC24E):
 *   CTRL+I = 0x09 = TAB — TAB always drives tab-completion;
 *     CMD_IMAGE is accessible only via "i" / "image".
 *   CTRL+M = 0x0D = CR  — CR always drives ENTER/commit;
 *     CMD_MOUNT also via CTRL+O (0x0F, UC24E P52).
 *   CTRL+U removed (UC24E P55 — umount command deleted).
 */

#ifndef LINE_H
#define LINE_H

#include "common.h"
#include "dir.h"
#include "mount.h"
#include "edit_txt.h"
#include "edit_hex.h"

/* ------------------------------------------------------------------
 * Command identifiers
 * ------------------------------------------------------------------ */

typedef enum cmd_id_s
{
    CMD_UNKNOWN  = -1,  /* Unrecognised command token                */
    CMD_NONE     =  0,  /* Empty input line                          */
    CMD_HELP,           /* h | help    | CTRL+H                      */
    CMD_QUIT,           /* q | quit    | CTRL+Q / CTRL+C             */
    CMD_DIR,            /* d | dir     | CTRL+D                      */
    CMD_LOAD,           /* l | load    | CTRL+L                      */
    CMD_EDIT,           /* e | edit    | CTRL+E                      */
    CMD_IMAGE,          /* i | image   (no CTRL+I — TAB reserved)    */
    CMD_MOUNT,          /* m | mount   (no CTRL+M — ENTER reserved)  */
                        /*   also CTRL+O (UC24E P52)                 */
    CMD_WHERE,          /* w | where   | CTRL+W                      */
    CMD_TRACE,          /* t | trace   | CTRL+T                      */
    CMD_EXECUTE,        /* x | execute | CTRL+X                      */
    CMD_COLORS,         /* c | colors  on|off (toggle ANSI output)   */
    CMD_INFO,           /* n | info      (application dashboard)     */
    CMD_HISTORY,        /* y | history [N]                           */
    CMD_ST2MSA,         /* s | st2msa  [--dir p] [--all] [-r]        */
    CMD_MSA2ST,         /* a | msa2st  [--dir p] [--all] [-r]        */
    CMD_SCRIPT,         /* r | script  <file>                        */
    CMD_COUNT           /* Sentinel - must be last                   */
} cmd_id_t;

/* ------------------------------------------------------------------
 * Parsed command
 * ------------------------------------------------------------------ */

#define LINE_MAX_ARGS     8    /* Maximum arguments per command        */
#define LINE_HISTORY_MAX  64   /* Maximum history ring entries         */
#define LINE_COMPLETE_MAX 32   /* Maximum completion candidates        */

typedef struct parsed_cmd_s
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
typedef struct line_context_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */
    
    char        szCwd[ST_MAX_PATH];        /* Current working dir       */
    char        szSelected[ST_MAX_PATH];   /* Path selected via `dir`   */
    st_mutex_t *ptSelectedMutex;           /* Protects szSelected       */
    st_bool_t   bColors;                   /* Console shows ANSI colors */
    st_bool_t   bRunning;                  /* ST_FALSE = shutdown       */

    dir_view_t      *ptDirView;     /* Pointer to the dir view   */
    edit_txt_view_t *ptEditTxtView; /* Pointer to the text view  */
    edit_hex_view_t *ptEditHexView; /* Pointer to the hex view   */
    mount_view_t    *ptMountView;   /* Pointer to the monut view */

    char aHistory[LINE_HISTORY_MAX][ST_MAX_CMD];
    int  iHistCount;   /* valid entries, 0..LINE_HISTORY_MAX */
    int  iHistHead;    /* next write slot                    */

    char        szScriptFile[ST_MAX_PATH]; /* --script path (UC4.3)     */
    st_bool_t   bDebugSteps;               /* line by line script mode  */
    FILE       *pfScript;                  /* Script file pointer       */
    int         iScriptLine;               /* Current script line read  */
} line_context_t;

/* ------------------------------------------------------------------
 * API — lifecycle
 * ------------------------------------------------------------------ */

/*
 * line_init() - Initialise the console subsystem.
 *
 * Clears the context, captures cwd, creates ptSelectedMutex, and
 * loads ~/.st4ever_history.
 *
 * Parameters:
 *   szScriptFile [in] : file name of a script to execute
 *
 * Returns:
 *   Value of the global line_context_t structure pointer on success.
 *   ST_ERROR    if getcwd() fails.
 */
st_u64_t line_init(const char* szScriptFile);

/*
 * line_run() - Start the blocking interactive console loop.
 *
 * If g_line_ptCtx.szScriptFile is non-empty, reads commands from that file
 * and returns when exhausted (batch mode, non-interactive).
 * Otherwise enters the interactive raw-mode loop; returns on `quit`,
 * EOF, or fatal error.
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   ST_NO_ERROR on normal exit.
 *   ST_ERROR    on fatal internal error.
 */
st_error_t line_run();

/*
 * line_shutdown() - Release console resources.
 *
 * Saves ~/.st4ever_history, closes any open dir view, destroys the
 * ptSelectedMutex, and zeroes the context.
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   ST_NO_ERROR always.
 */
st_error_t line_shutdown();

/* ------------------------------------------------------------------
 * API — console output
 * ------------------------------------------------------------------ */

void line_print_msg(const char *szFmt, ...);
void line_print_warning(const char *szFmt, ...);
void line_print_error(const char *szFmt, ...);

/* ------------------------------------------------------------------
 * API — selected path accessor (thread-safe)
 * ------------------------------------------------------------------ */

/*
 * line_set_selected() - Update szSelected under mutex.
 *
 * Called from the dir-view window thread.  NULL szPath clears the
 * selection.
 *
 * Parameters:
 *   szPath [in]     : Absolute path to record (NULL = clear).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on error
 */
st_error_t line_set_selected(const char     *szPath);

/*
 * line_get_selected() - Read szSelected under mutex.
 *
 * Parameters:
 *   szBuf  [out] : Receives selected path (empty string if none).
 *   uiLen  [in]  : Buffer size (>= 1).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if szBuf is NULL, or uiLen == 0.
 */
st_error_t line_get_selected(char                 *szBuf,
                               size_t                uiLen);

/* ------------------------------------------------------------------
 * API — command history
 * ------------------------------------------------------------------ */

/*
 * line_history_add() - Push an entry onto the history ring.
 *
 * Empty strings and duplicates of the previous entry are ignored.
 *
 * Parameters:
 *   szCmd [in] : Command string.
 *
 * Returns:
 *   ST_NO_ERROR always.
 *   ST_ERROR    if szCmd is NULL.
 */
st_error_t line_history_add(const char *szCmd);

/*
 * line_history_count() - Number of stored history entries (0..MAX).
 */
int line_history_count(void);

/*
 * line_history_get() - Retrieve a history entry by virtual index.
 *
 * Virtual 0 = oldest, count-1 = most recent.
 *
 * Parameters:
 *   iVirtIdx [in]  : 0..count-1.
 *   szBuf    [out] : Receives entry string.
 *   uiLen    [in]  : Buffer size.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if szBuf is NULL, uiLen==0, or iVirtIdx out of range.
 */
st_error_t line_history_get(int iVirtIdx, char *szBuf, size_t uiLen);

/*
 * line_history_clear() - Remove all history entries.
 */
void line_history_clear(void);

/*
 * line_history_save() - Write history to a file.
 *
 * Parameters:
 *   szPath [in] : File path (NULL = default ~/.st4ever_history).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if the file cannot be written.
 */
st_error_t line_history_save(const char *szPath);

/*
 * line_history_load() - Load history from a file.
 *
 * A missing file is treated as an empty history (ST_NO_ERROR).
 *
 * Parameters:
 *   szPath [in] : File path (NULL = default ~/.st4ever_history).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on read error.
 */
st_error_t line_history_load(const char *szPath);

/* ------------------------------------------------------------------
 * API — tab-completion query (testable without raw terminal)
 * ------------------------------------------------------------------ */

/*
 * line_complete_cmd_query() - Find commands matching a prefix.
 *
 * Matches against full command names (e.g. "help", "quit") and
 * their single-letter shortcuts.
 *
 * Parameters:
 *   szPrefix [in]  : Prefix to match (empty string matches all).
 *   aOut     [out] : Pointer to array of ST_MAX_PATH-char buffers.
 *   iMaxOut  [in]  : Capacity of aOut.
 *
 * Returns:
 *   Number of matches (0..iMaxOut).  -1 on invalid parameters.
 */
int line_complete_cmd_query(const char *szPrefix,
                             char       (*aOut)[ST_MAX_PATH],
                             int         iMaxOut);

/*
 * line_complete_path_query() - Find filesystem entries matching a
 * partial path prefix.
 *
 * Directories get a trailing '/'.  Relative paths are resolved
 * against szCwd.
 *
 * Parameters:
 *   szPrefix [in]  : Partial path (e.g. "src/li").
 *   szCwd    [in]  : Current working directory for relative paths.
 *   aOut     [out] : Receives matching full paths.
 *   iMaxOut  [in]  : Capacity of aOut.
 *
 * Returns:
 *   Number of matches (0..iMaxOut).  -1 on invalid parameters.
 */
int line_complete_path_query(const char *szPrefix,
                              const char *szCwd,
                              char       (*aOut)[ST_MAX_PATH],
                              int         iMaxOut);

/* ------------------------------------------------------------------
 * API — console title
 * ------------------------------------------------------------------ */

/*
 * line_update_console_title() - Refresh the terminal window title.
 *
 * Format: "ST4Ever vX.Y.Z | <cwd> [| sel: <file>] [| T]"
 * On Windows: SetConsoleTitleA(); on Linux: ANSI OSC 0 escape.
 *
 * Parameters:
 *   None
 */
st_error_t line_update_console_title();

/* ------------------------------------------------------------------
 * API — ANSI colour toggle
 * ------------------------------------------------------------------ */

/*
 * line_set_colors() - Enable or disable ANSI colour sequences.
 *
 * Parameters:
 *   bColors [in] : ST_TRUE = on (default), ST_FALSE = off.
 */
void line_set_colors(st_bool_t bColors);

/*
 * line_get_colors() - Return the current colour output state.
 */
st_bool_t line_get_colors(void);

char* line_get_current_dir();

void line_enable_script_debug(); 
void line_disable_script_debug();

#endif /* LINE_H */
