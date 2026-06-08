/*
 * ST.c - Atari ST machine emulation core (UC24)
 *
 * UC24: Hardware register read/write dispatch
 *   - Shifter (0xFF8200-0xFF827F): screen base, palette, resolution,
 *     sync mode.
 *   - YM2149/PSG (0xFF8800-0xFF8803): register latch + data bus.
 *   - MFP 68901 (0xFFFA00-0xFFFA3F): stub register storage.
 *   - ACIA 6850 (0xFFFC00-0xFFFC07): stub register storage.
 *   - ROM area (0xF80000-0xFDFFFF): read TOS if loaded, else 0xFF;
 *     write -> bus error (ST_ERROR).
 *   - Unmapped areas: read -> 0xFF open-bus; write -> silently ignored.
 *
 * Memory access functions (st_read/write_byte/word/long) are called on
 * every CPU memory access and must NOT carry LOG_TRACE (R22).
 */
#include "ST.h"
#include "trace.h"
#include <string.h>

/* ------------------------------------------------------------------
 * Address range constants (internal)
 * ------------------------------------------------------------------ */

#define ST_ADDR_SHIFTER_LO  0xFF8200u
#define ST_ADDR_SHIFTER_HI  0xFF827Fu
#define ST_ADDR_YM_LO       0xFF8800u
#define ST_ADDR_YM_HI       0xFF8803u
#define ST_ADDR_MFP_LO      0xFFFA00u
#define ST_ADDR_MFP_HI      0xFFFA3Fu
#define ST_ADDR_ACIA_LO     0xFFFC00u
#define ST_ADDR_ACIA_HI     0xFFFC07u
#define ST_ADDR_ROM_LO      0xF80000u
#define ST_ADDR_ROM_HI      0xFDFFFFu

/* ------------------------------------------------------------------
 * Internal: Shifter helpers
 * ------------------------------------------------------------------ */

static void st_rebuild_screen_base(st_machine_t *ptMachine)
{
    ptMachine->uiScreenBase =
          ((st_u32_t)ptMachine->aShifterRegs[ST_VID_SCREEN_BASE_HI] << 16)
        | ((st_u32_t)ptMachine->aShifterRegs[ST_VID_SCREEN_BASE_MI] <<  8)
        |  (st_u32_t)ptMachine->aShifterRegs[ST_VID_SCREEN_BASE_LO];
}

static void shifter_write_byte(st_machine_t *ptMachine,
                                st_u32_t      uiOffset,
                                st_u8_t       uiByte)
{
    st_u32_t uiPalOff;
    st_u32_t uiPalIdx;

    if (uiOffset >= 0x80u) { return; }
    ptMachine->aShifterRegs[uiOffset] = uiByte;

    if (uiOffset == ST_VID_SCREEN_BASE_HI
        || uiOffset == ST_VID_SCREEN_BASE_MI
        || uiOffset == ST_VID_SCREEN_BASE_LO)
    {
        st_rebuild_screen_base(ptMachine);
    }
    else if (uiOffset == ST_VID_RESOLUTION)
    {
        ptMachine->uiResolution = uiByte & 0x03u;
    }
    else if (uiOffset >= ST_VID_PALETTE
             && uiOffset < ST_VID_PALETTE + (st_u32_t)(ST_PALETTE_COLORS * 2))
    {
        uiPalOff = uiOffset - ST_VID_PALETTE;
        uiPalIdx = uiPalOff >> 1u;
        if (uiPalOff & 1u)
            ptMachine->auPalette[uiPalIdx] =
                (ptMachine->auPalette[uiPalIdx] & 0xFF00u)
                | (st_u16_t)uiByte;
        else
            ptMachine->auPalette[uiPalIdx] =
                (ptMachine->auPalette[uiPalIdx] & 0x00FFu)
                | (st_u16_t)((st_u16_t)uiByte << 8u);
    }
}

static void shifter_read_byte(const st_machine_t *ptMachine,
                               st_u32_t            uiOffset,
                               st_u8_t            *puiByte)
{
    *puiByte = (uiOffset < 0x80u)
               ? ptMachine->aShifterRegs[uiOffset] : 0xFFu;
}

/* ------------------------------------------------------------------
 * Internal: YM2149 (PSG) helpers
 * ------------------------------------------------------------------ */

static void ym2149_write_byte(st_machine_t *ptMachine,
                               st_u32_t      uiOffset,
                               st_u8_t       uiByte)
{
    switch (uiOffset)
    {
        case 0x00u: /* 0xFF8800: register select latch */
            ptMachine->uiYmRegSel = uiByte & 0x0Fu;
            break;
        case 0x02u: /* 0xFF8802: data write */
            ptMachine->auYmRegs[ptMachine->uiYmRegSel] = uiByte;
            break;
        default:
            break;
    }
}

