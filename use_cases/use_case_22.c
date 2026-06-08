/*
 * use_case_22.c - UC22 : CPU 68000 ADD/SUB/CMP/AND/OR/EOR/shifts
 *
 * TEST MATRIX - UC22:
 *   [N] Nominal    : 62 tests - ADD/SUB/CMP/AND/OR/EOR/NEG/NOT/TST/
 *                               EXT/ADDQ/SUBQ/ADDI/SUBI/CMPI/ANDI/ORI/
 *                               EORI/MULU/MULS/DIVU/DIVS/shifts/rotations
 *   [R] Robustness :  8 tests - flags after edge cases, zero divide stub,
 *                               NULL params inherited from UC21
 *   [S] Skipped    :  0 tests
 */
#include "use_cases.h"

/* INTENT[INT-CPU-002 -> TC-CPU-002 -> REQ-CPU-001 -> UFR-CPU-001]:
 * cpu_step() must execute arithmetic, logic and shift 68000 instructions
 * updating data registers and SR flags (N,Z,V,C,X) per the M68000 PRM. */

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
    /* SSP=0x0600, PC=0x0800 */
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

static void step1(void)
{
    cpu_step(&g_tCpu, &g_tMachine, NULL);
}

static int flag_n(void) { return (g_tCpu.uiSR & CPU_SR_N) ? 1 : 0; }
static int flag_z(void) { return (g_tCpu.uiSR & CPU_SR_Z) ? 1 : 0; }
static int flag_c(void) { return (g_tCpu.uiSR & CPU_SR_C) ? 1 : 0; }
static int flag_v(void) { return (g_tCpu.uiSR & CPU_SR_V) ? 1 : 0; }
static int flag_x(void) { return (g_tCpu.uiSR & CPU_SR_X) ? 1 : 0; }

/* ------------------------------------------------------------------ */
/* ADD / SUB / CMP                                                      */
/* ------------------------------------------------------------------ */

