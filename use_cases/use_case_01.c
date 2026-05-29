/*
 * use_case_01.c - UC1 Validation: Console prototype + Trace subsystem
 *
 * TEST MATRIX:
 *   [N] Nominal    : 45 tests  - all public functions in trace.h,
 *                                line.h, ST.h, CPU.h, disassemble.h
 *   [R] Robustness : 19 tests  - NULL params, out-of-bounds addresses,
 *                                double init/close, alignment errors
 *   [S] Skipped    :  0 tests  - no display required at UC1 level
 *
 * Test groups:
 *   Group 1: Trace subsystem  (trace_init, log levels, compaction,
 *             open/close, double-init guard, enable/disable)
 *   Group 2: Console context  (line_init NULL guard, nominal init,
 *             shutdown NULL guard, nominal shutdown)
 *   Group 3: ST machine       (st_init, read/write byte/word/long,
 *             alignment errors, bounds, NULL guards, st_shutdown)
 *   Group 4: CPU 68000        (cpu_init NULL guards, nominal init,
 *             cpu_step NULL guards, cpu_step on hello.prg)
 *   Group 5: Disassembler     (disasm_range nominal, zero-length
 *             buffer, NULL param guards)
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

    uiRead = fread(aHeader, 1, 28, pFile);
    if (uiRead != 28)
    {
        fclose(pFile);
        return ST_ERROR;
    }

    if (aHeader[0] != 0x60 || aHeader[1] != 0x1A)
    {
        fprintf(stderr, "  [uc01_load_prg_text] bad magic 0x%02X%02X\n",
                aHeader[0], aHeader[1]);
        fclose(pFile);
        return ST_ERROR;
    }

    uiTextSz = ((st_u32_t)aHeader[2] << 24)
             | ((st_u32_t)aHeader[3] << 16)
             | ((st_u32_t)aHeader[4] <<  8)
             |  (st_u32_t)aHeader[5];

    if (uiLoadAddr + uiTextSz > ST_RAM_SIZE)
    {
        fprintf(stderr,
                "  [uc01_load_prg_text] load overflows RAM "
                "(addr=0x%06X size=%u)\n",
                uiLoadAddr, uiTextSz);
        fclose(pFile);
        return ST_ERROR;
    }

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
 * Group 1: Trace subsystem
 * ------------------------------------------------------------------ */

static void test_trace(void)
{
    printf("\n--- Test group 1: Trace subsystem ---\n");

    /* INTENT: trace_init must succeed and open the console on request */
    UC_CHECK("[N] trace_init(TRUE)",
             trace_init(ST_TRUE));
    UC_TEST("[N] trace_is_open() == TRUE after init(TRUE)",
            trace_is_open() == ST_TRUE);

    /* INTENT: double init must be harmless - logs a warning, no error */
    UC_TEST("[R] trace_init() when already initialised returns ST_NO_ERROR",
            trace_init(ST_TRUE) == ST_NO_ERROR);

    /* INTENT: all four log levels must emit without crashing */
    printf("  [INFO] Emitting one entry per log level:\n");
    LOG_TRACE("UC1 test: LOG_TRACE entry (param=%d)", 42);
    LOG_INFO("UC1 test: LOG_INFO  entry");
    LOG_ERROR("UC1 test: LOG_ERROR entry (not a real error)");
    LOG_TODO("UC1 test: LOG_TODO  entry - this function is a stub");
    printf("  [PASS] [N] all four log levels emitted "
           "(check st4ever_trace.log)\n");

    /* INTENT: consecutive LOG_TRACE from same function collapse to [xN] */
    printf("  [INFO] Compaction test: 5x LOG_TRACE same function...\n");
    LOG_TRACE("compaction pass 1");
    LOG_TRACE("compaction pass 2");
    LOG_TRACE("compaction pass 3");
    LOG_TRACE("compaction pass 4");
    LOG_TRACE("compaction pass 5");
    LOG_INFO("compaction flush trigger - above 5 should appear as [x5]");
    printf("  [PASS] [N] compaction emitted "
           "(verify [x5] in trace log)\n");

    /* INTENT: trace_close must succeed and update the open flag */
    UC_CHECK("[N] trace_close()",
             trace_close());
    UC_TEST("[N] trace_is_open() == FALSE after close",
            trace_is_open() == ST_FALSE);

    /* INTENT: trace_close on an already-closed console must be harmless */
    UC_TEST("[R] trace_close() when already closed returns ST_NO_ERROR",
            trace_close() == ST_NO_ERROR);

    /* INTENT: trace_open must reopen and update the open flag */
    UC_CHECK("[N] trace_open()",
             trace_open());
    UC_TEST("[N] trace_is_open() == TRUE after open",
            trace_is_open() == ST_TRUE);

    /* INTENT: LOG_TRACE must be suppressible without affecting other levels */
    UC_CHECK("[N] trace_set_trace_enabled(FALSE)",
             trace_set_trace_enabled(ST_FALSE));
    UC_TEST("[N] trace_is_trace_enabled() == FALSE",
            trace_is_trace_enabled() == ST_FALSE);
    LOG_TRACE("THIS LINE MUST NOT APPEAR IN TRACE LOG");

    /* INTENT: LOG_TRACE must be re-activatable after suppression */
    UC_CHECK("[N] trace_set_trace_enabled(TRUE)",
             trace_set_trace_enabled(ST_TRUE));
    UC_TEST("[N] trace_is_trace_enabled() == TRUE",
            trace_is_trace_enabled() == ST_TRUE);
    LOG_TRACE("This line SHOULD appear in trace log after re-enable");
}

