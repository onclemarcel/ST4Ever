/*
 * use_case_00.c - Test file of the main.c scope 
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * Test Strategy explained in R23/R24/R25: The objective of this use case is
 * to test the access to the ST4Ever modules global context strutures for
 * test purpose. Context structures are provided as return from xxx_init()
 * functions or equivalent initialization function. Those context strucures
 * are escalated into the main structure returned by ST4Ever_init()
 *  
 * TEST MATRIX:
 *   [N] Nominal    : 62 tests  - all public functions & static functions
 *   [R] Robustness : 7  tests  - NULL params, out-of-bounds addresses,
 *                                double init/close, alignment errors
 *   [S] Skipped    : xx tests  - Manual test to complete coverage
 *
 * Test groups:
 *   Group 1: Management of ST4Ever application options
 *
 *   Group 2-8: Initialization functions chain & capture of the context
 *            structures
 *
 *   Group 9: Shutdown sequence - exercises every *_shutdown() in the
 *            exact order used by ST4Ever_shutdown() and checks that
 *            each context remains a recognized object (magic + eObject
 *            intact) afterward - st_shutdown() is the reference pattern
 *            the other *_shutdown() functions were homogenized with.
 *
 * Exit code: ST_NO_ERROR = all tests passed, ST_ERROR = one or more failures.
 */

#include "use_cases.h"

#ifdef ST_PLATFORM_WINDOWS
#include <windows.h>
#include <io.h>      /* _isatty, _fileno */
#include "../win/win.h"
#endif

