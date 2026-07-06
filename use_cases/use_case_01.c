/*
 * use_case_01.c - UC1 Validation: Console prototype + Trace subsystem
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * TEST MATRIX:
 *   [N] Nominal    : 50 tests  - See test groups description below
 *   [R] Robustness : 20 tests  - NULL params, out-of-bounds addresses,
 *                                double init/close, alignment errors
 *   [S] Skipped    :  0 tests  - no display required at UC1 level
 *
 * Test groups:
 *   Group 1: Trace subsystem  (nominal init, log levels, compaction,
 *                              GUI open/close)
 *   Group 2: ST machine       (nominal init, read/write byte/word/long,
 *             alignment errors, bounds, NULL guards)
 *   Group 3: CPU 68000        (cpu_init NULL/power-off guards, nominal
 *             init from hello.prg's reset vectors, cpu_step NULL guard,
 *             MOVEQ + RTS step-by-step execution, optional ptResult)
 *   Group 4: Disassembler     (disasm_range nominal, zero-length
 *             buffer, NULL param guards)
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"

/* Required by the UC_TEST / UC_CHECK macros in use_cases.h */
int        g_uc_fails = 0;
st_bool_t  gIsObject  = ST_FALSE;

static ST4Ever_context_t* ptCtx;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

/*
 * uc01_check_log_entry() - Verify a tag and message appear on the same
 *                          line in TRACE_LOGFILE.
 *
 * trace_log() calls fflush() after every write, so the file is readable
 * immediately after any LOG_xxx macro returns — no extra flush needed.
 *
 * Parameters:
 *   szTag     [in] : Tag string to find, e.g. "[TRC ]", "[INF ]".
 *   szContent [in] : Message fragment that must appear on the same line.
 *
 * Returns:
 *   ST_TRUE  if at least one line contains both szTag and szContent.
 *   ST_FALSE otherwise (file missing, tag absent, or content absent).
 */
