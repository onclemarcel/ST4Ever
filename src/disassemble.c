/*
 * disassemble.c - ST4Ever 68000 disassembler (DEVPAC3 output format)
 *
 * UC11: MOVE/MOVEA/MOVEQ/LEA/PEA/CLR/EXG/SWAP
 * UC12: ADD/ADDA/ADDI/ADDQ/ADDX/SUB/SUBA/SUBI/SUBQ/SUBX/
 *        CMP/CMPA/CMPI/CMPM/MULU/MULS/DIVU/DIVS/
 *        AND/ANDI/OR/ORI/EOR/EORI/NOT/NEG/NEGX/TST/EXT
 * UC13: ASL/ASR/LSL/LSR/ROL/ROR/BTST/BSET/BCLR/BCHG   (TODO)
 * UC14: BRA/BSR/Bcc/JMP/JSR/RTS/RTR/RTE/TRAP/ILLEGAL   (TODO)
 */

#include "disassemble.h"
#include "trace.h"
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------ */

static const char * const g_aszSzSfx[3] = { ".B", ".W", ".L" };

static const char * const g_aszDn[8] =
    { "D0","D1","D2","D3","D4","D5","D6","D7" };
static const char * const g_aszAn[8] =
    { "A0","A1","A2","A3","A4","A5","A6","A7" };

/* ------------------------------------------------------------------
 * Low-level helpers
 * ------------------------------------------------------------------ */

static st_u16_t disasm_rw(const st_u8_t *p)
{
    return (st_u16_t)(((st_u16_t)p[0] << 8) | p[1]);
}

static void disasm_fmt_words(const disasm_result_t *ptR,
                              char *pOut, size_t uiOutLen)
{
    int i;
    int iPos = 0;
    for (i = 0; i < ptR->iWordCount && i < 5; i++)
    {
        iPos += snprintf(pOut + iPos, uiOutLen - (size_t)iPos,
                         "%04X ", (unsigned)ptR->auWords[i]);
    }
    while (iPos < 20 && iPos < (int)uiOutLen - 1)
        pOut[iPos++] = ' ';
    pOut[iPos] = '\0';
}

static void disasm_fmt_line(disasm_result_t *ptR)
{
    char szW[24];
    disasm_fmt_words(ptR, szW, sizeof(szW));
    snprintf(ptR->szLine, DISASM_LINE_MAX,
             "$%06X:  %s%-8s %s",
             (unsigned)ptR->uiAddr, szW,
             ptR->szMnemonic, ptR->szOperands);
}

static void disasm_fill_words(disasm_result_t *ptR,
                               const st_u8_t *pBuf, size_t uiBufLen)
{
    int i;
    for (i = 1; i < ptR->iWordCount && i < 5; i++)
    {
        size_t uiOff = (size_t)(i * 2);
        if (uiOff + 1 < uiBufLen)
            ptR->auWords[i] = disasm_rw(pBuf + uiOff);
    }
}

static void disasm_dc_word(const st_u8_t *pBuf, size_t uiBufLen,
                            st_u32_t uiAddr, disasm_result_t *ptR)
{
    ptR->uiAddr      = uiAddr;
    ptR->iWordCount  = 1;
    ptR->bValid      = ST_FALSE;
    if (uiBufLen >= 2)
        ptR->auWords[0] = disasm_rw(pBuf);
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "DC.W");
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "$%04X", (unsigned)ptR->auWords[0]);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * Effective-address formatter
 *
 * pExt      : pointer to first extension byte after already-consumed words
 * uiRemB    : bytes remaining from pExt
 * uiPC      : address of the main opcode word
 * iExtSoFar : extension words already consumed before this EA
 * iMode     : EA mode field (3 bits, 0-7)
 * iReg      : EA register field (3 bits, 0-7)
 * iSz       : operand size  0=B, 1=W, 2=L
 * szOut/len : output buffer
 *
 * Returns: additional words consumed, or -1 on unsupported/invalid.
 * ------------------------------------------------------------------ */
