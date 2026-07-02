/*
 * line.c - ST4Ever console line reader and command dispatcher
 *
 * UC1:   fgets() loop, help/quit/trace stubs.
 * UC2:   Full trace on/off/toggle in line_cmd_trace().
 * UC4.2: Rich editor via console_set_raw() + console_read_key().
 *        line_read_rich(): cursor, deletion, CTRL shortcuts, ESC.
 * UC4.3: History UP/DOWN circular ring + ~/.st4ever_history.
 *        TAB completion: commands (1st word) and paths (rest).
 *        Ghost text: dim suffix shown at cursor.
 *        Contextual prompt: [T] and [selection] indicators.
 *        colors on/off: runtime ANSI toggle via c_*() functions.
 *        --script: batch mode via line_exec_script().
 *        Mutex-protected szSelected via line_set/get_selected().
 *        CTRL conflict resolution: TAB reserved for completion
 *        (CMD_IMAGE via "i"/"image" only); ENTER reserved for
 *        commit (CMD_MOUNT via "m"/"mount" only).
 * UC5:   where (cwd + selection), info (dashboard), history [N].
 *        P8:  line_update_console_title() — SetConsoleTitleA/OSC.
 *        P23bis: TAB inserts longest common prefix before cycling.
 *        P24: g_line_bColors = isatty(stdout) at line_init().
 *        trace clear (P27), trace level <lvl> (P28) sub-commands.
 * UC18.1: line_cmd_mount() / line_cmd_umount() + g_line_ptCtx.ptMountView.
 * UC19:   line_cmd_umount() interactive save dialog (P35).
 *         --st / --msa / --dir flags bypass dialog.
 * UC20:   line_cmd_image() — create .st/.msa from mounted content or dir.
 *         P41: ENTER in mount → selected file → edit_hex (mount.c).
 *         P37: 'F' in mount / --bootable flag → mount_make_bootable().
 */

#include "line.h"
#include "console.h"
#include "load.h"
#include "image_msa.h"
#include "file.h"
#include "trace.h"
#include "exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

#ifdef ST_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <io.h>       /* _isatty, _fileno */
    #include <direct.h>
    #include <dirent.h>
    #include <sys/stat.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/stat.h>
#endif

/* ------------------------------------------------------------------
 * The global line context structure
 * ------------------------------------------------------------------ */

static line_context_t  g_line_ptCtx = {.ulMagic = OBJ_MAGIC, 
                                       .eObject = ST_LINE_CTX,
                                       .iHistCount = -1};

/* ------------------------------------------------------------------
 * ANSI colour helpers — runtime-toggled via g_line_bColors
 * ------------------------------------------------------------------ */

static inline const char *c_reset(void)
{
    return g_line_ptCtx.bColors ? "\033[0m"  : "";
}
static inline const char *c_bold(void)
{
    return g_line_ptCtx.bColors ? "\033[1m"  : "";
}
static inline const char *c_dim(void)
{
    return g_line_ptCtx.bColors ? "\033[2m"  : "";
}
static inline const char *c_green(void)
{
    return g_line_ptCtx.bColors ? "\033[92m" : "";
}
static inline const char *c_yellow(void)
{
    return g_line_ptCtx.bColors ? "\033[93m" : "";
}
static inline const char *c_red(void)
{
    return g_line_ptCtx.bColors ? "\033[91m" : "";
}
static inline const char *c_cyan(void)
{
    return g_line_ptCtx.bColors ? "\033[96m" : "";
}
static inline const char *c_gray(void)
{
    return g_line_ptCtx.bColors ? "\033[90m" : "";
}

/* Forward declaration — defined in the string helpers section below */
static const char *line_path_basename(const char *szPath);

/* ------------------------------------------------------------------
 * Console title update (P8)
 * ------------------------------------------------------------------ */

st_error_t line_update_console_title()
{
    char        szTitle[256];
    char        szSel[ST_MAX_PATH];
    const char *pBase;
    const char *pCwd;
    int         iRet;

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }
    
    pCwd      = g_line_ptCtx.szCwd;
    szSel[0]  = '\0';

    line_get_selected(szSel, sizeof(szSel));
    
    if (szSel[0] != '\0')
    {
        pBase = line_path_basename(szSel);
        iRet = snprintf(szTitle, sizeof(szTitle),
                        "%s %s | %s | sel: %s%s",
                        ST_APP_NAME, ST_APP_VERSION, pCwd,
                        pBase ? pBase : szSel,
                        trace_is_open() ? " | [T]" : "");
    }
    else
    {
        iRet = snprintf(szTitle, sizeof(szTitle),
                        "%s %s | %s%s",
                        ST_APP_NAME, ST_APP_VERSION, pCwd,
                        trace_is_open() ? " | [T]" : "");
    }

    if (iRet < 0)
    {
        return ST_ERROR; /* snprintf error */
    }

#ifdef ST_PLATFORM_WINDOWS
    SetConsoleTitleA(szTitle);
#else
    printf("\033]0;%s\007", szTitle);
    fflush(stdout);
#endif

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Module-level state
 * ------------------------------------------------------------------ */


/* BUG-09: dir navigation history persisted across dir command sessions */
static char g_line_aDirNavHist[DIR_NAV_HIST_MAX][ST_MAX_PATH];
static int  g_line_iDirNavHistHead  = -1;
static int  g_line_iDirNavHistCount = 0;

/* ------------------------------------------------------------------
 * History ring buffer
 * ------------------------------------------------------------------ */


/* Map virtual index (0=oldest, count-1=newest) to physical slot. */
static int line_hist_phys(int iVirt)
{
    return (g_line_ptCtx.iHistHead - g_line_ptCtx.iHistCount + iVirt
            + LINE_HISTORY_MAX) % LINE_HISTORY_MAX;
}

/* ------------------------------------------------------------------
 * Command table
 * ------------------------------------------------------------------ */

typedef struct cmd_entry_s
{
    const char *szFull;
    const char *szShort;
    cmd_id_t    eId;
    const char *szSynopsis;
    const char *szHelp;
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
      "Open a directory tree view (defaults to cwd)" },

    { "load",    "l", CMD_LOAD,
      "load [file]",
      "Load a file into emulated ST memory" },

    { "edit",    "e", CMD_EDIT,
      "edit [file]",
      "Open a file in the text or hex editor" },

    { "image",   "i", CMD_IMAGE,
      "image [path]",
      "Create a .st/.msa disk image from a directory" },

    { "mount",   "m", CMD_MOUNT,
      "mount [path]",
      "Mount a directory or .st/.msa image on A:\\" },

    { "where",   "w", CMD_WHERE,
      "where",
      "Show current working directory and selected file" },

    { "trace",   "t", CMD_TRACE,
      "trace [on|off]",
      "Toggle / open / close the trace console" },

    { "execute", "x", CMD_EXECUTE,
      "execute [file]",
      "Execute an Atari ST binary (.PRG .TTP .TOS)" },

    { "colors",  "c", CMD_COLORS,
      "colors [on|off]",
      "Enable or disable ANSI colour output" },

    { "info",    "n", CMD_INFO,
      "info",
      "Show application status dashboard" },

    { "history", "y", CMD_HISTORY,
      "history [N]",
      "Show last N history entries (default 10)" },

    { "st2msa",  "s", CMD_ST2MSA,
      "st2msa [--dir path] [--all] [-r]",
      "Convert .st disk image(s) to .msa format" },

    { "msa2st",  "a", CMD_MSA2ST,
      "msa2st [--dir path] [--all] [-r]",
      "Convert .msa disk image(s) to .st format" },

    { "script",  "r", CMD_SCRIPT,
      "script <file>",
      "Run commands from a script file" },
};

/* ------------------------------------------------------------------
 * Internal: string helpers
 * ------------------------------------------------------------------ */

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

/* line_path_basename() - pointer to last path component. */
static const char *line_path_basename(const char *szPath)
{
    const char *p;
    const char *pLast;

    if (szPath == NULL || szPath[0] == '\0')
    {
        return szPath;
    }

    pLast = szPath;
    for (p = szPath; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            pLast = p + 1;
        }
    }
    return pLast;
}

/* ------------------------------------------------------------------
 * Internal: command tokenizer (backslash-escape aware)
 * ------------------------------------------------------------------ */

/* Split szArgBuf in place on unescaped whitespace.  Recognized escape
 * sequences (\<space>, \(, \)) are collapsed: the backslash is removed and
 * the next char is kept literally.  Other backslash combos are left as-is.
 *
 * *ppPtr must initially point to the start of the buffer.  Each call
 * advances it past the found token (and its terminating NUL/space).
 * Returns NULL when no more tokens remain. */
static char *line_next_token_unescape(char **ppPtr)
{
    char *pRead;
    char *pWrite;
    char *pStart;
    char  cNext;

    pRead = *ppPtr;

    while (*pRead == ' ' || *pRead == '\t')
        pRead++;

    if (*pRead == '\0')
    {
        *ppPtr = pRead;
        return NULL;
    }

    pStart = pRead;
    pWrite = pRead;

    while (*pRead != '\0')
    {
        if (*pRead == '\\')
        {
            cNext = *(pRead + 1);
            if (cNext == ' ' || cNext == '\t' ||
                cNext == '(' || cNext == ')')
            {
                pRead++;
                *pWrite++ = *pRead++;
                continue;
            }
        }
        if (*pRead == ' ' || *pRead == '\t')
        {
            *pWrite = '\0';
            pRead++;
            break;
        }
        *pWrite++ = *pRead++;
    }

    if (*pRead == '\0')
        *pWrite = '\0';

    *ppPtr = pRead;
    return pStart;
}

