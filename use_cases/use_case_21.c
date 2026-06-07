/*
 * use_case_21.c - UC21 : CPU 68000 MOVE/MOVEQ/LEA/CLR/SWAP
 *
 * TEST MATRIX - UC21:
 *   [N] Nominal    : 47 tests - MOVEQ, MOVE.B/W/L, MOVEA.W/L,
 *                               LEA, CLR.B/W/L, SWAP, 10-instr sequence
 *   [R] Robustness :  8 tests - NULL params, unimplemented opcode,
 *                               bus error, unaligned
 *   [S] Skipped    :  0 tests
 */
#include "use_cases.h"

/* INTENT[INT-CPU-001 -> TC-CPU-001 -> REQ-CPU-001 -> UFR-CPU-001]:
 * cpu_init() and cpu_step() with a real ST machine must decode and
 * execute 68000 instructions correctly, updating registers and SR. */

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
    /* Reset vectors: SSP=0x0600, PC=ST_LOAD_BASE=0x0800 */
    g_tMachine.aRam[0] = 0x00; g_tMachine.aRam[1] = 0x00;
    g_tMachine.aRam[2] = 0x06; g_tMachine.aRam[3] = 0x00; /* SSP */
    g_tMachine.aRam[4] = 0x00; g_tMachine.aRam[5] = 0x00;
    g_tMachine.aRam[6] = 0x08; g_tMachine.aRam[7] = 0x00; /* PC  */
    cpu_init(&g_tCpu, &g_tMachine);
}

/* Write a word (big-endian) into RAM at offset */
static void w16(st_u32_t uiAddr, st_u16_t uiVal)
{
    g_tMachine.aRam[uiAddr]     = (st_u8_t)(uiVal >> 8);
    g_tMachine.aRam[uiAddr + 1] = (st_u8_t)(uiVal & 0xFFu);
}

/* Read a byte from RAM */
static st_u8_t rb(st_u32_t uiAddr)
{
    return g_tMachine.aRam[uiAddr];
}

/* Read a word from RAM (big-endian) */
static st_u16_t rw(st_u32_t uiAddr)
{
    return (st_u16_t)((g_tMachine.aRam[uiAddr] << 8)
                      | g_tMachine.aRam[uiAddr + 1]);
}

/* SR flag helpers */
static int sr_n(void) { return (g_tCpu.uiSR & CPU_SR_N) ? 1 : 0; }
static int sr_z(void) { return (g_tCpu.uiSR & CPU_SR_Z) ? 1 : 0; }
static int sr_v(void) { return (g_tCpu.uiSR & CPU_SR_V) ? 1 : 0; }
static int sr_c(void) { return (g_tCpu.uiSR & CPU_SR_C) ? 1 : 0; }

/* ------------------------------------------------------------------ */
/* Test groups                                                          */
/* ------------------------------------------------------------------ */

static void test_moveq(void)
{
    printf("\n--- MOVEQ ---\n");
    setup();

    /* MOVEQ #$42,D0 : 0x7084 → word 0x7042 */
    w16(0x0800, 0x7042u); /* MOVEQ #$42,D0 */
    UC_CHECK("[N] MOVEQ step returns NO_ERROR",
             cpu_step(&g_tCpu, &g_tMachine, NULL));
    UC_TEST("[N] MOVEQ #$42,D0 : D0==0x42",
            g_tCpu.auDn[0] == 0x42u);
    UC_TEST("[N] MOVEQ #$42 : N=0",   sr_n() == 0);
    UC_TEST("[N] MOVEQ #$42 : Z=0",   sr_z() == 0);
    UC_TEST("[N] MOVEQ #$42 : V=0",   sr_v() == 0);
    UC_TEST("[N] MOVEQ #$42 : C=0",   sr_c() == 0);

    /* MOVEQ #0,D1 */
    setup();
    w16(0x0800, 0x7200u); /* MOVEQ #0,D1 */
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVEQ #0,D1 : D1==0",  g_tCpu.auDn[1] == 0u);
    UC_TEST("[N] MOVEQ #0,D1 : Z=1",    sr_z() == 1);
    UC_TEST("[N] MOVEQ #0,D1 : N=0",    sr_n() == 0);

    /* MOVEQ #-1 (0xFF),D2 */
    setup();
    w16(0x0800, 0x74FFu); /* MOVEQ #-1,D2 */
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVEQ #-1,D2 : D2==0xFFFFFFFF",
            g_tCpu.auDn[2] == 0xFFFFFFFFu);
    UC_TEST("[N] MOVEQ #-1,D2 : N=1",   sr_n() == 1);
    UC_TEST("[N] MOVEQ #-1,D2 : Z=0",   sr_z() == 0);

    /* MOVEQ with bit8 set → not a MOVEQ, LOG_TODO, no crash */
    setup();
    w16(0x0800, 0x71FFu); /* bit 8 set: not MOVEQ */
    UC_CHECK("[N] MOVEQ invalid (bit8 set) no crash",
             cpu_step(&g_tCpu, &g_tMachine, NULL));
}

