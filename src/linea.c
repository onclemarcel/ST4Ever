/*
 * linea.c - ST4Ever Line-A ($Axxx) opcode dispatcher
 *
 * UC28: LINEA_INIT ($A000) + stubs for other Line-A services.
 *
 * Architecture note: ST4Ever intercepts Line-A opcodes directly in the
 * CPU dispatcher (cpu_step case 0xA -> linea_dispatch) rather than
 * pushing an exception frame and running a TOS handler from RAM.  This
 * gives identical observable results for demo programs while removing
 * the need for a TOS ROM or a self-modifying RAM handler.
 */

#include "linea.h"
#include "trace.h"
#include <string.h>

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/* Return geometry for the current resolution stored in ptMachine.     */
static void get_resolution_params( int                *piWidth,
                                   int                *piHeight,
                                   int                *piPlanes,
                                   int                *piLinWr)
{
    switch (st_get_resolution())
    {
        case ST_RES_MED:
            *piWidth  = 640;
            *piHeight = 200;
            *piPlanes = 2;
            *piLinWr  = 160;
            break;
        case ST_RES_HIGH:
            *piWidth  = 640;
            *piHeight = 400;
            *piPlanes = 1;
            *piLinWr  = 80;
            break;
        default: /* ST_RES_LOW */
            *piWidth  = 320;
            *piHeight = 200;
            *piPlanes = 4;
            *piLinWr  = 160;
            break;
    }
}

/* ------------------------------------------------------------------
 * linea_init
 * ------------------------------------------------------------------ */

