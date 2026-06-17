/*
 * use_case_14B.c - UC14B: 68000 disassembler — completing rare instructions
 *
 *   TAS   : 0100 1010 11 EA  (byte, no size suffix, data alterable only)
 *   NBCD  : 0100 1000 00 EA  (byte, data alterable only)
 *   CHK.W : 0100 DDD 1 10 EA (EA source, Dn destination)
 *   SBCD  : 1000 Rx 1 00 RM Ry  (Ry,Rx or -(Ay),-(Ax))
 *   ABCD  : 1100 Rx 1 00 RM Ry  (Ry,Rx or -(Ay),-(Ax))
 *   MOVEP : 0000 Dn 1 d s 001 An + d16
 *
 * Opcode construction key:
 *   TAS   : 0x4AC0 | (mode<<3) | reg
 *   NBCD  : 0x4800 | (mode<<3) | reg
 *   CHK.W : 0x4180 | (Dn<<9) | (mode<<3) | reg
 *   SBCD Ry,Rx    : 0x8100 | (Rx<<9) | Ry
 *   SBCD -(Ay),-(Ax): 0x8108 | (Ax<<9) | Ay
 *   ABCD Ry,Rx    : 0xC100 | (Rx<<9) | Ry
 *   ABCD -(Ay),-(Ax): 0xC108 | (Ax<<9) | Ay
 *   MOVEP.W mem→reg: 0x0108 | (Dn<<9) | An + d16
 *   MOVEP.W reg→mem: 0x0188 | (Dn<<9) | An + d16
 *   MOVEP.L mem→reg: 0x0148 | (Dn<<9) | An + d16
 *   MOVEP.L reg→mem: 0x01C8 | (Dn<<9) | An + d16
 *
 * TEST MATRIX - UC14B:
 *   [N] Nominal    : 43 assertions — 12 TAS (4 instrs × 3),
 *                                     9 NBCD (3 instrs × 3),
 *                                     6 CHK.W (2 instrs × 3),
 *                                     4 SBCD (2 instrs × 2 fields),
 *                                     4 ABCD (2 instrs × 2 fields),
 *                                     8 MOVEP (4 instrs × 2 fields)
 *   [R] Robustness :  8 assertions — TAS An/ILLEGAL, NBCD An/#imm,
 *                                    CHK.W An, MOVEP short buf
 *   [S] Skipped    :  0
 *   Total          : 51 assertions (24 instruction scenarios)
 *
 * Traceability:
 *   INT-DIS-078..083 → TC-DIS-460..510 → REQ-DIS-045..050 → UFR-HEX-005
 *
 * ADAPTED:
 *   use_case_12.c: 0x4AC0 (TST sz=11 → TAS), 0xC100 (ABCD)
 *   use_case_13.c: 0x010B (dyn bit An mode → MOVEP.W)
 */

#include "use_cases.h"
#include "../src/disassemble.h"

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* 1-word opcode: check wc + mnem + ops */
#define DIS1(desc, b0, b1, wc, mnem, ops)                               \
    do {                                                                  \
        static const st_u8_t _buf[4] = { (b0),(b1),0,0 };               \
        disasm_result_t _r;                                               \
        memset(&_r, 0, sizeof(_r));                                       \
        disasm_one(_buf, 4, 0x1000, &_r);                                 \
        UC_TEST("[N] " desc " wc",   _r.iWordCount == (wc));             \
        UC_TEST("[N] " desc " mnem", strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " ops",  strcmp(_r.szOperands, (ops))  == 0); \
    } while (0)

/* 2-word opcode: check mnem + ops (wc=2) */
#define DIS2(desc, b0, b1, b2, b3, mnem, ops)                            \
    do {                                                                  \
        static const st_u8_t _buf[4] = { (b0),(b1),(b2),(b3) };         \
        disasm_result_t _r;                                               \
        memset(&_r, 0, sizeof(_r));                                       \
        disasm_one(_buf, 4, 0x1000, &_r);                                 \
        UC_TEST("[N] " desc " mnem", strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " ops",  strcmp(_r.szOperands, (ops))  == 0); \
    } while (0)

/* Robustness: check result is DC.W */
#define DIS_DCW(desc, b0, b1)                                             \
    do {                                                                  \
        static const st_u8_t _buf[4] = { (b0),(b1),0,0 };               \
        disasm_result_t _r;                                               \
        memset(&_r, 0, sizeof(_r));                                       \
        disasm_one(_buf, 4, 0x1000, &_r);                                 \
        UC_TEST("[R] " desc " is DC.W",                                   \
                strcmp(_r.szMnemonic, "DC.W") == 0);                      \
    } while (0)

