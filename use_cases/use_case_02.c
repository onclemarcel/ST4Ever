/*
 * use_case_02.c - UC2 Validation: trace command toggles the GUI View
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * R23/R24/R25: migrated from a free-standing trace_init()/trace_gui_open()
 * driver to the ST4Ever_init() context-access pattern (R24 §"Accès aux
 * contextes de modules") - ptCtx->ptTraceCtx is captured once in main()
 * and reused by every group, exactly like use_case_01.c's uc01_test_trace().
 * INT-TRC-xxx/TC-TRC-xxx numbering continues from the highest id used in
 * use_case_00.c/use_case_01.c (INT-TRC-008, TC-TRC-025) - the only two
 * other files already migrated to R24 at the time of writing.
 
 * Strategy R26: line_cmd_trace() and line_dispatch() are static.
 * UC2 validates behavioral contracts by driving the *real* 
 * line_dispatch()/line_cmd_trace() code path end-to-end, via 
 * line_run() in script (batch) mode (UC4.3's `--script` mechanism) with
 * the console's szScriptFile field set directly on ptCtx->ptConsoleCtx 
 *
 * White box test strategy using script debug mode:
 * When internal states are needed for test purpose:
 *   Script mode can be executed in debug line-by-line mode using:
 *   - line_enable_script_debug()
 *   - line_run() for a single line execution - note that a comment is a valid line
 *   - make use of ptCtx fields to access to internal globals values of modules
 *   - continue execution through line_run() or,
 *   - line_disable_script_debug() to come back to full script run mode
 *   - line_run() completes the script in headless mode
 * 
 *
 * TEST MATRIX:
 *   [N] Nominal    : 16 tests  - toggle, state consistency,
 *                                real command dispatch (trace/level/bad)
 *   [R] Robustness :  4 tests  - idempotent open/close, level isolation
 *   [S] Skipped    :  0 tests
 *
 * Test groups:
 *   Group 1: trace (toggle)   - Start with GUI open, close when open,
 *                               check that trace level filter is not
 *							     affected by the GUI open/close
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"

/* Required by the UC_TEST / UC_CHECK macros in use_cases.h */
int       g_uc_fails = 0;
st_bool_t gIsObject  = ST_FALSE;

static ST4Ever_context_t *ptCtx;

/* ------------------------------------------------------------------
 * Group 1: trace command (toggle - Start ST4Ever with -t)
 * ------------------------------------------------------------------ */