/* Required by the UC_TEST / UC_CHECK macros in use_cases.h */
int         g_uc_fails = 0;
st_bool_t   gIsObject  = ST_FALSE;

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
 *   -- [MAIN]1. Analyze given arguemnts, no argument will succeed --
 *   -- [MAIN]10. Recognize -t/--trace and set bTraceAtStart --
 *   -- [MAIN]11. Reject --script if it has no following
 *              argument --
 *   -- [MAIN]12. Reject --script if the next token looks
 *              like another option --
 *   -- [MAIN]13. Store the script file name --   
 *   -- [MAIN]14. Recognize -h/--help, print usage and quit --
 *   -- [MAIN]15. Reject any unrecognized option --
 *   -- [MAIN]23. Show the help message (only in release mode) --               
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
    
    /* -- [MAIN]1. Analyze given arguemnts, no argument will succeed -- */
    /* INTENT[INT-APP-001 → TC-APP-001 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * call the ST4Ever with no argument starts application with default values */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(1, g_uc00_szArgs_none);
    UC_CHECK("(INT-APP-001) [Chk] Launch ST4Ever with no argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-APP-001) bTraceAtStart is set to FALSE", 
                (ptCtx->bTraceAtStart == ST_FALSE)); 
    } else printf("  [SKIP] (TC-APP-001) ST_MAIN_CTX Object Check failed\n\n");

    /* -- [MAIN]10. Recognize -t/--trace and set bTraceAtStart -- */
    /* INTENT[INT-APP-002 → TC-APP-002...004 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * -t|--trace option is captured and bTraceAtStart is set to TRUE */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_trace_short);
    UC_CHECK("(INT-APP-002) [Chk] Launch ST4Ever with -t", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-APP-002) bTraceAtStart is set to TRUE", 
                 (ptCtx->bTraceAtStart == ST_TRUE));
    } else printf("  [SKIP] (TC-APP-002) ST_MAIN_CTX Object Check failed\n\n");

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(4, g_uc00_szArgs_trace_last);
    UC_CHECK("(INT-APP-002) [Chk] Launch ST4Ever with -t as last argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-APP-003) bTraceAtStart is set to TRUE", 
                (ptCtx->bTraceAtStart == ST_TRUE)); 
    } else printf("  [SKIP] (TC-APP-003) ST_MAIN_CTX Object Check failed\n\n");

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_trace_long);
    UC_CHECK("(INT-APP-002) [Chk] Launch ST4Ever with --trace", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-APP-004) bTraceAtStart is set to TRUE", 
                 (ptCtx->bTraceAtStart == ST_TRUE)); 
    } else printf("  [SKIP] (TC-APP-004) ST_MAIN_CTX Object Check failed\n\n");
	trace_gui_close();		// Close any open GUI

    /* -- [MAIN]11. Reject --script if it has no following
     *              argument -- */
    /* -- [MAIN]12. Reject --script if the next token looks
     *              like another option -- */        
    /* -- [MAIN]13. Store the script file name -- */
    /* INTENT[INT-APP-003 → TC-APP-005...010 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * --script option is captured with its file name stored in szScriptFile */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(4, g_uc00_szArgs_trace_last);
    UC_CHECK("(INT-APP-003) [Chk] Launch ST4Ever with --script no_script.txt", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-APP-005) szScriptFile is set - length is 13", 
                 (strlen(ptCtx->szScriptFile) == 13));
        UC_TEST("[N] (TC-APP-006) szScriptFile is 'no_script.txt'", 
                 (strncmp(ptCtx->szScriptFile, "no_script.txt", 
                          strlen(ptCtx->szScriptFile)) == 0)); 
    } else printf("  [SKIP] (TC-APP-005...006) ST_MAIN_CTX Object Check failed\n\n");

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_script_no_file);
    UC_TEST("[N] (TC-APP-007) Launch ST4Ever with --script but no file in argument", 
        ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(3, g_uc00_szArgs_script_option);
    UC_TEST("[N] (TC-APP-008) Launch ST4Ever with --script directly followed by an option", 
        ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(4, g_uc00_szArgs_script_last);
    UC_CHECK("(INT-APP-003) [Chk] Launch ST4Ever with --trace --script n", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-APP-009) szScriptFile is set - length is 1", 
                 (strlen(ptCtx->szScriptFile) == 1));
        UC_TEST("[N] (TC-APP-010) szScriptFile is 'n'", 
                 (strncmp(ptCtx->szScriptFile, "n", 
                          strlen(ptCtx->szScriptFile)) == 0));  
    } else printf("  [SKIP] (TC-APP-009...010) ST_MAIN_CTX Object Check failed\n\n");

    /* -- [MAIN]14. Recognize -h/--help, print usage and quit -- */
    /* -- [MAIN]23. Show the help message (only in release mode) -- */
    /* INTENT[INT-APP-004 → TC-APP-011...012 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * -h|--help is captured and application quits */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_help_short);
    UC_CHECK("(INT-APP-004) [Chk] Launch ST4Ever with invalid argument", (st_u64_t)ptCtx);
    UC_TEST("[R] (TC-APP-011) Launch ST4Ever with -h", 
                ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(5, g_uc00_szArgs_help_long);
    UC_TEST("[R] (TC-APP-012) Launch ST4Ever with --help in the middle", 
                ((st_u64_t)ptCtx == ST_QUIT));

    /* -- [MAIN]15. Reject any unrecognized option -- */
    /* INTENT[INT-APP-005 → TC-APP-013...014 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * incorrect short/long options returns ST_ERROR */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(3, g_uc00_szArgs_err_short);
    UC_CHECK("(INT-APP-005) [Chk] Launch ST4Ever with invalid argument", (st_u64_t)ptCtx);
    UC_TEST("[R] (TC-APP-013) Launch ST4Ever with invalid -x argument", 
                ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_err_long);
    UC_TEST("[R] (TC-APP-014) Launch ST4Ever with invalid --wrong argument", 
                ((st_u64_t)ptCtx == ST_QUIT));
				
	trace_gui_close(); // Close any open GUI
}