/* ------------------------------------------------------------------ */
int main(void)
{
    printf("=== UC14B: 68000 disassembler — TAS/NBCD/CHK/SBCD/ABCD/MOVEP ===\n");

    /* ----------------------------------------------------------------
     * INTENT[INT-DIS-078 → TC-DIS-460..471 → REQ-DIS-045 → UFR-HEX-005]:
     * TAS <ea> — byte, no size suffix.
     * An (mode 1) and #imm (mode 7 reg 4 = ILLEGAL $4AFC) are rejected.
     * ---------------------------------------------------------------- */
    printf("\n[UC14B] TAS\n");

    /* TAS D0 : 0x4AC0 | 0 = 0x4AC0 */
    DIS1("TAS D0",    0x4A, 0xC0, 1, "TAS", "D0");
    /* TAS (A0) : 0x4AC0 | (2<<3) | 0 = 0x4AD0 */
    DIS1("TAS (A0)",  0x4A, 0xD0, 1, "TAS", "(A0)");
    /* TAS (A1)+ : 0x4AC0 | (3<<3) | 1 = 0x4AD9 */
    DIS1("TAS (A1)+", 0x4A, 0xD9, 1, "TAS", "(A1)+");
    /* TAS -(A2) : 0x4AC0 | (4<<3) | 2 = 0x4AE2 */
    DIS1("TAS -(A2)", 0x4A, 0xE2, 1, "TAS", "-(A2)");

    /* Robustness: An mode (mode 1) → DC.W */
    DIS_DCW("TAS An invalid", 0x4A, 0xC8);
    /* Robustness: ILLEGAL opcode $4AFC (mode7,reg4=#imm) → DC.W */
    DIS_DCW("TAS #imm = ILLEGAL", 0x4A, 0xFC);

    /* ----------------------------------------------------------------
     * INTENT[INT-DIS-079 → TC-DIS-472..480 → REQ-DIS-046 → UFR-HEX-005]:
     * NBCD <ea> — byte, data alterable EA modes.
     * An (mode 1) and #imm (mode 7 reg 4) are invalid.
     * ---------------------------------------------------------------- */
    printf("\n[UC14B] NBCD\n");

    /* NBCD D0 : 0x4800 | 0 = 0x4800 */
    DIS1("NBCD D0",    0x48, 0x00, 1, "NBCD", "D0");
    /* NBCD (A0) : 0x4800 | (2<<3) | 0 = 0x4810 */
    DIS1("NBCD (A0)",  0x48, 0x10, 1, "NBCD", "(A0)");
    /* NBCD -(A3) : 0x4800 | (4<<3) | 3 = 0x4823 */
    DIS1("NBCD -(A3)", 0x48, 0x23, 1, "NBCD", "-(A3)");

    /* Robustness: An mode (mode 1) → DC.W */
    DIS_DCW("NBCD An invalid",   0x48, 0x08);
    /* Robustness: #imm (mode7,reg4) → DC.W */
    DIS_DCW("NBCD #imm invalid", 0x48, 0x3C);

    /* ----------------------------------------------------------------
     * INTENT[INT-DIS-080 → TC-DIS-481..486 → REQ-DIS-047 → UFR-HEX-005]:
     * CHK.W <ea>,Dn — data addressing EA (no An).
     * Dn field = bits 11-9 (destination). An (mode 1) source → DC.W.
     * ---------------------------------------------------------------- */
    printf("\n[UC14B] CHK.W\n");

    /* CHK.W D1,D2 : 0x4180 | (2<<9) | (0<<3) | 1 = 0x4581 */
    DIS1("CHK.W D1,D2",    0x45, 0x81, 1, "CHK.W", "D1,D2");
    /* CHK.W (A0),D3 : 0x4180 | (3<<9) | (2<<3) | 0 = 0x4790 */
    DIS1("CHK.W (A0),D3",  0x47, 0x90, 1, "CHK.W", "(A0),D3");

    /* Robustness: An source (mode 1) → DC.W
     * CHK.W A0,D0 : 0x4180 | (0<<9) | (1<<3) | 0 = 0x4188 */
    DIS_DCW("CHK.W An src invalid", 0x41, 0x88);

    /* ----------------------------------------------------------------
     * INTENT[INT-DIS-081 → TC-DIS-487..494 → REQ-DIS-048 → UFR-HEX-005]:
     * SBCD Ry,Rx (mode=0) or SBCD -(Ay),-(Ax) (mode=1).
     * Encoding: 1000 Rx 1 00 RM Ry  — Rx=bits 11-9, Ry=bits 2-0, RM=bit3
     * ---------------------------------------------------------------- */
    printf("\n[UC14B] SBCD\n");

    /* SBCD D1,D2 : 0x8100 | (2<<9) | 0 | 1 = 0x8501 */
    DIS1("SBCD D1,D2",       0x85, 0x01, 1, "SBCD", "D1,D2");
    /* SBCD -(A1),-(A2) : 0x8100 | (2<<9) | (1<<3) | 1 = 0x8509 */
    DIS1("SBCD -(A1),-(A2)", 0x85, 0x09, 1, "SBCD", "-(A1),-(A2)");

    /* ----------------------------------------------------------------
     * INTENT[INT-DIS-082 → TC-DIS-495..502 → REQ-DIS-049 → UFR-HEX-005]:
     * ABCD Ry,Rx (mode=0) or ABCD -(Ay),-(Ax) (mode=1).
     * Encoding: 1100 Rx 1 00 RM Ry  — same pattern as SBCD in group C.
     * ---------------------------------------------------------------- */
    printf("\n[UC14B] ABCD\n");

    /* ABCD D3,D4 : 0xC100 | (4<<9) | 0 | 3 = 0xC903 */
    DIS1("ABCD D3,D4",       0xC9, 0x03, 1, "ABCD", "D3,D4");
    /* ABCD -(A0),-(A1) : 0xC100 | (1<<9) | (1<<3) | 0 = 0xC308 */
    DIS1("ABCD -(A0),-(A1)", 0xC3, 0x08, 1, "ABCD", "-(A0),-(A1)");

    /* ----------------------------------------------------------------
     * INTENT[INT-DIS-083 → TC-DIS-503..510 → REQ-DIS-050 → UFR-HEX-005]:
     * MOVEP.W/.L  mem→reg or reg→mem with d16 extension word.
     * Encoding: 0000 Dn 1 dir sz 001 An + d16
     *   dir=0 mem→reg: d16(An),Dn
     *   dir=1 reg→mem: Dn,d16(An)
     *   sz=0 → .W   sz=1 → .L
     * ---------------------------------------------------------------- */
    printf("\n[UC14B] MOVEP\n");

    /* MOVEP.W $4(A0),D1 — dir=0 sz=0 : 0x0108|(1<<9)|0 = 0x0308 + 0x0004 */
    DIS2("MOVEP.W mem->reg", 0x03, 0x08, 0x00, 0x04,
         "MOVEP.W", "$4(A0),D1");
    /* MOVEP.W D2,−$2(A1) — dir=1 sz=0 : 0x0188|(2<<9)|1 = 0x0589 + 0xFFFE */
    DIS2("MOVEP.W reg->mem", 0x05, 0x89, 0xFF, 0xFE,
         "MOVEP.W", "D2,-$2(A1)");
    /* MOVEP.L $10(A2),D3 — dir=0 sz=1 : 0x0148|(3<<9)|2 = 0x074A + 0x0010 */
    DIS2("MOVEP.L mem->reg", 0x07, 0x4A, 0x00, 0x10,
         "MOVEP.L", "$10(A2),D3");
    /* MOVEP.L D4,$0(A3)  — dir=1 sz=1 : 0x01C8|(4<<9)|3 = 0x09CB + 0x0000 */
    DIS2("MOVEP.L reg->mem", 0x09, 0xCB, 0x00, 0x00,
         "MOVEP.L", "D4,$0(A3)");

    /* Robustness: MOVEP with buffer too short (only opcode, no d16) → DC.W */
    {
        static const st_u8_t _buf[2] = { 0x03, 0x08 };
        disasm_result_t _r;
        memset(&_r, 0, sizeof(_r));
        disasm_one(_buf, 2, 0x1000, &_r);
        UC_TEST("[R] MOVEP short buf → DC.W",
                strcmp(_r.szMnemonic, "DC.W") == 0);
    }

    printf("\n--- UC14B: %d failure(s) ---\n", g_uc_fails);
    return g_uc_fails ? 1 : 0;
}