static int disasm_fmt_ea(const st_u8_t *pExt, size_t uiRemB,
                          st_u32_t uiPC, int iExtSoFar,
                          int iMode, int iReg, int iSz,
                          char *szOut, size_t uiOutLen)
{
    st_u16_t uiW;
    st_u32_t uiAbs;
    st_i32_t iDisp;
    int      iIAn, iIReg, iISz;
    st_i32_t iIDisp;
    st_u32_t uiEPC = uiPC + (st_u32_t)((1 + iExtSoFar) * 2);

    switch (iMode)
    {
    case 0: snprintf(szOut, uiOutLen, "%s", g_aszDn[iReg]); return 0;
    case 1: snprintf(szOut, uiOutLen, "%s", g_aszAn[iReg]); return 0;
    case 2: snprintf(szOut, uiOutLen, "(%s)", g_aszAn[iReg]); return 0;
    case 3: snprintf(szOut, uiOutLen, "(%s)+", g_aszAn[iReg]); return 0;
    case 4: snprintf(szOut, uiOutLen, "-(%s)", g_aszAn[iReg]); return 0;

    case 5:
        if (uiRemB < 2) { snprintf(szOut, uiOutLen, "?"); return -1; }
        uiW   = disasm_rw(pExt);
        iDisp = (st_i32_t)(st_i16_t)uiW;
        if (iDisp < 0)
            snprintf(szOut, uiOutLen, "-$%X(%s)",
                     (unsigned)(-iDisp), g_aszAn[iReg]);
        else
            snprintf(szOut, uiOutLen, "$%X(%s)",
                     (unsigned)iDisp, g_aszAn[iReg]);
        return 1;

    case 6:
        if (uiRemB < 2) { snprintf(szOut, uiOutLen, "?"); return -1; }
        uiW    = disasm_rw(pExt);
        iIAn   = (uiW >> 15) & 1;
        iIReg  = (uiW >> 12) & 7;
        iISz   = (uiW >> 11) & 1;
        iIDisp = (st_i32_t)(st_i8_t)(uiW & 0xFF);
        if (iIDisp < 0)
            snprintf(szOut, uiOutLen, "-$%X(%s,%s%s)",
                     (unsigned)(-iIDisp), g_aszAn[iReg],
                     iIAn ? g_aszAn[iIReg] : g_aszDn[iIReg],
                     iISz ? ".L" : ".W");
        else
            snprintf(szOut, uiOutLen, "$%X(%s,%s%s)",
                     (unsigned)iIDisp, g_aszAn[iReg],
                     iIAn ? g_aszAn[iIReg] : g_aszDn[iIReg],
                     iISz ? ".L" : ".W");
        return 1;

    case 7:
        switch (iReg)
        {
        case 0:
            if (uiRemB < 2) { snprintf(szOut, uiOutLen, "?"); return -1; }
            uiW   = disasm_rw(pExt);
            uiAbs = (st_u32_t)(st_i32_t)(st_i16_t)uiW & 0x00FFFFFFu;
            if (uiW & 0x8000)
                snprintf(szOut, uiOutLen, "$%06X.W", (unsigned)uiAbs);
            else
                snprintf(szOut, uiOutLen, "$%04X.W", (unsigned)uiW);
            return 1;

        case 1:
            if (uiRemB < 4) { snprintf(szOut, uiOutLen, "?"); return -1; }
            uiAbs = ((st_u32_t)disasm_rw(pExt) << 16)
                  | (st_u32_t)disasm_rw(pExt + 2);
            snprintf(szOut, uiOutLen, "$%08X", (unsigned)uiAbs);
            return 2;

        case 2:
            if (uiRemB < 2) { snprintf(szOut, uiOutLen, "?"); return -1; }
            uiW   = disasm_rw(pExt);
            iDisp = (st_i32_t)(st_i16_t)uiW;
            uiAbs = (st_u32_t)((st_i32_t)uiEPC + iDisp) & 0x00FFFFFFu;
            snprintf(szOut, uiOutLen, "$%06X(PC)", (unsigned)uiAbs);
            return 1;

        case 3:
            if (uiRemB < 2) { snprintf(szOut, uiOutLen, "?"); return -1; }
            uiW    = disasm_rw(pExt);
            iIAn   = (uiW >> 15) & 1;
            iIReg  = (uiW >> 12) & 7;
            iISz   = (uiW >> 11) & 1;
            iIDisp = (st_i32_t)(st_i8_t)(uiW & 0xFF);
            uiAbs  = (st_u32_t)((st_i32_t)uiEPC + iIDisp) & 0x00FFFFFFu;
            snprintf(szOut, uiOutLen, "$%06X(PC,%s%s)",
                     (unsigned)uiAbs,
                     iIAn ? g_aszAn[iIReg] : g_aszDn[iIReg],
                     iISz ? ".L" : ".W");
            return 1;

        case 4:
            if (iSz == 0)
            {
                if (uiRemB < 2) { snprintf(szOut, uiOutLen, "?"); return -1; }
                uiW = disasm_rw(pExt);
                snprintf(szOut, uiOutLen, "#$%02X",
                         (unsigned)(uiW & 0xFF));
                return 1;
            }
            else if (iSz == 1)
            {
                if (uiRemB < 2) { snprintf(szOut, uiOutLen, "?"); return -1; }
                uiW = disasm_rw(pExt);
                snprintf(szOut, uiOutLen, "#$%04X", (unsigned)uiW);
                return 1;
            }
            else
            {
                if (uiRemB < 4) { snprintf(szOut, uiOutLen, "?"); return -1; }
                uiAbs = ((st_u32_t)disasm_rw(pExt) << 16)
                      | (st_u32_t)disasm_rw(pExt + 2);
                snprintf(szOut, uiOutLen, "#$%08X", (unsigned)uiAbs);
                return 2;
            }

        default:
            snprintf(szOut, uiOutLen, "?EA7%d?", iReg);
            return -1;
        }

    default:
        snprintf(szOut, uiOutLen, "?mode%d?", iMode);
        return -1;
    }
}

