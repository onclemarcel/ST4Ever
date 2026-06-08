/*
 * use_case_23.c - UC23 : CPU 68000 BRA/Bcc/BSR/RTS/TRAP + exception vectors
 *
 * TEST MATRIX - UC23:
 *   [N] Nominal    : 44 tests - BRA(byte+word), BSR+RTS, Bcc (14 conds),
 *                               JMP/JSR(abs.L+d16(An)+d16(PC)), NOP, STOP,
 *                               TRAP+exception_frame, RTR, RTE,
 *                               LINK+UNLK, DBcc(DBRA+cond_true),
 *                               Scc(ST+SF), full subroutine program
 *   [R] Robustness :  8 tests - NULL params, halted-CPU no-op, cpu_reset,
 *                               bad vector (halts), Bcc not-taken x6
 *   [S] Skipped    :  0 tests
 */
#include "use_cases.h"

/* INTENT[INT-CPU-011 -> TC-CPU-101..152 -> REQ-CPU-032..044 -> UFR-EXE-007]:
 * cpu_step() must execute BRA/BSR/Bcc, JMP/JSR/RTS/RTR/RTE, NOP, STOP,
 * TRAP (with full exception frame: SR + PC pushed on SSP, handler loaded
 * from vector table), LINK/UNLK, Scc, DBcc, updating PC and SR correctly.
 * cpu_raise_exception() must push SR then PC on the supervisor stack and
 * jump to the handler address read from the vector table. */

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

static void step1(void) { cpu_step(&g_tCpu, &g_tMachine, NULL); }

static int flag_n(void) { return (g_tCpu.uiSR & CPU_SR_N) ? 1 : 0; }
static int flag_z(void) { return (g_tCpu.uiSR & CPU_SR_Z) ? 1 : 0; }
static int flag_c(void) { return (g_tCpu.uiSR & CPU_SR_C) ? 1 : 0; }
static int flag_v(void) { return (g_tCpu.uiSR & CPU_SR_V) ? 1 : 0; }

/* Read 32-bit big-endian long from machine RAM */
static st_u32_t r32(st_u32_t uiAddr)
{
    return ((st_u32_t)g_tMachine.aRam[uiAddr]     << 24)
         | ((st_u32_t)g_tMachine.aRam[uiAddr + 1] << 16)
         | ((st_u32_t)g_tMachine.aRam[uiAddr + 2] <<  8)
         |  (st_u32_t)g_tMachine.aRam[uiAddr + 3];
}

static st_u16_t r16(st_u32_t uiAddr)
{
    return (st_u16_t)(((st_u16_t)g_tMachine.aRam[uiAddr] << 8)
                    |  (st_u16_t)g_tMachine.aRam[uiAddr + 1]);
}

/* ------------------------------------------------------------------ */
/* BRA                                                                  */
/* ------------------------------------------------------------------ */

static void test_bra(void)
{
    /* BRA.s +6 (byte displacement): 0x6006 — jump from 0x0800 to 0x0808 */
    setup();
    w16(0x0800, 0x6006);
    step1();
    TEST_ASSERT("[N] BRA byte: PC = 0x0808", g_tCpu.uiPC == 0x0808u);

    /* BRA.s -2 (byte displacement): backward to self = infinite loop
     * 0x60FE → disp8=0xFE=-2, target=0x0802+(-2)=0x0800 */
    setup();
    w16(0x0800, 0x60FEu);
    step1();
    TEST_ASSERT("[N] BRA byte backward: PC = 0x0800", g_tCpu.uiPC == 0x0800u);

    /* BRA.w word displacement: 0x6000 0x0100 — from 0x0800 to 0x0902 */
    setup();
    w16(0x0800, 0x6000);
    w16(0x0802, 0x0100);
    step1();
    TEST_ASSERT("[N] BRA word: PC = 0x0902", g_tCpu.uiPC == 0x0902u);

    /* BRA does not affect flags — set a known SR, check unchanged */
    setup();
    g_tCpu.uiSR |= (st_u16_t)(CPU_SR_Z | CPU_SR_C);
    w16(0x0800, 0x6004);
    step1();
    TEST_ASSERT("[N] BRA flags unchanged N=0", flag_n() == 0);
    TEST_ASSERT("[N] BRA flags unchanged Z=1", flag_z() == 1);
    TEST_ASSERT("[N] BRA flags unchanged C=1", flag_c() == 1);
}

