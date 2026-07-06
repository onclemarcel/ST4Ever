/*
 * tos.c - ST4Ever GEMDOS / XBIOS minimal TOS dispatcher (UC29)
 *
 * Intercepts TRAP #1 and TRAP #14 at emulator level (before the 68000
 * exception frame is pushed).  Only the subset needed for common demo
 * programs is implemented; all others emit LOG_TODO and return cleanly.
 */

#include "tos.h"
#include "trace.h"

/* ------------------------------------------------------------------
 * Internal: stack read helpers
 * ------------------------------------------------------------------ */

/* Read a word (16-bit) from the program stack at SP + uiOff.
 * Returns ST_ERROR on out-of-bounds access.                          */
static st_error_t stack_read_word( st_u32_t            uiSP,
                                   st_u32_t            uiOff,
                                   st_u16_t           *puiWord)
{
    return st_read_word(uiSP + uiOff, puiWord);
}

/* Read a longword (32-bit) from the program stack at SP + uiOff.
 * Returns ST_ERROR on out-of-bounds access.                          */
static st_error_t stack_read_long( st_u32_t            uiSP,
                                   st_u32_t            uiOff,
                                   st_u32_t           *puiLong)
{
    return st_read_long(uiSP + uiOff, puiLong);
}

/* ------------------------------------------------------------------
 * tos_gemdos - TRAP #1 handler
 * ------------------------------------------------------------------ */

st_error_t tos_gemdos(cpu_context_t     *ptCpu)
{
    st_u16_t    uiFn;
    st_u16_t    uiRetCode;
    st_error_t  eR;

    LOG_TRACE("ptCpu=%p", (void *)ptCpu);

    if (ptCpu == NULL)
    {
        LOG_ERROR("NULL parameter: ptCpu=%p",
                  (void *)ptCpu);
        return ST_ERROR;
    }

    eR = stack_read_word(ptCpu->auAn[7], 0u, &uiFn);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("GEMDOS: failed to read function number from SP=0x%06X",
                  (unsigned)ptCpu->auAn[7]);
        return ST_ERROR;
    }

    switch ((unsigned int)uiFn)
    {
        case TOS_GEMDOS_PTERM0:
            /* Terminate process with exit code 0 */
            LOG_INFO("GEMDOS Pterm0: halting CPU at PC=0x%06X",
                     (unsigned)ptCpu->uiPC);
            ptCpu->eState = CPU_STATE_STOPPED;
            ptCpu->auDn[0] = 0;
            return ST_NO_ERROR;

        case TOS_GEMDOS_CCONWS:
            /* Write null-terminated string to console — stub */
            LOG_TODO("GEMDOS Cconws at PC=0x%06X (no console output)",
                     (unsigned)ptCpu->uiPC);
            ptCpu->auDn[0] = 0;
            return ST_NO_ERROR;

        case TOS_GEMDOS_PTERM:
            /* Terminate process with exit code from stack */
            eR = stack_read_word(ptCpu->auAn[7], 2u, &uiRetCode);
            if (eR != ST_NO_ERROR)
            {
                uiRetCode = 0;
            }
            LOG_INFO("GEMDOS Pterm: retcode=%d halting CPU at PC=0x%06X",
                     (int)(st_i16_t)uiRetCode, (unsigned)ptCpu->uiPC);
            ptCpu->eState = CPU_STATE_STOPPED;
            ptCpu->auDn[0] = (st_u32_t)(st_i32_t)(st_i16_t)uiRetCode;
            return ST_NO_ERROR;

        default:
            LOG_TODO("GEMDOS fn=%u at PC=0x%06X (not implemented)",
                     (unsigned)uiFn, (unsigned)ptCpu->uiPC);
            ptCpu->auDn[0] = 0;
            return ST_NO_ERROR;
    }
}

/* ------------------------------------------------------------------
 * tos_xbios - TRAP #14 handler
 * ------------------------------------------------------------------ */