/* ------------------------------------------------------------------
 * UC11: MOVE / MOVEA
 * ------------------------------------------------------------------ */

static void disasm_move(const st_u8_t *pBuf, size_t uiBufLen,
                         st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc   = disasm_rw(pBuf);
    int      iSzBits = (uiOpc >> 12) & 3;
    int      iSz;
    int      iSrcMode, iSrcReg, iDstMode, iDstReg;
    int      nSrc, nDst, iExt = 0;
    char     szSrc[40], szDst[40];
    const st_u8_t *pExt = pBuf + 2;

    if      (iSzBits == 1) iSz = 0;
    else if (iSzBits == 3) iSz = 1;
    else                   iSz = 2;

    iSrcMode = (uiOpc >> 3) & 7;
    iSrcReg  = (uiOpc     ) & 7;
    iDstReg  = (uiOpc >> 9) & 7;
    iDstMode = (uiOpc >> 6) & 7;

    if (iSz == 0 && iDstMode == 1)
    { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    nSrc = disasm_fmt_ea(pExt, uiBufLen - 2, uiAddr, iExt,
                          iSrcMode, iSrcReg, iSz, szSrc, sizeof(szSrc));
    if (nSrc < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
    iExt += nSrc;
    pExt += nSrc * 2;

    nDst = disasm_fmt_ea(pExt, uiBufLen - 2 - (size_t)(nSrc * 2),
                          uiAddr, iExt, iDstMode, iDstReg, iSz,
                          szDst, sizeof(szDst));
    if (nDst < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
    iExt += nDst;

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1 + iExt;
    ptR->bValid     = ST_TRUE;

    if (iDstMode == 1)
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                 "MOVEA%s", g_aszSzSfx[iSz]);
    else
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                 "MOVE%s", g_aszSzSfx[iSz]);

    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "%s,%s", szSrc, szDst);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC11: MOVEQ
 * ------------------------------------------------------------------ */

static void disasm_moveq(const st_u8_t *pBuf,
                          st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iDn   = (uiOpc >> 9) & 7;

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "MOVEQ");
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "#$%02X,%s", (unsigned)(uiOpc & 0xFF), g_aszDn[iDn]);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * Shared EA helper: emit "MNEM.sz EA" (one operand)
 * ------------------------------------------------------------------ */

