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

    /* -- [MAIN]23. Show the help message (only in release mode) -- */
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

    /* -- Parse command-line options -- */
    for (int iArg = 1; iArg < argc; iArg++)
    {
        /* -- [MAIN]10. Recognize -t/--trace and set bTraceAtStart -- */
        if ((strcmp(argv[iArg], "-t") == 0)
            || (strcmp(argv[iArg], "--trace") == 0))
        {
            g_main_ptCtx.bTraceAtStart = ST_TRUE;
        }
        
        /* --- Manage the input script option --- */
        else if (strcmp(argv[iArg], "--script") == 0)
        {
            /* -- [MAIN]11. Reject --script if it has no following
             *              argument -- */
            if (iArg + 1 >= argc)
            {
#ifndef ST_TEST_FWK
                fprintf(stderr, "%s: --script requires a file argument\n",
                                ST_APP_NAME);
                ST4Ever_print_usage(argv[0]);
#endif
                return ST_QUIT;
            }

            /* -- [MAIN]12. Reject --script if the next token looks
             *              like another option -- */
            iArg++;
            if (argv[iArg][0] == '-')
            {
#ifndef ST_TEST_FWK
                fprintf(stderr, "%s: --script requires a file argument\n",
                                ST_APP_NAME);
                ST4Ever_print_usage(argv[0]);
#endif
                return ST_QUIT;
            }

            /* -- [MAIN]13. Store the script file name -- */
            strncpy(g_main_ptCtx.szScriptFile, argv[iArg], ST_MAX_PATH - 1);
            g_main_ptCtx.szScriptFile[ST_MAX_PATH - 1] = '\0';
        }
        
        /* -- [MAIN]14. Recognize -h/--help, print usage and quit -- */
        else if (strcmp(argv[iArg], "-h")     == 0
              || strcmp(argv[iArg], "--help") == 0)
        {
            ST4Ever_print_usage(argv[0]);
            return ST_QUIT;
        }
        else
        
        /* -- [MAIN]15. Reject any unrecognized option -- */
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
    /* -- [MAIN]1. Analyze given arguemnts, no argument will succeed -- */
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
    g_main_ptCtx.ptTraceCtx = trace_init();
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

    /* -- [MAIN]9. If requested on command line, open the Trace GUI view -- */
    if (g_main_ptCtx.bTraceAtStart) trace_gui_open();

    /* -- [MAIN]5. Init Virtual ST machine -- */
    /* TODO: Implement ROM upload function - see if ROM is uploaded at st_init() 
     *      or if a dedicated st_rom_load() function is better after st_init() */
    g_main_ptCtx.ptSTMachineCtx = st_init(NULL);
    if (g_main_ptCtx.ptSTMachineCtx == ST_ERROR)
    {
        LOG_ERROR("st_init() failed");
        return ST_ERROR;
    }

    /* -- [MAIN]6. Init 'load' command -- */
    g_main_ptCtx.ptSTLoadCtx = load_init(g_main_ptCtx.ptSTMachineCtx);
    if (g_main_ptCtx.ptSTLoadCtx == ST_ERROR)
    {
        LOG_ERROR("load_init() failed");
        return ST_ERROR;
    }

    /* -- [MAIN]7. Init 'exec' command -- */
    g_main_ptCtx.ptSTExecCtx = exec_init(g_main_ptCtx.ptSTMachineCtx);
    if (g_main_ptCtx.ptSTExecCtx == ST_ERROR)
    {
        LOG_ERROR("exec_init() failed");
        return ST_ERROR;
    }

    /* -- [MAIN]8. Init Main Console -- */
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
    st_error_t eExitCode = ST_NO_ERROR;
    st_bool_t  bIsCorrectObject;

    LOG_TRACE("Starting the complete shutdown sequence");

    /* -- [MAIN]16. Shutdown Main Console -- */
    CHECK_OBJ(g_main_ptCtx.ptConsoleCtx, ST_LINE_CTX, bIsCorrectObject);
    if (bIsCorrectObject)
       {
            if (line_shutdown() == ST_ERROR)
            {
                LOG_ERROR("line_shutdown() failed");
                eExitCode = ST_ERROR;
            }
       }

    /* -- [MAIN]17. Shutdown 'exec' command -- */
    CHECK_OBJ(g_main_ptCtx.ptSTExecCtx, ST_EXEC_CTX, bIsCorrectObject);
    if (bIsCorrectObject)
       {
            if (exec_shutdown() == ST_ERROR)
            {
                LOG_ERROR("exec_shutdown() failed");
                eExitCode = ST_ERROR;
            }
       }

    /* -- [MAIN]18. Shutdown 'load' command -- */
    CHECK_OBJ(g_main_ptCtx.ptSTLoadCtx, ST_LOAD_CTX, bIsCorrectObject);
    if (bIsCorrectObject)
       {
            if (load_shutdown() == ST_ERROR)
            {
                LOG_ERROR("load_shutdown() failed");
                eExitCode = ST_ERROR;
            }
       }

    /* -- [MAIN]19. Shutdown Virtual ST machine -- */
    CHECK_OBJ(g_main_ptCtx.ptSTMachineCtx, ST_MACHINE_CTX, bIsCorrectObject);
    if (bIsCorrectObject)
       {
            if (st_shutdown(NULL) == ST_ERROR)
            {
                LOG_ERROR("st_shutdown() failed");
                eExitCode = ST_ERROR;
            }
       }

    /* -- [MAIN]20. Shutdown GUI Module -- */
    CHECK_OBJ(g_main_ptCtx.ptGUICtx, ST_GUI_CTX, bIsCorrectObject);
    if (bIsCorrectObject)
       {
            if (gui_shutdown() == ST_ERROR)
            {
                LOG_ERROR("gui_shutdown() failed");
                eExitCode = ST_ERROR;
            }
       }

    LOG_INFO("%s shutdown complete (exit code %d)",
             ST_APP_NAME, eExitCode);

    /* -- [MAIN]21. Shutdown Trace Module -- */
    CHECK_OBJ(g_main_ptCtx.ptTraceCtx, ST_TRACE_CTX, bIsCorrectObject);
    if (bIsCorrectObject)
       {
            if (trace_shutdown() == ST_ERROR)
            {
                LOG_ERROR("trace_shutdown() failed");
                eExitCode = ST_ERROR;
            }
       }

#ifdef ST_PLATFORM_WINDOWS
    /* -- [MAIN]22. Platform-specific console shutdown -- */
    CHECK_OBJ(g_main_ptCtx.ptWinConsoleCtx, ST_WIN_CONSOLE_CTX, bIsCorrectObject);
    if (bIsCorrectObject)
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