st_error_t linea_init()
{
    int       iWidth;
    int       iHeight;
    int       iPlanes;
    int       iLinWr;
    int       iCelHt;
    int       iColsMinus1;
    int       iRowsMinus1;
    int       iCelWr;
    st_u32_t  uiBase;
    void*     ptSTRam;

    LOG_TRACE("Initialization of line A");

    /* Validate that the block area is within RAM */
    ptSTRam = (void*)st_get_ram_pointer(LINEA_PARAM_ADDR - LINEA_PARAM_NEG_SIZE);
    if (LINEA_PARAM_ADDR + 32u > ST_RAM_SIZE
        || ptSTRam == NULL)
    {
        LOG_ERROR("LINEA_PARAM_ADDR 0x%04X out of RAM bounds",
                  LINEA_PARAM_ADDR);
        return ST_ERROR;
    }

    /* Zero the entire block first */
    memset(ptSTRam, 0, LINEA_BLOCK_SIZE);

    /* Derive geometry from current resolution */
    get_resolution_params(&iWidth, &iHeight, &iPlanes, &iLinWr);

    /* Character cell: 8x8 font (fixed for this emulator stub) */
    iCelHt       = 8;
    iColsMinus1  = (iWidth / 8) - 1;   /* 39 for 320, 79 for 640  */
    iRowsMinus1  = (iHeight / iCelHt) - 1; /* 24 for 200px/8px    */
    iCelWr       = iLinWr * iCelHt;    /* bytes per text row      */

    /* Screen base from machine state */
    uiBase = st_get_screen_base();

    /* Write negative-offset fields (at LINEA_PARAM_ADDR - offset)   */
    st_write_word(LINEA_PARAM_ADDR - LINEA_V_CEL_MX_OFF,
               (st_u16_t)iColsMinus1);
    st_write_word(LINEA_PARAM_ADDR - LINEA_V_CEL_MY_OFF,
               (st_u16_t)iRowsMinus1);
    st_write_word(LINEA_PARAM_ADDR - LINEA_V_CEL_WR_OFF,
               (st_u16_t)iCelWr);
    st_write_word(LINEA_PARAM_ADDR - LINEA_V_CEL_HT_OFF,
               (st_u16_t)iCelHt);
    st_write_word(LINEA_PARAM_ADDR - LINEA_V_PLANES_OFF,
               (st_u16_t)iPlanes);
    st_write_word(LINEA_PARAM_ADDR - LINEA_V_LIN_WR_OFF,
               (st_u16_t)iLinWr);
    st_write_long(LINEA_PARAM_ADDR - LINEA_V_BAS_AD_OFF,
               uiBase);

    LOG_INFO("linea param block at 0x%04X: res=%u planes=%d linwr=%d "
             "base=0x%06X",
             LINEA_PARAM_ADDR,
             (unsigned)st_get_resolution(),
             iPlanes, iLinWr, (unsigned)uiBase);

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * PUT_PIXEL ($A001)
 * Register convention: D0.w = color index, D1.w = y, D2.w = x.
 * Writes one pixel into the bitplane frame buffer in ST RAM.
 * Clamps out-of-bounds coordinates silently (no error).
 * ------------------------------------------------------------------ */

static st_error_t linea_do_put_pixel(cpu68k_t     *ptCpu)
{
    int      iX;
    int      iY;
    int      iColor;
    int      iWidth;
    int      iHeight;
    int      iPlanes;
    int      iLinWr;
    int      iWordGroup;
    int      iBitPos;
    int      iPlane;
    int      iPlaneBit;
    st_u32_t uiBase;
    st_u32_t uiLineOff;
    st_u32_t uiGroupOff;
    st_u32_t uiWordAddr;
    st_u32_t uiBytesPerGroup;
    st_u16_t uiWord;

    /* Coordinates and color from data registers */
    iColor = (int)(st_i16_t)(st_u16_t)(ptCpu->auDn[0] & 0xFFFFu);
    iY     = (int)(st_i16_t)(st_u16_t)(ptCpu->auDn[1] & 0xFFFFu);
    iX     = (int)(st_i16_t)(st_u16_t)(ptCpu->auDn[2] & 0xFFFFu);

    get_resolution_params(&iWidth, &iHeight, &iPlanes, &iLinWr);

    /* Clamp — out-of-range pixel is silently ignored */
    if (iX < 0 || iX >= iWidth || iY < 0 || iY >= iHeight)
        return ST_NO_ERROR;

    uiBase       = st_get_screen_base();
    iWordGroup   = iX / 16;
    iBitPos      = 15 - (iX % 16);   /* MSB = leftmost pixel */
    uiLineOff    = (st_u32_t)(iY * iLinWr);
    uiBytesPerGroup = (st_u32_t)(iPlanes * 2);
    uiGroupOff   = (st_u32_t)(iWordGroup) * uiBytesPerGroup;

    for (iPlane = 0; iPlane < iPlanes; iPlane++)
    {
        iPlaneBit  = (iColor >> iPlane) & 1;
        uiWordAddr = uiBase + uiLineOff + uiGroupOff
                     + (st_u32_t)(iPlane * 2);

        if (uiWordAddr + 1u >= ST_RAM_SIZE)
            continue;

        st_read_word(uiWordAddr, &uiWord);

        if (iPlaneBit)
            uiWord |=  (st_u16_t)(1u << iBitPos);
        else
            uiWord &= (st_u16_t)~(st_u16_t)(1u << iBitPos);

        st_write_word(uiWordAddr, uiWord);
    }

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * LINEA_INIT ($A000)
 * Sets A0 = LINEA_PARAM_ADDR and refreshes the parameter block.
 * ------------------------------------------------------------------ */

static st_error_t linea_do_init(cpu68k_t     *ptCpu)
{
    st_error_t eResult;

    eResult = linea_init();
    if (eResult != ST_NO_ERROR)
    {
        return eResult;
    }

    /* A0 = pointer to Line-A parameter block */
    ptCpu->auAn[0] = LINEA_PARAM_ADDR;

    LOG_INFO("LINEA_INIT: A0=0x%04X", LINEA_PARAM_ADDR);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * linea_dispatch
 * ------------------------------------------------------------------ */

st_error_t linea_dispatch(cpu68k_t     *ptCpu,
                           st_u16_t      uiOpcode)
{
    unsigned int uiFn;

    if (ptCpu == NULL)
    {
        LOG_ERROR("NULL parameter: ptCpu=%p",
                  (void *)ptCpu);
        return ST_ERROR;
    }

    /* Function number in bits [7:0] */
    uiFn = (unsigned int)(uiOpcode & 0x00FFu);

    switch (uiFn)
    {
        case 0x00: /* LINEA_INIT */
            return linea_do_init(ptCpu);

        case 0x01: /* PUT_PIXEL - D0=color, D1=y, D2=x */
            return linea_do_put_pixel(ptCpu);

        case 0x02: /* GET_PIXEL */
            LOG_TODO("LINEA GET_PIXEL at PC=0x%06X", (unsigned)ptCpu->uiPC);
            ptCpu->auDn[0] = 0; /* return 0 (colour index) */
            return ST_NO_ERROR;

        case 0x03: /* DRAW_LINE */
            LOG_TODO("LINEA DRAW_LINE at PC=0x%06X", (unsigned)ptCpu->uiPC);
            return ST_NO_ERROR;

        case 0x04: /* FILLED_RECT */
            LOG_TODO("LINEA FILLED_RECT at PC=0x%06X",
                     (unsigned)ptCpu->uiPC);
            return ST_NO_ERROR;

        default:
            LOG_TODO("LINEA fn=0x%02X opcode=0x%04X at PC=0x%06X",
                     uiFn, (unsigned)uiOpcode, (unsigned)ptCpu->uiPC);
            return ST_NO_ERROR;
    }
}
