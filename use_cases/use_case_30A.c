/*
 * use_case_30A.c - UC30A: DEVPAC3 assembler infrastructure
 *
 * Tests the two-pass assembler (directives only, no instruction
 * encoding).  Validates: as_init/assemble/shutdown, PRG header,
 * SECTION/DC/DS/EVEN/EQU, symbol table, PRG binary content.
 *
 * TEST MATRIX - UC30A:
 *   [N] Nominal    : 26 tests - init, directives, PRG output, symbols
 *   [R] Robustness :  9 tests - NULL params, bad file, unknown mnem
 *   [S] Skipped    :  0 tests
 */

#include "use_cases.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* PRG header field extraction helpers (big-endian)                    */
/* ------------------------------------------------------------------ */

static st_u16_t prg_u16(const st_u8_t *p, int off)
{
    return (st_u16_t)(((st_u16_t)p[off] << 8) | p[off + 1]);
}

static st_u32_t prg_u32(const st_u8_t *p, int off)
{
    return ((st_u32_t)p[off    ] << 24)
         | ((st_u32_t)p[off + 1] << 16)
         | ((st_u32_t)p[off + 2] <<  8)
         |  (st_u32_t)p[off + 3];
}

/* Return pointer to .text start inside binary (after 28-byte header) */
static const st_u8_t *prg_text(const as_context_t *ptCtx)
{
    return ptCtx->pBinary + 28u;
}

/* Return pointer to .data start inside binary */
static const st_u8_t *prg_data(const as_context_t *ptCtx)
{
    st_u32_t uiTextSz = prg_u32(ptCtx->pBinary, 2);
    return ptCtx->pBinary + 28u + uiTextSz;
}

/* ------------------------------------------------------------------ */
/* Test helpers                                                         */
/* ------------------------------------------------------------------ */

static void cleanup_prg(const char *szPath)
{
    remove(szPath);
}

/* Assemble a source file and return ST_NO_ERROR on success */
static st_error_t run_assemble(as_context_t *ptCtx,
                                 const char *szSrc,
                                 const char *szOut,
                                 st_bool_t bReloc)
{
    if (as_init(ptCtx, szSrc, szOut, bReloc) != ST_NO_ERROR)
        return ST_ERROR;
    return as_assemble(ptCtx);
}

/* ------------------------------------------------------------------ */
/* Test groups                                                          */
/* ------------------------------------------------------------------ */

static void test_null_params(void)
{
    as_context_t tCtx;

    printf("\n[UC30A] NULL parameter guards\n");

    /* INTENT[INT-AS-001 -> TC-AS-001 -> REQ-AS-001]:
     * as_init returns ST_ERROR on any NULL pointer */
    TEST_ASSERT("[R] as_init NULL ctx",
        as_init(NULL, "x.S", "x.prg", ST_TRUE) == ST_ERROR);
    TEST_ASSERT("[R] as_init NULL src",
        as_init(&tCtx, NULL, "x.prg", ST_TRUE) == ST_ERROR);
    TEST_ASSERT("[R] as_init NULL out",
        as_init(&tCtx, "x.S", NULL, ST_TRUE) == ST_ERROR);

    /* INTENT[INT-AS-002 -> TC-AS-002 -> REQ-AS-001]:
     * as_assemble returns ST_ERROR on NULL context */
    TEST_ASSERT("[R] as_assemble NULL ctx",
        as_assemble(NULL) == ST_ERROR);

    /* INTENT[INT-AS-003 -> TC-AS-003 -> REQ-AS-001]:
     * as_shutdown returns ST_ERROR on NULL context */
    TEST_ASSERT("[R] as_shutdown NULL ctx",
        as_shutdown(NULL) == ST_ERROR);
}

static void test_missing_source(void)
{
    as_context_t tCtx;

    printf("\n[UC30A] Missing source file\n");

    /* INTENT[INT-AS-004 -> TC-AS-004 -> REQ-AS-002]:
     * as_assemble on non-existent source returns ST_ERROR with
     * at least one error entry */
    TEST_ASSERT("[R] as_init valid",
        as_init(&tCtx, "no_such_file.S", "out.prg",
                ST_TRUE) == ST_NO_ERROR);
    TEST_ASSERT("[R] as_assemble missing src -> ST_ERROR",
        as_assemble(&tCtx) == ST_ERROR);
    TEST_ASSERT("[R] error count > 0",
        tCtx.uiErrorCount > 0);
    as_shutdown(&tCtx);
}