static void test_add_sub_cmp(void)
{
    /* ADD.L D1,D0: D0=0x10, D1=0x05 → D0=0x15, N=0,Z=0,V=0,C=0 */
    setup();
    /* MOVEQ #$10,D0 */
    w16(0x0800, 0x7010);
    /* MOVEQ #5,D1 */
    w16(0x0802, 0x7205);
    /* ADD.L D1,D0: 1101 000 0 10 000 001 = 0xD080 | (sz=2<<6)=0x80 | 1
     * ADD.L D1,D0: opcode = 0xD081 */
    w16(0x0804, 0xD081);
    step1(); step1(); step1();
    TEST_ASSERT("[N] ADD.L D1,D0 result", g_tCpu.auDn[0] == 0x15u);
    TEST_ASSERT("[N] ADD.L D1,D0 N=0", flag_n() == 0);
    TEST_ASSERT("[N] ADD.L D1,D0 Z=0", flag_z() == 0);
    TEST_ASSERT("[N] ADD.L D1,D0 C=0", flag_c() == 0);
    TEST_ASSERT("[N] ADD.L D1,D0 V=0", flag_v() == 0);
    TEST_ASSERT("[N] ADD.L D1,D0 X=0", flag_x() == 0);

    /* ADD.W overflow: D0=0x7FFF + D1=0x0001 → 0x8000, V=1 */
    setup();
    /* MOVE.W #$7FFF,D0 */
    w16(0x0800, 0x303C); w16(0x0802, 0x7FFF);
    /* MOVEQ #1,D1 */
    w16(0x0804, 0x7201);
    /* ADD.W D1,D0 = 0xD041 */
    w16(0x0806, 0xD041);
    step1(); step1(); step1();
    TEST_ASSERT("[N] ADD.W overflow result", (g_tCpu.auDn[0] & 0xFFFFu) == 0x8000u);
    TEST_ASSERT("[N] ADD.W overflow V=1", flag_v() == 1);
    TEST_ASSERT("[N] ADD.W overflow N=1", flag_n() == 1);

    /* SUB.L D1,D0: D0=0x10, D1=0x03 → D0=0x0D */
    setup();
    w16(0x0800, 0x7010); /* MOVEQ #16,D0 */
    w16(0x0802, 0x7203); /* MOVEQ #3,D1 */
    /* SUB.L D1,D0 = 0x9081 */
    w16(0x0804, 0x9081);
    step1(); step1(); step1();
    TEST_ASSERT("[N] SUB.L D1,D0 result", g_tCpu.auDn[0] == 0x0Du);
    TEST_ASSERT("[N] SUB.L D1,D0 C=0",   flag_c() == 0);
    TEST_ASSERT("[N] SUB.L D1,D0 N=0",   flag_n() == 0);
    TEST_ASSERT("[N] SUB.L D1,D0 Z=0",   flag_z() == 0);

    /* SUB.L borrow: D0=0x01, D1=0x02 → -1 = 0xFFFFFFFF, C=1 */
    setup();
    w16(0x0800, 0x7001); /* MOVEQ #1,D0 */
    w16(0x0802, 0x7202); /* MOVEQ #2,D1 */
    w16(0x0804, 0x9081); /* SUB.L D1,D0 */
    step1(); step1(); step1();
    TEST_ASSERT("[N] SUB.L borrow result", g_tCpu.auDn[0] == 0xFFFFFFFFu);
    TEST_ASSERT("[N] SUB.L borrow C=1",   flag_c() == 1);
    TEST_ASSERT("[N] SUB.L borrow N=1",   flag_n() == 1);

    /* CMP.W #5,D0 with D0=5: Z=1 — flags only, D0 unchanged */
    setup();
    w16(0x0800, 0x7005); /* MOVEQ #5,D0 */
    /* CMPI.W #5,D0 = 0x0C40 + word 0x0005 */
    w16(0x0802, 0x0C40); w16(0x0804, 0x0005);
    step1(); step1();
    TEST_ASSERT("[N] CMPI.W equal Z=1", flag_z() == 1);
    TEST_ASSERT("[N] CMPI.W D0 unchanged", g_tCpu.auDn[0] == 5u);
}

/* ------------------------------------------------------------------ */
/* AND / OR / EOR                                                       */
/* ------------------------------------------------------------------ */

static void test_logic(void)
{
    /* AND.L D1,D0: 0xF0F0F0F0 & 0x0F0F0F0F → 0 */
    setup();
    /* MOVE.L #$F0F0F0F0,D0 */
    w16(0x0800, 0x203C); w16(0x0802, 0xF0F0); w16(0x0804, 0xF0F0);
    /* MOVE.L #$0F0F0F0F,D1 */
    w16(0x0806, 0x223C); w16(0x0808, 0x0F0F); w16(0x080A, 0x0F0F);
    /* AND.L D1,D0 = 0xC081 */
    w16(0x080C, 0xC081);
    step1(); step1(); step1();
    TEST_ASSERT("[N] AND.L result 0", g_tCpu.auDn[0] == 0u);
    TEST_ASSERT("[N] AND.L Z=1", flag_z() == 1);

    /* OR.W D1,D0: 0x00F0 | 0x000F → 0x00FF */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x00F0); /* MOVE.W #$F0,D0 */
    w16(0x0804, 0x323C); w16(0x0806, 0x000F); /* MOVE.W #$0F,D1 */
    /* OR.W D1,D0 = 0x8041 */
    w16(0x0808, 0x8041);
    step1(); step1(); step1();
    TEST_ASSERT("[N] OR.W result", (g_tCpu.auDn[0] & 0xFFFFu) == 0x00FFu);
    TEST_ASSERT("[N] OR.W N=0", flag_n() == 0);

    /* EOR.B D1,D0: 0xFF ^ 0xFF → 0x00, Z=1 */
    setup();
    w16(0x0800, 0x103C); w16(0x0802, 0x00FF); /* MOVE.B #$FF,D0 */
    w16(0x0804, 0x123C); w16(0x0806, 0x00FF); /* MOVE.B #$FF,D1 */
    /* EOR.B D1,D0 = 0xB100 */
    w16(0x0808, 0xB100);
    step1(); step1(); step1();
    TEST_ASSERT("[N] EOR.B result 0", (g_tCpu.auDn[0] & 0xFFu) == 0u);
    TEST_ASSERT("[N] EOR.B Z=1", flag_z() == 1);
}