static st_bool_t uc01_check_log_entry(const char *szTag,
                                       const char *szContent)
{
    FILE     *pFile;
    char      szLine[TRACE_VIEW_LINE_LEN];
    st_bool_t bFound;

    if (szTag == NULL || szContent == NULL)
    {
        return ST_FALSE;
    }

    bFound = ST_FALSE;
    pFile  = fopen(TRACE_LOGFILE, "r");
    if (pFile == NULL)
    {
        return ST_FALSE;
    }

    while (fgets(szLine, (int)sizeof(szLine), pFile) != NULL)
    {
        if (strstr(szLine, szTag)     != NULL
        &&  strstr(szLine, szContent) != NULL)
        {
            bFound = ST_TRUE;
            break;
        }
    }

    fclose(pFile);
    return bFound;
}

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
                                      st_u32_t      uiLoadAddr,
                                      st_u32_t     *puiTextSz)
{
    FILE     *pFile;
    st_u8_t   aHeader[28];
    st_u32_t  uiTextSz;
    size_t    uiRead;

    if (szPath == NULL || puiTextSz == NULL)
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

    uiRead = fread(st_get_ram_pointer(uiLoadAddr), 1, uiTextSz, pFile);
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

/*
 * uc01_test_trace() - Log levels / Trace collapse / Trace on/off
 *                     Trace open/close
 *
 * Code Coverage:
 *   trace.c:
 *   -- [TRACE]5. Trace Context must be initialized before logging -- 
 *   -- [TRACE]6. LOG_TRACE is compacted when called consecutively -- 
 *   -- [TRACE]7. Format logging messages --
 *   -- [TRACE]8. Log in file --
 *   -- [TRACE]11. GUI must not be open when trace module is not initialized --
 *   -- [TRACE]12. Calling trace_gui_open() when GUI is open is harmless --
 *   -- [TRACE]13. Allocate a new trace_view structure --
 *   -- [TRACE]14. Open the GUI window --
 *   -- [TRACE]15. Do not close if trace module is not initialized -- 
 *   -- [TRACE]16. Close the GUI window --
 * 
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc01_test_trace(void)
{
    trace_context_t *ptTrcCtx;

    printf("\n--- Test group 1: Trace subsystem ---\n");

    /* -- [TRACE]7. Format logging messages -- */
    /* -- [TRACE]8. Log in file -- */
    /* INTENT[INT-TRC-003 → TC-TRC-006...009 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * all four log levels must write their tag and message to TRACE_LOGFILE */
    UC_INFO("(INT-TRC-003) Log TRC, INF, ERR and TODO messages");
    LOG_TRACE("UC1 test: LOG_TRACE entry (param=%d)", 42);
    LOG_INFO("UC1 test: LOG_INFO  entry");
    LOG_ERROR("UC1 test: LOG_ERROR entry (not a real error)");
    LOG_TODO("UC1 test: LOG_TODO  entry - this function is a stub");
    UC_TEST("[N] (TC-TRC-006) LOG_TRACE emits [TRC ] tag in trace log",
            uc01_check_log_entry("[TRC ]", "LOG_TRACE entry"));
    UC_TEST("[N] (TC-TRC-007) LOG_INFO emits [INF ] tag in trace log",
            uc01_check_log_entry("[INF ]", "LOG_INFO  entry"));
    UC_TEST("[N] (TC-TRC-008) LOG_ERROR emits [ERR ] tag in trace log",
            uc01_check_log_entry("[ERR ]", "LOG_ERROR entry"));
    UC_TEST("[N] (TC-TRC-009) LOG_TODO emits [TODO] tag in trace log",
            uc01_check_log_entry("[TODO]", "LOG_TODO  entry"));

    /* -- [TRACE]5. Trace Context must be initialized before logging -- */
    /* INTENT[INT-TRC-004 → TC-TRC-010 → REQ-TRC-001 → UFR-xxx-yyy]:
     * trace_log() must silently return without writing anything when
     * bInitialised is FALSE  */
    UC_INFO("(INT-TRC-004) Do not log if trace module is not initialized");
    ptTrcCtx = (trace_context_t *)ptCtx->ptTraceCtx;
    ptTrcCtx->bInitialised = ST_FALSE;
    LOG_INFO("TRC6_INIT_GUARD");
    UC_TEST("[R] (TC-TRC-010) trace_log() skips log write when bInitialised is FALSE",
            !uc01_check_log_entry("[INF ]", "TRC6_INIT_GUARD"));
    ptTrcCtx->bInitialised = ST_TRUE;

    /* -- [TRACE]6. LOG_TRACE is compacted when called consecutively -- */
    /* INTENT[INT-TRC-005 → TC-TRC-011..012 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * 5 consecutive LOG_TRACE from the same function collapse to a single
     * [xN] compaction summary in the log; intermediate calls (passes 2-5)
     * are not individually written to the log file */
    UC_INFO("(INT-TRC-005) Compaction: 5x LOG_TRACE from same function");
    ptTrcCtx->szLastFunc[0] = '\0';   /* reset compaction state for clean baseline */
    ptTrcCtx->iCompactCount = 0;
    LOG_TRACE("TRC7_COMPACT_P1");
    LOG_TRACE("TRC7_COMPACT_P2");
    LOG_TRACE("TRC7_COMPACT_P3");
    LOG_TRACE("TRC7_COMPACT_P4");
    LOG_TRACE("TRC7_COMPACT_P5");
    LOG_INFO("TRC7_COMPACT_FLUSH");
    UC_TEST("[N] (TC-TRC-011) 5 consecutive LOG_TRACE compact to [x5] in log",
            uc01_check_log_entry("[TRC ]", "[x5]"));
    UC_TEST("[N] (TC-TRC-012) intermediate LOG_TRACE calls not written individually",
            !uc01_check_log_entry("[TRC ]", "TRC7_COMPACT_P2"));

    /* -- [TRACE]13. Allocate a new trace_view structure -- */
    /* -- [TRACE]14. Open the GUI window -- */
    /* -- [TRACE]12. Calling trace_gui_open() when GUI is open is harmless -- */
    /* -- [TRACE]11. GUI must not be open when trace module is not initialized -- */
    /* INTENT[INT-TRC-006 → TC-TRC-013...018 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * trace_gui_open initialise the trace_view_t context and open the GUI */
    UC_TEST("(INT-TRC-006) [Chk] ptView is NULL (GUI not created)", 
                ptTrcCtx->ptView == NULL);

    ptTrcCtx->bInitialised = ST_FALSE;   /* Forcing trace module to "not initialized" */
    ptTrcCtx->bOpen = ST_FALSE;          /* Force bOpen as sentinel */
    st_error_t result = trace_gui_open();   /* Function returns straight ST_ERROR */
    UC_TEST("[N] (TC-TRC-013) ptView is not set ", ptTrcCtx->ptView == NULL);
    UC_TEST("[R] (TC-TRC-014) bOpen is unchanged ", ptTrcCtx->bOpen == ST_FALSE);
    UC_TEST("[N] (TC-TRC-015) Trace_gui_open() returns ST_ERROR ", result == ST_ERROR);
    
    ptTrcCtx->bInitialised = ST_TRUE;   /* Forcing trace module to "initialized" */
    
    UC_CHECK("(INT-TRC-006) [Chk] trace_gui_open()", trace_gui_open());
    UC_TEST("[N] (TC-TRC-016) bOpen is set to ST_TRUE", ptTrcCtx->bOpen == ST_TRUE);
    UC_TEST("[N] (TC-TRC-017) ptView is set ", ptTrcCtx->ptView != NULL);
    trace_view_t *p = ptTrcCtx->ptView;
    UC_CHECK("(INT-TRC-006) [Chk] Re-launch trace_gui_open()", trace_gui_open());
    UC_TEST("[N] (TC-TRC-018) ptView is unchanged ", ptTrcCtx->ptView == p);
    
    
    /* -- [TRACE]15. Do not close if trace module is not initialized -- */
    /* -- [TRACE]16. Close the GUI window -- */
    /* INTENT[INT-TRC-007 → TC-TRC-019...023 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * trace_gui_close must succeed and update the open flag, if trace module
     * is initialized */
    ptTrcCtx->bInitialised = ST_FALSE;   /* Forcing trace module to "not initialized" */
    UC_CHECK("(INT-TRC-007) [Chk] trace_gui_close()", trace_gui_close());
    UC_TEST("[R] (TC-TRC-019) bOpen is unchanged ", ptTrcCtx->bOpen == ST_TRUE);
    ptTrcCtx->bInitialised = ST_TRUE;   /* Forcing trace module to "initialized" */
    UC_CHECK("(INT-TRC-007) [Chk] trace_gui_close()", trace_gui_close());
    UC_TEST("[N] (TC-TRC-020) bOpen is set ", ptTrcCtx->bOpen == ST_FALSE);
    UC_TEST("[N] (TC-TRC-021) ptView is set ", ptTrcCtx->ptView == NULL);
    UC_CHECK("(INT-TRC-007) [Chk] Re-launch trace_gui_close()", trace_gui_close());
    UC_TEST("[N] (TC-TRC-022) bOpen is unchanged ", ptTrcCtx->bOpen == ST_FALSE);
    UC_TEST("[N] (TC-TRC-023) ptView is unchanged ", ptTrcCtx->ptView == NULL);
}

