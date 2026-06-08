/*
 * use_case_24.c - UC24 : Atari ST memory map + HW register stubs
 *
 * TEST MATRIX - UC24:
 *   [N] Nominal    : 51 tests - init/shutdown + sync mode default, RAM r/w
 *                               (byte/word/long + addr 0), Shifter screen base
 *                               (hi/mi/lo) + palette (word, 15, all-16) +
 *                               resolution MED/HIGH + sync mode, YM2149
 *                               (reg0 w+r, reg7, all-16, mask 4-bit), MFP
 *                               (IER/timer/last), ACIA (kbd/MIDI/last), ROM
 *                               read (no ROM)=0xFF, ROM write=ST_ERROR,
 *                               unmapped read=0xFF + write no-crash.
 *   [R] Robustness : 10 tests - NULL machine/output, unaligned word r/w,
 *                               32-bit addr masked to 24 bits.
 *   [S] Skipped    :  0 tests
 */
#include "use_cases.h"

/* INTENT[INT-STM-013 → TC-STM-013..030 → REQ-STM-011,REQ-STM-015..023
 *        → UFR-EXE-009]:
 * The ST memory map dispatcher must route any 24-bit address to the correct
 * handler (RAM / ROM / Shifter / YM2149 / MFP / ACIA / open-bus) without
 * crashing.  Shifter writes must maintain aShifterRegs[] and the derived
 * fields (uiScreenBase, auPalette[], uiResolution) in sync.  YM2149 uses
 * a two-step select+data protocol.  ROM writes return ST_ERROR.  Unmapped
 * reads return 0xFF; unmapped writes are silently ignored.  All 32-bit
 * addresses are masked to 24 bits before dispatch. */

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static st_machine_t g_tMachine;

static void setup(void)
{
    st_init(&g_tMachine, NULL);
}

static void teardown(void)
{
    st_shutdown(&g_tMachine);
}

static st_u8_t rb(st_u32_t uiAddr)
{
    st_u8_t uiByte = 0;
    st_read_byte(&g_tMachine, uiAddr, &uiByte);
    return uiByte;
}

static st_u16_t rw(st_u32_t uiAddr)
{
    st_u16_t uiWord = 0;
    st_read_word(&g_tMachine, uiAddr, &uiWord);
    return uiWord;
}

/* ------------------------------------------------------------------ */
/* Init / shutdown                                                       */
/* ------------------------------------------------------------------ */

static void test_init_shutdown(void)
{
    st_machine_t tM;
    printf("\n--- init / shutdown ---\n");

    TEST_ASSERT("[N] st_init NULL", st_init(NULL, NULL) == ST_ERROR);
    TEST_ASSERT("[N] st_init OK",   st_init(&tM, NULL)  == ST_NO_ERROR);
    TEST_ASSERT("[N] bPoweredOn after init", tM.bPoweredOn == ST_TRUE);
    TEST_ASSERT("[N] resolution LOW after init",
                tM.uiResolution == ST_RES_LOW);
    TEST_ASSERT("[N] sync mode 50Hz after init",
                tM.aShifterRegs[ST_VID_SYNC_MODE] == 0x02u);
    TEST_ASSERT("[N] st_shutdown NULL", st_shutdown(NULL) == ST_ERROR);
    TEST_ASSERT("[N] st_shutdown OK",   st_shutdown(&tM)  == ST_NO_ERROR);
    TEST_ASSERT("[N] bPoweredOn cleared after shutdown",
                tM.bPoweredOn == ST_FALSE);
}

/* ------------------------------------------------------------------ */
/* RAM read / write                                                      */
/* ------------------------------------------------------------------ */

static void test_ram_rw(void)
{
    printf("\n--- RAM read/write ---\n");
    setup();

    TEST_ASSERT("[N] RAM write byte OK",
                st_write_byte(&g_tMachine, 0x001000u, 0xABu) == ST_NO_ERROR);
    TEST_ASSERT("[N] RAM read byte matches",
                rb(0x001000u) == 0xABu);

    TEST_ASSERT("[N] RAM write word OK",
                st_write_word(&g_tMachine, 0x002000u, 0x1234u) == ST_NO_ERROR);
    TEST_ASSERT("[N] RAM read word matches",
                rw(0x002000u) == 0x1234u);

    TEST_ASSERT("[N] RAM write long OK",
                st_write_long(&g_tMachine, 0x003000u, 0xDEADBEEFu)
                == ST_NO_ERROR);
    {
        st_u32_t uiL = 0;
        st_read_long(&g_tMachine, 0x003000u, &uiL);
        TEST_ASSERT("[N] RAM read long matches", uiL == 0xDEADBEEFu);
    }

    /* Vector table area (address 0) is also RAM */
    TEST_ASSERT("[N] RAM write at addr 0 OK",
                st_write_byte(&g_tMachine, 0x000000u, 0x55u) == ST_NO_ERROR);
    TEST_ASSERT("[N] RAM read at addr 0 matches", rb(0x000000u) == 0x55u);

    teardown();
}