/* ------------------------------------------------------------------ */
/* NEG / NOT / TST / EXT                                                */
/* ------------------------------------------------------------------ */

static void test_unary(void)
{
    /* NEG.L D0: D0=5 → D0=-5=0xFFFFFFFB, N=1, C=1 */
    setup();
    w16(0x0800, 0x7005); /* MOVEQ #5,D0 */
    /* NEG.L D0 = 0x4480 */
    w16(0x0802, 0x4480);
    step1(); step1();
    TEST_ASSERT("[N] NEG.L result", g_tCpu.auDn[0] == 0xFFFFFFFBu);
    TEST_ASSERT("[N] NEG.L N=1", flag_n() == 1);
    TEST_ASSERT("[N] NEG.L C=1", flag_c() == 1);

    /* NEG.L D0: D0=0 → 0, Z=1, C=0 */
    setup();
    w16(0x0800, 0x7000); /* MOVEQ #0,D0 */
    w16(0x0802, 0x4480);
    step1(); step1();
    TEST_ASSERT("[N] NEG.L 0 result", g_tCpu.auDn[0] == 0u);
    TEST_ASSERT("[N] NEG.L 0 Z=1", flag_z() == 1);
    TEST_ASSERT("[N] NEG.L 0 C=0", flag_c() == 0);

    /* NOT.W D0: D0=0x1234 → 0xEDCB, N=1 */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x1234); /* MOVE.W #$1234,D0 */
    /* NOT.W D0 = 0x4640 */
    w16(0x0804, 0x4640);
    step1(); step1();
    TEST_ASSERT("[N] NOT.W result", (g_tCpu.auDn[0] & 0xFFFFu) == 0xEDCBu);
    TEST_ASSERT("[N] NOT.W N=1", flag_n() == 1);

    /* TST.L D0: D0=0 → Z=1, flags updated, D0 unchanged */
    setup();
    w16(0x0800, 0x7000); /* MOVEQ #0,D0 */
    /* TST.L D0 = 0x4A80 */
    w16(0x0802, 0x4A80);
    step1(); step1();
    TEST_ASSERT("[N] TST.L Z=1", flag_z() == 1);
    TEST_ASSERT("[N] TST.L D0 unchanged", g_tCpu.auDn[0] == 0u);

    /* EXT.W D0: D0 byte=$FF → word=$FFFF, N=1 */
    setup();
    w16(0x0800, 0x103C); w16(0x0802, 0x00FF); /* MOVE.B #$FF,D0 */
    /* EXT.W D0 = 0x4880 */
    w16(0x0804, 0x4880);
    step1(); step1();
    TEST_ASSERT("[N] EXT.W result", (g_tCpu.auDn[0] & 0xFFFFu) == 0xFFFFu);
    TEST_ASSERT("[N] EXT.W N=1", flag_n() == 1);

    /* EXT.L D0: D0 word=$8000 → long=$FFFF8000, N=1 */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x8000); /* MOVE.W #$8000,D0 */
    /* EXT.L D0 = 0x48C0 */
    w16(0x0804, 0x48C0);
    step1(); step1();
    TEST_ASSERT("[N] EXT.L result", g_tCpu.auDn[0] == 0xFFFF8000u);
    TEST_ASSERT("[N] EXT.L N=1", flag_n() == 1);
}

/* ------------------------------------------------------------------ */
/* ADDQ / SUBQ                                                           */
/* ------------------------------------------------------------------ */