/* ------------------------------------------------------------------
 * Group 2: Console context
 * ------------------------------------------------------------------ */

static void test_line(void)
{
    line_context_t tCtx;
    st_error_t     eR;

    printf("\n--- Test group 2: Console context ---\n");

    /* INTENT: line_init must reject a NULL context pointer */
    eR = line_init(NULL);
    UC_TEST("[R] line_init(NULL) returns ST_ERROR", eR == ST_ERROR);

    /* INTENT: line_init must populate the context and capture the cwd */
    UC_CHECK("[N] line_init(&tCtx)", line_init(&tCtx));
    UC_TEST("[N] bRunning == TRUE after init",
            tCtx.bRunning == ST_TRUE);
    UC_TEST("[N] szCwd non-empty after init",
            tCtx.szCwd[0] != '\0');
    printf("  [INFO] cwd = '%s'\n", tCtx.szCwd);

    /* line_run() not called - would block on stdin */

    /* INTENT: line_shutdown must reject a NULL context pointer */
    eR = line_shutdown(NULL);
    UC_TEST("[R] line_shutdown(NULL) returns ST_ERROR", eR == ST_ERROR);

    /* INTENT: line_shutdown must clear the context and set bRunning FALSE */
    UC_CHECK("[N] line_shutdown(&tCtx)", line_shutdown(&tCtx));
    UC_TEST("[N] bRunning == FALSE after shutdown",
            tCtx.bRunning == ST_FALSE);
}

/* ------------------------------------------------------------------
 * Group 3: ST machine memory
 * ------------------------------------------------------------------ */

static void test_st_machine(void)
{
    st_machine_t tMachine;
    st_u8_t      uiByte;
    st_u16_t     uiWord;
    st_u32_t     uiLong;
    st_error_t   eR;

    printf("\n--- Test group 3: ST machine memory ---\n");

    /* INTENT: st_init must reject a NULL machine pointer */
    eR = st_init(NULL, NULL);
    UC_TEST("[R] st_init(NULL, NULL) returns ST_ERROR", eR == ST_ERROR);

    /* INTENT: st_init must succeed and power on the machine */
    UC_CHECK("[N] st_init(&tMachine, NULL)", st_init(&tMachine, NULL));
    UC_TEST("[N] bPoweredOn == TRUE", tMachine.bPoweredOn == ST_TRUE);
    UC_TEST("[N] resolution == ST_RES_LOW",
            tMachine.uiResolution == ST_RES_LOW);

    /* INTENT: byte read/write round-trip must be exact */
    UC_CHECK("[N] st_write_byte(0x1000, 0xAB)",
             st_write_byte(&tMachine, 0x1000, 0xAB));
    UC_CHECK("[N] st_read_byte(0x1000)",
             st_read_byte(&tMachine, 0x1000, &uiByte));
    UC_TEST("[N] read-back byte == 0xAB", uiByte == 0xAB);

    /* INTENT: NULL machine pointer must be rejected on read and write */
    eR = st_write_byte(NULL, 0x1000, 0xAB);
    UC_TEST("[R] st_write_byte(NULL, ...) returns ST_ERROR",
            eR == ST_ERROR);
    eR = st_read_byte(NULL, 0x1000, &uiByte);
    UC_TEST("[R] st_read_byte(NULL, ...) returns ST_ERROR",
            eR == ST_ERROR);

    /* INTENT: NULL output pointer must be rejected */
    eR = st_read_byte(&tMachine, 0x1000, NULL);
    UC_TEST("[R] st_read_byte(..., NULL) returns ST_ERROR",
            eR == ST_ERROR);

    /* INTENT: word read/write round-trip must preserve big-endian order */
    UC_CHECK("[N] st_write_word(0x2000, 0x1234)",
             st_write_word(&tMachine, 0x2000, 0x1234));
    UC_CHECK("[N] st_read_word(0x2000)",
             st_read_word(&tMachine, 0x2000, &uiWord));
    UC_TEST("[N] read-back word == 0x1234", uiWord == 0x1234);

    /* INTENT: long read/write round-trip must preserve big-endian order */
    UC_CHECK("[N] st_write_long(0x3000, 0xDEADBEEF)",
             st_write_long(&tMachine, 0x3000, 0xDEADBEEF));
    UC_CHECK("[N] st_read_long(0x3000)",
             st_read_long(&tMachine, 0x3000, &uiLong));
    UC_TEST("[N] read-back long == 0xDEADBEEF", uiLong == 0xDEADBEEF);

    /* INTENT: unaligned word access must raise a bus error */
    eR = st_write_word(&tMachine, 0x1001, 0x0000);
    UC_TEST("[R] unaligned word write (0x1001) returns ST_ERROR",
            eR == ST_ERROR);

    /* INTENT: unaligned long access must raise a bus error */
    eR = st_write_long(&tMachine, 0x1003, 0x00000000);
    UC_TEST("[R] unaligned long write (0x1003) returns ST_ERROR",
            eR == ST_ERROR);

    /*
     * INTENT: address beyond RAM (cartridge/ROM space) must not crash.
     * NOTE: UC1 stub returns ST_NO_ERROR + 0xFF for unmapped addresses
     *       (cartridge space is valid 24-bit addressing on the ST).
     * ADAPTED when UC24 implements real bus errors for unmapped regions.
     */
    eR = st_read_byte(&tMachine, ST_RAM_SIZE, &uiByte);
    UC_TEST("[R] st_read_byte(ST_RAM_SIZE) is safe (stub: ST_NO_ERROR+0xFF)",
            eR == ST_NO_ERROR && uiByte == 0xFF);

    /* INTENT: st_shutdown must reject a NULL pointer */
    eR = st_shutdown(NULL);
    UC_TEST("[R] st_shutdown(NULL) returns ST_ERROR", eR == ST_ERROR);

    /* INTENT: st_shutdown must power off the machine cleanly */
    UC_CHECK("[N] st_shutdown(&tMachine)", st_shutdown(&tMachine));
    UC_TEST("[N] bPoweredOn == FALSE after shutdown",
            tMachine.bPoweredOn == ST_FALSE);
}

