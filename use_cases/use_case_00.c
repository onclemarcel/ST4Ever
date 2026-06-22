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
 *   -- 1. Parse command-line options --
 * 
 * Parameters:
 *   None
 *
 * Returns: ST_NO_ERROR on success.
 */
static void uc00_manage_options()
{
    ST4Ever_context_t* ptCtx;

    printf("\n--- Test group 1: Parse command-line options ---\n");
    
    /* -- 1. Parse command-line options -- */
    /* INTENT[INT-APP-001 → TC-APP-001 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * call the ST4Ever main function with no argument : init is OK 
     * bTraceAtStart is FALSE */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(1, g_uc00_szArgs_none);
    UC_CHECK("[N] Launch ST4Ever with no argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceAtStart is set to TRUE", 
                (ptCtx->bTraceAtStart == ST_FALSE)); 
    }

    /* --- 1.1 Manage the Trace Console option --- */
    /* INTENT[INT-APP-002 → TC-APP-002...004 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * -t|--trace option is captured and bTraceAtStart is set to TRUE */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_trace_short);
    UC_CHECK("[N] Launch ST4Ever with -t", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceAtStart is set to TRUE", 
                 (ptCtx->bTraceAtStart == ST_TRUE)); 
    }

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(4, g_uc00_szArgs_trace_last);
    UC_CHECK("[N] Launch ST4Ever with -t as last argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceAtStart is set to TRUE", 
                (ptCtx->bTraceAtStart == ST_TRUE)); 
    }

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_trace_long);
    UC_CHECK("[N] Launch ST4Ever with --trace", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] bTraceAtStart is set to TRUE", 
                 (ptCtx->bTraceAtStart == ST_TRUE)); 
    }

    /* --- 1.2 Manage the input script option --- */
    /* INTENT[INT-APP-003 → TC-APP-005...008 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * --script option is captured with its file name stored in szScriptFile */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(4, g_uc00_szArgs_trace_last);
    UC_CHECK("[N] Launch ST4Ever with --script no_script.txt", (st_u64_t)ptCtx);
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
    UC_CHECK("[N] Launch ST4Ever with --trace --script n", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (!g_uc_fails) 
    {
        UC_TEST("[N] szScriptFile is set - length is 1", 
                 (strlen(ptCtx->szScriptFile) == 1));
        UC_TEST("[N] szScriptFile is 'n'", 
                 (strncmp(ptCtx->szScriptFile, "n", 
                          strlen(ptCtx->szScriptFile)) == 0));  
    }

    /* --- 1.3 Manage help option --- */
    /* INTENT[INT-APP-004 → TC-APP-009...010 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * -h|--help is captured and application quits */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_help_short);
    UC_TEST("[R] Launch ST4Ever with -h", 
                ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(5, g_uc00_szArgs_help_long);
    UC_TEST("[R] Launch ST4Ever with --help in the middle", 
                ((st_u64_t)ptCtx == ST_QUIT));

    /* --- 1.4 Manage unknown options --- */
    /* INTENT[INT-APP-005 → TC-APP-011...012 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * incorrect short/long options are captured, init sends ST_ERROR */
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(3, g_uc00_szArgs_err_short);
    UC_TEST("[R] Launch ST4Ever with invalid -x argument", 
                ((st_u64_t)ptCtx == ST_QUIT));

    ptCtx = (ST4Ever_context_t*)ST4Ever_init(2, g_uc00_szArgs_err_long);
    UC_TEST("[R] Launch ST4Ever with invalid --wrong argument", 
                ((st_u64_t)ptCtx == ST_QUIT));
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

    /* Launch main function */
    uc00_manage_options();
    
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
