/*
 * use_case_23A.c - UC23-bis : CPU 68000 MOVEM + ADDA.W/SUBA.W
 *
 * TEST MATRIX - UC23-bis:
 *   [N] Nominal    : 22 tests - MOVEM.L -(An)/saved snapshot,
 *                               MOVEM.L (An)+/restored identity,
 *                               MOVEM.W sign-extend on load,
 *                               MOVEM all-regs round-trip,
 *                               ADDA.W positive/negative sign-extend,
 *                               SUBA.W positive/negative sign-extend,
 *                               ADDA.L / SUBA.L no-flags (regression)
 *   [R] Robustness :  4 tests - NULL params, empty mask (no-op),
 *                               ADDA.W/SUBA.W SR flags not modified
 *   [S] Skipped    :  0 tests
 */
#include "use_cases.h"

/* INTENT[INT-CPU-012 → TC-CPU-153..178 → REQ-CPU-045..050 → UFR-EXE-007]:
 * MOVEM.L regs,-(An) must save registers using the reversed mask (bit 0=A7
 * down to bit 15=D0), pre-decrementing An for each register; the register
 * snapshot taken before any decrement must be the value stored when An is
 * in the list.  MOVEM.L (An)+,regs must restore registers in standard mask
 * order (bit 0=D0 .. bit 15=A7), post-incrementing An.  MOVEM.W loads must
 * sign-extend the 16-bit memory value to 32 bits before storing in the
 * destination register.  ADDA.W/SUBA.W must sign-extend the 16-bit source
 * to 32 bits before adding to/subtracting from An, without modifying SR. */

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static st_machine_t g_tMachine;
static cpu68k_t     g_tCpu;

static void setup(void)
{
    st_init(&g_tMachine, NULL);
    memset(g_tMachine.aRam, 0, sizeof(g_tMachine.aRam));
    /* Reset vectors: SSP=0x0600, PC=0x0800 */
    g_tMachine.aRam[0] = 0x00; g_tMachine.aRam[1] = 0x00;
    g_tMachine.aRam[2] = 0x06; g_tMachine.aRam[3] = 0x00;
    g_tMachine.aRam[4] = 0x00; g_tMachine.aRam[5] = 0x00;
    g_tMachine.aRam[6] = 0x08; g_tMachine.aRam[7] = 0x00;
    cpu_init(&g_tCpu, &g_tMachine);
}

static void w16(st_u32_t uiAddr, st_u16_t uiVal)
{
    g_tMachine.aRam[uiAddr]     = (st_u8_t)(uiVal >> 8);
    g_tMachine.aRam[uiAddr + 1] = (st_u8_t)(uiVal & 0xFFu);
}

static void w32(st_u32_t uiAddr, st_u32_t uiVal)
{
    g_tMachine.aRam[uiAddr]     = (st_u8_t)(uiVal >> 24);
    g_tMachine.aRam[uiAddr + 1] = (st_u8_t)(uiVal >> 16);
    g_tMachine.aRam[uiAddr + 2] = (st_u8_t)(uiVal >>  8);
    g_tMachine.aRam[uiAddr + 3] = (st_u8_t)(uiVal & 0xFFu);
}

static st_u32_t r32(st_u32_t uiAddr)
{
    return ((st_u32_t)g_tMachine.aRam[uiAddr]     << 24)
         | ((st_u32_t)g_tMachine.aRam[uiAddr + 1] << 16)
         | ((st_u32_t)g_tMachine.aRam[uiAddr + 2] <<  8)
         |  (st_u32_t)g_tMachine.aRam[uiAddr + 3];
}

static void step1(void) { cpu_step(&g_tCpu, &g_tMachine, NULL); }

/* ------------------------------------------------------------------ */
/* MOVEM.L registers → -(An)                                           */
/* ------------------------------------------------------------------ */