/* ------------------------------------------------------------------ */
/* Shifter — screen base                                                 */
/* ------------------------------------------------------------------ */

static void test_shifter_screen_base(void)
{
    printf("\n--- Shifter screen base ---\n");
    setup();

    /* Write hi=0x07, mi=0xB2 → base = 0x07B200 */
    st_write_byte(&g_tMachine, 0xFF8201u, 0x07u);  /* hi */
    st_write_byte(&g_tMachine, 0xFF8203u, 0xB2u);  /* mi */
    TEST_ASSERT("[N] uiScreenBase from hi+mi",
                g_tMachine.uiScreenBase == 0x07B200u);

    /* Read back through st_read_byte */
    TEST_ASSERT("[N] Shifter base hi read back", rb(0xFF8201u) == 0x07u);
    TEST_ASSERT("[N] Shifter base mi read back", rb(0xFF8203u) == 0xB2u);

    /* Write lo (STE field — stored, used in reconstruction) */
    st_write_byte(&g_tMachine, 0xFF820Du, 0x04u);
    TEST_ASSERT("[N] uiScreenBase from hi+mi+lo",
                g_tMachine.uiScreenBase == 0x07B204u);

    teardown();
}

/* ------------------------------------------------------------------ */
/* Shifter — palette                                                     */
/* ------------------------------------------------------------------ */

static void test_shifter_palette(void)
{
    st_u32_t i;
    printf("\n--- Shifter palette ---\n");
    setup();

    /* Write color 0 = 0x0777 (white in 3-bit ST format) via word */
    st_write_word(&g_tMachine, 0xFF8240u, 0x0777u);
    TEST_ASSERT("[N] palette[0] word write", g_tMachine.auPalette[0] == 0x0777u);
    TEST_ASSERT("[N] palette[0] read hi byte", rb(0xFF8240u) == 0x07u);
    TEST_ASSERT("[N] palette[0] read lo byte", rb(0xFF8241u) == 0x77u);

    /* Write color 15 (last entry) = 0x0ABC */
    st_write_word(&g_tMachine, 0xFF825Eu, 0x0ABCu);
    TEST_ASSERT("[N] palette[15] word write",
                g_tMachine.auPalette[15] == 0x0ABCu);

    /* Fill all 16 colors and verify */
    for (i = 0; i < 16u; i++)
    {
        st_write_word(&g_tMachine,
                      0xFF8240u + i * 2u,
                      (st_u16_t)(0x0100u + (st_u16_t)i));
    }
    TEST_ASSERT("[N] all 16 palette colors written",
                g_tMachine.auPalette[0] == 0x0100u
                && g_tMachine.auPalette[7]  == 0x0107u
                && g_tMachine.auPalette[15] == 0x010Fu);

    teardown();
}

/* ------------------------------------------------------------------ */
/* Shifter — resolution and sync mode                                    */
/* ------------------------------------------------------------------ */

static void test_shifter_resolution(void)
{
    printf("\n--- Shifter resolution + sync mode ---\n");
    setup();

    st_write_byte(&g_tMachine, 0xFF8260u, ST_RES_MED);
    TEST_ASSERT("[N] resolution MED write", g_tMachine.uiResolution == ST_RES_MED);
    TEST_ASSERT("[N] resolution MED read",  rb(0xFF8260u) == ST_RES_MED);

    st_write_byte(&g_tMachine, 0xFF8260u, ST_RES_HIGH);
    TEST_ASSERT("[N] resolution HIGH write", g_tMachine.uiResolution == ST_RES_HIGH);

    st_write_byte(&g_tMachine, 0xFF820Au, 0x02u);  /* 50 Hz */
    TEST_ASSERT("[N] sync mode write", rb(0xFF820Au) == 0x02u);

    teardown();
}

/* ------------------------------------------------------------------ */
/* YM2149 (PSG)                                                          */
/* ------------------------------------------------------------------ */

