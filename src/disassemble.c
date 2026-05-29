/*
 * disassemble.c - 68000 disassembler stub
 * TODO(UC11-UC14): implement instruction decode tables.
 */
#include "disassemble.h"
#include "trace.h"
#include <string.h>
#include <stdio.h>

st_error_t disasm_one(const st_u8_t  *pBuf,
                        size_t          uiBufLen,
                        st_u32_t        uiAddr,
                        disasm_result_t *ptResult)
{
    st_u16_t uiOpcode;

    LOG_TRACE("pBuf=%p len=%zu addr=0x%06X",
              (void *)pBuf, uiBufLen, uiAddr);
    if (pBuf == NULL || ptResult == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    memset(ptResult, 0, sizeof(disasm_result_t));
    ptResult->uiAddr  = uiAddr;
    ptResult->bValid  = ST_FALSE;

    if (uiBufLen < 2)
    {
        snprintf(ptResult->szLine, DISASM_LINE_MAX,
                 "                ; buffer too short");
        return ST_NO_ERROR;
    }

    /* Big-endian fetch */
    uiOpcode = (st_u16_t)(((st_u16_t)pBuf[0] << 8) | pBuf[1]);
    ptResult->auWords[0]  = uiOpcode;
    ptResult->iWordCount  = 1;

    /* Fallback: DC.W */
    snprintf(ptResult->szMnemonic, sizeof(ptResult->szMnemonic),
             "DC.W");
    snprintf(ptResult->szOperands, sizeof(ptResult->szOperands),
             "$%04X", (unsigned)uiOpcode);
    snprintf(ptResult->szLine, DISASM_LINE_MAX,
             "$%06X:  %04X          %-8s %s",
             uiAddr, (unsigned)uiOpcode,
             ptResult->szMnemonic, ptResult->szOperands);

    LOG_TODO("disasm_one: opcode 0x%04X not decoded (UC11)",
             (unsigned)uiOpcode);
    return ST_NO_ERROR;
}

st_error_t disasm_range(const st_u8_t  *pBuf,
                          size_t          uiBufLen,
                          st_u32_t        uiAddr,
                          disasm_result_t *ptResults,
                          size_t          uiMaxLines,
                          size_t         *puiLines)
{
    size_t     uiOffset;
    size_t     uiLine;
    st_error_t eR;

    LOG_TRACE("pBuf=%p len=%zu addr=0x%06X max=%zu",
              (void *)pBuf, uiBufLen, uiAddr, uiMaxLines);
    if (pBuf == NULL || ptResults == NULL || puiLines == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    uiOffset = 0;
    uiLine   = 0;

    while (uiOffset < uiBufLen && uiLine < uiMaxLines)
    {
        eR = disasm_one(pBuf + uiOffset, uiBufLen - uiOffset,
                        uiAddr + (st_u32_t)uiOffset,
                        &ptResults[uiLine]);
        if (eR != ST_NO_ERROR) { return eR; }

        uiOffset += (size_t)(ptResults[uiLine].iWordCount * 2);
        uiLine++;
    }

    *puiLines = uiLine;
    return ST_NO_ERROR;
}
