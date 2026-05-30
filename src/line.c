/*
 * line.c - ST4Ever console line reader and command dispatcher
 *
 * UC1: basic fgets() input loop with help, quit and trace handlers.
 *      All other commands dispatch to line_cmd_stub() which logs
 *      LOG_TODO and prints a "not yet implemented" message.
 * UC2: full trace on/off/toggle logic in line_cmd_trace().
 *
 * TODO(UC4): Replace fgets() with the rich line editor
 *            (history, tab-completion, ghost-text pre-completion,
 *             arrow-key navigation via win_console / lx_console).
 */

#include "line.h"
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef ST_PLATFORM_WINDOWS
    #include <direct.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
#endif

/* ------------------------------------------------------------------
 * ANSI colour codes
 * (work in MSYS2/mintty and standard Linux terminals)
 * ------------------------------------------------------------------ */

#define CON_RESET   "\033[0m"
#define CON_BOLD    "\033[1m"
#define CON_GREEN   "\033[92m"
#define CON_YELLOW  "\033[93m"
#define CON_RED     "\033[91m"
#define CON_CYAN    "\033[96m"
#define CON_GRAY    "\033[90m"

/* ------------------------------------------------------------------
 * Prompt string
 * ------------------------------------------------------------------ */

#define LINE_PROMPT  CON_GREEN ST_APP_NAME CON_RESET "> "

/* ------------------------------------------------------------------
 * Command table
 * ------------------------------------------------------------------ */

typedef struct
{
    const char *szFull;    /* Full command name (e.g. "help")       */
    const char *szShort;   /* Single-letter shortcut (e.g. "h")     */
    cmd_id_t    eId;       /* Matching CMD_* identifier              */
    const char *szSynopsis;/* One-line usage hint for `help`         */
    const char *szHelp;    /* Short description for `help`           */
} cmd_entry_t;

static const cmd_entry_t g_line_aCmds[] =
{
    { "help",    "h", CMD_HELP,
      "help",
      "Show this help message" },

    { "quit",    "q", CMD_QUIT,
      "quit",
      "Close all views and exit ST4Ever" },

    { "dir",     "d", CMD_DIR,
      "dir [path]",
      "Open a directory tree view (defaults to current dir)" },

    { "load",    "l", CMD_LOAD,
      "load [file]",
      "Load a file into the emulated ST memory" },

    { "edit",    "e", CMD_EDIT,
      "edit [file]",
      "Open a file in the text or hex editor" },

    { "image",   "i", CMD_IMAGE,
      "image [path]",
      "Create a .st or .msa disk image from a directory" },

    { "mount",   "m", CMD_MOUNT,
      "mount [path]",
      "Mount a directory as the ST floppy drive A:\\" },

    { "umount",  "u", CMD_UMOUNT,
      "umount",
      "Unmount the ST floppy A:\\ (optionally save image)" },

    { "where",   "w", CMD_WHERE,
      "where",
      "Show current working directory and selected file" },

    { "trace",   "t", CMD_TRACE,
      "trace [on|off]",
      "Toggle / open / close the trace console" },

    { "execute", "x", CMD_EXECUTE,
      "execute [file]",
      "Execute an Atari ST binary (.PRG .TTP .TOS)" },
};

/* ------------------------------------------------------------------
 * Internal: helpers
 * ------------------------------------------------------------------ */

/*
 * line_trim() - Remove leading and trailing whitespace in place.
 *
 * Parameters:
 *   szStr [in/out] : String to trim; modified in place.
 */
static void line_trim(char *szStr)
{
    char  *pStart;
    char  *pEnd;
    size_t uiLen;

    if (szStr == NULL)
    {
        return;
    }

    pStart = szStr;
    while (*pStart != '\0' && isspace((unsigned char)*pStart))
    {
        pStart++;
    }

    uiLen = strlen(pStart);
    if (uiLen == 0)
    {
        szStr[0] = '\0';
        return;
    }

    pEnd = pStart + uiLen - 1;
    while (pEnd > pStart && isspace((unsigned char)*pEnd))
    {
        pEnd--;
    }
    *(pEnd + 1) = '\0';

    if (pStart != szStr)
    {
        memmove(szStr, pStart, (size_t)(pEnd - pStart + 2));
    }
}