/* ------------------------------------------------------------------
 *   Group 2-8: Initialization functions chain & capture of the context
 *            structures
 * ------------------------------------------------------------------ */

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
        ptCtx = (win_console_context_t*)win_console_init();
        GetConsoleMode(hOut, &dwMode);
        
    }
    UC_CHECK("(INT-WIN-001) [Chk] win_console_init() has initialized Virtual Terminal",
                (st_u64_t)ptCtx);
    /* TC-WIN-001/002: GetConsoleMode() fails on pipe handles (mintty/MSYS2).
     * Skip the mode-delta assertion when stdout is not a real console handle. */
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_CHAR)
    {
        UC_TEST("[N] (TC-WIN-001) win_console_init() sets stdout to virtual terminal processing",
                     (dwMode - dwBefore) == ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    else
    {
        TEST_SKIP("[N] (TC-WIN-001) win_console_init() stdout VTP (pipe handle - mintty env)");
    }

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
    if (GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_CHAR)
    {
        UC_TEST("[N] (TC-WIN-002) win_console_init() sets stderr to virtual terminal processing",
                     (dwMode - dwBefore) == ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    else
    {
        TEST_SKIP("[N] (TC-WIN-002) win_console_init() stderr VTP (pipe handle - mintty env)");
    }

    /* -- [WIN_CONSOLE]3. Set Console Input/Output to Code Page UTF8 -- */
    /* INTENT[INT-WIN-002 → TC-WIN-003...004 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that win_console_init() sets Console Code page to UTF-8 */
    dwMode      = 0;
    dwBefore    = 0;

    if (SetConsoleCP(CP_UTF7))
    {
        dwBefore = GetConsoleCP();      // UINT 65000
        ptCtx = (win_console_context_t*)win_console_init();
        dwMode   = GetConsoleCP();      // UINT 65001
    }
    UC_CHECK("(INT-WIN-002) [Chk] win_console_init() has initialized Code Page",
                (st_u64_t)ptCtx);
    UC_TEST("[N] (TC-WIN-003) win_console_init() sets Console Input Code Page to UTF-8",
                 (dwMode - dwBefore) == 1);
    
    dwMode      = 0;
    dwBefore    = 0;

    if (SetConsoleOutputCP(CP_UTF7))
    {
        dwBefore = GetConsoleOutputCP();      // UINT 65000
        ptCtx = (win_console_context_t*)win_console_init();
        dwMode   = GetConsoleOutputCP();      // UINT 65001
    }
    UC_TEST("[N] (TC-WIN-004) win_console_init() sets Console Output Code Page to UTF-8",
                 (dwMode - dwBefore) == 1);
    
    /* -- [MAIN]2. Platform-specific console init -- */
    /* -- [WIN_CONSOLE]4. Init returns context sructure -- */
    /* INTENT[INT-WIN-003 → TC-WIN-005 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that win_console_init() returns correct context structure */
    UC_CHECK("(INT-WIN-003) [Chk] win_console_init() returns the context structure",
                (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_WIN_CONSOLE_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-WIN-005) Type of stdin is unknown", 
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
 *   -- [TRACE]4. Init returns context sructure --
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
    /* -- [TRACE]2. If not initialized, open TRACE_LOG file for writing -- */
    /* -- [TRACE]3. Init trace context structure -- */
    /* -- [TRACE]4. Init returns context sructure -- */
    /* INTENT[INT-TRC-001 → TC-TRC-001...004 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that first call to trace_init() returns the context structure */
    ptCtx = (trace_context_t*)trace_init(ST_FALSE);
    UC_CHECK("(INT-TRC-001) [Chk] Launch trace_init()", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_TRACE_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-TRC-001) bInitialised is set to true", 
                (ptCtx->bInitialised == ST_TRUE));
        UC_TEST("[N] (TC-TRC-002) Trace level in GUI by default is INFO", 
                (ptCtx->eViewMinLevel == LOG_LEVEL_INFO));
        UC_TEST("[N] (TC-TRC-003) open file handler is not null", 
                (ptCtx->pFile != NULL));
        UC_TEST("[N] (TC-TRC-004) GUI is not initialized", 
                (ptCtx->ptView == NULL));
        // Forcing dummy value for next test
        ptCtx->eViewMinLevel = LOG_LEVEL_TODO;
    } else printf("  [SKIP] (TC-TRC-001...004) ST_TRACE_CTX Object Check failed\n\n");

    /* -- [TRACE]1. Log Information if already initialised -- */
    /* INTENT[INT-TRC-002 → TC-TRC-005 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Call when initialized return ST_NO_ERROR and log a message */
    ptCtx = (trace_context_t*)trace_init(ST_TRUE);
    UC_CHECK("(INT-TRC-002) [Chk] Forcing bGUITraceEnabled to ST_TRUE & re-launch trace_init()"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_TRACE_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-TRC-005) bGUITraceEnabled is kept to TRUE", 
                (ptCtx->eViewMinLevel == LOG_LEVEL_TODO));
    } else printf("  [SKIP] (TC-TRC-005) ST_TRACE_CTX Object Check failed\n\n");    
    
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
    UC_CHECK("(INT-GUI-001) [Chk] Launch gui_init()", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_GUI_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-GUI-001) bInit is set to true", 
                (ptCtx->bInit == ST_TRUE));
        UC_TEST("[N] (TC-GUI-002) No windows is open", 
                (ptCtx->uiWndCount == 0));
        
        // Forcing dummy value for next test
        ptCtx->uiWndCount = 78;
    } else printf("  [SKIP] (TC-GUI-001...002) ST_GUI_CTX Object Check failed\n\n");

    /* -- [GUI]1. Log Information if already initialised -- */
    /* INTENT[INT-GUI-002 → TC-GUI-003 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Call when initialized return ST_NO_ERROR and log a message */
    ptCtx = (gui_context_t*)gui_init();
    UC_CHECK("(INT-GUI-002) [Chk] Forcing windows count to 78 & re-launch gui_init()"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_GUI_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-GUI-003) Window counter is kept to dummy value", 
                (ptCtx->uiWndCount == 78));

        // Reset bInitialized 
        ptCtx->bInit = ST_FALSE;
        ptCtx->uiWndCount = 78;
    } else printf("  [SKIP] (TC-GUI-003) ST_GUI_CTX Object Check failed\n\n");    
    
    /* -- [GUI]3. Create mutex list -- */
    /* -- [GUI]4. Platform-specific GUI init -- */
    /* INTENT[INT-GUI-003 → TC-GUI-004...007 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * check that gui_init() fills correctly the context structure with mutex
     * and platform-related functions initialized */
    ptCtx = (gui_context_t*)gui_init();
    UC_CHECK("(INT-GUI-003) [Chk] Forcing bInit to FALSE & re-launch gui_init()"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_GUI_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-GUI-004) bInit is set to true", 
                (ptCtx->bInit == ST_TRUE));
        UC_TEST("[N] (TC-GUI-005) No windows is open", 
                (ptCtx->uiWndCount == 0));
        UC_TEST("[N] (TC-GUI-006) Mutex is created", 
                (ptCtx->ptMutex != NULL));
        UC_TEST("[N] (TC-GUI-007) Platform-related init is OK", 
                (ptCtx->eGUIPtfInit == ST_NO_ERROR));     
    } else printf("  [SKIP] (TC-GUI-004...007) ST_GUI_CTX Object Check failed\n\n");
    
}