static void test_addq_subq(void)
{
    /* ADDQ.L #8,D0: D0=0 → 8 */
    setup();
    w16(0x0800, 0x7000); /* MOVEQ #0,D0 */
    /* ADDQ.L #8,D0 = 0x5080 (data=0→8, sz=2, mode=0, reg=0) */
    w16(0x0802, 0x5080);
    step1(); step1();
    TEST_ASSERT("[N] ADDQ.L #8 result", g_tCpu.auDn[0] == 8u);

    /* ADDQ.W #3,D0 */
    setup();
    w16(0x0800, 0x7000); /* MOVEQ #0,D0 */
    /* ADDQ.W #3,D0 = 0x5640 (data=3, sz=1, mode=0, reg=0)
     * 0101 011 0 01 000 000 = 0x5640 */
    w16(0x0802, 0x5640);
    step1(); step1();
    TEST_ASSERT("[N] ADDQ.W #3 result", (g_tCpu.auDn[0] & 0xFFFFu) == 3u);

    /* SUBQ.L #1,D0: D0=5 → 4 */
    setup();
    w16(0x0800, 0x7005); /* MOVEQ #5,D0 */
    /* SUBQ.L #1,D0 = 0x5380 (data=1, dir=1, sz=2, mode=0, reg=0)
     * 0101 001 1 10 000 000 = 0x5380 */
    w16(0x0802, 0x5380);
    step1(); step1();
    TEST_ASSERT("[N] SUBQ.L #1 result", g_tCpu.auDn[0] == 4u);
    TEST_ASSERT("[N] SUBQ.L #1 C=0",   flag_c() == 0);

    /* SUBQ.L #1,D0 with D0=0 → 0xFFFFFFFF, C=1 */
    setup();
    w16(0x0800, 0x7000); /* MOVEQ #0,D0 */
    w16(0x0802, 0x5380); /* SUBQ.L #1,D0 */
    step1(); step1();
    TEST_ASSERT("[N] SUBQ.L underflow result", g_tCpu.auDn[0] == 0xFFFFFFFFu);
    TEST_ASSERT("[N] SUBQ.L underflow C=1", flag_c() == 1);
}

/* ------------------------------------------------------------------ */
/* ADDI / SUBI / ANDI / ORI / EORI / CMPI                               */
/* ------------------------------------------------------------------ */