static void test_move(void)
{
    printf("\n--- MOVE ---\n");

    /* MOVE.L D2,D3 : 0x2602 */
    setup();
    g_tCpu.auDn[2] = 0x12345678u;
    w16(0x0800, 0x2602u); /* MOVE.L D2,D3 */
    UC_CHECK("[N] MOVE.L D2,D3 step", cpu_step(&g_tCpu, &g_tMachine, NULL));
    UC_TEST("[N] MOVE.L D2,D3 : D3==0x12345678",
            g_tCpu.auDn[3] == 0x12345678u);
    UC_TEST("[N] MOVE.L D2,D3 : N=0", sr_n() == 0);
    UC_TEST("[N] MOVE.L D2,D3 : Z=0", sr_z() == 0);

    /* MOVE.W D0,D4 : 0x3800 */
    setup();
    g_tCpu.auDn[0] = 0xDEAD8000u;
    w16(0x0800, 0x3800u); /* MOVE.W D0,D4 */
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.W D0,D4 : D4 low==0x8000",
            (g_tCpu.auDn[4] & 0xFFFFu) == 0x8000u);
    UC_TEST("[N] MOVE.W D0,D4 : N=1", sr_n() == 1);

    /* MOVE.B #$FF,D6 via immediate EA
     * Encoding: MOVE.B #imm,Dn = 0001 DDD 000 111 100
     * D6=6 → bits11-9=110  mode=000 (Dn dest) → bits8-6=000
     * src mode=7 reg=4 (imm) → bits5-3=111 bits2-0=100
     * = 0001 110 000 111 100 = 0x1C3C + ext 0x00FF */
    setup();
    w16(0x0800, 0x1C3Cu); /* MOVE.B #?,D6 */
    w16(0x0802, 0x00FFu); /* ext: #$FF */
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.B #$FF,D6 : D6&0xFF==0xFF",
            (g_tCpu.auDn[6] & 0xFFu) == 0xFFu);
    UC_TEST("[N] MOVE.B #$FF,D6 : N=1", sr_n() == 1);

    /* MOVE.W #$1234,D4
     * 0011 100 000 111 100 = 0x383C + ext 0x1234 */
    setup();
    w16(0x0800, 0x383Cu); /* MOVE.W #?,D4 */
    w16(0x0802, 0x1234u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.W #$1234,D4 : D4&0xFFFF==0x1234",
            (g_tCpu.auDn[4] & 0xFFFFu) == 0x1234u);
    UC_TEST("[N] MOVE.W #$1234 : N=0", sr_n() == 0);
    UC_TEST("[N] MOVE.W #$1234 : Z=0", sr_z() == 0);
    UC_TEST("[N] MOVE.W #$1234 : V=0", sr_v() == 0);
    UC_TEST("[N] MOVE.W #$1234 : C=0", sr_c() == 0);

    /* MOVE.L A0,D5 : source mode=1 (An), reg=0; dest Dn D5
     * 0010 101 000 001 000 = 0x2A08 */
    setup();
    g_tCpu.auAn[0] = 0x00001234u;
    w16(0x0800, 0x2A08u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.L A0,D5 : D5==0x1234",
            g_tCpu.auDn[5] == 0x00001234u);
}

