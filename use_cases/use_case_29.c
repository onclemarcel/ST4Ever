/*
 * use_case_29.c - UC29: XBIOS/GEMDOS minimal + Line-A PUT_PIXEL
 *
 * Validates:
 *  - tos_gemdos(): Pterm0/Pterm set CPU_STATE_STOPPED; unknown fn = no crash
 *  - tos_xbios(): Vsync (no-op); Setcolor (palette word updated);
 *                 Setpalette (all 16 words updated); Setscreen (base+rez)
 *  - cpu_step() TRAP #1 -> tos_gemdos; TRAP #14 -> tos_xbios
 *  - linea_dispatch $A001 PUT_PIXEL: bitplane write in low/high resolution
 *  - NULL parameter guards on tos_gemdos/tos_xbios
 *
 * TEST MATRIX - UC29:
 *   [N] Nominal    : 28 tests - GEMDOS Pterm0(3)/Pterm(2)/unknown(2),
 *                               XBIOS Vsync(2)/Setcolor(2)/Setpalette(4)/
 *                               Setscreen(5)/cpu_step dispatch(2),
 *                               PUT_PIXEL low-res(4) + high-res(2)
 *   [R] Robustness :  9 tests - PUT_PIXEL clamp OOB(3) + NULL guards(4) +
 *                               Setcolor idx OOB(2)
 *   [S] Skipped    :  0 tests - all tests are headless
 */

/* INTENT[INT-TOS-001..004 -> TC-TOS-001..004 -> REQ-TOS-001]:
 * tos_gemdos() reads the function number from the user stack and dispatches
 * to the correct handler.  Pterm0 (fn=0) and Pterm (fn=76) set
 * ptCpu->eState = CPU_STATE_STOPPED.  Unknown functions return ST_NO_ERROR
 * and leave eState unchanged. */

/* INTENT[INT-TOS-005..012 -> TC-TOS-005..012 -> REQ-TOS-002]:
 * tos_xbios() reads the function number from SP and dispatches accordingly.
 * Vsync (37): no-op, D0=0.  Setcolor (7): one palette entry updated via
 * st_write_word to 0xFF8240+(idx*2).  Setpalette (5): all 16 entries read
 * from RAM palptr and written to HW palette registers.
 * Setscreen (3): physbase != -1 updates screen base bytes; rez != -1 updates
 * resolution register; -1 sentinel leaves the corresponding field unchanged. */

/* INTENT[INT-TOS-013..016 -> TC-TOS-013..016 -> REQ-TOS-001,REQ-TOS-002]:
 * cpu_step() with TRAP #1 (0x4E41) routes to tos_gemdos; with TRAP #14
 * (0x4E4E) routes to tos_xbios.  Both terminate via CPU_STATE_STOPPED for
 * Pterm0/Pterm, observable without running the full emulator loop. */

/* INTENT[INT-TOS-017..022 -> TC-TOS-017..022 -> REQ-LNA-003]:
 * linea_dispatch $A001 (PUT_PIXEL) writes D0=color (index 0-15) into the
 * correct bitplane words at (D2=x, D1=y).  Low-res: 4 planes interleaved
 * 8 bytes/group; plane bit = (color >> plane) & 1.  High-res (1 plane):
 * 80 bytes/line, 2 bytes/group, only plane 0.  Out-of-bounds coords are
 * silently clamped (no RAM write, no crash). */

/* INTENT[INT-TOS-023..025 -> TC-TOS-023..025 -> REQ-TOS-001,REQ-TOS-002]:
 * NULL parameters to tos_gemdos and tos_xbios return ST_ERROR.
 * Setcolor with index >= 16 returns ST_NO_ERROR but logs an error and
 * does not write to the palette registers. */

#include "use_cases.h"

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

/* Read a big-endian 16-bit word from RAM. */
static st_u16_t read_be16(const st_machine_t *ptMachine,
                            st_u32_t            uiAddr)
{
    if (uiAddr + 1u >= ST_RAM_SIZE)
        return 0;
    return (st_u16_t)(((st_u16_t)ptMachine->aRam[uiAddr] << 8)
                     | ptMachine->aRam[uiAddr + 1u]);
}

