/*
 * use_case_26.c - UC26: Shifter video rendering engine
 *
 * Validates shifter_palette_to_rgb(), shifter_screen_size() and
 * shifter_render() for all three Atari ST resolutions (low/med/high).
 *
 * TEST MATRIX - UC26:
 *   [N] Nominal    : 26 tests - palette conversion (6), screen size (4),
 *                               low res pixel decoding (8), med res (3),
 *                               high res (5)
 *   [R] Robustness :  7 tests - NULL params (4), buffer too small (1),
 *                               screen base out of RAM (1), bad resolution (1)
 *   [S] Skipped    :  0 tests
 */

/* INTENT[INT-VID-001..006 -> TC-VID-001..006 -> REQ-VID-001]:
 * shifter_palette_to_rgb() maps the three 3-bit ST colour channels
 * (bits [10:8]=R, [6:4]=G, [2:0]=B) to 0x00RRGGBB using (n*255)/7.
 * Black (0x0000) -> 0x00000000; White (0x0777) -> 0x00FFFFFF;
 * Pure R/G/B primaries map their channel to 0xFF, others to 0x00.
 * Channel value 1 maps to 255/7 = 36 (integer division). */

/* INTENT[INT-VID-007..010 -> TC-VID-007..010 -> REQ-VID-002]:
 * shifter_screen_size() returns correct dimensions per resolution:
 * LOW=320x200, MED=640x200, HIGH=640x400, unknown -> 0x0. */

/* INTENT[INT-VID-011..019 -> TC-VID-011..019 -> REQ-VID-003]:
 * shifter_render() decodes bitplane memory at ptMachine->uiScreenBase
 * to a flat row-major RGB32 array.  Low res: 4 planes interleaved per
 * 16-pixel group; plane0 bit = LSB of colour index.  Med res: 2 planes.
 * High res: 1 plane, bit=1 -> palette[1], bit=0 -> palette[0].
 * Returns ST_NO_ERROR and sets *piWidth/*piHeight on success. */

/* INTENT[INT-VID-020..023 -> TC-VID-020..023 -> REQ-VID-003,REQ-VID-004]:
 * shifter_render() returns ST_ERROR for NULL ptMachine, NULL auPixels,
 * NULL piWidth, NULL piHeight, pixel buffer too small, screen base
 * extending beyond ST_RAM_SIZE, or unknown resolution. */

#include "use_cases.h"

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Shared pixel buffer — static to avoid 1 MB stack frame
 * ------------------------------------------------------------------ */

static st_u32_t g_aPixels[SHIFTER_MAX_PIXELS];

/* ------------------------------------------------------------------
 * [N] Palette conversion
 * ------------------------------------------------------------------ */

static void test_palette(void)
{
    st_u32_t uiRgb;
    st_u8_t  uiR;
    st_u8_t  uiG;
    st_u8_t  uiB;

    printf("\n--- [N] Palette conversion ---\n");

    uiRgb = shifter_palette_to_rgb(0x0000u);
    TEST_ASSERT("[N] INT-VID-001: black 0x0000 -> 0x00000000",
                uiRgb == 0x00000000u);

    uiRgb = shifter_palette_to_rgb(0x0777u);
    TEST_ASSERT("[N] INT-VID-002: white 0x0777 -> 0x00FFFFFF",
                uiRgb == 0x00FFFFFFu);

    uiRgb = shifter_palette_to_rgb(0x0700u);
    uiR   = (st_u8_t)((uiRgb >> 16u) & 0xFFu);
    uiG   = (st_u8_t)((uiRgb >>  8u) & 0xFFu);
    uiB   = (st_u8_t)( uiRgb         & 0xFFu);
    TEST_ASSERT("[N] INT-VID-003: red 0x0700 -> R=255,G=0,B=0",
                uiR == 0xFFu && uiG == 0x00u && uiB == 0x00u);

    uiRgb = shifter_palette_to_rgb(0x0070u);
    uiR   = (st_u8_t)((uiRgb >> 16u) & 0xFFu);
    uiG   = (st_u8_t)((uiRgb >>  8u) & 0xFFu);
    uiB   = (st_u8_t)( uiRgb         & 0xFFu);
    TEST_ASSERT("[N] INT-VID-004: green 0x0070 -> R=0,G=255,B=0",
                uiR == 0x00u && uiG == 0xFFu && uiB == 0x00u);

    uiRgb = shifter_palette_to_rgb(0x0007u);
    uiR   = (st_u8_t)((uiRgb >> 16u) & 0xFFu);
    uiG   = (st_u8_t)((uiRgb >>  8u) & 0xFFu);
    uiB   = (st_u8_t)( uiRgb         & 0xFFu);
    TEST_ASSERT("[N] INT-VID-005: blue 0x0007 -> R=0,G=0,B=255",
                uiR == 0x00u && uiG == 0x00u && uiB == 0xFFu);

    uiRgb = shifter_palette_to_rgb(0x0111u);
    uiR   = (st_u8_t)((uiRgb >> 16u) & 0xFFu);
    TEST_ASSERT("[N] INT-VID-006: channel 1 -> 36 (255/7)",
                uiR == (st_u8_t)((1u * 255u) / 7u));
}