static void test_immediate(void)
{
    /* ADDI.L #$100,D0: D0=0 → 0x100 */
    setup();
    /* ADDI.L #$100,D0 = 0x0680 + $00000100 */
    w16(0x0800, 0x0680); w16(0x0802, 0x0000); w16(0x0804, 0x0100);
    step1();
    TEST_ASSERT("[N] ADDI.L result", g_tCpu.auDn[0] == 0x100u);
    TEST_ASSERT("[N] ADDI.L Z=0",   flag_z() == 0);

    /* SUBI.W #5,D0: D0=10 → 5 */
    setup();
    w16(0x0800, 0x300C); /* MOVE.W A4,D0 (use alternative) */
    /* Actually MOVEQ then SUBI */
    w16(0x0800, 0x700A); /* MOVEQ #10,D0 */
    /* SUBI.W #5,D0 = 0x0440 + $0005 */
    w16(0x0802, 0x0440); w16(0x0804, 0x0005);
    step1(); step1();
    TEST_ASSERT("[N] SUBI.W result", (g_tCpu.auDn[0] & 0xFFFFu) == 5u);

    /* ANDI.B #$0F,D0: D0=0xFF → 0x0F */
    setup();
    w16(0x0800, 0x103C); w16(0x0802, 0x00FF); /* MOVE.B #$FF,D0 */
    /* ANDI.B #$0F,D0 = 0x0200 + $000F */
    w16(0x0804, 0x0200); w16(0x0806, 0x000F);
    step1(); step1();
    TEST_ASSERT("[N] ANDI.B result", (g_tCpu.auDn[0] & 0xFFu) == 0x0Fu);

    /* ORI.W #$F000,D0: D0=0x0FFF → 0xFFFF */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x0FFF); /* MOVE.W #$0FFF,D0 */
    /* ORI.W #$F000,D0 = 0x0040 + $F000 */
    w16(0x0804, 0x0040); w16(0x0806, 0xF000);
    step1(); step1();
    TEST_ASSERT("[N] ORI.W result", (g_tCpu.auDn[0] & 0xFFFFu) == 0xFFFFu);

    /* EORI.L #$FFFFFFFF,D0: D0=0 → 0xFFFFFFFF, N=1 */
    setup();
    /* EORI.L #$FFFFFFFF,D0 = 0x0A80 + $FFFFFFFF */
    w16(0x0800, 0x0A80); w16(0x0802, 0xFFFF); w16(0x0804, 0xFFFF);
    step1();
    TEST_ASSERT("[N] EORI.L result", g_tCpu.auDn[0] == 0xFFFFFFFFu);
    TEST_ASSERT("[N] EORI.L N=1", flag_n() == 1);

    /* CMPI.L #$100,D0 with D0=0x100: Z=1, D0 unchanged */
    setup();
    /* ADDI.L #$100,D0 first */
    w16(0x0800, 0x0680); w16(0x0802, 0x0000); w16(0x0804, 0x0100);
    /* CMPI.L #$100,D0 = 0x0C80 + $00000100 */
    w16(0x0806, 0x0C80); w16(0x0808, 0x0000); w16(0x080A, 0x0100);
    step1(); step1();
    TEST_ASSERT("[N] CMPI.L Z=1",  flag_z() == 1);
    TEST_ASSERT("[N] CMPI.L D0 unchanged", g_tCpu.auDn[0] == 0x100u);
}

/* ------------------------------------------------------------------ */
/* Shifts / Rotations                                                   */
/* ------------------------------------------------------------------ */

static void test_shifts(void)
{
    /* ASL.L #1,D0: D0=1 → 2, C=0 */
    setup();
    w16(0x0800, 0x7001); /* MOVEQ #1,D0 */
    /* ASL.L #1,D0 = 1110 001 1 10 0 00 000 = 0xE380 */
    w16(0x0802, 0xE380);
    step1(); step1();
    TEST_ASSERT("[N] ASL.L #1 result", g_tCpu.auDn[0] == 2u);
    TEST_ASSERT("[N] ASL.L #1 C=0", flag_c() == 0);

    /* ASL.L #1 with D0=0x80000000 → 0, C=1 (MSB shifted out) */
    setup();
    w16(0x0800, 0x203C); w16(0x0802, 0x8000); w16(0x0804, 0x0000);
    w16(0x0806, 0xE380); /* ASL.L #1,D0 */
    step1(); step1();
    TEST_ASSERT("[N] ASL.L carry out C=1", flag_c() == 1);
    TEST_ASSERT("[N] ASL.L carry result 0", g_tCpu.auDn[0] == 0u);
    TEST_ASSERT("[N] ASL.L carry Z=1", flag_z() == 1);

    /* ASR.L #1,D0: D0=0x80000000 → 0xC0000000 (sign preserved) */
    setup();
    w16(0x0800, 0x203C); w16(0x0802, 0x8000); w16(0x0804, 0x0000);
    /* ASR.L #1,D0 = 1110 001 0 10 0 00 000 = 0xE280 */
    w16(0x0806, 0xE280);
    step1(); step1();
    TEST_ASSERT("[N] ASR.L sign preserved", g_tCpu.auDn[0] == 0xC0000000u);
    TEST_ASSERT("[N] ASR.L C=0", flag_c() == 0);

    /* LSR.W #4,D0: D0=0x00F0 → 0x000F */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x00F0); /* MOVE.W #$F0,D0 */
    /* LSR.W #4,D0 = 1110 100 0 01 0 01 000 = 0xE848 */
    w16(0x0804, 0xE848);
    step1(); step1();
    TEST_ASSERT("[N] LSR.W #4 result", (g_tCpu.auDn[0] & 0xFFFFu) == 0x000Fu);
    TEST_ASSERT("[N] LSR.W #4 C=0", flag_c() == 0);

    /* LSL.B #1,D0: D0=0x80 → 0x00, C=1, Z=1 */
    setup();
    w16(0x0800, 0x103C); w16(0x0802, 0x0080); /* MOVE.B #$80,D0 */
    /* LSL.B #1,D0 = 1110 001 1 00 0 01 000 = 0xE308 */
    w16(0x0804, 0xE308);
    step1(); step1();
    TEST_ASSERT("[N] LSL.B #1 result", (g_tCpu.auDn[0] & 0xFFu) == 0u);
    TEST_ASSERT("[N] LSL.B #1 C=1", flag_c() == 1);
    TEST_ASSERT("[N] LSL.B #1 Z=1", flag_z() == 1);

    /* ROL.W #1,D0: D0=0x8000 → 0x0001, C=1 */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x8000); /* MOVE.W #$8000,D0 */
    /* ROL.W #1,D0 = 1110 001 1 01 0 11 000 = 0xE358 */
    w16(0x0804, 0xE358);
    step1(); step1();
    TEST_ASSERT("[N] ROL.W result", (g_tCpu.auDn[0] & 0xFFFFu) == 0x0001u);
    TEST_ASSERT("[N] ROL.W C=1", flag_c() == 1);

    /* ROR.W #1,D0: D0=0x0001 → 0x8000, C=1 */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x0001); /* MOVE.W #1,D0 */
    /* ROR.W #1,D0 = 1110 001 0 01 0 11 000 = 0xE258 */
    w16(0x0804, 0xE258);
    step1(); step1();
    TEST_ASSERT("[N] ROR.W result", (g_tCpu.auDn[0] & 0xFFFFu) == 0x8000u);
    TEST_ASSERT("[N] ROR.W C=1", flag_c() == 1);
}