/* Write a big-endian 16-bit word into RAM. */
static void write_be16(st_machine_t *ptMachine,
                        st_u32_t      uiAddr,
                        st_u16_t      uiVal)
{
    if (uiAddr + 1u < ST_RAM_SIZE)
    {
        ptMachine->aRam[uiAddr]      = (st_u8_t)(uiVal >> 8);
        ptMachine->aRam[uiAddr + 1u] = (st_u8_t)(uiVal & 0xFFu);
    }
}

/* Write a big-endian 32-bit long into RAM. */
static void write_be32(st_machine_t *ptMachine,
                        st_u32_t      uiAddr,
                        st_u32_t      uiVal)
{
    if (uiAddr + 3u < ST_RAM_SIZE)
    {
        ptMachine->aRam[uiAddr]      = (st_u8_t)(uiVal >> 24);
        ptMachine->aRam[uiAddr + 1u] = (st_u8_t)((uiVal >> 16) & 0xFFu);
        ptMachine->aRam[uiAddr + 2u] = (st_u8_t)((uiVal >>  8) & 0xFFu);
        ptMachine->aRam[uiAddr + 3u] = (st_u8_t)(uiVal & 0xFFu);
    }
}

/* Minimal machine setup without file I/O. */
static void machine_setup(st_machine_t *ptMachine,
                           st_u8_t       uiRes,
                           st_u32_t      uiScreenBase)
{
    memset(ptMachine, 0, sizeof(*ptMachine));
    ptMachine->bPoweredOn   = ST_TRUE;
    ptMachine->uiResolution = uiRes;
    ptMachine->uiScreenBase = uiScreenBase;
}

/* Minimal CPU setup: SP at iSP, in RUNNING state. */
static void cpu_setup(cpu68k_t *ptCpu, st_u32_t uiSP)
{
    memset(ptCpu, 0, sizeof(*ptCpu));
    ptCpu->eState   = CPU_STATE_RUNNING;
    ptCpu->auAn[7]  = uiSP;
}

/* ------------------------------------------------------------------
 * [N] GEMDOS Pterm0 / Pterm
 * ------------------------------------------------------------------ */

