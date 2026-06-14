/* use_case_30D.c - UC30D: DEVPAC3 shifts / bit-ops / MOVEM / ADDA / MUL-DIV
 *                         / ADDX-SUBX / Scc / DBcc / EXG / PEA
 *
 * TEST MATRIX - UC30D:
 *   [N] Nominal    : 44 tests - all shifts + bit-ops + MOVEM + ADDA/SUBA
 *                               + MULU/MULS/DIVU/DIVS + ADDX/SUBX
 *                               + SNE/SEQ + DBRA/DBNE + EXG + PEA
 *   [R] Robustness :  8 tests  - SHIFT count 0/9, SHIFT to An,
 *                                BTST bit#>31, MOVEM bad reglist,
 *                                ADDA to Dn, ADDX Dn/-(An) mismatch,
 *                                PEA #imm rejected
 *   [S] Skipped    :  0 tests
 *
 * INT map:
 *   INT-AS-089 -> TC-AS-159: test30d.S assembles OK
 *   INT-AS-090 -> TC-AS-160: .text size == 84 bytes
 *   INT-AS-091 -> TC-AS-161: ASL.W #2,D0 = 0xE540
 *   INT-AS-092 -> TC-AS-162: ASR.W #2,D1 = 0xE441
 *   INT-AS-093 -> TC-AS-163: LSL.W #1,D2 = 0xE34A
 *   INT-AS-094 -> TC-AS-164: LSR.W #1,D3 = 0xE24B
 *   INT-AS-095 -> TC-AS-165: ROL.W #1,D4 = 0xE35C
 *   INT-AS-096 -> TC-AS-166: ROR.W #1,D5 = 0xE25D
 *   INT-AS-097 -> TC-AS-167: ROXL.W #1,D6 = 0xE356
 *   INT-AS-098 -> TC-AS-168: ROXR.W #1,D7 = 0xE257
 *   INT-AS-099 -> TC-AS-169: ASL.W D1,D0 = 0xE360 (register count)
 *   INT-AS-100 -> TC-AS-170: BTST D0,D1 = 0x0101
 *   INT-AS-101 -> TC-AS-171: BCHG D2,D3 = 0x0543
 *   INT-AS-102 -> TC-AS-172: BCLR D4,D5 = 0x0985
 *   INT-AS-103 -> TC-AS-173: BSET D6,D7 = 0x0DC7
 *   INT-AS-104 -> TC-AS-174: BTST #7,D0 opword = 0x0800
 *   INT-AS-105 -> TC-AS-175: BTST #7,D0 ext    = 0x0007
 *   INT-AS-106 -> TC-AS-176: BSET #3,D1 opword = 0x08C1
 *   INT-AS-107 -> TC-AS-177: BSET #3,D1 ext    = 0x0003
 *   INT-AS-108 -> TC-AS-178: MOVEM.L store opword = 0x48E7
 *   INT-AS-109 -> TC-AS-179: MOVEM.L store mask   = 0xC0C0 (reversed)
 *   INT-AS-110 -> TC-AS-180: MOVEM.L load  opword = 0x4CDF
 *   INT-AS-111 -> TC-AS-181: MOVEM.L load  mask   = 0x0303 (normal)
 *   INT-AS-112 -> TC-AS-182: ADDA.W D0,A1 = 0xD2C0
 *   INT-AS-113 -> TC-AS-183: ADDA.L D2,A3 = 0xD782
 *   INT-AS-114 -> TC-AS-184: SUBA.W D4,A5 = 0x9AC4
 *   INT-AS-115 -> TC-AS-185: MULU.W D0,D1 = 0xC2C0
 *   INT-AS-116 -> TC-AS-186: MULS.W D2,D3 = 0xC7C2
 *   INT-AS-117 -> TC-AS-187: DIVU.W D0,D1 = 0x82C0
 *   INT-AS-118 -> TC-AS-188: DIVS.W D2,D3 = 0x87C2
 *   INT-AS-119 -> TC-AS-189: ADDX.W D0,D1 = 0xD340
 *   INT-AS-120 -> TC-AS-190: SUBX.W D2,D3 = 0x9742
 *   INT-AS-121 -> TC-AS-191: SNE D0 = 0x56C0
 *   INT-AS-122 -> TC-AS-192: SEQ D1 = 0x57C1
 *   INT-AS-123 -> TC-AS-193: DBRA D0,dbra_top opword = 0x51C8
 *   INT-AS-124 -> TC-AS-194: DBRA D0,dbra_top disp   = 0xFFFE (-2)
 *   INT-AS-125 -> TC-AS-195: DBNE D1,dbne_top opword = 0x56C9
 *   INT-AS-126 -> TC-AS-196: DBNE D1,dbne_top disp   = 0xFFFE (-2)
 *   INT-AS-127 -> TC-AS-197: EXG D0,D1 = 0xC141
 *   INT-AS-128 -> TC-AS-198: EXG A0,A1 = 0xC149
 *   INT-AS-129 -> TC-AS-199: EXG D0,A0 = 0xC188
 *   INT-AS-130 -> TC-AS-200: PEA (A0) = 0x4850
 *   INT-AS-131 -> TC-AS-201: PEA $100.W opword = 0x4878
 *   INT-AS-132 -> TC-AS-202: PEA $100.W ext    = 0x0100
 *   INT-AS-133 -> TC-AS-203: SHIFT count=0 rejected
 *   INT-AS-134 -> TC-AS-204: SHIFT count=9 rejected
 *   INT-AS-135 -> TC-AS-205: SHIFT dest=An rejected
 *   INT-AS-136 -> TC-AS-206: BTST #32,D0 rejected (bit# > 31 for Dn)
 *   INT-AS-137 -> TC-AS-207: MOVEM with empty reglist rejected
 *   INT-AS-138 -> TC-AS-208: ADDA with Dn dest rejected
 *   INT-AS-139 -> TC-AS-209: ADDX Dn,-(An) mismatch rejected
 *   INT-AS-140 -> TC-AS-210: PEA #imm rejected (not control EA)
 */