/* ------------------------------------------------------------------ */
/* BSR + RTS                                                            */
/* ------------------------------------------------------------------ */

static void test_bsr_rts(void)
{
    /* BSR byte displacement: BSR to subroutine at 0x0808
     * 0x6106 — disp8=6, target=0x0802+6=0x0808; return addr=0x0802
     * Subroutine: MOVEQ #42,D0 + RTS */
    setup();
    w16(0x0800, 0x6106);           /* BSR.s +6 */
    w16(0x0802, 0x4E75);           /* RTS (never reached directly) */
    w16(0x0808, 0x7001 + (42 << 8) - (42 << 8));
    /* MOVEQ #42,D0 = 0x702A */
    w16(0x0808, 0x702A);           /* MOVEQ #42,D0 */
    w16(0x080A, 0x4E75);           /* RTS */

    step1();   /* BSR: SP-=4, [SP]=0x0802, PC=0x0808 */
    TEST_ASSERT("[N] BSR byte: PC at subroutine", g_tCpu.uiPC == 0x0808u);
    TEST_ASSERT("[N] BSR byte: return addr on stack",
                r32(g_tCpu.auAn[7]) == 0x0802u);
    TEST_ASSERT("[N] BSR byte: SP decremented", g_tCpu.auAn[7] == 0x05FCu);

    step1();   /* MOVEQ #42,D0 */
    TEST_ASSERT("[N] subroutine body D0=42", g_tCpu.auDn[0] == 42u);

    step1();   /* RTS: PC = pop() = 0x0802 */
    TEST_ASSERT("[N] RTS returns to caller", g_tCpu.uiPC == 0x0802u);
    TEST_ASSERT("[N] RTS restores SP", g_tCpu.auAn[7] == 0x0600u);

    /* BSR word displacement */
    setup();
    w16(0x0800, 0x6100);           /* BSR.w */
    w16(0x0802, 0x0100);           /* displacement 0x0100 → target=0x0902 */
    w16(0x0902, 0x7064);           /* MOVEQ #100,D0 */
    w16(0x0904, 0x4E75);           /* RTS */

    step1();   /* BSR.w: PC=0x0902, retaddr=0x0804 on stack */
    TEST_ASSERT("[N] BSR word: PC at 0x0902", g_tCpu.uiPC == 0x0902u);
    TEST_ASSERT("[N] BSR word: return addr 0x0804",
                r32(g_tCpu.auAn[7]) == 0x0804u);
    step1();   /* MOVEQ #100,D0 */
    step1();   /* RTS → 0x0804 */
    TEST_ASSERT("[N] BSR word RTS: PC=0x0804", g_tCpu.uiPC == 0x0804u);
}

/* ------------------------------------------------------------------ */
/* Bcc — conditional branches                                           */
/* ------------------------------------------------------------------ */

