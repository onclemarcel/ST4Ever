/*
 * use_case_00.c - Test file of the main.c scope 
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * TEST MATRIX:
 *   [N] Nominal    : xx tests  - all public functions & statis functions
 *   [R] Robustness : xx tests  - NULL params, out-of-bounds addresses,
 *                                double init/close, alignment errors
 *   [S] Skipped    : xx tests  - Manual test to complete coverage
 *
 * Test groups:
 *   Group 1: Management of ST4Ever application options
 * 
 *   Group 2: Initialization functions chain & capture of the context
 *            structures
 *
 * Exit code: ST_NO_ERROR = all tests passed, ST_ERROR = one or more failures.
 */

#include "use_cases.h"

#ifdef ST_PLATFORM_WINDOWS
#include <windows.h>
#include "../win/win.h"
#endif

/* Required by the UC_TEST / UC_CHECK macros in use_cases.h */
int g_uc_fails = 0;

/* ------------------------------------------------------------------
 *   Group 1: Management of ST4Ever application options
 * ------------------------------------------------------------------ */
static char* g_uc00_szArgs_none[]            = {"Test.exe"};
static char* g_uc00_szArgs_trace_short[]     = {"Test.exe", "-t"};
static char* g_uc00_szArgs_trace_long[]      = {"Test.exe", "--trace"};
static char* g_uc00_szArgs_trace_last[]      = {"Test.exe", "--script", 
                                            "no_script.txt", "-t"};
static char* g_uc00_szArgs_err_short[]       = {"Test.exe", "-x", "--trace"};
static char* g_uc00_szArgs_err_long[]        = {"Test.exe", "--wrong"};
static char* g_uc00_szArgs_script_no_file[]  = {"Test.exe", "--script"};
static char* g_uc00_szArgs_script_option[]   = {"Test.exe", "--script",
                                                "--trace"};
static char* g_uc00_szArgs_script_last[]     = {"Test.exe", "--trace", 
                                                "--script", "n"};
static char* g_uc00_szArgs_help_short[]      = {"Test.exe", "-h"};
static char* g_uc00_szArgs_help_long[]       = {"Test.exe", "--trace", 
                                                "--help", "--script", "n"};

