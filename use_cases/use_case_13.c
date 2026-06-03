/*
 * use_case_13.c - UC13: 68000 disassembler — shifts, rotations, bit ops
 *
 *   Shifts/rotations: ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR
 *     - Register form (immediate count #1-8, or register count Dn)
 *     - Memory form  (word only, count=1)
 *   Bit operations: BTST/BCHG/BCLR/BSET
 *     - Static  (#imm source)
 *     - Dynamic (Dn source)
 *
 * All opcodes are hand-crafted from the MC68000 PRM.
 * Opcode construction for key patterns:
 *
 *   Register shift (immediate): 1110 cccc d ss 1 tt rrr
 *     cccc=count(0→8), d=dir(1=L), ss=size, tt=type(00AS,01LS,10ROX,11RO)
 *     ASL.W #2,D3:  1110 010 1 01 1 00 011 = 0xE563
 *     ASR.W #3,D0:  1110 011 0 01 1 00 000 = 0xE660
 *     LSL.B #1,D1:  1110 001 1 00 1 01 001 = 0xE329
 *     LSR.L #4,D2:  1110 100 0 10 1 01 010 = 0xE8CA
 *     ROXL.W #1,D5: 1110 001 1 01 1 10 101 = 0xE355 (wait, recalc)
 *
 *   Register shift (register):  1110 cccc d ss 0 tt rrr
 *     ASL.W D3,D0:  1110 011 1 01 0 00 000 = 0xE740
 *
 *   Memory shift: 0xE0C0..0xE7C0 | EA  (bits 10-8 index mnemonic)
 *     ASR.W (A0):   0xE0C0 | mode2,reg0 = 0xE0D0
 *     ASL.W (A0):   0xE1C0 | mode2,reg0 = 0xE1D0
 *
 *   Static bit op:  0x0800 | (op<<6) | EA  + ext-word #bit
 *     op: 0=BTST, 1=BCHG, 2=BCLR, 3=BSET (bits 7-6)
 *     BTST #5,D3:   0x0803, ext=0x0005
 *
 *   Dynamic bit op: 0x0100 | (Dn<<9) | (op<<6) | EA
 *     BTST D0,D3:   0x0103
 *
 * TEST MATRIX - UC13:
 *   [N] Nominal    : 62 tests - reg shift imm(16), reg shift Dn(6),
 *                               mem shift(12), bit static(8),
 *                               bit dynamic(8), wc checks(6), range(6)
 *   [R] Robustness :  8 tests - DC.W for invalid shift/bit ops, NULL
 *   [S] Skipped    :  0 tests - all headless
 *   Total          : 70
 *
 * Traceability:
 *   INT-DIS-042..060 → TC-DIS-200..269 → REQ-DIS-024..030 → UFR-HEX-005
 */

#include "use_cases.h"
#include "../src/disassemble.h"

int g_uc_fails = 0;

/* 1-word instruction: check mnemonic + operands */
#define UC13_1W(desc, b0, b1, mnem, ops) \
    do { \
        static const st_u8_t _buf[4] = { (b0), (b1), 0, 0 }; \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one(_buf, 4, 0x1000, &_r); \
        UC_TEST("[N] " desc " mnem", strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " ops",  strcmp(_r.szOperands, (ops))  == 0); \
    } while (0)

/* 2-word instruction: check wc + mnemonic + operands */
#define UC13_2W(desc, b0, b1, b2, b3, wc, mnem, ops) \
    do { \
        static const st_u8_t _buf[6] = \
            { (b0), (b1), (b2), (b3), 0, 0 }; \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one(_buf, 6, 0x1000, &_r); \
        UC_TEST("[N] " desc " wc",   _r.iWordCount == (wc)); \
        UC_TEST("[N] " desc " mnem", strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " ops",  strcmp(_r.szOperands, (ops))  == 0); \
    } while (0)

/* Expect DC.W (robustness) */
#define UC13_DCW(desc, b0, b1) \
    do { \
        static const st_u8_t _buf[4] = { (b0), (b1), 0, 0 }; \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one(_buf, 4, 0, &_r); \
        UC_TEST("[R] " desc " is DC.W", strcmp(_r.szMnemonic, "DC.W") == 0); \
    } while (0)

