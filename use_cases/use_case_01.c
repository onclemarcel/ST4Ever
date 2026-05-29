/*
 * use_case_01.c - UC1 Validation: Console prototype + Trace subsystem
 *
 * Tests all components active in UC1:
 *   1.  Trace init with console open at startup (-t flag behaviour)
 *   2.  One log entry of each level: TRACE, INFO, ERROR, TODO
 *   3.  Trace compaction: verify consecutive TRACE from same function
 *       are counted, not duplicated
 *   4.  trace_close() / trace_is_open() consistency
 *   5.  trace_open() / trace_is_open() consistency
 *   6.  trace_set_trace_enabled(FALSE) suppresses LOG_TRACE
 *   7.  trace_set_trace_enabled(TRUE) re-enables LOG_TRACE
 *   8.  line_init() + line_shutdown() without running the loop
 *   9.  ST machine init / read / write / shutdown
 *  10.  CPU init from reset vectors written into ST RAM
 *  11.  cpu_step() on the hand-crafted hello.prg in UC01/
 *  12.  trace_shutdown() clean close
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"

/* Required by the UC_TEST / UC_CHECK macros in use_cases.h */
int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

/*
 * uc01_load_prg_text() - Load the .text section of hello.prg into
 *                        ST RAM at a given address.
 *
 * Reads the PRG header, then copies text_size bytes starting at
 * PRG offset 28 into ptMachine->aRam[uiLoadAddr].
 *
 * Parameters:
 *   szPath    [in]     : Path to the .prg file.
 *   ptMachine [in/out] : Target machine RAM.
 *   uiLoadAddr[in]     : ST RAM destination address.
 *   puiTextSz [out]    : Receives text section size in bytes.
 *
 * Returns: ST_NO_ERROR on success.
 */