/*
 * uc02_test_trace_toggle() - `trace` (no argument) toggle behaviour
 *
 * Code Coverage:
 *   trace.c: - supporting execution of commands (tested specifically in
 *              other use_case_*.c
 *
 *   line.c:
 *   -- [LINE]8. 'trace' with no option toggle the GUI status --
 *   -- [LINE]9. 'trace' with option does not toggle the GUI --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc02_test_trace_toggle(void)
{
    static char 	 szPath[ST_MAX_PATH];
    FILE            *pF;
	trace_context_t *ptTraceCtx = (trace_context_t*)ptCtx->ptTraceCtx;
	line_context_t  *ptLineCtx  = (line_context_t*)ptCtx->ptConsoleCtx;

    printf("\n--- Test group 1: trace toggle (start with -t) ---\n");

    ptLineCtx->bQuiet = ST_TRUE;    // Do not show console messages during test

    /* -- [LINE]8. 'trace' with no option toggle the GUI status -- */
	/* -- [LINE]9. 'trace' with option does not toggle the GUI -- */
	/* INTENT[INT-TRC-009 → TC-TRC-026...033 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Starting from an open trace console at start of application, 'trace'
	 * toggles open GUI to close, 'trace' with option does not re-open it,
     * 'trace' with no argument re-opens, and finally 'trace' with invalid
     * argument do not toggle the GUI	 */
    UC_TEST("(INT-TRC-009) [Chk] console open at startup (-t/--trace)",
            ptTraceCtx->bOpen == ST_TRUE);

	UC_SCRIPT_CREATE(szPath, sizeof(szPath), "use_cases/UC02/trace_dispatch.txt", pF);

    UC_SCRIPT_ADD_LINE(pF, "# UC2 Group 1 Toggle trace\n");
    UC_SCRIPT_ADD_LINE(pF, "trace\n");		// First command closes the GUI
    UC_SCRIPT_ADD_LINE(pF, "trace level all\n");   // Set trace level (no opening)
    UC_SCRIPT_ADD_LINE(pF, "trace \n");     // Open GUI 
    UC_SCRIPT_ADD_LINE(pF, "trace xyz\n");  // Incorrect option - do not close GUI
    
	UC_SCRIPT_CLOSE(pF);
	
    strncpy(ptLineCtx->szScriptFile, szPath, ST_MAX_PATH - 1);
    ptLineCtx->szScriptFile[ST_MAX_PATH - 1] = '\0';

	line_enable_script_debug();
	UC_CHECK("(INT-TRC-009) [Chk] line_run()  [#Comment]",
			 line_run());	// Skip comment
			 
	UC_CHECK("(INT-TRC-009) [Chk] line_run()  [1st cmd: trace]",
			 line_run());   // Closes already open GUI
	UC_TEST("[N] (TC-TRC-026) GUI is closed",
			ptTraceCtx->bOpen == ST_FALSE);
	UC_TEST("[N] (TC-TRC-027) Trace level is INFO",
			ptTraceCtx->eViewMinLevel == LOG_LEVEL_INFO);
	
	UC_CHECK("(INT-TRC-009) [Chk] line_run()  [2nd cmd: trace level all]",
			 line_run());   // GUI remains closed
	UC_TEST("[N] (TC-TRC-028) GUI remains closed",
			ptTraceCtx->bOpen == ST_FALSE);
	UC_TEST("[N] (TC-TRC-029) Trace level is set to TRACE",
			ptTraceCtx->eViewMinLevel == LOG_LEVEL_TRACE);
	
	UC_CHECK("(INT-TRC-009) [Chk] line_run()  [3rd cmd: trace]",
			 line_run());   // Open GUI - trace level is unchanged
	UC_TEST("[N] (TC-TRC-030) GUI is re-open",
			ptTraceCtx->bOpen == ST_TRUE);
	UC_TEST("[N] (TC-TRC-031) Trace level is kept to TRACE",
			ptTraceCtx->eViewMinLevel == LOG_LEVEL_TRACE);
	
	UC_CHECK("(INT-TRC-009) [Chk] line_run()  [4th cmd: trace xyz]",
			 line_run());   // Incorrect option - no change
	UC_TEST("[N] (TC-TRC-032) GUi is kept open",
			ptTraceCtx->bOpen == ST_TRUE);
	UC_TEST("[N] (TC-TRC-033) Trace level is kept to TRACE",
			ptTraceCtx->eViewMinLevel == LOG_LEVEL_TRACE);
	
	UC_CHECK("(INT-TRC-009) [Chk] line_run()  [complete]",
			 line_run());
			 
	UC_TEST("(INT-TRC-009) [Chk] Debug mode is actually disabled",
            ptLineCtx->bDebugSteps == ST_FALSE);

}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    char *args[] = {"UC02", "-t"};

    printf("\n");
    printf("============================================================"
           "====\n");
    printf(" UC2 - trace on / off / toggle command validation\n");
    printf("============================================================"
           "====\n");

    ptCtx = (ST4Ever_context_t *)ST4Ever_init(2, args);
    UC_CHECK("(UC02) [Chk] Launch ST4Ever with -t option", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject)
    {
        uc02_test_trace_toggle();
    }
    else
    {
        printf("  [SKIP] (UC02) ST_MAIN_CTX Object Check failed\n\n");
    }

    printf("\n--- Shutdown ---\n");
    ST4Ever_shutdown();
    printf("  [INFO] trace_shutdown() complete - "
           "see st4ever_trace.log for full trace\n");

    printf("\n");
    printf("============================================================"
           "====\n");
    if (g_uc_fails == 0)
    {
        printf(" UC2 PASSED - 0 failures\n");
    }
    else
    {
        printf(" UC2 FAILED - %d failure(s)\n", g_uc_fails);
    }
    printf("============================================================"
           "====\n\n");

    return (g_uc_fails > 0) ? 1 : 0;
}