int main(void)
{
    disasm_result_t tR;
    st_error_t      eRet;
    size_t          uiLines = 0;

    printf("=== UC13: Shifts/Rotations + Bit Ops ===\n\n");

    /* ------------------------------------------------------------------ */
    /* Register shifts — immediate count                                   */
    /* Format: 1110 cccc d ss 1 tt rrr                                     */
    /* ------------------------------------------------------------------ */
    printf("-- Register shifts (immediate count) --\n");

    /* INTENT[INT-DIS-042 → TC-DIS-200..207 → REQ-DIS-024]:
     * ASL/ASR with immediate count, all sizes produce correct mnemonic
     * and "#count,Dn" operands. count=0 in field means 8. */

    /* ASL.W #2,D3: 1110 010 1 01 1 00 011 = 0xE563 */
    UC13_1W("ASL.W #2,D3", 0xE5, 0x63, "ASL.W", "#2,D3");

    /* ASR.W #3,D0: 1110 011 0 01 1 00 000 = 0xE660 */
    UC13_1W("ASR.W #3,D0", 0xE6, 0x60, "ASR.W", "#3,D0");

    /* INTENT[INT-DIS-043 → TC-DIS-208..211 → REQ-DIS-024]:
     * LSL/LSR correct across sizes. */

    /* LSL.B #1,D1: 1110 001 1 00 1 01 001 = 0xE329 */
    UC13_1W("LSL.B #1,D1", 0xE3, 0x29, "LSL.B", "#1,D1");

    /* LSR.L #4,D2: 1110 100 0 10 1 01 010 = 0xE8AA */
    UC13_1W("LSR.L #4,D2", 0xE8, 0xAA, "LSR.L", "#4,D2");

    /* INTENT[INT-DIS-044 → TC-DIS-212..215 → REQ-DIS-024]:
     * ROXL/ROXR correct. */

    /* ROXL.W #1,D5: 1110 001 1 01 1 10 101 = 0xE375 */
    UC13_1W("ROXL.W #1,D5", 0xE3, 0x75, "ROXL.W", "#1,D5");

    /* ROXR.B #2,D4: 1110 010 0 00 1 10 100 = 0xE434 */
    UC13_1W("ROXR.B #2,D4", 0xE4, 0x34, "ROXR.B", "#2,D4");

    /* INTENT[INT-DIS-045 → TC-DIS-216..219 → REQ-DIS-024]:
     * ROL/ROR correct; count=0 in field → displayed as #8. */

    /* ROL.L #8,D7: count=000(=8), 1110 000 1 10 1 11 111 = 0xE1BF */
    UC13_1W("ROL.L #8,D7", 0xE1, 0xBF, "ROL.L", "#8,D7");

    /* ROR.W #1,D0: 1110 001 0 01 1 11 000 = 0xE278 */
    UC13_1W("ROR.W #1,D0", 0xE2, 0x78, "ROR.W", "#1,D0");

    /* ------------------------------------------------------------------ */
    /* Register shifts — register count                                    */
    /* Format: 1110 cccc d ss 0 tt rrr                                     */
    /* ------------------------------------------------------------------ */
    printf("-- Register shifts (register count Dn) --\n");

    /* INTENT[INT-DIS-046 → TC-DIS-220..225 → REQ-DIS-025]:
     * Register-count form uses "Dn,Dn" operands. */

    /* ASL.W D3,D0: 1110 011 1 01 0 00 000 = 0xE740 */
    UC13_1W("ASL.W D3,D0", 0xE7, 0x40, "ASL.W", "D3,D0");

    /* LSR.B D2,D1: 1110 010 0 00 0 01 001 = 0xE409 */
    UC13_1W("LSR.B D2,D1", 0xE4, 0x09, "LSR.B", "D2,D1");

    /* ROR.L D0,D5: 1110 000 0 10 0 11 101 = 0xE09D */
    UC13_1W("ROR.L D0,D5", 0xE0, 0x9D, "ROR.L", "D0,D5");

    /* ------------------------------------------------------------------ */
    /* Memory shifts — word only, count=1                                  */
    /* Encoding: 0xEXC0 | EA where X selects mnemonic via bits 10-8       */
    /* ------------------------------------------------------------------ */
    printf("-- Memory shifts --\n");

    /* INTENT[INT-DIS-047 → TC-DIS-226..237 → REQ-DIS-026]:
     * All 8 memory shift mnemonics produce ".W" suffix and EA operand. */

    /* ASR.W (A0): 0xE0C0 | mode2,reg0(0x10) = 0xE0D0 */
    UC13_1W("ASR.W (A0)", 0xE0, 0xD0, "ASR.W", "(A0)");

    /* ASL.W (A1): 0xE1C0 | mode2,reg1(0x11) = 0xE1D1 */
    UC13_1W("ASL.W (A1)", 0xE1, 0xD1, "ASL.W", "(A1)");

    /* LSR.W (A2)+: 0xE2C0 | mode3,reg2(0x1A) = 0xE2DA */
    UC13_1W("LSR.W (A2)+", 0xE2, 0xDA, "LSR.W", "(A2)+");

    /* LSL.W -(A3): 0xE3C0 | mode4,reg3(0x23) = 0xE3E3 */
    UC13_1W("LSL.W -(A3)", 0xE3, 0xE3, "LSL.W", "-(A3)");

    /* ROXR.W (A4): 0xE4C0 | mode2,reg4(0x14) = 0xE4D4 */
    UC13_1W("ROXR.W (A4)", 0xE4, 0xD4, "ROXR.W", "(A4)");

    /* ROXL.W (A5): 0xE5C0 | mode2,reg5(0x15) = 0xE5D5 */
    UC13_1W("ROXL.W (A5)", 0xE5, 0xD5, "ROXL.W", "(A5)");

    /* ROR.W (A6): 0xE6C0 | mode2,reg6(0x16) = 0xE6D6 */
    UC13_1W("ROR.W (A6)", 0xE6, 0xD6, "ROR.W", "(A6)");

    /* ROL.W (A7)+: 0xE7C0 | mode3,reg7(0x1F) = 0xE7DF */
    UC13_1W("ROL.W (A7)+", 0xE7, 0xDF, "ROL.W", "(A7)+");

    /* Memory shift with extension word (abs.W addressing) */
    /* ROXR.W $1234.W: 0xE4C0 | mode7,reg0(0x38) = 0xE4F8, ext=0x1234 */
    {
        static const st_u8_t buf5[4] = { 0xE4, 0xF8, 0x12, 0x34 };
        disasm_result_t tRm;
        memset(&tRm, 0, sizeof(tRm));
        disasm_one(buf5, 4, 0x1000, &tRm);
        UC_TEST("[N] ROXR.W $1234.W wc",   tRm.iWordCount == 2);
        UC_TEST("[N] ROXR.W $1234.W mnem", strcmp(tRm.szMnemonic, "ROXR.W") == 0);
        UC_TEST("[N] ROXR.W $1234.W ops",  strcmp(tRm.szOperands, "$1234.W") == 0);
    }

    /* ------------------------------------------------------------------ */
    /* BTST/BCHG/BCLR/BSET — static (#imm)                               */
    /* Encoding: 0x0800 | (op<<6) | EA   + extension word #bit            */
    /* op: 0=BTST, 1=BCHG, 2=BCLR, 3=BSET                               */
    /* ------------------------------------------------------------------ */
    printf("-- Bit ops static (#imm) --\n");

    /* INTENT[INT-DIS-048 → TC-DIS-238..245 → REQ-DIS-027]:
     * Static bit ops produce "#N,EA" operands, iWordCount=2+n. */

    /* BTST #5,D3: 0x0803, ext=0x0005 → wc=2, "#5,D3" */
    UC13_2W("BTST #5,D3", 0x08, 0x03, 0x00, 0x05, 2, "BTST", "#5,D3");

    /* BCHG #0,(A1): 0x0851, ext=0x0000 → wc=2, "#0,(A1)" */
    UC13_2W("BCHG #0,(A1)", 0x08, 0x51, 0x00, 0x00, 2, "BCHG", "#0,(A1)");

    /* BCLR #7,(A2)+: 0x0892, ext=0x0007 → wc=2, "#7,(A2)+" */
    UC13_2W("BCLR #7,(A2)+", 0x08, 0x9A, 0x00, 0x07, 2, "BCLR", "#7,(A2)+");

    /* BSET #15,D7: 0x08C7, ext=0x000F → wc=2, "#15,D7" */
    UC13_2W("BSET #15,D7", 0x08, 0xC7, 0x00, 0x0F, 2, "BSET", "#15,D7");

    /* ------------------------------------------------------------------ */
    /* BTST/BCHG/BCLR/BSET — dynamic (Dn)                                */
    /* Encoding: 0x0100 | (Dn<<9) | (op<<6) | EA                          */
    /* ------------------------------------------------------------------ */
    printf("-- Bit ops dynamic (Dn) --\n");

    /* INTENT[INT-DIS-049 → TC-DIS-246..253 → REQ-DIS-028]:
     * Dynamic bit ops produce "Dn,EA" operands, iWordCount=1. */

    /* BTST D0,D3: Dn=0,op=0,EA=mode0,reg3 → 0x0100|0x00|0x03 = 0x0103 */
    UC13_1W("BTST D0,D3", 0x01, 0x03, "BTST", "D0,D3");

    /* BCHG D1,(A0): Dn=1,op=1(0x40),EA=mode2,reg0(0x10)
     * = 0x0000|0x0200|0x0100|0x0040|0x0010 = 0x0350 */
    UC13_1W("BCHG D1,(A0)", 0x03, 0x50, "BCHG", "D1,(A0)");

    /* BCLR D7,D5: Dn=7(0x0E00),op=2(0x80),EA=mode0,reg5
     * = 0x0000|0x0E00|0x0100|0x0080|0x0005 = 0x0F85 */
    UC13_1W("BCLR D7,D5", 0x0F, 0x85, "BCLR", "D7,D5");

    /* BSET D3,(A2)+: Dn=3(0x0600),op=3(0xC0),EA=mode3,reg2(0x1A)
     * = 0x0000|0x0600|0x0100|0x00C0|0x001A = 0x07DA */
    UC13_1W("BSET D3,(A2)+", 0x07, 0xDA, "BSET", "D3,(A2)+");

    /* ------------------------------------------------------------------ */
    /* Range test: mixed UC13 instructions                                 */
    /* ------------------------------------------------------------------ */
    printf("-- Range test --\n");

    /* INTENT[INT-DIS-050 → TC-DIS-254..259 → REQ-DIS-029]:
     * disasm_range() correctly advances by iWordCount over UC13 opcodes. */
    {
        /* ASL.W #2,D3 (1W) | BTST #5,D3 (2W) | LSR.B D2,D1 (1W) */
        static const st_u8_t buf6[8] =
            { 0xE5,0x63,  0x08,0x03, 0x00,0x05,  0xE4,0x09 };
        disasm_result_t atR[4];
        size_t uiN = 0;

        memset(atR, 0, sizeof(atR));
        disasm_range(buf6, 8, 0x2000, atR, 4, &uiN);
        UC_TEST("[N] range count", uiN == 3);
        UC_TEST("[N] range[0] ASL.W #2,D3",
                strcmp(atR[0].szMnemonic, "ASL.W") == 0);
        UC_TEST("[N] range[1] BTST #5,D3",
                strcmp(atR[1].szMnemonic, "BTST") == 0);
        UC_TEST("[N] range[1] wc=2",
                atR[1].iWordCount == 2);
        UC_TEST("[N] range[2] LSR.B D2,D1",
                strcmp(atR[2].szMnemonic, "LSR.B") == 0);
        UC_TEST("[N] range[2] addr",
                atR[2].uiAddr == 0x2006);
    }

    /* ------------------------------------------------------------------ */
    /* Regression: UC11/UC12 still pass                                    */
    /* ------------------------------------------------------------------ */
    printf("-- Regression UC11/UC12 --\n");

    /* INTENT[INT-DIS-001 ADAPTED(UC13)]: MOVEQ still recognised */
    {
        static const st_u8_t b2[2] = { 0x70, 0x2A };
        disasm_result_t tReg;
        memset(&tReg, 0, sizeof(tReg));
        disasm_one(b2, 2, 0, &tReg);
        UC_TEST("[N] MOVEQ regression mnem",
                strcmp(tReg.szMnemonic, "MOVEQ") == 0);
    }
    /* ADD.W D0,D1 */
    {
        static const st_u8_t b3[2] = { 0xD2, 0x40 };
        disasm_result_t tReg;
        memset(&tReg, 0, sizeof(tReg));
        disasm_one(b3, 2, 0, &tReg);
        UC_TEST("[N] ADD.W regression mnem",
                strcmp(tReg.szMnemonic, "ADD.W") == 0);
    }

    /* ------------------------------------------------------------------ */
    /* Robustness                                                           */
    /* ------------------------------------------------------------------ */
    printf("-- Robustness --\n");

    /* INTENT[INT-DIS-051 → TC-DIS-260..267 → REQ-DIS-030]:
     * Invalid encodings produce DC.W; NULL params return ST_ERROR. */

    /* Memory shift with Dn destination (mode=0) → DC.W */
    /* ASR.W with Dn: 0xE0C0 | mode0,reg0 = 0xE0C0 (which is Dn) */
    UC13_DCW("mem shift Dn dest → DC.W", 0xE0, 0xC0);

    /* Memory shift with An destination (mode=1) → DC.W */
    /* 0xE0C0 | mode1,reg0 = 0xE0C8 */
    UC13_DCW("mem shift An dest → DC.W", 0xE0, 0xC8);

    /* Memory shift bit11=1 (invalid encoding) → DC.W */
    /* 0xE0C0 with bit11 set → 0xE8C0 | mode2,reg0 = 0xE8D0 */
    /* But 0xE8xx: bits15-12=E, bits11-9=100, sz=11 → bit11=1 → DC.W */
    UC13_DCW("mem shift bit11 set → DC.W", 0xE8, 0xD0);

    /* Static bit op with An mode (mode=1) → DC.W */
    /* BTST #n,A0: 0x0808, ext=0x0000 */
    {
        static const st_u8_t buf2[4] = { 0x08, 0x08, 0x00, 0x00 };
        disasm_result_t tRd;
        memset(&tRd, 0, sizeof(tRd));
        disasm_one(buf2, 4, 0, &tRd);
        UC_TEST("[R] bit static An mode → DC.W",
                strcmp(tRd.szMnemonic, "DC.W") == 0);
    }

    /* Dynamic bit op with An mode (mode=1) → DC.W */
    /* BTST D0,A3: 0x0100|mode1,reg3 = 0x0100|0x0B = 0x010B */
    UC13_DCW("bit dyn An mode → DC.W", 0x01, 0x0B);

    /* NULL pBuf */
    eRet = disasm_one(NULL, 4, 0, &tR);
    UC_TEST("[R] NULL pBuf → ST_ERROR", eRet == ST_ERROR);

    /* NULL ptResult */
    {
        static const st_u8_t bNR[2] = { 0xE5, 0x63 };
        eRet = disasm_one(bNR, 2, 0, NULL);
        UC_TEST("[R] NULL result → ST_ERROR", eRet == ST_ERROR);
    }

    /* NULL for disasm_range puiLines */
    {
        static const st_u8_t bR[2] = { 0xE5, 0x63 };
        eRet = disasm_range(bR, 2, 0, &tR, 1, NULL);
        UC_TEST("[R] range NULL puiLines → ST_ERROR", eRet == ST_ERROR);
    }

    /* ------------------------------------------------------------------ */
    printf("\n=== UC13 done: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
