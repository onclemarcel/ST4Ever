/*
 * trace.c - ST4Ever trace and debug console implementation
 */

#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* ------------------------------------------------------------------
 * ANSI colour codes (work on MSYS2/mintty and Linux terminals)
 * ------------------------------------------------------------------ */

#define ANSI_RESET      "\033[0m"
#define ANSI_GRAY       "\033[90m"
#define ANSI_CYAN       "\033[96m"
#define ANSI_RED        "\033[91m"
#define ANSI_MAGENTA    "\033[95m"
#define ANSI_YELLOW     "\033[93m"

/* ------------------------------------------------------------------
 * Internal constants
 * ------------------------------------------------------------------ */

#define TRACE_LOGFILE           "st4ever_trace.log"
#define TRACE_COMPACT_FUNCLEN   80   /* max func name stored for compact */

/* ------------------------------------------------------------------
 * Module globals  (g_trace_ prefix, static = module-private)
 * ------------------------------------------------------------------ */

static st_bool_t g_trace_bInitialised  = ST_FALSE;
static st_bool_t g_trace_bOpen         = ST_FALSE;
static st_bool_t g_trace_bTraceEnabled = ST_TRUE;
static FILE     *g_trace_pFile         = NULL;

/* Compaction state for consecutive LOG_TRACE from the same function */
static char      g_trace_szLastFunc[TRACE_COMPACT_FUNCLEN] = {'\0'};
static int       g_trace_iCompactCount                     = 0;

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/*
 * trace_get_timestamp() - Write current wall-clock time into szBuf.
 *
 * Parameters:
 *   szBuf    [out] : Destination buffer (filled with "HH:MM:SS").
 *   uiBufLen [in]  : Byte capacity of szBuf.
 */
static void trace_get_timestamp(char *szBuf, size_t uiBufLen)
{
    time_t     tNow;
    struct tm *pTm;

    if (szBuf == NULL || uiBufLen == 0)
    {
        return;
    }

    tNow = time(NULL);
    pTm  = localtime(&tNow);

    if (pTm == NULL)
    {
        strncpy(szBuf, "??:??:??", uiBufLen - 1);
        szBuf[uiBufLen - 1] = '\0';
        return;
    }

    strftime(szBuf, uiBufLen, "%H:%M:%S", pTm);
}

/*
 * trace_level_label() - Short uppercase label for a log level.
 *
 * Parameters:
 *   eLevel [in] : Log level.
 *
 * Returns:
 *   Pointer to a static string.
 */
static const char *trace_level_label(log_level_t eLevel)
{
    switch (eLevel)
    {
        case LOG_LEVEL_TRACE: return "TRC ";
        case LOG_LEVEL_INFO:  return "INF ";
        case LOG_LEVEL_ERROR: return "ERR ";
        case LOG_LEVEL_TODO:  return "TODO";
        default:              return "??? ";
    }
}

/*
 * trace_level_ansi() - ANSI colour prefix for a log level.
 *
 * Parameters:
 *   eLevel [in] : Log level.
 *
 * Returns:
 *   Pointer to a static ANSI escape string.
 */
static const char *trace_level_ansi(log_level_t eLevel)
{
    switch (eLevel)
    {
        case LOG_LEVEL_TRACE: return ANSI_GRAY;
        case LOG_LEVEL_INFO:  return ANSI_CYAN;
        case LOG_LEVEL_ERROR: return ANSI_RED;
        case LOG_LEVEL_TODO:  return ANSI_MAGENTA;
        default:              return ANSI_RESET;
    }
}

/*
 * trace_flush_compact() - Emit a pending compaction summary line.
 *
 * If a previous LOG_TRACE was repeated N > 1 times in a row, this
 * emits one final line " funcname [xN]" before moving on.
 */