/*
 * line_parse_cmd() - Tokenise szInput into a parsed_cmd_t.
 *
 * The first token is matched against the command table.  Subsequent
 * tokens become argv[1..N].  All pointers inside ptParsed->aszArgv
 * point into ptParsed->szArgBuf.
 *
 * Parameters:
 *   szInput  [in]  : Raw console input (not modified).
 *   ptParsed [out] : Receives the parsed result.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if szInput or ptParsed is NULL.
 */
static st_error_t line_parse_cmd(const char   *szInput,
                                  parsed_cmd_t *ptParsed)
{
    char      *pToken;
    char      *pSaveptr;
    int        iArgIdx;
    size_t     uiCmdCount;
    size_t     uiIdx;
    st_bool_t  bFound;

    if (szInput == NULL || ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter: szInput=%p ptParsed=%p",
                  (void *)szInput, (void *)ptParsed);
        return ST_ERROR;
    }

    memset(ptParsed, 0, sizeof(parsed_cmd_t));
    ptParsed->eCmd = CMD_UNKNOWN;

    /* Keep verbatim copy */
    strncpy(ptParsed->szRaw, szInput, ST_MAX_CMD - 1);
    ptParsed->szRaw[ST_MAX_CMD - 1] = '\0';

    /* Working copy for tokenisation */
    strncpy(ptParsed->szArgBuf, szInput, ST_MAX_CMD - 1);
    ptParsed->szArgBuf[ST_MAX_CMD - 1] = '\0';
    line_trim(ptParsed->szArgBuf);

    if (ptParsed->szArgBuf[0] == '\0')
    {
        ptParsed->eCmd = CMD_NONE;
        return ST_NO_ERROR;
    }

    /* Tokenise */
    iArgIdx  = 0;
    pToken   = strtok_r(ptParsed->szArgBuf, " \t", &pSaveptr);

    while (pToken != NULL && iArgIdx < LINE_MAX_ARGS)
    {
        ptParsed->aszArgv[iArgIdx] = pToken;
        iArgIdx++;
        pToken = strtok_r(NULL, " \t", &pSaveptr);
    }
    ptParsed->iArgc = iArgIdx;

    if (iArgIdx == 0)
    {
        ptParsed->eCmd = CMD_NONE;
        return ST_NO_ERROR;
    }

    /* Match argv[0] against command table */
    uiCmdCount = ST_ARRAY_SIZE(g_line_aCmds);
    bFound     = ST_FALSE;

    for (uiIdx = 0; uiIdx < uiCmdCount; uiIdx++)
    {
        const cmd_entry_t *ptEntry;
        ptEntry = &g_line_aCmds[uiIdx];

        if (strcmp(ptParsed->aszArgv[0], ptEntry->szFull)  == 0
        ||  strcmp(ptParsed->aszArgv[0], ptEntry->szShort) == 0)
        {
            ptParsed->eCmd = ptEntry->eId;
            bFound = ST_TRUE;
            break;
        }
    }

    if (bFound == ST_FALSE)
    {
        ptParsed->eCmd = CMD_UNKNOWN;
    }

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Command handlers
 * ------------------------------------------------------------------ */

/*
 * line_cmd_help() - Display the command reference table.
 *
 * Parameters:
 *   ptParsed [in] : Parsed command (extra args trigger a warning).
 *   ptCtx    [in] : Console context (unused).
 *
 * Returns:
 *   ST_NO_ERROR.
 */