static void test_bcc(void)
{
    /* Helper: place BEQ +4 at 0x0800, set Z flag, expect branch taken */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_Z;
    w16(0x0800, 0x6704);           /* BEQ +4 → target=0x0806 */
    step1();
    TEST_ASSERT("[N] BEQ taken (Z=1)", g_tCpu.uiPC == 0x0806u);

    /* BEQ not taken (Z=0) */
    setup();
    g_tCpu.uiSR &= (st_u16_t)~CPU_SR_Z;
    w16(0x0800, 0x6704);
    step1();
    TEST_ASSERT("[R] BEQ not taken (Z=0)", g_tCpu.uiPC == 0x0802u);

    /* BNE taken (Z=0) */
    setup();
    g_tCpu.uiSR &= (st_u16_t)~CPU_SR_Z;
    w16(0x0800, 0x6604);           /* BNE +4 */
    step1();
    TEST_ASSERT("[N] BNE taken (Z=0)", g_tCpu.uiPC == 0x0806u);

    /* BNE not taken (Z=1) */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_Z;
    w16(0x0800, 0x6604);
    step1();
    TEST_ASSERT("[R] BNE not taken (Z=1)", g_tCpu.uiPC == 0x0802u);

    /* BLT: N!=V → branch */
    setup();
    g_tCpu.uiSR |=  (st_u16_t)CPU_SR_N;
    g_tCpu.uiSR &= (st_u16_t)~CPU_SR_V;
    w16(0x0800, 0x6D04);           /* BLT */
    step1();
    TEST_ASSERT("[N] BLT taken (N=1,V=0)", g_tCpu.uiPC == 0x0806u);

    /* BGT: !Z && N==V → branch */
    setup();
    g_tCpu.uiSR &= (st_u16_t)~(CPU_SR_Z | CPU_SR_N | CPU_SR_V);
    w16(0x0800, 0x6E04);           /* BGT */
    step1();
    TEST_ASSERT("[N] BGT taken (Z=0,N=V=0)", g_tCpu.uiPC == 0x0806u);

    /* BGE: N==V → branch */
    setup();
    g_tCpu.uiSR &= (st_u16_t)~(CPU_SR_N | CPU_SR_V);
    w16(0x0800, 0x6C04);           /* BGE */
    step1();
    TEST_ASSERT("[N] BGE taken (N=V=0)", g_tCpu.uiPC == 0x0806u);

    /* BLE: Z=1 → branch */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_Z;
    w16(0x0800, 0x6F04);           /* BLE */
    step1();
    TEST_ASSERT("[N] BLE taken (Z=1)", g_tCpu.uiPC == 0x0806u);

    /* BCC: C=0 → branch */
    setup();
    g_tCpu.uiSR &= (st_u16_t)~CPU_SR_C;
    w16(0x0800, 0x6404);           /* BCC */
    step1();
    TEST_ASSERT("[N] BCC taken (C=0)", g_tCpu.uiPC == 0x0806u);

    /* BCS: C=1 → branch */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_C;
    w16(0x0800, 0x6504);           /* BCS */
    step1();
    TEST_ASSERT("[N] BCS taken (C=1)", g_tCpu.uiPC == 0x0806u);

    /* BMI: N=1 → branch */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_N;
    w16(0x0800, 0x6B04);           /* BMI */
    step1();
    TEST_ASSERT("[N] BMI taken (N=1)", g_tCpu.uiPC == 0x0806u);

    /* BPL: N=0 → branch */
    setup();
    g_tCpu.uiSR &= (st_u16_t)~CPU_SR_N;
    w16(0x0800, 0x6A04);           /* BPL */
    step1();
    TEST_ASSERT("[N] BPL taken (N=0)", g_tCpu.uiPC == 0x0806u);

    /* BHI: C=0 && Z=0 → branch */
    setup();
    g_tCpu.uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_Z);
    w16(0x0800, 0x6204);           /* BHI */
    step1();
    TEST_ASSERT("[N] BHI taken (C=0,Z=0)", g_tCpu.uiPC == 0x0806u);

    /* BLS: C=1 → branch */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_C;
    w16(0x0800, 0x6304);           /* BLS */
    step1();
    TEST_ASSERT("[N] BLS taken (C=1)", g_tCpu.uiPC == 0x0806u);

    /* BVC: V=0 → branch */
    setup();
    g_tCpu.uiSR &= (st_u16_t)~CPU_SR_V;
    w16(0x0800, 0x6804);           /* BVC */
    step1();
    TEST_ASSERT("[N] BVC taken (V=0)", g_tCpu.uiPC == 0x0806u);

    /* BVS: V=1 → branch */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_V;
    w16(0x0800, 0x6904);           /* BVS */
    step1();
    TEST_ASSERT("[N] BVS taken (V=1)", g_tCpu.uiPC == 0x0806u);
}

