/*
 * line.c - ST4Ever console line reader and command dispatcher
 *
 * UC1: basic fgets() input loop with help, quit and trace handlers.
 *      All other commands dispatch to line_cmd_stub() which logs
 *      LOG_TODO and prints a "not yet implemented" message.
 * UC2: full trace on/off/toggle logic in line_cmd_trace().
 * UC4.2: rich line editor via console_set_raw() + console_read_key().
 *        line_read_rich() replaces fgets(); falls back to fgets() when
 *        stdin is not a TTY (CI, piped input).
 *        CTRL shortcuts dispatch commands without ENTER.
 *        ←/→/Home/End editing, Backspace/Delete.
 * TODO(UC4.3): history ↑↓ + tab-completion (command / path) +
 *              ghost text (dim colour, first-word = command,
 *              further words = file/dir completion).
 */

#include "line.h"
#include "console.h"
#include "dir.h"
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
 * ------------------------------------------------------------------ */

#define CON_RESET   "\033[0m"
#define CON_BOLD    "\033[1m"
#define CON_DIM     "\033[2m"    /* Dim/halfline (UC4.3 ghost text)    */
#define CON_GREEN   "\033[92m"
#define CON_YELLOW  "\033[93m"
#define CON_RED     "\033[91m"
#define CON_CYAN    "\033[96m"
#define CON_GRAY    "\033[90m"

/* ------------------------------------------------------------------
 * Module-level state
 * ------------------------------------------------------------------ */

static dir_view_t *g_line_ptDirView = NULL;

/* ------------------------------------------------------------------
 * Prompt string and visible prompt length
 * ------------------------------------------------------------------ */

#define LINE_PROMPT         CON_GREEN ST_APP_NAME CON_RESET "> "
/* Visible characters printed by LINE_PROMPT (without ANSI codes) */
#define LINE_PROMPT_VIS_LEN (sizeof(ST_APP_NAME) - 1 + 2)   /* "ST4Ever> " */

/* ------------------------------------------------------------------
 * Command table
 * ------------------------------------------------------------------ */

typedef struct cmd_entry_s
{
    const char *szFull;     /* Full command name (e.g. "help")       */
    const char *szShort;    /* Single-letter shortcut (e.g. "h")     */
    cmd_id_t    eId;        /* Matching CMD_* identifier              */
    const char *szSynopsis; /* One-line usage hint for `help`         */
    const char *szHelp;     /* Short description for `help`           */
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
      "dir [-a] [path]",
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

    strncpy(ptParsed->szRaw, szInput, ST_MAX_CMD - 1);
    ptParsed->szRaw[ST_MAX_CMD - 1] = '\0';

    strncpy(ptParsed->szArgBuf, szInput, ST_MAX_CMD - 1);
    ptParsed->szArgBuf[ST_MAX_CMD - 1] = '\0';
    line_trim(ptParsed->szArgBuf);

    if (ptParsed->szArgBuf[0] == '\0')
    {
        ptParsed->eCmd = CMD_NONE;
        return ST_NO_ERROR;
    }

    iArgIdx = 0;
    pToken  = strtok_r(ptParsed->szArgBuf, " \t", &pSaveptr);

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
           "  Arrow keys (\xe2\x86\x90\xe2\x86\x92) move the cursor; "
           "\xe2\x86\x91\xe2\x86\x93 navigate history (UC4.3).\n"
           "  TAB completes commands (1st word) or paths (further words) "
           "[UC4.3].\n"
           CON_RESET "\n");

    LOG_INFO("help displayed");
    return ST_NO_ERROR;
}

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
 * trace off      -> disable LOG_TRACE (view stays open — P19)
 * trace <other>  -> warning, no effect
 * Extra args     -> warning, first arg still processed
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
        /* P19: trace off filters LOG_TRACE only; view stays open. */
        eResult = trace_set_trace_enabled(ST_FALSE);
        if (eResult != ST_NO_ERROR)
        {
            line_print_error("trace off: disable LOG_TRACE failed");
            return ST_ERROR;
        }
        line_print_msg("Trace: LOG_TRACE filtered  "
                       "(use 'trace' to close the view)");
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
 * line_cmd_dir() - Open (or refresh) the directory tree view.
 */