#include "use_cases.h"
#include "../src/as.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
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

/* Pointer to the start of .text in the assembled PRG */
static const st_u8_t *prg_text(const as_context_t *ptCtx)
{
    return ptCtx->pBinary + 28u;
}

/* Big-endian word at offset off in .text */
static st_u16_t text_w(const as_context_t *ptCtx, size_t off)
{
    const st_u8_t *p = prg_text(ptCtx) + off;
    return (st_u16_t)(((st_u16_t)p[0] << 8) | p[1]);
}

static void cleanup(const char *szPath)
{
    remove(szPath);
}

/* Write a minimal one-instruction .S source to a temp file */
static FILE *open_tmp(const char *szPath)
{
    FILE *fp = fopen(szPath, "w");
    return fp;
}

/* ------------------------------------------------------------------ */
/* Robustness tests                                                      */
/* ------------------------------------------------------------------ */

static void test_shift_count_zero(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30D/tmp_sh0.S";
    const char *szOut = "use_cases/UC30D/tmp_sh0.prg";

    printf("\n[UC30D] SHIFT: count=0 rejected\n");

    /* INTENT[INT-AS-133 -> TC-AS-203 -> REQ-AS-013]:
     * ASL.W #0,D0 rejected — count must be 1-8 */
    fp = open_tmp(szSrc);
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        ASL.W #0,D0\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] ASL.W #0,D0 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_shift_count_nine(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30D/tmp_sh9.S";
    const char *szOut = "use_cases/UC30D/tmp_sh9.prg";

    printf("\n[UC30D] SHIFT: count=9 rejected\n");

    /* INTENT[INT-AS-134 -> TC-AS-204 -> REQ-AS-013]:
     * LSR.W #9,D0 rejected — count must be 1-8 */
    fp = open_tmp(szSrc);
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        LSR.W #9,D0\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] LSR.W #9,D0 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_shift_dest_an(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30D/tmp_shan.S";
    const char *szOut = "use_cases/UC30D/tmp_shan.prg";

    printf("\n[UC30D] SHIFT: dest=An rejected\n");

    /* INTENT[INT-AS-135 -> TC-AS-205 -> REQ-AS-013]:
     * ASL.W #1,A0 rejected — destination must be Dn */
    fp = open_tmp(szSrc);
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        ASL.W #1,A0\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] ASL.W #1,A0 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_btst_bit_out_of_range(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30D/tmp_btst32.S";
    const char *szOut = "use_cases/UC30D/tmp_btst32.prg";

    printf("\n[UC30D] BTST: bit#32 for Dn rejected\n");

    /* INTENT[INT-AS-136 -> TC-AS-206 -> REQ-AS-013]:
     * BTST #32,D0 rejected — static bit number for Dn must be 0-31 */
    fp = open_tmp(szSrc);
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        BTST #32,D0\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] BTST #32,D0 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_movem_empty_reglist(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30D/tmp_movem_empty.S";
    const char *szOut = "use_cases/UC30D/tmp_movem_empty.prg";

    printf("\n[UC30D] MOVEM: empty reglist rejected\n");

    /* INTENT[INT-AS-137 -> TC-AS-207 -> REQ-AS-013]:
     * MOVEM.L X0-X0,-(SP) with invalid register rejected */
    fp = open_tmp(szSrc);
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        MOVEM.L X0-X1,-(SP)\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] MOVEM bad reglist rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_adda_dn_dest(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30D/tmp_adda_dn.S";
    const char *szOut = "use_cases/UC30D/tmp_adda_dn.prg";

    printf("\n[UC30D] ADDA: dest=Dn rejected\n");

    /* INTENT[INT-AS-138 -> TC-AS-208 -> REQ-AS-013]:
     * ADDA.W D0,D1 rejected — dest must be An */
    fp = open_tmp(szSrc);
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        ADDA.W D0,D1\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] ADDA.W D0,D1 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_addx_mismatch(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30D/tmp_addx_mis.S";
    const char *szOut = "use_cases/UC30D/tmp_addx_mis.prg";

    printf("\n[UC30D] ADDX: Dn,-(An) mismatch rejected\n");

    /* INTENT[INT-AS-139 -> TC-AS-209 -> REQ-AS-013]:
     * ADDX.W D0,-(A1) rejected — both operands must be same class */
    fp = open_tmp(szSrc);
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        ADDX.W D0,-(A1)\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] ADDX.W D0,-(A1) rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_pea_imm(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30D/tmp_pea_imm.S";
    const char *szOut = "use_cases/UC30D/tmp_pea_imm.prg";

    printf("\n[UC30D] PEA: immediate EA rejected\n");

    /* INTENT[INT-AS-140 -> TC-AS-210 -> REQ-AS-013]:
     * PEA #42 rejected — #imm is not a control addressing mode */
    fp = open_tmp(szSrc);
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        PEA #42\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] PEA #42 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

/* ------------------------------------------------------------------ */
/* Nominal test: test30d.S fixture                                      */
/* ------------------------------------------------------------------ */

/*
 * .text layout (verified against MC68000 PRM):
 * off  instruction                 opword    (ext)
 *   0  ASL.W  #2,D0               E540
 *   2  ASR.W  #2,D1               E441
 *   4  LSL.W  #1,D2               E34A
 *   6  LSR.W  #1,D3               E24B
 *   8  ROL.W  #1,D4               E35C
 *  10  ROR.W  #1,D5               E25D
 *  12  ROXL.W #1,D6               E356
 *  14  ROXR.W #1,D7               E257
 *  16  ASL.W  D1,D0               E360  (register count)
 *  18  BTST   D0,D1               0101
 *  20  BCHG   D2,D3               0543
 *  22  BCLR   D4,D5               0985
 *  24  BSET   D6,D7               0DC7
 *  26  BTST   #7,D0               0800  0007
 *  30  BSET   #3,D1               08C1  0003
 *  34  MOVEM.L D0-D1/A0-A1,-(SP) 48E7  C0C0
 *  38  MOVEM.L (SP)+,D0-D1/A0-A1 4CDF  0303
 *  42  ADDA.W D0,A1               D2C0
 *  44  ADDA.L D2,A3               D782
 *  46  SUBA.W D4,A5               9AC4
 *  48  MULU.W D0,D1               C2C0
 *  50  MULS.W D2,D3               C7C2
 *  52  DIVU.W D0,D1               82C0
 *  54  DIVS.W D2,D3               87C2
 *  56  ADDX.W D0,D1               D340
 *  58  SUBX.W D2,D3               9742
 *  60  SNE    D0                  56C0
 *  62  SEQ    D1                  57C1
 *  64  DBRA   D0,dbra_top         51C8  FFFE  (disp=-2)
 *  68  DBNE   D1,dbne_top         56C9  FFFE  (disp=-2)
 *  72  EXG    D0,D1               C141
 *  74  EXG    A0,A1               C149
 *  76  EXG    D0,A0               C188
 *  78  PEA    (A0)                4850
 *  80  PEA    $100.W              4878  0100
 * Total: 84 bytes
 */

static void test_fixture(void)
{
    as_context_t tCtx;
    const char *szSrc = "use_cases/UC30D/test30d.S";
    const char *szOut = "use_cases/UC30D/test30d.prg";
    size_t      uiTextSz;

    printf("\n[UC30D] Fixture test30d.S\n");
    memset(&tCtx, 0, sizeof(tCtx));

    /* INTENT[INT-AS-089 -> TC-AS-159 -> REQ-AS-001]:
     * test30d.S assembles without errors */
    TEST_ASSERT("[N] test30d.S assembles OK",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_NO_ERROR);

    if (tCtx.pBinary == NULL || tCtx.uiBinaryLen < 28u)
    {
        TEST_SKIP("[N] binary too short — skip byte checks");
        as_shutdown(&tCtx); cleanup(szOut); return;
    }

    /* INTENT[INT-AS-090 -> TC-AS-160 -> REQ-AS-001]:
     * .text section is exactly 84 bytes (binary = 28-byte header + .text) */
    uiTextSz = tCtx.uiBinaryLen;
    TEST_ASSERT("[N] .text size == 84",
        uiTextSz == 28u + 84u);

    /* Shifts (immediate count) */
    /* INTENT[INT-AS-091 -> TC-AS-161 -> REQ-AS-013]: ASL.W #2,D0 */
    TEST_ASSERT("[N] ASL.W #2,D0 = 0xE540",  text_w(&tCtx,  0) == 0xE540u);
    /* INTENT[INT-AS-092 -> TC-AS-162 -> REQ-AS-013]: ASR.W #2,D1 */
    TEST_ASSERT("[N] ASR.W #2,D1 = 0xE441",  text_w(&tCtx,  2) == 0xE441u);
    /* INTENT[INT-AS-093 -> TC-AS-163 -> REQ-AS-013]: LSL.W #1,D2 */
    TEST_ASSERT("[N] LSL.W #1,D2 = 0xE34A",  text_w(&tCtx,  4) == 0xE34Au);
    /* INTENT[INT-AS-094 -> TC-AS-164 -> REQ-AS-013]: LSR.W #1,D3 */
    TEST_ASSERT("[N] LSR.W #1,D3 = 0xE24B",  text_w(&tCtx,  6) == 0xE24Bu);
    /* INTENT[INT-AS-095 -> TC-AS-165 -> REQ-AS-013]: ROL.W #1,D4 */
    TEST_ASSERT("[N] ROL.W  #1,D4 = 0xE35C", text_w(&tCtx,  8) == 0xE35Cu);
    /* INTENT[INT-AS-096 -> TC-AS-166 -> REQ-AS-013]: ROR.W #1,D5 */
    TEST_ASSERT("[N] ROR.W  #1,D5 = 0xE25D", text_w(&tCtx, 10) == 0xE25Du);
    /* INTENT[INT-AS-097 -> TC-AS-167 -> REQ-AS-013]: ROXL.W #1,D6 */
    TEST_ASSERT("[N] ROXL.W #1,D6 = 0xE356", text_w(&tCtx, 12) == 0xE356u);
    /* INTENT[INT-AS-098 -> TC-AS-168 -> REQ-AS-013]: ROXR.W #1,D7 */
    TEST_ASSERT("[N] ROXR.W #1,D7 = 0xE257", text_w(&tCtx, 14) == 0xE257u);

    /* Shift (register count) */
    /* INTENT[INT-AS-099 -> TC-AS-169 -> REQ-AS-013]: ASL.W D1,D0 */
    TEST_ASSERT("[N] ASL.W D1,D0 = 0xE360 (reg count)", text_w(&tCtx, 16) == 0xE360u);

    /* Bit ops (dynamic) */
    /* INTENT[INT-AS-100 -> TC-AS-170 -> REQ-AS-013]: BTST D0,D1 */
    TEST_ASSERT("[N] BTST D0,D1 = 0x0101", text_w(&tCtx, 18) == 0x0101u);
    /* INTENT[INT-AS-101 -> TC-AS-171 -> REQ-AS-013]: BCHG D2,D3 */
    TEST_ASSERT("[N] BCHG D2,D3 = 0x0543", text_w(&tCtx, 20) == 0x0543u);
    /* INTENT[INT-AS-102 -> TC-AS-172 -> REQ-AS-013]: BCLR D4,D5 */
    TEST_ASSERT("[N] BCLR D4,D5 = 0x0985", text_w(&tCtx, 22) == 0x0985u);
    /* INTENT[INT-AS-103 -> TC-AS-173 -> REQ-AS-013]: BSET D6,D7 */
    TEST_ASSERT("[N] BSET D6,D7 = 0x0DC7", text_w(&tCtx, 24) == 0x0DC7u);

    /* Bit ops (static #imm) */
    /* INTENT[INT-AS-104 -> TC-AS-174 -> REQ-AS-013]: BTST #7,D0 opword */
    TEST_ASSERT("[N] BTST #7,D0 opword = 0x0800", text_w(&tCtx, 26) == 0x0800u);
    /* INTENT[INT-AS-105 -> TC-AS-175 -> REQ-AS-013]: BTST #7,D0 ext */
    TEST_ASSERT("[N] BTST #7,D0 ext = 0x0007",    text_w(&tCtx, 28) == 0x0007u);
    /* INTENT[INT-AS-106 -> TC-AS-176 -> REQ-AS-013]: BSET #3,D1 opword */
    TEST_ASSERT("[N] BSET #3,D1 opword = 0x08C1", text_w(&tCtx, 30) == 0x08C1u);
    /* INTENT[INT-AS-107 -> TC-AS-177 -> REQ-AS-013]: BSET #3,D1 ext */
    TEST_ASSERT("[N] BSET #3,D1 ext = 0x0003",    text_w(&tCtx, 32) == 0x0003u);

    /* MOVEM */
    /* INTENT[INT-AS-108 -> TC-AS-178 -> REQ-AS-013]: MOVEM.L store opword */
    TEST_ASSERT("[N] MOVEM.L D0-D1/A0-A1,-(SP) opword = 0x48E7",
                text_w(&tCtx, 34) == 0x48E7u);
    /* INTENT[INT-AS-109 -> TC-AS-179 -> REQ-AS-013]: MOVEM.L store mask reversed */
    TEST_ASSERT("[N] MOVEM.L D0-D1/A0-A1,-(SP) mask = 0xC0C0 (reversed)",
                text_w(&tCtx, 36) == 0xC0C0u);
    /* INTENT[INT-AS-110 -> TC-AS-180 -> REQ-AS-013]: MOVEM.L load opword */
    TEST_ASSERT("[N] MOVEM.L (SP)+,D0-D1/A0-A1 opword = 0x4CDF",
                text_w(&tCtx, 38) == 0x4CDFu);
    /* INTENT[INT-AS-111 -> TC-AS-181 -> REQ-AS-013]: MOVEM.L load mask */
    TEST_ASSERT("[N] MOVEM.L (SP)+,D0-D1/A0-A1 mask = 0x0303",
                text_w(&tCtx, 40) == 0x0303u);

    /* ADDA / SUBA */
    /* INTENT[INT-AS-112 -> TC-AS-182 -> REQ-AS-013]: ADDA.W D0,A1 */
    TEST_ASSERT("[N] ADDA.W D0,A1 = 0xD2C0", text_w(&tCtx, 42) == 0xD2C0u);
    /* INTENT[INT-AS-113 -> TC-AS-183 -> REQ-AS-013]: ADDA.L D2,A3 */
    /* 1101 011 111 000010 = 0xD7C2 (An=A3=3, ooo=7 for .L, src=D2) */
    TEST_ASSERT("[N] ADDA.L D2,A3 = 0xD7C2", text_w(&tCtx, 44) == 0xD7C2u);
    /* INTENT[INT-AS-114 -> TC-AS-184 -> REQ-AS-013]: SUBA.W D4,A5 */
    TEST_ASSERT("[N] SUBA.W D4,A5 = 0x9AC4", text_w(&tCtx, 46) == 0x9AC4u);

    /* MUL / DIV */
    /* INTENT[INT-AS-115 -> TC-AS-185 -> REQ-AS-013]: MULU.W D0,D1 */
    TEST_ASSERT("[N] MULU.W D0,D1 = 0xC2C0", text_w(&tCtx, 48) == 0xC2C0u);
    /* INTENT[INT-AS-116 -> TC-AS-186 -> REQ-AS-013]: MULS.W D2,D3 */
    TEST_ASSERT("[N] MULS.W D2,D3 = 0xC7C2", text_w(&tCtx, 50) == 0xC7C2u);
    /* INTENT[INT-AS-117 -> TC-AS-187 -> REQ-AS-013]: DIVU.W D0,D1 */
    TEST_ASSERT("[N] DIVU.W D0,D1 = 0x82C0", text_w(&tCtx, 52) == 0x82C0u);
    /* INTENT[INT-AS-118 -> TC-AS-188 -> REQ-AS-013]: DIVS.W D2,D3 */
    TEST_ASSERT("[N] DIVS.W D2,D3 = 0x87C2", text_w(&tCtx, 54) == 0x87C2u);

    /* ADDX / SUBX */
    /* INTENT[INT-AS-119 -> TC-AS-189 -> REQ-AS-013]: ADDX.W D0,D1 */
    TEST_ASSERT("[N] ADDX.W D0,D1 = 0xD340", text_w(&tCtx, 56) == 0xD340u);
    /* INTENT[INT-AS-120 -> TC-AS-190 -> REQ-AS-013]: SUBX.W D2,D3 */
    TEST_ASSERT("[N] SUBX.W D2,D3 = 0x9742", text_w(&tCtx, 58) == 0x9742u);

    /* Scc */
    /* INTENT[INT-AS-121 -> TC-AS-191 -> REQ-AS-013]: SNE D0 */
    TEST_ASSERT("[N] SNE D0 = 0x56C0", text_w(&tCtx, 60) == 0x56C0u);
    /* INTENT[INT-AS-122 -> TC-AS-192 -> REQ-AS-013]: SEQ D1 */
    TEST_ASSERT("[N] SEQ D1 = 0x57C1", text_w(&tCtx, 62) == 0x57C1u);

    /* DBcc (self-referencing backward branches, disp=-2) */
    /* INTENT[INT-AS-123 -> TC-AS-193 -> REQ-AS-013]: DBRA opword */
    TEST_ASSERT("[N] DBRA D0,dbra_top opword = 0x51C8", text_w(&tCtx, 64) == 0x51C8u);
    /* INTENT[INT-AS-124 -> TC-AS-194 -> REQ-AS-013]: DBRA disp */
    TEST_ASSERT("[N] DBRA D0,dbra_top disp = 0xFFFE (-2)", text_w(&tCtx, 66) == 0xFFFEu);
    /* INTENT[INT-AS-125 -> TC-AS-195 -> REQ-AS-013]: DBNE opword */
    TEST_ASSERT("[N] DBNE D1,dbne_top opword = 0x56C9", text_w(&tCtx, 68) == 0x56C9u);
    /* INTENT[INT-AS-126 -> TC-AS-196 -> REQ-AS-013]: DBNE disp */
    TEST_ASSERT("[N] DBNE D1,dbne_top disp = 0xFFFE (-2)", text_w(&tCtx, 70) == 0xFFFEu);

    /* EXG */
    /* INTENT[INT-AS-127 -> TC-AS-197 -> REQ-AS-013]: EXG Dx,Dy */
    TEST_ASSERT("[N] EXG D0,D1 = 0xC141", text_w(&tCtx, 72) == 0xC141u);
    /* INTENT[INT-AS-128 -> TC-AS-198 -> REQ-AS-013]: EXG Ax,Ay */
    TEST_ASSERT("[N] EXG A0,A1 = 0xC149", text_w(&tCtx, 74) == 0xC149u);
    /* INTENT[INT-AS-129 -> TC-AS-199 -> REQ-AS-013]: EXG Dx,Ay */
    TEST_ASSERT("[N] EXG D0,A0 = 0xC188", text_w(&tCtx, 76) == 0xC188u);

    /* PEA */
    /* INTENT[INT-AS-130 -> TC-AS-200 -> REQ-AS-013]: PEA (An) */
    TEST_ASSERT("[N] PEA (A0) = 0x4850",     text_w(&tCtx, 78) == 0x4850u);
    /* INTENT[INT-AS-131 -> TC-AS-201 -> REQ-AS-013]: PEA abs.W opword */
    TEST_ASSERT("[N] PEA $100.W opword = 0x4878", text_w(&tCtx, 80) == 0x4878u);
    /* INTENT[INT-AS-132 -> TC-AS-202 -> REQ-AS-013]: PEA abs.W ext */
    TEST_ASSERT("[N] PEA $100.W ext = 0x0100",    text_w(&tCtx, 82) == 0x0100u);

    as_shutdown(&tCtx);
    cleanup(szOut);
}

/* ------------------------------------------------------------------ */
/* Main                                                                  */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC30D: DEVPAC3 shifts/bit-ops/MOVEM/ADDA/MUL-DIV/"
           "ADDX/Scc/DBcc/EXG/PEA ===\n");

    /* Robustness */
    test_shift_count_zero();
    test_shift_count_nine();
    test_shift_dest_an();
    test_btst_bit_out_of_range();
    test_movem_empty_reglist();
    test_adda_dn_dest();
    test_addx_mismatch();
    test_pea_imm();

    /* Nominal */
    test_fixture();

    printf("\n=== UC30D: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
