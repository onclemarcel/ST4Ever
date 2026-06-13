/* use_case_30B.c - UC30B: DEVPAC3 EA encoder + MOVE/MOVEQ/LEA/CLR/SWAP
 *
 * TEST MATRIX - UC30B:
 *   [N] Nominal    : 38 tests - instructions, real-PRG match, CRLF compat,
 *                               round-trip disasm->reassemble
 *   [R] Robustness :  6 tests - NULL params, bad EA (MOVEQ/LEA wrong reg),
 *                               unknown mnemonic, double init/shutdown
 *   [S] Skipped    :  0 tests (TEST30B.PRG absent: 3 sub-tests print SKIP)
 *
 * INT map:
 *   INT-AS-030 -> TC-AS-101: as_assemble(NULL)
 *   INT-AS-031 -> TC-AS-102: unknown mnemonic
 *   INT-AS-032 -> TC-AS-103: MOVEQ #1,A0 rejected
 *   INT-AS-033 -> TC-AS-104: LEA $1000.W,D0 rejected
 *   INT-AS-034 -> TC-AS-105: double init/shutdown cycle
 *   INT-AS-035 -> TC-AS-106: test30b.S assembles OK
 *   INT-AS-036 -> TC-AS-107: text_size == 58 bytes
 *   INT-AS-037 -> TC-AS-108: MOVEQ byte-level encodings
 *   INT-AS-038 -> TC-AS-109: MOVE.B/W/L + MOVEA byte-level encodings
 *   INT-AS-039 -> TC-AS-109: CLR/SWAP/LEA byte-level encodings
 *   INT-AS-040 -> TC-AS-110: byte-exact match vs real ATARI ST PRG
 *   INT-AS-041 -> TC-AS-111: CRLF source == LF source (binary identical)
 *   INT-AS-042 -> TC-AS-112: round-trip .S->PRG->disasm->CRLF .S->PRG closed
 */

#include "use_cases.h"
#include "../src/as.h"
#include "../src/disassemble.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers (same pattern as UC30A)                                      */
/* ------------------------------------------------------------------ */

static st_error_t run_assemble(as_context_t *ptCtx,
                                const char   *szSrc,
                                const char   *szOut,
                                st_bool_t     bReloc)
{
    if (as_init(ptCtx, szSrc, szOut, bReloc) != ST_NO_ERROR)
        return ST_ERROR;
    return as_assemble(ptCtx);
}

/* Return pointer to .text start (after 28-byte PRG header) */
static const st_u8_t *prg_text(const as_context_t *ptCtx)
{
    return ptCtx->pBinary + 28u;
}

static void cleanup_prg(const char *szPath)
{
    remove(szPath);
}

/* ------------------------------------------------------------------ */
/* Robustness tests                                                      */
/* ------------------------------------------------------------------ */

static void test_null_params(void)
{
    printf("\n[UC30B] NULL parameter guards\n");

    /* INTENT[INT-AS-030 -> TC-AS-101 -> REQ-AS-001]:
     * as_assemble(NULL) -> ST_ERROR, no crash */
    TEST_ASSERT("[R] as_assemble NULL ctx",
        as_assemble(NULL) == ST_ERROR);
}

static void test_unknown_mnemonic(void)
{
    as_context_t tCtx;
    FILE        *pF;
    const char  *szSrc = "use_cases/UC30B/tmp_bad.S";
    const char  *szOut = "use_cases/UC30B/tmp_bad.prg";

    printf("\n[UC30B] Unknown mnemonic -> ST_ERROR\n");

    pF = fopen(szSrc, "w");
    if (pF)
    {
        fprintf(pF, "\tSECTION TEXT\n\tFOOBAR.W D0,D1\n\tEND\n");
        fclose(pF);
    }

    /* INTENT[INT-AS-031 -> TC-AS-102 -> REQ-AS-010]:
     * Unrecognised mnemonic causes ST_ERROR */
    TEST_ASSERT("[R] unknown mnemonic -> ST_ERROR",
        run_assemble(&tCtx, szSrc, szOut, ST_TRUE) == ST_ERROR);

    as_shutdown(&tCtx);
    remove(szSrc);
    cleanup_prg(szOut);
}