static void disasm_unary_ea(const st_u8_t *pBuf, size_t uiBufLen,
                              st_u32_t uiAddr, const char *szMnem,
                              int iSz, int iMode, int iReg,
                              disasm_result_t *ptR)
{
    char szEA[40];
    int  n;

    n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                       iMode, iReg, iSz, szEA, sizeof(szEA));
    if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = disasm_rw(pBuf);
    ptR->iWordCount = 1 + n;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s%s", szMnem, g_aszSzSfx[iSz]);
    snprintf(ptR->szOperands, sizeof(ptR->szOperands), "%s", szEA);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC11 / UC12: group 0x4 miscellaneous
 * ------------------------------------------------------------------ */

static void disasm_misc4(const st_u8_t *pBuf, size_t uiBufLen,
                          st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iMode, iReg, iAn, iSz, n;
    char     szEA[40];

    /* --- UC12: NEGX  0100 0000 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4000)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        disasm_unary_ea(pBuf, uiBufLen, uiAddr, "NEGX", iSz, iMode, iReg, ptR);
        return;
    }

    /* --- UC11: CLR   0100 0010 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4200)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        disasm_unary_ea(pBuf, uiBufLen, uiAddr, "CLR", iSz, iMode, iReg, ptR);
        return;
    }

    /* --- UC12: NEG   0100 0100 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4400)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        disasm_unary_ea(pBuf, uiBufLen, uiAddr, "NEG", iSz, iMode, iReg, ptR);
        return;
    }

    /* --- UC12: NOT   0100 0110 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4600)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        disasm_unary_ea(pBuf, uiBufLen, uiAddr, "NOT", iSz, iMode, iReg, ptR);
        return;
    }

    /* --- UC11: LEA   0100 aaa1 11 EA -------------------------------- */
    if ((uiOpc & 0xF1C0) == 0x41C0)
    {
        iAn   = (uiOpc >> 9) & 7;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                           iMode, iReg, 2, szEA, sizeof(szEA));
        if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1 + n;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "LEA");
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", szEA, g_aszAn[iAn]);
        disasm_fill_words(ptR, pBuf, uiBufLen);
        disasm_fmt_line(ptR);
        return;
    }

    /* --- UC12: TST   0100 1010 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4A00)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        disasm_unary_ea(pBuf, uiBufLen, uiAddr, "TST", iSz, iMode, iReg, ptR);
        return;
    }

    /* --- UC11: SWAP  0100 1000 0100 0rrr ---------------------------- */
    if ((uiOpc & 0xFFF8) == 0x4840)
    {
        iReg = uiOpc & 7;
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "SWAP");
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s", g_aszDn[iReg]);
        disasm_fmt_line(ptR);
        return;
    }

    /* --- UC12: EXT.W 0100 1000 1000 0rrr ---------------------------- */
    if ((uiOpc & 0xFFF8) == 0x4880)
    {
        iReg = uiOpc & 7;
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "EXT.W");
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s", g_aszDn[iReg]);
        disasm_fmt_line(ptR);
        return;
    }

    /* --- UC12: EXT.L 0100 1000 1100 0rrr ---------------------------- */
    if ((uiOpc & 0xFFF8) == 0x48C0)
    {
        iReg = uiOpc & 7;
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "EXT.L");
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s", g_aszDn[iReg]);
        disasm_fmt_line(ptR);
        return;
    }

    /* --- UC11: PEA   0100 1000 01 EA (after SWAP, EXT checks) ------ */
    if ((uiOpc & 0xFFC0) == 0x4840)
    {
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iMode == 0 || iMode == 1 || iMode == 3 || iMode == 4
        || (iMode == 7 && iReg == 4))
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                           iMode, iReg, 2, szEA, sizeof(szEA));
        if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1 + n;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "PEA");
        snprintf(ptR->szOperands, sizeof(ptR->szOperands), "%s", szEA);
        disasm_fill_words(ptR, pBuf, uiBufLen);
        disasm_fmt_line(ptR);
        return;
    }

    disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
}

/* ------------------------------------------------------------------
 * UC11: EXG
 * ------------------------------------------------------------------ */