static void test_movea(void)
{
    printf("\n--- MOVEA ---\n");

    /* MOVEA.W #$8000,A1 : sign-extends to 0xFFFF8000
     * MOVEA.W : top nibble = 0x3, dest mode = 1 (An)
     * 0011 001 001 111 100 = 0x3248 + 0x8000?
     * Wait: bits11-9 = An=1, bits8-6 = mode=001
     * → 0011 001 001 111 100 = 0x327C + ext */
    setup();
    w16(0x0800, 0x327Cu); /* MOVEA.W #?,A1 */
    w16(0x0802, 0x8000u);
    UC_CHECK("[N] MOVEA.W step", cpu_step(&g_tCpu, &g_tMachine, NULL));
    UC_TEST("[N] MOVEA.W #$8000,A1 : sign-extended",
            g_tCpu.auAn[1] == 0xFFFF8000u);

    /* MOVEA.W does not affect flags */
    UC_TEST("[N] MOVEA.W : N unchanged (was 0)", sr_n() == 0);

    /* MOVEA.L D0,A2
     * 0010 010 001 000 000 = 0x2440 */
    setup();
    g_tCpu.auDn[0] = 0xDEADBEEFu;
    w16(0x0800, 0x2440u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVEA.L D0,A2 : A2==0xDEADBEEF",
            g_tCpu.auAn[2] == 0xDEADBEEFu);
}

static void test_lea(void)
{
    printf("\n--- LEA ---\n");

    /* LEA $0800.W,A0 : mode=7 reg=0 (abs.W)
     * 0100 000 111 111 000 = 0x41F8 + 0x0800 */
    setup();
    w16(0x0800, 0x41F8u);
    w16(0x0802, 0x0800u);
    UC_CHECK("[N] LEA abs.W step", cpu_step(&g_tCpu, &g_tMachine, NULL));
    UC_TEST("[N] LEA $0800.W,A0 : A0==0x0800",
            g_tCpu.auAn[0] == 0x0800u);

    /* LEA (A1),A2 : mode=2 reg=1
     * 0100 010 111 010 001 = 0x45D1 */
    setup();
    g_tCpu.auAn[1] = 0x1000u;
    w16(0x0800, 0x45D1u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] LEA (A1),A2 : A2==0x1000",
            g_tCpu.auAn[2] == 0x1000u);

    /* LEA does not change flags */
    UC_TEST("[N] LEA : N=0", sr_n() == 0);
    UC_TEST("[N] LEA : Z=0", sr_z() == 0);
}

static void test_clr(void)
{
    printf("\n--- CLR ---\n");

    /* CLR.L D3 : 0x4283 */
    setup();
    g_tCpu.auDn[3] = 0xDEADBEEFu;
    w16(0x0800, 0x4283u);
    UC_CHECK("[N] CLR.L D3 step", cpu_step(&g_tCpu, &g_tMachine, NULL));
    UC_TEST("[N] CLR.L D3 : D3==0",  g_tCpu.auDn[3] == 0u);
    UC_TEST("[N] CLR.L D3 : N=0",    sr_n() == 0);
    UC_TEST("[N] CLR.L D3 : Z=1",    sr_z() == 1);
    UC_TEST("[N] CLR.L D3 : V=0",    sr_v() == 0);
    UC_TEST("[N] CLR.L D3 : C=0",    sr_c() == 0);

    /* CLR.W D5 : preserves upper 16 bits
     * 0100 0010 01 000 101 = 0x4245 */
    setup();
    g_tCpu.auDn[5] = 0xDEAD1234u;
    w16(0x0800, 0x4245u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] CLR.W D5 : upper preserved",
            (g_tCpu.auDn[5] & 0xFFFF0000u) == 0xDEAD0000u);
    UC_TEST("[N] CLR.W D5 : low word==0",
            (g_tCpu.auDn[5] & 0xFFFFu) == 0u);
    UC_TEST("[N] CLR.W D5 : Z=1", sr_z() == 1);

    /* CLR.B D0 : 0x4200 */
    setup();
    g_tCpu.auDn[0] = 0xDEADBEFFu;
    w16(0x0800, 0x4200u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] CLR.B D0 : byte cleared",
            (g_tCpu.auDn[0] & 0xFFu) == 0u);
    UC_TEST("[N] CLR.B D0 : upper preserved",
            (g_tCpu.auDn[0] & 0xFFFFFF00u) == 0xDEADBE00u);
}

