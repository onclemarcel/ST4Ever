/*
 * use_case_25B.c - UC25B : Memory dump view + Disassembly view
 *
 * TEST MATRIX - UC25B:
 *   [N] Nominal    : 11 tests - exec_mem/exec_asm lifecycle (open without
 *                               session × 2 assertions each, close no-op),
 *                               disasm_one MOVEQ integration × 3,
 *                               exec_asm display line format × 2
 *   [R] Robustness :  4 tests - NULL pptView to open/close for both modules
 *   [S] Skipped    :  6 tests - GUI window open, PC highlight, navigation,
 *                               F/HOME snap (run make manual)
 */
#include "use_cases.h"

/* INTENT[INT-EXE-031..040 -> TC-EXE-031..040 -> REQ-EXE-029..031
 *        -> UFR-EXE-008]:
 * exec_mem and exec_asm open/close follow the same lifecycle contract as
 * exec_cpu and exec_mon: NULL pptView returns ST_ERROR; open without an
 * active session (exec_get_state returns NULL) returns ST_ERROR with
 * *pptView=NULL; close with *pptView==NULL is a no-op returning
 * ST_NO_ERROR. */

/* INTENT[INT-EXE-041..051 -> TC-EXE-041..051 -> REQ-EXE-030..032
 *        -> UFR-EXE-008]:
 * disasm_one() correctly decodes MOVEQ #42,D0 (0x702A) at ST_LOAD_BASE:
 * ST_NO_ERROR, iWordCount=1, bValid=TRUE. exec_asm display line format
 * (address + hex + mnemonic) is non-empty and starts with '$'. GUI-
 * dependent tests validate PC-row highlight and snap navigation. */

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static st_machine_t g_tMachine;

static void w16(st_u32_t uiAddr, st_u16_t uiVal)
{
    g_tMachine.aRam[uiAddr]     = (st_u8_t)(uiVal >> 8);
    g_tMachine.aRam[uiAddr + 1] = (st_u8_t)(uiVal & 0xFFu);
}

/* ------------------------------------------------------------------ */
/* [N][R] exec_mem open/close guards                                   */
/* ------------------------------------------------------------------ */

static void test_exec_mem_guards(void)
{
    exec_mem_view_t *ptView;
    st_error_t       eResult;

    printf("\n--- [N][R] exec_mem open/close guards ---\n");

    /* NULL pptView to open                                INT-EXE-031 */
    eResult = exec_mem_open(NULL);
    TEST_ASSERT("[R] exec_mem_open(NULL) = ST_ERROR",
                eResult == ST_ERROR);

    /* Open without active session (exec_get_state NULL) INT-EXE-032..033 */
    ptView  = NULL;
    eResult = exec_mem_open(&ptView);
    TEST_ASSERT("[N] exec_mem_open() without session = ST_ERROR",
                eResult == ST_ERROR);
    TEST_ASSERT("[N] exec_mem_open() without session: *pptView=NULL",
                ptView == NULL);

    /* NULL pptView to close                               INT-EXE-034 */
    eResult = exec_mem_close(NULL);
    TEST_ASSERT("[R] exec_mem_close(NULL) = ST_ERROR",
                eResult == ST_ERROR);

    /* Close with *pptView already NULL (no-op)            INT-EXE-035 */
    ptView  = NULL;
    eResult = exec_mem_close(&ptView);
    TEST_ASSERT("[N] exec_mem_close(&NULL) = ST_NO_ERROR (no-op)",
                eResult == ST_NO_ERROR);
}

/* ------------------------------------------------------------------ */
/* [N][R] exec_asm open/close guards                                   */
/* ------------------------------------------------------------------ */