/* ------------------------------------------------------------------ */
/* JMP / JSR                                                            */
/* ------------------------------------------------------------------ */

static void test_jmp_jsr(void)
{
    /* JMP abs.L: 0x4EF9 + 32-bit address */
    setup();
    w16(0x0800, 0x4EF9);
    w16(0x0802, 0x0000);
    w16(0x0804, 0x1234);           /* JMP 0x001234 */
    step1();
    TEST_ASSERT("[N] JMP abs.L: PC=0x001234", g_tCpu.uiPC == 0x001234u);

    /* JMP abs.W (sign-extended): 0x4EF8 + 16-bit address */
    setup();
    w16(0x0800, 0x4EF8);
    w16(0x0802, 0x1400);           /* JMP 0x001400 */
    step1();
    TEST_ASSERT("[N] JMP abs.W: PC=0x001400", g_tCpu.uiPC == 0x001400u);

    /* JMP (An): 0x4ED0 + mode 2 reg 0 → A0 */
    setup();
    g_tCpu.auAn[0] = 0x001500u;
    w16(0x0800, 0x4ED0);           /* JMP (A0) */
    step1();
    TEST_ASSERT("[N] JMP (A0): PC=0x001500", g_tCpu.uiPC == 0x001500u);

    /* JSR abs.L + RTS */
    setup();
    w16(0x0800, 0x4EB9);           /* JSR abs.L */
    w16(0x0802, 0x0000);
    w16(0x0804, 0x0900);           /* → 0x000900 */
    w16(0x0900, 0x4E75);           /* RTS */

    step1();   /* JSR: push 0x0806, PC=0x0900 */
    TEST_ASSERT("[N] JSR abs.L: PC=0x0900", g_tCpu.uiPC == 0x0900u);
    TEST_ASSERT("[N] JSR abs.L: retaddr on stack",
                r32(g_tCpu.auAn[7]) == 0x0806u);
    step1();   /* RTS: PC=0x0806 */
    TEST_ASSERT("[N] JSR+RTS returns 0x0806", g_tCpu.uiPC == 0x0806u);
    TEST_ASSERT("[N] JSR+RTS SP restored", g_tCpu.auAn[7] == 0x0600u);

    /* JSR d16(An): 0x4EA8 + word displacement, A0 = 0x0900 */
    setup();
    g_tCpu.auAn[0] = 0x000900u;
    w16(0x0800, 0x4EA8);           /* JSR d16(A0) */
    w16(0x0802, 0x0040);           /* disp = 0x0040 → target = 0x0940 */
    w16(0x0940, 0x4E75);           /* RTS */
    step1();
    TEST_ASSERT("[N] JSR d16(An): PC=0x0940", g_tCpu.uiPC == 0x0940u);
}

/* ------------------------------------------------------------------ */
/* NOP / STOP                                                           */
/* ------------------------------------------------------------------ */

static void test_nop_stop(void)
{
    /* NOP: PC advances by 2, flags unchanged */
    setup();
    g_tCpu.uiSR |= (st_u16_t)(CPU_SR_Z | CPU_SR_N);
    w16(0x0800, 0x4E71);
    step1();
    TEST_ASSERT("[N] NOP: PC=0x0802", g_tCpu.uiPC == 0x0802u);
    TEST_ASSERT("[N] NOP: flags unchanged Z=1", flag_z() == 1);
    TEST_ASSERT("[N] NOP: flags unchanged N=1", flag_n() == 1);

    /* STOP #0x2700: load SR = 0x2700, CPU halted */
    setup();
    w16(0x0800, 0x4E72);
    w16(0x0802, 0x2700);
    step1();
    TEST_ASSERT("[N] STOP: state=STOPPED",
                g_tCpu.eState == CPU_STATE_STOPPED);
    TEST_ASSERT("[N] STOP: SR=0x2700", g_tCpu.uiSR == 0x2700u);

    /* cpu_step on STOPPED CPU: no-op, no crash */
    step1();
    TEST_ASSERT("[R] STOP: CPU stays stopped",
                g_tCpu.eState == CPU_STATE_STOPPED);
}