st_error_t tos_xbios(cpu_context_t     *ptCpu)
{
    st_u16_t   uiFn;
    st_u32_t   uiPhysBase;
    st_u32_t   uiPalPtr;
    st_u16_t   uiRez;
    st_u16_t   uiColIdx;
    st_u16_t   uiColVal;
    st_u16_t   uiPalWord;
    unsigned   uiI;
    st_error_t eR;

    LOG_TRACE("ptCpu=%p", (void *)ptCpu);

    if (ptCpu == NULL)
    {
        LOG_ERROR("NULL parameter: ptCpu=%p",
                  (void *)ptCpu);
        return ST_ERROR;
    }

    eR = stack_read_word(ptCpu->auAn[7], 0u, &uiFn);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("XBIOS: failed to read function number from SP=0x%06X",
                  (unsigned)ptCpu->auAn[7]);
        return ST_ERROR;
    }

    switch ((unsigned int)uiFn)
    {
        case TOS_XBIOS_VSYNC:
            /* Wait for VBL — no-op in emulator */
            ptCpu->auDn[0] = 0;
            return ST_NO_ERROR;

        case TOS_XBIOS_SETPALETTE:
            /*
             * Setpalette(palette_ptr): SP+2 = long ptr to 16 ST words.
             * Copy all 16 colour entries to the HW palette registers.
             */
            eR = stack_read_long(ptCpu->auAn[7], 2u, &uiPalPtr);
            if (eR != ST_NO_ERROR)
            {
                LOG_ERROR("XBIOS Setpalette: failed to read palptr from "
                          "SP+2=0x%06X", (unsigned)(ptCpu->auAn[7] + 2u));
                return ST_ERROR;
            }
            for (uiI = 0u; uiI < (unsigned)ST_PALETTE_COLORS; uiI++)
            {
                eR = st_read_word(uiPalPtr + (st_u32_t)(uiI * 2u),
                                  &uiPalWord);
                if (eR == ST_NO_ERROR)
                {
                    (void)st_write_word(0xFF8240u + (st_u32_t)(uiI * 2u),
                                        uiPalWord);
                }
            }
            LOG_INFO("XBIOS Setpalette: palptr=0x%06X", (unsigned)uiPalPtr);
            ptCpu->auDn[0] = 0;
            return ST_NO_ERROR;

        case TOS_XBIOS_SETCOLOR:
            /*
             * Setcolor(index, color): SP+2 = word index, SP+4 = word color.
             * Sets one palette entry.  Returns the old colour value in D0.
             */
            eR = stack_read_word(ptCpu->auAn[7], 2u, &uiColIdx);
            if (eR != ST_NO_ERROR)
            {
                LOG_ERROR("XBIOS Setcolor: failed to read index from SP+2");
                return ST_ERROR;
            }
            eR = stack_read_word(ptCpu->auAn[7], 4u, &uiColVal);
            if (eR != ST_NO_ERROR)
            {
                LOG_ERROR("XBIOS Setcolor: failed to read value from SP+4");
                return ST_ERROR;
            }
            if (uiColIdx < (st_u16_t)ST_PALETTE_COLORS)
            {
                ptCpu->auDn[0] = (st_u32_t)st_get_palette(uiColIdx);
                    
                (void)st_write_word(0xFF8240u + (st_u32_t)(uiColIdx * 2u),
                                    uiColVal);
                LOG_INFO("XBIOS Setcolor: idx=%u val=0x%04X",
                         (unsigned)uiColIdx, (unsigned)uiColVal);
            }
            else
            {
                LOG_ERROR("XBIOS Setcolor: index %u out of range [0..15]",
                          (unsigned)uiColIdx);
                ptCpu->auDn[0] = 0;
            }
            return ST_NO_ERROR;

        case TOS_XBIOS_SETSCREEN:
            /*
             * Setscreen(physbase, logbase, rez):
             *   SP+2  physbase (long): physical screen base (-1 = unchanged)
             *   SP+6  logbase  (long): logical screen base  (-1 = unchanged)
             *   SP+10 rez      (word): resolution 0-2       (-1 = unchanged)
             * Only physbase and rez are handled; logbase is ignored.
             */
            eR = stack_read_long(ptCpu->auAn[7], 2u, &uiPhysBase);
            if (eR != ST_NO_ERROR)
            {
                LOG_ERROR("XBIOS Setscreen: failed to read physbase");
                return ST_ERROR;
            }
            eR = stack_read_word(ptCpu->auAn[7], 10u, &uiRez);
            if (eR != ST_NO_ERROR)
            {
                LOG_ERROR("XBIOS Setscreen: failed to read rez");
                return ST_ERROR;
            }

            if (uiPhysBase != 0xFFFFFFFFu)
            {
                (void)st_write_byte(0xFF8201u,
                                    (st_u8_t)((uiPhysBase >> 16) & 0xFFu));
                (void)st_write_byte(0xFF8203u,
                                    (st_u8_t)((uiPhysBase >>  8) & 0xFFu));
                (void)st_write_byte(0xFF820Du,
                                    (st_u8_t)(uiPhysBase & 0xFFu));
                LOG_INFO("XBIOS Setscreen: physbase=0x%06X",
                         (unsigned)uiPhysBase);
            }
            if (uiRez != 0xFFFFu)
            {
                (void)st_write_byte(0xFF8260u,
                                    (st_u8_t)(uiRez & 0x03u));
                LOG_INFO("XBIOS Setscreen: rez=%u", (unsigned)(uiRez & 3u));
            }
            ptCpu->auDn[0] = 0;
            return ST_NO_ERROR;

        default:
            LOG_TODO("XBIOS fn=%u at PC=0x%06X (not implemented)",
                     (unsigned)uiFn, (unsigned)ptCpu->uiPC);
            ptCpu->auDn[0] = 0;
            return ST_NO_ERROR;
    }
}