/*
 * uc00_manage_options() - Manage ST4Ever application options
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]1. Parse command-line options --
 * 
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc00_manage_options()
{
    ST4Ever_context_t* ptCtx;

    printf("\n--- Test group 1: Parse command-line options ---\n");
    
    /* -- [MAIN]1. Parse command-line options -- */
    /* INTENT[INT-APP-001 → TC-APP-001 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * call the ST4Ever main function with no argument : init is OK 
     * bTraceAtStart is FALSE */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(1, g_uc00_szArgs_none);
    UC_CHECK("[Chk] Launch ST4Ever with no argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceAtStart is set to TRUE", 
                (ptCtx->bTraceAtStart == ST_FALSE)); 
    }

    /* --- [MAIN]1.1 Manage the Trace Console option --- */
    /* INTENT[INT-APP-002 → TC-APP-002...004 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * -t|--trace option is captured and bTraceAtStart is set to TRUE */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_trace_short);
    UC_CHECK("[Chk] Launch ST4Ever with -t", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceAtStart is set to TRUE", 
                 (ptCtx->bTraceAtStart == ST_TRUE)); 
    }

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(4, g_uc00_szArgs_trace_last);
    UC_CHECK("[Chk] Launch ST4Ever with -t as last argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceAtStart is set to TRUE", 
                (ptCtx->bTraceAtStart == ST_TRUE)); 
    }

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_trace_long);
    UC_CHECK("[Chk] Launch ST4Ever with --trace", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceAtStart is set to TRUE", 
                 (ptCtx->bTraceAtStart == ST_TRUE)); 
    }

    /* --- [MAIN]1.2 Manage the input script option --- */
    /* INTENT[INT-APP-003 → TC-APP-005...008 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * --script option is captured with its file name stored in szScriptFile */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(4, g_uc00_szArgs_trace_last);
    UC_CHECK("[Chk] Launch ST4Ever with --script no_script.txt", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] szScriptFile is set - length is 13", 
                 (strlen(ptCtx->szScriptFile) == 13));
        UC_TEST("[N] szScriptFile is 'no_script.txt'", 
                 (strncmp(ptCtx->szScriptFile, "no_script.txt", 
                          strlen(ptCtx->szScriptFile)) == 0)); 
    }

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_script_no_file);
    UC_TEST("[N] Launch ST4Ever with --script but no file in argument", 
        ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(3, g_uc00_szArgs_script_option);
    UC_TEST("[N] Launch ST4Ever with --script directly followed by an option", 
        ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(4, g_uc00_szArgs_script_last);
    UC_CHECK("[Chk] Launch ST4Ever with --trace --script n", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] szScriptFile is set - length is 1", 
                 (strlen(ptCtx->szScriptFile) == 1));
        UC_TEST("[N] szScriptFile is 'n'", 
                 (strncmp(ptCtx->szScriptFile, "n", 
                          strlen(ptCtx->szScriptFile)) == 0));  
    }

    /* --- [MAIN]1.3 Manage help option --- */
    /* INTENT[INT-APP-004 → TC-APP-009...010 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * -h|--help is captured and application quits */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_help_short);
    UC_TEST("[R] Launch ST4Ever with -h", 
                ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(5, g_uc00_szArgs_help_long);
    UC_TEST("[R] Launch ST4Ever with --help in the middle", 
                ((st_u64_t)ptCtx == ST_QUIT));

    /* --- [MAIN]1.4 Manage unknown options --- */
    /* INTENT[INT-APP-005 → TC-APP-011...012 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * incorrect short/long options are captured, init sends ST_ERROR */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(3, g_uc00_szArgs_err_short);
    UC_TEST("[R] Launch ST4Ever with invalid -x argument", 
                ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_err_long);
    UC_TEST("[R] Launch ST4Ever with invalid --wrong argument", 
                ((st_u64_t)ptCtx == ST_QUIT));
}