static void test_moveq_to_an(void)
{
    as_context_t tCtx;
    FILE        *pF;
    const char  *szSrc = "use_cases/UC30B/tmp_moveq_an.S";
    const char  *szOut = "use_cases/UC30B/tmp_moveq_an.prg";

    printf("\n[UC30B] MOVEQ to An -> ST_ERROR\n");

    pF = fopen(szSrc, "w");
    if (pF)
    {
        fprintf(pF, "\tSECTION TEXT\n\tMOVEQ #1,A0\n\tEND\n");
        fclose(pF);
    }

    /* INTENT[INT-AS-032 -> TC-AS-103 -> REQ-AS-001]:
     * MOVEQ destination must be Dn */
    TEST_ASSERT("[R] MOVEQ #1,A0 -> ST_ERROR",
        run_assemble(&tCtx, szSrc, szOut, ST_TRUE) == ST_ERROR);

    as_shutdown(&tCtx);
    remove(szSrc);
    cleanup_prg(szOut);
}

static void test_lea_to_dn(void)
{
    as_context_t tCtx;
    FILE        *pF;
    const char  *szSrc = "use_cases/UC30B/tmp_lea_dn.S";
    const char  *szOut = "use_cases/UC30B/tmp_lea_dn.prg";

    printf("\n[UC30B] LEA to Dn -> ST_ERROR\n");

    pF = fopen(szSrc, "w");
    if (pF)
    {
        fprintf(pF, "\tSECTION TEXT\n\tLEA $1000.W,D0\n\tEND\n");
        fclose(pF);
    }

    /* INTENT[INT-AS-033 -> TC-AS-104 -> REQ-AS-001]:
     * LEA destination must be An */
    TEST_ASSERT("[R] LEA $1000.W,D0 -> ST_ERROR",
        run_assemble(&tCtx, szSrc, szOut, ST_TRUE) == ST_ERROR);

    as_shutdown(&tCtx);
    remove(szSrc);
    cleanup_prg(szOut);
}

static void test_init_shutdown_cycle(void)
{
    as_context_t tCtx;

    printf("\n[UC30B] as_init / as_shutdown lifecycle\n");

    /* INTENT[INT-AS-034 -> TC-AS-105 -> REQ-AS-001]:
     * Double init/shutdown is safe */
    TEST_ASSERT("[R] as_init ST_NO_ERROR",
        as_init(&tCtx, "use_cases/UC30B/test30b.S",
                "use_cases/UC30B/test30b_2.prg", ST_TRUE) == ST_NO_ERROR);
    as_shutdown(&tCtx);
    TEST_ASSERT("[R] re-init after shutdown OK",
        as_init(&tCtx, "use_cases/UC30B/test30b.S",
                "use_cases/UC30B/test30b_3.prg", ST_TRUE) == ST_NO_ERROR);
    as_shutdown(&tCtx);
    cleanup_prg("use_cases/UC30B/test30b_2.prg");
    cleanup_prg("use_cases/UC30B/test30b_3.prg");
}

/* ------------------------------------------------------------------ */
/* Nominal: full instruction encoding test                              */
/* ------------------------------------------------------------------ */