/* ------------------------------------------------------------------
 * Internal: command parser
 * ------------------------------------------------------------------ */

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

    iArgIdx  = 0;
    pSaveptr = ptParsed->szArgBuf;

    while (iArgIdx < LINE_MAX_ARGS)
    {
        pToken = line_next_token_unescape(&pSaveptr);
        if (pToken == NULL)
            break;
        ptParsed->aszArgv[iArgIdx] = pToken;
        iArgIdx++;
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
 * History file path
 * ------------------------------------------------------------------ */

static st_error_t line_history_default_path(char *szOut, size_t uiMax)
{
    const char *szHome;

#ifdef ST_PLATFORM_WINDOWS
    szHome = getenv("HOME");           /* MSYS2 sets this    */
    if (szHome == NULL)
        szHome = getenv("USERPROFILE"); /* cmd.exe fallback  */
#else
    szHome = getenv("HOME");
#endif

    if (szHome == NULL || szHome[0] == '\0')
    {
        LOG_INFO("HOME not set - history file disabled");
        return ST_ERROR;
    }

    snprintf(szOut, uiMax, "%s/.st4ever_history", szHome);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public history API
 * ------------------------------------------------------------------ */

st_error_t line_history_add(const char *szCmd)
{
    int    iLastPhys;
    size_t uiCmdLen;

    if (szCmd == NULL)
    {
        return ST_ERROR;
    }

    uiCmdLen = strlen(szCmd);
    if (uiCmdLen == 0)
    {
        return ST_NO_ERROR;
    }

    /* Skip duplicate of most recent entry */
    if (g_line_ptCtx.iHistCount > 0)
    {
        iLastPhys = line_hist_phys(g_line_ptCtx.iHistCount - 1);
        if (strcmp(g_line_ptCtx.aHistory[iLastPhys], szCmd) == 0)
        {
            return ST_NO_ERROR;
        }
    }

    strncpy(g_line_ptCtx.aHistory[g_line_ptCtx.iHistHead], szCmd, ST_MAX_CMD - 1);
    g_line_ptCtx.aHistory[g_line_ptCtx.iHistHead][ST_MAX_CMD - 1] = '\0';

    g_line_ptCtx.iHistHead = (g_line_ptCtx.iHistHead + 1) % LINE_HISTORY_MAX;
    if (g_line_ptCtx.iHistCount < LINE_HISTORY_MAX)
    {
        g_line_ptCtx.iHistCount++;
    }

    return ST_NO_ERROR;
}

int line_history_count(void)
{
    return g_line_ptCtx.iHistCount;
}

st_error_t line_history_get(int iVirtIdx, char *szBuf, size_t uiLen)
{
    int iPhys;

    if (szBuf == NULL || uiLen == 0)
    {
        return ST_ERROR;
    }
    if (iVirtIdx < 0 || iVirtIdx >= g_line_ptCtx.iHistCount)
    {
        return ST_ERROR;
    }

    iPhys = line_hist_phys(iVirtIdx);
    strncpy(szBuf, g_line_ptCtx.aHistory[iPhys], uiLen - 1);
    szBuf[uiLen - 1] = '\0';

    return ST_NO_ERROR;
}

void line_history_clear(void)
{
    g_line_ptCtx.iHistCount = 0;
    g_line_ptCtx.iHistHead  = 0;
}

st_error_t line_history_save(const char *szPath)
{
    char   szDefaultPath[ST_MAX_PATH];
    FILE  *pFile;
    int    iVirt;
    int    iPhys;

    if (szPath == NULL)
    {
        if (line_history_default_path(szDefaultPath,
                                       sizeof(szDefaultPath)) != ST_NO_ERROR)
        {
            return ST_ERROR;
        }
        szPath = szDefaultPath;
    }

    pFile = fopen(szPath, "w");
    if (pFile == NULL)
    {
        LOG_INFO("history_save: cannot open '%s': %s",
                 szPath, strerror(errno));
        return ST_ERROR;
    }

    for (iVirt = 0; iVirt < g_line_ptCtx.iHistCount; iVirt++)
    {
        iPhys = line_hist_phys(iVirt);
        fprintf(pFile, "%s\n", g_line_ptCtx.aHistory[iPhys]);
    }

    fclose(pFile);
    LOG_INFO("history saved to '%s' (%d entries)",
             szPath, g_line_ptCtx.iHistCount);
    return ST_NO_ERROR;
}

st_error_t line_history_load(const char *szPath)
{
    char   szDefaultPath[ST_MAX_PATH];
    char   szLine[ST_MAX_CMD];
    FILE  *pFile;
    size_t uiLen;

    if (szPath == NULL)
    {
        if (line_history_default_path(szDefaultPath,
                                       sizeof(szDefaultPath)) != ST_NO_ERROR)
        {
            return ST_NO_ERROR; /* no HOME — not an error */
        }
        szPath = szDefaultPath;
    }

    pFile = fopen(szPath, "r");
    if (pFile == NULL)
    {
        /* File not found is acceptable (first run) */
        return ST_NO_ERROR;
    }

    while (fgets(szLine, sizeof(szLine), pFile) != NULL)
    {
        /* Strip trailing CR/LF */
        uiLen = strlen(szLine);
        while (uiLen > 0 &&
               (szLine[uiLen - 1] == '\n' || szLine[uiLen - 1] == '\r'))
        {
            szLine[--uiLen] = '\0';
        }

        if (uiLen > 0 && szLine[0] != '#')
        {
            line_history_add(szLine);
        }
    }

    fclose(pFile);
    LOG_INFO("history loaded from '%s' (%d entries)",
             szPath, g_line_ptCtx.iHistCount);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public completion query API
 * ------------------------------------------------------------------ */

int line_complete_cmd_query(const char *szPrefix,
                             char       (*aOut)[ST_MAX_PATH],
                             int         iMaxOut)
{
    size_t uiCmdCount;
    size_t uiIdx;
    size_t uiPrefixLen;
    int    iCount;

    if (szPrefix == NULL || aOut == NULL || iMaxOut <= 0)
    {
        return -1;
    }

    uiCmdCount  = ST_ARRAY_SIZE(g_line_aCmds);
    uiPrefixLen = strlen(szPrefix);
    iCount      = 0;

    for (uiIdx = 0; uiIdx < uiCmdCount && iCount < iMaxOut; uiIdx++)
    {
        const cmd_entry_t *ptE = &g_line_aCmds[uiIdx];

        /* Match on full name or single-letter shortcut */
        if (strncmp(ptE->szFull,  szPrefix, uiPrefixLen) == 0
        ||  strncmp(ptE->szShort, szPrefix, uiPrefixLen) == 0)
        {
            /* Always store the full name as the candidate */
            strncpy(aOut[iCount], ptE->szFull, ST_MAX_PATH - 1);
            aOut[iCount][ST_MAX_PATH - 1] = '\0';
            iCount++;
        }
    }

    return iCount;
}

/* line_split_path() - split a partial path into dir and name prefix.
 * If no separator, szDir = "." and szName = szPath. */
static void line_split_path(const char *szPath,
                              char       *szDir,  size_t uiDirMax,
                              char       *szName, size_t uiNameMax)
{
    const char *p;
    const char *pLastSep;
    size_t      uiDirLen;

    pLastSep = NULL;
    for (p = szPath; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            pLastSep = p;
        }
    }

    if (pLastSep == NULL)
    {
        strncpy(szDir, ".", uiDirMax - 1);
        szDir[uiDirMax - 1] = '\0';
        strncpy(szName, szPath, uiNameMax - 1);
        szName[uiNameMax - 1] = '\0';
    }
    else
    {
        uiDirLen = (size_t)(pLastSep - szPath + 1); /* include sep */
        if (uiDirLen >= uiDirMax)
        {
            uiDirLen = uiDirMax - 1;
        }
        memcpy(szDir, szPath, uiDirLen);
        szDir[uiDirLen] = '\0';

        strncpy(szName, pLastSep + 1, uiNameMax - 1);
        szName[uiNameMax - 1] = '\0';
    }
}

/* Unescape a path token: "foo\ bar" → "foo bar", "foo\(bar\)" → "foo(bar)".
 * Recognised sequences: \<space>, \(, \).  Other backslash combos pass through
 * unchanged (e.g. directory separators on Windows are left untouched). */
static void line_unescape_path(const char *szSrc,
                                char       *szDst,
                                size_t      uiDst)
{
    size_t      uiOut = 0;
    const char *p     = szSrc;
    char        c;

    while (*p != '\0' && uiOut + 1 < uiDst)
    {
        if (*p == '\\' && *(p + 1) != '\0')
        {
            c = *(p + 1);
            if (c == ' ' || c == '(' || c == ')')
            {
                p++;
                szDst[uiOut++] = *p++;
                continue;
            }
        }
        szDst[uiOut++] = *p++;
    }
    szDst[uiOut] = '\0';
}

/* Escape a path candidate for shell-style insertion into the command buffer.
 * Characters escaped: space, (, ).                                            */
static void line_escape_path(const char *szSrc,
                               char       *szDst,
                               size_t      uiDst)
{
    size_t      uiOut = 0;
    const char *p     = szSrc;

    while (*p != '\0' && uiOut + 2 < uiDst)
    {
        if (*p == ' ' || *p == '(' || *p == ')')
        {
            szDst[uiOut++] = '\\';
        }
        szDst[uiOut++] = *p++;
    }
    szDst[uiOut] = '\0';
}

int line_complete_path_query(const char *szPrefix,
                              const char *szCwd,
                              char       (*aOut)[ST_MAX_PATH],
                              int         iMaxOut)
{
    char           szDir[ST_MAX_PATH];
    char           szNamePfx[ST_MAX_PATH];
    char           szUnescapedPfx[ST_MAX_PATH];
    char           szEscaped[ST_MAX_PATH];
    char           szScanDir[ST_MAX_PATH];
    char           szFullPath[ST_MAX_PATH + ST_MAX_PATH];
    DIR           *pDir;
    struct dirent *pEntry;
    struct stat    tStat;
    size_t         uiPrefixLen;
    int            iCount;
    int            bIsDir;

    ST_UNUSED(szCwd);

    if (szPrefix == NULL || aOut == NULL || iMaxOut <= 0)
    {
        return -1;
    }

    /* Unescape the incoming prefix so filesystem comparisons work with
     * actual filenames (e.g. "My\ File" → "My File"). */
    line_unescape_path(szPrefix, szUnescapedPfx, sizeof(szUnescapedPfx));

    line_split_path(szUnescapedPfx, szDir, sizeof(szDir),
                    szNamePfx, sizeof(szNamePfx));

    /* Use szDir directly for scanning (may be "." for relative) */
    strncpy(szScanDir, szDir, sizeof(szScanDir) - 1);
    szScanDir[sizeof(szScanDir) - 1] = '\0';

    pDir = opendir(szScanDir);
    if (pDir == NULL)
    {
        return 0;
    }

    uiPrefixLen = strlen(szNamePfx);
    iCount      = 0;

    while ((pEntry = readdir(pDir)) != NULL && iCount < iMaxOut)
    {
        if (strcmp(pEntry->d_name, ".") == 0 ||
            strcmp(pEntry->d_name, "..") == 0)
        {
            continue;
        }

        if (uiPrefixLen > 0 &&
            strncmp(pEntry->d_name, szNamePfx, uiPrefixLen) != 0)
        {
            continue;
        }

        /* Build full path for stat() */
        snprintf(szFullPath, sizeof(szFullPath),
                 "%s%s", szScanDir, pEntry->d_name);

        bIsDir = 0;
        if (stat(szFullPath, &tStat) == 0 && S_ISDIR(tStat.st_mode))
        {
            bIsDir = 1;
        }

        /* Build candidate: escape special chars (spaces, parens) so the
         * result can be inserted directly into the command buffer.
         * The dir part is already from the user-typed prefix (escaped or
         * plain), so it is kept as-is; only the filename part is escaped. */
        {
            const char *szDirPfx;
            size_t      uiDirLen;
            size_t      uiNameLen;
            size_t      uiPos;

            szDirPfx = (strcmp(szDir, ".") == 0) ? "" : szDir;
            uiDirLen  = strlen(szDirPfx);

            /* Escape the filename component */
            line_escape_path(pEntry->d_name, szEscaped, sizeof(szEscaped));
            uiNameLen = strlen(szEscaped);

            if (uiDirLen + uiNameLen + 2 >= ST_MAX_PATH)
            {
                continue;
            }
            uiPos = 0;
            memcpy(aOut[iCount], szDirPfx, uiDirLen);
            uiPos += uiDirLen;
            memcpy(aOut[iCount] + uiPos, szEscaped, uiNameLen);
            uiPos += uiNameLen;
            if (bIsDir)
            {
                aOut[iCount][uiPos++] = '/';
            }
            aOut[iCount][uiPos] = '\0';
        }

        iCount++;
    }

    closedir(pDir);
    return iCount;
}

/* ------------------------------------------------------------------
 * Public colour API
 * ------------------------------------------------------------------ */

void line_set_colors(st_bool_t bColors)
{
    g_line_ptCtx.bColors = bColors;
}

st_bool_t line_get_colors(void)
{
    return g_line_ptCtx.bColors;
}

/* ------------------------------------------------------------------
 * Thread-safe selected path accessors
 * ------------------------------------------------------------------ */

st_error_t line_set_selected(const char     *szPath)
{
    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    if (g_line_ptCtx.ptSelectedMutex != NULL)
    {
        platform_mutex_lock(g_line_ptCtx.ptSelectedMutex);
    }

    if (szPath == NULL)
    {
        g_line_ptCtx.szSelected[0] = '\0';
    }
    else
    {
        strncpy(g_line_ptCtx.szSelected, szPath, ST_MAX_PATH - 1);
        g_line_ptCtx.szSelected[ST_MAX_PATH - 1] = '\0';
    }

    if (g_line_ptCtx.ptSelectedMutex != NULL)
    {
        platform_mutex_unlock(g_line_ptCtx.ptSelectedMutex);
    }

    return ST_NO_ERROR;
}

st_error_t line_get_selected(char                 *szBuf,
                             size_t                uiLen)
{
    if (szBuf == NULL || uiLen == 0)
    {
        LOG_ERROR("NULL parameter or uiLen==0");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    if (g_line_ptCtx.ptSelectedMutex != NULL)
    {
        platform_mutex_lock(
            (st_mutex_t *)g_line_ptCtx.ptSelectedMutex);
    }

    strncpy(szBuf, g_line_ptCtx.szSelected, uiLen - 1);
    szBuf[uiLen - 1] = '\0';

    if (g_line_ptCtx.ptSelectedMutex != NULL)
    {
        platform_mutex_unlock(
            (st_mutex_t *)g_line_ptCtx.ptSelectedMutex);
    }

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Command handlers
 * ------------------------------------------------------------------ */

static st_error_t line_cmd_help(const parsed_cmd_t *ptParsed)
{
    size_t uiCmdCount;
    size_t uiIdx;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    if (ptParsed->iArgc > 1)
    {
        line_print_warning("help: ignoring extra arguments");
    }

    uiCmdCount = ST_ARRAY_SIZE(g_line_aCmds);

    printf("\n");
    printf("%s%s  %s %s  -  %s\n%s\n",
           c_bold(), c_cyan(),
           ST_APP_NAME, ST_APP_VERSION, ST_APP_DESC,
           c_reset());

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
        printf("  %s%-12s%s %-4s  %s%-22s%s  %s\n",
               c_green(), ptEntry->szFull, c_reset(),
               ptEntry->szShort,
               c_gray(), ptEntry->szSynopsis, c_reset(),
               ptEntry->szHelp);
    }

    printf("\n");
    printf("  %sCTRL+<key> shortcuts available for most commands.\n"
           "  UP/DOWN navigate history.  TAB completes commands\n"
           "  (1st word) or file/dir paths (further words).\n"
           "  Ghost text (dim) previews the first candidate.%s\n\n",
           c_gray(), c_reset());

    LOG_INFO("help displayed");
    return ST_NO_ERROR;
}

static st_error_t line_cmd_quit(const parsed_cmd_t *ptParsed)
{
    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    if (ptParsed->iArgc > 1)
    {
        line_print_warning("quit: ignoring extra arguments");
    }

    line_print_msg("Goodbye. The Atari ST lives on...");
    g_line_ptCtx.bRunning = ST_FALSE;

    LOG_INFO("quit requested");
    return ST_NO_ERROR;
}

static st_error_t line_cmd_trace(const parsed_cmd_t *ptParsed)
{
    st_error_t  eResult;
    const char *szArg;

    LOG_TRACE("ptParsed=%p",
              (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
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
            /* P26: LOG_TRACE disabled by default on toggle-open to prevent
             * flooding the view.  'trace on' enables it explicitly. */
            trace_set_trace_enabled(ST_FALSE);
            line_print_msg("Trace console opened (LOG_TRACE off — use 'trace on' to enable).");
        }
        return ST_NO_ERROR;
    }

    szArg = ptParsed->aszArgv[1];

    /* "level" and "clear" take their own sub-argument — skip generic check */
    if (ptParsed->iArgc > 2 &&
        strcmp(szArg, "level") != 0 &&
        strcmp(szArg, "clear") != 0)
    {
        line_print_warning("trace: ignoring extra arguments after '%s'",
                           szArg);
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
    else if (strcmp(szArg, "clear") == 0)
    {
        /* P27: clear the GUI ring buffer */
        if (ptParsed->iArgc > 2)
        {
            line_print_warning(
                "trace clear: ignoring extra arguments");
        }
        trace_clear();
        line_print_msg("Trace: buffer cleared.");
    }
    else if (strcmp(szArg, "level") == 0)
    {
        /* P28: set GUI display filter level */
        const char  *szLevel;
        log_level_t  eNewLevel;

        if (ptParsed->iArgc < 3)
        {
            line_print_warning(
                "trace level: expected all|info|error|todo");
            return ST_NO_ERROR;
        }

        szLevel = ptParsed->aszArgv[2];

        /* P57: incremental levels — todo < error < info < all.
         * "all" replaces the old "trace" keyword (kept as silent alias). */
        if (strcmp(szLevel, "all") == 0 ||
            strcmp(szLevel, "trace") == 0)        /* legacy alias */
        {
            eNewLevel = LOG_LEVEL_TRACE;
        }
        else if (strcmp(szLevel, "info") == 0)
        {
            eNewLevel = LOG_LEVEL_INFO;
        }
        else if (strcmp(szLevel, "error") == 0)
        {
            eNewLevel = LOG_LEVEL_ERROR;
        }
        else if (strcmp(szLevel, "todo") == 0)
        {
            eNewLevel = LOG_LEVEL_TODO;
        }
        else
        {
            line_print_warning(
                "trace level: unknown level '%s'  "
                "- use: all|info|error|todo",
                szLevel);
            return ST_NO_ERROR;
        }

        trace_set_view_level(eNewLevel);
        /* Normalise display name: always show canonical name */
        {
            const char *szCanon;
            switch (eNewLevel)
            {
                case LOG_LEVEL_TRACE: szCanon = "all";   break;
                case LOG_LEVEL_INFO:  szCanon = "info";  break;
                case LOG_LEVEL_ERROR: szCanon = "error"; break;
                default:              szCanon = "todo";  break;
            }
            line_print_msg("Trace: view level set to '%s'", szCanon);
        }
    }
    else
    {
        line_print_warning(
            "trace: unknown argument '%s'  "
            "- use: trace | trace on | trace off | "
            "trace clear | trace level all|info|error|todo",
            szArg);
    }

    return ST_NO_ERROR;
}

static st_error_t line_cmd_dir(const parsed_cmd_t *ptParsed)
{
    char        szSelPath[ST_MAX_PATH];
    const char *szPath;
    st_bool_t   bShowHidden;
    st_bool_t   bSelectOnly;
    st_error_t  eResult;
    file_stat_t tStat;
    int         iArg;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    bShowHidden = ST_FALSE;
    bSelectOnly = ST_FALSE;
    szPath      = NULL;
    szSelPath[0] = '\0';

    for (iArg = 1; iArg < ptParsed->iArgc; iArg++)
    {
        if (strcmp(ptParsed->aszArgv[iArg], "-a") == 0)
        {
            bShowHidden = ST_TRUE;
        }
        else if (strcmp(ptParsed->aszArgv[iArg], "--select") == 0)
        {
            bSelectOnly = ST_TRUE;
            if (iArg + 1 < ptParsed->iArgc)
            {
                strncpy(szSelPath, ptParsed->aszArgv[iArg + 1],
                        ST_MAX_PATH - 1);
                szSelPath[ST_MAX_PATH - 1] = '\0';
                iArg++;
            }
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

    /* P50: headless selection — set selected path without opening GUI */
    if (bSelectOnly)
    {
        if (szSelPath[0] == '\0')
        {
            line_print_warning("dir --select: path argument required.");
            return ST_NO_ERROR;
        }
        memset(&tStat, 0, sizeof(tStat));
        eResult = file_stat(szSelPath, &tStat);
        if (eResult != ST_NO_ERROR || !tStat.bExists)
        {
            line_print_error("dir --select: '%s': not found.", szSelPath);
            return ST_NO_ERROR;
        }
        line_set_selected(szSelPath);
        line_print_msg("Selected: %s", szSelPath);
        line_update_console_title();
        return ST_NO_ERROR;
    }

    if (g_line_ptCtx.ptDirView != NULL)
    {
        /* BUG-09: save history before closing so ALT+← works across
         * successive dir commands. */
        memcpy(g_line_aDirNavHist, g_line_ptCtx.ptDirView->aszNavHistory,
               sizeof(g_line_aDirNavHist));
        g_line_iDirNavHistHead  = g_line_ptCtx.ptDirView->iNavHistHead;
        g_line_iDirNavHistCount = g_line_ptCtx.ptDirView->iNavHistCount;

        eResult = dir_close(&g_line_ptCtx.ptDirView);
        if (eResult != ST_NO_ERROR)
        {
            line_print_warning("dir: could not close previous view");
        }
        g_line_ptCtx.ptDirView = NULL;
    }

    eResult = dir_open(szPath, (st_u64_t)&g_line_ptCtx, bShowHidden, &g_line_ptCtx.ptDirView);
    if (eResult != ST_NO_ERROR)
    {
        line_print_error("dir: failed to open view for '%s'",
                         szPath ? szPath : "(cwd)");
        return ST_ERROR;
    }

    /* BUG-09: restore previous history and append the newly opened path.
     * dir_open() seeds aszNavHistory[0] with szRoot, head=0, count=1.
     * We insert [prev_history] before the new root, then add new root at
     * iNewHead.  If history is full the oldest entry is silently dropped. */
    if (g_line_iDirNavHistHead >= 0)
    {
        int iNewHead;
        int i;
        char szNewRoot[ST_MAX_PATH];

        strncpy(szNewRoot, g_line_ptCtx.ptDirView->aszNavHistory[0],
                ST_MAX_PATH - 1);
        szNewRoot[ST_MAX_PATH - 1] = '\0';

        iNewHead = g_line_iDirNavHistHead + 1;
        if (iNewHead >= DIR_NAV_HIST_MAX)
            iNewHead = DIR_NAV_HIST_MAX - 1;

        for (i = 0; i <= g_line_iDirNavHistHead && i < iNewHead; i++)
        {
            strncpy(g_line_ptCtx.ptDirView->aszNavHistory[i],
                    g_line_aDirNavHist[i], ST_MAX_PATH - 1);
            g_line_ptCtx.ptDirView->aszNavHistory[i][ST_MAX_PATH - 1] = '\0';
        }
        strncpy(g_line_ptCtx.ptDirView->aszNavHistory[iNewHead], szNewRoot,
                ST_MAX_PATH - 1);
        g_line_ptCtx.ptDirView->aszNavHistory[iNewHead][ST_MAX_PATH - 1] = '\0';
        g_line_ptCtx.ptDirView->iNavHistHead  = iNewHead;
        g_line_ptCtx.ptDirView->iNavHistCount = iNewHead + 1;
    }

    line_print_msg("Directory view opened%s%s%s.",
                   szPath ? " for "              : "",
                   szPath ? szPath               : "",
                   bShowHidden ? " (showing hidden files)" : "");
    return ST_NO_ERROR;
}

static st_error_t line_cmd_colors(const parsed_cmd_t *ptParsed)
{
    const char *szArg;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    if (ptParsed->iArgc == 1)
    {
        /* Toggle */
        g_line_ptCtx.bColors = (g_line_ptCtx.bColors == ST_TRUE)
                         ? ST_FALSE : ST_TRUE;
        line_print_msg("Colors: %s",
                       g_line_ptCtx.bColors == ST_TRUE ? "ON" : "OFF");
        return ST_NO_ERROR;
    }

    szArg = ptParsed->aszArgv[1];

    if (ptParsed->iArgc > 2)
    {
        line_print_warning(
            "colors: ignoring extra arguments after '%s'", szArg);
    }

    if (strcmp(szArg, "on") == 0)
    {
        g_line_ptCtx.bColors = ST_TRUE;
        line_print_msg("Colors: ON");
    }
    else if (strcmp(szArg, "off") == 0)
    {
        g_line_ptCtx.bColors = ST_FALSE;
        line_print_msg("Colors: OFF");
    }
    else
    {
        line_print_warning(
            "colors: unknown argument '%s'  "
            "- use: colors | colors on | colors off",
            szArg);
    }

    return ST_NO_ERROR;
}

static st_error_t line_cmd_load(const parsed_cmd_t *ptParsed)
{
    char        szPath[ST_MAX_PATH];
    char        szSel[ST_MAX_PATH];
    file_stat_t tStat;
    st_error_t  eResult;
    int         i;
    const load_context_t *ptLoadCtx;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    szPath[0] = '\0';

    /* Collect first non-option argument as path */
    for (i = 1; i < ptParsed->iArgc; i++)
    {
        if (ptParsed->aszArgv[i][0] != '-')
        {
            strncpy(szPath, ptParsed->aszArgv[i], ST_MAX_PATH - 1);
            szPath[ST_MAX_PATH - 1] = '\0';
            break;
        }
    }

    /* Fall back to the dir-selected path when no explicit argument */
    if (szPath[0] == '\0')
    {
        szSel[0] = '\0';
        line_get_selected(szSel, sizeof(szSel));
        if (szSel[0] != '\0')
        {
            strncpy(szPath, szSel, ST_MAX_PATH - 1);
            szPath[ST_MAX_PATH - 1] = '\0';
        }
    }

    if (szPath[0] == '\0')
    {
        line_print_msg("No file selected. Provide a path or select a "
                       "file with 'dir'.");
        return ST_NO_ERROR;
    }

    /* Stat the target to provide user-friendly rejection messages */
    memset(&tStat, 0, sizeof(tStat));
    eResult = file_stat(szPath, &tStat);
    if (eResult != ST_NO_ERROR)
    {
        line_print_error("Cannot stat '%s'.", szPath);
        return ST_NO_ERROR;
    }

    if (!tStat.bExists)
    {
        line_print_error("'%s': file not found.", szPath);
        return ST_NO_ERROR;
    }

    if (tStat.bIsDir)
    {
        line_print_warning("'%s' is a directory. Use 'mount' to work "
                            "with directories.", szPath);
        return ST_NO_ERROR;
    }

    if (strcmp(tStat.szExt, "st")  == 0
     || strcmp(tStat.szExt, "msa") == 0
     || strcmp(tStat.szExt, "stx") == 0)
    {
        line_print_warning("'%s' is a disk image. Use 'mount' to work "
                            "with disk images.", szPath);
        return ST_NO_ERROR;
    }

    /* Attempt load into emulated ST RAM */
    eResult = load_file(szPath);
    if (eResult != ST_NO_ERROR)
    {
        line_print_error("Failed to load '%s'. See trace for details.",
                         szPath);
        return ST_NO_ERROR;
    }

    ptLoadCtx = load_get_context();
    if (ptLoadCtx->eType == LOAD_TYPE_PRG)
    {
        line_print_msg("Loaded PRG '%s' at ST:0x%06X "
                       "(.text=%u .data=%u .bss=%u, %u fixup(s)).",
                       szPath,
                       (unsigned)ptLoadCtx->uiLoadAddr,
                       (unsigned)ptLoadCtx->uiTextSize,
                       (unsigned)ptLoadCtx->uiDataSize,
                       (unsigned)ptLoadCtx->uiBssSize,
                       (unsigned)ptLoadCtx->uiFixupCount);
    }
    else
    {
        line_print_msg("Loaded '%s' at ST:0x%06X (%u bytes).",
                       szPath,
                       (unsigned)ptLoadCtx->uiLoadAddr,
                       (unsigned)ptLoadCtx->uiSize);
    }

    line_update_console_title();
    return ST_NO_ERROR;
}

/* line_cmd_edit() — open text or binary editor on a file.
 * UC8: routes text files to edit_txt; binary/image = stub. */
static st_error_t line_cmd_edit(const parsed_cmd_t *ptParsed)
{
    char        szPath[ST_MAX_PATH];
    char        szSel[ST_MAX_PATH];
    file_stat_t tStat;
    st_error_t  eResult;
    st_bool_t   bForceHex;
    int         i;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    /* edit backup [on|off] — toggle backup-on-open setting */
    if (ptParsed->iArgc >= 2
     && strcmp(ptParsed->aszArgv[1], "backup") == 0)
    {
        st_bool_t bVal;

        if (ptParsed->iArgc >= 3)
        {
            if (strcmp(ptParsed->aszArgv[2], "on") == 0)
                bVal = ST_TRUE;
            else if (strcmp(ptParsed->aszArgv[2], "off") == 0)
                bVal = ST_FALSE;
            else
            {
                line_print_warning(
                    "edit backup: expected 'on' or 'off'.");
                return ST_NO_ERROR;
            }
        }
        else
        {
            bVal = edit_txt_get_backup() ? ST_FALSE : ST_TRUE;
        }
        edit_txt_set_backup(bVal);
        line_print_msg(bVal
            ? "edit backup: on (timestamped .bak on open)."
            : "edit backup: off.");
        return ST_NO_ERROR;
    }

    bForceHex = ST_FALSE;
    szPath[0] = '\0';
    for (i = 1; i < ptParsed->iArgc; i++)
    {
        if (strcmp(ptParsed->aszArgv[i], "-h") == 0
         || strcmp(ptParsed->aszArgv[i], "--hex") == 0)
        {
            bForceHex = ST_TRUE;
        }
        else if (ptParsed->aszArgv[i][0] != '-')
        {
            strncpy(szPath, ptParsed->aszArgv[i], ST_MAX_PATH - 1);
            szPath[ST_MAX_PATH - 1] = '\0';
        }
    }

    if (szPath[0] == '\0')
    {
        szSel[0] = '\0';
        line_get_selected(szSel, sizeof(szSel));
        if (szSel[0] != '\0')
        {
            strncpy(szPath, szSel, ST_MAX_PATH - 1);
            szPath[ST_MAX_PATH - 1] = '\0';
        }
    }

    if (szPath[0] == '\0')
    {
        line_print_msg("No file selected. Provide a path or select a "
                       "file with 'dir'.");
        return ST_NO_ERROR;
    }

    memset(&tStat, 0, sizeof(tStat));
    eResult = file_stat(szPath, &tStat);
    if (eResult != ST_NO_ERROR)
    {
        line_print_error("Cannot stat '%s'.", szPath);
        return ST_NO_ERROR;
    }
    if (!tStat.bExists)
    {
        line_print_error("'%s': file not found.", szPath);
        return ST_NO_ERROR;
    }
    if (tStat.bIsDir)
    {
        line_print_warning("'%s' is a directory — use dir.", szPath);
        return ST_NO_ERROR;
    }

    /* Force hex mode (-h / --hex) or recognised binary extensions → hex */
    if (bForceHex
     || strcmp(tStat.szExt, "prg") == 0
     || strcmp(tStat.szExt, "ttp") == 0
     || strcmp(tStat.szExt, "tos") == 0
     || strcmp(tStat.szExt, "bin") == 0
     || strcmp(tStat.szExt, "raw") == 0
     || strcmp(tStat.szExt, "st")  == 0
     || strcmp(tStat.szExt, "msa") == 0
     || strcmp(tStat.szExt, "stx") == 0)
    {
        if (g_line_ptCtx.ptEditHexView != NULL)
        {
            eResult = edit_hex_close(&g_line_ptCtx.ptEditHexView);
            if (eResult != ST_NO_ERROR)
                line_print_warning(
                    "edit: could not close previous hex view");
            g_line_ptCtx.ptEditHexView = NULL;
        }
        eResult = edit_hex_open(szPath, &g_line_ptCtx.ptEditHexView);
        if (eResult != ST_NO_ERROR)
        {
            line_print_error("edit: failed to open '%s'.", szPath);
            return ST_NO_ERROR;
        }
        line_print_msg("Hex-editing '%s' (%zu bytes).",
                       szPath,
                       g_line_ptCtx.ptEditHexView
                       ? g_line_ptCtx.ptEditHexView->uiSize : (size_t)0);
        line_update_console_title();
        return ST_NO_ERROR;
    }

    /* All other files → text editor */
    if (g_line_ptCtx.ptEditTxtView != NULL)
    {
        eResult = edit_txt_close(&g_line_ptCtx.ptEditTxtView);
        if (eResult != ST_NO_ERROR)
            line_print_warning("edit: could not close previous view");
        g_line_ptCtx.ptEditTxtView = NULL;
    }

    eResult = edit_txt_open(szPath, &g_line_ptCtx.ptEditTxtView);
    if (eResult != ST_NO_ERROR)
    {
        line_print_error("edit: failed to open '%s'.", szPath);
        return ST_NO_ERROR;
    }

    line_print_msg("Editing '%s' (%d lines).",
                   szPath,
                   g_line_ptCtx.ptEditTxtView
                   ? g_line_ptCtx.ptEditTxtView->iLineCount : 0);
    line_update_console_title();
    return ST_NO_ERROR;
}

static st_error_t line_cmd_where(const parsed_cmd_t *ptParsed)
{
    char szSel[ST_MAX_PATH];

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    if (ptParsed->iArgc > 1)
    {
        line_print_warning("where: ignoring extra arguments");
    }

    line_print_msg("Working directory : %s%s%s",
                   c_cyan(), g_line_ptCtx.szCwd, c_reset());

    szSel[0] = '\0';
    line_get_selected(szSel, sizeof(szSel));

    if (szSel[0] != '\0')
    {
        line_print_msg("Selected          : %s%s%s",
                       c_green(), szSel, c_reset());
    }
    else
    {
        line_print_msg("Selected          : %s(none)%s",
                       c_gray(), c_reset());
    }

    LOG_INFO("where: cwd='%s' sel='%s'", g_line_ptCtx.szCwd, szSel);
    return ST_NO_ERROR;
}

static st_error_t line_cmd_info(const parsed_cmd_t *ptParsed)
{
    char szSel[ST_MAX_PATH];

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    if (ptParsed->iArgc > 1)
    {
        line_print_warning("info: ignoring extra arguments");
    }

    szSel[0] = '\0';
    line_get_selected(szSel, sizeof(szSel));

    printf("\n");
    printf("%s%s  === ST4Ever %s Status ===\n%s",
           c_bold(), c_cyan(), ST_APP_VERSION, c_reset());

    line_print_msg("Working dir  : %s%s%s",
                   c_cyan(), g_line_ptCtx.szCwd, c_reset());

    if (szSel[0] != '\0')
    {
        line_print_msg("Selected     : %s%s%s",
                       c_green(), szSel, c_reset());
    }
    else
    {
        line_print_msg("Selected     : %s(none)%s",
                       c_gray(), c_reset());
    }

    line_print_msg("Trace        : %s%s%s%s",
                   trace_is_open() ? c_green() : c_gray(),
                   trace_is_open() ? "open" : "closed",
                   trace_is_open()
                       ? (trace_is_trace_enabled()
                              ? "  [LOG_TRACE on]"
                              : "  [LOG_TRACE off]")
                       : "",
                   c_reset());

    line_print_msg("Colors       : %s%s%s",
                   g_line_ptCtx.bColors ? c_green() : c_gray(),
                   g_line_ptCtx.bColors ? "on" : "off",
                   c_reset());

    line_print_msg("History      : %d entr%s",
                   line_history_count(),
                   line_history_count() == 1 ? "y" : "ies");

    /* Stubs — future UCs */
    line_print_msg("Disk mounted : %s(none)%s", c_gray(), c_reset());

    {
        const load_context_t *ptLoadCtx = load_get_context();
        if (ptLoadCtx->bLoaded)
        {
            if (ptLoadCtx->eType == LOAD_TYPE_PRG)
            {
                line_print_msg("Binary       : %s%s%s (PRG, ST:0x%06X, "
                               ".text=%u .data=%u .bss=%u, %u fixup(s))",
                               c_green(),
                               line_path_basename(ptLoadCtx->szPath),
                               c_reset(),
                               (unsigned)ptLoadCtx->uiLoadAddr,
                               (unsigned)ptLoadCtx->uiTextSize,
                               (unsigned)ptLoadCtx->uiDataSize,
                               (unsigned)ptLoadCtx->uiBssSize,
                               (unsigned)ptLoadCtx->uiFixupCount);
            }
            else
            {
                line_print_msg("Binary       : %s%s%s (binary, ST:0x%06X, "
                               "%u bytes)",
                               c_green(),
                               line_path_basename(ptLoadCtx->szPath),
                               c_reset(),
                               (unsigned)ptLoadCtx->uiLoadAddr,
                               (unsigned)ptLoadCtx->uiSize);
            }
        }
        else
        {
            line_print_msg("Binary       : %s(none)%s",
                           c_gray(), c_reset());
        }
    }

    printf("\n");

    LOG_INFO("info: displayed dashboard");
    return ST_NO_ERROR;
}

static st_error_t line_cmd_history(const parsed_cmd_t *ptParsed)
{
    int  iCount;
    int  iN;
    int  iStart;
    int  iVirt;
    char szEntry[ST_MAX_CMD];

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    iCount = line_history_count();
    iN     = 10; /* default: last 10 entries */

    if (ptParsed->iArgc > 1)
    {
        iN = atoi(ptParsed->aszArgv[1]);
        if (iN <= 0)
        {
            line_print_warning("history: invalid count '%s'",
                               ptParsed->aszArgv[1]);
            return ST_NO_ERROR;
        }
    }

    if (ptParsed->iArgc > 2)
    {
        line_print_warning("history: ignoring extra arguments");
    }

    if (iCount == 0)
    {
        line_print_msg("(no history)");
        return ST_NO_ERROR;
    }

    if (iN > iCount)
    {
        iN = iCount;
    }

    iStart = iCount - iN;

    for (iVirt = iStart; iVirt < iCount; iVirt++)
    {
        if (line_history_get(iVirt, szEntry, sizeof(szEntry))
                == ST_NO_ERROR)
        {
            printf("  %s%3d%s  %s\n",
                   c_gray(), iVirt + 1, c_reset(), szEntry);
        }
    }

    LOG_INFO("history: displayed %d/%d entries", iN, iCount);
    return ST_NO_ERROR;
}

static st_error_t line_cmd_execute(const parsed_cmd_t *ptParsed)
{
    const load_context_t *ptLoadCtx;
    char                szSelected[ST_MAX_PATH];
    st_error_t          eResult;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    /* If a session is already open, close it first */
    if (exec_is_open())
    {
        exec_close();
    }

    /* Determine the target: explicit arg > selected > loaded */
    if (ptParsed->iArgc >= 2)
    {
        /* Explicit file argument: load it first */
        eResult = load_file(ptParsed->aszArgv[1]);
        if (eResult != ST_NO_ERROR)
        {
            line_print_error("execute: cannot load '%s'",
                             ptParsed->aszArgv[1]);
            return ST_NO_ERROR;
        }
    }
    else
    {
        /* No argument: check current selection */
        szSelected[0] = '\0';
        line_get_selected(szSelected, sizeof(szSelected));
        if (szSelected[0] != '\0')
        {
            eResult = load_file(szSelected);
            if (eResult != ST_NO_ERROR)
            {
                line_print_error("execute: cannot load '%s'",
                                 szSelected);
                return ST_NO_ERROR;
            }
        }
    }

    /* Check that a PRG is now loaded */
    ptLoadCtx = load_get_context();
    if (!ptLoadCtx->bLoaded)
    {
        line_print_error(
            "execute: no program loaded — use 'load <file>' first");
        return ST_NO_ERROR;
    }
    if (ptLoadCtx->eType != LOAD_TYPE_PRG)
    {
        line_print_error(
            "execute: loaded file is not an Atari ST PRG");
        return ST_NO_ERROR;
    }

    /* Open the execution session */
    eResult = exec_open(ptLoadCtx->szPath, ptLoadCtx->uiLoadAddr);
    if (eResult != ST_NO_ERROR)
    {
        line_print_error("execute: failed to open execution session");
        return ST_NO_ERROR;
    }

    line_print_msg("execute: session opened — use monitor window "
                   "[F5=Step F6=Run F7=Reset ESC=Quit]");
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * line_cmd_mount / line_cmd_umount (UC18.1)
 * ------------------------------------------------------------------ */

static st_error_t line_cmd_mount(const parsed_cmd_t *ptParsed)
{
    char        szPath[ST_MAX_PATH];
    char        szSel[ST_MAX_PATH];
    file_stat_t tStat;
    int         iKey;
    int         iArgIdx;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    /* Collect path: explicit arg > selected file > cwd (confirmation) */
    szPath[0] = '\0';
    for (iArgIdx = 1; iArgIdx < ptParsed->iArgc; iArgIdx++)
    {
        if (szPath[0] == '\0' &&
            ptParsed->aszArgv[iArgIdx][0] != '-')
        {
            strncpy(szPath, ptParsed->aszArgv[iArgIdx],
                    ST_MAX_PATH - 1);
            szPath[ST_MAX_PATH - 1] = '\0';
        }
    }

    if (szPath[0] == '\0')
    {
        szSel[0] = '\0';
        line_get_selected(szSel, sizeof(szSel));
        if (szSel[0] != '\0')
        {
            strncpy(szPath, szSel, ST_MAX_PATH - 1);
            szPath[ST_MAX_PATH - 1] = '\0';
        }
        else
        {
            /* Use cwd — ask for confirmation */
            line_print_msg("Mount '%s'? (y/n): ", g_line_ptCtx.szCwd);
            fflush(stdout);
            iKey = 0;
            if (console_read_key(&iKey) != ST_NO_ERROR ||
                (iKey != (int)'y' && iKey != (int)'Y'))
            {
                printf("\n");
                line_print_msg("Mount cancelled.");
                return ST_NO_ERROR;
            }
            printf("\n");
            strncpy(szPath, g_line_ptCtx.szCwd, ST_MAX_PATH - 1);
            szPath[ST_MAX_PATH - 1] = '\0';
        }
    }

    /* Reject plain files that belong to other commands */
    memset(&tStat, 0, sizeof(tStat));
    if (file_stat(szPath, &tStat) == ST_NO_ERROR && tStat.bExists)
    {
        if (!tStat.bIsDir &&
            strcmp(tStat.szExt, "st")   != 0 &&
            strcmp(tStat.szExt, "msa")  != 0)
        {
            line_print_warning("'%s' is not a directory or disk image — "
                               "use 'load' for executables.", szPath);
            return ST_NO_ERROR;
        }
    }

    /* Close any existing mount view first */
    if (g_line_ptCtx.ptMountView != NULL)
    {
        mount_view_close(&g_line_ptCtx.ptMountView);
        g_line_ptCtx.ptMountView = NULL;
    }

    if (mount_view_open(szPath, g_line_ptCtx.bRunning, &g_line_ptCtx.ptMountView) != ST_NO_ERROR)
    {
        line_print_error("mount failed for '%s'.", szPath);
        return ST_NO_ERROR;
    }

    line_print_msg("Mounted '%s' — %d file(s) on A:\\.",
                   szPath, g_line_ptCtx.ptMountView->iEntryCount);
    line_update_console_title();
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * image — create or save a .st/.msa disk image  (P58 --in/--out)
 *
 * Source resolution priority:
 *   1. --in <path>         explicit source
 *   2. mounted view        if g_line_ptCtx.ptMountView != NULL and no --in
 *   3. selected file/dir   from console context
 *   4. cwd                 last fallback (directory→image only)
 *
 * Output resolution priority:
 *   1. --out <path>        explicit output
 *   2. derived from source (convert: replace extension)
 *   3. ./disk.EXT or ./disk[N] (auto-numbered)
 * ------------------------------------------------------------------ */

/* Helper: check if szPath extension matches .st or .msa */
static int line_is_disk_path(const char *szPath)
{
    const char *szDot = strrchr(szPath, '.');
    return (szDot != NULL &&
            (strcmp(szDot, ".st")  == 0 ||
             strcmp(szDot, ".ST")  == 0 ||
             strcmp(szDot, ".msa") == 0 ||
             strcmp(szDot, ".MSA") == 0));
}

static st_error_t line_cmd_image(const parsed_cmd_t *ptParsed)
{
    mount_view_t    *ptTmpView  = NULL;
    image_st_t      *ptConvImg  = NULL;
    char             szIn[ST_MAX_PATH];   /* --in source path        */
    char             szOut[ST_MAX_PATH];  /* --out destination path  */
    char             szCwdBuf[ST_MAX_PATH];
    char             szAutoDst[ST_MAX_PATH];
    file_stat_t      tStat;
    mount_save_fmt_t eFmt       = MOUNT_SAVE_ST;
    st_bool_t        bBootable  = ST_FALSE;
    st_bool_t        bHaveIn    = ST_FALSE;
    st_bool_t        bHaveOut   = ST_FALSE;
    st_bool_t        bExtractDir = ST_FALSE;
    st_bool_t        bHaveFmt   = ST_FALSE;
    const char      *szExt;
    const char      *szDot;
    size_t           uiCwdLen;
    size_t           uiBaseLen;
    int              iExtNum;
    int              i;
    st_error_t       eResult;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    szIn[0]  = '\0';
    szOut[0] = '\0';

    /* ---- argument parsing ----------------------------------------- */
    for (i = 1; i < ptParsed->iArgc; i++)
    {
        if (strcmp(ptParsed->aszArgv[i], "--msa") == 0)
        {
            eFmt = MOUNT_SAVE_MSA;
            bHaveFmt = ST_TRUE;
        }
        else if (strcmp(ptParsed->aszArgv[i], "--st") == 0)
        {
            eFmt = MOUNT_SAVE_ST;
            bHaveFmt = ST_TRUE;
        }
        else if (strcmp(ptParsed->aszArgv[i], "--dir") == 0)
        {
            bExtractDir = ST_TRUE;
        }
        else if (strcmp(ptParsed->aszArgv[i], "--bootable") == 0)
        {
            bBootable = ST_TRUE;
        }
        else if (strcmp(ptParsed->aszArgv[i], "--in") == 0)
        {
            if (i + 1 < ptParsed->iArgc)
            {
                strncpy(szIn, ptParsed->aszArgv[++i], ST_MAX_PATH - 1);
                szIn[ST_MAX_PATH - 1] = '\0';
                bHaveIn = ST_TRUE;
            }
            else
            {
                line_print_warning("image: --in requires a path argument.");
                return ST_NO_ERROR;
            }
        }
        else if (strcmp(ptParsed->aszArgv[i], "--out") == 0)
        {
            if (i + 1 < ptParsed->iArgc)
            {
                strncpy(szOut, ptParsed->aszArgv[++i], ST_MAX_PATH - 1);
                szOut[ST_MAX_PATH - 1] = '\0';
                bHaveOut = ST_TRUE;
            }
            else
            {
                line_print_warning("image: --out requires a path argument.");
                return ST_NO_ERROR;
            }
        }
        else if (ptParsed->aszArgv[i][0] != '-')
        {
            /* P58: positional argument is ambiguous — require --in/--out */
            line_print_warning(
                "image: '%s' is ambiguous. "
                "Use --in <source> or --out <dest>.",
                ptParsed->aszArgv[i]);
            return ST_NO_ERROR;
        }
        else
        {
            line_print_warning("image: unknown option '%s'",
                               ptParsed->aszArgv[i]);
        }
    }

    /* ---- require at least one format/action flag ------------------- */
    if (!bHaveFmt && !bExtractDir)
    {
        line_print_warning(
            "image: specify --st, --msa, or --dir.  "
            "Examples:\n"
            "  image --st                  save mounted disk as disk.st\n"
            "  image --msa                 save mounted disk as disk.msa\n"
            "  image --dir                 extract mounted disk to ./disk/\n"
            "  image --in x.msa --st       convert x.msa to x.st\n"
            "  image --in mydir --msa      pack directory into disk.msa\n"
            "  image --st --out game.st    save mounted disk as game.st");
        return ST_NO_ERROR;
    }

    /* ---- cwd buffer (used for default output names) ---------------- */
    strncpy(szCwdBuf, g_line_ptCtx.szCwd, ST_MAX_PATH - 1);
    szCwdBuf[ST_MAX_PATH - 1] = '\0';
    uiCwdLen = strlen(szCwdBuf);
    if (uiCwdLen > 0 &&
        szCwdBuf[uiCwdLen - 1] != '/' &&
        szCwdBuf[uiCwdLen - 1] != '\\')
    {
        szCwdBuf[uiCwdLen]     = '/';
        szCwdBuf[uiCwdLen + 1] = '\0';
    }

    szExt = (eFmt == MOUNT_SAVE_MSA) ? "msa" : "st";

    /* ---- resolve source -------------------------------------------- */
    /* Priority: --in > mounted view > selected */
    if (!bHaveIn)
    {
        if (g_line_ptCtx.ptMountView == NULL)
        {
            /* No --in and no mounted view: fall back to selected */
            line_get_selected(szIn, ST_MAX_PATH);
            if (szIn[0] != '\0')
                bHaveIn = ST_TRUE;
        }
        /* else: mounted view is the implicit source — szIn stays empty,
         * g_line_ptCtx.ptMountView is used directly below.                   */
    }

    /* ---- Case: --dir extraction ------------------------------------ */
    if (bExtractDir)
    {
        /* Determine output directory */
        if (bHaveOut)
        {
            strncpy(szAutoDst, szOut, ST_MAX_PATH - 1);
            szAutoDst[ST_MAX_PATH - 1] = '\0';
        }
        else
        {
            snprintf(szAutoDst, sizeof(szAutoDst) + 4, "%sdisk", szCwdBuf);
            memset(&tStat, 0, sizeof(tStat));
            if (file_stat(szAutoDst, &tStat) == ST_NO_ERROR
             && tStat.bExists)
            {
                for (iExtNum = 2; iExtNum <= 99; iExtNum++)
                {
                    snprintf(szAutoDst, sizeof(szAutoDst) + 4,
                             "%sdisk%d", szCwdBuf, iExtNum);
                    memset(&tStat, 0, sizeof(tStat));
                    if (file_stat(szAutoDst, &tStat) != ST_NO_ERROR
                     || !tStat.bExists)
                        break;
                }
            }
        }

        /* Source: explicitly specified --in file */
        if (bHaveIn && line_is_disk_path(szIn))
        {
            memset(&tStat, 0, sizeof(tStat));
            if (file_stat(szIn, &tStat) != ST_NO_ERROR ||
                !tStat.bExists || tStat.bIsDir)
            {
                line_print_error("image --dir: source not found: '%s'", szIn);
                return ST_NO_ERROR;
            }
            eResult = mount_view_open(szIn, g_line_ptCtx.bRunning, &ptTmpView);
            if (eResult != ST_NO_ERROR || ptTmpView == NULL)
            {
                line_print_error("image --dir: failed to open disk '%s'",
                                 szIn);
                return ST_NO_ERROR;
            }
            eResult = mount_save_image(ptTmpView, MOUNT_SAVE_DIR, szAutoDst);
            mount_view_close(&ptTmpView);
        }
        else if (!bHaveIn && g_line_ptCtx.ptMountView != NULL)
        {
            /* Source: currently mounted image */
            eResult = mount_save_image(g_line_ptCtx.ptMountView,
                                       MOUNT_SAVE_DIR, szAutoDst);
        }
        else
        {
            line_print_warning(
                "image --dir: no source. "
                "Use --in <disk.st|msa> or open a disk with 'mount'.");
            return ST_NO_ERROR;
        }

        if (eResult == ST_NO_ERROR)
            line_print_msg("Extracted to: %s", szAutoDst);
        else
            line_print_error("Extraction failed.");
        return ST_NO_ERROR;
    }

    /* ---- Case: save / convert / pack ------------------------------- */
    /* Sub-case A: mounted view is the source */
    if (!bHaveIn && g_line_ptCtx.ptMountView != NULL)
    {
        if (!bHaveOut)
            snprintf(szOut, sizeof(szOut), "%.*sdisk.%s",
                     (int)(sizeof(szOut) - 9), szCwdBuf, szExt);
        if (bBootable)
            mount_make_bootable(g_line_ptCtx.ptMountView->ptImg);
        eResult = mount_save_image(g_line_ptCtx.ptMountView, eFmt, szOut);
        if (eResult == ST_NO_ERROR)
            line_print_msg("Image saved: %s", szOut);
        else
            line_print_error("Failed to save image.");
        line_update_console_title();
        return ST_NO_ERROR;
    }

    /* Sub-case B: --in is a disk file → file-to-file conversion */
    if (bHaveIn && line_is_disk_path(szIn))
    {
        memset(&tStat, 0, sizeof(tStat));
        if (file_stat(szIn, &tStat) != ST_NO_ERROR ||
            !tStat.bExists || tStat.bIsDir)
        {
            line_print_error("image: source not found: '%s'", szIn);
            return ST_NO_ERROR;
        }

        /* Default output: replace extension of source */
        if (!bHaveOut)
        {
            szDot = strrchr(szIn, '.');
            uiBaseLen = (szDot != NULL) ? (size_t)(szDot - szIn)
                                         : strlen(szIn);
            snprintf(szOut, sizeof(szOut), "%.*s.%s",
                     (int)uiBaseLen, szIn, szExt);
        }

        szDot = strrchr(szIn, '.');
        if (szDot != NULL &&
            (strcmp(szDot, ".st") == 0 || strcmp(szDot, ".ST") == 0))
            eResult = image_st_load(szIn, &ptConvImg);
        else
            eResult = image_msa_load(szIn, &ptConvImg);

        if (eResult != ST_NO_ERROR || ptConvImg == NULL)
        {
            line_print_error("image: failed to load '%s'", szIn);
            return ST_NO_ERROR;
        }

        if (bBootable)
            mount_make_bootable(ptConvImg);

        if (eFmt == MOUNT_SAVE_MSA)
            eResult = image_msa_save(ptConvImg, szOut);
        else
            eResult = image_st_save(ptConvImg, szOut);

        image_st_close(&ptConvImg);

        if (eResult == ST_NO_ERROR)
            line_print_msg("Image saved: %s", szOut);
        else
            line_print_error("Failed to save image.");

        line_update_console_title();
        return ST_NO_ERROR;
    }

    /* Sub-case C: --in is a directory (or fallback to selected/cwd) */
    {
        char szSrcDir[ST_MAX_PATH];

        if (bHaveIn)
        {
            strncpy(szSrcDir, szIn, ST_MAX_PATH - 1);
            szSrcDir[ST_MAX_PATH - 1] = '\0';
        }
        else if (szIn[0] != '\0') /* selected path from context */
        {
            strncpy(szSrcDir, szIn, ST_MAX_PATH - 1);
            szSrcDir[ST_MAX_PATH - 1] = '\0';
        }
        else
        {
            strncpy(szSrcDir, g_line_ptCtx.szCwd, ST_MAX_PATH - 1);
            szSrcDir[ST_MAX_PATH - 1] = '\0';
        }

        memset(&tStat, 0, sizeof(tStat));
        if (file_stat(szSrcDir, &tStat) != ST_NO_ERROR ||
            !tStat.bExists || !tStat.bIsDir)
        {
            line_print_warning(
                "image: no valid source. "
                "Use --in <dir|disk> or mount a disk first.");
            return ST_NO_ERROR;
        }

        if (!bHaveOut)
            snprintf(szOut, sizeof(szOut), "%.*sdisk.%s",
                     (int)(sizeof(szOut) - 9), szCwdBuf, szExt);

        eResult = mount_view_open(szSrcDir, g_line_ptCtx.bRunning, &ptTmpView);
        if (eResult != ST_NO_ERROR || ptTmpView == NULL)
        {
            line_print_error("image: failed to pack '%s'", szSrcDir);
            return ST_NO_ERROR;
        }

        if (bBootable)
            mount_make_bootable(ptTmpView->ptImg);

        eResult = mount_save_image(ptTmpView, eFmt, szOut);
        mount_view_close(&ptTmpView);

        if (eResult == ST_NO_ERROR)
            line_print_msg("Image saved: %s", szOut);
        else
            line_print_error("Failed to save image.");

        line_update_console_title();
        return ST_NO_ERROR;
    }
}

/* ------------------------------------------------------------------
 * UC20A: st2msa / msa2st batch conversion
 * ------------------------------------------------------------------ */

/*
 * Conversion direction context passed to the file_list_dir callback.
 */
typedef struct
{
    const char *szSrcExt;   /* source extension without dot: "st" | "msa" */
    const char *szDstExt;   /* dest   extension without dot: "msa"| "st"  */
    int         iConverted;
    int         iFailed;
} conv_ctx_t;

/*
 * line_conv_replace_ext() - Build output path by replacing the
 * extension of szSrc with szNewExt.
 *
 * Parameters:
 *   szSrc    [in]  : Source path ("foo/bar.st").
 *   szNewExt [in]  : New extension without dot ("msa").
 *   szOut    [out] : Destination buffer.
 *   uiOutLen [in]  : Buffer size.
 */
static void line_conv_replace_ext(const char *szSrc,
                                   const char *szNewExt,
                                   char       *szOut,
                                   size_t      uiOutLen)
{
    const char *pDot;
    size_t      uiBase;

    pDot = strrchr(szSrc, '.');
    if (pDot == NULL)
    {
        snprintf(szOut, uiOutLen, "%s.%s", szSrc, szNewExt);
        return;
    }
    uiBase = (size_t)(pDot - szSrc);
    snprintf(szOut, uiOutLen, "%.*s.%s", (int)uiBase, szSrc, szNewExt);
}

/*
 * line_conv_one() - Convert a single image file.
 *
 * Reads szSrcPath, writes szDstPath using the codec indicated by
 * szSrcExt/szDstExt.  Returns ST_NO_ERROR on success.
 */
static st_error_t line_conv_one(const char *szSrcPath,
                                 const char *szDstPath,
                                 const char *szSrcExt,
                                 const char *szDstExt)
{
    image_st_t *ptImg = NULL;
    st_error_t  eRet  = ST_ERROR;

    ST_UNUSED(szDstExt);  /* codec selected by szSrcExt, szDstExt unused */

    if (strcmp(szSrcExt, "st") == 0)
    {
        if (image_st_load(szSrcPath, &ptImg) != ST_NO_ERROR)
        {
            LOG_ERROR("st2msa: load failed: %s", szSrcPath);
            return ST_ERROR;
        }
        eRet = image_msa_save(ptImg, szDstPath);
        if (eRet != ST_NO_ERROR)
            LOG_ERROR("st2msa: save failed: %s", szDstPath);
    }
    else
    {
        if (image_msa_load(szSrcPath, &ptImg) != ST_NO_ERROR)
        {
            LOG_ERROR("msa2st: load failed: %s", szSrcPath);
            return ST_ERROR;
        }
        eRet = image_st_save(ptImg, szDstPath);
        if (eRet != ST_NO_ERROR)
            LOG_ERROR("msa2st: save failed: %s", szDstPath);
    }

    image_st_close(&ptImg);
    return eRet;
}

/* Recursive-capable variant using a nested struct */
typedef struct
{
    conv_ctx_t *ptConv;
    st_bool_t   bRecurse;
} conv_rec_ctx_t;

static void line_conv_rec_cb(const char        *szFullPath,
                              const char        *szName,
                              const file_stat_t *ptStat,
                              void              *pCtx);

static void line_conv_dir_rec(const char *szDir,
                               conv_ctx_t *ptConv,
                               st_bool_t   bRecurse)
{
    conv_rec_ctx_t tRec;

    tRec.ptConv   = ptConv;
    tRec.bRecurse = bRecurse;

    file_list_dir(szDir, ST_TRUE, line_conv_rec_cb, &tRec);
}

static void line_conv_rec_cb(const char        *szFullPath,
                              const char        *szName,
                              const file_stat_t *ptStat,
                              void              *pCtx)
{
    conv_rec_ctx_t *ptRec  = (conv_rec_ctx_t *)pCtx;
    conv_ctx_t     *ptConv = ptRec->ptConv;
    char            szDst[ST_MAX_PATH];
    file_stat_t     tStat;

    ST_UNUSED(szName);

    if (ptStat->bIsDir)
    {
        if (ptRec->bRecurse)
            line_conv_dir_rec(szFullPath, ptConv, ST_TRUE);
        return;
    }

    memset(&tStat, 0, sizeof(tStat));
    if (file_stat(szFullPath, &tStat) != ST_NO_ERROR)
        return;

    if (strcmp(tStat.szExt, ptConv->szSrcExt) != 0)
        return;

    line_conv_replace_ext(szFullPath, ptConv->szDstExt,
                          szDst, sizeof(szDst));

    line_print_msg("  %s -> %s", szFullPath, szDst);

    if (line_conv_one(szFullPath, szDst,
                      ptConv->szSrcExt, ptConv->szDstExt)
        == ST_NO_ERROR)
        ptConv->iConverted++;
    else
    {
        line_print_error("  FAILED: %s", szFullPath);
        ptConv->iFailed++;
    }
}

/*
 * line_cmd_convert() - Shared implementation for st2msa and msa2st.
 */
static st_error_t line_cmd_convert(const parsed_cmd_t *ptParsed,
                                    const char         *szSrcExt,
                                    const char         *szDstExt)
{
    char       szSrcPath[ST_MAX_PATH];
    char       szDstPath[ST_MAX_PATH];
    char       szDirPath[ST_MAX_PATH];
    char       szSel[ST_MAX_PATH];
    file_stat_t tStat;
    conv_ctx_t  tConv;
    st_bool_t   bAll     = ST_FALSE;
    st_bool_t   bRecurse = ST_FALSE;
    int         i;

    szSrcPath[0] = '\0';
    szDirPath[0] = '\0';

    
    LOG_TRACE("szSrcExt=%s szDstExt=%s ptParsed=%p", 
               szSrcExt, szDstExt, (void *)ptParsed);

    if (szSrcExt == NULL || szDstExt == NULL ||ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    
    /* Parse flags */
    for (i = 1; i < ptParsed->iArgc; i++)
    {
        if (strcmp(ptParsed->aszArgv[i], "--all") == 0)
        {
            bAll = ST_TRUE;
        }
        else if (strcmp(ptParsed->aszArgv[i], "-r") == 0)
        {
            bRecurse = ST_TRUE;
        }
        else if (strcmp(ptParsed->aszArgv[i], "--dir") == 0)
        {
            if (i + 1 < ptParsed->iArgc)
            {
                strncpy(szDirPath, ptParsed->aszArgv[i + 1],
                        sizeof(szDirPath) - 1);
                i++;
            }
            else
                line_print_warning("--dir requires a path argument.");
        }
        else
        {
            /* positional = source file path */
            if (szSrcPath[0] == '\0')
                strncpy(szSrcPath, ptParsed->aszArgv[i],
                        sizeof(szSrcPath) - 1);
            else
                line_print_warning("Extra argument ignored: %s",
                                   ptParsed->aszArgv[i]);
        }
    }

    /* --all mode: convert all matching files in a directory */
    if (bAll)
    {
        /* Resolve directory: --dir > positional > selected > cwd */
        if (szDirPath[0] != '\0')
        {
            /* explicit --dir already set */
        }
        else if (szSrcPath[0] != '\0')
        {
            strncpy(szDirPath, szSrcPath, sizeof(szDirPath) - 1);
        }
        else
        {
            line_get_selected(szSel, sizeof(szSel));
            if (szSel[0] != '\0')
            {
                memset(&tStat, 0, sizeof(tStat));
                file_stat(szSel, &tStat);
                if (tStat.bIsDir)
                    strncpy(szDirPath, szSel, sizeof(szDirPath) - 1);
                else
                    strncpy(szDirPath, g_line_ptCtx.szCwd,
                            sizeof(szDirPath) - 1);
            }
            else
                strncpy(szDirPath, g_line_ptCtx.szCwd,
                        sizeof(szDirPath) - 1);
        }

        memset(&tStat, 0, sizeof(tStat));
        if (file_stat(szDirPath, &tStat) != ST_NO_ERROR
        ||  !tStat.bExists || !tStat.bIsDir)
        {
            line_print_error("Not a directory: %s", szDirPath);
            return ST_NO_ERROR;
        }

        memset(&tConv, 0, sizeof(tConv));
        tConv.szSrcExt   = szSrcExt;
        tConv.szDstExt   = szDstExt;
        tConv.iConverted = 0;
        tConv.iFailed    = 0;

        line_print_msg("Converting .%s -> .%s in: %s%s",
                       szSrcExt, szDstExt, szDirPath,
                       bRecurse ? " (recursive)" : "");

        line_conv_dir_rec(szDirPath, &tConv, bRecurse);

        line_print_msg("Done: %d converted, %d failed.",
                       tConv.iConverted, tConv.iFailed);

        if (tConv.iFailed > 0)
            LOG_INFO("%d conversion failure(s) in %s", tConv.iFailed,
                     szDirPath);
        return ST_NO_ERROR;
    }

    /* Single-file mode */
    if (szSrcPath[0] == '\0')
    {
        line_get_selected(szSel, sizeof(szSel));
        if (szSel[0] != '\0')
            strncpy(szSrcPath, szSel, sizeof(szSrcPath) - 1);
    }

    if (szSrcPath[0] == '\0')
    {
        line_print_warning(
            "No file specified.  Select a .%s file with 'dir' "
            "or provide a path, or use --all to convert a directory.",
            szSrcExt);
        return ST_NO_ERROR;
    }

    memset(&tStat, 0, sizeof(tStat));
    if (file_stat(szSrcPath, &tStat) != ST_NO_ERROR
    ||  !tStat.bExists)
    {
        line_print_error("File not found: %s", szSrcPath);
        return ST_NO_ERROR;
    }
    if (tStat.bIsDir)
    {
        line_print_warning(
            "Path is a directory.  Use --all to convert all .%s "
            "files in a directory.", szSrcExt);
        return ST_NO_ERROR;
    }
    if (strcmp(tStat.szExt, szSrcExt) != 0)
    {
        line_print_warning("File is not a .%s image: %s",
                           szSrcExt, szSrcPath);
        return ST_NO_ERROR;
    }

    line_conv_replace_ext(szSrcPath, szDstExt,
                          szDstPath, sizeof(szDstPath));

    line_print_msg("%s -> %s", szSrcPath, szDstPath);

    if (line_conv_one(szSrcPath, szDstPath, szSrcExt, szDstExt)
        == ST_NO_ERROR)
        line_print_msg("Done.");
    else
        line_print_error("Conversion failed.");

    return ST_NO_ERROR;
}

static st_error_t line_cmd_st2msa(const parsed_cmd_t *ptParsed)
{
    return line_cmd_convert(ptParsed, "st", "msa");
}

static st_error_t line_cmd_msa2st(const parsed_cmd_t *ptParsed)
{
    return line_cmd_convert(ptParsed, "msa", "st");
}

/* ------------------------------------------------------------------
 * Dispatch table  (indexed by cmd_id_t value)
 * ------------------------------------------------------------------ */

/* Forward declaration — defined after line_read_rich() */
static st_error_t line_cmd_script(const parsed_cmd_t *ptParsed);

typedef st_error_t (*cmd_handler_fn)(const parsed_cmd_t *);

static const cmd_handler_fn g_line_aHandlers[CMD_COUNT] =
{
    /* CMD_NONE    (0) */ NULL,
    /* CMD_HELP       */ line_cmd_help,
    /* CMD_QUIT       */ line_cmd_quit,
    /* CMD_DIR        */ line_cmd_dir,
    /* CMD_LOAD       */ line_cmd_load,
    /* CMD_EDIT       */ line_cmd_edit,
    /* CMD_IMAGE      */ line_cmd_image,
    /* CMD_MOUNT      */ line_cmd_mount,
    /* CMD_WHERE      */ line_cmd_where,
    /* CMD_TRACE      */ line_cmd_trace,
    /* CMD_EXECUTE    */ line_cmd_execute,
    /* CMD_COLORS     */ line_cmd_colors,
    /* CMD_INFO       */ line_cmd_info,
    /* CMD_HISTORY    */ line_cmd_history,
    /* CMD_ST2MSA     */ line_cmd_st2msa,
    /* CMD_MSA2ST     */ line_cmd_msa2st,
    /* CMD_SCRIPT     */ line_cmd_script,
};

static st_error_t line_dispatch(const parsed_cmd_t *ptParsed)
{
    cmd_handler_fn pfnHandler;
    st_error_t     eResult;

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
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
        LOG_ERROR("command id out of range: %d",
                  (int)ptParsed->eCmd);
        return ST_ERROR;
    }

    pfnHandler = g_line_aHandlers[(int)ptParsed->eCmd];
    if (pfnHandler == NULL)
    {
        LOG_ERROR("NULL handler for command id %d",
                  (int)ptParsed->eCmd);
        return ST_ERROR;
    }

    eResult = pfnHandler(ptParsed);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("handler for '%s' returned ST_ERROR",
                  ptParsed->iArgc > 0 ? ptParsed->aszArgv[0] : "?");
    }

    return eResult;
}

/* ------------------------------------------------------------------
 * Rich line editor — helpers
 * ------------------------------------------------------------------ */

/*
 * line_build_prompt() - Build the contextual prompt string.
 *
 * Format: ST4Ever [T][basename]>
 * [T] shown when trace is open; [basename] when a file is selected.
 */
static void line_build_prompt(char                 *szOut,
                               size_t                uiMax)
{
    char       szSel[ST_MAX_PATH];
    const char *pBase;
    size_t     uiPos;
    int        iRet;

    if (szOut == NULL || uiMax == 0)
    {
        return;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return;
    }

    /* Base: coloured app name */
    iRet = snprintf(szOut, uiMax, "%s%s%s%s",
                    c_bold(), c_green(), ST_APP_NAME, c_reset());
    uiPos = (iRet > 0 && (size_t)iRet < uiMax) ? (size_t)iRet : uiMax - 1;

    /* [T] indicator when trace is open */
    if (trace_is_open() == ST_TRUE)
    {
        iRet = snprintf(szOut + uiPos, uiMax - uiPos,
                        "%s[T]%s", c_gray(), c_reset());
        if (iRet > 0 && (size_t)iRet < uiMax - uiPos)
        {
            uiPos += (size_t)iRet;
        }
    }

    /* [basename] indicator when a file is selected */
    szSel[0] = '\0';
    line_get_selected(szSel, sizeof(szSel));
    if (szSel[0] != '\0')
    {
        pBase = line_path_basename(szSel);
        if (pBase != NULL && pBase[0] != '\0')
        {
            iRet = snprintf(szOut + uiPos, uiMax - uiPos,
                            "%s[%.20s]%s",
                            c_gray(), pBase, c_reset());
            if (iRet > 0 && (size_t)iRet < uiMax - uiPos)
            {
                uiPos += (size_t)iRet;
            }
        }
    }

    /* Closing "> " */
    snprintf(szOut + uiPos, uiMax - uiPos, "> ");
}

/*
 * line_redraw() - Erase the current terminal line and redraw the
 * contextual prompt + buffer, with optional ghost text at cursor.
 *
 * Ghost text (szGhost) is shown in DIM colour at the cursor position
 * to preview a completion candidate.  Pass NULL for no ghost.
 */
static void line_redraw( const char           *szBuf,
                         size_t                uiLen,
                         size_t                uiCursor,
                         const char           *szGhost)
{
    char   szPrompt[256];
    size_t uiGhostLen;
    size_t uiMoveBack;

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return;
    }

    line_build_prompt(szPrompt, sizeof(szPrompt));

    /* Erase line and redraw from column 0 */
    printf("\r\033[2K%s", szPrompt);

    /* Chars before cursor */
    if (uiCursor > 0)
    {
        fwrite(szBuf, 1, uiCursor, stdout);
    }

    /* Ghost text at cursor (dim) */
    uiGhostLen = 0;
    if (szGhost != NULL && szGhost[0] != '\0' && g_line_ptCtx.bColors)
    {
        uiGhostLen = strlen(szGhost);
        printf("%s%s%s", c_dim(), szGhost, c_reset());
    }

    /* Chars after cursor */
    if (uiLen > uiCursor)
    {
        fwrite(szBuf + uiCursor, 1, uiLen - uiCursor, stdout);
    }

    /* Reposition cursor: move left past (remaining buf + ghost) */
    uiMoveBack = (uiLen - uiCursor) + uiGhostLen;
    if (uiMoveBack > 0)
    {
        printf("\033[%dD", (int)uiMoveBack);
    }

    fflush(stdout);
}

/*
 * line_shortcut() - Fill szBuf with a command name, redraw it as if
 * the user typed it, emit newline, and return ST_NO_ERROR to commit.
 */
static st_error_t line_shortcut(char                 *szBuf,
                                const char           *szCmd)
{
    size_t uiLen;

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    strncpy(szBuf, szCmd, ST_MAX_CMD - 1);
    szBuf[ST_MAX_CMD - 1] = '\0';
    uiLen = strlen(szBuf);
    line_redraw(szBuf, uiLen, uiLen, NULL);
    printf("\n");
    fflush(stdout);
    return ST_NO_ERROR;
}

/*
 * line_read_rich() - Read one command line using the raw line editor.
 *
 * Features: printable char insert, cursor movement, deletion, ESC
 * clear, CTRL shortcuts, history UP/DOWN, TAB completion with ghost
 * text.
 *
 * Adds the committed line to the history ring (non-empty only).
 *
 * Returns:
 *   ST_NO_ERROR  line committed in szBuf.
 *   ST_ERROR     stdin closed (EOF) or fatal read error.
 */
static st_error_t line_read_rich(char           *szBuf)
{
    /* Editing state */
    size_t     uiLen;
    size_t     uiCursor;
    int        iKey;
    st_error_t eResult;

    /* History browse state */
    char  szSavedInput[ST_MAX_CMD];
    int   iHistBrowse;    /* -1 = editing, >=0 = browsing entry idx  */
    int   iHistCount;
    int   iNewBrowse;

    /* Completion state */
    char       aaCompleteCands[LINE_COMPLETE_MAX][ST_MAX_PATH];
    int        iCompleteCount;
    int        iCompleteCur;   /* -1 = not in completion mode         */
    size_t     uiCompletePrefixLen;
    char       szGhost[ST_MAX_PATH];
    char       szPrefix[ST_MAX_CMD];
    size_t     uiTokenStart;
    size_t     uiScanIdx;
    st_bool_t  bIsFirstToken;
    const char *szFull;
    const char *szSuffix;
    size_t     uiSuffixLen;

    /* Prompt change detection for CON_KEY_TIMEOUT */
    char  szLastPrompt[256];

    /* Initialise */
    uiLen    = 0;
    uiCursor = 0;
    memset(szBuf, 0, ST_MAX_CMD);

    memset(szSavedInput, 0, sizeof(szSavedInput));
    iHistBrowse = -1;

    iCompleteCount      = 0;
    iCompleteCur        = -1;
    uiCompletePrefixLen = 0;
    szGhost[0]          = '\0';
    szLastPrompt[0]     = '\0';

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    /* Show initial empty prompt */
    line_redraw(szBuf, uiLen, uiCursor, NULL);
    line_build_prompt(szLastPrompt, sizeof(szLastPrompt));

    for (;;)
    {
        eResult = console_read_key(&iKey);
        if (eResult != ST_NO_ERROR || iKey == CON_KEY_EOF)
        {
            printf("\n");
            fflush(stdout);
            return ST_ERROR;
        }

        /* Timeout (200 ms, no keystroke): refresh prompt only when
         * the [T] or [file] indicators have actually changed.
         * Skipping the redraw when nothing changed prevents the cursor
         * from flickering while TAB ghost text is displayed. */
        if (iKey == CON_KEY_TIMEOUT)
        {
            char szNewPrompt[256];
            line_build_prompt(szNewPrompt, sizeof(szNewPrompt));
            if (strcmp(szNewPrompt, szLastPrompt) != 0)
            {
                strncpy(szLastPrompt, szNewPrompt, sizeof(szLastPrompt) - 1);
                szLastPrompt[sizeof(szLastPrompt) - 1] = '\0';
                line_redraw(szBuf, uiLen, uiCursor,
                            iCompleteCur >= 0 ? szGhost : NULL);
            }
            continue;
        }

        /* Clear completion state on any key that is not TAB */
        if (iKey != CON_KEY_TAB)
        {
            iCompleteCur = -1;
            szGhost[0]   = '\0';
        }

        /* ---- ENTER ------------------------------------------ */
        if (iKey == CON_KEY_ENTER || iKey == '\n')
        {
            if (uiLen > 0)
            {
                line_history_add(szBuf);
            }
            iHistBrowse = -1;
            printf("\n");
            fflush(stdout);
            return ST_NO_ERROR;
        }

        /* ---- ESC: clear buffer ------------------------------ */
        if (iKey == CON_KEY_ESC)
        {
            uiLen    = 0;
            uiCursor = 0;
            memset(szBuf, 0, ST_MAX_CMD);
            iHistBrowse = -1;
            line_redraw(szBuf, uiLen, uiCursor, NULL);
            continue;
        }

        /* ---- CTRL+C: cancel or quit if buffer empty --------- */
        if (iKey == CON_KEY_CTRL_C)
        {
            if (uiLen > 0)
            {
                uiLen    = 0;
                uiCursor = 0;
                memset(szBuf, 0, ST_MAX_CMD);
                iHistBrowse = -1;
                line_redraw(szBuf, uiLen, uiCursor, NULL);
            }
            else
            {
                return line_shortcut(szBuf, "quit");
            }
            continue;
        }

        /* ---- CTRL shortcuts: instant dispatch --------------- */
        if (iKey == CON_KEY_CTRL_Q)
        {
            return line_shortcut(szBuf, "quit");
        }
        if (iKey == CON_KEY_CTRL_H)
        {
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
        if (iKey == CON_KEY_CTRL_O)
        {
            return line_shortcut(szBuf, "mount");
        }
        if (iKey == CON_KEY_CTRL_W)
        {
            return line_shortcut(szBuf, "where");
        }
        if (iKey == CON_KEY_CTRL_X)
        {
            return line_shortcut(szBuf, "execute");
        }

        /* ---- TAB: completion -------------------------------- */
        if (iKey == CON_KEY_TAB)
        {
            if (iCompleteCur == -1)
            {
                /* Start new completion session.
                 * Scan for the start of the current token, treating
                 * backslash-escaped spaces (\<space>) as non-separators. */
                uiTokenStart  = 0;
                bIsFirstToken = ST_TRUE;
                uiScanIdx     = 0;
                while (uiScanIdx < uiCursor)
                {
                    if (szBuf[uiScanIdx] == '\\' &&
                        uiScanIdx + 1 < uiCursor)
                    {
                        uiScanIdx += 2; /* skip escaped char */
                        continue;
                    }
                    if (szBuf[uiScanIdx] == ' ')
                    {
                        uiTokenStart  = uiScanIdx + 1;
                        bIsFirstToken = ST_FALSE;
                    }
                    uiScanIdx++;
                }

                uiCompletePrefixLen = uiCursor - uiTokenStart;

                memset(szPrefix, 0, sizeof(szPrefix));
                memcpy(szPrefix, szBuf + uiTokenStart,
                       uiCompletePrefixLen);

                if (bIsFirstToken == ST_TRUE)
                {
                    iCompleteCount = line_complete_cmd_query(
                        szPrefix, aaCompleteCands,
                        LINE_COMPLETE_MAX);
                }
                else
                {
                    iCompleteCount = line_complete_path_query(
                        szPrefix, g_line_ptCtx.szCwd,
                        aaCompleteCands, LINE_COMPLETE_MAX);
                }

                if (iCompleteCount <= 0)
                {
                    /* No candidates: ignore TAB silently */
                    iCompleteCur = -1;
                    line_redraw(szBuf, uiLen, uiCursor, NULL);
                    continue;
                }

                if (iCompleteCount == 1)
                {
                    /* Unique match: insert suffix immediately */
                    szFull     = aaCompleteCands[0];
                    szSuffix   = szFull + uiCompletePrefixLen;
                    uiSuffixLen = strlen(szSuffix);

                    if (uiLen + uiSuffixLen < ST_MAX_CMD - 1)
                    {
                        memmove(szBuf + uiCursor + uiSuffixLen,
                                szBuf + uiCursor,
                                uiLen - uiCursor + 1);
                        memcpy(szBuf + uiCursor,
                               szSuffix, uiSuffixLen);
                        uiLen    += uiSuffixLen;
                        uiCursor += uiSuffixLen;
                    }
                    iCompleteCur = -1;
                    szGhost[0]   = '\0';
                    line_redraw(szBuf, uiLen, uiCursor, NULL);
                    continue;
                }

                /* P23bis: insert longest common prefix before cycling */
                {
                    size_t uiCommonLen;
                    size_t uiLen0;
                    int    iCIdx;

                    uiLen0      = strlen(aaCompleteCands[0]);
                    uiCommonLen = uiLen0;

                    for (iCIdx = 1; iCIdx < iCompleteCount; iCIdx++)
                    {
                        size_t uiLenC;
                        size_t uiPos;

                        uiLenC = strlen(aaCompleteCands[iCIdx]);
                        if (uiLenC < uiCommonLen)
                        {
                            uiCommonLen = uiLenC;
                        }
                        for (uiPos = 0; uiPos < uiCommonLen; uiPos++)
                        {
                            if (aaCompleteCands[0][uiPos]
                            !=  aaCompleteCands[iCIdx][uiPos])
                            {
                                uiCommonLen = uiPos;
                                break;
                            }
                        }
                    }

                    if (uiCommonLen > uiCompletePrefixLen)
                    {
                        const char *szComSuffix;
                        size_t      uiSuffLen;

                        szComSuffix = aaCompleteCands[0]
                                     + uiCompletePrefixLen;
                        uiSuffLen   = uiCommonLen
                                     - uiCompletePrefixLen;

                        if (uiLen + uiSuffLen < ST_MAX_CMD - 1)
                        {
                            memmove(szBuf + uiCursor + uiSuffLen,
                                    szBuf + uiCursor,
                                    uiLen - uiCursor + 1);
                            memcpy(szBuf + uiCursor,
                                   szComSuffix, uiSuffLen);
                            uiLen    += uiSuffLen;
                            uiCursor += uiSuffLen;
                            /* Update prefix so ghost suffix is correct */
                            uiCompletePrefixLen = uiCommonLen;
                        }
                    }
                }

                /* Multiple candidates: enter completion mode */
                iCompleteCur = 0;
            }
            else
            {
                /* Cycle to next candidate */
                iCompleteCur =
                    (iCompleteCur + 1) % iCompleteCount;
            }

            /* Update ghost text for current candidate */
            szFull = aaCompleteCands[iCompleteCur];
            strncpy(szGhost, szFull + uiCompletePrefixLen,
                    sizeof(szGhost) - 1);
            szGhost[sizeof(szGhost) - 1] = '\0';

            line_redraw(szBuf, uiLen, uiCursor, szGhost);
            continue;
        }

        /* ---- History: UP (go older) ------------------------- */
        if (iKey == CON_KEY_UP)
        {
            iHistCount = line_history_count();
            if (iHistCount == 0)
            {
                continue;
            }

            if (iHistBrowse == -1)
            {
                /* Save current buffer before browsing */
                strncpy(szSavedInput, szBuf, ST_MAX_CMD - 1);
                szSavedInput[ST_MAX_CMD - 1] = '\0';
                iNewBrowse = iHistCount - 1;
            }
            else if (iHistBrowse > 0)
            {
                iNewBrowse = iHistBrowse - 1;
            }
            else
            {
                continue; /* already at oldest */
            }

            iHistBrowse = iNewBrowse;
            if (line_history_get(iHistBrowse, szBuf,
                                  ST_MAX_CMD) == ST_NO_ERROR)
            {
                uiLen    = strlen(szBuf);
                uiCursor = uiLen;
                line_redraw(szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }

        /* ---- History: DOWN (go newer) ----------------------- */
        if (iKey == CON_KEY_DOWN)
        {
            if (iHistBrowse == -1)
            {
                continue; /* not browsing */
            }

            iHistCount = line_history_count();
            if (iHistBrowse < iHistCount - 1)
            {
                iHistBrowse++;
                if (line_history_get(iHistBrowse, szBuf,
                                      ST_MAX_CMD) == ST_NO_ERROR)
                {
                    uiLen    = strlen(szBuf);
                    uiCursor = uiLen;
                    line_redraw(szBuf, uiLen, uiCursor, NULL);
                }
            }
            else
            {
                /* Back to saved input */
                iHistBrowse = -1;
                strncpy(szBuf, szSavedInput, ST_MAX_CMD - 1);
                szBuf[ST_MAX_CMD - 1] = '\0';
                uiLen    = strlen(szBuf);
                uiCursor = uiLen;
                line_redraw(szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }

        /* ---- Cursor movement -------------------------------- */
        if (iKey == CON_KEY_LEFT)
        {
            if (uiCursor > 0)
            {
                uiCursor--;
                line_redraw(szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }
        if (iKey == CON_KEY_RIGHT)
        {
            if (uiCursor < uiLen)
            {
                uiCursor++;
                line_redraw(szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }
        if (iKey == CON_KEY_HOME)
        {
            uiCursor = 0;
            line_redraw(szBuf, uiLen, uiCursor, NULL);
            continue;
        }
        if (iKey == CON_KEY_END)
        {
            uiCursor = uiLen;
            line_redraw(szBuf, uiLen, uiCursor, NULL);
            continue;
        }

        /* ---- Deletion --------------------------------------- */
        if (iKey == CON_KEY_BACKSPACE)
        {
            if (uiCursor > 0)
            {
                memmove(szBuf + uiCursor - 1,
                        szBuf + uiCursor,
                        uiLen - uiCursor + 1);
                uiCursor--;
                uiLen--;
                iHistBrowse = -1;
                line_redraw(szBuf, uiLen, uiCursor, NULL);
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
                iHistBrowse  = -1;
                line_redraw(szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }

        /* ---- Printable character: insert at cursor ---------- */
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
                iHistBrowse  = -1;
                line_redraw(szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }

        /* All other keys: ignore silently */
    }
}

/* ------------------------------------------------------------------
 * Script (batch) mode
 * ------------------------------------------------------------------ */

static st_error_t line_exec_script(const char     *szPath)
{
    FILE         *pFile;
    char          szInput[ST_MAX_CMD];
    parsed_cmd_t  tParsed;
    st_error_t    eResult;
    size_t        uiLen;

    LOG_TRACE("szPath='%s'", szPath ? szPath : "NULL");

    if (szPath == NULL || szPath[0] == '\0')
    {
        LOG_ERROR("NULL/empty parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    pFile = fopen(szPath, "r");
    if (pFile == NULL)
    {
        LOG_ERROR("cannot open script '%s': %s",
                  szPath, strerror(errno));
        return ST_ERROR;
    }

    LOG_INFO("running script '%s'", szPath);

    while (g_line_ptCtx.bRunning == ST_TRUE
           && fgets(szInput, sizeof(szInput), pFile) != NULL)
    {
        /* Strip trailing CR/LF */
        uiLen = strlen(szInput);
        while (uiLen > 0
               && (szInput[uiLen - 1] == '\n'
               ||  szInput[uiLen - 1] == '\r'))
        {
            szInput[--uiLen] = '\0';
        }

        /* Skip comment lines and blank lines */
        if (szInput[0] == '#' || uiLen == 0)
            continue;

        eResult = line_parse_cmd(szInput, &tParsed);
        if (eResult == ST_NO_ERROR)
            line_dispatch(&tParsed);
    }

    fclose(pFile);
    LOG_INFO("script '%s' complete", szPath);
    return ST_NO_ERROR;
}

static st_error_t line_cmd_script(const parsed_cmd_t *ptParsed)
{
    char szPath[ST_MAX_PATH];

    LOG_TRACE("ptParsed=%p", (void *)ptParsed);

    if (ptParsed == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    if (ptParsed->iArgc < 2)
    {
        line_print_warning("script: usage: script <file>");
        return ST_NO_ERROR;
    }

    strncpy(szPath, ptParsed->aszArgv[1], ST_MAX_PATH - 1);
    szPath[ST_MAX_PATH - 1] = '\0';

    if (line_exec_script(szPath) != ST_NO_ERROR)
        line_print_error("script: failed to run '%s'.", szPath);

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API — lifecycle
 * ------------------------------------------------------------------ */

st_u64_t line_init(const char* szScriptFile)
{
    char       *pCwd;
    st_error_t  eResult;

    LOG_TRACE("Init console line context -- szScriptFile=%p", szScriptFile);

    /* -- [LINE]1. Reject any NULL incoming parameter -- */
    if (szScriptFile == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- [LINE]2. Log Information if already initialised -- */
    if (g_line_ptCtx.bRunning == ST_TRUE)
    {
        LOG_INFO("Already initialised");
        return (st_u64_t)&g_line_ptCtx;
    }
    
    g_line_ptCtx.bRunning = ST_TRUE;

    /* P24: auto-detect ANSI capability based on stdout TTY status */
    /* -- [LINE]3. Auto-detect ANSI capability -- */
#ifdef ST_PLATFORM_WINDOWS
    g_line_ptCtx.bColors = _isatty(_fileno(stdout)) ? ST_TRUE : ST_FALSE;
#else
    g_line_ptCtx.bColors = isatty(STDOUT_FILENO) ? ST_TRUE : ST_FALSE;
#endif

    /* -- [LINE]4. get current working directory -- */
    pCwd = getcwd(g_line_ptCtx.szCwd, ST_MAX_PATH);
    if (pCwd == NULL)
    {
        LOG_ERROR("getcwd() failed - using '.'");
        strncpy(g_line_ptCtx.szCwd, ".", ST_MAX_PATH - 1);
        g_line_ptCtx.szCwd[ST_MAX_PATH - 1] = '\0';
    }

    /* -- [LINE]5. Fills context struture fields -- */
    /* Create the mutex protecting szSelected */
    eResult = platform_mutex_create(&g_line_ptCtx.ptSelectedMutex);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("platform_mutex_create failed for ptSelectedMutex");
        return ST_ERROR;
    }

    /* Check if any script in given in parameter */
    if (szScriptFile[0] != '\0')
    {
        strncpy(g_line_ptCtx.szScriptFile, szScriptFile,
                ST_MAX_PATH - 1);
        g_line_ptCtx.szScriptFile[ST_MAX_PATH - 1] = '\0';
    }

    /* -- [LINE]6. Load console commands history -- */
    /* Reset count before load: -1 is the uninitialised sentinel; 0 means
     * "no entries yet" and is the correct baseline for line_hist_add(). */
    g_line_ptCtx.iHistCount = 0;
    line_history_load(NULL);

    LOG_INFO("console initialised, cwd='%s'", g_line_ptCtx.szCwd);
    return (st_u64_t)&g_line_ptCtx;
}

st_error_t line_run()
{
    char         szInput[ST_MAX_CMD];
    parsed_cmd_t tParsed;
    st_error_t   eResult;
    st_bool_t    bRawMode;
    size_t       uiLen;

    LOG_TRACE("Running the console command line");

    /* -- 1. Check if console line is initialized - return if not */
    if (!g_line_ptCtx.bRunning)
    {
        LOG_ERROR("Use line_init() before use of any line_*() function");
        return ST_ERROR;
    }

    /* -- Batch / script mode ----------------------------------- */
    if (g_line_ptCtx.szScriptFile[0] != '\0')
    {
        return line_exec_script(g_line_ptCtx.szScriptFile);
    }

    /* -- Banner ------------------------------------------------ */
    printf("\n");
    printf("%s%s"
           "  +-----------------------------------------+\n"
           "  |  %-40s|\n"
           "  |  %-40s|\n"
           "  |  %-40s|\n"
           "  +-----------------------------------------+\n"
           "%s\n",
           c_bold(), c_cyan(),
           ST_APP_NAME " " ST_APP_VERSION,
           ST_APP_DESC,
           "Type 'help' for available commands.",
           c_reset());

    LOG_INFO("console loop starting");

    /* P8: set initial console title */
    line_update_console_title();

    /* -- Switch to raw mode for the rich line editor ----------- */
    bRawMode = ST_FALSE;
    eResult  = console_set_raw();
    if (eResult == ST_NO_ERROR)
    {
        bRawMode = ST_TRUE;
    }
    else
    {
        LOG_INFO("stdin not a TTY - using plain fgets() input");
    }

    /* -- Main loop --------------------------------------------- */
    while (g_line_ptCtx.bRunning == ST_TRUE)
    {
        if (bRawMode == ST_TRUE)
        {
            eResult = line_read_rich(szInput);
            if (eResult != ST_NO_ERROR)
            {
                LOG_INFO("stdin EOF - requesting shutdown");
                g_line_ptCtx.bRunning = ST_FALSE;
                break;
            }
        }
        else
        {
            /* Plain fallback (non-TTY: CI, piped input) */
            printf(ST_APP_NAME "> ");
            fflush(stdout);

            if (fgets(szInput, sizeof(szInput), stdin) == NULL)
            {
                printf("\n");
                LOG_INFO("stdin EOF - requesting shutdown");
                g_line_ptCtx.bRunning = ST_FALSE;
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

        eResult = line_parse_cmd(szInput, &tParsed);
        if (eResult != ST_NO_ERROR)
        {
            LOG_ERROR("line_parse_cmd failed for input '%s'", szInput);
            continue;
        }

        eResult = line_dispatch(&tParsed);
        if (eResult != ST_NO_ERROR)
        {
            LOG_ERROR("line_dispatch returned error for '%s'", szInput);
            /* Non-fatal: log and continue */
        }

        /* P8: refresh console title after each command (state may change) */
        line_update_console_title();
    }

    /* -- Restore terminal mode --------------------------------- */
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

st_error_t line_shutdown()
{
    LOG_TRACE("Shutdown requested");

    /* Close any open dir view */
    if (g_line_ptCtx.ptDirView != NULL)
    {
        dir_close(&g_line_ptCtx.ptDirView);
        g_line_ptCtx.ptDirView = NULL;
    }

    /* Close any open text edit view */
    if (g_line_ptCtx.ptEditTxtView != NULL)
    {
        edit_txt_close(&g_line_ptCtx.ptEditTxtView);
        g_line_ptCtx.ptEditTxtView = NULL;
    }

    /* Close any open hex edit view */
    if (g_line_ptCtx.ptEditHexView != NULL)
    {
        edit_hex_close(&g_line_ptCtx.ptEditHexView);
        g_line_ptCtx.ptEditHexView = NULL;
    }

    /* Close any open mount view */
    if (g_line_ptCtx.ptMountView != NULL)
    {
        mount_view_close(&g_line_ptCtx.ptMountView);
        g_line_ptCtx.ptMountView = NULL;
    }

    /* Close any open execution session */
    if (exec_is_open())
    {
        exec_close();
    }

    /* Save history */
    line_history_save(NULL);

    /* Destroy the selected-path mutex */
    if (g_line_ptCtx.ptSelectedMutex != NULL)
    {
        platform_mutex_destroy(&g_line_ptCtx.ptSelectedMutex);
        g_line_ptCtx.ptSelectedMutex = NULL;
    }

    memset(&g_line_ptCtx, 0, sizeof(line_context_t));
    LOG_INFO("console shutdown complete");
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API — console output
 * ------------------------------------------------------------------ */

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

    printf("%s  Warning: ", c_yellow());
    va_start(vaArgs, szFmt);
    vprintf(szFmt, vaArgs);
    va_end(vaArgs);
    printf("%s\n", c_reset());
}

void line_print_error(const char *szFmt, ...)
{
    va_list vaArgs;

    if (szFmt == NULL)
    {
        return;
    }

    printf("%s  Error: ", c_red());
    va_start(vaArgs, szFmt);
    vprintf(szFmt, vaArgs);
    va_end(vaArgs);
    printf("%s\n", c_reset());
}

char* line_get_current_dir()
{
    return g_line_ptCtx.szCwd;
}