/*
 * uc00_check_win_console() - Check win_console_init()
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]2. Platform-specific console init --
 * 
 *   win_console.c:
 *   -- [WIN_CONSOLE]1. Set stdout in Virtual Terminal Processing Mode --
 *   -- [WIN_CONSOLE]2. Set stderr in Virtual Terminal Processing Mode --
 *   -- [WIN_CONSOLE]3. Set Console Input/Output to Code Page UTF8 -- 
 *   -- [WIN_CONSOLE]4. Init returns context sructure --
 *
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc00_check_win_console()
{
    unsigned long    dwMode    = 0;
    unsigned long    dwBefore  = 0;
    win_console_context_t* ptCtx = 0;

    printf("\n--- Test group 2: Platform-specific console init ---\n");
    
    /* -- [WIN_CONSOLE]1. Set stdout in Virtual Terminal Processing Mode -- */
    /* -- [WIN_CONSOLE]2. Set stderr in Virtual Terminal Processing Mode -- */
    /* INTENT[INT-WIN-001 → TC-WIN-001...002 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that win_console_init() set stdout/stderr to ANSI terminal Mode */
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE)
    {
        GetConsoleMode(hOut, &dwMode);
        dwMode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
        dwBefore = dwMode;
        win_console_init();
        GetConsoleMode(hOut, &dwMode);
        
    }
    UC_TEST("[N] win_console_init() sets stdout to virtual terminal processing",
                 (dwMode - dwBefore) == ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    
    dwMode      = 0;
    dwBefore    = 0;

    void* hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE)
    {
        GetConsoleMode(hErr, &dwMode);
        dwMode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hErr, dwMode);
        dwBefore = dwMode;
        win_console_init();
        GetConsoleMode(hErr, &dwMode);
    }
     UC_TEST("[N] win_console_init() sets stderr to virtual terminal processing",
                 (dwMode - dwBefore) == ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    /* -- [WIN_CONSOLE]3. Set Console Input/Output to Code Page UTF8 -- */
    /* INTENT[INT-WIN-002 → TC-WIN-003...004 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that win_console_init() sets Console Code page to UTF-8 */
    dwMode      = 0;
    dwBefore    = 0;

    if (SetConsoleCP(CP_UTF7))
    {
        dwBefore = GetConsoleCP();      // UINT 65000
        win_console_init();
        dwMode   = GetConsoleCP();      // UINT 65001
    }
    UC_TEST("[N] win_console_init() sets Console Input Code Page to UTF-8",
                 (dwMode - dwBefore) == 1);
    
    dwMode      = 0;
    dwBefore    = 0;

    if (SetConsoleOutputCP(CP_UTF7))
    {
        dwBefore = GetConsoleOutputCP();      // UINT 65000
        ptCtx = (win_console_context_t*)win_console_init();
        dwMode   = GetConsoleOutputCP();      // UINT 65001
    }
    UC_TEST("[N] win_console_init() sets Console Output Code Page to UTF-8",
                 (dwMode - dwBefore) == 1);
    
    /* -- [MAIN]2. Platform-specific console init -- */
    /* -- [WIN_CONSOLE]4. Init returns context sructure -- */
    /* INTENT[INT-WIN-003 → TC-WIN-005 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that win_console_init() returns correct context structure */
    UC_CHECK("[Chk] win_console_init() returns the context structure",
                (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_WIN_CONSOLE_CTX);
    
    if (!g_uc_fails) 
    {
        UC_TEST("[N] Type of stdin is unknown", 
                 (ptCtx->eStdinType == WIN_STDIN_UNKNOWN));
    }

}

/*
 * uc00_trace_subsystem() - Initialize trace module
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]3. Init Trace Module --
 * 
 *   trace.c:
 *   -- [TRACE]1. Log Information if already initialised --
 *   -- [TRACE]2. If not initialized, open TRACE_LOG file for writing --
 *   -- [TRACE]3. Init trace context structure --
 *   -- [TRACE]4. If input parameter is TRUE, open the GUI console --
 *   -- [TRACE]5. Init returns context sructure --
 * 
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc00_trace_subsystem()
{
    trace_context_t* ptCtx;

    printf("\n--- Test group 3: Init Trace Module ---\n");
    
    /* -- [MAIN]3. Init Trace Module -- */
    /* -- [TRACE]3. Init trace context structure -- */
    /* -- [TRACE]5. Init returns context sructure -- */
    /* INTENT[INT-TRC-001 → TC-TRC-001...002 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that first call to trace_init() returns the context structure */
    ptCtx = (trace_context_t*)trace_init(ST_FALSE);
    UC_CHECK("[Chk] Launch trace_init()", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_TRACE_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bInitialised is set to true", 
                (ptCtx->bInitialised == ST_TRUE));
        UC_TEST("[N] bTraceEnabled is set to true", 
                (ptCtx->bTraceEnabled == ST_TRUE));
        
        // Forcing dummy value for next test
        ptCtx->bTraceEnabled = ST_FALSE;
    }

    /* -- [TRACE]1. Log Information if already initialised -- */
    /* INTENT[INT-TRC-002 → TC-TRC-003 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that second call still returns the structure (no error) */
    ptCtx = (trace_context_t*)trace_init(ST_TRUE);
    UC_CHECK("[Chk] Forcing bTraceEnabled to FALSE & re-launch trace_init()"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_TRACE_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceEnabled is set to false", 
                (ptCtx->bTraceEnabled == ST_FALSE));

        // Reset bInitialized 
        ptCtx->bInitialised = ST_FALSE;
    }    
    
    /* -- [TRACE]2. If not initialized, open TRACE_LOG file for writing -- */
    /* -- [TRACE]4. If input parameter is TRUE, open the GUI console -- */
    /* INTENT[INT-TRC-003 → TC-TRC-004...007 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Re-init trace subsystem and call GUI (while gui is not initialized)
     * This should returns the context structure with ptView set to NULL but
     * with pFile set to not NULL (open file succeeded) */
    ptCtx = (trace_context_t*)trace_init(ST_TRUE);
    UC_CHECK("[Chk] Forcing bInitialized to FALSE and re-launch trace_init(TRUE)"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_TRACE_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bInitialised is set to true", 
                (ptCtx->bInitialised == ST_TRUE));
        UC_TEST("[N] bTraceEnabled is set to true", 
                (ptCtx->bTraceEnabled == ST_TRUE));
        UC_TEST("[N] open file handler is not null", 
                (ptCtx->pFile != NULL));
        UC_TEST("[N] GUI is not initialized", 
                (ptCtx->ptView == NULL));
    }    
    
}

