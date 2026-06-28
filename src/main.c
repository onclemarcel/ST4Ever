/*
 * main.c - ST4Ever application entry point
 *
 * Responsibilities:
 *   1. Parse command-line options (-t, -h, --script).
 *   2. Initialise platform-specific subsystems (win_console_init).
 *   3. Initialise the trace subsystem (trace_init).
 *   4. Initialise the GUI subsystem (gui_init).
 *   5. Initialise the ST Machine (st_init, load_init, exec_init)
 *   6. Initialise and run the interactive console (line_init/run).
 *   7. Perform orderly shutdown in reverse initialisation order.
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
#include "exec.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ST4Ever_context_t g_main_ptCtx = {.ulMagic = 0xCAFEDECA, 
                                         .eObject = ST_MAIN_CTX};

/* ------------------------------------------------------------------
 * Platform-specific entry points (declared here; defined in win/ or
 * linux/ source files so no platform header is needed here).
 * ------------------------------------------------------------------ */

#ifdef ST_PLATFORM_WINDOWS
#include "../win/win.h"
#endif

/* ------------------------------------------------------------------
 * Internal static functions
 * ------------------------------------------------------------------ */

/*
 * ST4Ever_print_usage() - Print command-line usage to stdout.
 *
 * Parameters:
 *   szArgv0 [in] : argv[0] (program name)
 * 
 * Returns:
 *   Void
 * 
 * Global Variables:
 *   None
 * 
 */
static void ST4Ever_print_usage(const char *szArgv0)
{
    LOG_TRACE("szArgv0=%p", szArgv0);

    /* -- 1.3 Show the help message (only in release mode) -- */
    const char *szName;

    szName = (szArgv0 != NULL) ? szArgv0 : ST_APP_NAME;

#ifndef ST_TEST_FWK
    printf("Usage: %s [options]\n\n", szName);
    printf("Options:\n");
    printf("  -t              Open the trace console at startup\n");
    printf("  --script <file> Run commands from file (batch mode)\n");
    printf("  -h / --help     Show this message and exit\n");
    printf("\n");
    printf("Once running, type 'help' for the list of commands.\n");
#endif
}

/*
 * ST4Ever_manage_options() - Manage command-line options.
 *
 * Parameters:
 *   argc [in] : number of commang-line arguments
 *   argv [in] : the array of command line arguments
 * 
 * Returns:
 *   ST_ERROR if an argument is unknown
 *   ST_NO_ERROR if arguments are recognized
 * 
 * Global Variables:
 *   g_main_ptCtx.bTraceAtStart [W] : flag used to start Trace Console
 *   g_main_ptCtx.szScriptFile  [W] : script name for headless execution
 * 
 */
