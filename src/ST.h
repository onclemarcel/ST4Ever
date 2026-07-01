/*
 * ST.h - ST4Ever Atari ST machine emulation core
 *
 * Models the Atari ST hardware memory map, hardware registers,
 * and provides read/write access to the emulated address space.
 *
 * Memory map (24-bit address space, 0x000000 - 0xFFFFFF):
 *
 *   0x000000 - 0x0007FF   Exception vectors (68000 vector table)
 *   0x000800 - 0x07FFFF   TOS / RAM  (varies by ST model)
 *   0x080000 - 0xEFFFFF   Cartridge / expansion (mostly unused)
 *   0xF00000 - 0xF3FFFF   Cartridge ROM
 *   0xF40000 - 0xF7FFFF   Reserved
 *   0xF80000 - 0xFBFFFF   TOS ROM (192KB on STF, 256KB on STE)
 *   0xFC0000 - 0xFDFFFF   TOS ROM (upper mirror / cartridge)
 *   0xFE0000 - 0xFEFFFF   Reserved
 *   0xFF8000 - 0xFF8FFF   Hardware registers:
 *     0xFF8200 - 0xFF827F    Video (Shifter): palette, resolution,
 *                             screen base, horizontal/vertical regs
 *     0xFF8800 - 0xFF8803    YM2149 (PSG) - sound + keyboard + ports
 *     0xFF8A00 - 0xFF8A3F    Blitter (STE only)
 *     0xFFFA00 - 0xFFFA3F    MFP 68901 - timers, UART, interrupts
 *     0xFFFC00 - 0xFFFC07    ACIA 6850 - keyboard + MIDI
 *   0xFFFF82 - 0xFFFFFF   Reserved / mirrors
 */

#ifndef ST_H
#define ST_H

#include "common.h"

/* ------------------------------------------------------------------
 * Memory constants
 * ------------------------------------------------------------------ */

#define ST_RAM_SIZE         (512 * 1024)    /* 512 KB standard RAM   */
#define ST_ROM_SIZE         (192 * 1024)    /* 192 KB TOS ROM        */
#define ST_ADDR_SPACE       (16 * 1024 * 1024) /* 24-bit = 16 MB     */

/* ------------------------------------------------------------------
 * Hardware register base addresses
 * ------------------------------------------------------------------ */

#define ST_REG_VIDEO_BASE   0xFF8200u   /* Shifter video registers   */
#define ST_REG_YM2149_BASE  0xFF8800u   /* PSG sound chip            */
#define ST_REG_MFP_BASE     0xFFFA00u   /* MFP 68901                 */
#define ST_REG_ACIA_KBD     0xFFFC00u   /* ACIA keyboard             */
#define ST_REG_ACIA_MIDI    0xFFFC04u   /* ACIA MIDI                 */

/* ------------------------------------------------------------------
 * Shifter (video) register offsets from ST_REG_VIDEO_BASE
 * ------------------------------------------------------------------ */

#define ST_VID_SCREEN_BASE_HI   0x01u   /* Screen base address hi    */
#define ST_VID_SCREEN_BASE_MI   0x03u   /* Screen base address mid   */
#define ST_VID_SCREEN_ADDR_HI   0x05u   /* Current display ptr hi    */
#define ST_VID_SCREEN_ADDR_MI   0x07u   /* Current display ptr mid   */
#define ST_VID_SCREEN_ADDR_LO   0x09u   /* Current display ptr lo    */
#define ST_VID_SYNC_MODE        0x0Au   /* Sync mode (50/60 Hz)      */
#define ST_VID_SCREEN_BASE_LO   0x0Du   /* Screen base address lo    */
#define ST_VID_LINE_WIDTH       0x0Fu   /* Scanline width            */
#define ST_VID_PALETTE          0x40u   /* 16x word palette (0x40-0x5F) */
#define ST_VID_RESOLUTION       0x60u   /* ST resolution register    */

/* Palette size */
#define ST_PALETTE_COLORS   16

/* Resolution values */
#define ST_RES_LOW          0   /* 320x200, 16 colours             */
#define ST_RES_MED          1   /* 640x200,  4 colours             */
#define ST_RES_HIGH         2   /* 640x400,  2 colours (mono)      */

/* ------------------------------------------------------------------
 * ROM constants
 * ------------------------------------------------------------------ */

/* ------------------------------------------------------------------
 * ST Machine context
 * ------------------------------------------------------------------ */

