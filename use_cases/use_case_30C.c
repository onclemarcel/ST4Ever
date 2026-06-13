/* use_case_30C.c - UC30C: DEVPAC3 ALU + control-flow instruction encoder
 *
 * TEST MATRIX - UC30C:
 *   [N] Nominal    : 45 tests - ADD/SUB/CMP/AND/OR/EOR + imm variants
 *                               + ADDQ/SUBQ + NEG/NOT/TST/EXT
 *                               + NOP/RTS/RTR/RTE/STOP/TRAP/JMP/JSR/LINK/UNLK
 *                               + BRA/BSR/Bcc(14) forward+backward
 *   [R] Robustness :  6 tests - NULL as_assemble, ALU no-Dn, EOR non-Dn,
 *                               ADDQ out of range, TRAP out of range,
 *                               branch undefined label
 *   [S] Skipped    :  0 tests
 *
 * INT map:
 *   INT-AS-043 -> TC-AS-113: as_assemble(NULL)
 *   INT-AS-044 -> TC-AS-114: ADD with no Dn operand
 *   INT-AS-045 -> TC-AS-115: EOR with non-Dn source
 *   INT-AS-046 -> TC-AS-116: ADDQ immediate out of range (#9)
 *   INT-AS-047 -> TC-AS-117: TRAP vector out of range (#16)
 *   INT-AS-048 -> TC-AS-118: branch undefined label
 *   INT-AS-049 -> TC-AS-119: test30c.S assembles OK
 *   INT-AS-050 -> TC-AS-120: .text size == 112 bytes
 *   INT-AS-051 -> TC-AS-121: ADD.W D0,D1 = 0xD240
 *   INT-AS-052 -> TC-AS-122: SUB.L D2,D3 = 0x9682
 *   INT-AS-053 -> TC-AS-123: AND.W D4,D5 = 0xCA44
 *   INT-AS-054 -> TC-AS-124: OR.W D0,D1 = 0x8240
 *   INT-AS-055 -> TC-AS-125: CMP.W D0,D1 = 0xB240
 *   INT-AS-056 -> TC-AS-126: ADD.W D0,(A1) direction=1 = 0xD151
 *   INT-AS-057 -> TC-AS-127: EOR.W D0,D1 = 0xB141
 *   INT-AS-058 -> TC-AS-128: ADDI.W #$1234,D0
 *   INT-AS-059 -> TC-AS-129: SUBI.W #$10,D1
 *   INT-AS-060 -> TC-AS-130: CMPI.L #$12345678,D0
 *   INT-AS-061 -> TC-AS-131: ANDI.B #$FF,D0
 *   INT-AS-062 -> TC-AS-132: ORI.W #$80,D0
 *   INT-AS-063 -> TC-AS-133: EORI.W #$FFFF,D0
 *   INT-AS-064 -> TC-AS-134: ADDQ.W #4,D0 = 0x5840
 *   INT-AS-065 -> TC-AS-135: ADDQ.W #8,D0 = 0x5040 (data=0 encodes 8)
 *   INT-AS-066 -> TC-AS-136: SUBQ.W #2,D0 = 0x5540
 *   INT-AS-067 -> TC-AS-137: NEG.W D0 = 0x4440
 *   INT-AS-068 -> TC-AS-138: NOT.W D0 = 0x4640
 *   INT-AS-069 -> TC-AS-139: TST.W D0 = 0x4A40
 *   INT-AS-070 -> TC-AS-140: EXT.W D0 = 0x4880
 *   INT-AS-071 -> TC-AS-141: EXT.L D0 = 0x48C0
 *   INT-AS-072 -> TC-AS-142: NOP = 0x4E71
 *   INT-AS-073 -> TC-AS-143: RTS = 0x4E75
 *   INT-AS-074 -> TC-AS-144: RTR = 0x4E77
 *   INT-AS-075 -> TC-AS-145: RTE = 0x4E73
 *   INT-AS-076 -> TC-AS-146: STOP #$2000 = 0x4E72 0x2000
 *   INT-AS-077 -> TC-AS-147: TRAP #1 = 0x4E41
 *   INT-AS-078 -> TC-AS-148: TRAP #14 = 0x4E4E
 *   INT-AS-079 -> TC-AS-149: JMP (A0) = 0x4ED0
 *   INT-AS-080 -> TC-AS-150: JSR (A0) = 0x4E90
 *   INT-AS-081 -> TC-AS-151: LINK A1,#-8 = 0x4E51 0xFFF8
 *   INT-AS-082 -> TC-AS-152: UNLK A1 = 0x4E59
 *   INT-AS-083 -> TC-AS-153: BRA forward: opword=0x6000, disp=2
 *   INT-AS-084 -> TC-AS-154: BSR forward: opword=0x6100, disp=2
 *   INT-AS-085 -> TC-AS-155: BEQ forward: opword=0x6700, disp=2
 *   INT-AS-086 -> TC-AS-156: BNE forward: opword=0x6600, disp=2
 *   INT-AS-087 -> TC-AS-157: BNE backward: opword=0x6600, disp=0xFFFC
 *   INT-AS-088 -> TC-AS-158: test30c_bcc.S: size=84, all 14 Bcc+aliases
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

/* ------------------------------------------------------------------ */
/* Robustness tests                                                      */
/* ------------------------------------------------------------------ */

