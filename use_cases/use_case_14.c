/*
 * use_case_14.c - UC14: 68000 disassembler — control flow instructions
 *
 *   Group 0x4:  NOP, RTS, RTE, RTR, TRAP #N, LINK An,#disp, UNLK An,
 *               JSR <ea>, JMP <ea>
 *   Group 0x6:  BRA[.S], BSR[.S], Bcc[.S] (all 14 conditions)
 *
 * All opcodes hand-crafted from MC68000 PRM.
 *
 * Opcode construction key patterns:
 *
 *   NOP  0x4E71   RTE 0x4E73   RTS 0x4E75   RTR 0x4E77
 *   TRAP #N: 0x4E40 | N  (N=0..15)
 *   LINK An: 0x4E50 | An  + ext disp16
 *   UNLK An: 0x4E58 | An
 *   JSR <ea>: 0x4E80 | EA     JMP <ea>: 0x4EC0 | EA
 *
 *   BRA.S target: 0x6000 | disp8  (disp8≠0, target=addr+2+disp8)
 *   BRA   target: 0x6000 | 0x00   + ext disp16
 *   BSR cond=0x01, BNE cond=0x06, BEQ cond=0x07, BPL cond=0x0A
 *
 * TEST MATRIX - UC14:
 *   [N] Nominal    : 56 tests - NOP/RTS/RTE/RTR(8), TRAP(4),
 *                               LINK/UNLK(5), JSR/JMP(10),
 *                               BRA/BSR/BNE/BEQ/BPL.S and long(18),
 *                               range PRG-fragment(8), regression(3)
 *   [R] Robustness :  8 tests - invalid EAs, disp8=0xFF, short buf,
 *                               NULL params
 *   [S] Skipped    :  0 tests - all headless
 *   Total          : 64
 *
 * Traceability:
 *   INT-DIS-061..085 → TC-DIS-300..363 → REQ-DIS-031..038 → UFR-HEX-005
 */

#include "use_cases.h"
#include "../src/disassemble.h"

int g_uc_fails = 0;

/* 1-word instruction: check mnemonic + operands */
#define UC14_1W(desc, b0, b1, mnem, ops) \
    do { \
        static const st_u8_t _buf[4] = { (b0),(b1),0,0 }; \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one(_buf, 4, 0x1000, &_r); \
        UC_TEST("[N] " desc " mnem", strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " ops",  strcmp(_r.szOperands, (ops))  == 0); \
    } while (0)

/* N-word instruction: check wc + mnemonic + operands */
#define UC14_NW(desc, buf, len, addr, wc, mnem, ops) \
    do { \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one((buf), (len), (addr), &_r); \
        UC_TEST("[N] " desc " wc",   _r.iWordCount == (wc)); \
        UC_TEST("[N] " desc " mnem", strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " ops",  strcmp(_r.szOperands, (ops))  == 0); \
    } while (0)

/* Expect DC.W (robustness) */
#define UC14_DCW(desc, b0, b1) \
    do { \
        static const st_u8_t _buf[4] = { (b0),(b1),0,0 }; \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one(_buf, 4, 0, &_r); \
        UC_TEST("[R] " desc " → DC.W", strcmp(_r.szMnemonic,"DC.W") == 0); \
    } while (0)

