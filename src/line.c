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
 *        --script: batch mode via line_run_script().
 *        Mutex-protected szSelected via line_set/get_selected().
 *        CTRL conflict resolution: TAB reserved for completion
 *        (CMD_IMAGE via "i"/"image" only); ENTER reserved for
 *        commit (CMD_MOUNT via "m"/"mount" only).
 * UC5:   where (cwd + selection), info (dashboard), history [N].
 *        P8:  line_update_console_title() — SetConsoleTitleA/OSC.
 *        P23bis: TAB inserts longest common prefix before cycling.
 *        P24: g_line_bColors = isatty(stdout) at line_init().
 *        trace clear (P27), trace level <lvl> (P28) sub-commands.
 */

#include "line.h"
#include "console.h"
#include "dir.h"
#include "edit_txt.h"
#include "edit_hex.h"
#include "load.h"
#include "file.h"
#include "trace.h"

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
 * ANSI colour helpers — runtime-toggled via g_line_bColors
 * ------------------------------------------------------------------ */

/* P24: initialised to ST_FALSE; line_init() sets it via isatty(). */
static st_bool_t g_line_bColors = ST_FALSE;

static inline const char *c_reset(void)
{
    return g_line_bColors ? "\033[0m"  : "";
}
static inline const char *c_bold(void)
{
    return g_line_bColors ? "\033[1m"  : "";
}
static inline const char *c_dim(void)
{
    return g_line_bColors ? "\033[2m"  : "";
}
static inline const char *c_green(void)
{
    return g_line_bColors ? "\033[92m" : "";
}
static inline const char *c_yellow(void)
{
    return g_line_bColors ? "\033[93m" : "";
}
static inline const char *c_red(void)
{
    return g_line_bColors ? "\033[91m" : "";
}
static inline const char *c_cyan(void)
{
    return g_line_bColors ? "\033[96m" : "";
}
static inline const char *c_gray(void)
{
    return g_line_bColors ? "\033[90m" : "";
}

/* Forward declaration — defined in the string helpers section below */
static const char *line_path_basename(const char *szPath);

/* ------------------------------------------------------------------
 * Console title update (P8)
 * ------------------------------------------------------------------ */

void line_update_console_title(const line_context_t *ptCtx)
{
    char        szTitle[256];
    char        szSel[ST_MAX_PATH];
    const char *pBase;
    const char *pCwd;
    int         iRet;

    pCwd      = (ptCtx != NULL) ? ptCtx->szCwd : "";
    szSel[0]  = '\0';

    if (ptCtx != NULL)
    {
        line_get_selected(ptCtx, szSel, sizeof(szSel));
    }

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
        return; /* snprintf error */
    }

#ifdef ST_PLATFORM_WINDOWS
    SetConsoleTitleA(szTitle);
#else
    printf("\033]0;%s\007", szTitle);
    fflush(stdout);
#endif
}

/* ------------------------------------------------------------------
 * Module-level state
 * ------------------------------------------------------------------ */

static dir_view_t      *g_line_ptDirView     = NULL;
static edit_txt_view_t *g_line_ptEditTxtView = NULL;
static edit_hex_view_t *g_line_ptEditHexView = NULL;

/* ------------------------------------------------------------------
 * History ring buffer
 * ------------------------------------------------------------------ */

static char g_line_aHistory[LINE_HISTORY_MAX][ST_MAX_CMD];
static int  g_line_iHistCount = 0;   /* valid entries, 0..LINE_HISTORY_MAX */
static int  g_line_iHistHead  = 0;   /* next write slot                    */

/* Map virtual index (0=oldest, count-1=newest) to physical slot. */
static int line_hist_phys(int iVirt)
{
    return (g_line_iHistHead - g_line_iHistCount + iVirt
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
      "Mount a directory as the ST floppy drive A:\\" },

    { "umount",  "u", CMD_UMOUNT,
      "umount",
      "Unmount the ST floppy A:\\ (save image if changed)" },

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
    if (g_line_iHistCount > 0)
    {
        iLastPhys = line_hist_phys(g_line_iHistCount - 1);
        if (strcmp(g_line_aHistory[iLastPhys], szCmd) == 0)
        {
            return ST_NO_ERROR;
        }
    }

    strncpy(g_line_aHistory[g_line_iHistHead], szCmd, ST_MAX_CMD - 1);
    g_line_aHistory[g_line_iHistHead][ST_MAX_CMD - 1] = '\0';

    g_line_iHistHead = (g_line_iHistHead + 1) % LINE_HISTORY_MAX;
    if (g_line_iHistCount < LINE_HISTORY_MAX)
    {
        g_line_iHistCount++;
    }

    return ST_NO_ERROR;
}