static st_error_t line_cmd_dir(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    const char *szPath;
    st_bool_t   bShowHidden;
    st_error_t  eResult;
    int         iArg;

    LOG_TRACE("ptParsed=%p ptCtx=%p", (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    bShowHidden = ST_FALSE;
    szPath      = NULL;

    for (iArg = 1; iArg < ptParsed->iArgc; iArg++)
    {
        if (strcmp(ptParsed->aszArgv[iArg], "-a") == 0)
        {
            bShowHidden = ST_TRUE;
        }
        else if (szPath == NULL)
        {
            szPath = ptParsed->aszArgv[iArg];
        }
        else
        {
            line_print_warning("dir: ignoring extra argument '%s'",
                               ptParsed->aszArgv[iArg]);
        }
    }

    if (g_line_ptDirView != NULL)
    {
        eResult = dir_close(&g_line_ptDirView);
        if (eResult != ST_NO_ERROR)
        {
            line_print_warning("dir: could not close previous view");
        }
        g_line_ptDirView = NULL;
    }

    eResult = dir_open(szPath, ptCtx, bShowHidden, &g_line_ptDirView);
    if (eResult != ST_NO_ERROR)
    {
        line_print_error("dir: failed to open view for '%s'",
                         szPath ? szPath : "(cwd)");
        return ST_ERROR;
    }

    line_print_msg("Directory view opened%s%s%s.",
                   szPath ? " for " : "",
                   szPath ? szPath  : "",
                   bShowHidden ? " (showing hidden files)" : "");
    return ST_NO_ERROR;
}

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

static const cmd_handler_fn g_line_aHandlers[CMD_COUNT] =
{
    /* CMD_NONE    (0) */ NULL,
    /* CMD_HELP       */ line_cmd_help,
    /* CMD_QUIT       */ line_cmd_quit,
    /* CMD_DIR        */ line_cmd_dir,
    /* CMD_LOAD       */ line_cmd_stub,
    /* CMD_EDIT       */ line_cmd_stub,
    /* CMD_IMAGE      */ line_cmd_stub,
    /* CMD_MOUNT      */ line_cmd_stub,
    /* CMD_UMOUNT     */ line_cmd_stub,
    /* CMD_WHERE      */ line_cmd_stub,
    /* CMD_TRACE      */ line_cmd_trace,
    /* CMD_EXECUTE    */ line_cmd_stub,
};

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

    if (ptParsed->eCmd == CMD_NONE)
    {
        return ST_NO_ERROR;
    }

    if (ptParsed->eCmd == CMD_UNKNOWN)
    {
        line_print_warning(
            "Unknown command '%s'.  Type 'help' for a list.",
            ptParsed->iArgc > 0 ? ptParsed->aszArgv[0] : "");
        LOG_INFO("unknown command: '%s'",
                 ptParsed->iArgc > 0 ? ptParsed->aszArgv[0] : "");
        return ST_NO_ERROR;
    }

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
 * Rich line editor (UC4.2)
 * ------------------------------------------------------------------ */

/*
 * line_redraw() - Erase the current terminal line and redraw the
 * prompt + buffer with the cursor at uiCursor.
 *
 * Works with any buffer length and cursor position.
 * TODO(UC4.3): add szGhost parameter for dim completion ghost text.
 */
static void line_redraw(const char *szBuf,
                         size_t      uiLen,
                         size_t      uiCursor)
{
    /* Go to column 0, erase entire line, redraw prompt + buffer */
    printf("\r\033[2K" LINE_PROMPT);
    if (uiLen > 0)
    {
        fwrite(szBuf, 1, uiLen, stdout);
    }
    /* Move cursor left from end-of-buffer to actual insert position */
    if (uiLen > uiCursor)
    {
        printf("\033[%dD", (int)(uiLen - uiCursor));
    }
    fflush(stdout);
}

/*
 * line_shortcut() - Fill szBuf with the shortcut command, redraw the
 * line showing the command as if the user typed it, print a newline,
 * and return ST_NO_ERROR to commit.
 */
static st_error_t line_shortcut(char       *szBuf,
                                  const char *szCmd)
{
    size_t uiLen;

    strncpy(szBuf, szCmd, ST_MAX_CMD - 1);
    szBuf[ST_MAX_CMD - 1] = '\0';
    uiLen = strlen(szBuf);
    line_redraw(szBuf, uiLen, uiLen);
    printf("\n");
    fflush(stdout);
    return ST_NO_ERROR;
}

/*
 * line_read_rich() - Read one command line using the raw line editor.
 *
 * Handles printable character insertion, cursor movement (←/→/Home/
 * End), deletion (Backspace/Delete), ESC to clear, CTRL shortcuts for
 * instant command dispatch, and UP/DOWN as no-ops (UC4.3 history).
 *
 * Parameters:
 *   ptCtx    [in]  : Console context (unused here; reserved for UC4.3)
 *   szBuf    [out] : Receives the committed line (null-terminated).
 *                    Empty string on ESC-then-ENTER or CTRL+C cancel.
 *
 * Returns:
 *   ST_NO_ERROR  line committed; szBuf contains the command string.
 *   ST_ERROR     stdin closed (EOF) or fatal read error.
 */
static st_error_t line_read_rich(line_context_t *ptCtx,
                                  char           *szBuf)
{
    size_t     uiLen;
    size_t     uiCursor;
    int        iKey;
    st_error_t eResult;

    ST_UNUSED(ptCtx);

    uiLen    = 0;
    uiCursor = 0;
    memset(szBuf, 0, ST_MAX_CMD);

    /* Show the initial empty prompt */
    line_redraw(szBuf, uiLen, uiCursor);

    for (;;)
    {
        eResult = console_read_key(&iKey);
        if (eResult != ST_NO_ERROR || iKey == CON_KEY_EOF)
        {
            printf("\n");
            fflush(stdout);
            return ST_ERROR;
        }

        /* ---- ENTER -------------------------------------------- */
        if (iKey == CON_KEY_ENTER || iKey == '\n')
        {
            printf("\n");
            fflush(stdout);
            return ST_NO_ERROR;
        }

        /* ---- ESC: clear buffer --------------------------------- */
        if (iKey == CON_KEY_ESC)
        {
            uiLen    = 0;
            uiCursor = 0;
            memset(szBuf, 0, ST_MAX_CMD);
            line_redraw(szBuf, uiLen, uiCursor);
            continue;
        }

        /* ---- CTRL+C: cancel or quit if buffer empty ------------ */
        if (iKey == CON_KEY_CTRL_C)
        {
            if (uiLen > 0)
            {
                uiLen    = 0;
                uiCursor = 0;
                memset(szBuf, 0, ST_MAX_CMD);
                line_redraw(szBuf, uiLen, uiCursor);
            }
            else
            {
                return line_shortcut(szBuf, "quit");
            }
            continue;
        }

        /* ---- CTRL shortcuts: instant dispatch ------------------ */
        if (iKey == CON_KEY_CTRL_Q)
        {
            return line_shortcut(szBuf, "quit");
        }
        if (iKey == CON_KEY_CTRL_H)
        {
            /* CON_KEY_CTRL_H = 0x08.
             * On mintty BACKSPACE sends 0x7F so 0x08 is always CTRL+H.
             * On terminals where BACKSPACE sends 0x08, console_read_key()
             * normalises it to CON_KEY_BACKSPACE (0x7F) so 0x08 still
             * reaches here as the help shortcut. */
            return line_shortcut(szBuf, "help");
        }
        if (iKey == CON_KEY_CTRL_T)
        {
            return line_shortcut(szBuf, "trace");
        }
        if (iKey == CON_KEY_CTRL_D)
        {
            return line_shortcut(szBuf, "dir");
        }
        if (iKey == CON_KEY_CTRL_L)
        {
            return line_shortcut(szBuf, "load");
        }
        if (iKey == CON_KEY_CTRL_E)
        {
            return line_shortcut(szBuf, "edit");
        }
        if (iKey == CON_KEY_CTRL_U)
        {
            return line_shortcut(szBuf, "umount");
        }
        if (iKey == CON_KEY_CTRL_W)
        {
            return line_shortcut(szBuf, "where");
        }
        if (iKey == CON_KEY_CTRL_X)
        {
            return line_shortcut(szBuf, "execute");
        }

        /* ---- TAB: no-op until UC4.3 tab-completion ------------- */
        if (iKey == '\t')
        {
            /* TODO(UC4.3): command completion on 1st word,
             * file/dir completion on further words. Ghost text in
             * CON_DIM colour shows the completion candidate. */
            continue;
        }

        /* ---- History: no-op until UC4.3 ------------------------ */
        if (iKey == CON_KEY_UP || iKey == CON_KEY_DOWN)
        {
            /* TODO(UC4.3): circular history buffer navigation */
            continue;
        }

        /* ---- Cursor movement ----------------------------------- */
        if (iKey == CON_KEY_LEFT)
        {
            if (uiCursor > 0)
            {
                uiCursor--;
                line_redraw(szBuf, uiLen, uiCursor);
            }
            continue;
        }
        if (iKey == CON_KEY_RIGHT)
        {
            if (uiCursor < uiLen)
            {
                uiCursor++;
                line_redraw(szBuf, uiLen, uiCursor);
            }
            continue;
        }
        if (iKey == CON_KEY_HOME)
        {
            uiCursor = 0;
            line_redraw(szBuf, uiLen, uiCursor);
            continue;
        }
        if (iKey == CON_KEY_END)
        {
            uiCursor = uiLen;
            line_redraw(szBuf, uiLen, uiCursor);
            continue;
        }

        /* ---- Deletion ------------------------------------------ */
        if (iKey == CON_KEY_BACKSPACE)
        {
            if (uiCursor > 0)
            {
                memmove(szBuf + uiCursor - 1,
                        szBuf + uiCursor,
                        uiLen - uiCursor + 1);
                uiCursor--;
                uiLen--;
                line_redraw(szBuf, uiLen, uiCursor);
            }
            continue;
        }
        if (iKey == CON_KEY_DELETE)
        {
            if (uiCursor < uiLen)
            {
                memmove(szBuf + uiCursor,
                        szBuf + uiCursor + 1,
                        uiLen - uiCursor);
                uiLen--;
                szBuf[uiLen] = '\0';
                line_redraw(szBuf, uiLen, uiCursor);
            }
            continue;
        }

        /* ---- Printable character: insert at cursor ------------- */
        if (iKey >= 0x20 && iKey <= 0x7E)
        {
            if (uiLen < ST_MAX_CMD - 1)
            {
                if (uiCursor < uiLen)
                {
                    memmove(szBuf + uiCursor + 1,
                            szBuf + uiCursor,
                            uiLen - uiCursor + 1);
                }
                szBuf[uiCursor] = (char)iKey;
                uiCursor++;
                uiLen++;
                szBuf[uiLen] = '\0';
                line_redraw(szBuf, uiLen, uiCursor);
            }
            continue;
        }

        /* All other keys: ignore silently */
    }
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
    st_bool_t    bRawMode;
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

    /* -- Switch to raw mode for the rich line editor --------------- */
    bRawMode = ST_FALSE;
    eResult  = console_set_raw();
    if (eResult == ST_NO_ERROR)
    {
        bRawMode = ST_TRUE;
    }
    else
    {
        /* Non-TTY context (CI, piped input): fall back to fgets() */
        LOG_INFO("stdin not a TTY - using plain fgets() input");
    }

    /* -- Main loop ------------------------------------------------- */
    while (ptCtx->bRunning == ST_TRUE)
    {
        if (bRawMode == ST_TRUE)
        {
            eResult = line_read_rich(ptCtx, szInput);
            if (eResult != ST_NO_ERROR)
            {
                LOG_INFO("stdin EOF - requesting shutdown");
                ptCtx->bRunning = ST_FALSE;
                break;
            }
        }
        else
        {
            /* Plain fallback (non-TTY: piped input, automated tests) */
            printf(LINE_PROMPT);
            fflush(stdout);

            if (fgets(szInput, sizeof(szInput), stdin) == NULL)
            {
                printf("\n");
                LOG_INFO("stdin EOF - requesting shutdown");
                ptCtx->bRunning = ST_FALSE;
                break;
            }

            uiLen = strlen(szInput);
            if (uiLen > 0 && szInput[uiLen - 1] == '\n')
            {
                szInput[--uiLen] = '\0';
            }
            if (uiLen > 0 && szInput[uiLen - 1] == '\r')
            {
                szInput[--uiLen] = '\0';
            }
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

    /* -- Restore terminal mode ------------------------------------ */
    if (bRawMode == ST_TRUE)
    {
        eResult = console_restore();
        if (eResult != ST_NO_ERROR)
        {
            LOG_ERROR("console_restore() failed");
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

    if (g_line_ptDirView != NULL)
    {
        dir_close(&g_line_ptDirView);
        g_line_ptDirView = NULL;
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