static void test_gemdos_pterm0(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;

    printf("\n--- [N] GEMDOS Pterm0 (fn=0) ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);
    linea_init(&tMachine);

    /* Push function number 0 at SP */
    write_be16(&tMachine, 0x8000u, 0u);

    eR = tos_gemdos(&tCpu, &tMachine);
    TEST_ASSERT("[N] INT-TOS-001: Pterm0 returns ST_NO_ERROR",
                eR == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-TOS-002: Pterm0 sets CPU_STATE_STOPPED",
                tCpu.eState == CPU_STATE_STOPPED);
    TEST_ASSERT("[N] INT-TOS-003: Pterm0 sets D0=0",
                tCpu.auDn[0] == 0u);
}

static void test_gemdos_pterm(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;

    printf("\n--- [N] GEMDOS Pterm (fn=76) ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);

    /* Push: retcode=42, then function number 76 */
    write_be16(&tMachine, 0x8000u, 76u);   /* SP+0: fn */
    write_be16(&tMachine, 0x8002u, 42u);   /* SP+2: retcode */

    eR = tos_gemdos(&tCpu, &tMachine);
    TEST_ASSERT("[N] INT-TOS-004: Pterm returns ST_NO_ERROR",
                eR == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-TOS-005: Pterm sets CPU_STATE_STOPPED",
                tCpu.eState == CPU_STATE_STOPPED);
}

static void test_gemdos_unknown(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;

    printf("\n--- [N] GEMDOS unknown function ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);

    write_be16(&tMachine, 0x8000u, 99u);   /* fn=99 unknown */

    eR = tos_gemdos(&tCpu, &tMachine);
    TEST_ASSERT("[N] INT-TOS-006: unknown GEMDOS fn returns ST_NO_ERROR",
                eR == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-TOS-007: unknown GEMDOS fn leaves CPU RUNNING",
                tCpu.eState == CPU_STATE_RUNNING);
}

/* ------------------------------------------------------------------
 * [N] XBIOS Vsync
 * ------------------------------------------------------------------ */

static void test_xbios_vsync(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;

    printf("\n--- [N] XBIOS Vsync (fn=37) ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);

    write_be16(&tMachine, 0x8000u, 37u);   /* fn=37 Vsync */

    eR = tos_xbios(&tCpu, &tMachine);
    TEST_ASSERT("[N] INT-TOS-008: Vsync returns ST_NO_ERROR",
                eR == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-TOS-009: Vsync leaves CPU RUNNING",
                tCpu.eState == CPU_STATE_RUNNING);
}

/* ------------------------------------------------------------------
 * [N] XBIOS Setcolor
 * ------------------------------------------------------------------ */

static void test_xbios_setcolor(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;

    printf("\n--- [N] XBIOS Setcolor (fn=7) ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);

    /* Stack: fn=7, index=3, color=0x700 (pure red in ST 3-bit) */
    write_be16(&tMachine, 0x8000u, 7u);       /* SP+0: fn   */
    write_be16(&tMachine, 0x8002u, 3u);       /* SP+2: idx  */
    write_be16(&tMachine, 0x8004u, 0x0700u);  /* SP+4: color*/

    eR = tos_xbios(&tCpu, &tMachine);
    TEST_ASSERT("[N] INT-TOS-010: Setcolor returns ST_NO_ERROR",
                eR == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-TOS-011: Setcolor updates palette[3]",
                tMachine.auPalette[3] == 0x0700u);
}

/* ------------------------------------------------------------------
 * [N] XBIOS Setpalette
 * ------------------------------------------------------------------ */

static void test_xbios_setpalette(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;
    unsigned     i;
    st_u32_t     uiPalPtr = 0x9000u;  /* palette array in RAM */

    printf("\n--- [N] XBIOS Setpalette (fn=5) ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);

    /* Build a 16-entry palette array in RAM at 0x9000 */
    for (i = 0u; i < 16u; i++)
    {
        /* colour = index * 0x100 (gives distinct non-zero values) */
        write_be16(&tMachine,
                   uiPalPtr + (st_u32_t)(i * 2u),
                   (st_u16_t)(i * 0x100u));
    }

    /* Stack: fn=5, palptr=0x9000 */
    write_be16(&tMachine, 0x8000u, 5u);
    write_be32(&tMachine, 0x8002u, uiPalPtr);

    eR = tos_xbios(&tCpu, &tMachine);
    TEST_ASSERT("[N] INT-TOS-012: Setpalette returns ST_NO_ERROR",
                eR == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-TOS-013: Setpalette updates palette[0]",
                tMachine.auPalette[0] == 0u);
    TEST_ASSERT("[N] INT-TOS-014: Setpalette updates palette[5]",
                tMachine.auPalette[5] == 0x0500u);
    TEST_ASSERT("[N] INT-TOS-015: Setpalette updates palette[15]",
                tMachine.auPalette[15] == 0x0F00u);
}

/* ------------------------------------------------------------------
 * [N] XBIOS Setscreen
 * ------------------------------------------------------------------ */

static void test_xbios_setscreen(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;

    printf("\n--- [N] XBIOS Setscreen (fn=3) ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);

    /*
     * Setscreen(physbase=0x50000, logbase=-1, rez=1 med):
     *   SP+0:  fn=3 (word)
     *   SP+2:  physbase=0x50000 (long)
     *   SP+6:  logbase=-1=0xFFFFFFFF (long)
     *   SP+10: rez=1 (word)
     */
    write_be16(&tMachine, 0x8000u,  3u);
    write_be32(&tMachine, 0x8002u,  0x50000u);
    write_be32(&tMachine, 0x8006u,  0xFFFFFFFFu);
    write_be16(&tMachine, 0x800Au,  1u);

    eR = tos_xbios(&tCpu, &tMachine);
    TEST_ASSERT("[N] INT-TOS-016: Setscreen returns ST_NO_ERROR",
                eR == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-TOS-017: Setscreen updates screen base",
                tMachine.uiScreenBase == 0x50000u);
    TEST_ASSERT("[N] INT-TOS-018: Setscreen updates resolution",
                tMachine.uiResolution == 1u);

    /* Now call again with physbase=-1 (leave unchanged), rez=-1 (leave) */
    machine_setup(&tMachine, ST_RES_MED, 0x30000u);
    cpu_setup(&tCpu, 0x8000u);

    write_be16(&tMachine, 0x8000u,  3u);
    write_be32(&tMachine, 0x8002u,  0xFFFFFFFFu);   /* physbase=-1 */
    write_be32(&tMachine, 0x8006u,  0xFFFFFFFFu);
    write_be16(&tMachine, 0x800Au,  0xFFFFu);        /* rez=-1 */

    eR = tos_xbios(&tCpu, &tMachine);
    TEST_ASSERT("[N] INT-TOS-019: Setscreen -1 sentinel leaves base unchanged",
                tMachine.uiScreenBase == 0x30000u);
    TEST_ASSERT("[N] INT-TOS-020: Setscreen -1 sentinel leaves rez unchanged",
                tMachine.uiResolution == (st_u8_t)ST_RES_MED);
}

/* ------------------------------------------------------------------
 * [N] cpu_step TRAP #1 and TRAP #14 dispatch
 * ------------------------------------------------------------------ */

static void test_cpu_step_trap_dispatch(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_u32_t     uiCodeBase = 0xC000u;  /* program load area */

    printf("\n--- [N] cpu_step TRAP #1 / TRAP #14 dispatch ---\n");

    /* TRAP #1 (Pterm0, fn=0) */
    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);
    linea_init(&tMachine);

    /* Write TRAP #1 opcode (0x4E41) at code_base */
    tMachine.aRam[uiCodeBase]     = 0x4Eu;
    tMachine.aRam[uiCodeBase + 1] = 0x41u;
    tCpu.uiPC = uiCodeBase;

    /* Function number 0 at SP */
    write_be16(&tMachine, 0x8000u, 0u);

    (void)cpu_step(&tCpu, &tMachine, NULL);
    TEST_ASSERT("[N] INT-TOS-021: TRAP #1 (0x4E41) routes to tos_gemdos",
                tCpu.eState == CPU_STATE_STOPPED);

    /* TRAP #14 (Vsync, fn=37) */
    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);
    linea_init(&tMachine);

    /* Write TRAP #14 opcode (0x4E4E) at code_base */
    tMachine.aRam[uiCodeBase]     = 0x4Eu;
    tMachine.aRam[uiCodeBase + 1] = 0x4Eu;
    tCpu.uiPC = uiCodeBase;

    write_be16(&tMachine, 0x8000u, 37u);   /* fn=37 Vsync */

    (void)cpu_step(&tCpu, &tMachine, NULL);
    TEST_ASSERT("[N] INT-TOS-022: TRAP #14 (0x4E4E) routes to tos_xbios",
                tCpu.eState == CPU_STATE_RUNNING);
}

/* ------------------------------------------------------------------
 * [N] PUT_PIXEL: low resolution, known bitplane pattern
 * ------------------------------------------------------------------ */

static void test_put_pixel_low_res(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_u32_t     uiBase = 0x10000u;
    st_u16_t     uiWord;

    printf("\n--- [N] PUT_PIXEL low res (320x200, 4 planes) ---\n");

    machine_setup(&tMachine, ST_RES_LOW, uiBase);
    cpu_setup(&tCpu, 0x8000u);
    linea_init(&tMachine);

    /*
     * Pixel (0,0) color=1 (bit 0 set → plane 0 bit 15 = 1):
     *   plane 0 word at uiBase + 0 = should have bit 15 set → 0x8000
     *   planes 1-3 at uiBase+2,4,6 should be 0
     */
    tCpu.auDn[0] = 1u;  /* color=1 */
    tCpu.auDn[1] = 0u;  /* y=0     */
    tCpu.auDn[2] = 0u;  /* x=0     */
    (void)linea_dispatch(&tCpu, &tMachine, 0xA001u);

    uiWord = read_be16(&tMachine, uiBase);
    TEST_ASSERT("[N] INT-TOS-023: PUT_PIXEL (0,0) color=1 sets plane0 bit15",
                uiWord == 0x8000u);
    uiWord = read_be16(&tMachine, uiBase + 2u);
    TEST_ASSERT("[N] INT-TOS-024: PUT_PIXEL (0,0) color=1 leaves plane1=0",
                uiWord == 0x0000u);

    /*
     * Pixel (1,0) color=3 (bits 0+1 set → plane0 bit14=1, plane1 bit14=1):
     *   plane 0 word at uiBase: bit 14 also set → 0xC000
     *   plane 1 word at uiBase+2: bit 14 set → 0x4000
     */
    tCpu.auDn[0] = 3u;  /* color=3 */
    tCpu.auDn[1] = 0u;
    tCpu.auDn[2] = 1u;  /* x=1     */
    (void)linea_dispatch(&tCpu, &tMachine, 0xA001u);

    uiWord = read_be16(&tMachine, uiBase);
    TEST_ASSERT("[N] INT-TOS-025: PUT_PIXEL (1,0) color=3 sets plane0 bit14",
                (uiWord & 0x4000u) != 0u);
    uiWord = read_be16(&tMachine, uiBase + 2u);
    TEST_ASSERT("[N] INT-TOS-026: PUT_PIXEL (1,0) color=3 sets plane1 bit14",
                (uiWord & 0x4000u) != 0u);
}

/* ------------------------------------------------------------------
 * [N] PUT_PIXEL: high resolution, 1 plane
 * ------------------------------------------------------------------ */

static void test_put_pixel_high_res(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_u32_t     uiBase = 0x10000u;
    st_u16_t     uiWord;

    printf("\n--- [N] PUT_PIXEL high res (640x400, 1 plane) ---\n");

    machine_setup(&tMachine, ST_RES_HIGH, uiBase);
    cpu_setup(&tCpu, 0x8000u);
    linea_init(&tMachine);

    /*
     * High res: 80 bytes/line, 2 bytes/group (1 plane).
     * Pixel (16, 0): word_group=1, bit=15 → addr = uiBase + 0 + 1*2 = uiBase+2
     */
    tCpu.auDn[0] = 1u;  /* color=1 (mono) */
    tCpu.auDn[1] = 0u;  /* y=0 */
    tCpu.auDn[2] = 16u; /* x=16 → word_group=1 */
    (void)linea_dispatch(&tCpu, &tMachine, 0xA001u);

    uiWord = read_be16(&tMachine, uiBase + 2u);
    TEST_ASSERT("[N] INT-TOS-027: PUT_PIXEL high res (16,0) sets word at +2",
                uiWord == 0x8000u);

    /* color=0 clears the bit */
    tCpu.auDn[0] = 0u;
    tCpu.auDn[1] = 0u;
    tCpu.auDn[2] = 16u;
    (void)linea_dispatch(&tCpu, &tMachine, 0xA001u);

    uiWord = read_be16(&tMachine, uiBase + 2u);
    TEST_ASSERT("[N] INT-TOS-028: PUT_PIXEL color=0 clears the bit",
                uiWord == 0x0000u);
}

/* ------------------------------------------------------------------
 * [R] PUT_PIXEL out-of-bounds clamping
 * ------------------------------------------------------------------ */

static void test_put_pixel_clamp(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_u32_t     uiBase = 0x10000u;
    st_u16_t     uiWord;
    st_error_t   eR;

    printf("\n--- [R] PUT_PIXEL out-of-bounds clamp ---\n");

    machine_setup(&tMachine, ST_RES_LOW, uiBase);
    cpu_setup(&tCpu, 0x8000u);
    linea_init(&tMachine);

    /* x=320 (out of range for 320px wide low res) */
    tCpu.auDn[0] = 15u;
    tCpu.auDn[1] = 0u;
    tCpu.auDn[2] = 320u;
    eR = linea_dispatch(&tCpu, &tMachine, 0xA001u);
    TEST_ASSERT("[R] INT-TOS-029: PUT_PIXEL x>=width returns ST_NO_ERROR",
                eR == ST_NO_ERROR);

    /* Negative x (sign-extended word -1 = 0xFFFF → as int16 = -1) */
    tCpu.auDn[0] = 15u;
    tCpu.auDn[1] = 0u;
    tCpu.auDn[2] = 0xFFFFu;  /* -1 as word */
    eR = linea_dispatch(&tCpu, &tMachine, 0xA001u);
    TEST_ASSERT("[R] INT-TOS-030: PUT_PIXEL x<0 returns ST_NO_ERROR",
                eR == ST_NO_ERROR);

    /* Verify no writes at uiBase (bitplane untouched by clamped calls) */
    uiWord = read_be16(&tMachine, uiBase);
    TEST_ASSERT("[R] INT-TOS-031: PUT_PIXEL clamped leaves bitplane unchanged",
                uiWord == 0x0000u);
}

/* ------------------------------------------------------------------
 * [R] NULL parameter guards
 * ------------------------------------------------------------------ */

static void test_null_guards(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;

    printf("\n--- [R] NULL parameter guards ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);

    eR = tos_gemdos(NULL, &tMachine);
    TEST_ASSERT("[R] INT-TOS-032: tos_gemdos(NULL, machine) = ST_ERROR",
                eR == ST_ERROR);

    eR = tos_gemdos(&tCpu, NULL);
    TEST_ASSERT("[R] INT-TOS-033: tos_gemdos(cpu, NULL) = ST_ERROR",
                eR == ST_ERROR);

    eR = tos_xbios(NULL, &tMachine);
    TEST_ASSERT("[R] INT-TOS-034: tos_xbios(NULL, machine) = ST_ERROR",
                eR == ST_ERROR);

    eR = tos_xbios(&tCpu, NULL);
    TEST_ASSERT("[R] INT-TOS-035: tos_xbios(cpu, NULL) = ST_ERROR",
                eR == ST_ERROR);
}

/* ------------------------------------------------------------------
 * [R] Setcolor with out-of-range index
 * ------------------------------------------------------------------ */

static void test_xbios_setcolor_oob(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eR;

    printf("\n--- [R] XBIOS Setcolor out-of-range index ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x10000u);
    cpu_setup(&tCpu, 0x8000u);

    /* index=16 is out of range [0..15] */
    write_be16(&tMachine, 0x8000u, 7u);       /* fn=7 Setcolor */
    write_be16(&tMachine, 0x8002u, 16u);      /* idx=16 OOB */
    write_be16(&tMachine, 0x8004u, 0x0700u);  /* color */

    eR = tos_xbios(&tCpu, &tMachine);
    TEST_ASSERT("[R] INT-TOS-036: Setcolor idx=16 returns ST_NO_ERROR",
                eR == ST_NO_ERROR);
    /* palette[0] must not have been touched */
    TEST_ASSERT("[R] INT-TOS-037: Setcolor OOB leaves palette unchanged",
                tMachine.auPalette[0] == 0u);
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC29: XBIOS/GEMDOS minimal + Line-A PUT_PIXEL ===\n");

    test_gemdos_pterm0();
    test_gemdos_pterm();
    test_gemdos_unknown();
    test_xbios_vsync();
    test_xbios_setcolor();
    test_xbios_setpalette();
    test_xbios_setscreen();
    test_cpu_step_trap_dispatch();
    test_put_pixel_low_res();
    test_put_pixel_high_res();
    test_put_pixel_clamp();
    test_null_guards();
    test_xbios_setcolor_oob();

    printf("\n=== UC29 Results: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
