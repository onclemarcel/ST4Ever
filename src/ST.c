/*
 * ST.c - Atari ST machine emulation (stub)
 * TODO(UC15): Load PRG + fixups.
 * TODO(UC24): Hardware register read/write handlers.
 */
#include "ST.h"
#include "trace.h"
#include <string.h>

st_error_t st_init(st_machine_t *ptMachine, const char *szRomPath)
{
    LOG_TRACE("ptMachine=%p szRomPath='%s'",
              (void *)ptMachine, szRomPath ? szRomPath : "(none)");
    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    memset(ptMachine, 0, sizeof(st_machine_t));
    ptMachine->uiResolution = ST_RES_LOW;
    ptMachine->bPoweredOn   = ST_TRUE;
    ST_UNUSED(szRomPath);
    LOG_TODO("st_init: ROM loading and HW register reset not yet "
             "implemented (UC24)");
    LOG_INFO("ST machine initialised (RAM=%uKB, stub mode)",
             (unsigned)(ST_RAM_SIZE / 1024));
    return ST_NO_ERROR;
}

st_error_t st_read_byte(const st_machine_t *ptMachine,
                          st_u32_t uiAddr, st_u8_t *puiByte)
{
    LOG_TRACE("addr=0x%06X", uiAddr);
    if (ptMachine == NULL || puiByte == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    if (uiAddr < ST_RAM_SIZE)
    {
        *puiByte = ptMachine->aRam[uiAddr];
        return ST_NO_ERROR;
    }
    LOG_TODO("st_read_byte: hw register and ROM reads not yet "
             "implemented (UC24) - addr=0x%06X", uiAddr);
    *puiByte = 0xFF;
    return ST_NO_ERROR;
}

st_error_t st_read_word(const st_machine_t *ptMachine,
                          st_u32_t uiAddr, st_u16_t *puiWord)
{
    st_u8_t uiHi;
    st_u8_t uiLo;
    st_error_t eR;

    LOG_TRACE("addr=0x%06X", uiAddr);
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

    LOG_TRACE("addr=0x%06X", uiAddr);
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
    LOG_TRACE("addr=0x%06X val=0x%02X", uiAddr, uiByte);
    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    if (uiAddr < ST_RAM_SIZE)
    {
        ptMachine->aRam[uiAddr] = uiByte;
        return ST_NO_ERROR;
    }
    LOG_TODO("st_write_byte: hw register writes not yet implemented "
             "(UC24) - addr=0x%06X val=0x%02X", uiAddr, uiByte);
    return ST_NO_ERROR;
}

st_error_t st_write_word(st_machine_t *ptMachine,
                           st_u32_t uiAddr, st_u16_t uiWord)
{
    st_error_t eR;
    LOG_TRACE("addr=0x%06X val=0x%04X", uiAddr, uiWord);
    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    if (uiAddr & 1u) { LOG_ERROR("unaligned word write at 0x%06X", uiAddr); return ST_ERROR; }
    eR = st_write_byte(ptMachine, uiAddr,     (st_u8_t)(uiWord >> 8));
    if (eR != ST_NO_ERROR) { return eR; }
    eR = st_write_byte(ptMachine, uiAddr + 1, (st_u8_t)(uiWord & 0xFF));
    return eR;
}

st_error_t st_write_long(st_machine_t *ptMachine,
                           st_u32_t uiAddr, st_u32_t uiLong)
{
    st_error_t eR;
    LOG_TRACE("addr=0x%06X val=0x%08X", uiAddr, uiLong);
    if (ptMachine == NULL) { LOG_ERROR("NULL ptMachine"); return ST_ERROR; }
    eR = st_write_word(ptMachine, uiAddr,     (st_u16_t)(uiLong >> 16));
    if (eR != ST_NO_ERROR) { return eR; }
    eR = st_write_word(ptMachine, uiAddr + 2, (st_u16_t)(uiLong & 0xFFFF));
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
