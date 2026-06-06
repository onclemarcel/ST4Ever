/*
 * use_case_11.c - UC11: 68000 disassembler — MOVE/MOVEQ/LEA/CLR/EXG/SWAP/PEA
 *
 * Tests run from the project root (make tests requirement — R12).
 *
 * All opcode bytes are hand-crafted from the Motorola MC68000 PRM;
 * expected disassembly follows DEVPAC3 Atari ST output format.
 *
 * TEST MATRIX - UC11:
 *   [N] Nominal    : 52 tests - MOVE(16), MOVEQ(4), LEA(5), CLR(5),
 *                               SWAP(3), PEA(3), EXG(5), EA modes(11)
 *   [R] Robustness : 10 tests - NULL/bounds/invalid opcodes
 *   [S] Skipped    :  0 tests - all headless
 *   Total          : 62
 *
 * Traceability:
 *   INT-DIS-010..022 → TC-DIS-010..067 → REQ-DIS-005..006, REQ-DIS-010..014
 *   → UFR-HEX-005
 */

#include "use_cases.h"
#include "../src/disassemble.h"

int g_uc_fails = 0;

/* Helper: disassemble one 2-byte instruction and check mnemonic+operands */
#define UC11_CHECK_1W(desc, b0, b1, mnem, ops) \
    do { \
        static const st_u8_t _buf[2] = { (b0), (b1) }; \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one(_buf, 2, 0x1000, &_r); \
        UC_TEST("[N] " desc " mnemonic", \
                strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " operands", \
                strcmp(_r.szOperands, (ops)) == 0); \
    } while (0)

/* Helper: disassemble N bytes at addr, check iWordCount + mnemonic + operands */
#define UC11_CHECK_NW(desc, buf, len, addr, wc, mnem, ops) \
    do { \
        disasm_result_t _r; \
        memset(&_r, 0, sizeof(_r)); \
        disasm_one((buf), (len), (addr), &_r); \
        UC_TEST("[N] " desc " wordCount", _r.iWordCount == (wc)); \
        UC_TEST("[N] " desc " mnemonic",  strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " operands",  strcmp(_r.szOperands, (ops)) == 0); \
    } while (0)

/* ==================================================================
 * main
 * ================================================================== */