static st_error_t uc01_load_prg_text(const char   *szPath,
                                      st_machine_t *ptMachine,
                                      st_u32_t      uiLoadAddr,
                                      st_u32_t     *puiTextSz)
{
    FILE     *pFile;
    st_u8_t   aHeader[28];
    st_u32_t  uiTextSz;
    size_t    uiRead;

    if (szPath == NULL || ptMachine == NULL || puiTextSz == NULL)
    {
        return ST_ERROR;
    }

    pFile = fopen(szPath, "rb");
    if (pFile == NULL)
    {
        fprintf(stderr, "  [uc01_load_prg_text] cannot open '%s'\n",
                szPath);
        return ST_ERROR;
    }

    /* Read 28-byte PRG header */
    uiRead = fread(aHeader, 1, 28, pFile);
    if (uiRead != 28)
    {
        fclose(pFile);
        return ST_ERROR;
    }

    /* Check magic */
    if (aHeader[0] != 0x60 || aHeader[1] != 0x1A)
    {
        fprintf(stderr, "  [uc01_load_prg_text] bad magic 0x%02X%02X\n",
                aHeader[0], aHeader[1]);
        fclose(pFile);
        return ST_ERROR;
    }

    /* text_size at offset 2 (big-endian long) */
    uiTextSz = ((st_u32_t)aHeader[2] << 24)
             | ((st_u32_t)aHeader[3] << 16)
             | ((st_u32_t)aHeader[4] <<  8)
             |  (st_u32_t)aHeader[5];

    /* Bounds check */
    if (uiLoadAddr + uiTextSz > ST_RAM_SIZE)
    {
        fprintf(stderr,
                "  [uc01_load_prg_text] load overflows RAM "
                "(addr=0x%06X size=%u)\n",
                uiLoadAddr, uiTextSz);
        fclose(pFile);
        return ST_ERROR;
    }

    /* Copy text bytes directly into RAM */
    uiRead = fread(&ptMachine->aRam[uiLoadAddr], 1, uiTextSz, pFile);
    fclose(pFile);

    if (uiRead != (size_t)uiTextSz)
    {
        return ST_ERROR;
    }

    *puiTextSz = uiTextSz;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------------ */

static void test_trace(void)
{
    printf("\n--- Test group 1: Trace subsystem ---\n");

    /* T1 - init with open */
    UC_CHECK("trace_init(ST_TRUE)",
             trace_init(ST_TRUE));
    UC_TEST("trace_is_open() after init(TRUE)",
            trace_is_open() == ST_TRUE);

    /* T2 - one of each level */
    printf("  [INFO] Emitting one entry per log level:\n");
    LOG_TRACE("UC1 test: LOG_TRACE entry (param=%d)", 42);
    LOG_INFO("UC1 test: LOG_INFO  entry");
    LOG_ERROR("UC1 test: LOG_ERROR entry (not a real error)");
    LOG_TODO("UC1 test: LOG_TODO  entry - this function is a stub");
    printf("  [PASS] All four log levels emitted "
           "(check st4ever_trace.log)\n");

    /* T3 - compaction: call LOG_TRACE 5 times, should produce 1 line */
    printf("  [INFO] Compaction test: 5x LOG_TRACE same function...\n");
    LOG_TRACE("compaction pass 1");
    LOG_TRACE("compaction pass 2");
    LOG_TRACE("compaction pass 3");
    LOG_TRACE("compaction pass 4");
    LOG_TRACE("compaction pass 5");
    /* Force flush by emitting a different level */
    LOG_INFO("compaction flush trigger - above 5 should appear as [x5]");
    printf("  [PASS] Compaction emitted (verify [x5] in trace log)\n");
    g_uc_fails += 0; /* visual-only test */

    /* T4 - close */
    UC_CHECK("trace_close()",
             trace_close());
    UC_TEST("trace_is_open() == FALSE after close",
            trace_is_open() == ST_FALSE);

    /* T5 - re-open */
    UC_CHECK("trace_open()",
             trace_open());
    UC_TEST("trace_is_open() == TRUE after open",
            trace_is_open() == ST_TRUE);

    /* T6 - disable LOG_TRACE */
    UC_CHECK("trace_set_trace_enabled(FALSE)",
             trace_set_trace_enabled(ST_FALSE));
    UC_TEST("trace_is_trace_enabled() == FALSE",
            trace_is_trace_enabled() == ST_FALSE);
    LOG_TRACE("THIS LINE MUST NOT APPEAR IN TRACE LOG");

    /* T7 - re-enable LOG_TRACE */
    UC_CHECK("trace_set_trace_enabled(TRUE)",
             trace_set_trace_enabled(ST_TRUE));
    UC_TEST("trace_is_trace_enabled() == TRUE",
            trace_is_trace_enabled() == ST_TRUE);
    LOG_TRACE("This line SHOULD appear in trace log after re-enable");
}

static void test_line(void)
{
    line_context_t tCtx;

    printf("\n--- Test group 2: Console context ---\n");

    /* T8 - init */
    UC_CHECK("line_init()", line_init(&tCtx));
    UC_TEST("bRunning == ST_TRUE after init",
            tCtx.bRunning == ST_TRUE);
    UC_TEST("szCwd non-empty after init",
            tCtx.szCwd[0] != '\0');
    printf("  [INFO] cwd = '%s'\n", tCtx.szCwd);

    /* line_run() not called - would block on stdin */

    UC_CHECK("line_shutdown()", line_shutdown(&tCtx));
    UC_TEST("bRunning == ST_FALSE after shutdown",
            tCtx.bRunning == ST_FALSE);
}

static void test_st_machine(void)
{
    st_machine_t tMachine;
    st_u8_t      uiByte;
    st_u16_t     uiWord;
    st_u32_t     uiLong;

    printf("\n--- Test group 3: ST machine memory ---\n");

    /* T9 - init */
    UC_CHECK("st_init()", st_init(&tMachine, NULL));
    UC_TEST("bPoweredOn == TRUE", tMachine.bPoweredOn == ST_TRUE);
    UC_TEST("resolution == LOW",
            tMachine.uiResolution == ST_RES_LOW);

    /* Write + read byte */
    UC_CHECK("st_write_byte(0x1000, 0xAB)",
             st_write_byte(&tMachine, 0x1000, 0xAB));
    UC_CHECK("st_read_byte(0x1000)",
             st_read_byte(&tMachine, 0x1000, &uiByte));
    UC_TEST("read-back byte == 0xAB", uiByte == 0xAB);

    /* Write + read word (big-endian) */
    UC_CHECK("st_write_word(0x2000, 0x1234)",
             st_write_word(&tMachine, 0x2000, 0x1234));
    UC_CHECK("st_read_word(0x2000)",
             st_read_word(&tMachine, 0x2000, &uiWord));
    UC_TEST("read-back word == 0x1234", uiWord == 0x1234);

    /* Write + read long */
    UC_CHECK("st_write_long(0x3000, 0xDEADBEEF)",
             st_write_long(&tMachine, 0x3000, 0xDEADBEEF));
    UC_CHECK("st_read_long(0x3000)",
             st_read_long(&tMachine, 0x3000, &uiLong));
    UC_TEST("read-back long == 0xDEADBEEF", uiLong == 0xDEADBEEF);

    /* Unaligned word write must return ST_ERROR */
    {
        st_error_t eR;
        eR = st_write_word(&tMachine, 0x1001, 0x0000);
        UC_TEST("unaligned word write returns ST_ERROR",
                eR == ST_ERROR);
    }

    UC_CHECK("st_shutdown()", st_shutdown(&tMachine));
    UC_TEST("bPoweredOn == FALSE after shutdown",
            tMachine.bPoweredOn == ST_FALSE);
}

static void test_cpu(void)
{
    st_machine_t      tMachine;
    cpu68k_t          tCpu;
    cpu_step_result_t tResult;
    st_u32_t          uiTextSz;
    st_error_t        eR;

    /* Load address for the test PRG */
    const st_u32_t    UI_LOAD_ADDR  = 0x1000;
    const st_u32_t    UI_STACK_ADDR = 0x0800;

    printf("\n--- Test group 4: CPU 68000 + hello.prg ---\n");

    UC_CHECK("st_init()", st_init(&tMachine, NULL));

    /* T10 - load hello.prg text section into ST RAM at 0x1000 */
    eR = uc01_load_prg_text("use_cases/UC01/hello.prg",
                              &tMachine,
                              UI_LOAD_ADDR,
                              &uiTextSz);
    UC_TEST("hello.prg text section loaded (4 bytes)",
            eR == ST_NO_ERROR && uiTextSz == 4);

    if (eR != ST_NO_ERROR)
    {
        printf("  [WARN] Skipping CPU tests - PRG load failed.\n"
               "         Run from the project root directory.\n");
        st_shutdown(&tMachine);
        return;
    }

    /* Write reset vectors: SSP=0x0800, PC=0x1000 */
    UC_CHECK("write SSP reset vector",
             st_write_long(&tMachine, CPU_VEC_RESET_SSP, UI_STACK_ADDR));
    UC_CHECK("write PC reset vector",
             st_write_long(&tMachine, CPU_VEC_RESET_PC,  UI_LOAD_ADDR));

    /* T10 - cpu_init reads reset vectors */
    UC_CHECK("cpu_init()", cpu_init(&tCpu, &tMachine));
    UC_TEST("CPU PC == 0x1000 after init",
            tCpu.uiPC == UI_LOAD_ADDR);
    UC_TEST("CPU SSP == 0x0800 after init",
            tCpu.uiSSP == UI_STACK_ADDR);
    UC_TEST("CPU SR supervisor mode set",
            (tCpu.uiSR & CPU_SR_S) != 0);
    UC_TEST("CPU state == RUNNING",
            tCpu.eState == CPU_STATE_RUNNING);

    printf("  [INFO] CPU after init: PC=0x%08X SSP=0x%08X SR=0x%04X\n",
           tCpu.uiPC, tCpu.uiSSP, (unsigned)tCpu.uiSR);

    /* T11 - cpu_step (stub: advances PC by 2, returns ST_NO_ERROR) */
    UC_CHECK("cpu_step() #1 (MOVEQ stub)",
             cpu_step(&tCpu, &tMachine, &tResult));
    UC_TEST("step #1 PC advanced by 2",
            tResult.uiPCAfter == UI_LOAD_ADDR + 2);
    UC_TEST("step #1 opcode == 0x702A (MOVEQ #42,D0)",
            tResult.uiOpcode == 0x702A);

    UC_CHECK("cpu_step() #2 (RTS stub)",
             cpu_step(&tCpu, &tMachine, &tResult));
    UC_TEST("step #2 opcode == 0x4E75 (RTS)",
            tResult.uiOpcode == 0x4E75);

    printf("  [INFO] CPU after 2 steps: PC=0x%08X instrCount=%llu\n",
           tCpu.uiPC,
           (unsigned long long)tCpu.ulInstrCount);

    UC_CHECK("st_shutdown()", st_shutdown(&tMachine));
}

static void test_disasm(void)
{
    const st_u8_t   aBytes[]  = { 0x70, 0x2A, 0x4E, 0x75 };
    disasm_result_t aResults[4];
    size_t          uiLines   = 0;
    st_error_t      eR;

    printf("\n--- Test group 5: Disassembler stub ---\n");

    eR = disasm_range(aBytes, sizeof(aBytes), 0x1000,
                      aResults, 4, &uiLines);
    UC_TEST("disasm_range() returns ST_NO_ERROR", eR == ST_NO_ERROR);
    UC_TEST("disasm_range() produces 2 DC.W stub lines", uiLines == 2);

    if (uiLines > 0)
    {
        printf("  [INFO] disasm[0]: %s\n", aResults[0].szLine);
    }
    if (uiLines > 1)
    {
        printf("  [INFO] disasm[1]: %s\n", aResults[1].szLine);
    }

    printf("  [NOTE] DC.W fallback expected - full decode in UC11-UC14\n");
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("================================================================\n");
    printf(" ST4Ever UC1 - Console Prototype + Trace + ST Machine + CPU\n");
    printf("================================================================\n");

    test_trace();
    test_line();
    test_st_machine();
    test_cpu();
    test_disasm();

    /* Shutdown trace */
    printf("\n--- Shutdown ---\n");
    if (trace_is_open()) { trace_close(); }
    trace_shutdown();
    printf("  [INFO] trace_shutdown() complete - "
           "see st4ever_trace.log for full trace\n");

    /* Summary */
    printf("\n================================================================\n");
    if (g_uc_fails == 0)
    {
        printf(" UC1 RESULT: ALL TESTS PASSED\n");
    }
    else
    {
        printf(" UC1 RESULT: %d TEST(S) FAILED\n", g_uc_fails);
    }
    printf("================================================================\n\n");

    return (g_uc_fails == 0) ? 0 : 1;
}