static void disasm_exg(const st_u8_t *pBuf,
                        st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iRx   = (uiOpc >> 9) & 7;
    int      iRy   = (uiOpc     ) & 7;

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "EXG");

    if ((uiOpc & 0xF1F8) == 0xC140)
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", g_aszDn[iRx], g_aszDn[iRy]);
    else if ((uiOpc & 0xF1F8) == 0xC148)
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", g_aszAn[iRx], g_aszAn[iRy]);
    else
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", g_aszDn[iRx], g_aszAn[iRy]);

    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: Dn ↔ EA binary ops (ADD, SUB, CMP, AND, OR, EOR)
 *
 * iDir: 0 = <ea>,Dn   1 = Dn,<ea>
 * ------------------------------------------------------------------ */

static void disasm_dreg_ea(const st_u8_t *pBuf, size_t uiBufLen,
                             st_u32_t uiAddr, const char *szMnem,
                             int iDn, int iDir, int iSz,
                             int iMode, int iReg,
                             disasm_result_t *ptR)
{
    char szEA[40];
    int  n;

    n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                       iMode, iReg, iSz, szEA, sizeof(szEA));
    if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = disasm_rw(pBuf);
    ptR->iWordCount = 1 + n;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s%s", szMnem, g_aszSzSfx[iSz]);

    if (iDir == 0) /* <ea>,Dn */
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", szEA, g_aszDn[iDn]);
    else           /* Dn,<ea> */
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", g_aszDn[iDn], szEA);

    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: ADDA / SUBA / CMPA  (An destination)
 * ------------------------------------------------------------------ */

static void disasm_x_a(const st_u8_t *pBuf, size_t uiBufLen,
                        st_u32_t uiAddr, const char *szMnem, int iSz,
                        disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iAn   = (uiOpc >> 9) & 7;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = (uiOpc     ) & 7;
    char     szEA[40];
    int      n;

    n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                       iMode, iReg, iSz, szEA, sizeof(szEA));
    if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1 + n;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s%s", szMnem, g_aszSzSfx[iSz]);
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "%s,%s", szEA, g_aszAn[iAn]);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: ADDQ / SUBQ  (3-bit immediate in opcode)
 * ------------------------------------------------------------------ */

static void disasm_addq_subq(const st_u8_t *pBuf, size_t uiBufLen,
                               st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iData = (uiOpc >> 9) & 7;   /* 0 means 8 */
    int      bSub  = (uiOpc >> 8) & 1;
    int      iSz   = (uiOpc >> 6) & 3;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = (uiOpc     ) & 7;
    char     szEA[40];
    int      n;

    if (iData == 0) iData = 8;
    if (iSz == 3) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                       iMode, iReg, iSz, szEA, sizeof(szEA));
    if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1 + n;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s%s", bSub ? "SUBQ" : "ADDQ", g_aszSzSfx[iSz]);
    /* DEVPAC3 shows ADDQ/SUBQ immediate as decimal */
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "#%d,%s", iData, szEA);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: ADDX / SUBX  (register-to-register or memory predecrement)
 * ------------------------------------------------------------------ */

static void disasm_addx_subx(const st_u8_t *pBuf,
                               st_u32_t uiAddr, const char *szMnem,
                               disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iDst  = (uiOpc >> 9) & 7;
    int      iSz   = (uiOpc >> 6) & 3;
    int      bMem  = (uiOpc >> 3) & 1;  /* 0=Dx,Dy  1=-(Ax),-(Ay) */
    int      iSrc  = (uiOpc     ) & 7;

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s%s", szMnem, g_aszSzSfx[iSz]);

    if (bMem)
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "-(%s),-(%s)", g_aszAn[iSrc], g_aszAn[iDst]);
    else
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", g_aszDn[iSrc], g_aszDn[iDst]);

    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: Immediate → EA  (ADDI, SUBI, CMPI, ANDI, ORI, EORI)
 * ------------------------------------------------------------------ */