/* ------------------------------------------------------------------
 * [N] Screen size queries
 * ------------------------------------------------------------------ */

static void test_screen_size(void)
{
    int iW;
    int iH;

    printf("\n--- [N] Screen size ---\n");

    iW = 0; iH = 0;
    shifter_screen_size(ST_RES_LOW, &iW, &iH);
    TEST_ASSERT("[N] INT-VID-007: low res 320x200",
                iW == SHIFTER_WIDTH_LOW && iH == SHIFTER_HEIGHT_LOW);

    iW = 0; iH = 0;
    shifter_screen_size(ST_RES_MED, &iW, &iH);
    TEST_ASSERT("[N] INT-VID-008: med res 640x200",
                iW == SHIFTER_WIDTH_MED && iH == SHIFTER_HEIGHT_MED);

    iW = 0; iH = 0;
    shifter_screen_size(ST_RES_HIGH, &iW, &iH);
    TEST_ASSERT("[N] INT-VID-009: high res 640x400",
                iW == SHIFTER_WIDTH_HIGH && iH == SHIFTER_HEIGHT_HIGH);

    iW = 99; iH = 99;
    shifter_screen_size(0xFFu, &iW, &iH);
    TEST_ASSERT("[N] INT-VID-010: unknown res -> 0x0",
                iW == 0 && iH == 0);
}

/* ------------------------------------------------------------------
 * [N] Low resolution render
 * ------------------------------------------------------------------ */

static void test_low_res(void)
{
    st_machine_t tMachine;
    int          iW;
    int          iH;
    st_error_t   eRet;

    printf("\n--- [N] Low res render (320x200, 4 planes) ---\n");

    st_init(&tMachine, NULL);
    tMachine.uiScreenBase = 0x010000u;
    tMachine.uiResolution = ST_RES_LOW;
    /* palette[0]=black, palette[1]=white */
    tMachine.auPalette[0] = 0x0000u;
    tMachine.auPalette[1] = 0x0777u;

    /* Group 0: plane0=0xFFFF → all 16 pixels have bit0=1 → color index 1 */
    /* Planes 1-3 = 0x0000 → bits 1-3 = 0 → color index = 1 → white */
    tMachine.aRam[0x010000] = 0xFFu;
    tMachine.aRam[0x010001] = 0xFFu;
    /* bytes 2-7 already 0 from st_init() */

    memset(g_aPixels, 0, sizeof(g_aPixels));
    iW = 0; iH = 0;
    eRet = shifter_render(&tMachine, g_aPixels,
                          SHIFTER_MAX_PIXELS, &iW, &iH);

    TEST_ASSERT("[N] INT-VID-011: low res render -> ST_NO_ERROR",
                eRet == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-VID-012: low res pixel 0 = white (plane0=0xFFFF)",
                g_aPixels[0] == 0x00FFFFFFu);
    TEST_ASSERT("[N] INT-VID-013: low res pixel 15 = white",
                g_aPixels[15] == 0x00FFFFFFu);
    /* pixel 16 is first pixel of group 1; all planes = 0 → index 0 = black */
    TEST_ASSERT("[N] INT-VID-014: low res pixel 16 = black (group1 all-zero)",
                g_aPixels[16] == 0x00000000u);
    TEST_ASSERT("[N] INT-VID-015: low res width = 320",  iW == 320);
    TEST_ASSERT("[N] INT-VID-016: low res height = 200", iH == 200);

    /* plane1=0xFFFF → bit1 of color index set → index 2, palette[2]=0 */
    st_init(&tMachine, NULL);
    tMachine.uiScreenBase = 0x010000u;
    tMachine.uiResolution = ST_RES_LOW;
    tMachine.auPalette[0] = 0x0000u;
    tMachine.auPalette[2] = 0x0700u;  /* red for index 2 */
    tMachine.aRam[0x010002] = 0xFFu;  /* plane1 hi */
    tMachine.aRam[0x010003] = 0xFFu;  /* plane1 lo */

    memset(g_aPixels, 0, sizeof(g_aPixels));
    eRet = shifter_render(&tMachine, g_aPixels,
                          SHIFTER_MAX_PIXELS, &iW, &iH);
    TEST_ASSERT("[N] INT-VID-017: low res plane1=0xFFFF -> color index 2",
                eRet == ST_NO_ERROR
                && g_aPixels[0] == shifter_palette_to_rgb(0x0700u));

    /* all planes 0 → entire frame black */
    st_init(&tMachine, NULL);
    tMachine.uiScreenBase = 0x010000u;
    tMachine.uiResolution = ST_RES_LOW;
    tMachine.auPalette[0] = 0x0000u;

    memset(g_aPixels, 0xAAu, sizeof(g_aPixels));
    eRet = shifter_render(&tMachine, g_aPixels,
                          SHIFTER_MAX_PIXELS, &iW, &iH);
    TEST_ASSERT("[N] INT-VID-018: low res all-zero FB -> all pixels black",
                eRet == ST_NO_ERROR && g_aPixels[0] == 0x00000000u
                && g_aPixels[320 * 200 - 1] == 0x00000000u);

    st_shutdown(&tMachine);
}