static st_error_t line_cmd_help(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    size_t uiCmdCount;
    size_t uiIdx;

    LOG_TRACE("ptParsed=%p ptCtx=%p", (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    if (ptParsed->iArgc > 1)
    {
        line_print_warning("help: ignoring extra arguments");
    }

    uiCmdCount = ST_ARRAY_SIZE(g_line_aCmds);

    printf("\n");
    printf(CON_BOLD CON_CYAN
           "  %s %s  -  %s\n"
           CON_RESET "\n",
           ST_APP_NAME, ST_APP_VERSION, ST_APP_DESC);

    printf("  %-12s %-4s  %-22s  %s\n",
           "Command", "Key", "Usage", "Description");
    printf("  %-12s %-4s  %-22s  %s\n",
           "------------", "----",
           "----------------------",
           "-------------------------------------");

    for (uiIdx = 0; uiIdx < uiCmdCount; uiIdx++)
    {
        const cmd_entry_t *ptEntry;
        ptEntry = &g_line_aCmds[uiIdx];
        printf("  " CON_GREEN "%-12s" CON_RESET
               " %-4s  " CON_GRAY "%-22s" CON_RESET "  %s\n",
               ptEntry->szFull,
               ptEntry->szShort,
               ptEntry->szSynopsis,
               ptEntry->szHelp);
    }

    printf("\n");
    printf("  " CON_GRAY
           "CTRL+<key> shortcuts available for all commands.\n"
           "  Arrow keys (history) and Tab (completion) coming in UC4.\n"
           CON_RESET "\n");

    LOG_INFO("help displayed");
    return ST_NO_ERROR;
}

/*
 * line_cmd_quit() - Request application shutdown.
 *
 * Sets ptCtx->bRunning = ST_FALSE to exit the console loop.
 *
 * Parameters:
 *   ptParsed [in]  : Parsed command (extra args trigger a warning).
 *   ptCtx    [out] : Console context.
 *
 * Returns:
 *   ST_NO_ERROR.
 */
static st_error_t line_cmd_quit(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    LOG_TRACE("ptParsed=%p ptCtx=%p", (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    if (ptParsed->iArgc > 1)
    {
        line_print_warning("quit: ignoring extra arguments");
    }

    line_print_msg("Goodbye. The Atari ST lives on...");
    ptCtx->bRunning = ST_FALSE;

    LOG_INFO("quit requested - bRunning set to ST_FALSE");
    return ST_NO_ERROR;
}

/*
 * line_cmd_trace() - Open/close/toggle the trace console.
 *
 * trace          -> toggle open/closed (trace_enabled state unchanged)
 * trace on       -> open + enable LOG_TRACE
 * trace off      -> disable LOG_TRACE + close
 * trace <other>  -> warning, no effect
 * Extra args     -> warning, first arg still processed
 *
 * Parameters:
 *   ptParsed [in] : Parsed command.
 *   ptCtx    [in] : Console context.
 *
 * Returns:
 *   ST_NO_ERROR.
 */
static st_error_t line_cmd_trace(const parsed_cmd_t *ptParsed,
                                  line_context_t     *ptCtx)
{
    st_error_t  eResult;
    const char *szArg;

    LOG_TRACE("ptParsed=%p ptCtx=%p", (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* No argument: toggle */
    if (ptParsed->iArgc == 1)
    {
        if (trace_is_open() == ST_TRUE)
        {
            eResult = trace_close();
            if (eResult != ST_NO_ERROR)
            {
                line_print_error("trace: close failed");
                return ST_ERROR;
            }
            line_print_msg("Trace console closed.");
        }
        else
        {
            eResult = trace_open();
            if (eResult != ST_NO_ERROR)
            {
                line_print_error("trace: open failed");
                return ST_ERROR;
            }
            line_print_msg("Trace console opened.");
        }
        return ST_NO_ERROR;
    }

    /* With argument */
    szArg = ptParsed->aszArgv[1];

    if (ptParsed->iArgc > 2)
    {
        line_print_warning(
            "trace: ignoring extra arguments after '%s'", szArg);
    }

    if (strcmp(szArg, "on") == 0)
    {
        eResult = trace_open();
        if (eResult != ST_NO_ERROR)
        {
            line_print_error("trace on: open failed");
            return ST_ERROR;
        }
        eResult = trace_set_trace_enabled(ST_TRUE);
        if (eResult != ST_NO_ERROR)
        {
            line_print_error("trace on: enable LOG_TRACE failed");
            return ST_ERROR;
        }
        line_print_msg("Trace: ON  (LOG_TRACE enabled)");
    }
    else if (strcmp(szArg, "off") == 0)
    {
        eResult = trace_set_trace_enabled(ST_FALSE);
        if (eResult != ST_NO_ERROR)
        {
            line_print_error("trace off: disable LOG_TRACE failed");
            return ST_ERROR;
        }
        eResult = trace_close();
        if (eResult != ST_NO_ERROR)
        {
            line_print_error("trace off: close failed");
            return ST_ERROR;
        }
        line_print_msg("Trace: OFF");
    }
    else
    {
        line_print_warning(
            "trace: unknown argument '%s'  "
            "- use: trace | trace on | trace off",
            szArg);
    }

    return ST_NO_ERROR;
}

/*
 * line_cmd_stub() - Generic stub for commands not yet implemented.
 *
 * Informs the user and logs a LOG_TODO so the stub is visible in
 * the trace.
 *
 * Parameters:
 *   ptParsed [in] : Parsed command.
 *   ptCtx    [in] : Console context.
 *
 * Returns:
 *   ST_NO_ERROR.
 */
static st_error_t line_cmd_stub(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    const char *szCmd;

    LOG_TRACE("ptParsed=%p ptCtx=%p", (void *)ptParsed, (void *)ptCtx);
    ST_UNUSED(ptCtx);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    szCmd = (ptParsed->iArgc > 0) ? ptParsed->aszArgv[0] : "?";

    line_print_warning(
        "'%s' is not yet implemented - coming in a future UC.",
        szCmd);

    LOG_TODO("command '%s' not yet implemented", szCmd);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Dispatch table  (indexed by cmd_id_t, CMD_NONE = 0)
 * ------------------------------------------------------------------ */

typedef st_error_t (*cmd_handler_fn)(const parsed_cmd_t *,
                                      line_context_t *);

/*
 * g_line_aHandlers[] maps each cmd_id_t to its handler function.
 * CMD_UNKNOWN (-1) is handled directly in line_dispatch() before
 * indexing this table.  CMD_NONE (0) maps to NULL.
 */
static const cmd_handler_fn g_line_aHandlers[CMD_COUNT] =
{
    /* CMD_NONE    (0) */ NULL,
    /* CMD_HELP       */ line_cmd_help,
    /* CMD_QUIT       */ line_cmd_quit,
    /* CMD_DIR        */ line_cmd_stub,
    /* CMD_LOAD       */ line_cmd_stub,
    /* CMD_EDIT       */ line_cmd_stub,
    /* CMD_IMAGE      */ line_cmd_stub,
    /* CMD_MOUNT      */ line_cmd_stub,
    /* CMD_UMOUNT     */ line_cmd_stub,
    /* CMD_WHERE      */ line_cmd_stub,
    /* CMD_TRACE      */ line_cmd_trace,
    /* CMD_EXECUTE    */ line_cmd_stub,
};

/*
 * line_dispatch() - Route a parsed command to its handler.
 *
 * Parameters:
 *   ptParsed [in]  : Parsed command to dispatch.
 *   ptCtx    [in]  : Console context passed to handlers.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    on handler failure or invalid command id.
 */
static st_error_t line_dispatch(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    cmd_handler_fn  pfnHandler;
    st_error_t      eResult;

    LOG_TRACE("eCmd=%d argc=%d",
              (int)ptParsed->eCmd, ptParsed->iArgc);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* Empty line: silent no-op */
    if (ptParsed->eCmd == CMD_NONE)
    {
        return ST_NO_ERROR;
    }

    /* Unknown command */
    if (ptParsed->eCmd == CMD_UNKNOWN)
    {
        line_print_warning(
            "Unknown command '%s'.  Type 'help' for a list.",
            ptParsed->iArgc > 0 ? ptParsed->aszArgv[0] : "");
        LOG_INFO("unknown command: '%s'",
                 ptParsed->iArgc > 0 ? ptParsed->aszArgv[0] : "");
        return ST_NO_ERROR;
    }

    /* Bounds check */
    if ((int)ptParsed->eCmd <= 0
    ||  (int)ptParsed->eCmd >= (int)CMD_COUNT)
    {
        LOG_ERROR("command id out of range: %d", (int)ptParsed->eCmd);
        return ST_ERROR;
    }

    pfnHandler = g_line_aHandlers[(int)ptParsed->eCmd];
    if (pfnHandler == NULL)
    {
        LOG_ERROR("NULL handler for command id %d",
                  (int)ptParsed->eCmd);
        return ST_ERROR;
    }

    eResult = pfnHandler(ptParsed, ptCtx);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("handler for '%s' returned ST_ERROR",
                  ptParsed->iArgc > 0 ? ptParsed->aszArgv[0] : "?");
    }

    return eResult;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_error_t line_init(line_context_t *ptCtx)
{
    char *pCwd;

    LOG_TRACE("ptCtx=%p", (void *)ptCtx);

    if (ptCtx == NULL)
    {
        LOG_ERROR("ptCtx is NULL");
        return ST_ERROR;
    }

    memset(ptCtx, 0, sizeof(line_context_t));
    ptCtx->bRunning = ST_TRUE;

    pCwd = getcwd(ptCtx->szCwd, ST_MAX_PATH);
    if (pCwd == NULL)
    {
        LOG_ERROR("getcwd() failed - using '.'");
        strncpy(ptCtx->szCwd, ".", ST_MAX_PATH - 1);
        ptCtx->szCwd[ST_MAX_PATH - 1] = '\0';
    }

    LOG_INFO("console initialised, cwd='%s'", ptCtx->szCwd);
    return ST_NO_ERROR;
}

st_error_t line_run(line_context_t *ptCtx)
{
    char         szInput[ST_MAX_CMD];
    parsed_cmd_t tParsed;
    st_error_t   eResult;
    size_t       uiLen;

    LOG_TRACE("ptCtx=%p", (void *)ptCtx);

    if (ptCtx == NULL)
    {
        LOG_ERROR("ptCtx is NULL");
        return ST_ERROR;
    }

    /* -- Banner ---------------------------------------------------- */
    printf("\n");
    printf(CON_BOLD CON_CYAN
           "  +-----------------------------------------+\n"
           "  |  %-40s|\n"
           "  |  %-40s|\n"
           "  |  %-40s|\n"
           "  +-----------------------------------------+\n"
           CON_RESET "\n",
           ST_APP_NAME " " ST_APP_VERSION,
           ST_APP_DESC,
           "Type 'help' for available commands.");

    LOG_INFO("console loop starting");

    /* -- Main loop ------------------------------------------------- */
    while (ptCtx->bRunning == ST_TRUE)
    {
        /* Display prompt */
        printf(LINE_PROMPT);
        fflush(stdout);

        /* Read one line of input */
        if (fgets(szInput, sizeof(szInput), stdin) == NULL)
        {
            /* EOF (CTRL+D) or read error */
            printf("\n");
            LOG_INFO("stdin EOF - requesting shutdown");
            ptCtx->bRunning = ST_FALSE;
            break;
        }

        /* Strip trailing newline */
        uiLen = strlen(szInput);
        if (uiLen > 0 && szInput[uiLen - 1] == '\n')
        {
            szInput[uiLen - 1] = '\0';
            uiLen--;
        }
        /* Also strip CR (Windows line endings in some terminals) */
        if (uiLen > 0 && szInput[uiLen - 1] == '\r')
        {
            szInput[uiLen - 1] = '\0';
        }

        /* Parse and dispatch */
        eResult = line_parse_cmd(szInput, &tParsed);
        if (eResult != ST_NO_ERROR)
        {
            LOG_ERROR("line_parse_cmd failed for input '%s'", szInput);
            continue;
        }

        eResult = line_dispatch(&tParsed, ptCtx);
        if (eResult != ST_NO_ERROR)
        {
            LOG_ERROR("line_dispatch returned error for '%s'", szInput);
            /* Non-fatal: log and continue */
        }
    }

    LOG_INFO("console loop exited");
    return ST_NO_ERROR;
}

st_error_t line_shutdown(line_context_t *ptCtx)
{
    LOG_TRACE("ptCtx=%p", (void *)ptCtx);

    if (ptCtx == NULL)
    {
        LOG_ERROR("ptCtx is NULL");
        return ST_ERROR;
    }

    memset(ptCtx, 0, sizeof(line_context_t));
    LOG_INFO("console shutdown complete");
    return ST_NO_ERROR;
}

void line_print_msg(const char *szFmt, ...)
{
    va_list vaArgs;

    if (szFmt == NULL)
    {
        return;
    }

    printf("  ");
    va_start(vaArgs, szFmt);
    vprintf(szFmt, vaArgs);
    va_end(vaArgs);
    printf("\n");
}

void line_print_warning(const char *szFmt, ...)
{
    va_list vaArgs;

    if (szFmt == NULL)
    {
        return;
    }

    printf(CON_YELLOW "  Warning: ");
    va_start(vaArgs, szFmt);
    vprintf(szFmt, vaArgs);
    va_end(vaArgs);
    printf(CON_RESET "\n");
}

void line_print_error(const char *szFmt, ...)
{
    va_list vaArgs;

    if (szFmt == NULL)
    {
        return;
    }

    printf(CON_RED "  Error: ");
    va_start(vaArgs, szFmt);
    vprintf(szFmt, vaArgs);
    va_end(vaArgs);
    printf(CON_RESET "\n");
}