static void ym2149_read_byte(const st_machine_t *ptMachine,
                              st_u32_t            uiOffset,
                              st_u8_t            *puiByte)
{
    /* 0xFF8800: read data from currently selected register */
    *puiByte = (uiOffset == 0x00u)
               ? ptMachine->auYmRegs[ptMachine->uiYmRegSel] : 0xFFu;
}

/* ------------------------------------------------------------------
 * Internal: MFP 68901 stubs
 * ------------------------------------------------------------------ */

static void mfp_write_byte(st_machine_t *ptMachine,
                            st_u32_t      uiOffset,
                            st_u8_t       uiByte)
{
    if (uiOffset < 0x40u)
        ptMachine->aMfpRegs[uiOffset] = uiByte;
}

static void mfp_read_byte(const st_machine_t *ptMachine,
                           st_u32_t            uiOffset,
                           st_u8_t            *puiByte)
{
    *puiByte = (uiOffset < 0x40u) ? ptMachine->aMfpRegs[uiOffset] : 0xFFu;
}

/* ------------------------------------------------------------------
 * Internal: ACIA 6850 stubs
 * ------------------------------------------------------------------ */

static void acia_write_byte(st_machine_t *ptMachine,
                             st_u32_t      uiOffset,
                             st_u8_t       uiByte)
{
    if (uiOffset < 8u)
        ptMachine->aAcia[uiOffset] = uiByte;
}