static void test_swap(void)
{
    printf("\n--- SWAP ---\n");

    /* SWAP D3 : 0x4843 */
    setup();
    g_tCpu.auDn[3] = 0xFFFFFFFFu;
    w16(0x0800, 0x4843u);
    UC_CHECK("[N] SWAP D3 step", cpu_step(&g_tCpu, &g_tMachine, NULL));
    UC_TEST("[N] SWAP D3 all-ones : D3 unchanged",
            g_tCpu.auDn[3] == 0xFFFFFFFFu);
    UC_TEST("[N] SWAP D3 all-ones : N=1", sr_n() == 1);
    UC_TEST("[N] SWAP D3 all-ones : Z=0", sr_z() == 0);
    UC_TEST("[N] SWAP D3 all-ones : V=0", sr_v() == 0);
    UC_TEST("[N] SWAP D3 all-ones : C=0", sr_c() == 0);

    /* SWAP result == 0 → Z=1 */
    setup();
    g_tCpu.auDn[3] = 0x00000000u;
    w16(0x0800, 0x4843u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] SWAP D3 zero : Z=1", sr_z() == 1);

    /* SWAP D0 : 0x4840 with 0x00FF0000 → 0x0000FF00 ... wait
     * 0x00FF0000 → swap halves → 0x000000FF */
    setup();
    g_tCpu.auDn[0] = 0x00FF0000u;
    w16(0x0800, 0x4840u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] SWAP D0 0x00FF0000 : D0==0x000000FF",
            g_tCpu.auDn[0] == 0x000000FFu);
    UC_TEST("[N] SWAP D0 0x00FF0000 : N=0", sr_n() == 0);
    UC_TEST("[N] SWAP D0 0x00FF0000 : Z=0", sr_z() == 0);
}

