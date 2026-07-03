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
 *   - Unmapped areas: read -> 0xFF open-bus; write -> bus error (ST_ERROR).
 *   - st_write_word()/st_write_long() validate the full byte span
 *     up-front (st_check_write_range()) so a rejected write never
 *     partially mutates memory (all-or-nothing).
 *
 * Memory access functions (st_read/write_byte/word/long) are called on
 * every CPU memory access and must NOT carry LOG_TRACE (R22).
 */
#include "ST.h"
#include "trace.h"
#include <string.h>

static st_machine_context_t g_st_machine_ptCtx = {.ulMagic = 0xCAFEDECA,
                                                  .eObject = ST_MACHINE_CTX};

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
#define ST_ADDR_ROM_LO      0xFC0000u
#define ST_ADDR_ROM_HI      0xFEFFFFu

/* ------------------------------------------------------------------
 * Internal: Shifter helpers
 * ------------------------------------------------------------------ */

static void st_rebuild_screen_base()
{
    g_st_machine_ptCtx.uiScreenBase =
        ((st_u32_t)g_st_machine_ptCtx.aShifterRegs[ST_VID_SCREEN_BASE_HI] << 16)
        | ((st_u32_t)g_st_machine_ptCtx.aShifterRegs[ST_VID_SCREEN_BASE_MI] <<  8)
        |  (st_u32_t)g_st_machine_ptCtx.aShifterRegs[ST_VID_SCREEN_BASE_LO];
}

static void shifter_write_byte(st_u32_t      uiOffset,
                               st_u8_t       uiByte)
{
    st_u32_t uiPalOff;
    st_u32_t uiPalIdx;

    if (uiOffset >= 0x80u) { return; }
    g_st_machine_ptCtx.aShifterRegs[uiOffset] = uiByte;

    if (uiOffset == ST_VID_SCREEN_BASE_HI
        || uiOffset == ST_VID_SCREEN_BASE_MI
        || uiOffset == ST_VID_SCREEN_BASE_LO)
    {
        st_rebuild_screen_base();
    }
    else if (uiOffset == ST_VID_RESOLUTION)
    {
        g_st_machine_ptCtx.uiResolution = uiByte & 0x03u;
    }
    else if (uiOffset >= ST_VID_PALETTE
             && uiOffset < ST_VID_PALETTE + (st_u32_t)(ST_PALETTE_COLORS * 2))
    {
        uiPalOff = uiOffset - ST_VID_PALETTE;
        uiPalIdx = uiPalOff >> 1u;
        if (uiPalOff & 1u)
            g_st_machine_ptCtx.auPalette[uiPalIdx] =
                (g_st_machine_ptCtx.auPalette[uiPalIdx] & 0xFF00u)
                | (st_u16_t)uiByte;
        else
            g_st_machine_ptCtx.auPalette[uiPalIdx] =
                (g_st_machine_ptCtx.auPalette[uiPalIdx] & 0x00FFu)
                | (st_u16_t)((st_u16_t)uiByte << 8u);
    }
}

static void shifter_read_byte(st_u32_t            uiOffset,
                               st_u8_t            *puiByte)
{
    *puiByte = (uiOffset < 0x80u)
               ? g_st_machine_ptCtx.aShifterRegs[uiOffset] : 0xFFu;
}

/* ------------------------------------------------------------------
 * Internal: YM2149 (PSG) helpers
 * ------------------------------------------------------------------ */

static void ym2149_write_byte( st_u32_t      uiOffset,
                               st_u8_t       uiByte)
{
    switch (uiOffset)
    {
        case 0x00u: /* 0xFF8800: register select latch */
            g_st_machine_ptCtx.uiYmRegSel = uiByte & 0x0Fu;
            break;
        case 0x02u: /* 0xFF8802: data write */
            g_st_machine_ptCtx.auYmRegs[g_st_machine_ptCtx.uiYmRegSel] = uiByte;
            break;
        default:
            break;
    }
}

static void ym2149_read_byte( st_u32_t            uiOffset,
                              st_u8_t            *puiByte)
{
    /* 0xFF8800: read data from currently selected register */
    *puiByte = (uiOffset == 0x00u)
               ? g_st_machine_ptCtx.auYmRegs[g_st_machine_ptCtx.uiYmRegSel] : 0xFFu;
}

/* ------------------------------------------------------------------
 * Internal: MFP 68901 stubs
 * ------------------------------------------------------------------ */

static void mfp_write_byte( st_u32_t      uiOffset,
                            st_u8_t       uiByte)
{
    if (uiOffset < 0x40u)
        g_st_machine_ptCtx.aMfpRegs[uiOffset] = uiByte;
}