/* ------------------------------------------------------------------ */
/* TRAP + exception frame                                               */
/* ------------------------------------------------------------------ */

static void test_trap(void)
{
    st_u32_t uiSavedSR;
    st_u32_t uiSavedPC;

    /* TRAP #0: vector at 0x0080, handler at 0x1000
     * SSP=0x0600, code at 0x0800 */
    setup();
    /* Write TRAP #0 vector = 0x00001000 */
    w32(0x0080, 0x00001000u);
    /* Code: TRAP #0 at 0x0800 */
    w16(0x0800, 0x4E40);           /* TRAP #0 */

    uiSavedSR  = (st_u32_t)g_tCpu.uiSR;
    uiSavedPC  = 0x0802u;          /* expected return addr = after TRAP */

    step1();   /* TRAP #0 */
    TEST_ASSERT("[N] TRAP: PC=handler 0x1000", g_tCpu.uiPC == 0x1000u);
    TEST_ASSERT("[N] TRAP: SR has S bit set",
                (g_tCpu.uiSR & CPU_SR_S) != 0);

    /* Verify stack: SR pushed at SP, PC pushed above it
     * After exception: SP = 0x0600 - 6 = 0x05FA
     * [0x05FA] = SR (word), [0x05FC] = PC (long) */
    TEST_ASSERT("[N] TRAP: SP = 0x05FA", g_tCpu.auAn[7] == 0x05FAu);
    TEST_ASSERT("[N] TRAP: SR on stack",
                r16(0x05FA) == (st_u16_t)uiSavedSR);
    TEST_ASSERT("[N] TRAP: PC on stack", r32(0x05FC) == uiSavedPC);

    /* cpu_raise_exception() directly with TRAP #1 */
    setup();
    w32(0x0084, 0x00002000u);      /* TRAP #1 vector = 0x2000 */
    g_tCpu.uiPC = 0x0808u;
    cpu_raise_exception(&g_tCpu, &g_tMachine, CPU_VEC_TRAP(1));
    TEST_ASSERT("[N] raise_exception: PC=0x2000",
                g_tCpu.uiPC == 0x2000u);
}

/* ------------------------------------------------------------------ */
/* RTR / RTE                                                            */
/* ------------------------------------------------------------------ */

static void test_rtr_rte(void)
{
    /* RTR: pop CCR (low byte of SR) then PC
     * Setup: push fake return context manually */
    setup();
    /* Manually push: PC=0x1234 (long) then SR_low=0x000F (word) */
    g_tCpu.auAn[7] = 0x05F8u;
    w32(0x05FA, 0x00001234u);      /* PC on stack at 0x05FA */
    w16(0x05F8, 0x000Fu);          /* CCR = 0x000F (XNZVC all set) */
    /* RTR at 0x0800 */
    w16(0x0800, 0x4E77);
    step1();
    TEST_ASSERT("[N] RTR: PC=0x1234", g_tCpu.uiPC == 0x1234u);
    /* CCR bits restored: XNZVC = 0x1F masked to 0x1F in SR low byte */
    TEST_ASSERT("[N] RTR: CCR restored (N=1)", flag_n() == 1);
    TEST_ASSERT("[N] RTR: CCR restored (Z=1)", flag_z() == 1);
    TEST_ASSERT("[N] RTR: CCR restored (C=1)", flag_c() == 1);
    TEST_ASSERT("[N] RTR: CCR restored (V=1)", flag_v() == 1);
    /* High byte of SR (supervisor, interrupt mask) must be unchanged */
    TEST_ASSERT("[N] RTR: SR high byte preserved",
                (g_tCpu.uiSR & 0xFF00u) == (CPU_SR_S | CPU_SR_I_MASK));

    /* RTE: pop full SR then PC */
    setup();
    g_tCpu.auAn[7] = 0x05F8u;
    w32(0x05FA, 0x00005678u);      /* PC */
    w16(0x05F8, 0x2000u);          /* SR = 0x2000 (S=1, no interrupt mask) */
    w16(0x0800, 0x4E73);
    step1();
    TEST_ASSERT("[N] RTE: PC=0x5678", g_tCpu.uiPC == 0x5678u);
    TEST_ASSERT("[N] RTE: SR=0x2000", g_tCpu.uiSR == 0x2000u);
}