static void disasm_imm_ea(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, const char *szMnem,
                           disasm_result_t *ptR)
{
    st_u16_t uiOpc  = disasm_rw(pBuf);
    int      iSz    = (uiOpc >> 6) & 3;
    int      iMode  = (uiOpc >> 3) & 7;
    int      iReg   = (uiOpc     ) & 7;
    int      nImm, nEA;
    char     szImm[32], szDst[40];
    const st_u8_t *pExt = pBuf + 2;

    if (iSz == 3) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    /* Source is always immediate: mode=7, reg=4 */
    nImm = disasm_fmt_ea(pExt, uiBufLen - 2, uiAddr, 0,
                          7, 4, iSz, szImm, sizeof(szImm));
    if (nImm < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
    pExt += nImm * 2;

    /* Destination EA — detect CCR/SR special cases */
    if (iMode == 7 && iReg == 4)
    {
        /* mode=7, reg=4 in dest field = CCR (size B) or SR (size W) */
        nEA = 0;
        snprintf(szDst, sizeof(szDst), "%s",
                 (iSz == 0) ? "CCR" : "SR");
    }
    else
    {
        nEA = disasm_fmt_ea(pExt, uiBufLen - 2 - (size_t)(nImm * 2),
                             uiAddr, nImm, iMode, iReg, iSz,
                             szDst, sizeof(szDst));
        if (nEA < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
    }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1 + nImm + nEA;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s%s", szMnem, g_aszSzSfx[iSz]);
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "%s,%s", szImm, szDst);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: MULU / MULS / DIVU / DIVS  (Dn destination, EA source, word)
 * ------------------------------------------------------------------ */

static void disasm_mul_div(const st_u8_t *pBuf, size_t uiBufLen,
                            st_u32_t uiAddr, const char *szMnem,
                            disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iDn   = (uiOpc >> 9) & 7;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = (uiOpc     ) & 7;
    char     szEA[40];
    int      n;

    /* Source operand is word-sized */
    n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                       iMode, iReg, 1, szEA, sizeof(szEA));
    if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1 + n;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "%s.W", szMnem);
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "%s,%s", szEA, g_aszDn[iDn]);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: CMPM  1011 DDD1 SS 001 SSS  (An)+,(An)+
 * ------------------------------------------------------------------ */

static void disasm_cmpm(const st_u8_t *pBuf,
                         st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iDst  = (uiOpc >> 9) & 7;  /* destination An */
    int      iSz   = (uiOpc >> 6) & 3;
    int      iSrc  = (uiOpc     ) & 7;  /* source An */

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "CMPM%s", g_aszSzSfx[iSz]);
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "(%s)+,(%s)+", g_aszAn[iSrc], g_aszAn[iDst]);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: Group 0x0 — immediate arithmetic
 * ------------------------------------------------------------------ */

static void disasm_group0(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);

    switch ((uiOpc >> 8) & 0x0F)
    {
    case 0x0: disasm_imm_ea(pBuf, uiBufLen, uiAddr, "ORI",  ptR); break;
    case 0x2: disasm_imm_ea(pBuf, uiBufLen, uiAddr, "ANDI", ptR); break;
    case 0x4: disasm_imm_ea(pBuf, uiBufLen, uiAddr, "SUBI", ptR); break;
    case 0x6: disasm_imm_ea(pBuf, uiBufLen, uiAddr, "ADDI", ptR); break;
    case 0xA: disasm_imm_ea(pBuf, uiBufLen, uiAddr, "EORI", ptR); break;
    case 0xC: disasm_imm_ea(pBuf, uiBufLen, uiAddr, "CMPI", ptR); break;
    default:  disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); break;
    }
}

/* ------------------------------------------------------------------
 * UC12: Group 0x5 — ADDQ / SUBQ  (Scc/DBcc → DC.W)
 * ------------------------------------------------------------------ */

static void disasm_group5(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iSz   = (uiOpc >> 6) & 3;

    if (iSz == 3)
    {
        /* Scc / DBcc — not in UC12 scope */
        disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
    }
    else
    {
        disasm_addq_subq(pBuf, uiBufLen, uiAddr, ptR);
    }
}

/* ------------------------------------------------------------------
 * UC12: Group 0x8 — OR / DIVU / DIVS
 * ------------------------------------------------------------------ */