typedef struct st_machine_context_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */

    st_u8_t  aRam[ST_RAM_SIZE];         /* Main RAM (0x000000+)      */
    st_u8_t  aRom[ST_ROM_SIZE];         /* TOS ROM image             */
    st_bool_t bRomLoaded;               /* TOS ROM present           */

    /* Shifter raw register bytes (index = offset from 0xFF8200)     */
    st_u8_t  aShifterRegs[0x80];

    /* Derived Shifter state (kept in sync with aShifterRegs)        */
    st_u32_t uiScreenBase;              /* Video frame buffer addr   */
    st_u16_t auPalette[ST_PALETTE_COLORS]; /* ST colour palette      */
    st_u8_t  uiResolution;              /* ST_RES_LOW/MED/HIGH       */

    /* YM2149 (PSG) state                                            */
    st_u8_t  uiYmRegSel;               /* Currently selected reg (0-15) */
    st_u8_t  auYmRegs[16];             /* YM2149 internal registers  */

    /* MFP 68901 stub registers (0xFFFA00 + offset)                  */
    st_u8_t  aMfpRegs[0x40];

    /* ACIA 6850 stub registers (0xFFFC00 + offset, kbd+MIDI)        */
    st_u8_t  aAcia[8];

    /* Misc */
    st_bool_t bPoweredOn;               /* Machine is running        */
} st_machine_context_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * st_init() - Initialise and power on the emulated ST.
 *
 * Zeros RAM, loads TOS ROM if szRomPath is non-NULL, resets
 * hardware registers to power-on defaults.
 *
 * Parameters:
 *   ptMachine [out] : Machine state to initialise.
 *   szRomPath [in]  : Path to TOS ROM image file (may be NULL).
 *
 * Returns:
 *   Value of the global st_machine_context_t structure pointer on success.
*/
st_u64_t st_init(const char *szRomPath);

/*
 * st_read_byte() - Read one byte from the emulated address space.
 *
 * Parameters:
 *   ptMachine [in]  : Machine state.
 *   uiAddr    [in]  : 24-bit address.
 *   puiByte   [out] : Receives the read value.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on bus error.
 */
st_error_t st_read_byte( st_u32_t            uiAddr,
                          st_u8_t            *puiByte);

/*
 * st_read_word() - Read one big-endian word (16 bits) from the
 *                  emulated address space.
 *
 * Parameters:
 *   ptMachine [in]  : Machine state.
 *   uiAddr    [in]  : 24-bit address (must be even).
 *   puiWord   [out] : Receives the read value.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on bus/alignment error.
 */
st_error_t st_read_word( st_u32_t            uiAddr,
                          st_u16_t           *puiWord);

/*
 * st_read_long() - Read one big-endian longword (32 bits).
 *
 * Parameters:
 *   ptMachine [in]  : Machine state.
 *   uiAddr    [in]  : 24-bit address (must be even).
 *   puiLong   [out] : Receives the read value.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on bus/alignment error.
 */
st_error_t st_read_long( st_u32_t            uiAddr,
                          st_u32_t           *puiLong);

/*
 * st_write_byte() - Write one byte to the emulated address space.
 *
 * Parameters:
 *   ptMachine [in/out] : Machine state.
 *   uiAddr    [in]     : 24-bit address.
 *   uiByte    [in]     : Value to write.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on bus error.
 */
st_error_t st_write_byte( st_u32_t      uiAddr,
                           st_u8_t       uiByte);

/*
 * st_write_word() - Write one big-endian word.
 *
 * Parameters:
 *   ptMachine [in/out] : Machine state.
 *   uiAddr    [in]     : 24-bit address (must be even).
 *   uiWord    [in]     : Value to write.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on bus/alignment error.
 */
st_error_t st_write_word( st_u32_t      uiAddr,
                           st_u16_t      uiWord);

/*
 * st_write_long() - Write one big-endian longword.
 *
 * Parameters:
 *   ptMachine [in/out] : Machine state.
 *   uiAddr    [in]     : 24-bit address (must be even).
 *   uiLong    [in]     : Value to write.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on bus/alignment error.
 */
st_error_t st_write_long(  st_u32_t      uiAddr,
                           st_u32_t      uiLong);

/*
 * st_shutdown() - Release machine resources.
 *
 * Parameters:
 *   ptMachine [in/out] : Machine state to reset.
 *
 * Returns: ST_NO_ERROR.
 */
st_error_t st_shutdown();

st_u16_t st_get_palette(st_u16_t uiColIdx);
st_u32_t st_get_screen_base();
st_u32_t st_get_resolution();
void*    st_get_ram_pointer(st_u32_t uiAddr);

#endif /* ST_H */