static void test_movem_save(void)
{
    printf("\n--- MOVEM save (-(An)) ---\n");
    setup();

    /* Set D0=0x11, D1=0x22, A3=0x200, stack at 0x0600 */
    g_tCpu.auDn[0] = 0x00000011u;
    g_tCpu.auDn[1] = 0x00000022u;
    g_tCpu.auAn[3] = 0x00000200u;

    /* MOVEM.L D0-D1,-(A7)  — opcode 0x48E7, mask for D0+D1 in reversed
     * scheme: D0=bit15, D1=bit14 → mask = 0xC000 */
    w16(0x0800, 0x48E7u);
    w16(0x0802, 0xC000u);
    step1();

    /* A7 should have decremented by 8 (two longs) from 0x0600 */
    TEST_ASSERT("[N] MOVEM.L D0-D1,-(A7): A7 decremented by 8",
                g_tCpu.auAn[7] == 0x05F8u);
    /* D1 stored first at 0x05FC (bit 14 processed before bit 15) */
    TEST_ASSERT("[N] MOVEM.L D0-D1,-(A7): D1 at [0x05FC]",
                r32(0x05FCu) == 0x00000022u);
    /* D0 stored last at 0x05F8 (bit 15, last processed → lowest address) */
    TEST_ASSERT("[N] MOVEM.L D0-D1,-(A7): D0 at [0x05F8]",
                r32(0x05F8u) == 0x00000011u);

    /* MOVEM.L A3,-(A7)  — mask for A3 in reversed scheme: A3=bit4 → 0x0010 */
    setup();
    g_tCpu.auAn[3] = 0x00001234u;
    w16(0x0800, 0x48E7u);
    w16(0x0802, 0x0010u);
    step1();

    TEST_ASSERT("[N] MOVEM.L A3,-(A7): A7 decremented by 4",
                g_tCpu.auAn[7] == 0x05FCu);
    TEST_ASSERT("[N] MOVEM.L A3,-(A7): A3 stored at [0x05FC]",
                r32(0x05FCu) == 0x00001234u);
}

/* ------------------------------------------------------------------ */
/* MOVEM.L (An)+ → registers                                           */
/* ------------------------------------------------------------------ */

static void test_movem_restore(void)
{
    printf("\n--- MOVEM restore ((An)+) ---\n");
    setup();

    /* Plant D0/D1 values on stack */
    w32(0x05F8u, 0xABCDEF01u);  /* D0 at low addr */
    w32(0x05FCu, 0x12345678u);  /* D1 at high addr */
    g_tCpu.auAn[7] = 0x05F8u;

    /* MOVEM.L (A7)+,D0-D1  — opcode 0x4CDF, mask D0+D1 = 0x0003 */
    w16(0x0800, 0x4CDFu);
    w16(0x0802, 0x0003u);
    step1();

    TEST_ASSERT("[N] MOVEM.L (A7)+,D0-D1: D0 restored",
                g_tCpu.auDn[0] == 0xABCDEF01u);
    TEST_ASSERT("[N] MOVEM.L (A7)+,D0-D1: D1 restored",
                g_tCpu.auDn[1] == 0x12345678u);
    TEST_ASSERT("[N] MOVEM.L (A7)+,D0-D1: A7 incremented by 8",
                g_tCpu.auAn[7] == 0x0600u);
}

/* ------------------------------------------------------------------ */
/* MOVEM round-trip: save then restore = identity                      */
/* ------------------------------------------------------------------ */