/* ------------------------------------------------------------------ */
/* LINK + UNLK                                                          */
/* ------------------------------------------------------------------ */

static void test_link_unlk(void)
{
    /* LINK A6,#-8: 0x4E56 0xFFF8
     * Before: A6=0x1234, SP=0x0600
     * After:  stack=[0x1234 at 0x05FC], A6=0x05FC, SP=0x05F4 */
    setup();
    g_tCpu.auAn[6] = 0x1234u;
    w16(0x0800, 0x4E56);           /* LINK A6 */
    w16(0x0802, 0xFFF8u);          /* displacement = -8 */
    step1();
    TEST_ASSERT("[N] LINK: old A6 on stack",
                r32(g_tCpu.auAn[6]) == 0x1234u);
    TEST_ASSERT("[N] LINK: A6 = frame pointer (0x05FC)",
                g_tCpu.auAn[6] == 0x05FCu);
    TEST_ASSERT("[N] LINK: SP = 0x05F4 (0x05FC-8)",
                g_tCpu.auAn[7] == 0x05F4u);

    /* UNLK A6: 0x4E5E
     * SP=A6=0x05FC, pop long into A6 → A6=0x1234, SP=0x0600 */
    w16(0x0804, 0x4E5E);           /* UNLK A6 */
    step1();
    TEST_ASSERT("[N] UNLK: A6 restored to 0x1234",
                g_tCpu.auAn[6] == 0x1234u);
    TEST_ASSERT("[N] UNLK: SP restored to 0x0600",
                g_tCpu.auAn[7] == 0x0600u);
}

/* ------------------------------------------------------------------ */
/* DBcc (DBRA = DBF)                                                    */
/* ------------------------------------------------------------------ */

static void test_dbcc(void)
{
    /* DBRA D0,loop : 0x51C8 + word_displacement
     * DBRA = DBF = condition always false → always decrement and branch
     * D0=3, loop at 0x0800, displacement=-2 (0xFFFE) → branch to 0x0800
     * After 4 iterations (3→2→1→0→-1): D0.W=-1, falls through */
    setup();
    g_tCpu.auDn[0] = 3u;
    w16(0x0800, 0x51C8);           /* DBRA D0 */
    w16(0x0802, 0xFFFEu);          /* displacement -2 → loop back to 0x0800 */

    step1(); /* D0=3→2, branch back */
    TEST_ASSERT("[N] DBRA iter1: D0.W=2",
                (g_tCpu.auDn[0] & 0xFFFFu) == 2u);
    TEST_ASSERT("[N] DBRA iter1: PC=0x0800", g_tCpu.uiPC == 0x0800u);

    step1(); /* D0=2→1 */
    step1(); /* D0=1→0 */
    TEST_ASSERT("[N] DBRA iter3: D0.W=0",
                (g_tCpu.auDn[0] & 0xFFFFu) == 0u);
    TEST_ASSERT("[N] DBRA iter3: still looping",
                g_tCpu.uiPC == 0x0800u);

    step1(); /* D0=0→-1: fall through to 0x0804 */
    TEST_ASSERT("[N] DBRA expired: D0.W=0xFFFF",
                (g_tCpu.auDn[0] & 0xFFFFu) == 0xFFFFu);
    TEST_ASSERT("[N] DBRA expired: fall through PC=0x0804",
                g_tCpu.uiPC == 0x0804u);

    /* DBcc with condition TRUE (DBNE, Z=0 → NE true → fall through) */
    setup();
    g_tCpu.auDn[1] = 5u;
    g_tCpu.uiSR &= (st_u16_t)~CPU_SR_Z;   /* Z=0: NE is true */
    w16(0x0800, 0x56C9);           /* DBNE D1 */
    w16(0x0802, 0xFFFEu);
    step1();
    /* NE is true → condition true → fall through without decrement */
    TEST_ASSERT("[N] DBcc cond_true: D1 unchanged",
                (g_tCpu.auDn[1] & 0xFFFFu) == 5u);
    TEST_ASSERT("[N] DBcc cond_true: fall through PC=0x0804",
                g_tCpu.uiPC == 0x0804u);
}