int line_history_count(void)
{
    return g_line_iHistCount;
}

st_error_t line_history_get(int iVirtIdx, char *szBuf, size_t uiLen)
{
    int iPhys;

    if (szBuf == NULL || uiLen == 0)
    {
        return ST_ERROR;
    }
    if (iVirtIdx < 0 || iVirtIdx >= g_line_iHistCount)
    {
        return ST_ERROR;
    }

    iPhys = line_hist_phys(iVirtIdx);
    strncpy(szBuf, g_line_aHistory[iPhys], uiLen - 1);
    szBuf[uiLen - 1] = '\0';

    return ST_NO_ERROR;
}

void line_history_clear(void)
{
    g_line_iHistCount = 0;
    g_line_iHistHead  = 0;
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

    for (iVirt = 0; iVirt < g_line_iHistCount; iVirt++)
    {
        iPhys = line_hist_phys(iVirt);
        fprintf(pFile, "%s\n", g_line_aHistory[iPhys]);
    }

    fclose(pFile);
    LOG_INFO("history saved to '%s' (%d entries)",
             szPath, g_line_iHistCount);
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
             szPath, g_line_iHistCount);
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

int line_complete_path_query(const char *szPrefix,
                              const char *szCwd,
                              char       (*aOut)[ST_MAX_PATH],
                              int         iMaxOut)
{
    char           szDir[ST_MAX_PATH];
    char           szNamePfx[ST_MAX_PATH];
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

    line_split_path(szPrefix, szDir, sizeof(szDir),
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

        /* Build candidate via explicit memcpy to avoid snprintf
         * truncation warning. Skip if total length would overflow. */
        {
            const char *szDirPfx;
            size_t      uiDirLen;
            size_t      uiNameLen;
            size_t      uiPos;

            szDirPfx = (strcmp(szDir, ".") == 0) ? "" : szDir;
            uiDirLen  = strlen(szDirPfx);
            uiNameLen = strlen(pEntry->d_name);
            if (uiDirLen + uiNameLen + 2 >= ST_MAX_PATH)
            {
                continue;
            }
            uiPos = 0;
            memcpy(aOut[iCount], szDirPfx, uiDirLen);
            uiPos += uiDirLen;
            memcpy(aOut[iCount] + uiPos, pEntry->d_name, uiNameLen);
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
    g_line_bColors = bColors;
}

st_bool_t line_get_colors(void)
{
    return g_line_bColors;
}

/* ------------------------------------------------------------------
 * Thread-safe selected path accessors
 * ------------------------------------------------------------------ */

st_error_t line_set_selected(line_context_t *ptCtx,
                               const char     *szPath)
{
    if (ptCtx == NULL)
    {
        LOG_ERROR("ptCtx is NULL");
        return ST_ERROR;
    }

    if (ptCtx->ptSelectedMutex != NULL)
    {
        platform_mutex_lock(ptCtx->ptSelectedMutex);
    }

    if (szPath == NULL)
    {
        ptCtx->szSelected[0] = '\0';
    }
    else
    {
        strncpy(ptCtx->szSelected, szPath, ST_MAX_PATH - 1);
        ptCtx->szSelected[ST_MAX_PATH - 1] = '\0';
    }

    if (ptCtx->ptSelectedMutex != NULL)
    {
        platform_mutex_unlock(ptCtx->ptSelectedMutex);
    }

    return ST_NO_ERROR;
}

st_error_t line_get_selected(const line_context_t *ptCtx,
                               char                 *szBuf,
                               size_t                uiLen)
{
    if (ptCtx == NULL || szBuf == NULL || uiLen == 0)
    {
        LOG_ERROR("NULL parameter or uiLen==0");
        return ST_ERROR;
    }

    if (ptCtx->ptSelectedMutex != NULL)
    {
        platform_mutex_lock(
            (st_mutex_t *)ptCtx->ptSelectedMutex);
    }

    strncpy(szBuf, ptCtx->szSelected, uiLen - 1);
    szBuf[uiLen - 1] = '\0';

    if (ptCtx->ptSelectedMutex != NULL)
    {
        platform_mutex_unlock(
            (st_mutex_t *)ptCtx->ptSelectedMutex);
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

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

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

static st_error_t line_cmd_quit(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

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

    LOG_INFO("quit requested");
    return ST_NO_ERROR;
}

static st_error_t line_cmd_trace(const parsed_cmd_t *ptParsed,
                                  line_context_t     *ptCtx)
{
    st_error_t  eResult;
    const char *szArg;

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

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
            /* P26: LOG_TRACE disabled by default on toggle-open to prevent
             * flooding the view.  'trace on' enables it explicitly. */
            trace_set_trace_enabled(ST_FALSE);
            line_print_msg("Trace console opened (LOG_TRACE off — use 'trace on' to enable).");
        }
        return ST_NO_ERROR;
    }

    szArg = ptParsed->aszArgv[1];

    if (ptParsed->iArgc > 2)
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
                "trace level: expected trace|info|error");
            return ST_NO_ERROR;
        }

        szLevel = ptParsed->aszArgv[2];

        if (strcmp(szLevel, "trace") == 0)
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
        else
        {
            line_print_warning(
                "trace level: unknown level '%s'  "
                "- use: trace|info|error",
                szLevel);
            return ST_NO_ERROR;
        }

        trace_set_view_level(eNewLevel);
        line_print_msg("Trace: view level set to '%s'", szLevel);
    }
    else
    {
        line_print_warning(
            "trace: unknown argument '%s'  "
            "- use: trace | trace on | trace off | "
            "trace clear | trace level trace|info|error",
            szArg);
    }

    return ST_NO_ERROR;
}