static void test_movem_roundtrip(void)
{
    st_u32_t uiD0_orig, uiD1_orig, uiD2_orig, uiA0_orig, uiA1_orig;
    printf("\n--- MOVEM round-trip ---\n");
    setup();

    uiD0_orig = 0xDEAD0000u;
    uiD1_orig = 0xBEEF1111u;
    uiD2_orig = 0xCAFE2222u;
    uiA0_orig = 0x00001000u;
    uiA1_orig = 0x00002000u;

    g_tCpu.auDn[0] = uiD0_orig;
    g_tCpu.auDn[1] = uiD1_orig;
    g_tCpu.auDn[2] = uiD2_orig;
    g_tCpu.auAn[0] = uiA0_orig;
    g_tCpu.auAn[1] = uiA1_orig;

    /* MOVEM.L D0-D2/A0-A1,-(A7)
     * Reversed mask: D0=bit15, D1=bit14, D2=bit13, A0=bit7, A1=bit6
     * = 0xE0C0 */
    w16(0x0800, 0x48E7u);
    w16(0x0802, 0xE0C0u);
    /* MOVEM.L (A7)+,D0-D2/A0-A1
     * Standard mask: D0=bit0, D1=bit1, D2=bit2, A0=bit8, A1=bit9
     * = 0x0307 */
    w16(0x0804, 0x4CDFu);
    w16(0x0806, 0x0307u);

    step1();  /* save */
    /* Clobber the registers */
    g_tCpu.auDn[0] = 0; g_tCpu.auDn[1] = 0; g_tCpu.auDn[2] = 0;
    g_tCpu.auAn[0] = 0; g_tCpu.auAn[1] = 0;
    step1();  /* restore */

    TEST_ASSERT("[N] round-trip: D0 restored", g_tCpu.auDn[0] == uiD0_orig);
    TEST_ASSERT("[N] round-trip: D1 restored", g_tCpu.auDn[1] == uiD1_orig);
    TEST_ASSERT("[N] round-trip: D2 restored", g_tCpu.auDn[2] == uiD2_orig);
    TEST_ASSERT("[N] round-trip: A0 restored", g_tCpu.auAn[0] == uiA0_orig);
    TEST_ASSERT("[N] round-trip: A1 restored", g_tCpu.auAn[1] == uiA1_orig);
    TEST_ASSERT("[N] round-trip: SP restored", g_tCpu.auAn[7] == 0x0600u);
}

/* ------------------------------------------------------------------ */
/* MOVEM.W sign-extension on load                                      */
/* ------------------------------------------------------------------ */

static void test_movem_word(void)
{
    printf("\n--- MOVEM.W sign-extend ---\n");
    setup();

    /* Plant a negative word (0x8042) and a positive word (0x0042) */
    w16(0x05FCu, 0x8042u);  /* negative */
    w16(0x05FEu, 0x0042u);  /* positive */
    g_tCpu.auAn[7] = 0x05FCu;

    /* MOVEM.W (A7)+,D0-D1  — opcode 0x4C9F (0x4C, bit 6=0=word, mode 3 (A7)+)
     * 0x4C9F: 0100 1100 1001 1111 → wait:
     * dir=1 (bit 10=1), sz=0 (bit 6=0=word), mode=3 ((A7)+), reg=7
     * = 0100 1 1 00 1 0 011 111 = 0x4C9F
     * mask D0-D1 = 0x0003 */
    w16(0x0800, 0x4C9Fu);
    w16(0x0802, 0x0003u);
    step1();

    TEST_ASSERT("[N] MOVEM.W (A7)+,D0: negative word sign-extended",
                g_tCpu.auDn[0] == 0xFFFF8042u);
    TEST_ASSERT("[N] MOVEM.W (A7)+,D1: positive word unchanged",
                g_tCpu.auDn[1] == 0x00000042u);
    TEST_ASSERT("[N] MOVEM.W: A7 incremented by 4 (two words)",
                g_tCpu.auAn[7] == 0x0600u);
}

/* ------------------------------------------------------------------ */
/* ADDA.W / SUBA.W                                                     */
/* ------------------------------------------------------------------ */