/*
 * uc00_st_module() - Initialize ST Machine module
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]5. Init Virtual ST machine --
 * 
 *   ST.c:
 *   -- [ST]1. Init the ST Machine context --
 *   -- [ST]2. Init ROM in memory --
 *   -- [ST]3. Init returns context sructure --
 * 
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc00_st_module()
{
    st_machine_context_t* ptCtx;

    printf("\n--- Test group 5: Init ST Module ---\n");
    
    /* -- [MAIN]5. Init Virtual ST machine -- */
    /* -- [ST]1. Init the ST Machine context -- */
    /* -- [ST]3. Init returns context sructure -- */
    /* INTENT[INT-STM-001 → TC-STM-001...002 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that ST Machine is initialized and context pointer valid */
    ptCtx = (st_machine_context_t*)st_init(NULL);
    UC_CHECK("(INT-STM-001) [Chk] Launch st_init(NULL)", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MACHINE_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-STM-001) ST Machine is ON", 
                (ptCtx->bPoweredOn == ST_TRUE));
        UC_TEST("[N] (TC-STM-002) Video default is European 50Hz", 
                (ptCtx->aShifterRegs[ST_VID_SYNC_MODE] == 0x02u));
        
        // Forcing "power-off"
        ptCtx->bPoweredOn = ST_FALSE;
    } else printf("  [SKIP] (TC-STM-001...002) ST_MACHINE_CTX "
                            "Object Check failed\n\n");

    /* -- [ST]2. Init ROM in memory -- */
    /* INTENT[INT-STM-002 → TC-STM-003...004 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that ST Machine ROM area is created (TODO) */
    st_u16_t uiWord = 0x1234;
    ptCtx = (st_machine_context_t*)st_init("test.rom");
    UC_CHECK("(INT-STM-002) [Chk] Launch st_init(\"test.rom\")", 
              (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MACHINE_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-STM-003) ST Machine is ON", 
                (ptCtx->bPoweredOn == ST_TRUE));
        
        ////////////////////////////////////////////////////
        // TODO - Do real test when function is implemented
        // Simulate a ROM uploaded
        ptCtx->bRomLoaded = ST_TRUE;
        st_read_word(0xFC0000u, &uiWord);
        UC_TEST("[TODO] (TC-STM-004) Check that ROM Start Address is not NULL (TODO)", 
                (uiWord == 0));
        ////////////////////////////////////////////////////
    } else printf("  [SKIP] (TC-STM-003...004) ST_MACHINE_CTX "
                            "Object Check failed\n\n");
}