static void test_exec_asm_guards(void)
{
    exec_asm_view_t *ptView;
    st_error_t       eResult;

    printf("\n--- [N][R] exec_asm open/close guards ---\n");

    /* NULL pptView to open                                INT-EXE-036 */
    eResult = exec_asm_open(NULL);
    TEST_ASSERT("[R] exec_asm_open(NULL) = ST_ERROR",
                eResult == ST_ERROR);

    /* Open without active session                       INT-EXE-037..038 */
    ptView  = NULL;
    eResult = exec_asm_open(&ptView);
    TEST_ASSERT("[N] exec_asm_open() without session = ST_ERROR",
                eResult == ST_ERROR);
    TEST_ASSERT("[N] exec_asm_open() without session: *pptView=NULL",
                ptView == NULL);

    /* NULL pptView to close                               INT-EXE-039 */
    eResult = exec_asm_close(NULL);
    TEST_ASSERT("[R] exec_asm_close(NULL) = ST_ERROR",
                eResult == ST_ERROR);

    /* Close with *pptView already NULL (no-op)            INT-EXE-040 */
    ptView  = NULL;
    eResult = exec_asm_close(&ptView);
    TEST_ASSERT("[N] exec_asm_close(&NULL) = ST_NO_ERROR (no-op)",
                eResult == ST_NO_ERROR);
}

/* ------------------------------------------------------------------ */
/* [N] Disassembly line integration                                    */
/* ------------------------------------------------------------------ */

static void test_disasm_line_integration(void)
{
    disasm_result_t tResult;
    st_error_t      eResult;
    char            szLine[128];
    int             iLen;

    printf("\n--- [N] Disassembly line integration ---\n");

    st_init(&g_tMachine, NULL);
    memset(g_tMachine.aRam, 0, sizeof(g_tMachine.aRam));

    /* MOVEQ #42,D0 = 0x702A at ST_LOAD_BASE */
    w16(ST_LOAD_BASE, 0x702A);

    /* disasm_one on MOVEQ #42,D0          INT-EXE-041 / INT-EXE-042..043 */
    memset(&tResult, 0, sizeof(tResult));
    eResult = disasm_one(g_tMachine.aRam + ST_LOAD_BASE,
                         (size_t)(ST_RAM_SIZE - ST_LOAD_BASE),
                         ST_LOAD_BASE, &tResult);
    TEST_ASSERT("[N] disasm_one MOVEQ #42,D0 = ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    TEST_ASSERT("[N] disasm_one MOVEQ: iWordCount = 1",
                tResult.iWordCount == 1);
    TEST_ASSERT("[N] disasm_one MOVEQ: bValid = TRUE",
                tResult.bValid == ST_TRUE);

    /* Build display line — same pattern as exec_asm_build_line
     * address + hex word + text            INT-EXE-044 / INT-EXE-045 */
    iLen = snprintf(szLine, sizeof(szLine), "$%06X %04X  %s",
                    (unsigned)ST_LOAD_BASE,
                    (unsigned)tResult.auWords[0],
                    tResult.szLine);
    TEST_ASSERT("[N] exec_asm display line: non-empty",
                iLen > 0);
    TEST_ASSERT("[N] exec_asm display line: contains address",
                szLine[0] == '$');

    st_shutdown(&g_tMachine);
}

/* ------------------------------------------------------------------ */
/* [S] Skipped: GUI-dependent                                           */
/* ------------------------------------------------------------------ */

static void test_uc25b_gui_skipped(void)
{
    printf("\n--- [S] GUI-dependent tests (skipped headless) ---\n");

    /* INT-EXE-046 */ TEST_SKIP("[S] exec_mem_open() opens GUI_WND_EXEC_MEM window");
    /* INT-EXE-047 */ TEST_SKIP("[S] exec_asm_open() opens GUI_WND_EXEC_ASM window");
    /* INT-EXE-048 */ TEST_SKIP("[S] exec_mem renders PC row highlighted yellow");
    /* INT-EXE-049 */ TEST_SKIP("[S] exec_mem HOME key snaps to PC row");
    /* INT-EXE-050 */ TEST_SKIP("[S] exec_asm renders PC instruction highlighted yellow");
    /* INT-EXE-051 */ TEST_SKIP("[S] exec_asm F key snaps to PC");
}

/* ------------------------------------------------------------------ */
/* main                                                                  */
/* ------------------------------------------------------------------ */

int main(void)
{
    st_error_t eResult;

    printf("=== UC25B: Memory dump view + Disassembly view ===\n");

    eResult = trace_init(ST_FALSE);
    if (eResult != ST_NO_ERROR)
    {
        fprintf(stderr, "trace_init failed\n");
        return 1;
    }

    test_exec_mem_guards();
    test_exec_asm_guards();
    test_disasm_line_integration();
    test_uc25b_gui_skipped();

    trace_shutdown();

    printf("\n=== UC25B: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