static void test_ym2149(void)
{
    st_u32_t i;
    printf("\n--- YM2149 ---\n");
    setup();

    /* Select register 0 and write 0xAA */
    st_write_byte(&g_tMachine, 0xFF8800u, 0x00u);   /* select reg 0  */
    st_write_byte(&g_tMachine, 0xFF8802u, 0xAAu);   /* write data    */
    TEST_ASSERT("[N] YM reg 0 written",
                g_tMachine.auYmRegs[0] == 0xAAu);
    TEST_ASSERT("[N] YM reg 0 read via 0xFF8800",
                rb(0xFF8800u) == 0xAAu);

    /* Select register 7 (I/O port) and write 0x3F */
    st_write_byte(&g_tMachine, 0xFF8800u, 0x07u);
    st_write_byte(&g_tMachine, 0xFF8802u, 0x3Fu);
    TEST_ASSERT("[N] YM reg 7 written",  g_tMachine.auYmRegs[7] == 0x3Fu);
    TEST_ASSERT("[N] YM regSel updated", g_tMachine.uiYmRegSel  == 0x07u);

    /* Write all 16 registers */
    for (i = 0; i < 16u; i++)
    {
        st_write_byte(&g_tMachine, 0xFF8800u, (st_u8_t)i);
        st_write_byte(&g_tMachine, 0xFF8802u, (st_u8_t)(0x10u + i));
    }
    TEST_ASSERT("[N] YM all 16 regs written",
                g_tMachine.auYmRegs[0] == 0x10u
                && g_tMachine.auYmRegs[15] == 0x1Fu);

    /* Reg select high nibble masked to 0x0F */
    st_write_byte(&g_tMachine, 0xFF8800u, 0xF3u);
    TEST_ASSERT("[N] YM regSel masks to 4 bits", g_tMachine.uiYmRegSel == 0x03u);

    teardown();
}

/* ------------------------------------------------------------------ */
/* MFP 68901 stubs                                                       */
/* ------------------------------------------------------------------ */

static void test_mfp(void)
{
    printf("\n--- MFP 68901 stubs ---\n");
    setup();

    /* Write to MFP interrupt enable register (offset 0x07 = 0xFFFA07) */
    st_write_byte(&g_tMachine, 0xFFFA07u, 0x40u);
    TEST_ASSERT("[N] MFP IER write", g_tMachine.aMfpRegs[0x07u] == 0x40u);
    TEST_ASSERT("[N] MFP IER read",  rb(0xFFFA07u) == 0x40u);

    /* Write to MFP timer A (offset 0x19) */
    st_write_byte(&g_tMachine, 0xFFFA19u, 0x07u);
    TEST_ASSERT("[N] MFP timer write", rb(0xFFFA19u) == 0x07u);

    /* Boundary: last MFP byte at 0xFFFA3F */
    st_write_byte(&g_tMachine, 0xFFFA3Fu, 0xFFu);
    TEST_ASSERT("[N] MFP last byte write", rb(0xFFFA3Fu) == 0xFFu);

    teardown();
}

/* ------------------------------------------------------------------ */
/* ACIA 6850 stubs                                                        */
/* ------------------------------------------------------------------ */

static void test_acia(void)
{
    printf("\n--- ACIA 6850 stubs ---\n");
    setup();

    /* Keyboard ACIA control (0xFFFC00) */
    st_write_byte(&g_tMachine, 0xFFFC00u, 0x03u);  /* master reset */
    TEST_ASSERT("[N] ACIA kbd ctrl write", g_tMachine.aAcia[0] == 0x03u);
    TEST_ASSERT("[N] ACIA kbd ctrl read",  rb(0xFFFC00u) == 0x03u);

    /* MIDI ACIA (0xFFFC04) */
    st_write_byte(&g_tMachine, 0xFFFC04u, 0x95u);
    TEST_ASSERT("[N] ACIA MIDI write", rb(0xFFFC04u) == 0x95u);

    /* Last ACIA byte (0xFFFC07) */
    st_write_byte(&g_tMachine, 0xFFFC07u, 0x42u);
    TEST_ASSERT("[N] ACIA last byte", rb(0xFFFC07u) == 0x42u);

    teardown();
}

/* ------------------------------------------------------------------ */
/* ROM area                                                               */
/* ------------------------------------------------------------------ */