/* ------------------------------------------------------------------
 * [N] Medium resolution render
 * ------------------------------------------------------------------ */

static void test_med_res(void)
{
    st_machine_t tMachine;
    int          iW;
    int          iH;
    st_error_t   eRet;

    printf("\n--- [N] Med res render (640x200, 2 planes) ---\n");

    st_init(&tMachine, NULL);
    tMachine.uiScreenBase = 0x010000u;
    tMachine.uiResolution = ST_RES_MED;
    tMachine.auPalette[0] = 0x0000u;  /* black */
    tMachine.auPalette[1] = 0x0700u;  /* red */

    /* Group 0: plane0=0xFFFF, plane1=0 → color index 1 (red) */
    tMachine.aRam[0x010000] = 0xFFu;
    tMachine.aRam[0x010001] = 0xFFu;

    memset(g_aPixels, 0, sizeof(g_aPixels));
    iW = 0; iH = 0;
    eRet = shifter_render(&tMachine, g_aPixels,
                          SHIFTER_MAX_PIXELS, &iW, &iH);

    TEST_ASSERT("[N] INT-VID-019: med res render -> ST_NO_ERROR",
                eRet == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-VID-020: med res pixel 0 = palette[1] (red)",
                g_aPixels[0] == shifter_palette_to_rgb(0x0700u));
    TEST_ASSERT("[N] INT-VID-021: med res width=640 height=200",
                iW == 640 && iH == 200);

    st_shutdown(&tMachine);
}

/* ------------------------------------------------------------------
 * [N] High resolution render
 * ------------------------------------------------------------------ */