/* ------------------------------------------------------------------
 * Group 2: ST machine memory
 * ------------------------------------------------------------------ */

/*
 * uc01_test_st_machine() - Read/Write memory
 *
 * Code Coverage:
 *   ST.c:
 *  -- [ST]4. Reject any NULL parameter with an error --
 *  -- [ST]5. Returns 0xFF if machine is not powered on --
 *  -- [ST]6. If address is in RAM, return the RAM Value --
 *  -- [ST]12. If address is in unknown area, returns 0xFF --
 *  -- [ST]13. Reject any NULL parameter with an error --
 *  -- [ST]14. Reject unaligned address with an error --
 *  -- [ST]15. Read word, based on read bytes function --
 *  -- [ST]16. Reject any NULL parameter with an error --
 *  -- [ST]17. Read long, based on read word function --
 *  -- [ST]18. If address is in RAM, write the RAM Value --
 *  -- [ST]19. If address is in ROM, reject with an error --
 *  -- [ST]24. Log an error, if address is in unknown area --
 *  -- [ST]25. Reject unaligned address with an error --
 *  -- [ST]26. Write word, based on write byte function --
 *  -- [ST]27. Write long, based on write word function --
 *  -- [ST]28. Do not write in memory if machine is not powered on --
 *  -- [ST]29. Reject the write if any of the 4 bytes is not writable,
 *             before touching any word (atomic long write) --
 *
 * Parameters:
 *   None
 *
 * Returns: 
 *   Void
 */
