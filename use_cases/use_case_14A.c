/*
 * use_case_14A.c - UC14A: 68000 disassembler — completing group 4 and 5
 *
 *   Group 0x4: MOVEM.W/L (store -(An) / load (An)+)
 *              MOVE.W SR,<ea> / MOVE.W <ea>,SR / MOVE.W <ea>,CCR
 *   Group 0x5: Scc <ea> (16 conditions)
 *              DBcc Dn,<label> / DBRA Dn,<label> (16 conditions)
 *
 * Opcode construction key:
 *   MOVEM store: 0x4880 | (sz<<6) | (mode<<3) | reg  + mask_word
 *     sz=0 → .W,  sz=1 → .L
 *     mode 4 (-(An)): mask bit-reversed (A7=bit0 ... D0=bit15)
 *   MOVEM load:  0x4C80 | (sz<<6) | (mode<<3) | reg  + mask_word
 *     mode 3 ((An)+): mask normal (D0=bit0 ... A7=bit15)
 *
 *   MOVE.W SR,Dn:   0x40C0 | Dn
 *   MOVE.W <ea>,SR: 0x46C0 | ea
 *   MOVE.W <ea>,CCR:0x44C0 | ea
 *
 *   Scc:  0x50C0 | (cond<<8) | (mode<<3) | reg   (cond bits 11-8)
 *   DBcc: 0x50C8 | (cond<<8) | Dn         + disp16
 *
 * TEST MATRIX - UC14A:
 *   [N] Nominal    : 86 assertions — 8 MOVEM store (4 instrs × 3),
 *                                    14 MOVEM load (4 instrs × 3 + 2 empty),
 *                                    18 MOVE SR/CCR (6 instrs × 3),
 *                                    18 Scc (6 instrs × 3),
 *                                    24 DBcc/DBRA (8 instrs × 3)
 *   [R] Robustness :  8 assertions — invalid EA modes, short buffer,
 *                                    An source, #imm destination
 *   [S] Skipped    :  0
 *   Total          : 94 assertions (40 instruction scenarios)
 *
 * Traceability:
 *   INT-DIS-072..077 → TC-DIS-420..459 → REQ-DIS-039..044 → UFR-HEX-005
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

/* Multi-byte buffer: check wc + mnem + ops */
#define DISN(desc, buf, len, addr, wc, mnem, ops)                        \
    do {                                                                  \
        disasm_result_t _r;                                               \
        memset(&_r, 0, sizeof(_r));                                       \
        disasm_one((buf), (len), (addr), &_r);                            \
        UC_TEST("[N] " desc " wc",   _r.iWordCount == (wc));             \
        UC_TEST("[N] " desc " mnem", strcmp(_r.szMnemonic, (mnem)) == 0); \
        UC_TEST("[N] " desc " ops",  strcmp(_r.szOperands, (ops))  == 0); \
    } while (0)

/* Robustness: check result is DC.W */
#define DIS_DCW(desc, buf, len)                                           \
    do {                                                                  \
        disasm_result_t _r;                                               \
        memset(&_r, 0, sizeof(_r));                                       \
        disasm_one((buf), (len), 0x1000, &_r);                            \
        UC_TEST("[R] " desc " is DC.W",                                   \
                strcmp(_r.szMnemonic, "DC.W") == 0);                      \
    } while (0)

