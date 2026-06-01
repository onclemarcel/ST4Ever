/*
 * main.c - ST4Ever application entry point
 *
 * Responsibilities:
 *   1. Parse command-line options (-t, -h, --script).
 *   2. Initialise platform-specific subsystems (win_console_init).
 *   3. Initialise the trace subsystem (trace_init).
 *   4. Initialise the GUI subsystem (gui_init).
 *   5. Initialise and run the interactive console (line_init/run).
 *   6. Perform orderly shutdown in reverse initialisation order.
 *
 * Command-line options:
 *   -t              Open the trace console at startup.
 *   -h / --help     Print usage text and exit.
 *   --script <file> Run commands from file (batch mode, UC4.3).
 */

#include "common.h"
#include "trace.h"
#include "line.h"
#include "gui.h"
#include "ST.h"
#include "load.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Platform-specific entry points (declared here; defined in win/ or
 * linux/ source files so no platform header is needed here).
 * ------------------------------------------------------------------ */

#ifdef ST_PLATFORM_WINDOWS
extern st_error_t win_console_init(void);
extern st_error_t win_console_shutdown(void);
#endif

/* ------------------------------------------------------------------
 * Internal: usage text
 * ------------------------------------------------------------------ */

/*
 * print_usage() - Print command-line usage to stdout.
 *
 * Parameters:
 *   szArgv0 [in] : argv[0] (program name).
 */
static void print_usage(const char *szArgv0)
{
    const char *szName;

    szName = (szArgv0 != NULL) ? szArgv0 : ST_APP_NAME;

    printf("Usage: %s [options]\n\n", szName);
    printf("Options:\n");
    printf("  -t              Open the trace console at startup\n");
    printf("  --script <file> Run commands from file (batch mode)\n");
    printf("  -h / --help     Show this message and exit\n");
    printf("\n");
    printf("Once running, type 'help' for the list of commands.\n");
}

/* ------------------------------------------------------------------
 * main()
 * ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    st_bool_t       bTraceAtStart;
    st_error_t      eResult;
    line_context_t  tLineCtx;
    st_machine_t    tMachine;
    int             iArg;
    int             iExitCode;
    char            szScriptFile[ST_MAX_PATH];

    bTraceAtStart  = ST_FALSE;
    iExitCode      = 0;
    szScriptFile[0] = '\0';
    memset(&tLineCtx, 0, sizeof(tLineCtx));

    /* ---- 1. Parse command-line options --------------------------- */
    for (iArg = 1; iArg < argc; iArg++)
    {
        if (strcmp(argv[iArg], "-t") == 0)
        {
            bTraceAtStart = ST_TRUE;
        }
        else if (strcmp(argv[iArg], "--script") == 0)
        {
            if (iArg + 1 >= argc)
            {
                fprintf(stderr,
                        "%s: --script requires a file argument\n",
                        ST_APP_NAME);
                print_usage(argv[0]);
                return 1;
            }
            iArg++;
            strncpy(szScriptFile, argv[iArg], ST_MAX_PATH - 1);
            szScriptFile[ST_MAX_PATH - 1] = '\0';
        }
        else if (strcmp(argv[iArg], "-h")     == 0
              || strcmp(argv[iArg], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr,
                    "%s: unknown option '%s'\n",
                    ST_APP_NAME, argv[iArg]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* ---- 2. Platform-specific console init ----------------------- */
#ifdef ST_PLATFORM_WINDOWS
    eResult = win_console_init();
    if (eResult != ST_NO_ERROR)
    {
        fprintf(stderr,
                "[FATAL] win_console_init() failed\n");
        return 1;
    }
#endif

    /* ---- 3. Trace subsystem -------------------------------------- */
    eResult = trace_init(bTraceAtStart);
    if (eResult != ST_NO_ERROR)
    {
        fprintf(stderr,
                "[FATAL] trace_init() failed\n");
        iExitCode = 1;
        goto shutdown_console;
    }

    LOG_INFO("%s %s starting (argc=%d, -t=%s)",
             ST_APP_NAME, ST_APP_VERSION,
             argc,
             bTraceAtStart == ST_TRUE ? "yes" : "no");

    /* ---- 4. GUI subsystem (stubbed until UC3) -------------------- */
    eResult = gui_init();
    if (eResult != ST_NO_ERROR)
    {
        /* Non-fatal for UC1: GUI is all stubs */
        LOG_ERROR("gui_init() failed - GUI views will not open");
    }

    /* ---- 5. ST machine + load module ----------------------------- */
    eResult = st_init(&tMachine, NULL);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("st_init() failed");
        iExitCode = 1;
        goto shutdown_gui;
    }

    eResult = load_init(&tMachine);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("load_init() failed");
        iExitCode = 1;
        goto shutdown_st;
    }

    /* ---- 6. Console init & run ----------------------------------- */
    eResult = line_init(&tLineCtx);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("line_init() failed");
        iExitCode = 1;
        goto shutdown_load;
    }

    /* Restore --script path (line_init() zeroes the context) */
    if (szScriptFile[0] != '\0')
    {
        strncpy(tLineCtx.szScriptFile, szScriptFile,
                ST_MAX_PATH - 1);
        tLineCtx.szScriptFile[ST_MAX_PATH - 1] = '\0';
    }

    eResult = line_run(&tLineCtx);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("line_run() returned ST_ERROR");
        iExitCode = 1;
    }

    eResult = line_shutdown(&tLineCtx);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("line_shutdown() failed");
    }

    /* ---- 7. Ordered shutdown ------------------------------------- */
shutdown_load:
    eResult = load_shutdown();
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("load_shutdown() failed");
    }

shutdown_st:
    eResult = st_shutdown(&tMachine);
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("st_shutdown() failed");
    }

shutdown_gui:
    eResult = gui_shutdown();
    if (eResult != ST_NO_ERROR)
    {
        LOG_ERROR("gui_shutdown() failed");
    }

    LOG_INFO("%s shutdown complete (exit code %d)",
             ST_APP_NAME, iExitCode);

shutdown_console:
    eResult = trace_shutdown();
    if (eResult != ST_NO_ERROR)
    {
        fprintf(stderr, "[FATAL] trace_shutdown() failed\n");
    }

#ifdef ST_PLATFORM_WINDOWS
    win_console_shutdown();
#endif

    return iExitCode;
}