static st_error_t line_cmd_dir(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    const char *szPath;
    st_bool_t   bShowHidden;
    st_error_t  eResult;
    int         iArg;

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

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
                   szPath ? " for "              : "",
                   szPath ? szPath               : "",
                   bShowHidden ? " (showing hidden files)" : "");
    return ST_NO_ERROR;
}

static st_error_t line_cmd_colors(const parsed_cmd_t *ptParsed,
                                   line_context_t     *ptCtx)
{
    const char *szArg;

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    if (ptParsed->iArgc == 1)
    {
        /* Toggle */
        g_line_bColors = (g_line_bColors == ST_TRUE)
                         ? ST_FALSE : ST_TRUE;
        line_print_msg("Colors: %s",
                       g_line_bColors == ST_TRUE ? "ON" : "OFF");
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
        g_line_bColors = ST_TRUE;
        line_print_msg("Colors: ON");
    }
    else if (strcmp(szArg, "off") == 0)
    {
        g_line_bColors = ST_FALSE;
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

static st_error_t line_cmd_load(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    char        szPath[ST_MAX_PATH];
    char        szSel[ST_MAX_PATH];
    file_stat_t tStat;
    st_error_t  eResult;
    int         i;
    const load_state_t *ptState;

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
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
        line_get_selected(ptCtx, szSel, sizeof(szSel));
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

    ptState = load_get_state();
    if (ptState->eType == LOAD_TYPE_PRG)
    {
        line_print_msg("Loaded PRG '%s' at ST:0x%06X (%u bytes, "
                       "fixup deferred to UC15).",
                       szPath,
                       (unsigned)ptState->uiLoadAddr,
                       (unsigned)ptState->uiSize);
    }
    else
    {
        line_print_msg("Loaded '%s' at ST:0x%06X (%u bytes).",
                       szPath,
                       (unsigned)ptState->uiLoadAddr,
                       (unsigned)ptState->uiSize);
    }

    line_update_console_title(ptCtx);
    return ST_NO_ERROR;
}

/* line_cmd_edit() — open text or binary editor on a file.
 * UC8: routes text files to edit_txt; binary/image = stub. */
static st_error_t line_cmd_edit(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    char        szPath[ST_MAX_PATH];
    char        szSel[ST_MAX_PATH];
    file_stat_t tStat;
    st_error_t  eResult;
    int         i;

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    szPath[0] = '\0';
    for (i = 1; i < ptParsed->iArgc; i++)
    {
        if (ptParsed->aszArgv[i][0] != '-')
        {
            strncpy(szPath, ptParsed->aszArgv[i], ST_MAX_PATH - 1);
            szPath[ST_MAX_PATH - 1] = '\0';
            break;
        }
    }

    if (szPath[0] == '\0')
    {
        szSel[0] = '\0';
        line_get_selected(ptCtx, szSel, sizeof(szSel));
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

    /* Disk images → redirect to mount */
    if (strcmp(tStat.szExt, "st")  == 0
     || strcmp(tStat.szExt, "msa") == 0
     || strcmp(tStat.szExt, "stx") == 0)
    {
        line_print_warning("'%s' is a disk image. Use 'mount'.",
                           szPath);
        return ST_NO_ERROR;
    }

    /* ATARI ST binaries and generic binary extensions → hex editor */
    if (strcmp(tStat.szExt, "prg") == 0
     || strcmp(tStat.szExt, "ttp") == 0
     || strcmp(tStat.szExt, "tos") == 0
     || strcmp(tStat.szExt, "bin") == 0
     || strcmp(tStat.szExt, "raw") == 0)
    {
        if (g_line_ptEditHexView != NULL)
        {
            eResult = edit_hex_close(&g_line_ptEditHexView);
            if (eResult != ST_NO_ERROR)
                line_print_warning(
                    "edit: could not close previous hex view");
            g_line_ptEditHexView = NULL;
        }
        eResult = edit_hex_open(szPath, ptCtx,
                                 &g_line_ptEditHexView);
        if (eResult != ST_NO_ERROR)
        {
            line_print_error("edit: failed to open '%s'.", szPath);
            return ST_NO_ERROR;
        }
        line_print_msg("Hex-editing '%s' (%zu bytes).",
                       szPath,
                       g_line_ptEditHexView
                       ? g_line_ptEditHexView->uiSize : (size_t)0);
        line_update_console_title(ptCtx);
        return ST_NO_ERROR;
    }

    /* All other files → text editor */
    if (g_line_ptEditTxtView != NULL)
    {
        eResult = edit_txt_close(&g_line_ptEditTxtView);
        if (eResult != ST_NO_ERROR)
            line_print_warning("edit: could not close previous view");
        g_line_ptEditTxtView = NULL;
    }

    eResult = edit_txt_open(szPath, ptCtx, &g_line_ptEditTxtView);
    if (eResult != ST_NO_ERROR)
    {
        line_print_error("edit: failed to open '%s'.", szPath);
        return ST_NO_ERROR;
    }

    line_print_msg("Editing '%s' (%d lines).",
                   szPath,
                   g_line_ptEditTxtView
                   ? g_line_ptEditTxtView->iLineCount : 0);
    line_update_console_title(ptCtx);
    return ST_NO_ERROR;
}

static st_error_t line_cmd_where(const parsed_cmd_t *ptParsed,
                                  line_context_t     *ptCtx)
{
    char szSel[ST_MAX_PATH];

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    if (ptParsed->iArgc > 1)
    {
        line_print_warning("where: ignoring extra arguments");
    }

    line_print_msg("Working directory : %s%s%s",
                   c_cyan(), ptCtx->szCwd, c_reset());

    szSel[0] = '\0';
    line_get_selected(ptCtx, szSel, sizeof(szSel));

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

    LOG_INFO("where: cwd='%s' sel='%s'", ptCtx->szCwd, szSel);
    return ST_NO_ERROR;
}

static st_error_t line_cmd_info(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    char szSel[ST_MAX_PATH];

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    if (ptParsed->iArgc > 1)
    {
        line_print_warning("info: ignoring extra arguments");
    }

    szSel[0] = '\0';
    line_get_selected(ptCtx, szSel, sizeof(szSel));

    printf("\n");
    printf("%s%s  === ST4Ever %s Status ===\n%s",
           c_bold(), c_cyan(), ST_APP_VERSION, c_reset());

    line_print_msg("Working dir  : %s%s%s",
                   c_cyan(), ptCtx->szCwd, c_reset());

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
                   g_line_bColors ? c_green() : c_gray(),
                   g_line_bColors ? "on" : "off",
                   c_reset());

    line_print_msg("History      : %d entr%s",
                   line_history_count(),
                   line_history_count() == 1 ? "y" : "ies");

    /* Stubs — future UCs */
    line_print_msg("Disk mounted : %s(none)%s", c_gray(), c_reset());

    {
        const load_state_t *ptLoadState = load_get_state();
        if (ptLoadState->bLoaded)
        {
            const char *szType;
            szType = (ptLoadState->eType == LOAD_TYPE_PRG)
                     ? "PRG-stub" : "binary";
            line_print_msg("Binary       : %s%s%s (%s, ST:0x%06X, "
                           "%u bytes)",
                           c_green(),
                           line_path_basename(ptLoadState->szPath),
                           c_reset(),
                           szType,
                           (unsigned)ptLoadState->uiLoadAddr,
                           (unsigned)ptLoadState->uiSize);
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

static st_error_t line_cmd_history(const parsed_cmd_t *ptParsed,
                                    line_context_t     *ptCtx)
{
    int  iCount;
    int  iN;
    int  iStart;
    int  iVirt;
    char szEntry[ST_MAX_CMD];

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);

    ST_UNUSED(ptCtx);

    if (ptParsed == NULL || ptCtx == NULL)
    {
        LOG_ERROR("NULL parameter");
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

static st_error_t line_cmd_stub(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    const char *szCmd;

    LOG_TRACE("ptParsed=%p ptCtx=%p",
              (void *)ptParsed, (void *)ptCtx);
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
 * Dispatch table  (indexed by cmd_id_t value)
 * ------------------------------------------------------------------ */

typedef st_error_t (*cmd_handler_fn)(const parsed_cmd_t *,
                                      line_context_t *);

static const cmd_handler_fn g_line_aHandlers[CMD_COUNT] =
{
    /* CMD_NONE    (0) */ NULL,
    /* CMD_HELP       */ line_cmd_help,
    /* CMD_QUIT       */ line_cmd_quit,
    /* CMD_DIR        */ line_cmd_dir,
    /* CMD_LOAD       */ line_cmd_load,
    /* CMD_EDIT       */ line_cmd_edit,
    /* CMD_IMAGE      */ line_cmd_stub,
    /* CMD_MOUNT      */ line_cmd_stub,
    /* CMD_UMOUNT     */ line_cmd_stub,
    /* CMD_WHERE      */ line_cmd_where,
    /* CMD_TRACE      */ line_cmd_trace,
    /* CMD_EXECUTE    */ line_cmd_stub,
    /* CMD_COLORS     */ line_cmd_colors,
    /* CMD_INFO       */ line_cmd_info,
    /* CMD_HISTORY    */ line_cmd_history,
};

static st_error_t line_dispatch(const parsed_cmd_t *ptParsed,
                                 line_context_t     *ptCtx)
{
    cmd_handler_fn pfnHandler;
    st_error_t     eResult;

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

    eResult = pfnHandler(ptParsed, ptCtx);
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
static void line_build_prompt(const line_context_t *ptCtx,
                               char                 *szOut,
                               size_t                uiMax)
{
    char       szSel[ST_MAX_PATH];
    const char *pBase;
    size_t     uiPos;
    int        iRet;

    if (ptCtx == NULL || szOut == NULL || uiMax == 0)
    {
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
    line_get_selected(ptCtx, szSel, sizeof(szSel));
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
static void line_redraw(const line_context_t *ptCtx,
                         const char           *szBuf,
                         size_t                uiLen,
                         size_t                uiCursor,
                         const char           *szGhost)
{
    char   szPrompt[256];
    size_t uiGhostLen;
    size_t uiMoveBack;

    line_build_prompt(ptCtx, szPrompt, sizeof(szPrompt));

    /* Erase line and redraw from column 0 */
    printf("\r\033[2K%s", szPrompt);

    /* Chars before cursor */
    if (uiCursor > 0)
    {
        fwrite(szBuf, 1, uiCursor, stdout);
    }

    /* Ghost text at cursor (dim) */
    uiGhostLen = 0;
    if (szGhost != NULL && szGhost[0] != '\0' && g_line_bColors)
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
static st_error_t line_shortcut(const line_context_t *ptCtx,
                                  char                 *szBuf,
                                  const char           *szCmd)
{
    size_t uiLen;

    strncpy(szBuf, szCmd, ST_MAX_CMD - 1);
    szBuf[ST_MAX_CMD - 1] = '\0';
    uiLen = strlen(szBuf);
    line_redraw(ptCtx, szBuf, uiLen, uiLen, NULL);
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
static st_error_t line_read_rich(line_context_t *ptCtx,
                                  char           *szBuf)
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

    /* Show initial empty prompt */
    line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
    line_build_prompt(ptCtx, szLastPrompt, sizeof(szLastPrompt));

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
            line_build_prompt(ptCtx, szNewPrompt, sizeof(szNewPrompt));
            if (strcmp(szNewPrompt, szLastPrompt) != 0)
            {
                strncpy(szLastPrompt, szNewPrompt, sizeof(szLastPrompt) - 1);
                szLastPrompt[sizeof(szLastPrompt) - 1] = '\0';
                line_redraw(ptCtx, szBuf, uiLen, uiCursor,
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
            line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
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
                line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
            }
            else
            {
                return line_shortcut(ptCtx, szBuf, "quit");
            }
            continue;
        }

        /* ---- CTRL shortcuts: instant dispatch --------------- */
        if (iKey == CON_KEY_CTRL_Q)
        {
            return line_shortcut(ptCtx, szBuf, "quit");
        }
        if (iKey == CON_KEY_CTRL_H)
        {
            return line_shortcut(ptCtx, szBuf, "help");
        }
        if (iKey == CON_KEY_CTRL_T)
        {
            return line_shortcut(ptCtx, szBuf, "trace");
        }
        if (iKey == CON_KEY_CTRL_D)
        {
            return line_shortcut(ptCtx, szBuf, "dir");
        }
        if (iKey == CON_KEY_CTRL_L)
        {
            return line_shortcut(ptCtx, szBuf, "load");
        }
        if (iKey == CON_KEY_CTRL_E)
        {
            return line_shortcut(ptCtx, szBuf, "edit");
        }
        if (iKey == CON_KEY_CTRL_U)
        {
            return line_shortcut(ptCtx, szBuf, "umount");
        }
        if (iKey == CON_KEY_CTRL_W)
        {
            return line_shortcut(ptCtx, szBuf, "where");
        }
        if (iKey == CON_KEY_CTRL_X)
        {
            return line_shortcut(ptCtx, szBuf, "execute");
        }

        /* ---- TAB: completion -------------------------------- */
        if (iKey == CON_KEY_TAB)
        {
            if (iCompleteCur == -1)
            {
                /* Start new completion session */
                uiTokenStart  = 0;
                bIsFirstToken = ST_TRUE;
                for (uiScanIdx = 0; uiScanIdx < uiCursor; uiScanIdx++)
                {
                    if (szBuf[uiScanIdx] == ' ')
                    {
                        uiTokenStart  = uiScanIdx + 1;
                        bIsFirstToken = ST_FALSE;
                    }
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
                        szPrefix, ptCtx->szCwd,
                        aaCompleteCands, LINE_COMPLETE_MAX);
                }

                if (iCompleteCount <= 0)
                {
                    /* No candidates: ignore TAB silently */
                    iCompleteCur = -1;
                    line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
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
                    line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
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

            line_redraw(ptCtx, szBuf, uiLen, uiCursor, szGhost);
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
                line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
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
                    line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
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
                line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }

        /* ---- Cursor movement -------------------------------- */
        if (iKey == CON_KEY_LEFT)
        {
            if (uiCursor > 0)
            {
                uiCursor--;
                line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }
        if (iKey == CON_KEY_RIGHT)
        {
            if (uiCursor < uiLen)
            {
                uiCursor++;
                line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }
        if (iKey == CON_KEY_HOME)
        {
            uiCursor = 0;
            line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
            continue;
        }
        if (iKey == CON_KEY_END)
        {
            uiCursor = uiLen;
            line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
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
                line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
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
                line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
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
                line_redraw(ptCtx, szBuf, uiLen, uiCursor, NULL);
            }
            continue;
        }

        /* All other keys: ignore silently */
    }
}

/* ------------------------------------------------------------------
 * Script (batch) mode
 * ------------------------------------------------------------------ */

static st_error_t line_run_script(line_context_t *ptCtx)
{
    FILE         *pFile;
    char          szInput[ST_MAX_CMD];
    parsed_cmd_t  tParsed;
    st_error_t    eResult;
    size_t        uiLen;

    LOG_TRACE("ptCtx=%p szScriptFile='%s'",
              (void *)ptCtx, ptCtx->szScriptFile);

    pFile = fopen(ptCtx->szScriptFile, "r");
    if (pFile == NULL)
    {
        LOG_ERROR("cannot open script '%s': %s",
                  ptCtx->szScriptFile, strerror(errno));
        return ST_ERROR;
    }

    LOG_INFO("running script '%s'", ptCtx->szScriptFile);

    while (ptCtx->bRunning == ST_TRUE
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

        /* Skip comment lines */
        if (szInput[0] == '#' || uiLen == 0)
        {
            continue;
        }

        eResult = line_parse_cmd(szInput, &tParsed);
        if (eResult == ST_NO_ERROR)
        {
            line_dispatch(&tParsed, ptCtx);
        }
    }

    fclose(pFile);
    LOG_INFO("script '%s' complete", ptCtx->szScriptFile);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API — lifecycle
 * ------------------------------------------------------------------ */

st_error_t line_init(line_context_t *ptCtx)
{
    char       *pCwd;
    st_error_t  eResult;

    LOG_TRACE("ptCtx=%p", (void *)ptCtx);

    if (ptCtx == NULL)
    {
        LOG_ERROR("ptCtx is NULL");
        return ST_ERROR;
    }

    memset(ptCtx, 0, sizeof(line_context_t));
    ptCtx->bRunning = ST_TRUE;

    /* P24: auto-detect ANSI capability based on stdout TTY status */
#ifdef ST_PLATFORM_WINDOWS
    g_line_bColors = _isatty(_fileno(stdout)) ? ST_TRUE : ST_FALSE;
#else
    g_line_bColors = isatty(STDOUT_FILENO) ? ST_TRUE : ST_FALSE;
#endif

    pCwd = getcwd(ptCtx->szCwd, ST_MAX_PATH);
    if (pCwd == NULL)
    {
        LOG_ERROR("getcwd() failed - using '.'");
        strncpy(ptCtx->szCwd, ".", ST_MAX_PATH - 1);
        ptCtx->szCwd[ST_MAX_PATH - 1] = '\0';
    }

    /* Create the mutex protecting szSelected */
    eResult = platform_mutex_create(&ptCtx->ptSelectedMutex);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("platform_mutex_create failed for ptSelectedMutex");
        return ST_ERROR;
    }

    /* Load command history from the default history file */
    line_history_load(NULL);

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

    /* -- Batch / script mode ----------------------------------- */
    if (ptCtx->szScriptFile[0] != '\0')
    {
        return line_run_script(ptCtx);
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
    line_update_console_title(ptCtx);

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
            /* Plain fallback (non-TTY: CI, piped input) */
            printf(ST_APP_NAME "> ");
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

        /* P8: refresh console title after each command (state may change) */
        line_update_console_title(ptCtx);
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

st_error_t line_shutdown(line_context_t *ptCtx)
{
    LOG_TRACE("ptCtx=%p", (void *)ptCtx);

    if (ptCtx == NULL)
    {
        LOG_ERROR("ptCtx is NULL");
        return ST_ERROR;
    }

    /* Close any open dir view */
    if (g_line_ptDirView != NULL)
    {
        dir_close(&g_line_ptDirView);
        g_line_ptDirView = NULL;
    }

    /* Close any open text edit view */
    if (g_line_ptEditTxtView != NULL)
    {
        edit_txt_close(&g_line_ptEditTxtView);
        g_line_ptEditTxtView = NULL;
    }

    /* Close any open hex edit view */
    if (g_line_ptEditHexView != NULL)
    {
        edit_hex_close(&g_line_ptEditHexView);
        g_line_ptEditHexView = NULL;
    }

    /* Save history */
    line_history_save(NULL);

    /* Destroy the selected-path mutex */
    if (ptCtx->ptSelectedMutex != NULL)
    {
        platform_mutex_destroy(&ptCtx->ptSelectedMutex);
        ptCtx->ptSelectedMutex = NULL;
    }

    memset(ptCtx, 0, sizeof(line_context_t));
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