/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC14A: disassembler MOVEM / Scc / DBcc / MOVE SR/CCR ===\n\n");

    /* INTENT[INT-DIS-072 → TC-DIS-420..423 → REQ-DIS-039,041]:
     * MOVEM store: mask word follows opcode, -(An) uses reversed bit
     * order (bit0=A7 ... bit15=D0), non-predecrement modes use normal
     * order; modes 0/1/3 (Dn/An/(An)+) are rejected. */

    /* ================================================================
     * MOVEM store (registers → memory)
     * ================================================================ */

    /* MOVEM.L D0-D7/A0-A7,-(A7) — all registers, predecrement A7
     * Opcode: 0x48E7 = 0100 1000 1110 0111 (mode=4/-(An), reg=7, sz=1/.L)
     * Mask (reversed): all bits set = 0xFFFF
     * bReverse=1: bit i → register 15-i
     *   bit15 → D0, bit14 → D1, ..., bit8 → D7
     *   bit7  → A0, bit6 → A1, ..., bit0 → A7
     * So 0xFFFF = D0-D7/A0-A7
     */
    {
        static const st_u8_t buf[] = { 0x48,0xE7, 0xFF,0xFF };
        DISN("MOVEM.L D0-D7/A0-A7,-(A7)", buf, 4, 0x1000, 2,
             "MOVEM.L", "D0-D7/A0-A7,-(A7)");
    }

    /* MOVEM.L D0/A6,-(A7) — D0 and A6
     * Mask (reversed): D0 → bit15 = 0x8000, A6 → bit1 = 0x0002
     * Encoded mask = 0x8002
     */
    {
        static const st_u8_t buf[] = { 0x48,0xE7, 0x80,0x02 };
        DISN("MOVEM.L D0/A6,-(A7)", buf, 4, 0x1000, 2,
             "MOVEM.L", "D0/A6,-(A7)");
    }

    /* MOVEM.W D0-D3,(A0) — D0..D3 to indirect, mode=2
     * Opcode: 0x4890 = 0100 1000 1001 0000 (mode=2/(An), reg=0, sz=0/.W)
     * Mask (normal): D0=bit0, D1=bit1, D2=bit2, D3=bit3 → 0x000F
     */
    {
        static const st_u8_t buf[] = { 0x48,0x90, 0x00,0x0F };
        DISN("MOVEM.W D0-D3,(A0)", buf, 4, 0x1000, 2,
             "MOVEM.W", "D0-D3,(A0)");
    }

    /* MOVEM.L A0-A7,d16(A5) — all address registers, mode=5
     * Opcode: 0x48ED = 0100 1000 1110 1101 (mode=5/d16(An), reg=5, sz=1/.L)
     * Mask (normal): A0-A7 → bits 8-15 → 0xFF00
     * Extension word: disp16 = 0x0020 (+32)
     */
    {
        static const st_u8_t buf[] = { 0x48,0xED, 0xFF,0x00, 0x00,0x20 };
        DISN("MOVEM.L A0-A7,$20(A5)", buf, 6, 0x1000, 3,
             "MOVEM.L", "A0-A7,$20(A5)");
    }

    /* INTENT[INT-DIS-073 → TC-DIS-424..432 → REQ-DIS-040,041]:
     * MOVEM load: normal mask order (bit0=D0 ... bit15=A7); (An)+ is
     * valid; -(An) is rejected; iExtSoFar=1 for PC-relative EA. */

    /* ================================================================
     * MOVEM load (memory → registers)
     * ================================================================ */

    /* MOVEM.L (A0)+,D0-D7 — postincrement, load all data registers
     * Opcode: 0x4CD8 = 0100 1100 1101 1000 (mode=3/(An)+, reg=0, sz=1/.L)
     * Mask (normal): D0-D7 → bits 0-7 → 0x00FF
     */
    {
        static const st_u8_t buf[] = { 0x4C,0xD8, 0x00,0xFF };
        DISN("MOVEM.L (A0)+,D0-D7", buf, 4, 0x1000, 2,
             "MOVEM.L", "(A0)+,D0-D7");
    }

    /* MOVEM.W (A7)+,D0-D7/A0-A7 — pop all from stack
     * Opcode: 0x4C9F = 0100 1100 1001 1111 (mode=3/(An)+, reg=7, sz=0/.W)
     * Mask: 0xFFFF
     */
    {
        static const st_u8_t buf[] = { 0x4C,0x9F, 0xFF,0xFF };
        DISN("MOVEM.W (A7)+,D0-D7/A0-A7", buf, 4, 0x1000, 2,
             "MOVEM.W", "(A7)+,D0-D7/A0-A7");
    }

    /* MOVEM.L (A0),D3/A5 — indirect, single registers from each group
     * Opcode: 0x4CD0 (mode=2, reg=0, sz=1/.L)
     * Mask (normal): D3=bit3=0x0008, A5=bit13=0x2000 → 0x2008
     */
    {
        static const st_u8_t buf[] = { 0x4C,0xD0, 0x20,0x08 };
        DISN("MOVEM.L (A0),D3/A5", buf, 4, 0x1000, 2,
             "MOVEM.L", "(A0),D3/A5");
    }

    /* MOVEM.L $1234.W,D0-D3 — abs.W EA, load
     * Opcode: 0x4CF8 = 0100 1100 1111 1000 (mode=7, reg=0, sz=1)
     * Mask (normal): D0-D3 bits 0-3 → 0x000F
     * EA ext: 0x1234 = abs.W
     */
    {
        static const st_u8_t buf[] = { 0x4C,0xF8, 0x00,0x0F, 0x12,0x34 };
        DISN("MOVEM.L $1234.W,D0-D3", buf, 6, 0x1000, 3,
             "MOVEM.L", "$1234.W,D0-D3");
    }

    /* ================================================================
     * MOVEM with mask = 0x0000 — edge case (no registers)
     * Should still decode as MOVEM with "(none)" or similar
     * ================================================================ */
    {
        static const st_u8_t buf[] = { 0x4C,0xD8, 0x00,0x00 };
        disasm_result_t r;
        memset(&r, 0, sizeof(r));
        disasm_one(buf, 4, 0x1000, &r);
        UC_TEST("[N] MOVEM empty mask wc=2", r.iWordCount == 2);
        UC_TEST("[N] MOVEM empty mask mnem",
                strcmp(r.szMnemonic, "MOVEM.L") == 0);
    }

    /* INTENT[INT-DIS-074 → TC-DIS-433..438 → REQ-DIS-044]:
     * sz=3 (bits 7-6=11) in NEGX/CLR/NEG/NOT groups (0x40/0x42/0x44/0x46)
     * decodes as MOVE to/from SR/CCR; An source (mode=1) is rejected. */

    /* ================================================================
     * MOVE SR / CCR
     * ================================================================ */

    /* MOVE.W SR,D0  — 0x40C0 */
    DIS1("MOVE.W SR,D0", 0x40,0xC0, 1, "MOVE.W", "SR,D0");

    /* MOVE.W SR,D7  — 0x40C7 */
    DIS1("MOVE.W SR,D7", 0x40,0xC7, 1, "MOVE.W", "SR,D7");

    /* MOVE.W SR,(A0) — 0x40D0 */
    DIS1("MOVE.W SR,(A0)", 0x40,0xD0, 1, "MOVE.W", "SR,(A0)");

    /* MOVE.W D0,CCR — 0x44C0 */
    DIS1("MOVE.W D0,CCR", 0x44,0xC0, 1, "MOVE.W", "D0,CCR");

    /* MOVE.W D0,SR  — 0x46C0 */
    DIS1("MOVE.W D0,SR",  0x46,0xC0, 1, "MOVE.W", "D0,SR");

    /* MOVE.W #$2700,SR — 0x46FC + 0x2700 */
    {
        static const st_u8_t buf[] = { 0x46,0xFC, 0x27,0x00 };
        DISN("MOVE.W #$2700,SR", buf, 4, 0x1000, 2, "MOVE.W", "#$2700,SR");
    }

    /* INTENT[INT-DIS-075 → TC-DIS-439..444 → REQ-DIS-042]:
     * Scc: cond in bits 11-8, EA in bits 5-0; 16 mnemonics; #imm
     * destination (mode=7,reg=4) is rejected → DC.W. */

    /* ================================================================
     * Scc (set byte conditionally)
     * ================================================================ */

    /* ST D0  — 0x50C0 (condition=0, always true, mode=0 Dn) */
    DIS1("ST D0", 0x50,0xC0, 1, "ST", "D0");

    /* SF D0  — 0x51C0 (condition=1, always false) */
    DIS1("SF D0", 0x51,0xC0, 1, "SF", "D0");

    /* SNE D3 — 0x56C3 (condition=6, NE) */
    DIS1("SNE D3", 0x56,0xC3, 1, "SNE", "D3");

    /* SEQ D5 — 0x57C5 (condition=7, EQ) */
    DIS1("SEQ D5", 0x57,0xC5, 1, "SEQ", "D5");

    /* SGE (A0) — 0x5CC0 | 0x10 = 0x5CD0... wait
     * SGE: cond=0xC → opcode bits 11-8 = 1100 → byte = (0x5C) or (0xC << 8) | 0xC0?
     * Scc: 0101 cccc 11 MMMRRR
     * SGE (cond=0xC=1100): 0101 1100 1100 0010 = 0x5CC2 (mode=0, D2)
     */
    DIS1("SGE D2", 0x5C,0xC2, 1, "SGE", "D2");

    /* SLE (A0)+ — cond=0xF → 0x5FC0 | mode3 reg0 = 0x5FC8...
     * 0101 1111 11 011 000 = 0x5FD8 (mode=3/(A0)+)
     */
    DIS1("SLE (A0)+", 0x5F,0xD8, 1, "SLE", "(A0)+");

    /* INTENT[INT-DIS-076 → TC-DIS-445..452 → REQ-DIS-043]:
     * DBcc: mode=001 in bits 5-3; cond=1 uses "DBRA" mnemonic; target
     * = (addr+2+disp16) & 0xFFFFFF displayed as $XXXXXX (6 hex digits). */

    /* ================================================================
     * DBcc / DBRA (decrement and branch)
     * ================================================================ */

    /* DBRA D0,$1010 — cond=1 (False → DBRA), Dn=D0
     * Opcode: 0x51C8 = 0101 0001 1100 1000 (cond=1, mode=001, reg=0)
     * disp16 = 0x000E → target = 0x1000 + 2 + 0x000E = 0x1010
     */
    {
        static const st_u8_t buf[] = { 0x51,0xC8, 0x00,0x0E };
        DISN("DBRA D0,$001010", buf, 4, 0x1000, 2,
             "DBRA", "D0,$001010");
    }

    /* DBRA D5,$0FF0 — negative displacement
     * Opcode: 0x51CD (cond=1, reg=5)
     * disp16 = 0xFFEE (signed -18) → target = 0x1000+2-18 = 0x0FF0
     */
    {
        static const st_u8_t buf[] = { 0x51,0xCD, 0xFF,0xEE };
        DISN("DBRA D5,$000FF0", buf, 4, 0x1000, 2,
             "DBRA", "D5,$000FF0");
    }

    /* DBT D0,$1010 — cond=0 (True → DBT) */
    {
        static const st_u8_t buf[] = { 0x50,0xC8, 0x00,0x0E };
        DISN("DBT D0,$001010", buf, 4, 0x1000, 2,
             "DBT", "D0,$001010");
    }

    /* DBNE D3,$1020 — cond=6, NE
     * Opcode: 0x56CB (cond=6, reg=3)
     * disp16 = 0x001E → target = 0x1000+2+0x001E = 0x1020
     */
    {
        static const st_u8_t buf[] = { 0x56,0xCB, 0x00,0x1E };
        DISN("DBNE D3,$001020", buf, 4, 0x1000, 2,
             "DBNE", "D3,$001020");
    }

    /* DBEQ D7,$0FF8 — cond=7, EQ, negative disp
     * Opcode: 0x57CF (cond=7, reg=7)
     * disp16 = 0xFFF6 (signed -10) → target = 0x1000+2-10 = 0x0FF8
     */
    {
        static const st_u8_t buf[] = { 0x57,0xCF, 0xFF,0xF6 };
        DISN("DBEQ D7,$000FF8", buf, 4, 0x1000, 2,
             "DBEQ", "D7,$000FF8");
    }

    /* DBLT D0,$1002 — cond=0xD, LT, minimum positive disp (+0)
     * target = 0x1000+2+0 = 0x1002
     */
    {
        static const st_u8_t buf[] = { 0x5D,0xC8, 0x00,0x00 };
        DISN("DBLT D0,$001002", buf, 4, 0x1000, 2,
             "DBLT", "D0,$001002");
    }

    /* DBGT D2 with zero crossing — cond=0xE, GT
     * Opcode: 0x5ECA (cond=0xE, reg=2)
     * disp16 = 0x000A → target = 0x1000+2+10 = 0x100C
     */
    {
        static const st_u8_t buf[] = { 0x5E,0xCA, 0x00,0x0A };
        DISN("DBGT D2,$00100C", buf, 4, 0x1000, 2,
             "DBGT", "D2,$00100C");
    }

    /* DBLE D1 — cond=0xF, LE
     * Opcode: 0x5FC9 (cond=0xF, reg=1)
     * disp16 = 0x0006 → target = 0x1008
     */
    {
        static const st_u8_t buf[] = { 0x5F,0xC9, 0x00,0x06 };
        DISN("DBLE D1,$001008", buf, 4, 0x1000, 2,
             "DBLE", "D1,$001008");
    }

    /* INTENT[INT-DIS-077 → TC-DIS-453..459 → REQ-DIS-039..044]:
     * Invalid modes/operands for MOVEM, Scc, DBcc, MOVE SR/CCR produce
     * DC.W: An/Dn/postincrement store, predecrement load, #imm Scc,
     * short buffer, An source for CCR/SR. */

    /* ================================================================
     * Robustness
     * ================================================================ */

    /* MOVEM.L store with An dest (mode=1) → DC.W
     * 0x4880-0x48C7 are EXT.W/L (can't reach MOVEM decoder for mode=0).
     * Use mode=1 (.L size) to verify An rejection in MOVEM.L path. */
    {
        static const st_u8_t buf[] = { 0x48,0xC8, 0xFF,0xFF };
        DIS_DCW("MOVEM.L store mode=1 (An) → DC.W", buf, 4);
    }

    /* MOVEM with An destination (mode=1) → DC.W */
    {
        static const st_u8_t buf[] = { 0x48,0x88, 0xFF,0xFF };
        DIS_DCW("MOVEM mode=1 (An) → DC.W", buf, 4);
    }

    /* MOVEM store with (An)+ (mode=3) → DC.W (not valid for store) */
    {
        static const st_u8_t buf[] = { 0x48,0x98, 0xFF,0xFF };
        DIS_DCW("MOVEM store mode=3 ((An)+) → DC.W", buf, 4);
    }

    /* MOVEM load with -(An) (mode=4) → DC.W (not valid for load) */
    {
        static const st_u8_t buf[] = { 0x4C,0xA0, 0xFF,0xFF };
        DIS_DCW("MOVEM load mode=4 (-(An)) → DC.W", buf, 4);
    }

    /* Scc with #imm dest (mode=7, reg=4) → DC.W */
    {
        static const st_u8_t buf[] = { 0x56,0xFC, 0x00,0x00 };
        DIS_DCW("SNE #imm → DC.W", buf, 4);
    }

    /* DBcc short buffer (only 2 bytes) → DC.W */
    {
        static const st_u8_t buf[] = { 0x51,0xC8 };
        DIS_DCW("DBRA D0 short buf → DC.W", buf, 2);
    }

    /* MOVEM short buffer (only 2 bytes, no mask word) → DC.W */
    {
        static const st_u8_t buf[] = { 0x48,0xE7 };
        DIS_DCW("MOVEM short buf → DC.W", buf, 2);
    }

    /* MOVE.W An,SR — An is invalid source for MOVE to SR → DC.W
     * 0x46C8 = MOVE An (mode=1, reg=0) to SR → invalid on 68000
     */
    {
        static const st_u8_t buf[] = { 0x46,0xC8, 0x00,0x00 };
        DIS_DCW("MOVE.W An,SR → DC.W", buf, 4);
    }

    printf("\n=== UC14A done: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