static void test_minimal(void)
{
    as_context_t tCtx;
    const char *szSrc = "use_cases/UC30A/minimal.S";
    const char *szOut = "use_cases/UC30A/minimal.prg";

    printf("\n[UC30A] Minimal source (DC.W only)\n");

    /* INTENT[INT-AS-005 -> TC-AS-005 -> REQ-AS-003]:
     * Assembling a data-only source succeeds and produces
     * a well-formed PRG binary */
    TEST_ASSERT("[N] as_init",
        as_init(&tCtx, szSrc, szOut, ST_TRUE) == ST_NO_ERROR);
    TEST_ASSERT("[N] as_assemble OK",
        as_assemble(&tCtx) == ST_NO_ERROR);
    TEST_ASSERT("[N] binary allocated",
        tCtx.pBinary != NULL);
    TEST_ASSERT("[N] binary length > header",
        tCtx.uiBinaryLen > 28u);
    TEST_ASSERT("[N] error count == 0",
        tCtx.uiErrorCount == 0u);

    /* INTENT[INT-AS-006 -> TC-AS-006 -> REQ-AS-004]:
     * PRG header magic is 0x601A */
    TEST_ASSERT("[N] PRG magic 0x601A",
        tCtx.pBinary != NULL &&
        prg_u16(tCtx.pBinary, 0) == 0x601Au);

    /* INTENT[INT-AS-007 -> TC-AS-007 -> REQ-AS-004]:
     * text_size == 4 (two DC.W words), data_size == 0, bss_size == 0 */
    TEST_ASSERT("[N] text_size == 4",
        prg_u32(tCtx.pBinary, 2) == 4u);
    TEST_ASSERT("[N] data_size == 0",
        prg_u32(tCtx.pBinary, 6) == 0u);
    TEST_ASSERT("[N] bss_size == 0",
        prg_u32(tCtx.pBinary, 10) == 0u);

    /* INTENT[INT-AS-008 -> TC-AS-008 -> REQ-AS-005]:
     * DC.W $4E71 emits big-endian bytes $4E,$71 */
    TEST_ASSERT("[N] text[0] == $4E",
        tCtx.pBinary != NULL &&
        prg_text(&tCtx)[0] == 0x4Eu);
    TEST_ASSERT("[N] text[1] == $71",
        prg_text(&tCtx)[1] == 0x71u);

    /* INTENT[INT-AS-009 -> TC-AS-009 -> REQ-AS-005]:
     * DC.W $4E75 emits big-endian bytes $4E,$75 */
    TEST_ASSERT("[N] text[2] == $4E",
        prg_text(&tCtx)[2] == 0x4Eu);
    TEST_ASSERT("[N] text[3] == $75",
        prg_text(&tCtx)[3] == 0x75u);

    as_shutdown(&tCtx);
    cleanup_prg(szOut);
}