static void uc01_test_st_machine(void)
{
    st_u8_t      uiByte;
    st_u16_t     uiWord;
    st_u32_t     uiLong;
    
    st_machine_context_t *ptSTCtx = (st_machine_context_t*)ptCtx->ptSTMachineCtx;
    st_error_t   result;

    printf("\n--- Test group 2: ST machine memory ---\n");

    /* -- [ST]4. Reject any NULL parameter with an error -- */
    /* -- [ST]5. Returns 0xFF if machine is not powered on -- */
    /* -- [ST]6. If address is in RAM, return the RAM Value -- */
    /* -- [ST]12. If address is in unknown area, returns 0xFF -- */
    /* INTENT[INT-STM-003 → TC-STM-005...010 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Attempt reading bytes from RAM area and out of bound area (Bus Error) */
    UC_TEST("[Chk] Check that ST Machine is ON", ptSTCtx->bPoweredOn == ST_TRUE);
    UC_INFO("(INT-STM-003) Reading bytes in RAM & out of bound");
    result = st_read_byte(0x1000, NULL);   
    UC_TEST("[R] (TC-STM-005) st_read_byte(0x1000, NULL) returns ST_ERROR ", result == ST_ERROR);
    ptSTCtx->bPoweredOn = ST_FALSE;
    result = st_read_byte(0x1000, &uiByte);   
    UC_TEST("[R] (TC-STM-006) Reading a byte with ST machine off returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[R] (TC-STM-007) Read byte is 0xFF ", uiByte == 0xFFu);
    ptSTCtx->bPoweredOn = ST_TRUE;
    UC_CHECK("(INT-STM-003) [Chk] st_read_byte(0x1000)",
             st_read_byte(0x1000, &uiByte));
    UC_TEST("[N] (TC-STM-008) Read byte @0x1000 is 0x00 ", uiByte == 0x00u);
    UC_CHECK("(INT-STM-003) [Chk] st_read_byte(0x0000)",
             st_read_byte(0x0000, &uiByte));
    UC_TEST("[N] (TC-STM-009) Read byte @0x0000 is 0x00 ", uiByte == 0x00u);
    result = st_read_byte(0xFFFCDE, &uiByte);
    UC_TEST("[N] (TC-STM-010) Reading a byte out of bound returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[N] (TC-STM-011) Read byte @0xFFFCDE is 0xFF ", uiByte == 0xFFu);
    UC_CHECK("(INT-STM-003) [Chk] Read last RAM byte",
             st_read_byte(ST_RAM_SIZE - 1, &uiByte));
    UC_TEST("[R] (TC-STM-012) Last byte of RAM is 0x00 ", uiByte == 0x00u);
        

    /* -- [ST]13. Reject any NULL parameter with an error -- */
    /* -- [ST]14. Reject unaligned address with an error -- */
    /* -- [ST]15. Read word, based on read bytes function -- */
    /* INTENT[INT-STM-004 → TC-STM-013...023 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Attempt reading words from RAM area and out of bound area (Bus Error) */
    UC_INFO("(INT-STM-004) Reading words in RAM & out of bound");
    result = st_read_word(0x1000, NULL);   
    UC_TEST("[R] (TC-STM-013) st_read_word(0x1000, NULL) returns ST_ERROR ", result == ST_ERROR);
    ptSTCtx->bPoweredOn = ST_FALSE;
    result = st_read_word(0x1000, &uiWord);   
    UC_TEST("[R] (TC-STM-014) Reading a word with ST machine off returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[R] (TC-STM-015) Read word is 0xFFFF ", uiWord == 0xFFFFu);
    ptSTCtx->bPoweredOn = ST_TRUE;
    UC_CHECK("(INT-STM-004) [Chk] st_read_word(0x1000)",
             st_read_word(0x1000, &uiWord));
    UC_TEST("[N] (TC-STM-016) Read word @0x1000 is 0x0000 ", uiWord == 0x0000u);
    UC_CHECK("(INT-STM-004) [Chk] st_read_word(0x0000)",
             st_read_word(0x0000, &uiWord));
    UC_TEST("[N] (TC-STM-017) Read word @0x0000 is 0x0000 ", uiWord == 0x0000u);
    result = st_read_word(0xFFFCDE, &uiWord);
    UC_TEST("[N] (TC-STM-018) Reading a word out of bound returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[N] (TC-STM-019) Read word @0xFFFCDE is 0xFFFF ", uiWord == 0xFFFFu);
    result = st_read_word(0x0001, &uiWord);   
    UC_TEST("[R] (TC-STM-020) Reading word at unaligned address returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[R] (TC-STM-021) Read word is 0xFFFF ", uiWord == 0xFFFFu);
    UC_CHECK("(INT-STM-004) [Chk] Read last RAM word",
             st_read_word(ST_RAM_SIZE - 2, &uiWord));
    UC_TEST("[R] (TC-STM-022) Last word of RAM is 0x00 ", uiWord == 0x0000u);
    result = st_read_word(ST_RAM_SIZE - 1, &uiWord);
    UC_TEST("[R] (TC-STM-023) Reading word at last byte of RAM returns ST_ERROR ", result == ST_ERROR);
        
    /* -- [ST]16. Reject any NULL parameter with an error -- */
    /* -- [ST]17. Read long, based on read word function -- */
    /* INTENT[INT-STM-005 → TC-STM-024...035 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Attempt reading longs from RAM area and out of bound area (Bus Error) */
    UC_INFO("(INT-STM-005) Reading longs in RAM & out of bound");
    result = st_read_long(0x1000, NULL);   
    UC_TEST("[R] (TC-STM-024) st_read_long(0x1000, NULL) returns ST_ERROR ", result == ST_ERROR);
    ptSTCtx->bPoweredOn = ST_FALSE;
    result = st_read_long(0x1000, &uiLong);   
    UC_TEST("[R] (TC-STM-025) Reading a long with ST machine off returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[R] (TC-STM-026) Read long is 0xFFFF ", uiLong == 0xFFFFFFFFu);
    ptSTCtx->bPoweredOn = ST_TRUE;
    UC_CHECK("(INT-STM-005) [Chk] st_read_long(0x1000)",
             st_read_long(0x1000, &uiLong));
    UC_TEST("[N] (TC-STM-027) Read long @0x1000 is 0x0000 ", uiLong == 0x00000000u);
    UC_CHECK("(INT-STM-005) [Chk] st_read_long(0x0000)",
             st_read_long(0x0000, &uiLong));
    UC_TEST("[N] (TC-STM-028) Read long @0x0000 is 0x0000 ", uiLong == 0x00000000u);
    result = st_read_long(0xFFFCDE, &uiLong);
    UC_TEST("[N] (TC-STM-029) Reading a long out of bound returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[N] (TC-STM-030) Read long @0xFFFCDE is 0xFFFF ", uiLong == 0xFFFFFFFFu);
    result = st_read_long(0x0001, &uiLong);   
    UC_TEST("[R] (TC-STM-031) Reading long at unaligned address returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[R] (TC-STM-032) Read long is 0xFFFF ", uiLong == 0xFFFFFFFFu);
    UC_CHECK("(INT-STM-005) [Chk] Read last RAM long",
             st_read_long(ST_RAM_SIZE - 4, &uiLong));
    UC_TEST("[R] (TC-STM-033) Last long of RAM is 0x00 ", uiLong == 0x00000000u);
    result = st_read_long(ST_RAM_SIZE - 2, &uiLong);
    UC_TEST("[R] (TC-STM-034) Reading long at last word of RAM returns ST_ERROR ", result == ST_ERROR);
    UC_TEST("[N] (TC-STM-035) Read long is 0xFFFFFFFF ", uiLong == 0xFFFFFFFFu);
    result = st_read_long(ST_RAM_SIZE - 1, &uiLong);
    UC_TEST("[R] (TC-STM-036) Reading long at last byte of RAM returns ST_ERROR ", result == ST_ERROR);


    /* -- [ST]28. Do not write in memory if machine is not powered on -- */
    /* -- [ST]18. If address is in RAM, write the RAM Value -- */
    /* -- [ST]19. If address is in ROM, reject with an error -- */
    /* -- [ST]24. Log an error, if address is in unknown area-- */
    /* INTENT[INT-STM-006 → TC-STM-037...041 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Attempt writing bytes to RAM area, ROM area (read-only) and
     * unmapped area (silently ignored) */
    UC_INFO("(INT-STM-006) Writing bytes in RAM, ROM & unmapped area");
    ptSTCtx->bPoweredOn = ST_FALSE;
    result = st_write_byte(0x1000, 0xABu);
    UC_TEST("[R] (TC-STM-037) Reading a byte with ST machine off returns ST_ERROR ", result == ST_ERROR);
    ptSTCtx->bPoweredOn = ST_TRUE;
    UC_CHECK("(INT-STM-006) [Chk] st_write_byte(0x1000, 0xAB)",
             st_write_byte(0x1000, 0xABu));
    UC_CHECK("(INT-STM-006) [Chk] Read back written byte",
             st_read_byte(0x1000, &uiByte));
    UC_TEST("[N] (TC-STM-038) Written byte @0x1000 is 0xAB ", uiByte == 0xABu);
    result = st_write_byte(0xFC0000, 0x11u);
    UC_TEST("[N] (TC-STM-039) Writing byte to ROM area returns ST_ERROR ", result == ST_ERROR);
    result = st_write_byte(0xFFFCDE, 0x22u);
    UC_TEST("[N] (TC-STM-040) Writing byte to unmapped area returns ST_ERROR ", result == ST_ERROR);
    UC_CHECK("(INT-STM-006) [Chk] Write last RAM byte",
             st_write_byte(ST_RAM_SIZE - 1, 0x55u));
    UC_CHECK("(INT-STM-006) [Chk] Read back last RAM byte",
             st_read_byte(ST_RAM_SIZE - 1, &uiByte));
    UC_TEST("[R] (TC-STM-041) Last byte of RAM after write is 0x55 ", uiByte == 0x55u);


    /* -- [ST]25. Reject unaligned address with an error -- */
    /* -- [ST]26. Write word, based on write byte function -- */
    /* INTENT[INT-STM-007 → TC-STM-042...047 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Attempt writing words to RAM area, unaligned address and ROM area
     * (read-only) */
    UC_INFO("(INT-STM-007) Writing words in RAM, unaligned & ROM area");
    UC_CHECK("(INT-STM-007) [Chk] st_write_word(0x1000, 0x1234)",
             st_write_word(0x1000, 0x1234u));
    UC_CHECK("(INT-STM-007) [Chk] Read back written word",
             st_read_word(0x1000, &uiWord));
    UC_TEST("[N] (TC-STM-042) Written word @0x1000 is 0x1234 ", uiWord == 0x1234u);
    result = st_write_word(0x0001, 0x5678u);
    UC_TEST("[R] (TC-STM-043) Writing word at unaligned address returns ST_ERROR ", result == ST_ERROR);
    result = st_write_word(0xFC0000, 0x9999u);
    UC_TEST("[N] (TC-STM-044) Writing word to ROM area returns ST_ERROR ", result == ST_ERROR);
    result = st_write_word(0xFFFCDE, 0x4321u);
    UC_TEST("[N] (TC-STM-045) Writing word to unmapped area returns ST_ERROR ", result == ST_ERROR);
    UC_CHECK("(INT-STM-007) [Chk] Write last RAM word",
             st_write_word(ST_RAM_SIZE - 2, 0xBEEFu));
    UC_CHECK("(INT-STM-007) [Chk] Read back last RAM word",
             st_read_word(ST_RAM_SIZE - 2, &uiWord));
    UC_TEST("[R] (TC-STM-046) Last word of RAM after write is 0xBEEF ", uiWord == 0xBEEFu);
    result = st_write_word(ST_RAM_SIZE - 1, 0x4321u);
    UC_TEST("[N] (TC-STM-047) Writing word at last byte of RAM returns ST_ERROR ", result == ST_ERROR);
    

    /* -- [ST]27. Write long, based on write word function -- */
    /* -- [ST]29. Reject the write if any of the 4 bytes is not writable,
     *            before touching any word (atomic long write) -- */
    /* INTENT[INT-STM-008 → TC-STM-048...052 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * Attempt writing longs to RAM area and ROM area (read-only) ; a
     * long straddling the RAM/unmapped boundary must be rejected
     * entirely, without leaving its high word written (TC-STM-051/052) */
    UC_INFO("(INT-STM-008) Writing longs in RAM & ROM area");
    UC_CHECK("(INT-STM-008) [Chk] st_write_long(0x1000, 0xDEADBEEF)",
             st_write_long(0x1000, 0xDEADBEEFu));
    UC_CHECK("(INT-STM-008) [Chk] Read back written long",
             st_read_long(0x1000, &uiLong));
    UC_TEST("[N] (TC-STM-048) Written long @0x1000 is 0xDEADBEEF ", uiLong == 0xDEADBEEFu);
    result = st_write_long(0xFC0000, 0x11223344u);
    UC_TEST("[N] (TC-STM-049) Writing long to ROM area returns ST_ERROR ", result == ST_ERROR);
    UC_CHECK("(INT-STM-008) [Chk] Write last RAM long",
             st_write_long(ST_RAM_SIZE - 4, 0xCAFEBABEu));
    UC_CHECK("(INT-STM-008) [Chk] Read back last RAM long",
             st_read_long(ST_RAM_SIZE - 4, &uiLong));
    UC_TEST("[R] (TC-STM-050) Last long of RAM after write is 0xCAFEBABE ", uiLong == 0xCAFEBABEu);
    result = st_write_long(ST_RAM_SIZE - 2, 0xDEADC0DEu);
    UC_TEST("[N] (TC-STM-051) Writing word at last byte of RAM returns ST_ERROR ", result == ST_ERROR);
    UC_CHECK("(INT-STM-008) [Chk] Read back last RAM word",
             st_read_word(ST_RAM_SIZE - 2, &uiWord));
    UC_TEST("[R] (TC-STM-052) Last word of RAM after incorrect last long write is 0xBABE ", uiWord == 0xBABEu);
    
 }

