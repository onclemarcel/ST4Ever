/*
 * use_case_28.c - UC28: Line-A traps + minimal Shifter rendering
 *
 * Validates:
 *  - linea_init(): parameter block written to RAM with correct values
 *  - linea_dispatch() LINEA_INIT ($A000): A0 set, block refreshed
 *  - linea_dispatch() stubs ($A001, $A002, unknown): no crash, ST_NO_ERROR
 *  - linea_dispatch() NULL guard: ST_ERROR without crash
 *  - CPU cpu_step() case 0xA: opcode dispatched (no LOG_TODO path)
 *  - shifter_render() on a 1-bitplane pattern (exercising UC28 pipeline)
 *
 * TEST MATRIX - UC28:
 *   [N] Nominal    : 19 tests - linea_init params, LINEA_INIT A0 return,
 *                               stub functions, cpu_step Line-A dispatch,
 *                               shifter_render 1-plane with known pattern
 *   [R] Robustness :  5 tests - NULL params to linea_init/linea_dispatch
 *   [S] Skipped    :  0 tests - all tests are headless
 */

/* INTENT[INT-LNA-001..008 -> TC-LNA-001..008 -> REQ-LNA-001,REQ-LNA-002]:
 * linea_init() writes the Line-A parameter block into ST RAM at
 * LINEA_PARAM_ADDR.  The negative-offset fields reflect the current
 * resolution: V_PLANES=4 (low), V_LIN_WR=160, V_BAS_AD=ptMachine->uiScreenBase.
 * On ST_RES_HIGH the block must show V_PLANES=1 and V_LIN_WR=80. */

/* INTENT[INT-LNA-009..014 -> TC-LNA-009..014 -> REQ-LNA-003]:
 * linea_dispatch($A000) performs LINEA_INIT: sets ptCpu->auAn[0] to
 * LINEA_PARAM_ADDR and refreshes the parameter block (V_BAS_AD updated).
 * linea_dispatch($A001/$A002/unknown) returns ST_NO_ERROR without modifying
 * A0.  cpu_step() with a $A000 opcode at PC completes without asserting
 * a LOG_TODO for the 0xA group. */

/* INTENT[INT-LNA-015..019 -> TC-LNA-015..019 -> REQ-VID-001,REQ-VID-003]:
 * After linea_init() + direct write to bitplane 0 in RAM, shifter_render()
 * produces correct pixel colours: all-0xFF plane gives all pixels = palette[1]. */

/* INTENT[INT-LNA-020..024 -> TC-LNA-020..024 -> REQ-LNA-001,REQ-LNA-003]:
 * NULL parameters to linea_init() and linea_dispatch() return ST_ERROR
 * without crashing. */

#include "use_cases.h"

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

/* Read a big-endian 16-bit word from ptMachine->aRam. */
static st_u16_t read_be16(const st_machine_t *ptMachine, st_u32_t uiAddr)
{
    if (uiAddr + 1u >= ST_RAM_SIZE)
    {
        return 0;
    }
    return (st_u16_t)( ((st_u16_t)ptMachine->aRam[uiAddr] << 8)
                     |  ptMachine->aRam[uiAddr + 1u] );
}

/* Read a big-endian 32-bit long from ptMachine->aRam. */
static st_u32_t read_be32(const st_machine_t *ptMachine, st_u32_t uiAddr)
{
    if (uiAddr + 3u >= ST_RAM_SIZE)
    {
        return 0;
    }
    return  ((st_u32_t)ptMachine->aRam[uiAddr]      << 24)
          | ((st_u32_t)ptMachine->aRam[uiAddr + 1u] << 16)
          | ((st_u32_t)ptMachine->aRam[uiAddr + 2u] <<  8)
          |  (st_u32_t)ptMachine->aRam[uiAddr + 3u];
}

/* Minimal st_machine_t initialisation without file I/O. */
static void machine_setup(st_machine_t *ptMachine,
                           st_u8_t       uiRes,
                           st_u32_t      uiScreenBase)
{
    memset(ptMachine, 0, sizeof(*ptMachine));
    ptMachine->bPoweredOn   = ST_TRUE;
    ptMachine->uiResolution = uiRes;
    ptMachine->uiScreenBase = uiScreenBase;
}

/* ------------------------------------------------------------------
 * [N] linea_init: constants and block layout
 * ------------------------------------------------------------------ */