/* ------------------------------------------------------------------ */
/* MULU / MULS / DIVU / DIVS                                            */
/* ------------------------------------------------------------------ */

static void test_mul_div(void)
{
    /* MULU.W #3,D0: D0=word $0005 → 15 */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x0005); /* MOVE.W #5,D0 */
    /* MULU.W #3,D0 = 0xC0FC + 0x0003
     * MULU.W (imm),D0: 1100 000 0 11 111 100 = 0xC0FC */
    w16(0x0804, 0xC0FC); w16(0x0806, 0x0003);
    step1(); step1();
    TEST_ASSERT("[N] MULU.W #3 result", g_tCpu.auDn[0] == 15u);
    TEST_ASSERT("[N] MULU.W N=0", flag_n() == 0);
    TEST_ASSERT("[N] MULU.W Z=0", flag_z() == 0);

    /* MULS.W #-1,D0: D0=word 5 → -5 = 0xFFFFFFFB */
    setup();
    w16(0x0800, 0x303C); w16(0x0802, 0x0005); /* MOVE.W #5,D0 */
    /* MULS.W #-1,D0: 1100 000 1 11 111 100 = 0xC1FC */
    w16(0x0804, 0xC1FC); w16(0x0806, 0xFFFF);
    step1(); step1();
    TEST_ASSERT("[N] MULS.W #-1 result", g_tCpu.auDn[0] == 0xFFFFFFFBu);
    TEST_ASSERT("[N] MULS.W N=1", flag_n() == 1);

    /* DIVU.W #3,D0: D0=10 → quotient=3, remainder=1 */
    setup();
    /* Set D0 to 10 via ADDI */
    w16(0x0800, 0x0680); w16(0x0802, 0x0000); w16(0x0804, 0x000A);
    /* DIVU.W #3,D0: 1000 000 0 11 111 100 = 0x80FC */
    w16(0x0806, 0x80FC); w16(0x0808, 0x0003);
    step1(); step1();
    TEST_ASSERT("[N] DIVU.W quotient",   (g_tCpu.auDn[0] & 0xFFFFu) == 3u);
    TEST_ASSERT("[N] DIVU.W remainder",  ((g_tCpu.auDn[0] >> 16) & 0xFFFFu) == 1u);

    /* DIVS.W #-2,D0: D0 = -10 (as long) → quotient=5 */
    setup();
    /* Set D0 = $FFFFFFF6 (= -10 signed) via NEG after ADDI */
    w16(0x0800, 0x0680); w16(0x0802, 0x0000); w16(0x0804, 0x000A);
    w16(0x0806, 0x4480); /* NEG.L D0 → D0 = -10 */
    /* DIVS.W #-2,D0: 1000 000 1 11 111 100 = 0x81FC */
    w16(0x0808, 0x81FC); w16(0x080A, 0xFFFE);
    step1(); step1(); step1();
    TEST_ASSERT("[N] DIVS.W quotient",  (g_tCpu.auDn[0] & 0xFFFFu) == 5u);
}