/*
 * uc00_load_module() - Initialize the 'load' command
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]6. Init 'load' command --
 * 
 *   load.c:
 *   -- [LOAD]1. Attach an existing ST machine --
 *   -- [LOAD]2. Init returns context sructure --
 * 
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc00_load_module()
{
    load_context_t* ptCtx;
    st_machine_context_t* ptMachineCtx;

    printf("\n--- Test group 6: Init 'load' Command ---\n");
    
    /* -- [MAIN]6. Init 'load' command -- */
    /* -- [LOAD]1. Attach an existing ST machine -- */
    /* -- [LOAD]2. Init returns context sructure -- */
    /* INTENT[INT-LOD-001 → TC-LOD-001...002 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that load command initializes its own context, except if
     * input parameter is not a valid ST Machine context pointer */
    ptMachineCtx = (st_machine_context_t*)st_init("test.rom");
    ptCtx = (load_context_t*)load_init((st_u64_t)ptMachineCtx);
    UC_CHECK("(INT-LOD-001) [Chk] Launch load_init(ptMachineCtx)", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_LOAD_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-LOD-001) ST Machine is ON", 
                (st_u64_t)ptCtx->bIsMachineOn == ST_TRUE);
    } else printf("  [SKIP] (TC-LOD-001) ST_LOAD_CTX Object Check failed\n\n");

    ptCtx = (load_context_t*)load_init(ST_NO_ERROR);
    UC_TEST("[R] (TC-LOD-002) load_init() fails when an invalid machine is provided", 
                (st_u64_t)ptCtx == ST_ERROR);
    
}

/*
 * uc00_exec_module() - Initialize the 'exec' command
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]7. Init 'exec' command --
 * 
 *   exec.c:
 *   -- [EXEC]1. Attach an existing ST machine --
 *   -- [EXEC]2. Init exec context structure -- 
 *   -- [EXEC]3. Init returns context sructure -- 
 * 
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc00_exec_module()
{
    exec_context_t* ptCtx;
    st_machine_context_t* ptMachineCtx;

    printf("\n--- Test group 7: Init 'exec' Command ---\n");
    
    /* -- [MAIN]7. Init 'exec' command -- */
    /* -- [EXEC]1. Attach an existing ST machine -- */
    /* -- [EXEC]2. Init exec context structure -- */
    /* -- [EXEC]3. Init returns context sructure -- */
    /* INTENT[INT-EXE-001 → TC-EXE-001...003 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that load command initializes its own context, except if
     * input parameter is not a valid ST Machine context pointer */
    ptMachineCtx = (st_machine_context_t*)st_init("test.rom");
    ptCtx = (exec_context_t*)exec_init((st_u64_t)ptMachineCtx);
    UC_CHECK("(INT-EXE-001) [Chk] Launch exec_init(ptMachineCtx)", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_EXEC_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-EXE-001) ST Machine is ON", 
                (st_u64_t)ptCtx->bIsMachineOn == ST_TRUE);
        UC_TEST("[N] (TC-EXE-002) 'exec' module init is OK", 
                (st_u64_t)ptCtx->bInitOK == ST_TRUE);
    } else printf("  [SKIP] (TC-EXE-001...002) ST_EXEC_CTX Object Check failed\n\n");

    ptCtx = (exec_context_t*)exec_init(ST_NO_ERROR);
    UC_TEST("[R] (TC-EXE-003) exec_init() fails when an invalid machine is provided", 
                (st_u64_t)ptCtx == ST_ERROR);
}