static void acia_read_byte(const st_machine_t *ptMachine,
                            st_u32_t            uiOffset,
                            st_u8_t            *puiByte)
{
    *puiByte = (uiOffset < 8u) ? ptMachine->aAcia[uiOffset] : 0xFFu;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_error_t st_init(st_machine_t *ptMachine, const char *szRomPath)
{
    LOG_TRACE("ptMachine=%p szRomPath='%s'",
              (void *)ptMachine, szRomPath ? szRomPath : "(none)");
    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    memset(ptMachine, 0, sizeof(st_machine_t));
    ptMachine->uiResolution = ST_RES_LOW;
    ptMachine->bPoweredOn   = ST_TRUE;
    /* Sync mode: 50 Hz (European default) */
    ptMachine->aShifterRegs[ST_VID_SYNC_MODE] = 0x02u;
    ST_UNUSED(szRomPath);
    LOG_TODO("st_init: TOS ROM loading deferred to UC25 "
             "(szRomPath='%s')", szRomPath ? szRomPath : "(none)");
    LOG_INFO("ST machine initialised (RAM=%uKB)",
             (unsigned)(ST_RAM_SIZE / 1024));
    return ST_NO_ERROR;
}

st_error_t st_read_byte(const st_machine_t *ptMachine,
                          st_u32_t uiAddr, st_u8_t *puiByte)
{
    st_u32_t uiA;
    st_u32_t uiRomOff;

    if (ptMachine == NULL || puiByte == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    uiA = uiAddr & 0xFFFFFFu;

    if (uiA < ST_RAM_SIZE)
    {
        *puiByte = ptMachine->aRam[uiA];
    }
    else if (uiA >= ST_ADDR_ROM_LO && uiA <= ST_ADDR_ROM_HI)
    {
        uiRomOff = uiA - ST_ADDR_ROM_LO;
        *puiByte = (ptMachine->bRomLoaded && uiRomOff < (st_u32_t)ST_ROM_SIZE)
                   ? ptMachine->aRom[uiRomOff] : 0xFFu;
    }
    else if (uiA >= ST_ADDR_SHIFTER_LO && uiA <= ST_ADDR_SHIFTER_HI)
    {
        shifter_read_byte(ptMachine, uiA - ST_ADDR_SHIFTER_LO, puiByte);
    }
    else if (uiA >= ST_ADDR_YM_LO && uiA <= ST_ADDR_YM_HI)
    {
        ym2149_read_byte(ptMachine, uiA - ST_ADDR_YM_LO, puiByte);
    }
    else if (uiA >= ST_ADDR_MFP_LO && uiA <= ST_ADDR_MFP_HI)
    {
        mfp_read_byte(ptMachine, uiA - ST_ADDR_MFP_LO, puiByte);
    }
    else if (uiA >= ST_ADDR_ACIA_LO && uiA <= ST_ADDR_ACIA_HI)
    {
        acia_read_byte(ptMachine, uiA - ST_ADDR_ACIA_LO, puiByte);
    }
    else
    {
        *puiByte = 0xFFu;   /* open bus */
    }
    return ST_NO_ERROR;
}

st_error_t st_read_word(const st_machine_t *ptMachine,
                          st_u32_t uiAddr, st_u16_t *puiWord)
{
    st_u8_t uiHi;
    st_u8_t uiLo;
    st_error_t eR;

    if (ptMachine == NULL || puiWord == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    if (uiAddr & 1u)
    {
        LOG_ERROR("unaligned word read at 0x%06X", uiAddr);
        return ST_ERROR;
    }
    eR = st_read_byte(ptMachine, uiAddr,     &uiHi);
    if (eR != ST_NO_ERROR) { return eR; }
    eR = st_read_byte(ptMachine, uiAddr + 1, &uiLo);
    if (eR != ST_NO_ERROR) { return eR; }
    *puiWord = (st_u16_t)(((st_u16_t)uiHi << 8) | uiLo);
    return ST_NO_ERROR;
}

st_error_t st_read_long(const st_machine_t *ptMachine,
                          st_u32_t uiAddr, st_u32_t *puiLong)
{
    st_u16_t uiHi;
    st_u16_t uiLo;
    st_error_t eR;

    if (ptMachine == NULL || puiLong == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    eR = st_read_word(ptMachine, uiAddr,     &uiHi);
    if (eR != ST_NO_ERROR) { return eR; }
    eR = st_read_word(ptMachine, uiAddr + 2, &uiLo);
    if (eR != ST_NO_ERROR) { return eR; }
    *puiLong = ((st_u32_t)uiHi << 16) | uiLo;
    return ST_NO_ERROR;
}

st_error_t st_write_byte(st_machine_t *ptMachine,
                           st_u32_t uiAddr, st_u8_t uiByte)
{
    st_u32_t uiA;

    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    uiA = uiAddr & 0xFFFFFFu;

    if (uiA < ST_RAM_SIZE)
    {
        ptMachine->aRam[uiA] = uiByte;
    }
    else if (uiA >= ST_ADDR_ROM_LO && uiA <= ST_ADDR_ROM_HI)
    {
        LOG_ERROR("ROM write: addr=0x%06X val=0x%02X", uiA, uiByte);
        return ST_ERROR;    /* bus error — ROM is read-only */
    }
    else if (uiA >= ST_ADDR_SHIFTER_LO && uiA <= ST_ADDR_SHIFTER_HI)
    {
        shifter_write_byte(ptMachine, uiA - ST_ADDR_SHIFTER_LO, uiByte);
    }
    else if (uiA >= ST_ADDR_YM_LO && uiA <= ST_ADDR_YM_HI)
    {
        ym2149_write_byte(ptMachine, uiA - ST_ADDR_YM_LO, uiByte);
    }
    else if (uiA >= ST_ADDR_MFP_LO && uiA <= ST_ADDR_MFP_HI)
    {
        mfp_write_byte(ptMachine, uiA - ST_ADDR_MFP_LO, uiByte);
    }
    else if (uiA >= ST_ADDR_ACIA_LO && uiA <= ST_ADDR_ACIA_HI)
    {
        acia_write_byte(ptMachine, uiA - ST_ADDR_ACIA_LO, uiByte);
    }
    /* else: unmapped — silently ignored (open bus) */
    return ST_NO_ERROR;
}

st_error_t st_write_word(st_machine_t *ptMachine,
                           st_u32_t uiAddr, st_u16_t uiWord)
{
    st_error_t eR;

    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    if (uiAddr & 1u)
    {
        LOG_ERROR("unaligned word write at 0x%06X", uiAddr);
        return ST_ERROR;
    }
    eR = st_write_byte(ptMachine, uiAddr,     (st_u8_t)(uiWord >> 8));
    if (eR != ST_NO_ERROR) { return eR; }
    eR = st_write_byte(ptMachine, uiAddr + 1, (st_u8_t)(uiWord & 0xFFu));
    return eR;
}

st_error_t st_write_long(st_machine_t *ptMachine,
                           st_u32_t uiAddr, st_u32_t uiLong)
{
    st_error_t eR;

    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    eR = st_write_word(ptMachine, uiAddr,     (st_u16_t)(uiLong >> 16));
    if (eR != ST_NO_ERROR) { return eR; }
    eR = st_write_word(ptMachine, uiAddr + 2, (st_u16_t)(uiLong & 0xFFFFu));
    return eR;
}

st_error_t st_shutdown(st_machine_t *ptMachine)
{
    LOG_TRACE("ptMachine=%p", (void *)ptMachine);
    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    memset(ptMachine, 0, sizeof(st_machine_t));
    LOG_INFO("ST machine shut down");
    return ST_NO_ERROR;
}