static void test_rom_area(void)
{
    printf("\n--- ROM area ---\n");
    setup();

    /* No ROM loaded: reads return 0xFF */
    TEST_ASSERT("[N] ROM read (no ROM) → 0xFF",
                rb(0xF80000u) == 0xFFu);
    TEST_ASSERT("[N] ROM read mid (no ROM) → 0xFF",
                rb(0xFA0000u) == 0xFFu);

    /* ROM write always → ST_ERROR (bus error) */
    TEST_ASSERT("[N] ROM write → ST_ERROR",
                st_write_byte(&g_tMachine, 0xF80000u, 0x00u) == ST_ERROR);
    TEST_ASSERT("[N] ROM write word → ST_ERROR",
                st_write_word(&g_tMachine, 0xFA0000u, 0x1234u) == ST_ERROR);

    teardown();
}

/* ------------------------------------------------------------------ */
/* Unmapped areas                                                         */
/* ------------------------------------------------------------------ */

static void test_unmapped(void)
{
    printf("\n--- Unmapped areas ---\n");
    setup();

    /* 0x080000: cartridge/expansion — returns 0xFF, no crash */
    TEST_ASSERT("[N] unmapped read 0x080000 → 0xFF", rb(0x080000u) == 0xFFu);
    TEST_ASSERT("[N] unmapped write 0x080000 OK",
                st_write_byte(&g_tMachine, 0x080000u, 0xAAu) == ST_NO_ERROR);

    /* 0xFE0000: reserved — same */
    TEST_ASSERT("[N] unmapped read 0xFE0000 → 0xFF", rb(0xFE0000u) == 0xFFu);
    TEST_ASSERT("[N] unmapped write 0xFE0000 OK",
                st_write_byte(&g_tMachine, 0xFE0000u, 0x00u) == ST_NO_ERROR);

    teardown();
}

/* ------------------------------------------------------------------ */
/* Robustness                                                             */
/* ------------------------------------------------------------------ */

static void test_robustness(void)
{
    st_u8_t  uiByte  = 0;
    st_u16_t uiWord  = 0;
    st_u32_t uiLong  = 0;

    printf("\n--- Robustness ---\n");
    setup();

    /* NULL machine */
    TEST_ASSERT("[R] read_byte NULL machine",
                st_read_byte(NULL, 0x001000u, &uiByte) == ST_ERROR);
    TEST_ASSERT("[R] write_byte NULL machine",
                st_write_byte(NULL, 0x001000u, 0x00u) == ST_ERROR);
    TEST_ASSERT("[R] read_word NULL machine",
                st_read_word(NULL, 0x001000u, &uiWord) == ST_ERROR);
    TEST_ASSERT("[R] write_word NULL machine",
                st_write_word(NULL, 0x001000u, 0x0000u) == ST_ERROR);

    /* NULL output pointer */
    TEST_ASSERT("[R] read_byte NULL puiByte",
                st_read_byte(&g_tMachine, 0x001000u, NULL) == ST_ERROR);
    TEST_ASSERT("[R] read_word NULL puiWord",
                st_read_word(&g_tMachine, 0x001000u, NULL) == ST_ERROR);
    TEST_ASSERT("[R] read_long NULL puiLong",
                st_read_long(&g_tMachine, 0x001000u, NULL) == ST_ERROR);

    /* Unaligned word access */
    TEST_ASSERT("[R] read_word odd addr → ST_ERROR",
                st_read_word(&g_tMachine, 0x001001u, &uiWord) == ST_ERROR);
    TEST_ASSERT("[R] write_word odd addr → ST_ERROR",
                st_write_word(&g_tMachine, 0x001001u, 0x0000u) == ST_ERROR);

    /* 32-bit address masked to 24 bits (bit 24 set → wraps to 0x001000) */
    st_write_byte(&g_tMachine, 0x001000u, 0x77u);
    {
        st_u8_t uiMasked = 0;
        st_read_byte(&g_tMachine, 0x01001000u, &uiMasked);
        TEST_ASSERT("[R] 32-bit addr masked to 24 bits", uiMasked == 0x77u);
    }

    ST_UNUSED(uiLong);
    teardown();
}

/* ------------------------------------------------------------------ */
/* main                                                                   */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC24: ST memory map + HW register stubs ===\n");

    test_init_shutdown();
    test_ram_rw();
    test_shifter_screen_base();
    test_shifter_palette();
    test_shifter_resolution();
    test_ym2149();
    test_mfp();
    test_acia();
    test_rom_area();
    test_unmapped();
    test_robustness();

    printf("\n=== UC24 result: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