static void test_high_res(void)
{
    st_machine_t tMachine;
    int          iW;
    int          iH;
    st_error_t   eRet;

    printf("\n--- [N] High res render (640x400, 1 plane) ---\n");

    st_init(&tMachine, NULL);
    tMachine.uiScreenBase = 0x010000u;
    tMachine.uiResolution = ST_RES_HIGH;
    tMachine.auPalette[0] = 0x0000u;  /* black */
    tMachine.auPalette[1] = 0x0777u;  /* white */

    /* First byte = 0x80: bit 7 = 1 -> pixel 0 = white; bit 6 = 0 -> pixel 1 = black */
    tMachine.aRam[0x010000] = 0x80u;

    memset(g_aPixels, 0, sizeof(g_aPixels));
    iW = 0; iH = 0;
    eRet = shifter_render(&tMachine, g_aPixels,
                          SHIFTER_MAX_PIXELS, &iW, &iH);

    TEST_ASSERT("[N] INT-VID-022: high res render -> ST_NO_ERROR",
                eRet == ST_NO_ERROR);
    TEST_ASSERT("[N] INT-VID-023: high res pixel 0 = white (bit=1)",
                g_aPixels[0] == 0x00FFFFFFu);
    TEST_ASSERT("[N] INT-VID-024: high res pixel 1 = black (bit=0)",
                g_aPixels[1] == 0x00000000u);
    TEST_ASSERT("[N] INT-VID-025: high res pixel 8 = black (next byte=0)",
                g_aPixels[8] == 0x00000000u);
    TEST_ASSERT("[N] INT-VID-026: high res width=640 height=400",
                iW == 640 && iH == 400);

    st_shutdown(&tMachine);
}

/* ------------------------------------------------------------------
 * [R] Robustness
 * ------------------------------------------------------------------ */

static void test_robustness(void)
{
    st_machine_t tMachine;
    int          iW;
    int          iH;
    st_error_t   eRet;

    printf("\n--- [R] Robustness ---\n");

    /* NULL ptMachine */
    iW = 0; iH = 0;
    eRet = shifter_render(NULL, g_aPixels, SHIFTER_MAX_PIXELS, &iW, &iH);
    TEST_ASSERT("[R] INT-VID-027: NULL ptMachine -> ST_ERROR",
                eRet == ST_ERROR);

    /* NULL auPixels */
    st_init(&tMachine, NULL);
    tMachine.uiScreenBase = 0x010000u;
    tMachine.uiResolution = ST_RES_LOW;
    iW = 0; iH = 0;
    eRet = shifter_render(&tMachine, NULL, 0, &iW, &iH);
    TEST_ASSERT("[R] INT-VID-028: NULL auPixels -> ST_ERROR",
                eRet == ST_ERROR);

    /* NULL piWidth */
    eRet = shifter_render(&tMachine, g_aPixels, SHIFTER_MAX_PIXELS,
                          NULL, &iH);
    TEST_ASSERT("[R] INT-VID-029: NULL piWidth -> ST_ERROR",
                eRet == ST_ERROR);

    /* NULL piHeight */
    eRet = shifter_render(&tMachine, g_aPixels, SHIFTER_MAX_PIXELS,
                          &iW, NULL);
    TEST_ASSERT("[R] INT-VID-030: NULL piHeight -> ST_ERROR",
                eRet == ST_ERROR);

    /* Buffer too small (need 320*200=64000, providing 1) */
    iW = 0; iH = 0;
    eRet = shifter_render(&tMachine, g_aPixels, 1u, &iW, &iH);
    TEST_ASSERT("[R] INT-VID-031: pixel buffer too small -> ST_ERROR",
                eRet == ST_ERROR);

    /* Screen base outside RAM (0x0F0000 = 960KB > ST_RAM_SIZE=512KB) */
    tMachine.uiScreenBase = 0x0F0000u;
    iW = 0; iH = 0;
    eRet = shifter_render(&tMachine, g_aPixels, SHIFTER_MAX_PIXELS,
                          &iW, &iH);
    TEST_ASSERT("[R] INT-VID-032: screen base out of RAM -> ST_ERROR",
                eRet == ST_ERROR);

    /* Unknown resolution */
    tMachine.uiScreenBase = 0x010000u;
    tMachine.uiResolution = 0xFFu;
    iW = 0; iH = 0;
    eRet = shifter_render(&tMachine, g_aPixels, SHIFTER_MAX_PIXELS,
                          &iW, &iH);
    TEST_ASSERT("[R] INT-VID-033: unknown resolution -> ST_ERROR",
                eRet == ST_ERROR);

    st_shutdown(&tMachine);
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    st_error_t eResult;

    printf("=== UC26: Shifter video rendering engine ===\n");

    eResult = trace_init(ST_FALSE);
    if (eResult != ST_NO_ERROR)
    {
        fprintf(stderr, "trace_init failed\n");
        return 1;
    }

    test_palette();
    test_screen_size();
    test_low_res();
    test_med_res();
    test_high_res();
    test_robustness();

    trace_shutdown();

    printf("\n=== UC26: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