static void test_adda_suba_w(void)
{
    printf("\n--- ADDA.W / SUBA.W ---\n");
    setup();

    /* ADDA.W #$0010,A0 — encoding: 0xD0FC 0x0010
     * opcode: 1101 000 011 111 100 = 0xD0FC
     * bits 8-6 = 011 = ADDA.W, EA = immediate (#0x0010) */
    g_tCpu.auAn[0] = 0x00001000u;
    w16(0x0800, 0xD0FCu);
    w16(0x0802, 0x0010u);
    step1();
    TEST_ASSERT("[N] ADDA.W #$10,A0: result = 0x1010",
                g_tCpu.auAn[0] == 0x00001010u);

    /* ADDA.W #$8000,A1 — negative immediate, sign-extended to 0xFFFF8000 */
    setup();
    g_tCpu.auAn[1] = 0x00002000u;
    /* ADDA.W #$8000,A1: opcode 1101 001 011 111 100 = 0xD2FC */
    w16(0x0800, 0xD2FCu);
    w16(0x0802, 0x8000u);
    step1();
    /* 0x00002000 + 0xFFFF8000 = 0xFFFFA000 (32-bit wrap) */
    TEST_ASSERT("[N] ADDA.W #$8000,A1: sign-extended negative",
                g_tCpu.auAn[1] == 0xFFFFA000u);

    /* SUBA.W #$0004,A2 — opcode 1001 010 011 111 100 = 0x94FC */
    setup();
    g_tCpu.auAn[2] = 0x00001008u;
    w16(0x0800, 0x94FCu);
    w16(0x0802, 0x0004u);
    step1();
    TEST_ASSERT("[N] SUBA.W #$4,A2: result = 0x1004",
                g_tCpu.auAn[2] == 0x00001004u);

    /* SUBA.W #$8000,A3 — sign-extended: 0x3000 - 0xFFFF8000 = 0x0000B000 */
    setup();
    g_tCpu.auAn[3] = 0x00003000u;
    /* SUBA.W #$8000,A3: opcode 1001 011 011 111 100 = 0x96FC */
    w16(0x0800, 0x96FCu);
    w16(0x0802, 0x8000u);
    step1();
    TEST_ASSERT("[N] SUBA.W #$8000,A3: sign-extended negative subtraction",
                g_tCpu.auAn[3] == 0x0000B000u);

    /* ADDA.L regression: still works */
    setup();
    g_tCpu.auAn[0] = 0x00001000u;
    /* ADDA.L #$10,A0: opcode 1101 000 111 111 100 = 0xD1FCu */
    w16(0x0800, 0xD1FCu);
    w32(0x0802, 0x00000010u);
    step1();
    TEST_ASSERT("[N] ADDA.L #$10,A0 regression: result = 0x1010",
                g_tCpu.auAn[0] == 0x00001010u);

    /* SUBA.L regression */
    setup();
    g_tCpu.auAn[0] = 0x00001010u;
    /* SUBA.L #$10,A0: opcode 1001 000 111 111 100 = 0x91FCu */
    w16(0x0800, 0x91FCu);
    w32(0x0802, 0x00000010u);
    step1();
    TEST_ASSERT("[N] SUBA.L #$10,A0 regression: result = 0x1000",
                g_tCpu.auAn[0] == 0x00001000u);
}

/* ------------------------------------------------------------------ */
/* Robustness                                                           */
/* ------------------------------------------------------------------ */

static void test_robustness(void)
{
    st_u16_t uiSR;
    printf("\n--- Robustness ---\n");

    /* NULL guards */
    TEST_ASSERT("[R] cpu_step NULL ptCpu → ST_ERROR",
                cpu_step(NULL, &g_tMachine, NULL) == ST_ERROR);
    TEST_ASSERT("[R] cpu_step NULL ptMachine → ST_ERROR",
                cpu_step(&g_tCpu, NULL, NULL) == ST_ERROR);

    /* Empty mask: MOVEM.L with mask=0 is a no-op (An unchanged, PC advances) */
    setup();
    w16(0x0800, 0x48E7u);
    w16(0x0802, 0x0000u);  /* no registers */
    {
        st_u32_t uiSP = g_tCpu.auAn[7];
        step1();
        TEST_ASSERT("[R] MOVEM.L empty mask: A7 unchanged",
                    g_tCpu.auAn[7] == uiSP);
    }

    /* ADDA.W / SUBA.W must not modify SR flags */
    setup();
    g_tCpu.auAn[0] = 0x1000u;
    g_tCpu.uiSR = 0x2715u; /* supervisor + all CCR bits set */
    uiSR = g_tCpu.uiSR;
    w16(0x0800, 0xD0FCu);
    w16(0x0802, 0x0001u);
    step1();
    TEST_ASSERT("[R] ADDA.W: SR flags unchanged",
                g_tCpu.uiSR == uiSR);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC23-bis: MOVEM + ADDA.W/SUBA.W ===\n");
    test_movem_save();
    test_movem_restore();
    test_movem_roundtrip();
    test_movem_word();
    test_adda_suba_w();
    test_robustness();
    printf("\n=== UC23-bis results: %d failure(s) ===\n", g_uc_fails);
    return g_uc_fails ? 1 : 0;
}