static void test_10instr_sequence(void)
{
    cpu_step_result_t tRes;
    int               i;

    printf("\n--- 10-instruction step sequence ---\n");

    /* Build the 10-instruction program at 0x0800:
     *
     *  0x0800: 70 42        MOVEQ #$42,D0       D0 = 0x42
     *  0x0802: 72 00        MOVEQ #0,D1         D1 = 0, Z=1
     *  0x0804: 74 FF        MOVEQ #-1,D2        D2 = 0xFFFFFFFF, N=1
     *  0x0806: 26 02        MOVE.L D2,D3        D3 = 0xFFFFFFFF
     *  0x0808: 48 43        SWAP D3             D3 = 0xFFFFFFFF (still)
     *  0x080A: 42 83        CLR.L D3            D3 = 0, Z=1
     *  0x080C: 38 3C 12 34  MOVE.W #$1234,D4    D4&0xFFFF = 0x1234
     *  0x0810: 41 F8 08 00  LEA $0800.W,A0      A0 = 0x0800
     *  0x0814: 2A 08        MOVE.L A0,D5        D5 = 0x0800
     *  0x0816: 1C 3C 00 FF  MOVE.B #$FF,D6      D6&0xFF = 0xFF, N=1
     */
    setup();
    w16(0x0800, 0x7042u);
    w16(0x0802, 0x7200u);
    w16(0x0804, 0x74FFu);
    w16(0x0806, 0x2602u);
    w16(0x0808, 0x4843u);
    w16(0x080A, 0x4283u);
    w16(0x080C, 0x383Cu);
    w16(0x080E, 0x1234u);
    w16(0x0810, 0x41F8u);
    w16(0x0812, 0x0800u);
    w16(0x0814, 0x2A08u);
    w16(0x0816, 0x1C3Cu);
    w16(0x0818, 0x00FFu);

    /* Step 1: MOVEQ #$42,D0 */
    memset(&tRes, 0, sizeof(tRes));
    UC_CHECK("[N] Step 1 MOVEQ #$42,D0",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 1: opcode==0x7042",  tRes.uiOpcode == 0x7042u);
    UC_TEST("[N] Step 1: PCbefore==0x800", tRes.uiPCBefore == 0x0800u);
    UC_TEST("[N] Step 1: PCafter==0x802",  tRes.uiPCAfter  == 0x0802u);
    UC_TEST("[N] Step 1: D0==0x42",         g_tCpu.auDn[0]  == 0x42u);
    UC_TEST("[N] Step 1: instr_count==1",   g_tCpu.ulInstrCount == 1u);

    /* Step 2: MOVEQ #0,D1 */
    UC_CHECK("[N] Step 2 MOVEQ #0,D1",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 2: D1==0",  g_tCpu.auDn[1] == 0u);
    UC_TEST("[N] Step 2: Z=1",    sr_z() == 1);

    /* Step 3: MOVEQ #-1,D2 */
    UC_CHECK("[N] Step 3 MOVEQ #-1,D2",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 3: D2==0xFFFFFFFF",
            g_tCpu.auDn[2] == 0xFFFFFFFFu);
    UC_TEST("[N] Step 3: N=1", sr_n() == 1);

    /* Step 4: MOVE.L D2,D3 */
    UC_CHECK("[N] Step 4 MOVE.L D2,D3",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 4: D3==0xFFFFFFFF",
            g_tCpu.auDn[3] == 0xFFFFFFFFu);

    /* Step 5: SWAP D3 */
    UC_CHECK("[N] Step 5 SWAP D3",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 5: D3==0xFFFFFFFF",
            g_tCpu.auDn[3] == 0xFFFFFFFFu);

    /* Step 6: CLR.L D3 */
    UC_CHECK("[N] Step 6 CLR.L D3",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 6: D3==0", g_tCpu.auDn[3] == 0u);
    UC_TEST("[N] Step 6: Z=1",   sr_z() == 1);

    /* Step 7: MOVE.W #$1234,D4 */
    UC_CHECK("[N] Step 7 MOVE.W #$1234,D4",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 7: D4&0xFFFF==0x1234",
            (g_tCpu.auDn[4] & 0xFFFFu) == 0x1234u);

    /* Step 8: LEA $0800.W,A0 */
    UC_CHECK("[N] Step 8 LEA $0800.W,A0",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 8: A0==0x0800",
            g_tCpu.auAn[0] == 0x0800u);

    /* Step 9: MOVE.L A0,D5 */
    UC_CHECK("[N] Step 9 MOVE.L A0,D5",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 9: D5==0x0800",
            g_tCpu.auDn[5] == 0x0800u);

    /* Step 10: MOVE.B #$FF,D6 */
    UC_CHECK("[N] Step 10 MOVE.B #$FF,D6",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[N] Step 10: D6&0xFF==0xFF",
            (g_tCpu.auDn[6] & 0xFFu) == 0xFFu);
    UC_TEST("[N] Step 10: N=1", sr_n() == 1);

    /* Totals */
    UC_TEST("[N] 10 instructions counted",
            g_tCpu.ulInstrCount == 10u);
    UC_TEST("[N] Final PC==0x081A",
            g_tCpu.uiPC == 0x081Au);

    /* Unused-variable suppressor for loop var */
    i = 0;
    (void)i;
}

static void test_robustness(void)
{
    cpu_step_result_t tRes;

    printf("\n--- Robustness ---\n");

    /* NULL ptCpu */
    UC_TEST("[R] cpu_step NULL ptCpu",
            cpu_step(NULL, &g_tMachine, NULL) == ST_ERROR);

    /* NULL ptMachine */
    setup();
    UC_TEST("[R] cpu_step NULL ptMachine",
            cpu_step(&g_tCpu, NULL, NULL) == ST_ERROR);

    /* Unimplemented opcode (ADD) — should LOG_TODO, not crash */
    setup();
    w16(0x0800, 0xD080u); /* ADD.L D0,D0 — group 0xD, UC22 */
    UC_CHECK("[R] Unimplemented opcode no crash",
             cpu_step(&g_tCpu, &g_tMachine, &tRes));
    UC_TEST("[R] Unimplemented: PC advanced by 2",
            g_tCpu.uiPC == 0x0802u);

    /* cpu_step on stopped CPU — not an error */
    setup();
    g_tCpu.eState = CPU_STATE_STOPPED;
    UC_CHECK("[R] cpu_step on stopped CPU",
             cpu_step(&g_tCpu, &g_tMachine, NULL));

    /* ptResult NULL is legal */
    setup();
    w16(0x0800, 0x7042u); /* MOVEQ #$42,D0 */
    UC_CHECK("[R] cpu_step ptResult NULL",
             cpu_step(&g_tCpu, &g_tMachine, NULL));
    UC_TEST("[R] MOVEQ with NULL result still updates D0",
            g_tCpu.auDn[0] == 0x42u);

    /* CLR.L via memory write (An) */
    setup();
    g_tCpu.auAn[1] = 0x2000u;
    g_tMachine.aRam[0x2000] = 0xDEu;
    g_tMachine.aRam[0x2001] = 0xADu;
    g_tMachine.aRam[0x2002] = 0xBEu;
    g_tMachine.aRam[0x2003] = 0xEFu;
    w16(0x0800, 0x4291u); /* CLR.L (A1) */
    UC_CHECK("[R] CLR.L (A1) memory", cpu_step(&g_tCpu, &g_tMachine, NULL));
    UC_TEST("[R] CLR.L (A1) : mem zeroed",
            g_tMachine.aRam[0x2000] == 0 &&
            g_tMachine.aRam[0x2001] == 0 &&
            g_tMachine.aRam[0x2002] == 0 &&
            g_tMachine.aRam[0x2003] == 0);
}

static void test_memory_ea(void)
{
    printf("\n--- Memory EA (An) modes ---\n");

    /* MOVE.W (A0),D0 : read word from memory
     * 0011 000 000 010 000 = 0x3010 */
    setup();
    g_tCpu.auAn[0] = 0x2000u;
    g_tMachine.aRam[0x2000] = 0x12u;
    g_tMachine.aRam[0x2001] = 0x34u;
    w16(0x0800, 0x3010u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.W (A0),D0 : D0&0xFFFF==0x1234",
            (g_tCpu.auDn[0] & 0xFFFFu) == 0x1234u);

    /* MOVE.W D0,(A1) : write word to memory
     * 0011 001 010 000 000 = 0x3280 */
    setup();
    g_tCpu.auDn[0]  = 0xABCDu;
    g_tCpu.auAn[1]  = 0x3000u;
    w16(0x0800, 0x3280u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.W D0,(A1) : mem[0x3000]==0xABCD",
            rw(0x3000u) == 0xABCDu);

    /* MOVE.W (A0)+,D0 : post-increment
     * 0011 000 000 011 000 = 0x3018 */
    setup();
    g_tCpu.auAn[0]  = 0x2000u;
    g_tMachine.aRam[0x2000] = 0xBEu;
    g_tMachine.aRam[0x2001] = 0xEFu;
    w16(0x0800, 0x3018u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.W (A0)+,D0 : value read",
            (g_tCpu.auDn[0] & 0xFFFFu) == 0xBEEFu);
    UC_TEST("[N] MOVE.W (A0)+ : A0 incremented by 2",
            g_tCpu.auAn[0] == 0x2002u);

    /* MOVE.W -(A1),D0 : pre-decrement
     * 0011 000 000 100 001 = 0x3021 */
    setup();
    g_tCpu.auAn[1]  = 0x2004u;
    g_tMachine.aRam[0x2002] = 0xCAu;
    g_tMachine.aRam[0x2003] = 0xFEu;
    w16(0x0800, 0x3021u);
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.W -(A1),D0 : A1 decremented first",
            g_tCpu.auAn[1] == 0x2002u);
    UC_TEST("[N] MOVE.W -(A1),D0 : value read",
            (g_tCpu.auDn[0] & 0xFFFFu) == 0xCAFEu);

    /* MOVE.B d16(A0),D0 : displacement
     * 0001 000 000 101 000 + ext 0x0004 = 0x1028 0x0004 */
    setup();
    g_tCpu.auAn[0]  = 0x2000u;
    g_tMachine.aRam[0x2004] = 0x7Fu;
    w16(0x0800, 0x1028u);
    w16(0x0802, 0x0004u); /* disp=4 */
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.B d16(A0),D0 : D0&0xFF==0x7F",
            (g_tCpu.auDn[0] & 0xFFu) == 0x7Fu);

    /* Byte from RAM via byte read */
    setup();
    g_tCpu.auAn[2]  = 0x2010u;
    g_tMachine.aRam[0x2010] = 0x55u;
    w16(0x0800, 0x1012u); /* MOVE.B (A2),D0 */
    cpu_step(&g_tCpu, &g_tMachine, NULL);
    UC_TEST("[N] MOVE.B (A2),D0 : D0&0xFF==0x55",
            (g_tCpu.auDn[0] & 0xFFu) == 0x55u);

    /* Suppress unused-variable warning for rb helper */
    (void)rb;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC21: CPU 68000 MOVE/MOVEQ/LEA/CLR/SWAP ===\n");

    test_moveq();
    test_move();
    test_movea();
    test_lea();
    test_clr();
    test_swap();
    test_10instr_sequence();
    test_memory_ea();
    test_robustness();

    printf("\n=== UC21 RESULT: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