static void test_instruction_encodings(void)
{
    as_context_t   tCtx;
    const st_u8_t *pT;
    const char    *szSrc = "use_cases/UC30B/test30b.S";
    const char    *szOut = "use_cases/UC30B/test30b_enc.prg";
    st_u32_t       uiTextSz;

    printf("\n[UC30B] Instruction encoding: 23 instructions, 58 bytes\n");

    /* INTENT[INT-AS-035 -> TC-AS-106 -> REQ-AS-003]:
     * Full fixture assembles cleanly (0 errors) */
    TEST_ASSERT("[N] test30b.S assembles OK",
        run_assemble(&tCtx, szSrc, szOut, ST_TRUE) == ST_NO_ERROR);
    TEST_ASSERT("[N] binary non-NULL",
        tCtx.pBinary != NULL);

    if (tCtx.pBinary == NULL)
    { as_shutdown(&tCtx); cleanup_prg(szOut); return; }

    /* text_size field at offset 2 (BE u32) */
    uiTextSz = ((st_u32_t)tCtx.pBinary[2]  << 24)
             | ((st_u32_t)tCtx.pBinary[3]  << 16)
             | ((st_u32_t)tCtx.pBinary[4]  <<  8)
             |  (st_u32_t)tCtx.pBinary[5];

    /* INTENT[INT-AS-036 -> TC-AS-107 -> REQ-AS-004]:
     * .text size = 58 bytes (all 23 instructions) */
    TEST_ASSERT("[N] text_size == 58", uiTextSz == 58u);

    pT = prg_text(&tCtx);

    /* INTENT[INT-AS-037 -> TC-AS-108 -> REQ-AS-013,REQ-AS-014]:
     * MOVEQ encoding: opcode $7dn0 + sign-extended immediate byte */
    /* --- MOVEQ #0,D0 -> 0x7000 (offset 0) --- */
    TEST_ASSERT("[N] MOVEQ #0,D0  [0]=0x70", pT[0] == 0x70);
    TEST_ASSERT("[N] MOVEQ #0,D0  [1]=0x00", pT[1] == 0x00);

    /* --- MOVEQ #42,D3 -> 0x762A (offset 2) --- */
    TEST_ASSERT("[N] MOVEQ #42,D3 [2]=0x76", pT[2] == 0x76);
    TEST_ASSERT("[N] MOVEQ #42,D3 [3]=0x2A", pT[3] == 0x2A);

    /* --- MOVEQ #-1,D7 -> 0x7EFF (offset 4) --- */
    TEST_ASSERT("[N] MOVEQ #-1,D7 [4]=0x7E", pT[4] == 0x7E);
    TEST_ASSERT("[N] MOVEQ #-1,D7 [5]=0xFF", pT[5] == 0xFF);

    /* INTENT[INT-AS-038 -> TC-AS-109 -> REQ-AS-015,REQ-AS-016,REQ-AS-017]:
     * MOVE.B/W/L and MOVEA.W/L: size in bits 13-12, dst EA reversed,
     * all EA modes (Dn, An, (An), (An)+, ABS.W, IMM) verified byte-exact */
    /* --- MOVE.W D0,D1 -> 0x3200 (offset 6) --- */
    TEST_ASSERT("[N] MOVE.W D0,D1 [6]=0x32", pT[6] == 0x32);
    TEST_ASSERT("[N] MOVE.W D0,D1 [7]=0x00", pT[7] == 0x00);

    /* --- MOVE.W D7,D6 -> 0x3C07 (offset 8) --- */
    TEST_ASSERT("[N] MOVE.W D7,D6 [8]=0x3C", pT[8]  == 0x3C);
    TEST_ASSERT("[N] MOVE.W D7,D6 [9]=0x07", pT[9]  == 0x07);

    /* --- MOVE.W A2,D1 -> 0x320A (offset 10) --- */
    TEST_ASSERT("[N] MOVE.W A2,D1 [10]=0x32", pT[10] == 0x32);
    TEST_ASSERT("[N] MOVE.W A2,D1 [11]=0x0A", pT[11] == 0x0A);

    /* --- MOVE.L D2,D3 -> 0x2602 (offset 12) --- */
    TEST_ASSERT("[N] MOVE.L D2,D3 [12]=0x26", pT[12] == 0x26);
    TEST_ASSERT("[N] MOVE.L D2,D3 [13]=0x02", pT[13] == 0x02);

    /* --- MOVE.L A1,D3 -> 0x2609 (offset 14) --- */
    TEST_ASSERT("[N] MOVE.L A1,D3 [14]=0x26", pT[14] == 0x26);
    TEST_ASSERT("[N] MOVE.L A1,D3 [15]=0x09", pT[15] == 0x09);

    /* --- MOVE.B D4,D5 -> 0x1A04 (offset 16) --- */
    TEST_ASSERT("[N] MOVE.B D4,D5 [16]=0x1A", pT[16] == 0x1A);
    TEST_ASSERT("[N] MOVE.B D4,D5 [17]=0x04", pT[17] == 0x04);

    /* --- MOVE.W #$1234,D0 -> 0x303C 0x1234 (offset 18) --- */
    TEST_ASSERT("[N] MOVE.W #$1234,D0 [18]=0x30", pT[18] == 0x30);
    TEST_ASSERT("[N] MOVE.W #$1234,D0 [19]=0x3C", pT[19] == 0x3C);
    TEST_ASSERT("[N] MOVE.W #$1234,D0 [20]=0x12", pT[20] == 0x12);
    TEST_ASSERT("[N] MOVE.W #$1234,D0 [21]=0x34", pT[21] == 0x34);

    /* --- MOVE.W (A0),D1 -> 0x3210 (offset 22) --- */
    TEST_ASSERT("[N] MOVE.W (A0),D1 [22]=0x32", pT[22] == 0x32);
    TEST_ASSERT("[N] MOVE.W (A0),D1 [23]=0x10", pT[23] == 0x10);

    /* --- MOVE.W D2,(A1)+ -> 0x32C2 (offset 24) --- */
    TEST_ASSERT("[N] MOVE.W D2,(A1)+ [24]=0x32", pT[24] == 0x32);
    TEST_ASSERT("[N] MOVE.W D2,(A1)+ [25]=0xC2", pT[25] == 0xC2);

    /* --- MOVE.W D0,$1000.W -> 0x31C0 0x1000 (offset 26) --- */
    TEST_ASSERT("[N] MOVE.W D0,$1000.W [26]=0x31", pT[26] == 0x31);
    TEST_ASSERT("[N] MOVE.W D0,$1000.W [27]=0xC0", pT[27] == 0xC0);
    TEST_ASSERT("[N] MOVE.W D0,$1000.W [28]=0x10", pT[28] == 0x10);
    TEST_ASSERT("[N] MOVE.W D0,$1000.W [29]=0x00", pT[29] == 0x00);

    /* --- MOVE.W $2000.W,D1 -> 0x3238 0x2000 (offset 30) --- */
    TEST_ASSERT("[N] MOVE.W $2000.W,D1 [30]=0x32", pT[30] == 0x32);
    TEST_ASSERT("[N] MOVE.W $2000.W,D1 [31]=0x38", pT[31] == 0x38);
    TEST_ASSERT("[N] MOVE.W $2000.W,D1 [32]=0x20", pT[32] == 0x20);
    TEST_ASSERT("[N] MOVE.W $2000.W,D1 [33]=0x00", pT[33] == 0x00);

    /* --- MOVEA.W D0,A1 -> 0x3240 (offset 34) --- */
    TEST_ASSERT("[N] MOVEA.W D0,A1 [34]=0x32", pT[34] == 0x32);
    TEST_ASSERT("[N] MOVEA.W D0,A1 [35]=0x40", pT[35] == 0x40);

    /* --- MOVEA.L D0,A2 -> 0x2440 (offset 36) --- */
    TEST_ASSERT("[N] MOVEA.L D0,A2 [36]=0x24", pT[36] == 0x24);
    TEST_ASSERT("[N] MOVEA.L D0,A2 [37]=0x40", pT[37] == 0x40);

    /* INTENT[INT-AS-039 -> TC-AS-109 -> REQ-AS-013,REQ-AS-015]:
     * CLR.B/W/L: opcode $42xx; SWAP: opcode $4840+Dn; LEA: opcode $41F8/$43F9 */
    /* --- CLR.W D0 -> 0x4240 (offset 38) --- */
    TEST_ASSERT("[N] CLR.W D0 [38]=0x42", pT[38] == 0x42);
    TEST_ASSERT("[N] CLR.W D0 [39]=0x40", pT[39] == 0x40);

    /* --- CLR.L D1 -> 0x4281 (offset 40) --- */
    TEST_ASSERT("[N] CLR.L D1 [40]=0x42", pT[40] == 0x42);
    TEST_ASSERT("[N] CLR.L D1 [41]=0x81", pT[41] == 0x81);

    /* --- CLR.B D2 -> 0x4202 (offset 42) --- */
    TEST_ASSERT("[N] CLR.B D2 [42]=0x42", pT[42] == 0x42);
    TEST_ASSERT("[N] CLR.B D2 [43]=0x02", pT[43] == 0x02);

    /* --- SWAP D0 -> 0x4840 (offset 44) --- */
    TEST_ASSERT("[N] SWAP D0 [44]=0x48", pT[44] == 0x48);
    TEST_ASSERT("[N] SWAP D0 [45]=0x40", pT[45] == 0x40);

    /* --- SWAP D3 -> 0x4843 (offset 46) --- */
    TEST_ASSERT("[N] SWAP D3 [46]=0x48", pT[46] == 0x48);
    TEST_ASSERT("[N] SWAP D3 [47]=0x43", pT[47] == 0x43);

    /* --- LEA $1234.W,A0 -> 0x41F8 0x1234 (offset 48) --- */
    TEST_ASSERT("[N] LEA $1234.W,A0 [48]=0x41", pT[48] == 0x41);
    TEST_ASSERT("[N] LEA $1234.W,A0 [49]=0xF8", pT[49] == 0xF8);
    TEST_ASSERT("[N] LEA $1234.W,A0 [50]=0x12", pT[50] == 0x12);
    TEST_ASSERT("[N] LEA $1234.W,A0 [51]=0x34", pT[51] == 0x34);

    /* --- LEA $12345678.L,A1 -> 0x43F9 0x12345678 (offset 52) --- */
    TEST_ASSERT("[N] LEA $12345678.L,A1 [52]=0x43", pT[52] == 0x43);
    TEST_ASSERT("[N] LEA $12345678.L,A1 [53]=0xF9", pT[53] == 0xF9);
    TEST_ASSERT("[N] LEA $12345678.L,A1 [54]=0x12", pT[54] == 0x12);
    TEST_ASSERT("[N] LEA $12345678.L,A1 [55]=0x34", pT[55] == 0x34);
    TEST_ASSERT("[N] LEA $12345678.L,A1 [56]=0x56", pT[56] == 0x56);
    TEST_ASSERT("[N] LEA $12345678.L,A1 [57]=0x78", pT[57] == 0x78);

    as_shutdown(&tCtx);
    cleanup_prg(szOut);
}