static void disasm_group8(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iSz   = (uiOpc >> 6) & 3;
    int      iDn   = (uiOpc >> 9) & 7;
    int      iDir  = (uiOpc >> 8) & 1;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = (uiOpc     ) & 7;

    if (iSz == 3)
    {
        disasm_mul_div(pBuf, uiBufLen, uiAddr,
                       iDir ? "DIVS" : "DIVU", ptR);
        return;
    }
    /* SBCD (bit8=1, size=00, mode 000 or 001) → DC.W */
    if (iDir == 1 && iSz == 0 && (iMode == 0 || iMode == 1))
    {
        disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
        return;
    }
    disasm_dreg_ea(pBuf, uiBufLen, uiAddr, "OR",
                   iDn, iDir, iSz, iMode, iReg, ptR);
}

/* ------------------------------------------------------------------
 * UC12: Group 0x9 — SUB / SUBA / SUBX
 * ------------------------------------------------------------------ */

static void disasm_group9(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iSz   = (uiOpc >> 6) & 3;
    int      iDn   = (uiOpc >> 9) & 7;
    int      iDir  = (uiOpc >> 8) & 1;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = (uiOpc     ) & 7;

    if (iSz == 3)
    {
        disasm_x_a(pBuf, uiBufLen, uiAddr, "SUBA",
                   iDir ? 2 : 1, ptR);
        return;
    }
    /* SUBX: bit8=1, bits5-4=00 */
    if (iDir == 1 && (iMode == 0 || iMode == 1))
    {
        disasm_addx_subx(pBuf, uiAddr, "SUBX", ptR);
        return;
    }
    disasm_dreg_ea(pBuf, uiBufLen, uiAddr, "SUB",
                   iDn, iDir, iSz, iMode, iReg, ptR);
}

/* ------------------------------------------------------------------
 * UC12: Group 0xB — CMP / CMPA / EOR / CMPM
 * ------------------------------------------------------------------ */

static void disasm_groupB(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iSz   = (uiOpc >> 6) & 3;
    int      iDn   = (uiOpc >> 9) & 7;
    int      iDir  = (uiOpc >> 8) & 1;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = (uiOpc     ) & 7;

    if (iSz == 3)
    {
        disasm_x_a(pBuf, uiBufLen, uiAddr, "CMPA",
                   iDir ? 2 : 1, ptR);
        return;
    }
    if (iDir == 1)
    {
        /* CMPM: bits5-3 = 001 (address postincrement mode) */
        if (iMode == 1)
        {
            disasm_cmpm(pBuf, uiAddr, ptR);
            return;
        }
        disasm_dreg_ea(pBuf, uiBufLen, uiAddr, "EOR",
                       iDn, 1, iSz, iMode, iReg, ptR);
        return;
    }
    disasm_dreg_ea(pBuf, uiBufLen, uiAddr, "CMP",
                   iDn, 0, iSz, iMode, iReg, ptR);
}

/* ------------------------------------------------------------------
 * UC12: Group 0xC — AND / MULU / MULS  (EXG from UC11 kept)
 * ------------------------------------------------------------------ */

static void disasm_groupC(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iSz   = (uiOpc >> 6) & 3;
    int      iDn   = (uiOpc >> 9) & 7;
    int      iDir  = (uiOpc >> 8) & 1;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = (uiOpc     ) & 7;

    /* EXG patterns from UC11 take priority */
    if ((uiOpc & 0xF1F8) == 0xC140
    ||  (uiOpc & 0xF1F8) == 0xC148
    ||  (uiOpc & 0xF1F8) == 0xC188)
    {
        disasm_exg(pBuf, uiAddr, ptR);
        return;
    }
    if (iSz == 3)
    {
        disasm_mul_div(pBuf, uiBufLen, uiAddr,
                       iDir ? "MULS" : "MULU", ptR);
        return;
    }
    /* ABCD (bit8=1, size=00, mode 000/001) → DC.W */
    if (iDir == 1 && iSz == 0 && (iMode == 0 || iMode == 1))
    {
        disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
        return;
    }
    disasm_dreg_ea(pBuf, uiBufLen, uiAddr, "AND",
                   iDn, iDir, iSz, iMode, iReg, ptR);
}

/* ------------------------------------------------------------------
 * UC12: Group 0xD — ADD / ADDA / ADDX
 * ------------------------------------------------------------------ */

