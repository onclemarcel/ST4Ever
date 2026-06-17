/*
 * use_case_12.c - UC12: 68000 disassembler — ADD/SUB/CMP/MUL/DIV/AND/OR/EOR/NOT/NEG
 *                       + ADDQ/SUBQ/ADDX/SUBX/ADDI/SUBI/CMPI/ANDI/ORI/EORI/TST/EXT
 *
 * Tests run from the project root (make tests requirement — R12).
 *
 * All opcodes are hand-crafted from the MC68000 PRM.
 * Opcode construction for key patterns:
 *   ADD.W D0,D1 (EA→Dn):  top=D, Dn=001, dir=0, sz=01, mode=000, reg=000 = 0xD240
 *   ADD.W D0,(A0) (Dn→EA): top=D, Dn=000, dir=1, sz=01, mode=010, reg=000 = 0xD150
 *   ADDA.L (A0),A1:        top=D, An=001, dir=1(=L), sz=11, EA=mode010 = 0xD3D0
 *   ADDX.W D0,D1:          top=D, dst=001, dir=1, sz=01, mode=000, src=000 = 0xD340
 *
 * TEST MATRIX - UC12:
 *   [N] Nominal    : 64 tests - ADDI/SUBI/CMPI/ORI/ANDI/EORI(12), ADDQ/SUBQ(6),
 *                               ADD/SUB/CMP(12), ADDA/SUBA/CMPA(6), ADDX/SUBX(4),
 *                               AND/OR/EOR(6), MULU/MULS/DIVU/DIVS(4),
 *                               NEG/NOT/NEGX/TST/EXT(10), UC11 regression(4)
 *   [R] Robustness :  8 tests - NULL/invalid params (existing UC11 coverage + UC12)
 *   [S] Skipped    :  0 tests - all headless
 *   Total          : 72
 *
 * Traceability:
 *   INT-DIS-030..041 → TC-DIS-100..159 → REQ-DIS-015..023 → UFR-HEX-005
 */

#include "use_cases.h"
#include "../src/disassemble.h"

int g_uc_fails = 0;

/* Helpers (same pattern as UC11) */
#define UC12_1W(desc, b0, b1, mnem, ops) \
    do { \
        static const st_u8_t _buf[2] = { (b0), (b1) }; \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one(_buf, 2, 0x1000, &_r); \
        UC_TEST("[N] " desc " mnemonic", strcmp(_r.szMnemonic,(mnem))==0); \
        UC_TEST("[N] " desc " operands", strcmp(_r.szOperands,(ops))==0); \
    } while (0)

#define UC12_NW(desc, buf, len, addr, wc, mnem, ops) \
    do { \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one((buf),(len),(addr),&_r); \
        UC_TEST("[N] " desc " wc",   _r.iWordCount==(wc)); \
        UC_TEST("[N] " desc " mnem", strcmp(_r.szMnemonic,(mnem))==0); \
        UC_TEST("[N] " desc " ops",  strcmp(_r.szOperands,(ops))==0); \
    } while (0)