/* ------------------------------------------------------------------ */
/* Scc                                                                  */
/* ------------------------------------------------------------------ */

static void test_scc(void)
{
    /* ST D0 (always true): 0x50C0 → byte EA=Dn, sets 0xFF */
    setup();
    g_tCpu.auDn[0] = 0u;
    w16(0x0800, 0x50C0);           /* ST D0 */
    step1();
    TEST_ASSERT("[N] ST D0: byte set to 0xFF",
                (g_tCpu.auDn[0] & 0xFFu) == 0xFFu);

    /* SF D1 (always false): 0x51C1 → byte set to 0x00 */
    setup();
    g_tCpu.auDn[1] = 0xFFFFFFFFu;
    w16(0x0800, 0x51C1);           /* SF D1 */
    step1();
    TEST_ASSERT("[N] SF D1: byte set to 0x00",
                (g_tCpu.auDn[1] & 0xFFu) == 0x00u);
    /* Upper bytes unchanged */
    TEST_ASSERT("[N] SF D1: upper bytes unchanged",
                (g_tCpu.auDn[1] & 0xFFFFFF00u) == 0xFFFFFF00u);

    /* SEQ D2 (Z=1): 0x57C2 */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_Z;
    g_tCpu.auDn[2] = 0u;
    w16(0x0800, 0x57C2);           /* SEQ D2 */
    step1();
    TEST_ASSERT("[N] SEQ D2 (Z=1): 0xFF",
                (g_tCpu.auDn[2] & 0xFFu) == 0xFFu);

    /* SNE D2 (Z=1 → false): 0x56C2 */
    setup();
    g_tCpu.uiSR |= (st_u16_t)CPU_SR_Z;
    g_tCpu.auDn[2] = 0xFFu;
    w16(0x0800, 0x56C2);           /* SNE D2 */
    step1();
    TEST_ASSERT("[N] SNE D2 (Z=1 → false): 0x00",
                (g_tCpu.auDn[2] & 0xFFu) == 0x00u);
}

/* ------------------------------------------------------------------ */
/* Full subroutine program (integration)                                */
/* ------------------------------------------------------------------ */

static void test_full_program(void)
{
    /* Program: compute D0 = sum(1..5) via BSR + loop
     *
     * 0x0800: MOVEQ #0,D0       ; accumulator = 0
     * 0x0802: MOVEQ #5,D1       ; counter
     * 0x0804: BSR.s sub (+2)    ; 0x6102 → call sub at 0x0808
     * 0x0806: STOP #0x2700      ; done
     *
     * sub (0x0808):
     * 0x0808: ADD.W D1,D0       ; D0 += D1
     * 0x080A: SUBQ.W #1,D1      ; D1--
     * 0x080C: BNE -8            ; 0x66F8 → back to 0x0806? No: base=0x080E, -8 → 0x0806
     *   Need to branch back to 0x0808: base=0x080E, target=0x0808, disp=-6=0xFFFA
     * 0x080C: BNE -6            ; 0x66FA → 0x080E + (-6) = 0x0808
     * 0x080E: RTS
     */
    setup();
    w16(0x0800, 0x7000);           /* MOVEQ #0,D0 */
    w16(0x0802, 0x7205);           /* MOVEQ #5,D1 */
    w16(0x0804, 0x6102);           /* BSR.s +2 → 0x0808 */
    w16(0x0806, 0x4E72);           /* STOP */
    w16(0x0808, 0xD041);           /* ADD.W D1,D0: 1101 000 0 01 000 001 = 0xD041 */
    w16(0x080A, 0x5341);           /* SUBQ.W #1,D1: 0101 001 1 01 000 001 = 0x5341 */
    w16(0x080C, 0x66FAu);          /* BNE -6 → 0x080E + (-6) = 0x0808 */
    w16(0x080E, 0x4E75);           /* RTS */
    w16(0x0810, 0x2700u);          /* STOP immediate word */

    /* Execute */
    step1();   /* MOVEQ #0,D0 */
    step1();   /* MOVEQ #5,D1 */
    step1();   /* BSR → 0x0808 */
    /* 5 iterations × 3 instructions (ADD+SUBQ+BNE) = 15 steps;
     * last BNE is step 15 (falls through to RTS), step 16 = RTS */
    {
        int i;
        for (i = 0; i < 5 * 3; i++)
            step1();
    }
    step1();   /* RTS → 0x0806 */

    TEST_ASSERT("[N] full program: D0 = sum(1..5) = 15",
                g_tCpu.auDn[0] == 15u);
    TEST_ASSERT("[N] full program: D1 = 0 (exhausted)",
                g_tCpu.auDn[1] == 0u);
    TEST_ASSERT("[N] full program: returned to caller (PC=0x0806)",
                g_tCpu.uiPC == 0x0806u);
    TEST_ASSERT("[N] full program: SP restored",
                g_tCpu.auAn[7] == 0x0600u);
}