int main(void)
{
    disasm_result_t tR;
    st_error_t      eRet;
    size_t          uiLines = 0;

    printf("=== UC11: 68000 disassembler — MOVE/MOVEQ/LEA/CLR/EXG/SWAP/PEA ===\n\n");

    /* ================================================================
     * BLOCK 1 — MOVE: register-to-register (all 3 sizes)
     * ================================================================ */
    printf("--- Block 1: MOVE register-to-register [N] ---\n");

    /* INTENT[INT-DIS-010 → TC-DIS-010..015]:
     * MOVE Dn,Dn for all three sizes uses correct mnemonic and
     * source/destination register names. */

    /* MOVE.B D0,D1 = 0x1200 */
    UC11_CHECK_1W("MOVE.B D0,D1", 0x12, 0x00, "MOVE.B", "D0,D1");

    /* MOVE.W D0,D1 = 0x3200 */
    UC11_CHECK_1W("MOVE.W D0,D1", 0x32, 0x00, "MOVE.W", "D0,D1");

    /* MOVE.L D0,D1 = 0x2200 */
    UC11_CHECK_1W("MOVE.L D0,D1", 0x22, 0x00, "MOVE.L", "D0,D1");

    /* MOVE.W D7,D6 = 0x3C07 */
    UC11_CHECK_1W("MOVE.W D7,D6", 0x3C, 0x07, "MOVE.W", "D7,D6");

    /* ================================================================
     * BLOCK 2 — MOVE: memory addressing modes as source
     * ================================================================ */
    printf("\n--- Block 2: MOVE source EA modes [N] ---\n");

    /* INTENT[INT-DIS-011 → TC-DIS-016..025]:
     * Each addressing mode for the source operand produces the correct
     * DEVPAC3 syntax; word counts match the number of extension words. */

    /* MOVE.W (A0),D0 = 0x3010 */
    UC11_CHECK_1W("MOVE.W (A0),D0", 0x30, 0x10, "MOVE.W", "(A0),D0");

    /* MOVE.W (A0)+,D0 = 0x3018 */
    UC11_CHECK_1W("MOVE.W (A0)+,D0", 0x30, 0x18, "MOVE.W", "(A0)+,D0");

    /* MOVE.W -(A0),D0 = 0x3020 */
    UC11_CHECK_1W("MOVE.W -(A0),D0", 0x30, 0x20, "MOVE.W", "-(A0),D0");

    /* MOVE.W $10(A0),D0 = 0x3028 + ext 0x0010 (2 words) */
    {
        static const st_u8_t aBuf[4] = { 0x30, 0x28, 0x00, 0x10 };
        UC11_CHECK_NW("MOVE.W d(A0),D0", aBuf, 4, 0x1000, 2,
                      "MOVE.W", "$10(A0),D0");
    }

    /* MOVE.W #$1234,D0 = 0x303C + ext 0x1234 (2 words) */
    {
        static const st_u8_t aBuf[4] = { 0x30, 0x3C, 0x12, 0x34 };
        UC11_CHECK_NW("MOVE.W #imm,D0", aBuf, 4, 0x1000, 2,
                      "MOVE.W", "#$1234,D0");
    }

    /* MOVE.L #$12345678,D0 = 0x203C + 0x1234 + 0x5678 (3 words) */
    {
        static const st_u8_t aBuf[6] =
            { 0x20, 0x3C, 0x12, 0x34, 0x56, 0x78 };
        UC11_CHECK_NW("MOVE.L #imm32,D0", aBuf, 6, 0x1000, 3,
                      "MOVE.L", "#$12345678,D0");
    }

    /* MOVE.W $8200.W,D0 — abs.W sign-extends to $FF8200
     * opcode 0x3038 + ext 0x8200 */
    {
        static const st_u8_t aBuf[4] = { 0x30, 0x38, 0x82, 0x00 };
        UC11_CHECK_NW("MOVE.W abs.W,D0", aBuf, 4, 0x1000, 2,
                      "MOVE.W", "$FF8200.W,D0");
    }

    /* MOVE.L $12345678,D0 — abs.L
     * opcode 0x2039 + 0x1234 + 0x5678 */
    {
        static const st_u8_t aBuf[6] =
            { 0x20, 0x39, 0x12, 0x34, 0x56, 0x78 };
        UC11_CHECK_NW("MOVE.L abs.L,D0", aBuf, 6, 0x1000, 3,
                      "MOVE.L", "$12345678,D0");
    }

    /* ================================================================
     * BLOCK 3 — MOVEA
     * ================================================================ */
    printf("\n--- Block 3: MOVEA [N] ---\n");

    /* INTENT[INT-DIS-012 → TC-DIS-026..029]:
     * MOVE.W/L to An uses MOVEA mnemonic; MOVE.B to An → DC.W. */

    /* MOVEA.W D0,A0 = 0x3040 */
    UC11_CHECK_1W("MOVEA.W D0,A0", 0x30, 0x40, "MOVEA.W", "D0,A0");

    /* MOVEA.L D0,A1 = 0x2240
     * 0010_001_001_000_000: size=L, dst=(A1,mode001), src=(D0,mode000) */
    UC11_CHECK_1W("MOVEA.L D0,A1", 0x22, 0x40, "MOVEA.L", "D0,A1");

    /* MOVE.B src,An (0x1040) → not valid → DC.W                      */
    /* INTENT: MOVE.B to An is not a valid 68000 instruction → DC.W   */
    {
        static const st_u8_t aBuf[2] = { 0x10, 0x40 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] MOVE.B to An → DC.W (invalid)",
                tR.bValid == ST_FALSE);
    }

    /* ================================================================
     * BLOCK 4 — MOVEQ
     * ================================================================ */
    printf("\n--- Block 4: MOVEQ [N] ---\n");

    /* INTENT[INT-DIS-013 → TC-DIS-030..033]:
     * MOVEQ sign-extends 8-bit data to 32 bits at runtime;
     * disassembler shows the unsigned hex byte value. */

    /* MOVEQ #$42,D0 = 0x7042 */
    UC11_CHECK_1W("MOVEQ #$42,D0", 0x70, 0x42, "MOVEQ", "#$42,D0");

    /* MOVEQ #$FF,D7 = 0x7EFF
     * 0111_111_0_1111_1111: D7=bits11-9=111, bit8=0, data=$FF */
    UC11_CHECK_1W("MOVEQ #$FF,D7", 0x7E, 0xFF, "MOVEQ", "#$FF,D7");

    /* MOVEQ #0,D3 = 0x7600 */
    UC11_CHECK_1W("MOVEQ #0,D3",   0x76, 0x00, "MOVEQ", "#$00,D3");

    /* 0x7100 — bit 8 set → not MOVEQ → DC.W */
    {
        static const st_u8_t aBuf[2] = { 0x71, 0x00 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] 0x7100 bit8=1 → not MOVEQ → DC.W",
                tR.bValid == ST_FALSE);
    }

    /* ================================================================
     * BLOCK 5 — LEA
     * ================================================================ */
    printf("\n--- Block 5: LEA [N] ---\n");

    /* INTENT[INT-DIS-014 → TC-DIS-034..043]:
     * LEA with control-addressing source; destination is always An. */

    /* LEA (A0),A1 = 0x43D0
     * 0100_001_1_11_010_000: A1=bits11-9=001, bit8=1, bits7-6=11,
     * EA=mode010(indirect)+reg000(A0) */
    UC11_CHECK_1W("LEA (A0),A1", 0x43, 0xD0, "LEA", "(A0),A1");

    /* LEA $FF8200.W,A0 = 0x41F8 + 0x8200 */
    {
        static const st_u8_t aBuf[4] = { 0x41, 0xF8, 0x82, 0x00 };
        UC11_CHECK_NW("LEA abs.W,A0", aBuf, 4, 0x1000, 2,
                      "LEA", "$FF8200.W,A0");
    }

    /* LEA $12345678,A2 = 0x45F9 + 0x1234 + 0x5678 */
    {
        static const st_u8_t aBuf[6] =
            { 0x45, 0xF9, 0x12, 0x34, 0x56, 0x78 };
        UC11_CHECK_NW("LEA abs.L,A2", aBuf, 6, 0x1000, 3,
                      "LEA", "$12345678,A2");
    }

    /* LEA $10(A0),A3 = 0x47E8 + 0x0010
     * 0100_011_1_11_101_000: A3=bits11-9=011, EA=mode101(d16)+reg000(A0) */
    {
        static const st_u8_t aBuf[4] = { 0x47, 0xE8, 0x00, 0x10 };
        UC11_CHECK_NW("LEA d(A0),A3", aBuf, 4, 0x1000, 2,
                      "LEA", "$10(A0),A3");
    }

    /* LEA d(PC),A4: at addr $1000, ext=0x0010
     * PC for ext = $1002, EA = $1002+$10 = $1012
     * opcode 0x49FA = 0100_100_1_11_111_010: A4=100, EA=mode7(111)+reg2(PC) */
    {
        static const st_u8_t aBuf[4] = { 0x49, 0xFA, 0x00, 0x10 };
        UC11_CHECK_NW("LEA d(PC),A4", aBuf, 4, 0x1000, 2,
                      "LEA", "$001012(PC),A4");
    }

    /* ================================================================
     * BLOCK 6 — CLR
     * ================================================================ */
    printf("\n--- Block 6: CLR [N] ---\n");

    /* INTENT[INT-DIS-015 → TC-DIS-044..051]:
     * CLR in all three sizes; CLR with size=11 is invalid → DC.W. */

    /* CLR.B D0 = 0x4200 */
    UC11_CHECK_1W("CLR.B D0", 0x42, 0x00, "CLR.B", "D0");

    /* CLR.W D5 = 0x4245 */
    UC11_CHECK_1W("CLR.W D5", 0x42, 0x45, "CLR.W", "D5");

    /* CLR.L D7 = 0x4287 */
    UC11_CHECK_1W("CLR.L D7", 0x42, 0x87, "CLR.L", "D7");

    /* CLR.W (A1) = 0x4251 */
    UC11_CHECK_1W("CLR.W (A1)", 0x42, 0x51, "CLR.W", "(A1)");

    /* CLR size=11 (0x42C0) → invalid size → DC.W */
    {
        static const st_u8_t aBuf[2] = { 0x42, 0xC0 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] CLR size=11 → DC.W (invalid size)",
                tR.bValid == ST_FALSE);
    }

    /* ================================================================
     * BLOCK 7 — SWAP
     * ================================================================ */
    printf("\n--- Block 7: SWAP [N] ---\n");

    /* INTENT[INT-DIS-016 → TC-DIS-052..054]:
     * SWAP Dn operates on all 8 data registers. */

    UC11_CHECK_1W("SWAP D0", 0x48, 0x40, "SWAP", "D0");
    UC11_CHECK_1W("SWAP D3", 0x48, 0x43, "SWAP", "D3");
    UC11_CHECK_1W("SWAP D7", 0x48, 0x47, "SWAP", "D7");

    /* ================================================================
     * BLOCK 8 — PEA
     * ================================================================ */
    printf("\n--- Block 8: PEA [N] ---\n");

    /* INTENT[INT-DIS-017 → TC-DIS-055..060]:
     * PEA with control addressing modes; Dn source → DC.W (not control). */

    /* PEA (A0) = 0x4850 */
    UC11_CHECK_1W("PEA (A0)", 0x48, 0x50, "PEA", "(A0)");

    /* PEA abs.W: 0x4878 + 0x1234 */
    {
        static const st_u8_t aBuf[4] = { 0x48, 0x78, 0x12, 0x34 };
        UC11_CHECK_NW("PEA abs.W", aBuf, 4, 0x1000, 2,
                      "PEA", "$1234.W");
    }

    /* PEA abs.L: 0x4879 + 0x0000 + 0x1000 */
    {
        static const st_u8_t aBuf[6] =
            { 0x48, 0x79, 0x00, 0x00, 0x10, 0x00 };
        UC11_CHECK_NW("PEA abs.L", aBuf, 6, 0x1000, 3,
                      "PEA", "$00001000");
    }

    /* PEA Dn (mode=000) → DC.W: 0x4840 = SWAP, 0x4841..7 = SWAP too */
    {
        /* 0x4840 decoded as SWAP D0, not PEA */
        static const st_u8_t aBuf[2] = { 0x48, 0x40 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] 0x4840 → SWAP D0 (not PEA)",
                strcmp(tR.szMnemonic, "SWAP") == 0);
    }

    /* ================================================================
     * BLOCK 9 — EXG
     * ================================================================ */
    printf("\n--- Block 9: EXG [N] ---\n");

    /* INTENT[INT-DIS-018 → TC-DIS-061..070]:
     * Three EXG modes: Dx,Dy / Ax,Ay / Dx,Ay — all without extension words. */

    /* EXG D0,D1 = 0xC141 */
    UC11_CHECK_1W("EXG D0,D1", 0xC1, 0x41, "EXG", "D0,D1");

    /* EXG D3,D7 = 0xC747 */
    UC11_CHECK_1W("EXG D3,D7", 0xC7, 0x47, "EXG", "D3,D7");

    /* EXG A0,A1 = 0xC149 — bits11-9=A0(iRx), bits2-0=A1(iRy) */
    /* ADAPTED:UC15A — DEVPAC3 puts first source operand in bits2-0 (iRy),
     * so disassembler outputs A{iRy},A{iRx} = A1,A0 */
    UC11_CHECK_1W("EXG A1,A0", 0xC1, 0x49, "EXG", "A1,A0");

    /* EXG A3,A5 = 0xC74D — bits11-9=A3(iRx), bits2-0=A5(iRy) */
    /* ADAPTED:UC15A — iRy first: A5,A3 */
    UC11_CHECK_1W("EXG A5,A3", 0xC7, 0x4D, "EXG", "A5,A3");

    /* EXG D0,A1 = 0xC189 */
    UC11_CHECK_1W("EXG D0,A1", 0xC1, 0x89, "EXG", "D0,A1");

    /* ================================================================
     * BLOCK 10 — EA modes: d8(An,Xn) and indexed
     * ================================================================ */
    printf("\n--- Block 10: EA indexed modes [N] ---\n");

    /* INTENT[INT-DIS-019 → TC-DIS-071..076]:
     * Brief extension word format for d8(An,Xn): register, size,
     * displacement correctly decoded. */

    /* MOVE.W $5(A0,D1.W),D0
     * opcode 0x3028 = MOVE.W d16(A0),D0 — wait, that's mode 5.
     * d8(An,Xn) is mode 6: 0x3030 = MOVE.W (A0,Xn),D0
     * opcode=0x3030, ext=0x1005 (D1.W, disp=5) */
    {
        /* ext: bit15=0(Dn), bit14-12=001(D1), bit11=0(W), bit7-0=05 */
        static const st_u8_t aBuf[4] = { 0x30, 0x30, 0x10, 0x05 };
        UC11_CHECK_NW("MOVE.W d8(A0,D1.W),D0", aBuf, 4, 0x1000, 2,
                      "MOVE.W", "$5(A0,D1.W),D0");
    }

    /* MOVE.W $0(A0,A1.L),D0
     * opcode=0x3030, ext=0x9800 (A1.L, disp=0) */
    {
        /* ext: bit15=1(An), bit14-12=001(A1), bit11=1(L), bit7-0=00 */
        static const st_u8_t aBuf[4] = { 0x30, 0x30, 0x98, 0x00 };
        UC11_CHECK_NW("MOVE.W $0(A0,A1.L),D0", aBuf, 4, 0x1000, 2,
                      "MOVE.W", "$0(A0,A1.L),D0");
    }

    /* MOVE.W -$8(A2,D3.W),D0
     * opcode=0x3032, ext=0x30F8 (D3.W, disp=-8=0xF8) */
    {
        /* ext: bit15=0(Dn), bit14-12=011(D3), bit11=0(W), bit7-0=F8 */
        static const st_u8_t aBuf[4] = { 0x30, 0x32, 0x30, 0xF8 };
        UC11_CHECK_NW("MOVE.W -$8(A2,D3.W),D0", aBuf, 4, 0x1000, 2,
                      "MOVE.W", "-$8(A2,D3.W),D0");
    }

    /* ================================================================
     * BLOCK 11 — disasm_range with mixed instructions
     * ================================================================ */
    printf("\n--- Block 11: disasm_range mixed [N] ---\n");

    /* INTENT[INT-DIS-020 → TC-DIS-077..080]:
     * disasm_range decodes a sequence of different instruction types;
     * word counts accumulate correctly across multi-word instructions. */

    {
        /* MOVEQ #$42,D0 (1 word) + LEA abs.W,A0 (2 words) = 3 words total */
        static const st_u8_t aSeq[6] =
        {
            0x70, 0x42,        /* MOVEQ #$42,D0 */
            0x41, 0xF8, 0x10, 0x00  /* LEA $1000.W,A0 */
        };
        disasm_result_t aR[4];
        size_t uiN = 0;

        memset(aR, 0, sizeof(aR));
        eRet = disasm_range(aSeq, 6, 0x2000, aR, 4, &uiN);

        UC_TEST("[N] range mixed → ST_NO_ERROR", eRet == ST_NO_ERROR);
        UC_TEST("[N] range mixed → 2 lines",     uiN == 2);
        UC_TEST("[N] range line0 = MOVEQ",
                strcmp(aR[0].szMnemonic, "MOVEQ") == 0);
        UC_TEST("[N] range line1 = LEA",
                strcmp(aR[1].szMnemonic, "LEA") == 0);
        UC_TEST("[N] range line1 wordCount = 2", aR[1].iWordCount == 2);
        UC_TEST("[N] range line1 addr = $2002", aR[1].uiAddr == 0x2002);
    }

    /* ================================================================
     * BLOCK 12 — Robustness
     * ================================================================ */
    printf("\n--- Block 12: Robustness [R] ---\n");

    /* INTENT[INT-DIS-021 → TC-DIS-081..090]:
     * NULL and boundary conditions handled without crash. */

    eRet = disasm_one(NULL, 4, 0x1000, &tR);
    UC_TEST("[R] disasm_one(NULL buf) → ST_ERROR", eRet == ST_ERROR);

    memset(&tR, 0, sizeof(tR));
    eRet = disasm_one((const st_u8_t*)"\x70\x42", 4, 0x1000, NULL);
    UC_TEST("[R] disasm_one(NULL result) → ST_ERROR", eRet == ST_ERROR);

    /* Buffer too short (1 byte) → ST_NO_ERROR, word count = 1 */
    {
        static const st_u8_t aBuf[1] = { 0x70 };
        memset(&tR, 0, sizeof(tR));
        eRet = disasm_one(aBuf, 1, 0x1000, &tR);
        UC_TEST("[R] 1-byte buffer → ST_NO_ERROR", eRet == ST_NO_ERROR);
        UC_TEST("[R] 1-byte buffer → wordCount == 1",
                tR.iWordCount == 1);
    }

    /* disasm_range: NULL pBuf */
    eRet = disasm_range(NULL, 4, 0x1000, &tR, 1, &uiLines);
    UC_TEST("[R] range(NULL buf) → ST_ERROR", eRet == ST_ERROR);

    /* disasm_range: NULL results */
    {
        static const st_u8_t aBuf[2] = { 0x70, 0x42 };
        eRet = disasm_range(aBuf, 2, 0x1000, NULL, 1, &uiLines);
        UC_TEST("[R] range(NULL results) → ST_ERROR", eRet == ST_ERROR);
    }

    /* disasm_range: NULL puiLines */
    {
        static const st_u8_t aBuf[2] = { 0x70, 0x42 };
        eRet = disasm_range(aBuf, 2, 0x1000, &tR, 1, NULL);
        UC_TEST("[R] range(NULL puiLines) → ST_ERROR", eRet == ST_ERROR);
    }

    /* Unknown opcode → DC.W, bValid == ST_FALSE */
    {
        /* 0x0000 = ORI.B #imm,Dn — not implemented in UC11 → DC.W */
        static const st_u8_t aBuf[2] = { 0x00, 0x00 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[R] unknown opcode 0x0000 → DC.W",
                tR.bValid == ST_FALSE
                && strcmp(tR.szMnemonic, "DC.W") == 0);
    }

    /* 0xFFFF → DC.W */
    {
        static const st_u8_t aBuf[2] = { 0xFF, 0xFF };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[R] unknown opcode 0xFFFF → DC.W", tR.bValid == ST_FALSE);
    }

    /* range zero-length buffer → 0 lines */
    {
        static const st_u8_t aBuf[2] = { 0x70, 0x42 };
        disasm_result_t aR[4];
        uiLines = 99;
        eRet = disasm_range(aBuf, 0, 0x1000, aR, 4, &uiLines);
        UC_TEST("[R] range(len=0) → ST_NO_ERROR", eRet == ST_NO_ERROR);
        UC_TEST("[R] range(len=0) → 0 lines",     uiLines == 0);
    }

    /* ================================================================
     * BLOCK 13 — UC1 regression: stub tests now produce real results
     * ================================================================ */
    printf("\n--- Block 13: UC1 regression (stub → real) [N] ---\n");

    /* INTENT[INT-DIS-022 → TC-DIS-091..094]:
     * The hello.prg bytes 0x702A and 0x4E75 now decode correctly.
     * TC-DIS-001 (ADAPTED UC11) now expects bValid = ST_TRUE for MOVEQ
     * and DC.W for 0x4E75 (RTS not yet implemented, UC14). */

    {
        static const st_u8_t aBuf[2] = { 0x70, 0x2A };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] 0x702A = MOVEQ #$2A,D0 (bValid)",
                tR.bValid == ST_TRUE);
        UC_TEST("[N] 0x702A mnemonic = MOVEQ",
                strcmp(tR.szMnemonic, "MOVEQ") == 0);
    }
    {
        /* ADAPTED: UC14 — RTS now decoded (bValid=TRUE, mnemonic="RTS") */
        static const st_u8_t aBuf[2] = { 0x4E, 0x75 };
        memset(&tR, 0, sizeof(tR));
        disasm_one(aBuf, 2, 0x1000, &tR);
        UC_TEST("[N] 0x4E75 = RTS decoded ADAPTED(UC14)",
                tR.bValid == ST_TRUE
                && strcmp(tR.szMnemonic, "RTS") == 0);
    }

    /* ================================================================
     * Summary
     * ================================================================ */
    printf("\n=== UC11 result: %d failure(s) ===\n\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