static void test_directives(void)
{
    as_context_t tCtx;
    const char *szSrc = "use_cases/UC30A/test.S";
    const char *szOut = "use_cases/UC30A/test.prg";
    st_u32_t    uiTextSz;
    st_u32_t    uiDataSz;
    st_u32_t    uiBssSz;
    const st_u8_t *pText;
    const st_u8_t *pData;

    printf("\n[UC30A] SECTION / DC / DS / EVEN / EQU\n");

    TEST_ASSERT("[N] as_assemble test.S OK",
        run_assemble(&tCtx, szSrc, szOut,
                     ST_TRUE) == ST_NO_ERROR);

    if (tCtx.pBinary == NULL)
    {
        TEST_ASSERT("[N] binary present (skip remaining)", ST_FALSE);
        as_shutdown(&tCtx);
        return;
    }

    uiTextSz = prg_u32(tCtx.pBinary, 2);
    uiDataSz = prg_u32(tCtx.pBinary, 6);
    uiBssSz  = prg_u32(tCtx.pBinary, 10);
    pText    = prg_text(&tCtx);
    pData    = prg_data(&tCtx);

    /* INTENT[INT-AS-010 -> TC-AS-010 -> REQ-AS-006]:
     * BSS size reflects DS.B 64 and generates no emitted bytes */
    TEST_ASSERT("[N] bss_size == 64",
        uiBssSz == 64u);

    /* INTENT[INT-AS-011 -> TC-AS-011 -> REQ-AS-006]:
     * .data section: 3 words + 1 long + 4 bytes = 14 bytes */
    TEST_ASSERT("[N] data_size == 14",
        uiDataSz == 14u);

    /* INTENT[INT-AS-012 -> TC-AS-012 -> REQ-AS-005]:
     * First two bytes of .text are EQU MAGIC=$601A emitted as DC.W */
    TEST_ASSERT("[N] text[0] == $60 (MAGIC hi)",
        pText[0] == 0x60u);
    TEST_ASSERT("[N] text[1] == $1A (MAGIC lo)",
        pText[1] == 0x1Au);

    /* INTENT[INT-AS-013 -> TC-AS-013 -> REQ-AS-005]:
     * DC.L $DEADBEEF at offset 4 */
    TEST_ASSERT("[N] text[4] == $DE",
        pText[4] == 0xDEu);
    TEST_ASSERT("[N] text[5] == $AD",
        pText[5] == 0xADu);
    TEST_ASSERT("[N] text[6] == $BE",
        pText[6] == 0xBEu);
    TEST_ASSERT("[N] text[7] == $EF",
        pText[7] == 0xEFu);

    /* INTENT[INT-AS-014 -> TC-AS-014 -> REQ-AS-005]:
     * DC.B 'H','e','l','l' starts at offset 8 */
    TEST_ASSERT("[N] text[8]  == 'H'",  pText[8]  == 'H');
    TEST_ASSERT("[N] text[9]  == 'e'",  pText[9]  == 'e');
    TEST_ASSERT("[N] text[10] == 'l'",  pText[10] == 'l');
    TEST_ASSERT("[N] text[11] == 'l'",  pText[11] == 'l');

    /* INTENT[INT-AS-015 -> TC-AS-015 -> REQ-AS-007]:
     * EVEN after odd byte count pads to word boundary */
    TEST_ASSERT("[N] text[14] == $FF (pre-EVEN byte)",
        pText[14] == 0xFFu);
    TEST_ASSERT("[N] text[15] == $00 (EVEN pad)",
        pText[15] == 0x00u);

    /* INTENT[INT-AS-016 -> TC-AS-016 -> REQ-AS-008]:
     * DS.W COUNT (EQU 8) reserves 16 zero bytes at offset 16 */
    {
        int i;
        st_bool_t bAllZero = ST_TRUE;
        for (i = 16; i < 32 && (size_t)(28 + uiTextSz) > 28u; i++)
            if (pText[i] != 0x00u) { bAllZero = ST_FALSE; break; }
        TEST_ASSERT("[N] DS.W 8 -> 16 zero bytes", bAllZero);
    }

    /* INTENT[INT-AS-017 -> TC-AS-017 -> REQ-AS-006]:
     * .data: DC.W $1234 at offset 0 */
    TEST_ASSERT("[N] data[0] == $12", pData[0] == 0x12u);
    TEST_ASSERT("[N] data[1] == $34", pData[1] == 0x34u);
    TEST_ASSERT("[N] data[2] == $56", pData[2] == 0x56u);
    TEST_ASSERT("[N] data[3] == $78", pData[3] == 0x78u);

    /* INTENT[INT-AS-018 -> TC-AS-018 -> REQ-AS-006]:
     * .data: DS.B 4 zeros at offset 10 */
    TEST_ASSERT("[N] data[10..13] == 0",
        pData[10] == 0 && pData[11] == 0 &&
        pData[12] == 0 && pData[13] == 0);

    /* Unused variable suppression */
    ST_UNUSED(uiTextSz);

    as_shutdown(&tCtx);
    cleanup_prg(szOut);
}

static void test_shutdown_frees(void)
{
    as_context_t tCtx;
    const char *szSrc = "use_cases/UC30A/minimal.S";
    const char *szOut = "use_cases/UC30A/minimal2.prg";

    printf("\n[UC30A] as_shutdown frees resources\n");

    /* INTENT[INT-AS-019 -> TC-AS-019 -> REQ-AS-009]:
     * as_shutdown frees pBinary and zeroes the context */
    TEST_ASSERT("[N] assemble OK",
        run_assemble(&tCtx, szSrc, szOut,
                     ST_FALSE) == ST_NO_ERROR);
    TEST_ASSERT("[N] binary non-NULL before shutdown",
        tCtx.pBinary != NULL);
    TEST_ASSERT("[N] as_shutdown OK",
        as_shutdown(&tCtx) == ST_NO_ERROR);
    TEST_ASSERT("[N] binary NULL after shutdown",
        tCtx.pBinary == NULL);
    TEST_ASSERT("[N] length 0 after shutdown",
        tCtx.uiBinaryLen == 0u);

    cleanup_prg(szOut);
}