static void mfp_read_byte( st_u32_t            uiOffset,
                           st_u8_t            *puiByte)
{
    *puiByte = (uiOffset < 0x40u) ? g_st_machine_ptCtx.aMfpRegs[uiOffset] : 0xFFu;
}

/* ------------------------------------------------------------------
 * Internal: ACIA 6850 stubs
 * ------------------------------------------------------------------ */

static void acia_write_byte(st_u32_t      uiOffset,
                             st_u8_t       uiByte)
{
    if (uiOffset < 8u)
        g_st_machine_ptCtx.aAcia[uiOffset] = uiByte;
}

static void acia_read_byte(st_u32_t            uiOffset,
                            st_u8_t            *puiByte)
{
    *puiByte = (uiOffset < 8u) ? g_st_machine_ptCtx.aAcia[uiOffset] : 0xFFu;
}

/* ------------------------------------------------------------------
 * Public API (see description in ST.h)
 * ------------------------------------------------------------------ */
///////////////////////////////////////////////////////////////////////////////
//
st_u64_t st_init(const char *szRomPath)
{
    LOG_TRACE("ptMachine=%p szRomPath='%s'",
              (void *)&g_st_machine_ptCtx, szRomPath ? szRomPath : "(none)");
    
    /* -- [ST]1. Init the ST Machine context -- */
    g_st_machine_ptCtx.bPoweredOn   = ST_TRUE;
    g_st_machine_ptCtx.aShifterRegs[ST_VID_SYNC_MODE] = 0x02u; // European 50Hz default

    /* -- [ST]2. Init ROM in memory -- */
    LOG_TODO("st_init: TOS ROM loading deferred to UC25 "
             "(szRomPath='%s')", szRomPath ? szRomPath : "(none)");

    /* -- [ST]3. Init returns context sructure -- */
    LOG_INFO("ST machine initialised (RAM=%uKB)",
             (unsigned)(ST_RAM_SIZE / 1024));
    return (st_u64_t)&g_st_machine_ptCtx;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t st_read_byte(st_u32_t uiAddr, st_u8_t *puiByte)
{
    st_u32_t uiA;
    st_u32_t uiRomOff;

    // LOG_TRACE off : function is called too many times - see LOG_ERROR

    /* -- [ST]4. Reject any NULL parameter with an error -- */
    if (puiByte == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    
    /* -- [ST]5. Returns 0xFF if machine is not powered on -- */
    if (g_st_machine_ptCtx.bPoweredOn == ST_FALSE)
    {
        LOG_ERROR("Needs st_init() before reading byte in memory");
        *puiByte = 0xFFu;
        return ST_ERROR;
    }
    
    uiA = uiAddr & 0xFFFFFFu;

    if (uiA < ST_RAM_SIZE)
    {
        /* -- [ST]6. If address is in RAM, return the RAM Value -- */
        *puiByte = g_st_machine_ptCtx.aRam[uiA];
    }
    else if (uiA >= ST_ADDR_ROM_LO && uiA <= ST_ADDR_ROM_HI)
    {
        /* -- [ST]7. If address is in ROM, return the ROM Value -- */
        uiRomOff = uiA - ST_ADDR_ROM_LO;
        *puiByte = (g_st_machine_ptCtx.bRomLoaded && uiRomOff < (st_u32_t)ST_ROM_SIZE)
                   ? g_st_machine_ptCtx.aRom[uiRomOff] : 0xFFu;
    }
    else if (uiA >= ST_ADDR_SHIFTER_LO && uiA <= ST_ADDR_SHIFTER_HI)
    {
        /* -- [ST]8. If address is in Shifter area, returns value -- */
        shifter_read_byte(uiA - ST_ADDR_SHIFTER_LO, puiByte);
    }
    else if (uiA >= ST_ADDR_YM_LO && uiA <= ST_ADDR_YM_HI)
    {
        /* -- [ST]9. If address is in YM2149 area, returns value -- */
        ym2149_read_byte(uiA - ST_ADDR_YM_LO, puiByte);
    }
    else if (uiA >= ST_ADDR_MFP_LO && uiA <= ST_ADDR_MFP_HI)
    {
        /* -- [ST]10. If address is in MFP area, returns value -- */
        mfp_read_byte(uiA - ST_ADDR_MFP_LO, puiByte);
    }
    else if (uiA >= ST_ADDR_ACIA_LO && uiA <= ST_ADDR_ACIA_HI)
    {
        /* -- [ST]11. If address is in ACIA area, returns value -- */
        acia_read_byte(uiA - ST_ADDR_ACIA_LO, puiByte);
    }
    else
    {
        /* -- [ST]12. If address is in unknown area, returns 0xFF -- */
        LOG_ERROR("Attempting to read in unknown memory area (0x%06X)", uiA);
        *puiByte = 0xFFu;   /* open bus */
        return ST_ERROR;
    }
    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t st_read_word( st_u32_t uiAddr, st_u16_t *puiWord)
{
    st_u8_t uiHi;
    st_u8_t uiLo;
    st_error_t eR;

    // LOG_TRACE off : function is called too many times - see LOG_ERROR

    /* -- [ST]13. Reject any NULL parameter with an error -- */
    if (puiWord == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    /* -- [ST]14. Reject unaligned address with an error -- */
    if (uiAddr & 1u)
    {
        LOG_ERROR("unaligned word read at 0x%06X", uiAddr);
        *puiWord = 0xFFFFu; 
        return ST_ERROR;
    }

    /* -- [ST]15. Read word, based on read bytes function -- */
    eR = st_read_byte(uiAddr,     &uiHi);
    if (eR != ST_NO_ERROR) { *puiWord = 0xFFFFu; return eR; }
    eR = st_read_byte(uiAddr + 1, &uiLo);
    if (eR != ST_NO_ERROR) { *puiWord = 0xFFFFu; return eR; }
    *puiWord = (st_u16_t)(((st_u16_t)uiHi << 8) | uiLo);
    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t st_read_long( st_u32_t uiAddr, st_u32_t *puiLong)
{
    st_u16_t uiHi;
    st_u16_t uiLo;
    st_error_t eR;

    // LOG_TRACE off : function is called too many times - see LOG_ERROR

    /* -- [ST]16. Reject any NULL parameter with an error -- */
    if (puiLong == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    
    /* -- [ST]17. Read long, based on read word function -- */
    eR = st_read_word(uiAddr,     &uiHi);
    if (eR != ST_NO_ERROR) { *puiLong = 0xFFFFFFFFu; return eR; }
    eR = st_read_word(uiAddr + 2, &uiLo);
    if (eR != ST_NO_ERROR) { *puiLong = 0xFFFFFFFFu; return eR; }
    *puiLong = ((st_u32_t)uiHi << 16) | uiLo;
    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t st_write_byte( st_u32_t uiAddr, st_u8_t uiByte)
{
    st_u32_t uiA;

    // LOG_TRACE off : function is called too many times - see LOG_ERROR

    /* -- [ST]28. Do not write in memory if machine is not powered on -- */
    if (g_st_machine_ptCtx.bPoweredOn == ST_FALSE)
    {
        LOG_ERROR("Needs st_init() before writing in memory");
        return ST_ERROR;
    }
    
    uiA = uiAddr & 0xFFFFFFu;

    if (uiA < ST_RAM_SIZE)
    {
        /* -- [ST]18. If address is in RAM, write the RAM Value -- */
        g_st_machine_ptCtx.aRam[uiA] = uiByte;
    }
    else if (uiA >= ST_ADDR_ROM_LO && uiA <= ST_ADDR_ROM_HI)
    {
        /* -- [ST]19. If address is in ROM, reject with an error -- */
        LOG_ERROR("ROM write: addr=0x%06X val=0x%02X", uiA, uiByte);
        return ST_ERROR;    /* bus error — ROM is read-only */
    }
    else if (uiA >= ST_ADDR_SHIFTER_LO && uiA <= ST_ADDR_SHIFTER_HI)
    {
        /* -- [ST]20. If address is in Shifter area, write value -- */
        shifter_write_byte(uiA - ST_ADDR_SHIFTER_LO, uiByte);
    }
    else if (uiA >= ST_ADDR_YM_LO && uiA <= ST_ADDR_YM_HI)
    {
        /* -- [ST]21. If address is in YM2149 area, write value -- */
        ym2149_write_byte(uiA - ST_ADDR_YM_LO, uiByte);
    }
    else if (uiA >= ST_ADDR_MFP_LO && uiA <= ST_ADDR_MFP_HI)
    {
        /* -- [ST]22. If address is in MFP area, write value -- */
        mfp_write_byte(uiA - ST_ADDR_MFP_LO, uiByte);
    }
    else if (uiA >= ST_ADDR_ACIA_LO && uiA <= ST_ADDR_ACIA_HI)
    {
        /* -- [ST]23. If address is in ACIA area, write value -- */
        acia_write_byte(uiA - ST_ADDR_ACIA_LO, uiByte);
    }
    else
    {
        /* -- [ST]24. Log an error, if address is in unknown area-- */
        LOG_ERROR("Attempting to write in unmapped memory");
        return ST_ERROR;
    }
    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
/*
 * st_check_write_range() - Classify [uiAddr, uiAddr+uiLen) as writable,
 * without writing anything.
 *
 * Same region boundaries as st_write_byte() (RAM + Shifter/YM/MFP/ACIA
 * accepted, ROM + unmapped rejected). Used by st_write_long() to
 * validate its 4-byte span up-front, so a failure on the low word
 * cannot leave the high word already written (all-or-nothing write,
 * matching the read side's behaviour on error). Not needed by
 * st_write_word(): every region's low bound and size are even, so an
 * aligned word write can never straddle two regions.
 */
static st_error_t st_check_write_range(st_u32_t uiAddr, st_u32_t uiLen)
{
    st_u32_t uiA;
    st_u32_t uiI;

    for (uiI = 0; uiI < uiLen; uiI++)
    {
        uiA = (uiAddr + uiI) & 0xFFFFFFu;

        if (uiA < ST_RAM_SIZE)                                     { continue; }
        if (uiA >= ST_ADDR_SHIFTER_LO && uiA <= ST_ADDR_SHIFTER_HI) { continue; }
        if (uiA >= ST_ADDR_YM_LO      && uiA <= ST_ADDR_YM_HI)      { continue; }
        if (uiA >= ST_ADDR_MFP_LO     && uiA <= ST_ADDR_MFP_HI)     { continue; }
        if (uiA >= ST_ADDR_ACIA_LO    && uiA <= ST_ADDR_ACIA_HI)    { continue; }

        /* ROM (read-only) or unmapped: reject the whole range */
        LOG_ERROR("write range [0x%06X..0x%06X] rejected at byte 0x%06X",
                  uiAddr, uiAddr + uiLen - 1, uiA);
        return ST_ERROR;
    }
    return ST_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t st_write_word(  st_u32_t uiAddr, st_u16_t uiWord)
{
    st_error_t eR;

    // LOG_TRACE off : function is called too many times - see LOG_ERROR

    /* -- [ST]25. Reject unaligned address with an error -- */
    if (uiAddr & 1u)
    {
        LOG_ERROR("unaligned word write at 0x%06X", uiAddr);
        return ST_ERROR;
    }

    /* No range pre-check needed here: every region's low bound is even
     * and every region's size is even (see st_check_write_range()), so
     * an even uiAddr and uiAddr+1 always fall in the same region — a
     * word write can never straddle two regions. */

    /* -- [ST]26. Write word, based on write byte function -- */
    eR = st_write_byte(uiAddr,     (st_u8_t)(uiWord >> 8));
    if (eR != ST_NO_ERROR) { return eR; }
    eR = st_write_byte(uiAddr + 1, (st_u8_t)(uiWord & 0xFFu));
    return eR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t st_write_long(st_u32_t uiAddr, st_u32_t uiLong)
{
    st_error_t eR;

    // LOG_TRACE off : function is called too many times - see LOG_ERROR

    /* -- [ST]29. Reject the write if any of the 4 bytes is not writable,
     *            before touching any word (atomic long write) -- */
    eR = st_check_write_range(uiAddr, 4u);
    if (eR != ST_NO_ERROR) { return eR; }

    /* -- [ST]27. Write long, based on write word function -- */
    eR = st_write_word(uiAddr,     (st_u16_t)(uiLong >> 16));
    if (eR != ST_NO_ERROR) { return eR; }
    eR = st_write_word(uiAddr + 2, (st_u16_t)(uiLong & 0xFFFFu));
    return eR;
}

///////////////////////////////////////////////////////////////////////////////
//
st_error_t st_shutdown()
{
    LOG_TRACE("ptMachine=%p", (void *)&g_st_machine_ptCtx);

    /* -- [ST]30. Reset the ST Machine context to power-off state -- */
    memset(&g_st_machine_ptCtx, 0, sizeof(st_machine_context_t));
    g_st_machine_ptCtx.ulMagic = 0xCAFEDECA;
    g_st_machine_ptCtx.eObject = ST_MACHINE_CTX;
    LOG_INFO("ST machine shut down (bPoweredOn=%d)", g_st_machine_ptCtx.bPoweredOn);
    return ST_NO_ERROR;
}

st_u16_t st_get_palette(st_u16_t uiColIdx)
{
    return g_st_machine_ptCtx.auPalette[uiColIdx];
}

st_u32_t st_get_screen_base()
{
    return g_st_machine_ptCtx.uiScreenBase;
}

st_u32_t st_get_resolution()
{
    return g_st_machine_ptCtx.uiResolution;
}

void*    st_get_ram_pointer(st_u32_t uiAddr)
{
    /* -- [ST]31. Reject an out-of-bounds address, return NULL -- */
    return (uiAddr >= ST_RAM_SIZE) ? (void*)NULL : &(g_st_machine_ptCtx.aRam[uiAddr]);
}