static void disasm_groupD(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iSz   = (uiOpc >> 6) & 3;
    int      iDn   = (uiOpc >> 9) & 7;
    int      iDir  = (uiOpc >> 8) & 1;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = (uiOpc     ) & 7;

    if (iSz == 3)
    {
        disasm_x_a(pBuf, uiBufLen, uiAddr, "ADDA",
                   iDir ? 2 : 1, ptR);
        return;
    }
    /* ADDX: bit8=1, mode 000 or 001 */
    if (iDir == 1 && (iMode == 0 || iMode == 1))
    {
        disasm_addx_subx(pBuf, uiAddr, "ADDX", ptR);
        return;
    }
    disasm_dreg_ea(pBuf, uiBufLen, uiAddr, "ADD",
                   iDn, iDir, iSz, iMode, iReg, ptR);
}

/* ------------------------------------------------------------------
 * disasm_one() — main dispatch
 * ------------------------------------------------------------------ */

st_error_t disasm_one(const st_u8_t  *pBuf,
                        size_t          uiBufLen,
                        st_u32_t        uiAddr,
                        disasm_result_t *ptResult)
{
    st_u16_t uiOpc;

    if (pBuf == NULL || ptResult == NULL)
    {
        LOG_ERROR("NULL parameter: pBuf=%p ptResult=%p",
                  (void *)pBuf, (void *)ptResult);
        return ST_ERROR;
    }

    memset(ptResult, 0, sizeof(*ptResult));
    ptResult->uiAddr = uiAddr;

    if (uiBufLen < 2)
    {
        ptResult->iWordCount = 1;
        snprintf(ptResult->szMnemonic, sizeof(ptResult->szMnemonic),
                 "DC.W");
        snprintf(ptResult->szOperands, sizeof(ptResult->szOperands),
                 "???");
        snprintf(ptResult->szLine, DISASM_LINE_MAX,
                 "$%06X:  ????                 DC.W     ???",
                 (unsigned)uiAddr);
        return ST_NO_ERROR;
    }

    uiOpc = disasm_rw(pBuf);

    switch (uiOpc >> 12)
    {
    case 0x0: disasm_group0(pBuf, uiBufLen, uiAddr, ptResult); break;
    case 0x1: /* fall through */
    case 0x2: /* fall through */
    case 0x3: disasm_move  (pBuf, uiBufLen, uiAddr, ptResult); break;
    case 0x4: disasm_misc4 (pBuf, uiBufLen, uiAddr, ptResult); break;
    case 0x5: disasm_group5(pBuf, uiBufLen, uiAddr, ptResult); break;
    case 0x8: disasm_group8(pBuf, uiBufLen, uiAddr, ptResult); break;
    case 0x9: disasm_group9(pBuf, uiBufLen, uiAddr, ptResult); break;

    case 0x7:
        if ((uiOpc & 0x0100) == 0)
            disasm_moveq(pBuf, uiAddr, ptResult);
        else
            disasm_dc_word(pBuf, uiBufLen, uiAddr, ptResult);
        break;

    case 0xB: disasm_groupB(pBuf, uiBufLen, uiAddr, ptResult); break;
    case 0xC: disasm_groupC(pBuf, uiBufLen, uiAddr, ptResult); break;
    case 0xD: disasm_groupD(pBuf, uiBufLen, uiAddr, ptResult); break;

    default:
        disasm_dc_word(pBuf, uiBufLen, uiAddr, ptResult);
        break;
    }

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * disasm_range() — unchanged
 * ------------------------------------------------------------------ */

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

    if (pBuf == NULL || ptResults == NULL || puiLines == NULL)
    {
        LOG_ERROR("NULL parameter: pBuf=%p ptResults=%p puiLines=%p",
                  (void *)pBuf, (void *)ptResults, (void *)puiLines);
        return ST_ERROR;
    }

    uiOffset = 0;
    uiLine   = 0;

    while (uiOffset < uiBufLen && uiLine < uiMaxLines)
    {
        eR = disasm_one(pBuf + uiOffset,
                        uiBufLen - uiOffset,
                        uiAddr + (st_u32_t)uiOffset,
                        &ptResults[uiLine]);
        if (eR != ST_NO_ERROR) return eR;

        uiOffset += (size_t)(ptResults[uiLine].iWordCount * 2);
        uiLine++;
    }

    *puiLines = uiLine;
    return ST_NO_ERROR;
}