static void test_null_assemble(void)
{
    printf("\n[UC30C] NULL parameter guard\n");

    /* INTENT[INT-AS-043 -> TC-AS-113 -> REQ-AS-001]:
     * as_assemble(NULL) -> ST_ERROR, no crash */
    TEST_ASSERT("[R] as_assemble(NULL)",
        as_assemble(NULL) == ST_ERROR);
}

static void test_add_no_dn(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30C/tmp_add_nodn.S";
    const char *szOut = "use_cases/UC30C/tmp_add_nodn.prg";

    printf("\n[UC30C] ALU: ADD with no Dn operand\n");

    /* INTENT[INT-AS-044 -> TC-AS-114 -> REQ-AS-013]:
     * ADD (A0),(A1) rejected — one operand must be Dn */
    fp = fopen(szSrc, "w");
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        ADD.W (A0),(A1)\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] ADD (A0),(A1) rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_eor_non_dn_source(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30C/tmp_eor_nodn.S";
    const char *szOut = "use_cases/UC30C/tmp_eor_nodn.prg";

    printf("\n[UC30C] EOR: non-Dn source rejected\n");

    /* INTENT[INT-AS-045 -> TC-AS-115 -> REQ-AS-013]:
     * EOR (A0),D1 rejected — EOR source must be Dn */
    fp = fopen(szSrc, "w");
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        EOR.W (A0),D1\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] EOR (A0),D1 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_addq_out_of_range(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30C/tmp_addq9.S";
    const char *szOut = "use_cases/UC30C/tmp_addq9.prg";

    printf("\n[UC30C] ADDQ: immediate out of range (#9)\n");

    /* INTENT[INT-AS-046 -> TC-AS-116 -> REQ-AS-016]:
     * ADDQ #9,D0 rejected — quick immediate must be 1-8 */
    fp = fopen(szSrc, "w");
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        ADDQ.W #9,D0\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] ADDQ #9,D0 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_trap_out_of_range(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30C/tmp_trap16.S";
    const char *szOut = "use_cases/UC30C/tmp_trap16.prg";

    printf("\n[UC30C] TRAP: vector out of range (#16)\n");

    /* INTENT[INT-AS-047 -> TC-AS-117 -> REQ-AS-017]:
     * TRAP #16 rejected — vector must be 0-15 */
    fp = fopen(szSrc, "w");
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        TRAP #16\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] TRAP #16 rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

static void test_branch_undefined_label(void)
{
    as_context_t tCtx;
    FILE *fp;
    const char *szSrc = "use_cases/UC30C/tmp_bra_undef.S";
    const char *szOut = "use_cases/UC30C/tmp_bra_undef.prg";

    printf("\n[UC30C] Branch: undefined label in pass 2\n");

    /* INTENT[INT-AS-048 -> TC-AS-118 -> REQ-AS-018]:
     * BRA undefined_label -> ST_ERROR in pass 2 */
    fp = fopen(szSrc, "w");
    if (!fp) { TEST_SKIP("[R] file write failed"); return; }
    fputs("        SECTION TEXT\n"
          "        BRA no_such_label\n"
          "        END\n", fp);
    fclose(fp);

    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[R] BRA undefined_label rejected",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_ERROR);

    as_shutdown(&tCtx);
    cleanup(szSrc); cleanup(szOut);
}

/* ------------------------------------------------------------------ */
/* Nominal tests — test30c.S byte-level verification                    */
/* ------------------------------------------------------------------ */

static as_context_t g_tCtx30C;

static void test_assemble_test30c(void)
{
    printf("\n[UC30C] test30c.S assembly\n");

    /* INTENT[INT-AS-049 -> TC-AS-119 -> REQ-AS-013..020]:
     * test30c.S assembles without errors */
    memset(&g_tCtx30C, 0, sizeof(g_tCtx30C));
    TEST_ASSERT("[N] test30c.S assembles OK",
        run_assemble(&g_tCtx30C,
                     "use_cases/UC30C/test30c.S",
                     "use_cases/UC30C/test30c.prg",
                     ST_FALSE) == ST_NO_ERROR);

    /* INTENT[INT-AS-050 -> TC-AS-120]:
     * .text section is exactly 112 bytes */
    TEST_ASSERT("[N] .text size == 112 bytes",
        g_tCtx30C.uiBinaryLen == 28u + 112u);
}

/* --- ALU binary --- */
static void test_alu_binary(void)
{
    const st_u8_t *p;

    printf("\n[UC30C] ALU binary encodings\n");
    if (g_tCtx30C.pBinary == NULL)
    { TEST_SKIP("[N] test30c not assembled"); return; }

    p = prg_text(&g_tCtx30C);

    /* INTENT[INT-AS-051 -> TC-AS-121]:
     * ADD.W D0,D1 -> 0xD240 (EA->Dn, D1, W, D0) */
    TEST_ASSERT("[N] ADD.W D0,D1 = 0xD240",
        p[0] == 0xD2 && p[1] == 0x40);

    /* INTENT[INT-AS-052 -> TC-AS-122]:
     * SUB.L D2,D3 -> 0x9682 (EA->Dn, D3, L, D2) */
    TEST_ASSERT("[N] SUB.L D2,D3 = 0x9682",
        p[2] == 0x96 && p[3] == 0x82);

    /* INTENT[INT-AS-053 -> TC-AS-123]:
     * AND.W D4,D5 -> 0xCA44 (EA->Dn, D5, W, D4) */
    TEST_ASSERT("[N] AND.W D4,D5 = 0xCA44",
        p[4] == 0xCA && p[5] == 0x44);

    /* INTENT[INT-AS-054 -> TC-AS-124]:
     * OR.W D0,D1 -> 0x8240 (EA->Dn, D1, W, D0) */
    TEST_ASSERT("[N] OR.W D0,D1 = 0x8240",
        p[6] == 0x82 && p[7] == 0x40);

    /* INTENT[INT-AS-055 -> TC-AS-125]:
     * CMP.W D0,D1 -> 0xB240 (EA->Dn, dir=0, D1, W, D0) */
    TEST_ASSERT("[N] CMP.W D0,D1 = 0xB240",
        p[8] == 0xB2 && p[9] == 0x40);

    /* INTENT[INT-AS-056 -> TC-AS-126]:
     * ADD.W D0,(A1) -> 0xD151 (Dn->EA, dir=1, D0, W, (A1)) */
    TEST_ASSERT("[N] ADD.W D0,(A1) = 0xD151",
        p[10] == 0xD1 && p[11] == 0x51);

    /* INTENT[INT-AS-057 -> TC-AS-127]:
     * EOR.W D0,D1 -> 0xB141 (dir=1 always, D0 src, W, D1) */
    TEST_ASSERT("[N] EOR.W D0,D1 = 0xB141",
        p[12] == 0xB1 && p[13] == 0x41);
}

/* --- Immediate ALU --- */
static void test_imm_alu(void)
{
    const st_u8_t *p;

    printf("\n[UC30C] Immediate ALU encodings\n");
    if (g_tCtx30C.pBinary == NULL)
    { TEST_SKIP("[N] test30c not assembled"); return; }

    p = prg_text(&g_tCtx30C);

    /* INTENT[INT-AS-058 -> TC-AS-128]:
     * ADDI.W #$1234,D0 -> 06 40 12 34 */
    TEST_ASSERT("[N] ADDI.W #$1234,D0 opword",
        p[14] == 0x06 && p[15] == 0x40);
    TEST_ASSERT("[N] ADDI.W #$1234,D0 immediate",
        p[16] == 0x12 && p[17] == 0x34);

    /* INTENT[INT-AS-059 -> TC-AS-129]:
     * SUBI.W #$10,D1 -> 04 41 00 10 */
    TEST_ASSERT("[N] SUBI.W #$10,D1 opword",
        p[18] == 0x04 && p[19] == 0x41);
    TEST_ASSERT("[N] SUBI.W #$10,D1 immediate",
        p[20] == 0x00 && p[21] == 0x10);

    /* INTENT[INT-AS-060 -> TC-AS-130]:
     * CMPI.L #$12345678,D0 -> 0C 80 12 34 56 78 */
    TEST_ASSERT("[N] CMPI.L opword",
        p[22] == 0x0C && p[23] == 0x80);
    TEST_ASSERT("[N] CMPI.L immediate hi-word",
        p[24] == 0x12 && p[25] == 0x34);
    TEST_ASSERT("[N] CMPI.L immediate lo-word",
        p[26] == 0x56 && p[27] == 0x78);

    /* INTENT[INT-AS-061 -> TC-AS-131]:
     * ANDI.B #$FF,D0 -> 02 00 00 FF */
    TEST_ASSERT("[N] ANDI.B opword",
        p[28] == 0x02 && p[29] == 0x00);
    TEST_ASSERT("[N] ANDI.B immediate (upper byte must be 0)",
        p[30] == 0x00 && p[31] == 0xFF);

    /* INTENT[INT-AS-062 -> TC-AS-132]:
     * ORI.W #$80,D0 -> 00 40 00 80 */
    TEST_ASSERT("[N] ORI.W opword",
        p[32] == 0x00 && p[33] == 0x40);
    TEST_ASSERT("[N] ORI.W immediate",
        p[34] == 0x00 && p[35] == 0x80);

    /* INTENT[INT-AS-063 -> TC-AS-133]:
     * EORI.W #$FFFF,D0 -> 0A 40 FF FF */
    TEST_ASSERT("[N] EORI.W opword",
        p[36] == 0x0A && p[37] == 0x40);
    TEST_ASSERT("[N] EORI.W immediate",
        p[38] == 0xFF && p[39] == 0xFF);
}

/* --- Quick + Unary --- */
static void test_quick_and_unary(void)
{
    const st_u8_t *p;

    printf("\n[UC30C] Quick and Unary encodings\n");
    if (g_tCtx30C.pBinary == NULL)
    { TEST_SKIP("[N] test30c not assembled"); return; }

    p = prg_text(&g_tCtx30C);

    /* INTENT[INT-AS-064 -> TC-AS-134]:
     * ADDQ.W #4,D0 -> 0x5840 (data=4 in bits 11-9) */
    TEST_ASSERT("[N] ADDQ.W #4,D0 = 0x5840",
        p[40] == 0x58 && p[41] == 0x40);

    /* INTENT[INT-AS-065 -> TC-AS-135]:
     * ADDQ.W #8,D0 -> 0x5040 (data=0 in bits 11-9 encodes 8) */
    TEST_ASSERT("[N] ADDQ.W #8,D0 = 0x5040",
        p[42] == 0x50 && p[43] == 0x40);

    /* INTENT[INT-AS-066 -> TC-AS-136]:
     * SUBQ.W #2,D0 -> 0x5540 (data=2, SUBQ=1) */
    TEST_ASSERT("[N] SUBQ.W #2,D0 = 0x5540",
        p[44] == 0x55 && p[45] == 0x40);

    /* INTENT[INT-AS-067 -> TC-AS-137]:
     * NEG.W D0 -> 0x4440 */
    TEST_ASSERT("[N] NEG.W D0 = 0x4440",
        p[46] == 0x44 && p[47] == 0x40);

    /* INTENT[INT-AS-068 -> TC-AS-138]:
     * NOT.W D0 -> 0x4640 */
    TEST_ASSERT("[N] NOT.W D0 = 0x4640",
        p[48] == 0x46 && p[49] == 0x40);

    /* INTENT[INT-AS-069 -> TC-AS-139]:
     * TST.W D0 -> 0x4A40 */
    TEST_ASSERT("[N] TST.W D0 = 0x4A40",
        p[50] == 0x4A && p[51] == 0x40);

    /* INTENT[INT-AS-070 -> TC-AS-140]:
     * EXT.W D0 -> 0x4880 (byte->word) */
    TEST_ASSERT("[N] EXT.W D0 = 0x4880",
        p[52] == 0x48 && p[53] == 0x80);

    /* INTENT[INT-AS-071 -> TC-AS-141]:
     * EXT.L D0 -> 0x48C0 (word->long) */
    TEST_ASSERT("[N] EXT.L D0 = 0x48C0",
        p[54] == 0x48 && p[55] == 0xC0);
}

/* --- Control flow single-word --- */
static void test_ctrl_implied(void)
{
    const st_u8_t *p;

    printf("\n[UC30C] Control flow encodings\n");
    if (g_tCtx30C.pBinary == NULL)
    { TEST_SKIP("[N] test30c not assembled"); return; }

    p = prg_text(&g_tCtx30C);

    /* INTENT[INT-AS-072 -> TC-AS-142]: NOP = 0x4E71 */
    TEST_ASSERT("[N] NOP = 0x4E71",
        p[56] == 0x4E && p[57] == 0x71);

    /* INTENT[INT-AS-073 -> TC-AS-143]: RTS = 0x4E75 */
    TEST_ASSERT("[N] RTS = 0x4E75",
        p[58] == 0x4E && p[59] == 0x75);

    /* INTENT[INT-AS-074 -> TC-AS-144]: RTR = 0x4E77 */
    TEST_ASSERT("[N] RTR = 0x4E77",
        p[60] == 0x4E && p[61] == 0x77);

    /* INTENT[INT-AS-075 -> TC-AS-145]: RTE = 0x4E73 */
    TEST_ASSERT("[N] RTE = 0x4E73",
        p[62] == 0x4E && p[63] == 0x73);

    /* INTENT[INT-AS-076 -> TC-AS-146]: STOP #$2000 = 4E 72 20 00 */
    TEST_ASSERT("[N] STOP opword = 0x4E72",
        p[64] == 0x4E && p[65] == 0x72);
    TEST_ASSERT("[N] STOP #$2000 SR value",
        p[66] == 0x20 && p[67] == 0x00);

    /* INTENT[INT-AS-077 -> TC-AS-147]: TRAP #1 = 0x4E41 */
    TEST_ASSERT("[N] TRAP #1 = 0x4E41",
        p[68] == 0x4E && p[69] == 0x41);

    /* INTENT[INT-AS-078 -> TC-AS-148]: TRAP #14 = 0x4E4E */
    TEST_ASSERT("[N] TRAP #14 = 0x4E4E",
        p[70] == 0x4E && p[71] == 0x4E);

    /* INTENT[INT-AS-079 -> TC-AS-149]: JMP (A0) = 0x4ED0 */
    TEST_ASSERT("[N] JMP (A0) = 0x4ED0",
        p[72] == 0x4E && p[73] == 0xD0);

    /* INTENT[INT-AS-080 -> TC-AS-150]: JSR (A0) = 0x4E90 */
    TEST_ASSERT("[N] JSR (A0) = 0x4E90",
        p[74] == 0x4E && p[75] == 0x90);

    /* INTENT[INT-AS-081 -> TC-AS-151]:
     * LINK A1,#-8 = 0x4E51 0xFFF8 */
    TEST_ASSERT("[N] LINK A1,#-8 opword = 0x4E51",
        p[76] == 0x4E && p[77] == 0x51);
    TEST_ASSERT("[N] LINK A1,#-8 displacement = 0xFFF8",
        p[78] == 0xFF && p[79] == 0xF8);

    /* INTENT[INT-AS-082 -> TC-AS-152]: UNLK A1 = 0x4E59 */
    TEST_ASSERT("[N] UNLK A1 = 0x4E59",
        p[80] == 0x4E && p[81] == 0x59);
}

/* --- Branch instructions --- */
static void test_branches(void)
{
    const st_u8_t *p;

    printf("\n[UC30C] Branch encodings\n");
    if (g_tCtx30C.pBinary == NULL)
    { TEST_SKIP("[N] test30c not assembled"); return; }

    p = prg_text(&g_tCtx30C);

    /* INTENT[INT-AS-083 -> TC-AS-153]:
     * BRA bra_tgt (long): opword=0x6000, disp=+2 (target@86, BRA@82) */
    TEST_ASSERT("[N] BRA forward opword = 0x6000",
        p[82] == 0x60 && p[83] == 0x00);
    TEST_ASSERT("[N] BRA forward displacement = +2",
        p[84] == 0x00 && p[85] == 0x02);

    /* INTENT[INT-AS-084 -> TC-AS-154]:
     * BSR bsr_tgt (long): opword=0x6100, disp=+2 (target@92, BSR@88) */
    TEST_ASSERT("[N] BSR forward opword = 0x6100",
        p[88] == 0x61 && p[89] == 0x00);
    TEST_ASSERT("[N] BSR forward displacement = +2",
        p[90] == 0x00 && p[91] == 0x02);

    /* INTENT[INT-AS-085 -> TC-AS-155]:
     * BEQ beq_tgt (long): opword=0x6700, disp=+2 (target@98, BEQ@94) */
    TEST_ASSERT("[N] BEQ forward opword = 0x6700",
        p[94] == 0x67 && p[95] == 0x00);
    TEST_ASSERT("[N] BEQ forward displacement = +2",
        p[96] == 0x00 && p[97] == 0x02);

    /* INTENT[INT-AS-086 -> TC-AS-156]:
     * BNE bne_tgt (long): opword=0x6600, disp=+2 (target@104, BNE@100) */
    TEST_ASSERT("[N] BNE forward opword = 0x6600",
        p[100] == 0x66 && p[101] == 0x00);
    TEST_ASSERT("[N] BNE forward displacement = +2",
        p[102] == 0x00 && p[103] == 0x02);

    /* INTENT[INT-AS-087 -> TC-AS-157]:
     * BNE bwd_tgt (backward): opword=0x6600, disp=-4=0xFFFC
     * bwd_tgt@106, BNE@108 -> disp=106-(108+2)=-4 */
    TEST_ASSERT("[N] BNE backward opword = 0x6600",
        p[108] == 0x66 && p[109] == 0x00);
    TEST_ASSERT("[N] BNE backward displacement = 0xFFFC (-4)",
        p[110] == 0xFF && p[111] == 0xFC);
}

/* --- All 14 Bcc conditions via test30c_bcc.S --- */
static void test_all_bcc(void)
{
    as_context_t tCtx;
    const st_u8_t *p;
    const char *szSrc = "use_cases/UC30C/test30c_bcc.S";
    const char *szOut = "use_cases/UC30C/test30c_bcc.prg";

    printf("\n[UC30C] All 14 Bcc conditions + BHS/BLO aliases\n");

    /* INTENT[INT-AS-088 -> TC-AS-158]:
     * test30c_bcc.S assembles to 84 bytes, condition codes correct */
    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[N] test30c_bcc.S assembles OK",
        run_assemble(&tCtx, szSrc, szOut, ST_FALSE) == ST_NO_ERROR);

    TEST_ASSERT("[N] test30c_bcc .text size == 84 bytes",
        tCtx.uiBinaryLen == 28u + 84u);

    if (tCtx.pBinary == NULL) { as_shutdown(&tCtx); return; }
    p = prg_text(&tCtx);

    /* Each block: NOP(2) + Bcc-long(4) = 6 bytes. Bcc at offset N+2.
     * All backward: displacement = -4 = 0xFFFC.
     * Check condition byte (first byte of opword = 0x60|cc). */
    TEST_ASSERT("[N] BHI at offset 2: byte[2]=0x62",  p[2]  == 0x62);
    TEST_ASSERT("[N] BLS at offset 8: byte[8]=0x63",  p[8]  == 0x63);
    TEST_ASSERT("[N] BCC at offset 14: byte[14]=0x64", p[14] == 0x64);
    TEST_ASSERT("[N] BHS (=BCC) at offset 20: byte[20]=0x64", p[20] == 0x64);
    TEST_ASSERT("[N] BCS at offset 26: byte[26]=0x65", p[26] == 0x65);
    TEST_ASSERT("[N] BLO (=BCS) at offset 32: byte[32]=0x65", p[32] == 0x65);
    TEST_ASSERT("[N] BVC at offset 38: byte[38]=0x68", p[38] == 0x68);
    TEST_ASSERT("[N] BVS at offset 44: byte[44]=0x69", p[44] == 0x69);
    TEST_ASSERT("[N] BPL at offset 50: byte[50]=0x6A", p[50] == 0x6A);
    TEST_ASSERT("[N] BMI at offset 56: byte[56]=0x6B", p[56] == 0x6B);
    TEST_ASSERT("[N] BGE at offset 62: byte[62]=0x6C", p[62] == 0x6C);
    TEST_ASSERT("[N] BLT at offset 68: byte[68]=0x6D", p[68] == 0x6D);
    TEST_ASSERT("[N] BGT at offset 74: byte[74]=0x6E", p[74] == 0x6E);
    TEST_ASSERT("[N] BLE at offset 80: byte[80]=0x6F", p[80] == 0x6F);

    /* All backward displacements must be 0xFFFC */
    TEST_ASSERT("[N] all Bcc backward displacements = 0xFFFC",
        text_w(&tCtx, 4)  == 0xFFFC &&
        text_w(&tCtx, 10) == 0xFFFC &&
        text_w(&tCtx, 16) == 0xFFFC &&
        text_w(&tCtx, 22) == 0xFFFC &&
        text_w(&tCtx, 28) == 0xFFFC &&
        text_w(&tCtx, 34) == 0xFFFC &&
        text_w(&tCtx, 40) == 0xFFFC &&
        text_w(&tCtx, 46) == 0xFFFC &&
        text_w(&tCtx, 52) == 0xFFFC &&
        text_w(&tCtx, 58) == 0xFFFC &&
        text_w(&tCtx, 64) == 0xFFFC &&
        text_w(&tCtx, 70) == 0xFFFC &&
        text_w(&tCtx, 76) == 0xFFFC &&
        text_w(&tCtx, 82) == 0xFFFC);

    as_shutdown(&tCtx);
    cleanup(szOut);
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC30C: DEVPAC3 ALU + control flow encoder ===\n");

    test_null_assemble();
    test_add_no_dn();
    test_eor_non_dn_source();
    test_addq_out_of_range();
    test_trap_out_of_range();
    test_branch_undefined_label();

    test_assemble_test30c();
    test_alu_binary();
    test_imm_alu();
    test_quick_and_unary();
    test_ctrl_implied();
    test_branches();
    test_all_bcc();

    as_shutdown(&g_tCtx30C);
    cleanup("use_cases/UC30C/test30c.prg");

    printf("\n=== UC30C results: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