/* ------------------------------------------------------------------ */
/* Robustness                                                           */
/* ------------------------------------------------------------------ */

static void test_robustness(void)
{
    cpu_step_result_t tRes;

    /* [R] cpu_step NULL ptCpu → ST_ERROR */
    TEST_ASSERT("[R] cpu_step NULL cpu",
                cpu_step(NULL, &g_tMachine, &tRes) == ST_ERROR);

    /* [R] cpu_step NULL ptMachine → ST_ERROR */
    setup();
    TEST_ASSERT("[R] cpu_step NULL machine",
                cpu_step(&g_tCpu, NULL, &tRes) == ST_ERROR);

    /* [R] Unimplemented opcode 0x6000 (BRA UC23) → ST_NO_ERROR, PC+2 */
    setup();
    w16(0x0800, 0x6000); w16(0x0802, 0x0000);
    {
        st_u32_t uiPCBefore = g_tCpu.uiPC;
        st_error_t eR = cpu_step(&g_tCpu, &g_tMachine, &tRes);
        TEST_ASSERT("[R] unimplemented opcode ST_NO_ERROR", eR == ST_NO_ERROR);
        TEST_ASSERT("[R] unimplemented opcode PC advanced",
                    g_tCpu.uiPC == uiPCBefore + 2u);
    }

    /* [R] ADDQ sz=3 (Scc) → ST_NO_ERROR (LOG_TODO) */
    setup();
    /* ADDQ sz=3: 0101 xxx 0 11 yyy zzz */
    w16(0x0800, 0x50C0); /* Scc stub */
    {
        st_error_t eR = cpu_step(&g_tCpu, &g_tMachine, &tRes);
        TEST_ASSERT("[R] Scc/ADDQ-sz3 ST_NO_ERROR", eR == ST_NO_ERROR);
    }

    /* [R] DIV by zero → ST_NO_ERROR (LOG_TODO exception deferred UC23) */
    setup();
    w16(0x0800, 0x7005); /* MOVEQ #5,D0 */
    /* DIVU.W #0,D0 = 0x80FC + 0x0000 */
    w16(0x0802, 0x80FC); w16(0x0804, 0x0000);
    step1();
    {
        st_error_t eR = cpu_step(&g_tCpu, &g_tMachine, &tRes);
        TEST_ASSERT("[R] DIV by zero no crash", eR == ST_NO_ERROR);
    }

    /* [R] NEG.L size=3 (sz field = 11) → ST_NO_ERROR (LOG_TODO) */
    setup();
    /* 0100 0000 11 000 000 = 0x40C0 — sz=3 → unary rejects */
    w16(0x0800, 0x40C0);
    {
        st_error_t eR = cpu_step(&g_tCpu, &g_tMachine, &tRes);
        TEST_ASSERT("[R] unary sz=3 ST_NO_ERROR", eR == ST_NO_ERROR);
    }

    /* [R] ADDI sz=3 → ST_NO_ERROR (LOG_TODO) */
    setup();
    /* ADDI sz=3: 0000 0110 11 000 000 = 0x06C0 */
    w16(0x0800, 0x06C0);
    {
        st_error_t eR = cpu_step(&g_tCpu, &g_tMachine, &tRes);
        TEST_ASSERT("[R] ADDI sz=3 ST_NO_ERROR", eR == ST_NO_ERROR);
    }

    /* [R] Group0 unknown op nibble → ST_NO_ERROR (LOG_TODO) */
    setup();
    /* 0000 1110 01 000 000 = 0x0E40 — not a valid immediate op */
    w16(0x0800, 0x0E40);
    {
        st_error_t eR = cpu_step(&g_tCpu, &g_tMachine, &tRes);
        TEST_ASSERT("[R] group0 unknown op ST_NO_ERROR", eR == ST_NO_ERROR);
    }
}