static st_error_t ST4Ever_manage_options(int argc, char* argv[])
{
    LOG_TRACE("argc=%d - argv=%p", argc, argv);

    /* -- 1. Parse command-line options -- */
    for (int iArg = 1; iArg < argc; iArg++)
    {
        /* --- 1.1 Manage the Trace Console option --- */
        if ((strcmp(argv[iArg], "-t") == 0)
            || (strcmp(argv[iArg], "--trace") == 0))
        {
            g_main_ptCtx.bTraceAtStart = ST_TRUE;
        }
        /* --- 1.2 Manage the input script option --- */
        else if (strcmp(argv[iArg], "--script") == 0)
        {
            /* ---- 1.2.1 --script option needs with a file name ---- */
            if (iArg + 1 >= argc)
            {
                fprintf(stderr, "%s: --script requires a file argument\n",
                                ST_APP_NAME);
                ST4Ever_print_usage(argv[0]);
                return ST_QUIT;
            }

            /* ---- 1.2.2 Check that next argument is not an option ---- */
            iArg++;
            if (argv[iArg][0] == '-')
            {
                fprintf(stderr, "%s: --script requires a file argument\n",
                                ST_APP_NAME);
                ST4Ever_print_usage(argv[0]);
                return ST_QUIT;
            }

            /* ---- 1.2.3 Copy the argument into the script string ---- */
            strncpy(g_main_ptCtx.szScriptFile, argv[iArg], ST_MAX_PATH - 1);
            g_main_ptCtx.szScriptFile[ST_MAX_PATH - 1] = '\0';
        }
        /* --- 1.3 Manage the help option --- */
        else if (strcmp(argv[iArg], "-h")     == 0
              || strcmp(argv[iArg], "--help") == 0)
        {
            ST4Ever_print_usage(argv[0]);
            return ST_QUIT;
        }
        else
        /* --- 1.4 Manage unknown options --- */
        {
            LOG_ERROR("%s: unknown option '%s'\n", ST_APP_NAME, argv[iArg]);
            ST4Ever_print_usage(argv[0]);
            return ST_QUIT;
        }
    }

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API - main application function
 * ------------------------------------------------------------------ */

st_u64_t ST4Ever_init(int argc, char *argv[])
{
    st_machine_t    tMachine;

    /* -- [MAIN]1. Parse command-line options -- */
    if (ST4Ever_manage_options(argc, argv) == ST_QUIT) return ST_QUIT;

    /* -- [MAIN]2. Platform-specific console init -- */
#ifdef ST_PLATFORM_WINDOWS
    g_main_ptCtx.ptWinConsoleCtx = win_console_init();
    if (g_main_ptCtx.ptWinConsoleCtx == ST_ERROR)
    {
        LOG_ERROR("[FATAL] win_console_init() failed");
        return ST_ERROR;
    }
#endif

    /* -- [MAIN]3. Init Trace Module -- */
    g_main_ptCtx.ptTraceCtx = trace_init(g_main_ptCtx.bTraceAtStart);
    if (g_main_ptCtx.ptTraceCtx == ST_ERROR)
    {
        LOG_ERROR("[FATAL] trace_init() failed");
        return ST_ERROR;
    }

    /* First information message in trace file & trace window, if open */
    LOG_INFO("%s %s starting (argc=%d, -t=%s, --script=%s)",
             ST_APP_NAME, ST_APP_VERSION,
             argc,
             g_main_ptCtx.bTraceAtStart == ST_TRUE ? "yes" : "no",
             g_main_ptCtx.szScriptFile[0] == "\0"  ? "none":
             g_main_ptCtx.szScriptFile);

    /* -- [MAIN]4. Init GUI Module -- */
    g_main_ptCtx.ptGUICtx = gui_init();
    if (g_main_ptCtx.ptGUICtx == ST_ERROR)
    {
        LOG_ERROR("[FATAL] gui_init() failed");
        return ST_ERROR;
    }

    /* ---- 5. ST machine + load module ----------------------------- */
    g_main_ptCtx.ptSTMachineCtx = st_init(&tMachine, NULL);
    if (g_main_ptCtx.ptSTMachineCtx == ST_ERROR)
    {
        LOG_ERROR("st_init() failed");
        return ST_ERROR;
    }

    g_main_ptCtx.ptSTLoadCtx = load_init(&tMachine);
    if (g_main_ptCtx.ptSTLoadCtx == ST_ERROR)
    {
        LOG_ERROR("load_init() failed");
        return ST_ERROR;
    }

    g_main_ptCtx.ptSTExecCtx = exec_init(&tMachine);
    if (g_main_ptCtx.ptSTExecCtx == ST_ERROR)
    {
        LOG_ERROR("exec_init() failed");
        return ST_ERROR;
    }

    /* ---- 6. Console init & run ----------------------------------- */
    g_main_ptCtx.ptConsoleCtx = line_init(g_main_ptCtx.szScriptFile);
    if (g_main_ptCtx.ptConsoleCtx == ST_ERROR)
    {
        LOG_ERROR("line_init() failed");
        return ST_ERROR;
    }

    return (st_u64_t)&g_main_ptCtx;
}

void ST4Ever_run()
{
    if (line_run() == ST_ERROR) LOG_ERROR("line_run() returned ST_ERROR");
}

st_error_t ST4Ever_shutdown()
{
    st_error_t eExitCode;

    if ((g_main_ptCtx.ptConsoleCtx != ST_ERROR) &&
       ( g_main_ptCtx.ptConsoleCtx != ST_NO_ERROR))
       {
            if (line_shutdown() == ST_ERROR)
            {
                LOG_ERROR("line_shutdown() failed");
                eExitCode = ST_ERROR;
            } 
       }

    if ((g_main_ptCtx.ptSTExecCtx != ST_ERROR) &&
       ( g_main_ptCtx.ptSTExecCtx != ST_NO_ERROR))
       {
            if (exec_shutdown() == ST_ERROR)
            {
                LOG_ERROR("exec_shutdown() failed");
                eExitCode = ST_ERROR;
            } 
       }

    if ((g_main_ptCtx.ptSTLoadCtx != ST_ERROR) &&
       ( g_main_ptCtx.ptSTLoadCtx != ST_NO_ERROR))
       {
            if (load_shutdown() == ST_ERROR)
            {
                LOG_ERROR("load_shutdown() failed");
                eExitCode = ST_ERROR;
            } 
       }

    if ((g_main_ptCtx.ptSTMachineCtx != ST_ERROR) &&
       ( g_main_ptCtx.ptSTMachineCtx != ST_NO_ERROR))
       {
            if (st_shutdown(NULL) == ST_ERROR)
            {
                LOG_ERROR("st_shutdown() failed");
                eExitCode = ST_ERROR;
            } 
       }

    if ((g_main_ptCtx.ptGUICtx != ST_ERROR) &&
       ( g_main_ptCtx.ptGUICtx != ST_NO_ERROR))
       {
            if (gui_shutdown() == ST_ERROR)
            {
                LOG_ERROR("gui_shutdown() failed");
                eExitCode = ST_ERROR;
            } 
       }

    LOG_INFO("%s shutdown complete (exit code %d)",
             ST_APP_NAME, eExitCode);

    if ((g_main_ptCtx.ptTraceCtx != ST_ERROR) &&
       ( g_main_ptCtx.ptTraceCtx != ST_NO_ERROR))
       {
            if (trace_shutdown() == ST_ERROR)
            {
                LOG_ERROR("trace_shutdown() failed");
                eExitCode = ST_ERROR;
            } 
       }

#ifdef ST_PLATFORM_WINDOWS
        if ((g_main_ptCtx.ptWinConsoleCtx != ST_ERROR) &&
       ( g_main_ptCtx.ptWinConsoleCtx != ST_NO_ERROR))
       {
            if (win_console_shutdown() == ST_ERROR)
            {
                LOG_ERROR("win_console_shutdown() failed");
                eExitCode = ST_ERROR;
            } 
       }
#endif

    return eExitCode;
}

/* ------------------------------------------------------------------
 * main() - Application main entry point, unless a test framework
 *          takes the hand on the main function by defining ST_TEST_FWK
 * ------------------------------------------------------------------ */
#ifndef ST_TEST_FWK
int main(int argc, char* argv[])
{
    /* Init application */
    st_u64_t result = ST4Ever_init(argc, argv);
    if ((result == ST_ERROR) || (result == ST_QUIT)) return ST4Ever_shutdown();

    /* Run the command line */
    ST4Ever_run();

    /* Shutdown properly */
    return ST4Ever_shutdown();
}
#endif