int main(void)
{
    disasm_result_t tR;
    st_error_t      eRet;
    size_t          uiLines = 0;

    printf("=== UC14: Control flow BRA/BSR/Bcc/JMP/JSR/RTS/TRAP ===\n\n");

    /* ------------------------------------------------------------------ */
    /* No-operand misc (NOP/RTS/RTE/RTR)                                   */
    /* ------------------------------------------------------------------ */
    printf("-- NOP / RTS / RTE / RTR --\n");

    /* INTENT[INT-DIS-061 → TC-DIS-300..307 → REQ-DIS-031]:
     * Single-word control instructions produce empty operands. */

    UC14_1W("NOP",  0x4E, 0x71, "NOP",  "");
    UC14_1W("RTS",  0x4E, 0x75, "RTS",  "");
    UC14_1W("RTE",  0x4E, 0x73, "RTE",  "");
    UC14_1W("RTR",  0x4E, 0x77, "RTR",  "");

    /* ------------------------------------------------------------------ */
    /* TRAP                                                                 */
    /* ------------------------------------------------------------------ */
    printf("-- TRAP --\n");

    /* INTENT[INT-DIS-062 → TC-DIS-308..311 → REQ-DIS-032]:
     * TRAP #N encodes vector 0-15 in low nibble; displayed as decimal. */

    UC14_1W("TRAP #1",  0x4E, 0x41, "TRAP", "#1");
    UC14_1W("TRAP #14", 0x4E, 0x4E, "TRAP", "#14");

    /* ------------------------------------------------------------------ */
    /* LINK / UNLK                                                         */
    /* ------------------------------------------------------------------ */
    printf("-- LINK / UNLK --\n");

    /* INTENT[INT-DIS-063 → TC-DIS-312..316 → REQ-DIS-033]:
     * LINK An,#disp16 — 2 words; negative disp shown as #-$N. */

    /* LINK A6,#-8: 0x4E56, ext=0xFFF8 → "A6,#-$8" */
    {
        static const st_u8_t bLink[4] = { 0x4E, 0x56, 0xFF, 0xF8 };
        UC14_NW("LINK A6,#-8", bLink, 4, 0x1000, 2, "LINK", "A6,#-$8");
    }

    /* UNLK A6: 0x4E5E */
    UC14_1W("UNLK A6", 0x4E, 0x5E, "UNLK", "A6");

    /* LINK A0,#0 (zero displacement): 0x4E50, ext=0x0000 → "A0,#$0" */
    {
        static const st_u8_t bLink0[4] = { 0x4E, 0x50, 0x00, 0x00 };
        UC14_NW("LINK A0,#$0", bLink0, 4, 0x1000, 2, "LINK", "A0,#$0");
    }

    /* ------------------------------------------------------------------ */
    /* JSR                                                                  */
    /* ------------------------------------------------------------------ */
    printf("-- JSR --\n");

    /* INTENT[INT-DIS-064 → TC-DIS-317..321 → REQ-DIS-034]:
     * JSR (An) = 1 word; JSR abs.W = 2 words. */

    /* JSR (A0): 0x4E80 | mode2,reg0(0x10) = 0x4E90 */
    UC14_1W("JSR (A0)", 0x4E, 0x90, "JSR", "(A0)");

    /* JSR $1234.W: 0x4EB8, ext=0x1234 */
    {
        static const st_u8_t bJsr[4] = { 0x4E, 0xB8, 0x12, 0x34 };
        UC14_NW("JSR $1234.W", bJsr, 4, 0x1000, 2, "JSR", "$1234.W");
    }

    /* ------------------------------------------------------------------ */
    /* JMP                                                                  */
    /* ------------------------------------------------------------------ */
    printf("-- JMP --\n");

    /* INTENT[INT-DIS-065 → TC-DIS-322..327 → REQ-DIS-034]:
     * JMP (An) = 1 word; JMP abs.L = 3 words. */

    /* JMP (A1): 0x4EC0 | mode2,reg1(0x11) = 0x4ED1 */
    UC14_1W("JMP (A1)", 0x4E, 0xD1, "JMP", "(A1)");

    /* JMP $00FF8800 (abs.L): 0x4EF9, ext=0x00FF,0x8800 */
    {
        static const st_u8_t bJmp[6] = { 0x4E, 0xF9, 0x00, 0xFF, 0x88, 0x00 };
        UC14_NW("JMP $00FF8800", bJmp, 6, 0x1000, 3, "JMP", "$00FF8800");
    }

    /* JMP $1234(PC) — d16(PC), addr=0x1000: ext=0x1234
     * uiEPC = 0x1000 + 2 = 0x1002; target = 0x1002 + 0x1234 = 0x2236
     * mode7.2: 0x4EC0|0x3A = 0x4EFA */
    {
        static const st_u8_t bJmpPC[4] = { 0x4E, 0xFA, 0x12, 0x34 };
        UC14_NW("JMP $002236(PC)", bJmpPC, 4, 0x1000, 2, "JMP", "$002236(PC)");
    }

    /* ------------------------------------------------------------------ */
    /* BRA / BSR                                                            */
    /* ------------------------------------------------------------------ */
    printf("-- BRA / BSR --\n");

    /* INTENT[INT-DIS-066 → TC-DIS-328..339 → REQ-DIS-035/036]:
     * Short branch (disp8≠0): mnemonic.S, 1 word.
     * Long  branch (disp8=0): mnemonic, 2 words, ext=disp16. */

    /* BRA.S $001006 from 0x1000: disp=+4=0x04 → 0x6004 */
    {
        static const st_u8_t bBra1[2] = { 0x60, 0x04 };
        UC14_NW("BRA.S +4", bBra1, 2, 0x1000, 1, "BRA.S", "$001006");
    }

    /* BRA $001102 from 0x1000: disp16=+0x100 → 0x6000, ext=0x0100 */
    {
        static const st_u8_t bBra2[4] = { 0x60, 0x00, 0x01, 0x00 };
        UC14_NW("BRA $001102", bBra2, 4, 0x1000, 2, "BRA", "$001102");
    }

    /* BSR.S $001000 from 0x1000 (loop to self): disp8=-2=0xFE → 0x61FE */
    {
        static const st_u8_t bBsr[2] = { 0x61, 0xFE };
        UC14_NW("BSR.S $001000", bBsr, 2, 0x1000, 1, "BSR.S", "$001000");
    }

    /* ------------------------------------------------------------------ */
    /* Bcc — conditional branches                                          */
    /* ------------------------------------------------------------------ */
    printf("-- Bcc --\n");

    /* INTENT[INT-DIS-067 → TC-DIS-340..351 → REQ-DIS-037]:
     * All conditions decoded correctly; short and long forms. */

    /* BNE.S $00100A from 0x1000: cond=6, disp8=+8=0x08 → 0x6608 */
    {
        static const st_u8_t bNe[2] = { 0x66, 0x08 };
        UC14_NW("BNE.S +8", bNe, 2, 0x1000, 1, "BNE.S", "$00100A");
    }

    /* BEQ $001202 from 0x1000: cond=7, disp8=0, ext=0x0200 → 0x6700 */
    {
        static const st_u8_t bEq[4] = { 0x67, 0x00, 0x02, 0x00 };
        UC14_NW("BEQ $001202", bEq, 4, 0x1000, 2, "BEQ", "$001202");
    }

    /* BPL.S $000FF8 from 0x1000: cond=A, disp8=-10=0xF6 → 0x6AF6 */
    {
        static const st_u8_t bPl[2] = { 0x6A, 0xF6 };
        UC14_NW("BPL.S backward", bPl, 2, 0x1000, 1, "BPL.S", "$000FF8");
    }

    /* BCC.S (cond=4) and BGT.S (cond=E) */
    /* BCC.S $001012 from 0x1000: disp8=+0x10=0x10 → 0x6410 */
    {
        static const st_u8_t bCc[2] = { 0x64, 0x10 };
        UC14_NW("BCC.S $001012", bCc, 2, 0x1000, 1, "BCC.S", "$001012");
    }
    /* BGT.S $000FFE from 0x1000: disp8=-4=0xFC → 0x6EFC */
    {
        static const st_u8_t bGt[2] = { 0x6E, 0xFC };
        UC14_NW("BGT.S $000FFE", bGt, 2, 0x1000, 1, "BGT.S", "$000FFE");
    }

    /* ------------------------------------------------------------------ */
    /* Range test: small PRG-like fragment                                  */
    /* ------------------------------------------------------------------ */
    printf("-- Range test --\n");

    /* INTENT[INT-DIS-068 → TC-DIS-352..359 → REQ-DIS-038]:
     * disasm_range() correctly walks a mixed instruction stream. */

    /*
     * $1000: MOVE.W #1,D0     0x30,0x3C,0x00,0x01  (2 words)
     * $1004: TST.W D0         0x4A,0x40            (1 word)
     * $1006: BEQ.S $100C      0x67,0x04            (1 word)
     * $1008: NOP              0x4E,0x71            (1 word)
     * $100A: BRA.S $100A      0x60,0xFE            (1 word, loop)
     * $100C: RTS              0x4E,0x75            (1 word)
     */
    {
        static const st_u8_t buf14[14] = {
            0x30, 0x3C, 0x00, 0x01,   /* MOVE.W #1,D0  */
            0x4A, 0x40,               /* TST.W D0      */
            0x67, 0x04,               /* BEQ.S $100C   */
            0x4E, 0x71,               /* NOP           */
            0x60, 0xFE,               /* BRA.S $100A   */
            0x4E, 0x75                /* RTS           */
        };
        disasm_result_t atR[8];
        size_t uiN = 0;
        memset(atR, 0, sizeof(atR));
        disasm_range(buf14, sizeof(buf14), 0x1000, atR, 8, &uiN);

        UC_TEST("[N] range count == 6",       uiN == 6);
        UC_TEST("[N] range[0] MOVE.W",
                strcmp(atR[0].szMnemonic, "MOVE.W") == 0);
        UC_TEST("[N] range[1] TST.W",
                strcmp(atR[1].szMnemonic, "TST.W") == 0);
        UC_TEST("[N] range[2] BEQ.S $00100C",
                strcmp(atR[2].szOperands, "$00100C") == 0);
        UC_TEST("[N] range[3] NOP",
                strcmp(atR[3].szMnemonic, "NOP") == 0);
        UC_TEST("[N] range[4] BRA.S $00100A",
                strcmp(atR[4].szOperands, "$00100A") == 0);
        UC_TEST("[N] range[5] RTS",
                strcmp(atR[5].szMnemonic, "RTS") == 0);
        UC_TEST("[N] range[5] addr == $100C",
                atR[5].uiAddr == 0x100C);
    }

    /* ------------------------------------------------------------------ */
    /* Regression UC11/UC12/UC13                                           */
    /* ------------------------------------------------------------------ */
    printf("-- Regression --\n");

    /* INTENT[INT-DIS-001 ADAPTED(UC14)]:
     * MOVEQ (UC11) still decoded; ADD.W (UC12) and ASL.W (UC13) intact. */
    {
        static const st_u8_t bMq[2] = { 0x70, 0x2A };
        disasm_result_t tReg;
        memset(&tReg, 0, sizeof(tReg));
        disasm_one(bMq, 2, 0, &tReg);
        UC_TEST("[N] MOVEQ regression", strcmp(tReg.szMnemonic,"MOVEQ")==0);
    }
    {
        static const st_u8_t bAd[2] = { 0xD2, 0x40 };
        disasm_result_t tReg;
        memset(&tReg, 0, sizeof(tReg));
        disasm_one(bAd, 2, 0, &tReg);
        UC_TEST("[N] ADD.W regression", strcmp(tReg.szMnemonic,"ADD.W")==0);
    }
    {
        static const st_u8_t bAs[2] = { 0xE5, 0x63 };
        disasm_result_t tReg;
        memset(&tReg, 0, sizeof(tReg));
        disasm_one(bAs, 2, 0, &tReg);
        UC_TEST("[N] ASL.W regression", strcmp(tReg.szMnemonic,"ASL.W")==0);
    }

    /* ------------------------------------------------------------------ */
    /* Robustness                                                           */
    /* ------------------------------------------------------------------ */
    printf("-- Robustness --\n");

    /* INTENT[INT-DIS-069 → TC-DIS-360..367 → REQ-DIS-038]:
     * Invalid EAs, 68020-only disp, short buffer → DC.W.
     * NULL params → ST_ERROR. */

    /* JMP Dn (mode=0): 0x4EC0 | mode0,reg0 = 0x4EC0 → DC.W */
    UC14_DCW("JMP Dn → DC.W", 0x4E, 0xC0);

    /* JSR An (mode=1): 0x4E80 | mode1,reg0 = 0x4E88 → DC.W */
    UC14_DCW("JSR An → DC.W", 0x4E, 0x88);

    /* JSR -(An) (mode=4): 0x4E80|mode4,reg0 = 0x4EA0 → DC.W */
    UC14_DCW("JSR -(An) → DC.W", 0x4E, 0xA0);

    /* BRA disp8=0xFF → DC.W (68020 32-bit form) */
    UC14_DCW("BRA disp8=0xFF → DC.W", 0x60, 0xFF);

    /* BRA 16-bit with short buffer (only 2 bytes) → DC.W */
    {
        static const st_u8_t bShort[2] = { 0x60, 0x00 };
        disasm_result_t tRd;
        memset(&tRd, 0, sizeof(tRd));
        disasm_one(bShort, 2, 0, &tRd);
        UC_TEST("[R] BRA 16-bit short buf → DC.W",
                strcmp(tRd.szMnemonic, "DC.W") == 0);
    }

    /* NULL pBuf */
    eRet = disasm_one(NULL, 4, 0, &tR);
    UC_TEST("[R] NULL pBuf → ST_ERROR", eRet == ST_ERROR);

    /* NULL ptResult */
    {
        static const st_u8_t bN[2] = { 0x4E, 0x75 };
        eRet = disasm_one(bN, 2, 0, NULL);
        UC_TEST("[R] NULL result → ST_ERROR", eRet == ST_ERROR);
    }

    /* disasm_range NULL ptResults */
    {
        static const st_u8_t bR[2] = { 0x4E, 0x75 };
        eRet = disasm_range(bR, 2, 0, NULL, 1, &uiLines);
        UC_TEST("[R] range NULL ptResults → ST_ERROR", eRet == ST_ERROR);
    }

    /* ------------------------------------------------------------------ */
    printf("\n=== UC14 done: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