static void trace_flush_compact(void)
{
    char szTime[16];

    if (g_trace_iCompactCount <= 1)
    {
        g_trace_iCompactCount = 0;
        g_trace_szLastFunc[0] = '\0';
        return;
    }

    trace_get_timestamp(szTime, sizeof(szTime));

    if (g_trace_bOpen == ST_TRUE)
    {
        fprintf(stderr,
                "%s%s [%s] %s [x%d]%s\n",
                ANSI_GRAY,
                szTime,
                trace_level_label(LOG_LEVEL_TRACE),
                g_trace_szLastFunc,
                g_trace_iCompactCount,
                ANSI_RESET);
        fflush(stderr);
    }

    if (g_trace_pFile != NULL)
    {
        fprintf(g_trace_pFile,
                "%s [%s] %s [x%d]\n",
                szTime,
                trace_level_label(LOG_LEVEL_TRACE),
                g_trace_szLastFunc,
                g_trace_iCompactCount);
        fflush(g_trace_pFile);
    }

    g_trace_iCompactCount = 0;
    g_trace_szLastFunc[0] = '\0';
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_error_t trace_init(st_bool_t bOpen)
{
    if (g_trace_bInitialised == ST_TRUE)
    {
        /* Already up - not an error, but note it */
        fprintf(stderr,
                "[trace_init] WARNING: called more than once\n");
        return ST_NO_ERROR;
    }

    g_trace_pFile = fopen(TRACE_LOGFILE, "w");
    if (g_trace_pFile == NULL)
    {
        /* Non-fatal: continue with stderr-only output */
        fprintf(stderr,
                "[trace_init] WARNING: cannot open '%s' - "
                "file logging disabled\n",
                TRACE_LOGFILE);
    }

    g_trace_bInitialised  = ST_TRUE;
    g_trace_bTraceEnabled = ST_TRUE;
    g_trace_iCompactCount = 0;
    g_trace_szLastFunc[0] = '\0';

    if (bOpen == ST_TRUE)
    {
        return trace_open();
    }

    return ST_NO_ERROR;
}

st_error_t trace_open(void)
{
    if (g_trace_bInitialised == ST_FALSE)
    {
        fprintf(stderr,
                "[trace_open] ERROR: trace not initialised\n");
        return ST_ERROR;
    }

    g_trace_bOpen = ST_TRUE;

    /* UC1: output to stderr.  TODO(UC3): open SDL2 trace window */
    LOG_TODO("GUI trace window not yet implemented "
             "(UC3) - output to stderr");

    LOG_INFO("Trace console opened");
    return ST_NO_ERROR;
}

st_error_t trace_close(void)
{
    if (g_trace_bInitialised == ST_FALSE)
    {
        return ST_NO_ERROR; /* Nothing to close */
    }

    trace_flush_compact();

    LOG_INFO("Trace console closed");
    g_trace_bOpen = ST_FALSE;

    return ST_NO_ERROR;
}

st_error_t trace_set_trace_enabled(st_bool_t bEnabled)
{
    /* Flush compaction before changing state */
    trace_flush_compact();

    g_trace_bTraceEnabled = bEnabled;

    LOG_INFO("LOG_TRACE %s", bEnabled == ST_TRUE ? "enabled" : "disabled");
    return ST_NO_ERROR;
}

st_bool_t trace_is_trace_enabled(void)
{
    return g_trace_bTraceEnabled;
}

st_bool_t trace_is_open(void)
{
    return g_trace_bOpen;
}

void trace_log(log_level_t  eLevel,
               const char  *szFunc,
               int          iLine,
               const char  *szFmt,
               ...)
{
    char    szTime[16];
    char    szMsg[ST_MAX_MSG];
    va_list vaArgs;
    int     bCompacted;

    /* Guard: subsystem not ready */
    if (g_trace_bInitialised == ST_FALSE)
    {
        return;
    }

    /* Filter LOG_TRACE when disabled */
    if (eLevel == LOG_LEVEL_TRACE
    &&  g_trace_bTraceEnabled == ST_FALSE)
    {
        return;
    }

    /* Nothing to write to */
    if (g_trace_bOpen == ST_FALSE && g_trace_pFile == NULL)
    {
        return;
    }

    /* ---- Compaction logic for LOG_TRACE ---- */
    bCompacted = 0;

    if (eLevel == LOG_LEVEL_TRACE && szFunc != NULL)
    {
        if (g_trace_szLastFunc[0] != '\0'
        &&  strncmp(g_trace_szLastFunc,
                    szFunc,
                    TRACE_COMPACT_FUNCLEN - 1) == 0)
        {
            /* Same function as previous TRACE: just count it */
            g_trace_iCompactCount++;
            bCompacted = 1;
        }
        else
        {
            /* New function: flush any previous compaction first */
            trace_flush_compact();
            strncpy(g_trace_szLastFunc,
                    szFunc,
                    TRACE_COMPACT_FUNCLEN - 1);
            g_trace_szLastFunc[TRACE_COMPACT_FUNCLEN - 1] = '\0';
            g_trace_iCompactCount = 1;
        }
    }
    else
    {
        /* Non-TRACE entry: flush any pending compaction */
        trace_flush_compact();
    }

    if (bCompacted)
    {
        return; /* Will be flushed when function changes */
    }

    /* ---- Format the message ---- */
    va_start(vaArgs, szFmt);
    vsnprintf(szMsg, sizeof(szMsg), szFmt, vaArgs);
    va_end(vaArgs);

    trace_get_timestamp(szTime, sizeof(szTime));

    /* ---- Write to stderr (coloured) ---- */
    if (g_trace_bOpen == ST_TRUE)
    {
        fprintf(stderr,
                "%s%s [%s] %s:%d  %s%s\n",
                trace_level_ansi(eLevel),
                szTime,
                trace_level_label(eLevel),
                szFunc != NULL ? szFunc : "?",
                iLine,
                szMsg,
                ANSI_RESET);
        fflush(stderr);
    }

    /* ---- Write to log file (plain text) ---- */
    if (g_trace_pFile != NULL)
    {
        fprintf(g_trace_pFile,
                "%s [%s] %s:%d  %s\n",
                szTime,
                trace_level_label(eLevel),
                szFunc != NULL ? szFunc : "?",
                iLine,
                szMsg);
        fflush(g_trace_pFile);
    }
}

st_error_t trace_shutdown(void)
{
    st_error_t eResult;

    if (g_trace_bInitialised == ST_FALSE)
    {
        return ST_NO_ERROR;
    }

    LOG_INFO("%s %s shutting down - trace closing",
             ST_APP_NAME, ST_APP_VERSION);
    trace_flush_compact();

    eResult = ST_NO_ERROR;

    if (g_trace_pFile != NULL)
    {
        if (fclose(g_trace_pFile) != 0)
        {
            fprintf(stderr,
                    "[trace_shutdown] ERROR: fclose() failed\n");
            eResult = ST_ERROR;
        }
        g_trace_pFile = NULL;
    }

    g_trace_bInitialised  = ST_FALSE;
    g_trace_bOpen         = ST_FALSE;
    g_trace_bTraceEnabled = ST_TRUE;
    g_trace_iCompactCount = 0;
    g_trace_szLastFunc[0] = '\0';

    return eResult;
}