static void test_linea_init_constants(void)
{
    printf("\n--- [N] linea_init constants and addresses ---\n");

    /* Block base is within RAM */
    TEST_ASSERT("[N] INT-LNA-001: LINEA_PARAM_ADDR < ST_RAM_SIZE",
                LINEA_PARAM_ADDR < ST_RAM_SIZE);

    /* Negative-offset area does not underflow */
    TEST_ASSERT("[N] INT-LNA-002: LINEA_PARAM_ADDR > LINEA_PARAM_NEG_SIZE",
                LINEA_PARAM_ADDR > LINEA_PARAM_NEG_SIZE);

    /* Full block fits in RAM */
    TEST_ASSERT("[N] INT-LNA-003: block end within RAM",
                LINEA_PARAM_ADDR + 32u < ST_RAM_SIZE);
}

/* ------------------------------------------------------------------
 * [N] linea_init: low-res parameter values
 * ------------------------------------------------------------------ */

static void test_linea_init_low_res(void)
{
    st_machine_t tMachine;
    st_error_t   eResult;
    st_u16_t     uiPlanes;
    st_u16_t     uiLinWr;
    st_u32_t     uiBasAd;
    st_u32_t     uiScrBase;

    printf("\n--- [N] linea_init low-res parameter block ---\n");

    uiScrBase = 0x4000u;
    machine_setup(&tMachine, ST_RES_LOW, uiScrBase);

    eResult = linea_init(&tMachine);
    TEST_ASSERT("[N] INT-LNA-004: linea_init low-res -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);

    uiPlanes = read_be16(&tMachine,
                         LINEA_PARAM_ADDR - LINEA_V_PLANES_OFF);
    TEST_ASSERT("[N] INT-LNA-005: V_PLANES=4 for low-res",
                uiPlanes == 4u);

    uiLinWr = read_be16(&tMachine,
                        LINEA_PARAM_ADDR - LINEA_V_LIN_WR_OFF);
    TEST_ASSERT("[N] INT-LNA-006: V_LIN_WR=160 for low-res",
                uiLinWr == 160u);

    uiBasAd = read_be32(&tMachine,
                        LINEA_PARAM_ADDR - LINEA_V_BAS_AD_OFF);
    TEST_ASSERT("[N] INT-LNA-007: V_BAS_AD matches uiScreenBase",
                uiBasAd == uiScrBase);
}

/* ------------------------------------------------------------------
 * [N] linea_init: high-res parameter values
 * ------------------------------------------------------------------ */

static void test_linea_init_high_res(void)
{
    st_machine_t tMachine;
    st_error_t   eResult;
    st_u16_t     uiPlanes;
    st_u16_t     uiLinWr;

    printf("\n--- [N] linea_init high-res parameter block ---\n");

    machine_setup(&tMachine, ST_RES_HIGH, 0x8000u);

    eResult = linea_init(&tMachine);
    TEST_ASSERT("[N] INT-LNA-008: linea_init high-res -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);

    uiPlanes = read_be16(&tMachine,
                         LINEA_PARAM_ADDR - LINEA_V_PLANES_OFF);
    TEST_ASSERT("[N] INT-LNA-009: V_PLANES=1 for high-res",
                uiPlanes == 1u);

    uiLinWr = read_be16(&tMachine,
                        LINEA_PARAM_ADDR - LINEA_V_LIN_WR_OFF);
    TEST_ASSERT("[N] INT-LNA-010: V_LIN_WR=80 for high-res",
                uiLinWr == 80u);
}

/* ------------------------------------------------------------------
 * [N] linea_dispatch: LINEA_INIT ($A000) sets A0
 * ------------------------------------------------------------------ */

static void test_linea_dispatch_init(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eResult;
    st_u32_t     uiNewBase;

    printf("\n--- [N] linea_dispatch LINEA_INIT ---\n");

    uiNewBase = 0x6000u;
    machine_setup(&tMachine, ST_RES_LOW, uiNewBase);
    memset(&tCpu, 0, sizeof(tCpu));
    tCpu.auAn[0] = 0xDEADu; /* sentinel — must be overwritten */

    eResult = linea_dispatch(&tCpu, &tMachine, 0xA000u);
    TEST_ASSERT("[N] INT-LNA-011: LINEA_INIT -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-LNA-012: LINEA_INIT sets A0 = LINEA_PARAM_ADDR",
                tCpu.auAn[0] == LINEA_PARAM_ADDR);

    /* V_BAS_AD in block matches the new screen base */
    {
        st_u32_t uiBasAd = read_be32(&tMachine,
                                      LINEA_PARAM_ADDR - LINEA_V_BAS_AD_OFF);
        TEST_ASSERT("[N] INT-LNA-013: LINEA_INIT refreshes V_BAS_AD",
                    uiBasAd == uiNewBase);
    }
}

/* ------------------------------------------------------------------
 * [N] linea_dispatch: stub functions ($A001, $A002, unknown)
 * ------------------------------------------------------------------ */

static void test_linea_dispatch_stubs(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eResult;

    printf("\n--- [N] linea_dispatch stubs ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x4000u);
    memset(&tCpu, 0, sizeof(tCpu));

    /* PUT_PIXEL stub */
    eResult = linea_dispatch(&tCpu, &tMachine, 0xA001u);
    TEST_ASSERT("[N] INT-LNA-014: PUT_PIXEL stub -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);

    /* GET_PIXEL stub returns 0 in D0 */
    tCpu.auDn[0] = 0xFFFFu;
    eResult = linea_dispatch(&tCpu, &tMachine, 0xA002u);
    TEST_ASSERT("[N] INT-LNA-015: GET_PIXEL stub -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-LNA-016: GET_PIXEL stub sets D0=0",
                tCpu.auDn[0] == 0u);

    /* Unknown Line-A function */
    eResult = linea_dispatch(&tCpu, &tMachine, 0xA0FFu);
    TEST_ASSERT("[N] INT-LNA-017: unknown Line-A fn -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
}

/* ------------------------------------------------------------------
 * [N] CPU cpu_step: $A000 opcode dispatched without fatal error
 * ------------------------------------------------------------------ */

static void test_cpu_step_line_a(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eResult;

    /* Encode LINEA_INIT at RAM[0x1000]: $A000 (big-endian) */
    static const st_u32_t EXEC_BASE = 0x1000u;

    printf("\n--- [N] cpu_step Line-A opcode dispatch ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x4000u);
    memset(&tCpu, 0, sizeof(tCpu));

    /* Place $A000 opcode at EXEC_BASE */
    tMachine.aRam[EXEC_BASE]     = 0xA0u;
    tMachine.aRam[EXEC_BASE + 1] = 0x00u;
    /* Place NOP ($4E71) after, so CPU has valid next opcode */
    tMachine.aRam[EXEC_BASE + 2] = 0x4Eu;
    tMachine.aRam[EXEC_BASE + 3] = 0x71u;

    tCpu.uiPC    = EXEC_BASE;
    tCpu.uiSR    = 0x2700u; /* supervisor, int mask 7 */
    tCpu.eState  = CPU_STATE_RUNNING;
    tCpu.auAn[0] = 0xBEEFu; /* sentinel */

    eResult = cpu_step(&tCpu, &tMachine, NULL);
    TEST_ASSERT("[N] INT-LNA-018: cpu_step $A000 -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-LNA-019: cpu_step $A000 sets A0 = LINEA_PARAM_ADDR",
                tCpu.auAn[0] == LINEA_PARAM_ADDR);
    TEST_ASSERT("[N] INT-LNA-020: cpu_step $A000 advances PC by 2",
                tCpu.uiPC == EXEC_BASE + 2u);
}

/* ------------------------------------------------------------------
 * [N] shifter_render: 1-bitplane pattern (high-res, plane0=0xFF...)
 * ------------------------------------------------------------------ */

static void test_shifter_render_1plane(void)
{
    st_machine_t tMachine;
    st_u32_t     auPixels[SHIFTER_MAX_PIXELS];
    int          iWidth;
    int          iHeight;
    st_error_t   eResult;
    /* Screen base well within RAM */
    static const st_u32_t SCRBUF = 0x10000u;
    unsigned int uiRowByte;
    st_u8_t      uiExpectedPx0;

    printf("\n--- [N] shifter_render 1-bitplane high-res pattern ---\n");

    machine_setup(&tMachine, ST_RES_HIGH, SCRBUF);

    /* High-res: 640x400, 1 plane, 80 bytes/scanline
     * Fill first 80 bytes of first scanline with 0xFF
     * (all bits set -> every pixel picks palette[1]) */
    for (uiRowByte = 0; uiRowByte < 80u; uiRowByte++)
    {
        tMachine.aRam[SCRBUF + uiRowByte] = 0xFFu;
    }

    /* palette[0] = 0x0000 (black), palette[1] = 0x0777 (white ST) */
    tMachine.auPalette[0] = 0x0000u;
    tMachine.auPalette[1] = 0x0777u;

    iWidth  = 0;
    iHeight = 0;
    eResult = shifter_render(&tMachine,
                              auPixels,
                              SHIFTER_MAX_PIXELS,
                              &iWidth,
                              &iHeight);

    TEST_ASSERT("[N] INT-LNA-021: shifter_render 1-plane -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-LNA-022: rendered width = 640",
                iWidth == 640);
    TEST_ASSERT("[N] INT-LNA-023: rendered height = 400",
                iHeight == 400);

    /* First pixel in first row: bit 7 of byte 0 = 1 -> palette[1] */
    uiExpectedPx0 = (tMachine.auPalette[1] >> 8) & 0xFFu; /* R component */
    /* shifter_palette_to_rgb(0x0777) → 0x00FFFFFF; R byte = 0xFF */
    TEST_ASSERT("[N] INT-LNA-024: pixel[0] has R=0xFF (from palette[1]=0x0777)",
                ((auPixels[0] >> 16) & 0xFFu) == 0xFFu);

    /* First pixel of second row (byte 80): plane byte = 0, so palette[0]
     * palette[0]=0x0000, R=0 */
    TEST_ASSERT("[N] INT-LNA-025: pixel[640] R=0x00 (from palette[0]=0x0000)",
                ((auPixels[640] >> 16) & 0xFFu) == 0x00u);

    ST_UNUSED(uiExpectedPx0);
}

/* ------------------------------------------------------------------
 * [R] Robustness
 * ------------------------------------------------------------------ */

static void test_robustness(void)
{
    st_machine_t tMachine;
    cpu68k_t     tCpu;
    st_error_t   eResult;

    printf("\n--- [R] Robustness ---\n");

    machine_setup(&tMachine, ST_RES_LOW, 0x4000u);
    memset(&tCpu, 0, sizeof(tCpu));

    /* linea_init NULL */
    eResult = linea_init(NULL);
    TEST_ASSERT("[R] INT-LNA-R01: linea_init(NULL) -> ST_ERROR",
                eResult == ST_ERROR);

    /* linea_dispatch NULL ptCpu */
    eResult = linea_dispatch(NULL, &tMachine, 0xA000u);
    TEST_ASSERT("[R] INT-LNA-R02: linea_dispatch NULL ptCpu -> ST_ERROR",
                eResult == ST_ERROR);

    /* linea_dispatch NULL ptMachine */
    eResult = linea_dispatch(&tCpu, NULL, 0xA000u);
    TEST_ASSERT("[R] INT-LNA-R03: linea_dispatch NULL ptMachine -> ST_ERROR",
                eResult == ST_ERROR);

    /* linea_dispatch both NULL */
    eResult = linea_dispatch(NULL, NULL, 0xA000u);
    TEST_ASSERT("[R] INT-LNA-R04: linea_dispatch NULL,NULL -> ST_ERROR",
                eResult == ST_ERROR);

    /* cpu_step with NULL ptResult is allowed */
    {
        st_machine_t tM2;
        cpu68k_t     tC2;
        static const st_u32_t BASE2 = 0x2000u;

        machine_setup(&tM2, ST_RES_LOW, 0x4000u);
        memset(&tC2, 0, sizeof(tC2));
        tM2.aRam[BASE2]     = 0xA0u;
        tM2.aRam[BASE2 + 1] = 0x00u;
        tC2.uiPC   = BASE2;
        tC2.uiSR   = 0x2700u;
        tC2.eState = CPU_STATE_RUNNING;
        eResult = cpu_step(&tC2, &tM2, NULL);
        TEST_ASSERT("[R] INT-LNA-R05: cpu_step $A000 NULL ptResult -> "
                    "ST_NO_ERROR",
                    eResult == ST_NO_ERROR);
    }
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC28: Line-A traps + minimal Shifter rendering ===\n");

    test_linea_init_constants();
    test_linea_init_low_res();
    test_linea_init_high_res();
    test_linea_dispatch_init();
    test_linea_dispatch_stubs();
    test_cpu_step_line_a();
    test_shifter_render_1plane();
    test_robustness();

    printf("\n=== UC28: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