/* ------------------------------------------------------------------
 * Group 3: CPU 68000 + hello.prg
 * ------------------------------------------------------------------ */

/*
 * uc01_test_cpu() - cpu_init() / cpu_step() on hello.prg
 *
 * Code Coverage:
 *   CPU.c:
 *   -- [CPU]1. Read the reset SSP vector from ST RAM --
 *   -- [CPU]2. Read the reset PC vector from ST RAM --
 *   -- [CPU]3. Enter supervisor mode and RUNNING state at the reset PC --
 *   -- [CPU]4. Do nothing if the CPU is not RUNNING --
 *   -- [CPU]5. Fetch the opcode word and advance PC --
 *   -- [CPU]6. Fill the optional result structure when ptResult is provided --
 *   -- [CPU]11. Dispatch Misc opcodes(0x4xxx) --
 *   -- [CPU]14. Dispatch MOVEQ opcodes (0x7xxx) --
 *   -- [CPU]23. Update result & cycles count for time accuracy --
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   Void
 */
static void uc01_test_cpu(void)
{
    cpu_context_t*    tCpu;
    cpu_step_result_t tResult;
    st_u32_t          uiTextSz;
    st_u64_t          ulR;

    const st_u32_t    UI_LOAD_ADDR  = 0x1000;
    const st_u32_t    UI_STACK_ADDR = 0x0800;

    printf("\n--- Test group 3: CPU 68000 + hello.prg ---\n");

    /* -- [CPU]1. Read the reset SSP vector from ST RAM -- */
    /* -- [CPU]2. Read the reset PC vector from ST RAM -- */
    /* -- [CPU]3. Enter supervisor mode and RUNNING state at the reset PC -- */
    /* INTENT[INT-CPU-001 → TC-CPU-001...005 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * cpu_init reset PC & SSP values if ST Machine is powered on */
    UC_CHECK("(INT-CPU-001) [Chk] st_shutdown() (force machine OFF)",
             st_shutdown());
    ulR = cpu_init();
    UC_TEST("[R] (TC-CPU-001) cpu_init() fails when the"
            " ST machine is powered off", ulR == ST_ERROR);
    UC_CHECK("(INT-CPU-001) [Chk] st_init() (restore machine ON)", st_init(NULL));
    UC_CHECK("(INT-CPU-001) [Chk] write SSP reset vector",
             st_write_long(CPU_VEC_RESET_SSP, UI_STACK_ADDR));
    UC_CHECK("(INT-CPU-001) [Chk] write PC reset vector",
             st_write_long(CPU_VEC_RESET_PC,  UI_LOAD_ADDR));

    tCpu = (cpu_context_t*)cpu_init(NULL);
    UC_CHECK("(INT-CPU-001) [Chk] cpu_init() launched", (st_u64_t)tCpu);
    UC_TEST("[R] (TC-CPU-002) CPU SSP == 0x0800 after init",
            tCpu->uiSSP == UI_STACK_ADDR);
    UC_TEST("[R] (TC-CPU-003) CPU PC == 0x1000 after init",
            tCpu->uiPC == UI_LOAD_ADDR);
    UC_TEST("[N] (TC-CPU-004) CPU SR supervisor mode set",
            (tCpu->uiSR & CPU_SR_S) != 0);
    UC_TEST("[N] (TC-CPU-005) CPU state == RUNNING",
            tCpu->eState == CPU_STATE_RUNNING);


    /* INTENT[INT-CPU-003 → TC-CPU-003 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * hello.prg text section must load cleanly into ST RAM
    eR = uc01_load_prg_text("use_cases/UC01/hello.prg",
                              UI_LOAD_ADDR,
                              &uiTextSz);
    UC_TEST("[N] (TC-CPU-003) hello.prg text section loaded (4 bytes)",
            eR == ST_NO_ERROR && uiTextSz == 4);

    if (eR != ST_NO_ERROR)
    {
        printf("  [WARN] Skipping remaining CPU tests - PRG load failed.\n"
               "         Run from the project root directory.\n");
        st_shutdown();
        return;
    }*/

    /* -- [CPU]7. Do nothing if the CPU is not RUNNING -- */
    /* -- [CPU]8. Fetch the opcode word and advance PC -- */
    /* -- [CPU]9. Dispatch MOVEQ opcodes (0x7xxx) -- */
    /* INTENT[INT-CPU-006 → TC-CPU-009...011 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * cpu_step must fetch the MOVEQ #42,D0 opcode, advance PC by 2,
     * and execute it (D0=42)
    UC_CHECK("[N] cpu_step() #1 (MOVEQ #42,D0)",
             cpu_step(&tCpu, &tResult));
    UC_TEST("[N] (TC-CPU-009) step #1 PC advanced by 2",
            tResult.uiPCAfter == UI_LOAD_ADDR + 2);
    UC_TEST("[N] (TC-CPU-010) step #1 opcode == 0x702A (MOVEQ #42,D0)",
            tResult.uiOpcode == 0x702A);
    UC_TEST("[N] (TC-CPU-011) step #1 D0==42 after MOVEQ",
            tCpu.auDn[0] == 42u);*/

    /* -- [CPU]10. Dispatch Misc opcodes incl. RTS/NOP/STOP/RTE/RTR
     *             (0x4Exx) -- */
    /* INTENT[INT-CPU-007 → TC-CPU-012...013 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * cpu_step must fetch and fully execute the RTS opcode on the
     * second step - RTS pops the return address from the stack; since
     * nothing was pushed, it pops the zeroed stack top (PC becomes 0)
    UC_CHECK("[N] cpu_step() #2 (RTS)", cpu_step(&tCpu, &tResult));
    UC_TEST("[N] (TC-CPU-012) step #2 opcode == 0x4E75 (RTS)",
            tResult.uiOpcode == 0x4E75);
    UC_TEST("[N] (TC-CPU-013) step #2 PC popped from the (zeroed) stack",
            tCpu.uiPC == 0u);*/

    /* -- [CPU]11. Fill the optional result structure when ptResult is
     *             provided -- */
    /* INTENT[INT-CPU-008 → TC-CPU-014 → REQ-xxx-yyy → UFR-xxx-yyy]:
     * cpu_step must accept a NULL ptResult (diagnostic output is
     * optional) and still execute the fetched instruction normally
    eR = cpu_step(&tCpu, NULL);
    UC_TEST("[N] (TC-CPU-014) cpu_step(&tCpu, NULL) returns ST_NO_ERROR",
            eR == ST_NO_ERROR);

    printf("  [INFO] CPU after 3 steps: PC=0x%08X instrCount=%llu\n",
           tCpu.uiPC,
           (unsigned long long)tCpu.ulInstrCount);*/
}

/* ------------------------------------------------------------------
 * Group 4: Disassembler stub
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

    /* Init the log function before anything else for file logging */
    char* args[] = {"UC01"};
    ptCtx = (ST4Ever_context_t*)ST4Ever_init(1, args);
    UC_CHECK("(UC01) [Chk] Launch ST4Ever with no argument", (st_u64_t)ptCtx);
    UC_CHECK_OBJ(ptCtx, ST_MAIN_CTX);
    if (gIsObject) 
    {
        /* Launch the use_case_01.c tests */
        uc01_test_trace();
        uc01_test_st_machine();
        uc01_test_cpu();
        test_disasm();
    } else printf("  [SKIP] (UC01) ST_MAIN_CTX Object Check failed\n\n");

    printf("\n--- Shutdown ---\n");
    ST4Ever_shutdown();
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