/* ------------------------------------------------------------------
 * Group 4: CPU 68000 + hello.prg
 * ------------------------------------------------------------------ */

static void test_cpu(void)
{
    st_machine_t      tMachine;
    cpu68k_t          tCpu;
    cpu_step_result_t tResult;
    st_u32_t          uiTextSz;
    st_error_t        eR;

    const st_u32_t    UI_LOAD_ADDR  = 0x1000;
    const st_u32_t    UI_STACK_ADDR = 0x0800;

    printf("\n--- Test group 4: CPU 68000 + hello.prg ---\n");

    UC_CHECK("[N] st_init()", st_init(&tMachine, NULL));

    /* INTENT: cpu_init must reject NULL pointers before touching state */
    eR = cpu_init(NULL, &tMachine);
    UC_TEST("[R] cpu_init(NULL, &tMachine) returns ST_ERROR",
            eR == ST_ERROR);
    eR = cpu_init(&tCpu, NULL);
    UC_TEST("[R] cpu_init(&tCpu, NULL) returns ST_ERROR",
            eR == ST_ERROR);

    /* INTENT: hello.prg text section must load cleanly into ST RAM */
    eR = uc01_load_prg_text("use_cases/UC01/hello.prg",
                              &tMachine,
                              UI_LOAD_ADDR,
                              &uiTextSz);
    UC_TEST("[N] hello.prg text section loaded (4 bytes)",
            eR == ST_NO_ERROR && uiTextSz == 4);

    if (eR != ST_NO_ERROR)
    {
        printf("  [WARN] Skipping remaining CPU tests - PRG load failed.\n"
               "         Run from the project root directory.\n");
        st_shutdown(&tMachine);
        return;
    }

    UC_CHECK("[N] write SSP reset vector",
             st_write_long(&tMachine, CPU_VEC_RESET_SSP, UI_STACK_ADDR));
    UC_CHECK("[N] write PC reset vector",
             st_write_long(&tMachine, CPU_VEC_RESET_PC,  UI_LOAD_ADDR));

    /* INTENT: cpu_init must read reset vectors and enter supervisor mode */
    UC_CHECK("[N] cpu_init(&tCpu, &tMachine)",
             cpu_init(&tCpu, &tMachine));
    UC_TEST("[N] CPU PC == 0x1000 after init",
            tCpu.uiPC == UI_LOAD_ADDR);
    UC_TEST("[N] CPU SSP == 0x0800 after init",
            tCpu.uiSSP == UI_STACK_ADDR);
    UC_TEST("[N] CPU SR supervisor mode set",
            (tCpu.uiSR & CPU_SR_S) != 0);
    UC_TEST("[N] CPU state == RUNNING",
            tCpu.eState == CPU_STATE_RUNNING);

    printf("  [INFO] CPU after init: PC=0x%08X SSP=0x%08X SR=0x%04X\n",
           tCpu.uiPC, tCpu.uiSSP, (unsigned)tCpu.uiSR);

    /* INTENT: cpu_step must reject NULL ptCpu and NULL ptMachine */
    eR = cpu_step(NULL, &tMachine, &tResult);
    UC_TEST("[R] cpu_step(NULL, &tMachine, ...) returns ST_ERROR",
            eR == ST_ERROR);
    eR = cpu_step(&tCpu, NULL, &tResult);
    UC_TEST("[R] cpu_step(&tCpu, NULL, ...) returns ST_ERROR",
            eR == ST_ERROR);

    /*
     * INTENT: cpu_step must fetch the MOVEQ #42,D0 opcode and advance PC.
     * NOTE: UC1 stub behaviour - PC advances by 2 per step.
     * ADAPTED when UC21 implements real 68000 decode and execution.
     */
    UC_CHECK("[N] cpu_step() #1 (MOVEQ stub)",
             cpu_step(&tCpu, &tMachine, &tResult));
    UC_TEST("[N] step #1 PC advanced by 2",
            tResult.uiPCAfter == UI_LOAD_ADDR + 2);
    UC_TEST("[N] step #1 opcode == 0x702A (MOVEQ #42,D0)",
            tResult.uiOpcode == 0x702A);

    /* INTENT: cpu_step must fetch the RTS opcode on the second step */
    UC_CHECK("[N] cpu_step() #2 (RTS stub)",
             cpu_step(&tCpu, &tMachine, &tResult));
    UC_TEST("[N] step #2 opcode == 0x4E75 (RTS)",
            tResult.uiOpcode == 0x4E75);

    printf("  [INFO] CPU after 2 steps: PC=0x%08X instrCount=%llu\n",
           tCpu.uiPC,
           (unsigned long long)tCpu.ulInstrCount);

    UC_CHECK("[N] st_shutdown()", st_shutdown(&tMachine));
}