/* ------------------------------------------------------------------ */
/* Arithmetic program sequence test                                     */
/* ------------------------------------------------------------------ */

static void test_arith_program(void)
{
    /*
     * Program: compute (5 + 3) - 2 in D0, then AND with 0x0F
     * MOVEQ #5,D0      ; D0=5
     * ADDQ.L #3,D0     ; D0=8
     * SUBI.L #2,D0     ; D0=6
     * ANDI.B #$0F,D0   ; D0=6 (0x06 & 0x0F = 0x06)
     * TST.B D0         ; flags: Z=0
     */
    setup();
    w16(0x0800, 0x7005);              /* MOVEQ #5,D0 */
    w16(0x0802, 0x5680);              /* ADDQ.L #3,D0: 0101 011 0 10 000 000 */
    w16(0x0804, 0x0480);              /* SUBI.L #2,D0 */
    w16(0x0806, 0x0000); w16(0x0808, 0x0002);
    w16(0x080A, 0x0200); w16(0x080C, 0x000F); /* ANDI.B #$0F,D0 */
    w16(0x080E, 0x4A00);              /* TST.B D0 */
    step1(); step1(); step1(); step1(); step1();
    TEST_ASSERT("[N] arith prog D0=6", (g_tCpu.auDn[0] & 0xFFu) == 6u);
    TEST_ASSERT("[N] arith prog Z=0",  flag_z() == 0);
    TEST_ASSERT("[N] arith prog N=0",  flag_n() == 0);
    TEST_ASSERT("[N] arith prog C=0",  flag_c() == 0);

    /*
     * Shift sequence: D0=0x01, ASL.L #8,D0 → 0x100
     * Then LSR.L #4,D0 → 0x10
     */
    setup();
    w16(0x0800, 0x7001);              /* MOVEQ #1,D0 */
    /* ASL.L #8,D0: 1110 000 1 10 0 00 000 = 0xE180 (count=0→8) */
    w16(0x0802, 0xE180);
    step1(); step1();
    TEST_ASSERT("[N] ASL #8 result", g_tCpu.auDn[0] == 0x100u);
    /* After LSR.L #4: 0x100 >> 4 = 0x10 */
    setup();
    w16(0x0800, 0x7001);
    w16(0x0802, 0xE180); /* ASL.L #8,D0 */
    w16(0x0804, 0xE880); /* LSR.L #4,D0 */
    step1(); step1(); step1();
    TEST_ASSERT("[N] LSR #4 result", g_tCpu.auDn[0] == 0x10u);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC22 : CPU 68000 arithmetic / logic / shifts ===\n");

    test_add_sub_cmp();
    test_logic();
    test_unary();
    test_addq_subq();
    test_immediate();
    test_shifts();
    test_mul_div();
    test_robustness();
    test_arith_program();

    if (g_uc_fails == 0)
        printf("\nAll UC22 tests PASSED.\n");
    else
        printf("\n%d UC22 test(s) FAILED.\n", g_uc_fails);

    return g_uc_fails ? 1 : 0;
}