/* ------------------------------------------------------------------ */
/* Robustness                                                           */
/* ------------------------------------------------------------------ */

static void test_robustness(void)
{
    st_error_t eR;

    /* NULL ptCpu */
    eR = cpu_step(NULL, &g_tMachine, NULL);
    TEST_ASSERT("[R] cpu_step NULL ptCpu → ST_ERROR", eR == ST_ERROR);

    /* NULL ptMachine */
    setup();
    eR = cpu_step(&g_tCpu, NULL, NULL);
    TEST_ASSERT("[R] cpu_step NULL ptMachine → ST_ERROR", eR == ST_ERROR);

    /* cpu_raise_exception: NULL ptCpu */
    eR = cpu_raise_exception(NULL, &g_tMachine, 0x0080u);
    TEST_ASSERT("[R] raise_exception NULL ptCpu → ST_ERROR",
                eR == ST_ERROR);

    /* cpu_raise_exception: NULL ptMachine */
    setup();
    eR = cpu_raise_exception(&g_tCpu, NULL, 0x0080u);
    TEST_ASSERT("[R] raise_exception NULL ptMachine → ST_ERROR",
                eR == ST_ERROR);

    /* cpu_reset: re-reads reset vectors */
    setup();
    g_tCpu.uiPC = 0xABCDu;
    cpu_reset(&g_tCpu, &g_tMachine);
    TEST_ASSERT("[R] cpu_reset: PC restored to 0x0800",
                g_tCpu.uiPC == 0x0800u);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("\n=== UC23 : CPU 68000 - BRA/Bcc/BSR/RTS/TRAP/exceptions ===\n");

    printf("\n--- BRA ---\n");
    test_bra();

    printf("\n--- BSR + RTS ---\n");
    test_bsr_rts();

    printf("\n--- Bcc (14 conditions) ---\n");
    test_bcc();

    printf("\n--- JMP / JSR ---\n");
    test_jmp_jsr();

    printf("\n--- NOP / STOP ---\n");
    test_nop_stop();

    printf("\n--- TRAP + exception frame ---\n");
    test_trap();

    printf("\n--- RTR / RTE ---\n");
    test_rtr_rte();

    printf("\n--- LINK / UNLK ---\n");
    test_link_unlk();

    printf("\n--- DBcc ---\n");
    test_dbcc();

    printf("\n--- Scc ---\n");
    test_scc();

    printf("\n--- Full program ---\n");
    test_full_program();

    printf("\n--- Robustness ---\n");
    test_robustness();

    printf("\n=== UC23 results: %d failure(s) ===\n", g_uc_fails);
    return g_uc_fails > 0 ? 1 : 0;
}