/* ------------------------------------------------------------------
 * Group 5: Disassembler stub
 * ------------------------------------------------------------------ */

static void test_disasm(void)
{
    const st_u8_t   aBytes[]  = { 0x70, 0x2A, 0x4E, 0x75 };
    disasm_result_t aResults[4];
    size_t          uiLines   = 0;
    st_error_t      eR;

    printf("\n--- Test group 5: Disassembler stub ---\n");

    /* INTENT: disasm_range must decode 4 bytes into 2 DC.W stub lines */
    eR = disasm_range(aBytes, sizeof(aBytes), 0x1000,
                      aResults, 4, &uiLines);
    UC_TEST("[N] disasm_range() returns ST_NO_ERROR", eR == ST_NO_ERROR);
    UC_TEST("[N] disasm_range() produces 2 DC.W stub lines", uiLines == 2);

    if (uiLines > 0) { printf("  [INFO] disasm[0]: %s\n", aResults[0].szLine); }
    if (uiLines > 1) { printf("  [INFO] disasm[1]: %s\n", aResults[1].szLine); }
    printf("  [NOTE] DC.W fallback expected - full decode in UC11-UC14\n");

    /* INTENT: zero-length buffer must produce 0 lines without error */
    uiLines = 99;
    eR = disasm_range(aBytes, 0, 0x1000, aResults, 4, &uiLines);
    UC_TEST("[N] disasm_range(len=0) returns ST_NO_ERROR",
            eR == ST_NO_ERROR);
    UC_TEST("[N] disasm_range(len=0) writes 0 lines", uiLines == 0);

    /* INTENT: NULL params must be rejected with ST_ERROR */
    eR = disasm_range(NULL, sizeof(aBytes), 0x1000, aResults, 4, &uiLines);
    UC_TEST("[R] disasm_range(NULL buf) returns ST_ERROR",
            eR == ST_ERROR);

    eR = disasm_range(aBytes, sizeof(aBytes), 0x1000, NULL, 4, &uiLines);
    UC_TEST("[R] disasm_range(NULL results) returns ST_ERROR",
            eR == ST_ERROR);

    eR = disasm_range(aBytes, sizeof(aBytes), 0x1000, aResults, 4, NULL);
    UC_TEST("[R] disasm_range(NULL puiLines) returns ST_ERROR",
            eR == ST_ERROR);
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

    printf("\n--- Shutdown ---\n");
    if (trace_is_open()) { trace_close(); }
    trace_shutdown();
    printf("  [INFO] trace_shutdown() complete - "
           "see st4ever_trace.log for full trace\n");

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