static void test_unknown_mnemonic(void)
{
    as_context_t tCtx;
    const char *szOut = "use_cases/UC30A/unk.prg";
    const char *szSrc = "use_cases/UC30A/unknown.S";
    FILE *fp;

    printf("\n[UC30A] Unknown mnemonic -> error\n");

    /* Write a source with a 68000 instruction */
    fp = fopen(szSrc, "w");
    if (fp != NULL)
    {
        fprintf(fp, "        SECTION TEXT\n");
        fprintf(fp, "        MOVE.W  D0,D1\n");
        fprintf(fp, "        END\n");
        fclose(fp);
    }

    /* INTENT[INT-AS-020 -> TC-AS-020 -> REQ-AS-010]:
     * An unknown mnemonic (MOVE — UC30B+) returns ST_ERROR with
     * a non-empty error message */
    TEST_ASSERT("[R] as_init OK",
        as_init(&tCtx, szSrc, szOut, ST_TRUE) == ST_NO_ERROR);
    TEST_ASSERT("[R] as_assemble -> ST_ERROR (instruction NYI)",
        as_assemble(&tCtx) == ST_ERROR);
    TEST_ASSERT("[R] error count > 0",
        tCtx.uiErrorCount > 0);
    TEST_ASSERT("[R] error message non-empty",
        tCtx.ptErrors != NULL &&
        tCtx.ptErrors[0].szMsg[0] != '\0');

    as_shutdown(&tCtx);
    remove(szSrc);
    cleanup_prg(szOut);
}

static void test_equ(void)
{
    as_context_t tCtx;
    const char *szOut = "use_cases/UC30A/equ.prg";
    const char *szSrc = "use_cases/UC30A/equ.S";
    FILE *fp;

    printf("\n[UC30A] EQU symbolic constants\n");

    fp = fopen(szSrc, "w");
    if (fp != NULL)
    {
        fprintf(fp, "SIZE    EQU     $0010\n");
        fprintf(fp, "BASE    EQU     256\n");
        fprintf(fp, "        SECTION DATA\n");
        fprintf(fp, "        DC.W    SIZE\n");   /* $0010 */
        fprintf(fp, "        DC.L    BASE\n");   /* 256 = $00000100 */
        fprintf(fp, "        END\n");
        fclose(fp);
    }

    /* INTENT[INT-AS-021 -> TC-AS-021 -> REQ-AS-011]:
     * EQU constants resolve correctly in DC directives */
    TEST_ASSERT("[N] as_assemble equ.S OK",
        run_assemble(&tCtx, szSrc, szOut,
                     ST_TRUE) == ST_NO_ERROR);

    TEST_ASSERT("[N] data_size == 6 (1 word + 1 long)",
        tCtx.pBinary != NULL &&
        prg_u32(tCtx.pBinary, 6) == 6u);

    {
        const st_u8_t *pData = prg_data(&tCtx);
        TEST_ASSERT("[N] DC.W SIZE -> $00,$10",
            pData[0] == 0x00u && pData[1] == 0x10u);
        TEST_ASSERT("[N] DC.L BASE -> $00,$00,$01,$00",
            pData[2] == 0x00u && pData[3] == 0x00u &&
            pData[4] == 0x01u && pData[5] == 0x00u);
    }

    as_shutdown(&tCtx);
    remove(szSrc);
    cleanup_prg(szOut);
}

static void test_absolute_mode(void)
{
    as_context_t tCtx;
    const char *szSrc = "use_cases/UC30A/minimal.S";
    const char *szOut = "use_cases/UC30A/minimal_abs.prg";

    printf("\n[UC30A] Absolute (non-relocatable) mode\n");

    /* INTENT[INT-AS-022 -> TC-AS-022 -> REQ-AS-012]:
     * bRelocatable=ST_FALSE sets abs_flag != 0 in PRG header.
     * The abs_flag field is at byte offset 24 (4-byte field in our
     * header: offsets 22-25 are flags 4 bytes, but abs_flag is at
     * offset 26 as a 2-byte field — check != 0). */
    TEST_ASSERT("[N] as_assemble absolute mode OK",
        run_assemble(&tCtx, szSrc, szOut,
                     ST_FALSE) == ST_NO_ERROR);
    TEST_ASSERT("[N] abs_flag non-zero in header (offset 26)",
        tCtx.pBinary != NULL &&
        prg_u16(tCtx.pBinary, 26) != 0u);

    as_shutdown(&tCtx);
    cleanup_prg(szOut);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC30A: DEVPAC3 Assembler Infrastructure ===\n");
    printf("Testing: as_init / as_assemble / as_shutdown\n");
    printf("Directives: SECTION DC.B/W/L DS.B/W/L EVEN EQU END\n\n");

    test_null_params();
    test_missing_source();
    test_minimal();
    test_directives();
    test_shutdown_frees();
    test_unknown_mnemonic();
    test_equ();
    test_absolute_mode();

    printf("\n=== UC30A Results: %d failure(s) ===\n\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