/* ------------------------------------------------------------------ */
/* disasm_write_s() — produce DEVPAC3-compatible .S with CRLF          */
/* ------------------------------------------------------------------ */

/*
 * Disassemble pText (iTextLen bytes) and write a DEVPAC3 source file
 * to szPath with CRLF line endings (required by ATARI ST DEVPAC3).
 * Uses szMnemonic and szOperands from disasm_one() to reconstruct
 * assembler-compatible source (szLine includes the hex dump prefix so
 * it cannot be used directly).
 */
static st_error_t disasm_write_s(const st_u8_t *pText,
                                   size_t         uiTextLen,
                                   const char    *szPath)
{
    disasm_result_t  atR[512];
    size_t           uiLines;
    size_t           i;
    FILE            *pF;

    if (disasm_range(pText, uiTextLen, 0u,
                     atR, 512u, &uiLines) != ST_NO_ERROR)
        return ST_ERROR;

    pF = fopen(szPath, "wb");   /* binary: write \r\n explicitly */
    if (pF == NULL)
    {
        LOG_ERROR("cannot create '%s'", szPath);
        return ST_ERROR;
    }

    fprintf(pF, "\tSECTION TEXT\r\n\r\n");
    for (i = 0; i < uiLines; i++)
    {
        /* DC.W fallback: disassembler couldn't decode the opcode */
        if (!atR[i].bValid)
        {
            fprintf(pF, "\t%-8s %s\r\n",
                    atR[i].szMnemonic, atR[i].szOperands);
        }
        else
        {
            fprintf(pF, "\t%-8s %s\r\n",
                    atR[i].szMnemonic, atR[i].szOperands);
        }
    }
    fprintf(pF, "\r\n\tEND\r\n");
    fclose(pF);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------ */
/* Helper: read PRG .text from a binary file                            */
/* ------------------------------------------------------------------ */

/*
 * Load a raw .PRG file and return a heap-allocated copy of the .text
 * section. Caller must free *ppText. *puiTextLen receives the size.
 * Returns ST_ERROR if the file is malformed or cannot be opened.
 */
static st_error_t load_prg_text(const char *szPath,
                                  st_u8_t   **ppText,
                                  size_t     *puiTextLen)
{
    FILE     *pF;
    st_u8_t   aHdr[28];
    st_u32_t  uiTextSz;
    st_u8_t  *pText;
    size_t    uiRead;

    *ppText     = NULL;
    *puiTextLen = 0u;

    pF = fopen(szPath, "rb");
    if (pF == NULL) { LOG_ERROR("cannot open '%s'", szPath); return ST_ERROR; }

    if (fread(aHdr, 1u, 28u, pF) != 28u)
    { fclose(pF); return ST_ERROR; }

    /* Verify magic 0x601A */
    if (aHdr[0] != 0x60 || aHdr[1] != 0x1A)
    { fclose(pF); return ST_ERROR; }

    uiTextSz = ((st_u32_t)aHdr[2] << 24) | ((st_u32_t)aHdr[3] << 16)
             | ((st_u32_t)aHdr[4] <<  8) |  (st_u32_t)aHdr[5];

    pText = (st_u8_t *)malloc(uiTextSz + 1u);
    if (pText == NULL) { fclose(pF); return ST_ERROR; }

    uiRead = fread(pText, 1u, uiTextSz, pF);
    fclose(pF);

    if (uiRead != uiTextSz) { free(pText); return ST_ERROR; }

    *ppText     = pText;
    *puiTextLen = uiTextSz;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------ */
/* Test: byte-exact match against the real ATARI ST PRG                 */
/* ------------------------------------------------------------------ */

static void test_real_prg_match(void)
{
    as_context_t  tCtx;
    st_u8_t      *pRealText;
    size_t        uiRealLen;
    st_u32_t      uiOurTextSz;
    const st_u8_t *pOurText;
    const char   *szSrc  = "use_cases/UC30B/test30b.S";
    const char   *szOut  = "use_cases/UC30B/test30b_ours.prg";
    const char   *szReal = "use_cases/UC30B/TEST30B.PRG";

    printf("\n[UC30B] Real ATARI ST PRG vs our assembler output\n");

    /* INTENT[INT-AS-040 -> TC-AS-110 -> REQ-AS-003]:
     * Our assembled .text is byte-identical to the binary produced by
     * the real DEVPAC3 assembler on an ATARI ST.
     * TEST30B.PRG must be assembled on a real ATARI ST and copied to
     * use_cases/UC30B/TEST30B.PRG.  Skip gracefully if absent. */
    {
        FILE *pChk = fopen(szReal, "rb");
        if (pChk == NULL)
        {
            printf("  [SKIP] [N] load TEST30B.PRG — copy from ATARI ST\n");
            printf("  [SKIP] [N] .text size matches real PRG\n");
            printf("  [SKIP] [N] .text bytes identical to real ATARI ST PRG\n");
            return;
        }
        fclose(pChk);
    }

    /* Load the real PRG .text */
    TEST_ASSERT("[N] load TEST30B.PRG .text",
        load_prg_text(szReal, &pRealText, &uiRealLen) == ST_NO_ERROR);
    if (pRealText == NULL) return;

    /* Assemble with our assembler */
    TEST_ASSERT("[N] assemble test30b.S OK",
        run_assemble(&tCtx, szSrc, szOut, ST_TRUE) == ST_NO_ERROR);

    if (tCtx.pBinary != NULL)
    {
        uiOurTextSz = ((st_u32_t)tCtx.pBinary[2] << 24)
                    | ((st_u32_t)tCtx.pBinary[3] << 16)
                    | ((st_u32_t)tCtx.pBinary[4] <<  8)
                    |  (st_u32_t)tCtx.pBinary[5];
        pOurText = tCtx.pBinary + 28u;

        TEST_ASSERT("[N] .text size matches real PRG (58 bytes)",
            uiOurTextSz == (st_u32_t)uiRealLen);
        TEST_ASSERT("[N] .text bytes identical to real ATARI ST PRG",
            uiOurTextSz == (st_u32_t)uiRealLen &&
            memcmp(pOurText, pRealText, uiRealLen) == 0);
    }

    free(pRealText);
    as_shutdown(&tCtx);
    cleanup_prg(szOut);
}

/* ------------------------------------------------------------------ */
/* Test: CRLF source file is assembled identically to LF source         */
/* ------------------------------------------------------------------ */

static void test_crlf_compat(void)
{
    as_context_t tCtxLF;
    as_context_t tCtxCR;
    FILE        *pF;
    const char  *szSrc   = "use_cases/UC30B/test30b.S";
    const char  *szOutLF = "use_cases/UC30B/test30b_lf.prg";
    const char  *szOutCR = "use_cases/UC30B/test30b_crlf.prg";
    const char  *szCRLF  = "use_cases/UC30B/test30b_crlf.S";
    char         szLineBuf[256];
    FILE        *pIn;

    printf("\n[UC30B] CRLF source produces identical binary to LF source\n");

    /* Create a CRLF copy of test30b.S */
    pIn = fopen(szSrc, "r");
    pF  = fopen(szCRLF, "wb");
    if (pIn && pF)
    {
        while (fgets(szLineBuf, 256, pIn) != NULL)
        {
            size_t n = strlen(szLineBuf);
            /* Strip existing newline, write CRLF */
            while (n > 0 && (szLineBuf[n-1] == '\n' || szLineBuf[n-1] == '\r'))
                szLineBuf[--n] = '\0';
            fwrite(szLineBuf, 1u, n, pF);
            fwrite("\r\n", 1u, 2u, pF);
        }
    }
    if (pIn) fclose(pIn);
    if (pF)  fclose(pF);

    /* Assemble LF version */
    TEST_ASSERT("[N] assemble LF source OK",
        run_assemble(&tCtxLF, szSrc, szOutLF, ST_TRUE) == ST_NO_ERROR);

    /* Assemble CRLF version */
    TEST_ASSERT("[N] assemble CRLF source OK",
        run_assemble(&tCtxCR, szCRLF, szOutCR, ST_TRUE) == ST_NO_ERROR);

    /* INTENT[INT-AS-041 -> TC-AS-111 -> REQ-AS-003]:
     * CRLF and LF sources produce bit-identical binary output */
    TEST_ASSERT("[N] CRLF binary == LF binary",
        tCtxLF.pBinary != NULL && tCtxCR.pBinary != NULL &&
        tCtxLF.uiBinaryLen == tCtxCR.uiBinaryLen &&
        memcmp(tCtxLF.pBinary, tCtxCR.pBinary, tCtxLF.uiBinaryLen) == 0);

    as_shutdown(&tCtxLF);
    as_shutdown(&tCtxCR);
    cleanup_prg(szOutLF);
    cleanup_prg(szOutCR);
    remove(szCRLF);
}

/* ------------------------------------------------------------------ */
/* Test: round-trip PRG -> disasm -> .S (CRLF) -> reassemble -> compare */
/* ------------------------------------------------------------------ */

static void test_roundtrip(void)
{
    as_context_t  tCtxA;
    as_context_t  tCtxB;
    const st_u8_t *pTextA;
    const st_u8_t *pTextB;
    st_u32_t       uiSzA;
    st_u32_t       uiSzB;
    const char    *szSrc  = "use_cases/UC30B/test30b.S";
    const char    *szOutA = "use_cases/UC30B/test30b_a.prg";
    const char    *szRT   = "use_cases/UC30B/test30b_rt.S";
    const char    *szOutB = "use_cases/UC30B/test30b_b.prg";

    printf("\n[UC30B] Round-trip: .S -> PRG -> disasm -> CRLF .S -> PRG\n");

    /* Step 1: assemble test30b.S -> A */
    TEST_ASSERT("[N] round-trip: assemble original",
        run_assemble(&tCtxA, szSrc, szOutA, ST_TRUE) == ST_NO_ERROR);
    if (tCtxA.pBinary == NULL)
    { as_shutdown(&tCtxA); return; }

    uiSzA  = ((st_u32_t)tCtxA.pBinary[2] << 24)
           | ((st_u32_t)tCtxA.pBinary[3] << 16)
           | ((st_u32_t)tCtxA.pBinary[4] <<  8)
           |  (st_u32_t)tCtxA.pBinary[5];
    pTextA = tCtxA.pBinary + 28u;

    /* Step 2: disassemble .text section -> DEVPAC3 .S with CRLF */
    TEST_ASSERT("[N] round-trip: disassemble to .S",
        disasm_write_s(pTextA, (size_t)uiSzA, szRT) == ST_NO_ERROR);

    /* Step 3: reassemble the disassembled source -> B */
    TEST_ASSERT("[N] round-trip: reassemble disassembled .S",
        run_assemble(&tCtxB, szRT, szOutB, ST_TRUE) == ST_NO_ERROR);
    if (tCtxB.pBinary == NULL)
    { as_shutdown(&tCtxA); as_shutdown(&tCtxB); return; }

    uiSzB  = ((st_u32_t)tCtxB.pBinary[2] << 24)
           | ((st_u32_t)tCtxB.pBinary[3] << 16)
           | ((st_u32_t)tCtxB.pBinary[4] <<  8)
           |  (st_u32_t)tCtxB.pBinary[5];
    pTextB = tCtxB.pBinary + 28u;

    /* INTENT[INT-AS-042 -> TC-AS-112 -> REQ-AS-003]:
     * Disassembling our binary and reassembling produces the same .text.
     * Validates assembler↔disassembler fidelity: the loop is closed. */
    TEST_ASSERT("[N] round-trip: .text size preserved",
        uiSzA == uiSzB);
    TEST_ASSERT("[N] round-trip: .text bytes identical (loop closed)",
        uiSzA == uiSzB && memcmp(pTextA, pTextB, (size_t)uiSzA) == 0);

    as_shutdown(&tCtxA);
    as_shutdown(&tCtxB);
    cleanup_prg(szOutA);
    cleanup_prg(szOutB);
    /* Keep test30b_rt.S: useful reference for ATARI ST DEVPAC3 validation */
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC30B: DEVPAC3 EA Encoder + MOVE/MOVEQ/LEA/CLR/SWAP ===\n");
    printf("Testing: as_encode_move / as_encode_moveq / as_encode_lea\n");
    printf("         as_encode_clr / as_encode_swap / as_parse_ea\n\n");

    test_null_params();
    test_unknown_mnemonic();
    test_moveq_to_an();
    test_lea_to_dn();
    test_init_shutdown_cycle();
    test_instruction_encodings();
    test_real_prg_match();
    test_crlf_compat();
    test_roundtrip();

    printf("\n=== UC30B Results: %d failure(s) ===\n\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