int main(void)
{
    disasm_result_t tR;
    st_error_t      eRet;
    size_t          uiLines = 0;

    printf("=== UC12: ADD/SUB/CMP/MUL/DIV/AND/OR/EOR/NOT/NEG ===\n\n");

    /* ================================================================
     * BLOCK 1 — Immediate arithmetic (group 0x0)
     * ================================================================ */
    printf("--- Block 1: Immediate arithmetic (ADDI/SUBI/CMPI/ORI/ANDI/EORI) [N] ---\n");

    /* INTENT[INT-DIS-030 → TC-DIS-100..111]:
     * Immediate-to-EA instructions use correct mnemonic, size suffix,
     * and format immediate as #$XX / #$XXXX / #$XXXXXXXX. */

    /* ADDI.B #$42,D0 = 0x0600 + ext 0x0042 */
    {
        static const st_u8_t aBuf[4] = { 0x06, 0x00, 0x00, 0x42 };
        UC12_NW("ADDI.B #$42,D0", aBuf, 4, 0x1000, 2, "ADDI.B", "#$42,D0");
    }
    /* ADDI.W #$1234,D1 = 0x0641 + ext 0x1234 */
    {
        static const st_u8_t aBuf[4] = { 0x06, 0x41, 0x12, 0x34 };
        UC12_NW("ADDI.W #$1234,D1", aBuf, 4, 0x1000, 2, "ADDI.W", "#$1234,D1");
    }
    /* SUBI.W #$0001,D0 = 0x0440 + ext 0x0001 */
    {
        static const st_u8_t aBuf[4] = { 0x04, 0x40, 0x00, 0x01 };
        UC12_NW("SUBI.W #$0001,D0", aBuf, 4, 0x1000, 2, "SUBI.W", "#$0001,D0");
    }
    /* CMPI.L #$12345678,D0 = 0x0C80 + 0x1234 + 0x5678 */
    {
        static const st_u8_t aBuf[6] = { 0x0C, 0x80, 0x12, 0x34, 0x56, 0x78 };
        UC12_NW("CMPI.L #imm32,D0", aBuf, 6, 0x1000, 3, "CMPI.L", "#$12345678,D0");
    }
    /* ORI.W #$00FF,(A0) = 0x0050 + ext 0x00FF */
    {
        static const st_u8_t aBuf[4] = { 0x00, 0x50, 0x00, 0xFF };
        UC12_NW("ORI.W #$00FF,(A0)", aBuf, 4, 0x1000, 2, "ORI.W", "#$00FF,(A0)");
    }
    /* ANDI.B #$0F,D3 = 0x0203 + ext 0x000F */
    {
        static const st_u8_t aBuf[4] = { 0x02, 0x03, 0x00, 0x0F };
        UC12_NW("ANDI.B #$0F,D3", aBuf, 4, 0x1000, 2, "ANDI.B", "#$0F,D3");
    }
    /* EORI.W #$FFFF,D7 = 0x0A47 + ext 0xFFFF */
    {
        static const st_u8_t aBuf[4] = { 0x0A, 0x47, 0xFF, 0xFF };
        UC12_NW("EORI.W #$FFFF,D7", aBuf, 4, 0x1000, 2, "EORI.W", "#$FFFF,D7");
    }

    /* ================================================================
     * BLOCK 2 — ADDQ / SUBQ
     * ================================================================ */
    printf("\n--- Block 2: ADDQ / SUBQ [N] ---\n");

    /* INTENT[INT-DIS-031 → TC-DIS-112..117]:
     * ADDQ/SUBQ show immediate as decimal (1–8); 0 in opcode field = 8. */

    /* ADDQ.W #4,D0 = 0x5840: 0101_100_0_01_000_000
     * bits11-9=100(data=4), bit8=0(ADDQ), bits7-6=01(W), bits5-0=D0 */
    UC12_1W("ADDQ.W #4,D0", 0x58, 0x40, "ADDQ.W", "#4,D0");

    /* ADDQ.L #1,A0 = 0x5288: 0101_001_0_10_001_000
     * bits11-9=001(data=1), bit8=0, bits7-6=10(L), bits5-3=001(An), bits2-0=000(A0) */
    UC12_1W("ADDQ.L #1,A0", 0x52, 0x88, "ADDQ.L", "#1,A0");

    /* ADDQ.B #8,(A0): data=0(=8), sz=B, EA=(A0)
     * 0101_000_0_00_010_000 = 0x5010 */
    UC12_1W("ADDQ.B #8,(A0)", 0x50, 0x10, "ADDQ.B", "#8,(A0)");

    /* SUBQ.W #2,D5 = 0x5545: 0101_010_1_01_000_101
     * bits11-9=010(data=2), bit8=1(SUBQ), bits7-6=01(W), bits5-0=D5 */
    UC12_1W("SUBQ.W #2,D5", 0x55, 0x45, "SUBQ.W", "#2,D5");

    /* SUBQ.L #4,A1 = 0x5989: 0101_100_1_10_001_001
     * bits11-9=100(4), bit8=1(SUBQ), bits7-6=10(L), mode=001(An), reg=001(A1) */
    UC12_1W("SUBQ.L #4,A1", 0x59, 0x89, "SUBQ.L", "#4,A1");

    /* ADAPTED: UC14A — 0x51C0 (group5 cond=1 sz=3 mode=0) now decodes as SF D0 */
    {
        static const st_u8_t aBuf[2] = { 0x51, 0xC0 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] 0x51C0 = SF D0 ADAPTED(UC14A)",
                tR.bValid == ST_TRUE
                && strcmp(tR.szMnemonic, "SF") == 0);
    }

    /* ================================================================
     * BLOCK 3 — ADD / SUB / CMP (register ↔ EA)
     * ================================================================ */
    printf("\n--- Block 3: ADD / SUB / CMP [N] ---\n");

    /* INTENT[INT-DIS-032 → TC-DIS-118..129]:
     * ADD/SUB/CMP in both directions (EA→Dn and Dn→EA). */

    /* ADD.W D0,D1 (EA→Dn): 1101_001_0_01_000_000 = 0xD240 */
    UC12_1W("ADD.W D0,D1 (EA→Dn)", 0xD2, 0x40, "ADD.W", "D0,D1");

    /* ADD.W D0,(A0) (Dn→EA): 1101_000_1_01_010_000 = 0xD150 */
    UC12_1W("ADD.W D0,(A0) (Dn→EA)", 0xD1, 0x50, "ADD.W", "D0,(A0)");

    /* SUB.L D3,D5 (EA→Dn): 1001_101_0_10_000_011 = 0x9A83 */
    UC12_1W("SUB.L D3,D5 (EA→Dn)", 0x9A, 0x83, "SUB.L", "D3,D5");

    /* CMP.B (A0),D2 (EA→Dn): 1011_010_0_00_010_000 = 0xB410 */
    UC12_1W("CMP.B (A0),D2", 0xB4, 0x10, "CMP.B", "(A0),D2");

    /* ADD.L #$12345678,D0 (ADDI style? No — this is ADD with imm EA):
     * 1101_000_0_10_111_100 = 0xD0BC + 2 ext words */
    {
        static const st_u8_t aBuf[6] = { 0xD0, 0xBC, 0x12, 0x34, 0x56, 0x78 };
        UC12_NW("ADD.L #imm32,D0", aBuf, 6, 0x1000, 3, "ADD.L", "#$12345678,D0");
    }

    /* SUB.W $10(A0),D1 (EA→Dn): 1001_001_0_01_101_000 = 0x9268 + ext 0x0010
     * bits7-6=01(W), bits5-3=101(d16(An)), bits11-9=001(D1), bit8=0(EA→Dn) */
    {
        static const st_u8_t aBuf[4] = { 0x92, 0x68, 0x00, 0x10 };
        UC12_NW("SUB.W $10(A0),D1", aBuf, 4, 0x1000, 2, "SUB.W", "$10(A0),D1");
    }

    /* ================================================================
     * BLOCK 4 — ADDA / SUBA / CMPA
     * ================================================================ */
    printf("\n--- Block 4: ADDA / SUBA / CMPA [N] ---\n");

    /* INTENT[INT-DIS-033 → TC-DIS-130..135]:
     * ADDA/SUBA/CMPA: An destination, EA source, W or L. */

    /* ADDA.W D0,A1: 1101_001_0_11_000_000 = 0xD2C0 */
    UC12_1W("ADDA.W D0,A1", 0xD2, 0xC0, "ADDA.W", "D0,A1");

    /* ADDA.L (A0),A1: 1101_001_1_11_010_000 = 0xD3D0 */
    UC12_1W("ADDA.L (A0),A1", 0xD3, 0xD0, "ADDA.L", "(A0),A1");

    /* SUBA.W D0,A0: 1001_000_0_11_000_000 = 0x90C0 */
    UC12_1W("SUBA.W D0,A0", 0x90, 0xC0, "SUBA.W", "D0,A0");

    /* CMPA.L D7,A3: 1011_011_1_11_000_111 = 0xB7C7 */
    UC12_1W("CMPA.L D7,A3", 0xB7, 0xC7, "CMPA.L", "D7,A3");

    /* ================================================================
     * BLOCK 5 — ADDX / SUBX
     * ================================================================ */
    printf("\n--- Block 5: ADDX / SUBX [N] ---\n");

    /* INTENT[INT-DIS-034 → TC-DIS-136..141]:
     * ADDX/SUBX in register and memory (predecrement) forms. */

    /* ADDX.W D0,D1: 1101_001_1_01_000_000 = 0xD340 */
    UC12_1W("ADDX.W D0,D1", 0xD3, 0x40, "ADDX.W", "D0,D1");

    /* ADDX.L -(A0),-(A1): 1101_001_1_10_000_1000 → wrong, let me compute:
     * 1101 yyy1 ss 000 1xxx: dst=A1(yyy=001), sz=10(L), bMem=1, src=A0(xxx=000)
     * = 0xD388 */
    UC12_1W("ADDX.L -(A0),-(A1)", 0xD3, 0x88, "ADDX.L", "-(A0),-(A1)");

    /* SUBX.B D3,D5: 1001_101_1_00_000_011 = 0x9B03 */
    UC12_1W("SUBX.B D3,D5", 0x9B, 0x03, "SUBX.B", "D3,D5");

    /* ================================================================
     * BLOCK 6 — AND / OR / EOR
     * ================================================================ */
    printf("\n--- Block 6: AND / OR / EOR [N] ---\n");

    /* INTENT[INT-DIS-035 → TC-DIS-142..147]:
     * AND/OR/EOR in both directions; EOR is always Dn→EA when bit8=1. */

    /* AND.W D0,D1 (EA→Dn): 1100_001_0_01_000_000 = 0xC240 */
    UC12_1W("AND.W D0,D1 (EA→Dn)", 0xC2, 0x40, "AND.W", "D0,D1");

    /* AND.W D0,(A0) (Dn→EA): 1100_000_1_01_010_000 = 0xC150 */
    UC12_1W("AND.W D0,(A0) (Dn→EA)", 0xC1, 0x50, "AND.W", "D0,(A0)");

    /* OR.L D2,D3 (EA→Dn): 1000_011_0_10_000_010 = 0x8682 */
    UC12_1W("OR.L D2,D3 (EA→Dn)", 0x86, 0x82, "OR.L", "D2,D3");

    /* EOR.W D0,D1 (Dn→EA): 1011_000_1_01_000_001 = 0xB141 */
    UC12_1W("EOR.W D0,D1 (Dn→EA)", 0xB1, 0x41, "EOR.W", "D0,D1");

    /* ================================================================
     * BLOCK 7 — MULU / MULS / DIVU / DIVS
     * ================================================================ */
    printf("\n--- Block 7: MULU / MULS / DIVU / DIVS [N] ---\n");

    /* INTENT[INT-DIS-036 → TC-DIS-148..155]:
     * MUL/DIV: .W suffix, EA source word, Dn destination. */

    /* MULU.W D0,D1: 1100_001_0_11_000_000 = 0xC2C0 */
    UC12_1W("MULU.W D0,D1", 0xC2, 0xC0, "MULU.W", "D0,D1");

    /* MULS.W D0,D2: 1100_010_1_11_000_000 = 0xC5C0
     * bits11-9=010(D2), bit8=1(MULS), bits7-6=11(sz=3) */
    UC12_1W("MULS.W D0,D2", 0xC5, 0xC0, "MULS.W", "D0,D2");

    /* DIVU.W D1,D0: 1000_000_0_11_000_001 = 0x80C1 */
    UC12_1W("DIVU.W D1,D0", 0x80, 0xC1, "DIVU.W", "D1,D0");

    /* DIVS.W (A0),D0: 1000_000_1_11_010_000 = 0x81D0 */
    UC12_1W("DIVS.W (A0),D0", 0x81, 0xD0, "DIVS.W", "(A0),D0");

    /* ================================================================
     * BLOCK 8 — NEG / NOT / NEGX / TST / EXT
     * ================================================================ */
    printf("\n--- Block 8: NEG / NOT / NEGX / TST / EXT [N] ---\n");

    /* INTENT[INT-DIS-037 → TC-DIS-156..169]:
     * Unary operations: NEG/NOT/NEGX/TST on EA; EXT.W/L on Dn. */

    /* NEG.B D0 = 0x4400 */
    UC12_1W("NEG.B D0", 0x44, 0x00, "NEG.B", "D0");

    /* NEG.W (A0) = 0x4450 */
    UC12_1W("NEG.W (A0)", 0x44, 0x50, "NEG.W", "(A0)");

    /* NOT.L D3 = 0x4683 */
    UC12_1W("NOT.L D3", 0x46, 0x83, "NOT.L", "D3");

    /* NOT.W D0 = 0x4640 */
    UC12_1W("NOT.W D0", 0x46, 0x40, "NOT.W", "D0");

    /* NEGX.B D0 = 0x4000 */
    UC12_1W("NEGX.B D0", 0x40, 0x00, "NEGX.B", "D0");

    /* TST.W D5 = 0x4A45 */
    UC12_1W("TST.W D5", 0x4A, 0x45, "TST.W", "D5");

    /* TST.L (A0) = 0x4A90 */
    UC12_1W("TST.L (A0)", 0x4A, 0x90, "TST.L", "(A0)");

    /* EXT.W D2 = 0x4882 */
    UC12_1W("EXT.W D2", 0x48, 0x82, "EXT.W", "D2");

    /* EXT.L D5 = 0x48C5 */
    UC12_1W("EXT.L D5", 0x48, 0xC5, "EXT.L", "D5");

    /* ADAPTED: UC14A — 0x44C0 (NEG group sz=3 mode=0) now decodes as MOVE.W D0,CCR */
    {
        static const st_u8_t aBuf[2] = { 0x44, 0xC0 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] 0x44C0 = MOVE.W D0,CCR ADAPTED(UC14A)",
                tR.bValid == ST_TRUE
                && strcmp(tR.szMnemonic, "MOVE.W") == 0);
    }

    /* ================================================================
     * BLOCK 9 — CMPM
     * ================================================================ */
    printf("\n--- Block 9: CMPM [N] ---\n");

    /* INTENT[INT-DIS-038 → TC-DIS-170..171]:
     * CMPM uses postincrement addressing for both operands. */

    /* CMPM.W (A0)+,(A1)+: 1011_001_1_01_001_000 = 0xB348 */
    UC12_1W("CMPM.W (A0)+,(A1)+", 0xB3, 0x48, "CMPM.W", "(A0)+,(A1)+");

    /* ================================================================
     * BLOCK 10 — disasm_range: synthetic PRG-like sequence
     * ================================================================ */
    printf("\n--- Block 10: disasm_range on PRG-like sequence [N] ---\n");

    /* INTENT[INT-DIS-039 → TC-DIS-172..180]:
     * A realistic sequence of instructions found in an Atari ST .PRG:
     *   MOVEQ #0,D0           (0x7000)
     *   ADDI.W #$1000,D0      (0x0640 0x1000)
     *   CMP.W (A0),D0         (0xB050)
     *   NEG.W D0              (0x4440)
     *   MULU.W D0,D1          (0xC2C0)
     * Total: 8 bytes → 5 instructions (words: 1+2+1+1+1=6 → 12 bytes? no)
     *
     * Actually count: MOVEQ=1w, ADDI=2w, CMP=1w, NEG=1w, MULU=1w = 6w = 12 bytes */

    {
        static const st_u8_t aSeq[12] = {
            0x70, 0x00,              /* MOVEQ #0,D0          */
            0x06, 0x40, 0x10, 0x00, /* ADDI.W #$1000,D0     */
            0xB0, 0x50,              /* CMP.W (A0),D0        */
            0x44, 0x40,              /* NEG.W D0             */
            0xC2, 0xC0               /* MULU.W D0,D1         */
        };
        disasm_result_t aR[8];
        memset(aR, 0, sizeof(aR));
        eRet = disasm_range(aSeq, 12, 0x2000, aR, 8, &uiLines);
        UC_TEST("[N] PRG sequence → ST_NO_ERROR", eRet == ST_NO_ERROR);
        UC_TEST("[N] PRG sequence → 5 lines",     uiLines == 5);
        UC_TEST("[N] PRG line0 = MOVEQ",
                strcmp(aR[0].szMnemonic, "MOVEQ") == 0);
        UC_TEST("[N] PRG line1 = ADDI.W",
                strcmp(aR[1].szMnemonic, "ADDI.W") == 0);
        UC_TEST("[N] PRG line1 wordCount = 2",    aR[1].iWordCount == 2);
        UC_TEST("[N] PRG line2 = CMP.W",
                strcmp(aR[2].szMnemonic, "CMP.W") == 0);
        UC_TEST("[N] PRG line2 addr = $2006",     aR[2].uiAddr == 0x2006);
        UC_TEST("[N] PRG line3 = NEG.W",
                strcmp(aR[3].szMnemonic, "NEG.W") == 0);
        UC_TEST("[N] PRG line4 = MULU.W",
                strcmp(aR[4].szMnemonic, "MULU.W") == 0);
    }

    /* ================================================================
     * BLOCK 11 — UC11 regression
     * ================================================================ */
    printf("\n--- Block 11: UC11 regression [N] ---\n");

    /* INTENT[INT-DIS-040 → TC-DIS-181..184]:
     * UC11 instructions still decode correctly after UC12 changes. */

    /* MOVE.W D0,D1 still works */
    UC12_1W("MOVE.W D0,D1 (regression)", 0x32, 0x00, "MOVE.W", "D0,D1");

    /* LEA abs.W still works: 0x41F8 + 0x8200 */
    {
        static const st_u8_t aBuf[4] = { 0x41, 0xF8, 0x82, 0x00 };
        UC12_NW("LEA abs.W (regression)", aBuf, 4, 0x1000, 2,
                "LEA", "$FF8200.W,A0");
    }

    /* ================================================================
     * BLOCK 12 — Robustness
     * ================================================================ */
    printf("\n--- Block 12: Robustness [R] ---\n");

    /* INTENT[INT-DIS-041 → TC-DIS-185..192]:
     * Invalid sizes / unsupported opcodes → DC.W; NULL params → ST_ERROR. */

    eRet = disasm_one(NULL, 4, 0x1000, &tR);
    UC_TEST("[R] disasm_one(NULL buf) → ST_ERROR", eRet == ST_ERROR);

    eRet = disasm_one((const st_u8_t*)"\xD2\x40", 4, 0x1000, NULL);
    UC_TEST("[R] disasm_one(NULL result) → ST_ERROR", eRet == ST_ERROR);

    /* ADAPTED: UC14A — 0x40C0 (NEGX group sz=3 mode=0) now decodes as MOVE.W SR,D0 */
    {
        static const st_u8_t aBuf[2] = { 0x40, 0xC0 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[R] 0x40C0 = MOVE.W SR,D0 ADAPTED(UC14A)",
                tR.bValid == ST_TRUE
                && strcmp(tR.szMnemonic, "MOVE.W") == 0);
    }

    /* ADAPTED: UC14B — 0x4AC0 (TST size=11) now decodes as TAS D0 */
    {
        static const st_u8_t aBuf[2] = { 0x4A, 0xC0 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[R] TST size=11 → TAS D0 ADAPTED(UC14B)",
                tR.bValid == ST_TRUE
                && strcmp(tR.szMnemonic, "TAS") == 0);
    }

    /* Unknown opcode 0x6000 (BRA — UC14) → DC.W */
    {
        static const st_u8_t aBuf[2] = { 0x60, 0x00 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[R] 0x6000 (BRA UC14) → DC.W", tR.bValid == ST_FALSE);
    }

    /* ADAPTED: UC14 — RTS now decoded; was DC.W placeholder */
    {
        static const st_u8_t aBuf[2] = { 0x4E, 0x75 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] 0x4E75 = RTS decoded ADAPTED(UC14)",
                tR.bValid == ST_TRUE
                && strcmp(tR.szMnemonic, "RTS") == 0);
    }

    /* ADAPTED: UC14B — 0xC100 now decodes as ABCD D0,D0 */
    {
        static const st_u8_t aBuf[2] = { 0xC1, 0x00 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[R] 0xC100 = ABCD D0,D0 ADAPTED(UC14B)",
                tR.bValid == ST_TRUE
                && strcmp(tR.szMnemonic, "ABCD") == 0);
    }

    /* ADAPTED: UC14A — 0x50C0 (group5 cond=0 sz=3 mode=0) now decodes as ST D0 */
    {
        static const st_u8_t aBuf[2] = { 0x50, 0xC0 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[R] 0x50C0 = ST D0 ADAPTED(UC14A)",
                tR.bValid == ST_TRUE
                && strcmp(tR.szMnemonic, "ST") == 0);
    }

    /* ================================================================
     * Summary
     * ================================================================ */
    printf("\n=== UC12 result: %d failure(s) ===\n\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