/*
 * uc00_console_module() - Initialize main console
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]8. Init Main Console --
 * 
 *   line.c:
 *   -- [LINE]1. Reject any NULL incoming parameter --
 *   -- [LINE]2. Log Information if already initialised --
 *   -- [LINE]3. Auto-detect ANSI capability --
 *   -- [LINE]4. get current working directory --
 *   -- [LINE]5. Fills context structure fields (mutex, script) --
 *   -- [LINE]6. Load console commands history --
 * 
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc00_console_module()
{
    line_context_t* ptCtx;
    st_bool_t       bExpectedColors;

    printf("\n--- Test group 8: Init Main Console ---\n");
    
    /* -- [MAIN]8. Init Main Console -- */
    /* -- [LINE]1. Reject any NULL incoming parameter -- */
    /* -- [LINE]3. Auto-detect ANSI capability -- */
    /* -- [LINE]4. get current working directory -- */
    /* -- [LINE]5. Fills context structure fields (mutex, script) -- */
    /* -- [LINE]6. Load console commands history -- */
    /* INTENT[INT-CON-001 → TC-CON-001...007 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that console line initializes its own context, except if
     * input parameter is NULL pointer */
    ptCtx = (line_context_t*)line_init("");
    UC_CHECK("(INT-CON-001) [Chk] Launch line_init(NULL)", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_LINE_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-CON-001) Console init is OK", 
                ptCtx->bRunning == ST_TRUE);
        /* bColors = isatty(stdout): TRUE in a TTY, FALSE under make/pipe */
#ifdef ST_PLATFORM_WINDOWS
        bExpectedColors = _isatty(_fileno(stdout)) ? ST_TRUE : ST_FALSE;
#else
        bExpectedColors = isatty(STDOUT_FILENO) ? ST_TRUE : ST_FALSE;
#endif
        UC_TEST("[N] (TC-CON-002) Console ANSI colors matches isatty(stdout)",
                ptCtx->bColors == bExpectedColors);
        UC_TEST("[N] (TC-CON-003) szCwd is set", 
                ptCtx->szCwd[0] != '\0');
        UC_TEST("[N] (TC-CON-004) Mutex is initialized", 
                ptCtx->ptSelectedMutex != NULL);
        UC_TEST("[N] (TC-CON-005) Script name is the one provided as input", 
                ptCtx->szScriptFile[0] == '\0');
        UC_TEST("[N] (TC-CON-006) History is initialized", 
                ptCtx->iHistCount >= 0);

        // Forcing bRunning to ST_FALSE for subsequent tests
        ptCtx->bColors = ST_FALSE;
    } else printf("  [SKIP] (TC-CON-001...006) ST_EXEC_CTX Object Check failed\n\n");

    ptCtx = (line_context_t*)line_init(NULL);
    UC_TEST("[R] (TC-CON-007) lint_init(NULL) fails", 
                (st_u64_t)ptCtx == ST_ERROR);

    /* -- [LINE]2. Log Information if already initialised -- */
    /* -- [LINE]5. Fills context structure fields (mutex, script) -- */
    /* INTENT[INT-CON-002 → TC-CON-008...009 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Call when initialized return ST_NO_ERROR and log a message */
    ptCtx = (line_context_t*)line_init("script.txt");
    UC_CHECK("(INT-CON-002) [Chk] Force bColors to ST_FALSE & re-launch line_init(\"script.txt\")"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_LINE_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-CON-008) bColors is unchanged after a second call", 
                ptCtx->bColors == ST_FALSE);
        UC_TEST("[N] (TC-CON-009) Script name is not updated", 
                ptCtx->szScriptFile[0] == '\0');

        // Forcing bRunning to ST_FALSE for subsequent tests
        ptCtx->bRunning = ST_FALSE;
    } else printf("  [SKIP] (TC-CON-008...009) ST_LINE_CTX Object Check failed\n\n");    
    
    /* -- [LINE]5. Fills context structure fields (mutex, script) -- */
    /* INTENT[INT-CON-003 → TC-CON-010 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Check that mutex & script name are captured in context structure */
    ptCtx = (line_context_t*)line_init("script.txt");
    UC_CHECK("(INT-CON-003) [Chk] Force bRunning to ST_FALSE & re-launch line_init(\"script.txt\")"
              , (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_LINE_CTX);
    if (gIsObject) 
    {
        UC_TEST("[N] (TC-CON-010) Script name is updated", 
                      strcmp(ptCtx->szScriptFile, "script.txt") == 0);
    } else printf("  [SKIP] (TC-CON-010) ST_LINE_CTX Object Check failed\n\n");

}

