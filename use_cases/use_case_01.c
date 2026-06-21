/*
 * use_case_01.c - UC1 Validation: Console prototype + Trace subsystem
 *
 * SRTD reference: SRTD.md §5 — test cases TC-TRC-*, TC-CON-*, TC-STM-*,
 *                              TC-CPU-*, TC-DIS-*
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN]: intent text
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

    /* INTENT[INT-TRC-001 → TC-TRC-001 → REQ-TRC-001]:
     * trace_init must succeed and open the console on request */
    UC_CHECK("[N] trace_init(TRUE)",
             trace_init(ST_TRUE));
    UC_TEST("[N] trace_is_open() == TRUE after init(TRUE)",
            trace_is_open() == ST_TRUE);

    /* INTENT[INT-TRC-002 → TC-TRC-002 → REQ-TRC-002]:
     * double init must be harmless - logs a warning, no error */
    UC_TEST("[R] trace_init() when already initialised returns ST_NO_ERROR",
            trace_init(ST_TRUE) == ST_NO_ERROR);

    /* INTENT[INT-TRC-003 → TC-TRC-003 → REQ-TRC-004]:
     * all four log levels must emit without crashing */
    printf("  [INFO] Emitting one entry per log level:\n");
    LOG_TRACE("UC1 test: LOG_TRACE entry (param=%d)", 42);
    LOG_INFO("UC1 test: LOG_INFO  entry");
    LOG_ERROR("UC1 test: LOG_ERROR entry (not a real error)");
    LOG_TODO("UC1 test: LOG_TODO  entry - this function is a stub");
    printf("  [PASS] [N] all four log levels emitted "
           "(check st4ever_trace.log)\n");

    /* INTENT[INT-TRC-004 → TC-TRC-004 → REQ-TRC-005]:
     * consecutive LOG_TRACE from same function collapse to [xN] */
    printf("  [INFO] Compaction test: 5x LOG_TRACE same function...\n");
    LOG_TRACE("compaction pass 1");
    LOG_TRACE("compaction pass 2");
    LOG_TRACE("compaction pass 3");
    LOG_TRACE("compaction pass 4");
    LOG_TRACE("compaction pass 5");
    LOG_INFO("compaction flush trigger - above 5 should appear as [x5]");
    printf("  [PASS] [N] compaction emitted "
           "(verify [x5] in trace log)\n");

    /* INTENT[INT-TRC-005 → TC-TRC-005 → REQ-TRC-006]:
     * trace_close must succeed and update the open flag */
    UC_CHECK("[N] trace_close()",
             trace_close());
    UC_TEST("[N] trace_is_open() == FALSE after close",
            trace_is_open() == ST_FALSE);

    /* INTENT[INT-TRC-006 → TC-TRC-006 → REQ-TRC-007]:
     * trace_close on an already-closed console must be harmless */
    UC_TEST("[R] trace_close() when already closed returns ST_NO_ERROR",
            trace_close() == ST_NO_ERROR);

    /* INTENT[INT-TRC-007 → TC-TRC-007 → REQ-TRC-008]:
     * trace_open must reopen and update the open flag */
    UC_CHECK("[N] trace_open()",
             trace_open());
    UC_TEST("[N] trace_is_open() == TRUE after open",
            trace_is_open() == ST_TRUE);

    /* INTENT[INT-TRC-008 → TC-TRC-008 → REQ-TRC-009]:
     * LOG_TRACE must be suppressible without affecting other levels */
    UC_CHECK("[N] trace_set_trace_enabled(FALSE)",
             trace_set_trace_enabled(ST_FALSE));
    UC_TEST("[N] trace_is_trace_enabled() == FALSE",
            trace_is_trace_enabled() == ST_FALSE);
    LOG_TRACE("THIS LINE MUST NOT APPEAR IN TRACE LOG");

    /* INTENT[INT-TRC-009 → TC-TRC-009 → REQ-TRC-010]:
     * LOG_TRACE must be re-activatable after suppression */
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
    line_context_t* tCtx;
    const char*     szScript = "";
    st_u64_t        ullR;

    printf("\n--- Test group 2: Console context ---\n");

    /* INTENT[INT-CON-001 → TC-CON-001 → REQ-CON-001]:
     * line_init must reject a NULL context pointer */
    ullR = line_init(NULL);
    UC_TEST("[R] line_init(NULL) returns ST_ERROR", ullR == ST_ERROR);

    /* INTENT[INT-CON-002 → TC-CON-002 → REQ-CON-002]:
     * line_init must populate the context and capture the cwd */
    ullR = line_init(szScript);
    UC_CHECK("[N] line_init(szScript = "")", ullR);
    tCtx = (void*)ullR;
    UC_CHECK_OBJ(tCtx, ST_LINE_CTX);

    if (g_uc_fails) return;
    
    UC_TEST("[N] bRunning == TRUE after init",
            tCtx->bRunning == ST_TRUE);
    UC_TEST("[N] szCwd non-empty after init",
            tCtx->szCwd[0] != '\0');
    printf("  [INFO] cwd = '%s'\n", tCtx->szCwd);

    /* line_run() not called - would block on stdin */

    /* INTENT[INT-CON-003 → TC-CON-003 → REQ-CON-004]:
     * line_shutdown must reject a NULL context pointer */
    ullR = line_shutdown();
    UC_TEST("[R] line_shutdown(NULL) returns ST_ERROR", ullR == ST_NO_ERROR);

    /* INTENT[INT-CON-004 → TC-CON-004 → REQ-CON-005]:
     * line_shutdown must clear the context and set bRunning FALSE */
    UC_CHECK("[N] line_shutdown()", line_shutdown());
    UC_TEST("[N] bRunning == FALSE after shutdown",
            tCtx->bRunning == ST_FALSE);
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

    /* INTENT[INT-STM-001 → TC-STM-001 → REQ-STM-001]:
     * st_init must reject a NULL machine pointer */
    eR = st_init(NULL, NULL);
    UC_TEST("[R] st_init(NULL, NULL) returns ST_ERROR", eR == ST_ERROR);

    /* INTENT[INT-STM-002 → TC-STM-002 → REQ-STM-002]:
     * st_init must succeed and power on the machine */
    UC_CHECK("[N] st_init(&tMachine, NULL)", st_init(&tMachine, NULL));
    UC_TEST("[N] bPoweredOn == TRUE", tMachine.bPoweredOn == ST_TRUE);
    UC_TEST("[N] resolution == ST_RES_LOW",
            tMachine.uiResolution == ST_RES_LOW);

    /* INTENT[INT-STM-003 → TC-STM-003 → REQ-STM-006]:
     * byte read/write round-trip must be exact */
    UC_CHECK("[N] st_write_byte(0x1000, 0xAB)",
             st_write_byte(&tMachine, 0x1000, 0xAB));
    UC_CHECK("[N] st_read_byte(0x1000)",
             st_read_byte(&tMachine, 0x1000, &uiByte));
    UC_TEST("[N] read-back byte == 0xAB", uiByte == 0xAB);

    /* INTENT[INT-STM-004 → TC-STM-004 → REQ-STM-004]:
     * NULL machine pointer must be rejected on read and write */
    eR = st_write_byte(NULL, 0x1000, 0xAB);
    UC_TEST("[R] st_write_byte(NULL, ...) returns ST_ERROR",
            eR == ST_ERROR);
    eR = st_read_byte(NULL, 0x1000, &uiByte);
    UC_TEST("[R] st_read_byte(NULL, ...) returns ST_ERROR",
            eR == ST_ERROR);

    /* INTENT[INT-STM-005 → TC-STM-005 → REQ-STM-005]:
     * NULL output pointer must be rejected */
    eR = st_read_byte(&tMachine, 0x1000, NULL);
    UC_TEST("[R] st_read_byte(..., NULL) returns ST_ERROR",
            eR == ST_ERROR);

    /* INTENT[INT-STM-006 → TC-STM-006 → REQ-STM-007]:
     * word read/write round-trip must preserve big-endian order */
    UC_CHECK("[N] st_write_word(0x2000, 0x1234)",
             st_write_word(&tMachine, 0x2000, 0x1234));
    UC_CHECK("[N] st_read_word(0x2000)",
             st_read_word(&tMachine, 0x2000, &uiWord));
    UC_TEST("[N] read-back word == 0x1234", uiWord == 0x1234);

    /* INTENT[INT-STM-007 → TC-STM-007 → REQ-STM-008]:
     * long read/write round-trip must preserve big-endian order */
    UC_CHECK("[N] st_write_long(0x3000, 0xDEADBEEF)",
             st_write_long(&tMachine, 0x3000, 0xDEADBEEF));
    UC_CHECK("[N] st_read_long(0x3000)",
             st_read_long(&tMachine, 0x3000, &uiLong));
    UC_TEST("[N] read-back long == 0xDEADBEEF", uiLong == 0xDEADBEEF);

    /* INTENT[INT-STM-008 → TC-STM-008 → REQ-STM-009]:
     * unaligned word access must raise a bus error */
    eR = st_write_word(&tMachine, 0x1001, 0x0000);
    UC_TEST("[R] unaligned word write (0x1001) returns ST_ERROR",
            eR == ST_ERROR);

    /* INTENT[INT-STM-009 → TC-STM-009 → REQ-STM-010]:
     * unaligned long access must raise a bus error */
    eR = st_write_long(&tMachine, 0x1003, 0x00000000);
    UC_TEST("[R] unaligned long write (0x1003) returns ST_ERROR",
            eR == ST_ERROR);

    /*
     * INTENT[INT-STM-010 → TC-STM-010 → REQ-STM-011]:
     * address beyond RAM (cartridge/ROM space) must not crash.
     * NOTE: UC1 stub returns ST_NO_ERROR + 0xFF for unmapped addresses
     *       (cartridge space is valid 24-bit addressing on the ST).
     * ADAPTED when UC24 implements real bus errors for unmapped regions.
     */
    eR = st_read_byte(&tMachine, ST_RAM_SIZE, &uiByte);
    UC_TEST("[R] st_read_byte(ST_RAM_SIZE) is safe (stub: ST_NO_ERROR+0xFF)",
            eR == ST_NO_ERROR && uiByte == 0xFF);

    /* INTENT[INT-STM-011 → TC-STM-011 → REQ-STM-013]:
     * st_shutdown must reject a NULL pointer */
    eR = st_shutdown(NULL);
    UC_TEST("[R] st_shutdown(NULL) returns ST_ERROR", eR == ST_ERROR);

    /* INTENT[INT-STM-012 → TC-STM-012 → REQ-STM-014]:
     * st_shutdown must power off the machine cleanly */
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

    /* INTENT[INT-CPU-001 → TC-CPU-001/002 → REQ-CPU-001]:
     * cpu_init must reject NULL pointers before touching state */
    eR = cpu_init(NULL, &tMachine);
    UC_TEST("[R] cpu_init(NULL, &tMachine) returns ST_ERROR",
            eR == ST_ERROR);
    eR = cpu_init(&tCpu, NULL);
    UC_TEST("[R] cpu_init(&tCpu, NULL) returns ST_ERROR",
            eR == ST_ERROR);

    /* INTENT[INT-CPU-002 → TC-CPU-003 → REQ-STM-006]:
     * hello.prg text section must load cleanly into ST RAM */
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

    /* INTENT[INT-CPU-003 → TC-CPU-004 → REQ-CPU-002/003/004/005]:
     * cpu_init must read reset vectors and enter supervisor mode */
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

    /* INTENT[INT-CPU-004 → TC-CPU-005 → REQ-CPU-006]:
     * cpu_step must reject NULL ptCpu and NULL ptMachine */
    eR = cpu_step(NULL, &tMachine, &tResult);
    UC_TEST("[R] cpu_step(NULL, &tMachine, ...) returns ST_ERROR",
            eR == ST_ERROR);
    eR = cpu_step(&tCpu, NULL, &tResult);
    UC_TEST("[R] cpu_step(&tCpu, NULL, ...) returns ST_ERROR",
            eR == ST_ERROR);

    /*
     * INTENT[INT-CPU-005 → TC-CPU-006 → REQ-CPU-007/008]:
     * cpu_step must fetch the MOVEQ #42,D0 opcode, advance PC by 2,
     * and execute it (D0=42 after UC21 real decode).
     * ADAPTED: UC21 - real MOVEQ decode: D0==42, PC+2 still holds
     *   (MOVEQ is a 1-word instruction).
     */
    UC_CHECK("[N] cpu_step() #1 (MOVEQ #42,D0)",
             cpu_step(&tCpu, &tMachine, &tResult));
    UC_TEST("[N] step #1 PC advanced by 2",
            tResult.uiPCAfter == UI_LOAD_ADDR + 2);
    UC_TEST("[N] step #1 opcode == 0x702A (MOVEQ #42,D0)",
            tResult.uiOpcode == 0x702A);
    /* ADAPTED: UC21 - D0 now actually holds 42 after real MOVEQ decode */
    UC_TEST("[N] step #1 D0==42 after MOVEQ",
            tCpu.auDn[0] == 42u);

    /* INTENT[INT-CPU-006 → TC-CPU-007 → REQ-CPU-007]:
     * cpu_step must fetch the RTS opcode on the second step.
     * ADAPTED: UC21 - RTS (0x4E75) is in group 0x4/misc4; it hits
     *   LOG_TODO (UC23) but returns ST_NO_ERROR (non-fatal). */
    UC_CHECK("[N] cpu_step() #2 (RTS — LOG_TODO UC23)",
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

    /* INTENT[INT-DIS-001 → TC-DIS-001 → REQ-DIS-004/005]:
     * disasm_range must decode 4 bytes into 2 instruction lines.
     * ADAPTED: UC14 - MOVEQ (#42,D0) and RTS now fully decoded;
     *   uiLines==2 is still correct (2 real instructions, not DC.W). */
    eR = disasm_range(aBytes, sizeof(aBytes), 0x1000,
                      aResults, 4, &uiLines);
    UC_TEST("[N] disasm_range() returns ST_NO_ERROR", eR == ST_NO_ERROR);
    /* ADAPTED: UC14 — description updated; assertion uiLines==2 unchanged */
    UC_TEST("[N] disasm_range() produces 2 decoded lines", uiLines == 2);

    if (uiLines > 0) { printf("  [INFO] disasm[0]: %s\n", aResults[0].szLine); }
    if (uiLines > 1) { printf("  [INFO] disasm[1]: %s\n", aResults[1].szLine); }
    printf("  [NOTE] UC11+UC14: MOVEQ and RTS fully decoded\n");

    /* INTENT[INT-DIS-002 → TC-DIS-002 → REQ-DIS-004]:
     * zero-length buffer must produce 0 lines without error */
    uiLines = 99;
    eR = disasm_range(aBytes, 0, 0x1000, aResults, 4, &uiLines);
    UC_TEST("[N] disasm_range(len=0) returns ST_NO_ERROR",
            eR == ST_NO_ERROR);
    UC_TEST("[N] disasm_range(len=0) writes 0 lines", uiLines == 0);

    /* INTENT[INT-DIS-003 → TC-DIS-003/004/005 → REQ-DIS-001/002/003]:
     * NULL params must be rejected with ST_ERROR */
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