/*
 * uc00_gui_module() - Initialize GUI module
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]4. Init GUI Module --
 * 
 *   trace.c:
 *   -- [GUI]1. Log Information if already initialised --
 *   -- [GUI]2. Initialize windows list --
 *   -- [GUI]3. Create mutex list --
 *   -- [GUI]4. Platform-specific GUI init --
 *   -- [GUI]5. Init returns context sructure --
 * 
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc00_gui_module()
{
    gui_context_t* ptCtx;

    printf("\n--- Test group 4: Init GUI Module ---\n");
    
    /* -- [MAIN]4. Init GUI Module -- */
    /* -- [GUI]2. Initialize windows list -- */
    /* -- [GUI]5. Init returns context sructure -- */
    /* INTENT[INT-GUI-001 → TC-GUI-001...002 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that first call to gui_init() returns the context structure */
    ptCtx = (gui_context_t*)gui_init();
    UC_CHECK("[Chk] Launch gui_init()", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_GUI_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bInit is set to true", 
                (ptCtx->bInit == ST_TRUE));
        UC_TEST("[N] No windows is open", 
                (ptCtx->uiWndCount == 0));
        
        // Forcing dummy value for next test
        ptCtx->uiWndCount = 78;
    }

    /* -- [GUI]1. Log Information if already initialised -- */
    /* INTENT[INT-GUI-002 → TC-GUI-003 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that second call still returns the structure (no error) */
    ptCtx = (gui_context_t*)gui_init();
    UC_CHECK("[Chk] Forcing windows count to 78 & re-launch gui_init()"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_GUI_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] Window counter is kept to dummy value", 
                (ptCtx->uiWndCount == 78));

        // Reset bInitialized 
        ptCtx->bInit = ST_FALSE;
        ptCtx->uiWndCount = 78;
    }    
    
    /* -- [GUI]3. Create mutex list -- */
    /* -- [GUI]4. Platform-specific GUI init -- */
    /* INTENT[INT-GUI-003 → TC-GUI-004...007 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * After bInit set FALSE, check that gui_init() fills correctly the context
     * structure - with platform-related functions initialized */
    ptCtx = (gui_context_t*)gui_init();
    UC_CHECK("[Chk] Forcing bInit to FALSE & re-launch gui_init()"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_GUI_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bInit is set to true", 
                (ptCtx->bInit == ST_TRUE));
        UC_TEST("[N] No windows is open", 
                (ptCtx->uiWndCount == 0));
        UC_TEST("[N] Mutex is created", 
                (ptCtx->ptMutex != NULL));
        UC_TEST("[N] Platform-related init is OK", 
                (ptCtx->eGUIPtfInit == ST_NO_ERROR));     
    }
    
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("================================================================\n");
    printf(" ST4Ever UC0 - ST4Ever Main Application Tests\n");
    printf("================================================================\n");

    /* Init the log function */
    trace_init(ST_TRUE);

    /* Test the main init function */
    uc00_manage_options();
#ifdef ST_PLATFORM_WINDOWS
    uc00_check_win_console();
#endif
    uc00_trace_subsystem();
    uc00_gui_module();

    
    /* Close the log function */
    printf("\n--- Shutdown ---\n");
    trace_shutdown();
    printf("  [INFO] trace_shutdown() complete - "
           "see st4ever_trace.log for full trace\n");

    printf("\n================================================================\n");
    if (g_uc_fails == 0)
    {
        printf(" UC0 RESULT: ALL TESTS PASSED\n");
    }
    else
    {
        printf(" UC0 RESULT: %d TEST(S) FAILED\n", g_uc_fails);
    }
    printf("================================================================\n\n");

    return (g_uc_fails == 0) ? 0 : 1;
}