/*
 * uc00_shutdown_sequence() - Shutdown all modules in the exact order
 *                            used by ST4Ever_shutdown() and verify each
 *                            context remains a recognized object (magic
 *                            + eObject intact) afterward.
 *
 * st_shutdown() is the reference/canonical pattern (full memset, then
 * ulMagic/eObject restored) - line_shutdown()/load_shutdown() used to
 * memset without restoring the magic, and gui_shutdown()/exec_shutdown()/
 * trace_shutdown() only reset a handful of fields; all six were
 * homogenized with st_shutdown()'s pattern before writing this test, so
 * that a single, uniform assertion ("still a recognized object, freshly
 * reset") holds for every module.
 *
 * Code Coverage:
 *   main.c:
 *   -- [MAIN]16. Shutdown Main Console --
 *   -- [MAIN]17. Shutdown 'exec' command --
 *   -- [MAIN]18. Shutdown 'load' command --
 *   -- [MAIN]19. Shutdown Virtual ST machine --
 *   -- [MAIN]20. Shutdown GUI Module --
 *   -- [MAIN]21. Shutdown Trace Module --
 *
 *   ST.c:
 *   -- [ST]30. Reset the ST Machine context to power-off state --
 *
 *   trace.c:
 *   -- [TRACE]24. Do nothing if trace module is not initialized --
 *   -- [TRACE]25. Close the GUI window if still open --
 *   -- [TRACE]26. Close the trace log file --
 *   -- [TRACE]27. Reset trace context state to uninitialized --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc00_shutdown_sequence()
{
    line_context_t*       ptLineCtx;
    exec_context_t*       ptExecCtx;
    load_context_t*       ptLoadCtx;
    st_machine_context_t* ptSTCtx;
    gui_context_t*        ptGUICtx;
    trace_context_t*      ptTrcCtx;
    st_error_t             eResult;

    printf("\n--- Test group 9: Shutdown sequence "
           "(ST4Ever_shutdown order) ---\n");

    ptSTCtx   = (st_machine_context_t*)st_init(NULL);
    ptLineCtx = (line_context_t*)line_init("");
    ptExecCtx = (exec_context_t*)exec_init((st_u64_t)ptSTCtx);
    ptLoadCtx = (load_context_t*)load_init((st_u64_t)ptSTCtx);
    ptGUICtx  = (gui_context_t*)gui_init();
    ptTrcCtx  = (trace_context_t*)trace_init(ST_FALSE);

    /* -- [MAIN]16. Shutdown Main Console -- */
    /* -- [MAIN]17. Shutdown 'exec' command -- */
    /* -- [MAIN]18. Shutdown 'load' command -- */
    /* -- [MAIN]19. Shutdown Virtual ST machine -- */
    /* -- [ST]30. Reset the ST Machine context to power-off state -- */
    /* -- [MAIN]20. Shutdown GUI Module -- */
    /* -- [MAIN]21. Shutdown Trace Module -- */
    /* INTENT[INT-APP-006 → TC-APP-015...025 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * ST4Ever_shutdown() must shut down each subsystem in sequence
     * and return exactly ST_NO_ERROR when none of them fails -
     * This involves the console, exec, load, ST Machine, GUI and Trace */
    eResult = ST4Ever_shutdown();
    UC_INFO("(INT-APP-006) ST4Ever_shutdown() full orchestration");
    UC_TEST("[N] (TC-APP-015) ST4Ever_shutdown() returns exactly"
            " ST_NO_ERROR when every subsystem shuts down cleanly",
            eResult == ST_NO_ERROR);

    UC_CHECK_OBJ(ptLineCtx, ST_LINE_CTX);
    UC_TEST("[N] (TC-APP-016) context is still a recognized object"
            " after line_shutdown()", gIsObject);
    if (gIsObject)
    {
        UC_TEST("[N] (TC-APP-017) bRunning is reset to ST_FALSE",
                ptLineCtx->bRunning == ST_FALSE);
    } else printf("  [SKIP] (TC-APP-017) ST_LINE_CTX Object Check failed\n\n");

    UC_CHECK_OBJ(ptExecCtx, ST_EXEC_CTX);
    UC_TEST("[N] (TC-APP-018) context is still a recognized object"
            " after exec_shutdown()", gIsObject);
    if (gIsObject)
    {
        UC_TEST("[N] (TC-APP-019) bInitOK is reset to ST_FALSE",
                ptExecCtx->bInitOK == ST_FALSE);
    } else printf("  [SKIP] (TC-APP-019) ST_EXEC_CTX Object Check failed\n\n");

    UC_CHECK_OBJ(ptLoadCtx, ST_LOAD_CTX);
    UC_TEST("[N] (TC-APP-020) context is still a recognized object"
            " after load_shutdown()", gIsObject);
    if (gIsObject)
    {
        UC_TEST("[N] (TC-APP-021) bIsMachineOn is reset to ST_FALSE",
                ptLoadCtx->bIsMachineOn == ST_FALSE);
    } else printf("  [SKIP] (TC-APP-021) ST_LOAD_CTX Object Check failed\n\n");

    UC_CHECK_OBJ(ptSTCtx, ST_MACHINE_CTX);
    UC_TEST("[N] (TC-APP-022) context is still a recognized object"
            " after st_shutdown()", gIsObject);
    if (gIsObject)
    {
        UC_TEST("[N] (TC-APP-023) bPoweredOn is reset to ST_FALSE",
                ptSTCtx->bPoweredOn == ST_FALSE);
    } else printf("  [SKIP] (TC-APP-023) ST_MACHINE_CTX Object Check failed\n\n");

    UC_CHECK_OBJ(ptGUICtx, ST_GUI_CTX);
    UC_TEST("[N] (TC-APP-024) context is still a recognized object"
            " after gui_shutdown()", gIsObject);
    if (gIsObject)
    {
        UC_TEST("[N] (TC-APP-025) bInit is reset to ST_FALSE",
                ptGUICtx->bInit == ST_FALSE);
    } else printf("  [SKIP] (TC-APP-025) ST_GUI_CTX Object Check failed\n\n");

    /* -- [TRACE]24. Do nothing if trace module is not initialized -- */
    /* -- [TRACE]25. Close the GUI window if still open -- */
    /* -- [TRACE]26. Close the trace log file -- */
    /* -- [TRACE]27. Reset trace context state to uninitialized -- */
    /* INTENT[INT-TRC-008 → TC-TRC-024...025 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * trace_shutdown() must reset the trace context to a clean state,
     * while it remains a recognized ST_TRACE_CTX object - it must run
     * last: no further LOG_* call is guaranteed to reach the log file
     * afterward */
    UC_INFO("(INT-TRC-008) trace_shutdown() test");
    UC_CHECK_OBJ(ptTrcCtx, ST_TRACE_CTX);
    UC_TEST("[N] (TC-TRC-024) context is still a recognized object"
            " after trace_shutdown()", gIsObject);
    if (gIsObject)
    {
        UC_TEST("[N] (TC-TRC-025) bInitialised is reset to ST_FALSE",
                ptTrcCtx->bInitialised == ST_FALSE);
    } else printf("  [SKIP] (TC-TRC-025) ST_TRACE_CTX Object Check failed\n\n");
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
    uc00_st_module();
    uc00_load_module();
    uc00_exec_module();
    uc00_console_module();

    /* Shutdown sequence - exercises every *_shutdown(), including the
     * final trace_shutdown() (no separate cleanup call needed) */
    uc00_shutdown_sequence();
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
