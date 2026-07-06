/*
 * disassemble.c - ST4Ever 68000 disassembler (DEVPAC3 output format)
 *
 * UC11: MOVE/MOVEA/MOVEQ/LEA/PEA/CLR/EXG/SWAP
 * UC12: ADD/ADDA/ADDI/ADDQ/ADDX/SUB/SUBA/SUBI/SUBQ/SUBX/
 *        CMP/CMPA/CMPI/CMPM/MULU/MULS/DIVU/DIVS/
 *        AND/ANDI/OR/ORI/EOR/EORI/NOT/NEG/NEGX/TST/EXT
 * UC13: ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR (register + memory)
 *        BTST/BCHG/BCLR/BSET (static #imm + dynamic Dn)
 * UC14: BRA/BSR/Bcc/JMP/JSR/RTS/RTR/RTE/TRAP/ILLEGAL
 * UC14A: MOVEM.W/L / Scc / DBcc/DBRA / MOVE to-from SR/CCR
 */

#include "disassemble.h"
#include "trace.h"
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------ */

static const char * const g_aszSzSfx[3] = { ".B", ".W", ".L" };

/* UC13: shift/rotate mnemonics [type][direction] */
static const char * const g_aszShiftMnem[4][2] =
{
    { "ASR",  "ASL"  },
    { "LSR",  "LSL"  },
    { "ROXR", "ROXL" },
    { "ROR",  "ROL"  }
};

/* UC13: memory shift mnemonics, indexed by bits 10-8 of opcode */
static const char * const g_aszShiftMemMnem[8] =
{
    "ASR", "ASL", "LSR", "LSL", "ROXR", "ROXL", "ROR", "ROL"
};

/* UC13: bit-operation mnemonics, indexed by bits 7-6 of opcode */
static const char * const g_aszBitOp[4] =
{
    "BTST", "BCHG", "BCLR", "BSET"
};

/* UC14: branch condition mnemonics, indexed by bits 11-8 of group 6 opcode */
static const char * const g_aszBcc[16] =
{
    "BRA", "BSR", "BHI", "BLS", "BCC", "BCS", "BNE", "BEQ",
    "BVC", "BVS", "BPL", "BMI", "BGE", "BLT", "BGT", "BLE"
};

static const char * const g_aszDn[8] =
    { "D0","D1","D2","D3","D4","D5","D6","D7" };
static const char * const g_aszAn[8] =
    { "A0","A1","A2","A3","A4","A5","A6","A7" };

/* UC14A: Scc mnemonics, indexed by bits 11-8 of group 5 opcode (sz=3) */
static const char * const g_aszScc[16] =
{
    "ST",  "SF",  "SHI", "SLS", "SCC", "SCS", "SNE", "SEQ",
    "SVC", "SVS", "SPL", "SMI", "SGE", "SLT", "SGT", "SLE"
};

/* UC14A: DBcc mnemonics — DBRA is the standard alias for DBF (cond=1) */
static const char * const g_aszDBcc[16] =
{
    "DBT",  "DBRA", "DBHI", "DBLS", "DBCC", "DBCS", "DBNE", "DBEQ",
    "DBVC", "DBVS", "DBPL", "DBMI", "DBGE", "DBLT", "DBGT", "DBLE"
};

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
    /* Use signed decimal for negative immediates: MOVEQ always sign-extends
     * the byte to 32 bits, so #$FF → 0xFFFFFFFF.  Outputting #-1 instead
     * of #$FF makes the intent unambiguous and avoids DEVPAC3 sign-extend
     * warnings on ATARI ST.  Positive values keep hex notation. */
    if ((st_i8_t)(uiOpc & 0xFF) < 0)
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "#%d,%s", (int)(st_i8_t)(uiOpc & 0xFF), g_aszDn[iDn]);
    else
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
 * UC14: helper — single-word instruction with no operands
 * (NOP, RTS, RTE, RTR)
 * ------------------------------------------------------------------ */

static void disasm_no_operand(const st_u8_t *pBuf, st_u32_t uiAddr,
                               const char *szMnem, disasm_result_t *ptR)
{
    ptR->uiAddr        = uiAddr;
    ptR->auWords[0]    = disasm_rw(pBuf);
    ptR->iWordCount    = 1;
    ptR->bValid        = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "%s", szMnem);
    ptR->szOperands[0] = '\0';
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * EA extension word count for DC.W fallback — modes 5/6/7.0-7.3 only.
 * Mode 7.4 (#imm) is intentionally excluded: special 1-word opcodes
 * like ILLEGAL ($4AFC) land on mode 7.4 and must stay iWordCount=1.
 * ------------------------------------------------------------------ */

static int disasm_ea_ext_cnt(int iMode, int iReg)
{
    if (iMode == 5) return 1;               /* d16(An)   */
    if (iMode == 6) return 1;               /* d8(An,Xn) */
    if (iMode != 7) return 0;
    if (iReg == 0)  return 1;               /* abs.W     */
    if (iReg == 1)  return 2;               /* abs.L     */
    if (iReg == 2)  return 1;               /* d16(PC)   */
    if (iReg == 3)  return 1;               /* d8(PC,Xn) */
    return 0;  /* iReg==4 (#imm) intentionally excluded */
}

/* ------------------------------------------------------------------
 * UC14A: MOVEM register list formatter
 *
 * uiMask   : 16-bit register mask from the opcode extension word
 * bReverse : 1 for -(An) predecrement (mask bits are reversed)
 * szOut    : receives "D0/D2-D5/A0-A7" style string
 * ------------------------------------------------------------------ */
static void disasm_fmt_reglist(st_u16_t uiMask, int bReverse,
                                char *szOut, size_t uiOutLen)
{
    static const char * const aszReg[16] = {
        "D0","D1","D2","D3","D4","D5","D6","D7",
        "A0","A1","A2","A3","A4","A5","A6","A7"
    };
    int aPresent[16];
    int i, iStart, iPos;
    int bFirst;

    for (i = 0; i < 16; i++)
        aPresent[i] = bReverse ? ((uiMask >> (15 - i)) & 1)
                                : ((uiMask >> i) & 1);

    iPos   = 0;
    bFirst = 1;
    i      = 0;
    while (i < 16)
    {
        if (!aPresent[i]) { i++; continue; }
        iStart = i;
        /* extend run without crossing the D7/A0 boundary */
        while (i < 16 && aPresent[i] && !(iStart <= 7 && i >= 8))
            i++;
        if (!bFirst && iPos < (int)uiOutLen - 1)
            szOut[iPos++] = '/';
        bFirst = 0;
        if (i - iStart == 1)
            iPos += snprintf(szOut + iPos, uiOutLen - (size_t)iPos,
                             "%s", aszReg[iStart]);
        else
            iPos += snprintf(szOut + iPos, uiOutLen - (size_t)iPos,
                             "%s-%s", aszReg[iStart], aszReg[i - 1]);
    }
    if (bFirst)
        snprintf(szOut, uiOutLen, "(none)");
}

/* ------------------------------------------------------------------
 * UC11 / UC12: group 0x4 miscellaneous
 * ------------------------------------------------------------------ */

static void disasm_misc4(const st_u8_t *pBuf, size_t uiBufLen,
                          st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t     uiOpc = disasm_rw(pBuf);
    int          iMode, iReg, iAn, iSz, n;
    int          iVec;
    st_u16_t     uiExt;
    st_i32_t     iDisp;
    const char  *szJmpMnem;
    char         szEA[40];

    /* --- UC12: NEGX  0100 0000 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4000)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3)
        {
            /* UC14A: MOVE.W SR,<ea> */
            n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                              iMode, iReg, 1, szEA, sizeof(szEA));
            if (n < 0)
            { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            ptR->uiAddr     = uiAddr;
            ptR->auWords[0] = uiOpc;
            ptR->iWordCount = 1 + n;
            ptR->bValid     = ST_TRUE;
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "MOVE.W");
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "SR,%s", szEA);
            disasm_fill_words(ptR, pBuf, uiBufLen);
            disasm_fmt_line(ptR);
            return;
        }
        disasm_unary_ea(pBuf, uiBufLen, uiAddr, "NEGX", iSz, iMode, iReg, ptR);
        return;
    }

    /* --- UC11: CLR   0100 0010 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4200)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3)
        {
            /* UC14A: MOVE.W CCR,<ea> (68010+, rare in 68000 code) */
            n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                              iMode, iReg, 1, szEA, sizeof(szEA));
            if (n < 0)
            { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            ptR->uiAddr     = uiAddr;
            ptR->auWords[0] = uiOpc;
            ptR->iWordCount = 1 + n;
            ptR->bValid     = ST_TRUE;
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "MOVE.W");
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "CCR,%s", szEA);
            disasm_fill_words(ptR, pBuf, uiBufLen);
            disasm_fmt_line(ptR);
            return;
        }
        disasm_unary_ea(pBuf, uiBufLen, uiAddr, "CLR", iSz, iMode, iReg, ptR);
        return;
    }

    /* --- UC12: NEG   0100 0100 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4400)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3)
        {
            /* UC14A: MOVE.W <ea>,CCR */
            if (iMode == 1)
            { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                              iMode, iReg, 1, szEA, sizeof(szEA));
            if (n < 0)
            { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            ptR->uiAddr     = uiAddr;
            ptR->auWords[0] = uiOpc;
            ptR->iWordCount = 1 + n;
            ptR->bValid     = ST_TRUE;
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "MOVE.W");
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s,CCR", szEA);
            disasm_fill_words(ptR, pBuf, uiBufLen);
            disasm_fmt_line(ptR);
            return;
        }
        disasm_unary_ea(pBuf, uiBufLen, uiAddr, "NEG", iSz, iMode, iReg, ptR);
        return;
    }

    /* --- UC12: NOT   0100 0110 ss EA -------------------------------- */
    if ((uiOpc & 0xFF00) == 0x4600)
    {
        iSz   = (uiOpc >> 6) & 3;
        iMode = (uiOpc >> 3) & 7;
        iReg  = (uiOpc     ) & 7;
        if (iSz == 3)
        {
            /* UC14A: MOVE.W <ea>,SR */
            if (iMode == 1)
            { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                              iMode, iReg, 1, szEA, sizeof(szEA));
            if (n < 0)
            { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            ptR->uiAddr     = uiAddr;
            ptR->auWords[0] = uiOpc;
            ptR->iWordCount = 1 + n;
            ptR->bValid     = ST_TRUE;
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "MOVE.W");
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s,SR", szEA);
            disasm_fill_words(ptR, pBuf, uiBufLen);
            disasm_fmt_line(ptR);
            return;
        }
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
        if (iSz == 3)
        {
            /* UC14B: TAS — 0100 1010 11 EA — byte, no size suffix
             * An (mode 1) invalid; #imm (mode7,reg4) = ILLEGAL → DC.W */
            if (iMode == 1 || (iMode == 7 && iReg == 4))
            { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                               iMode, iReg, 0, szEA, sizeof(szEA));
            if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            ptR->uiAddr     = uiAddr;
            ptR->auWords[0] = uiOpc;
            ptR->iWordCount = 1 + n;
            ptR->bValid     = ST_TRUE;
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "TAS");
            snprintf(ptR->szOperands, sizeof(ptR->szOperands), "%s", szEA);
            disasm_fill_words(ptR, pBuf, uiBufLen);
            disasm_fmt_line(ptR);
            return;
        }
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

    /* --- UC14: 0x4Exx — NOP/RTE/RTS/RTR/TRAP/LINK/UNLK/JSR/JMP --- */
    if ((uiOpc & 0xFF00) == 0x4E00)
    {
        if      (uiOpc == 0x4E71)
            { disasm_no_operand(pBuf, uiAddr, "NOP", ptR); }
        else if (uiOpc == 0x4E73)
            { disasm_no_operand(pBuf, uiAddr, "RTE", ptR); }
        else if (uiOpc == 0x4E75)
            { disasm_no_operand(pBuf, uiAddr, "RTS", ptR); }
        else if (uiOpc == 0x4E77)
            { disasm_no_operand(pBuf, uiAddr, "RTR", ptR); }
        else if ((uiOpc & 0xFFF0) == 0x4E40)
        {
            /* TRAP #N (N = 0-15) */
            iVec = uiOpc & 0xF;
            ptR->uiAddr     = uiAddr;
            ptR->auWords[0] = uiOpc;
            ptR->iWordCount = 1;
            ptR->bValid     = ST_TRUE;
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "TRAP");
            snprintf(ptR->szOperands, sizeof(ptR->szOperands), "#%d", iVec);
            disasm_fmt_line(ptR);
        }
        else if ((uiOpc & 0xFFF8) == 0x4E50)
        {
            /* LINK An,#disp16 */
            if (uiBufLen < 4)
                { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
            iAn   = uiOpc & 7;
            uiExt = disasm_rw(pBuf + 2);
            iDisp = (st_i32_t)(st_i16_t)uiExt;
            ptR->uiAddr     = uiAddr;
            ptR->auWords[0] = uiOpc;
            ptR->iWordCount = 2;
            ptR->bValid     = ST_TRUE;
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "LINK");
            if (iDisp < 0)
                snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                         "%s,#-$%X", g_aszAn[iAn], (unsigned)(-iDisp));
            else
                snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                         "%s,#$%X", g_aszAn[iAn], (unsigned)iDisp);
            disasm_fill_words(ptR, pBuf, uiBufLen);
            disasm_fmt_line(ptR);
        }
        else if ((uiOpc & 0xFFF8) == 0x4E58)
        {
            /* UNLK An */
            iAn = uiOpc & 7;
            ptR->uiAddr     = uiAddr;
            ptR->auWords[0] = uiOpc;
            ptR->iWordCount = 1;
            ptR->bValid     = ST_TRUE;
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "UNLK");
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s", g_aszAn[iAn]);
            disasm_fmt_line(ptR);
        }
        else if ((uiOpc & 0xFFC0) == 0x4E80
             ||  (uiOpc & 0xFFC0) == 0x4EC0)
        {
            /* JSR / JMP <ea> */
            szJmpMnem = ((uiOpc & 0xFFC0) == 0x4E80) ? "JSR" : "JMP";
            iMode = (uiOpc >> 3) & 7;
            iReg  = uiOpc & 7;
            /* Reject non-control EAs: Dn, An, (An)+, -(An), #imm */
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
            snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                     "%s", szJmpMnem);
            snprintf(ptR->szOperands, sizeof(ptR->szOperands), "%s", szEA);
            disasm_fill_words(ptR, pBuf, uiBufLen);
            disasm_fmt_line(ptR);
        }
        else if (uiOpc == 0x4E72)  /* STOP #imm — opcode + SR value = 2 words */
        {
            disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
            if (uiBufLen >= 4)
            {
                ptR->iWordCount = 2;
                disasm_fill_words(ptR, pBuf, uiBufLen);
                disasm_fmt_line(ptR);
            }
        }
        else
        {
            disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
        }
        return;
    }

    /* UC14A: MOVEM — 0100 1d00 1s MMMRRR + register-mask + optional EA-ext.
     * EXT.W ($4880-$4887) and EXT.L ($48C0-$48C7) are caught above.
     * Any remaining $48xx/$4Cxx with bit 7 set are MOVEM.
     * d=0: store (reglist → EA), d=1: load (EA → reglist)
     * s=0: .W,  s=1: .L
     */
    if ((uiOpc & 0xFF80) == 0x4880 || (uiOpc & 0xFF80) == 0x4C80)
    {
        int      iMvmDir  = (uiOpc >> 10) & 1; /* 0=store,  1=load  */
        int      iMvmSz   = (uiOpc >>  6) & 1; /* 0=.W,     1=.L    */
        int      iMvmMode = (uiOpc >>  3) & 7;
        int      iMvmReg  = (uiOpc       ) & 7;
        st_u16_t uiMask;
        char     szRL[64];
        int      nEA;

        /* Reject Dn (0), An (1), #imm (7.4) */
        if (iMvmMode == 0 || iMvmMode == 1
        || (iMvmMode == 7 && iMvmReg == 4))
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        /* Store: (An)+ not valid; Load: -(An) not valid */
        if (!iMvmDir && iMvmMode == 3)
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        if (iMvmDir && iMvmMode == 4)
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        if (uiBufLen < 4)
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

        uiMask = disasm_rw(pBuf + 2);
        /* Mask is reversed in bit order for -(An) predecrement store */
        disasm_fmt_reglist(uiMask, (!iMvmDir && iMvmMode == 4),
                           szRL, sizeof(szRL));

        nEA = disasm_fmt_ea(pBuf + 4, uiBufLen - 4, uiAddr, 1,
                            iMvmMode, iMvmReg,
                            iMvmSz ? 2 : 1, szEA, sizeof(szEA));
        if (nEA < 0)
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->auWords[1] = uiMask;
        ptR->iWordCount = 2 + nEA;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                 "MOVEM.%c", iMvmSz ? 'L' : 'W');
        if (!iMvmDir)   /* store: reglist,<ea> */
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s,%s", szRL, szEA);
        else            /* load:  <ea>,reglist */
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s,%s", szEA, szRL);
        disasm_fill_words(ptR, pBuf, uiBufLen);
        disasm_fmt_line(ptR);
        return;
    }
    /* UC14B: NBCD — 0100 1000 00 EA — byte, no size suffix
     * An (mode 1) and #imm (mode7,reg4) invalid */
    if ((uiOpc & 0xFFC0) == 0x4800)
    {
        iMode = (uiOpc >> 3) & 7;
        iReg  = uiOpc & 7;
        if (iMode == 1 || (iMode == 7 && iReg == 4))
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                           iMode, iReg, 0, szEA, sizeof(szEA));
        if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1 + n;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "NBCD");
        snprintf(ptR->szOperands, sizeof(ptR->szOperands), "%s", szEA);
        disasm_fill_words(ptR, pBuf, uiBufLen);
        disasm_fmt_line(ptR);
        return;
    }

    /* UC14B: CHK.W — 0100 DDD 1 10 EA — mode 1 (An) invalid source */
    if ((uiOpc & 0xF1C0) == 0x4180)
    {
        iAn   = (uiOpc >> 9) & 7;  /* destination Dn */
        iMode = (uiOpc >> 3) & 7;
        iReg  = uiOpc & 7;
        if (iMode == 1)
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                           iMode, iReg, 1, szEA, sizeof(szEA));
        if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1 + n;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "CHK.W");
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", szEA, g_aszDn[iAn]);
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
        /* DEVPAC3 convention: first An operand is in iRy (bits 2-0) */
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", g_aszAn[iRy], g_aszAn[iRx]);
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
 * UC14: Group 0x6 — BRA/BSR/Bcc
 *
 * Encoding: 0110 cccc dddddddd
 *   cccc = condition (0=BRA, 1=BSR, 2-15=Bcc)
 *   dddd = 8-bit displacement (0 → 16-bit extension; 0xFF → DC.W/68020)
 *
 * Target = uiAddr + 2 + displacement
 * Short form (8-bit ≠ 0): mnemonic with ".S" suffix
 * Long form  (8-bit = 0): mnemonic without suffix, 16-bit ext word
 * ------------------------------------------------------------------ */

static void disasm_group6(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc   = disasm_rw(pBuf);
    int      iCond   = (uiOpc >> 8) & 0xF;
    int      iDisp8  = (int)(st_i8_t)(uiOpc & 0xFF);
    st_u16_t uiExt;
    st_i32_t iDisp16;
    st_u32_t uiTarget;

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->bValid     = ST_TRUE;

    if ((uiOpc & 0xFF) == 0xFF)
    {
        /* 32-bit displacement (68020+ only) — not valid on 68000 */
        disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
        return;
    }

    if ((uiOpc & 0xFF) == 0x00)
    {
        /* 16-bit displacement in extension word */
        if (uiBufLen < 4) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        uiExt    = disasm_rw(pBuf + 2);
        iDisp16  = (st_i32_t)(st_i16_t)uiExt;
        uiTarget = (st_u32_t)((st_i32_t)(uiAddr + 2) + iDisp16) & 0x00FFFFFFu;
        ptR->iWordCount = 2;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                 "%s", g_aszBcc[iCond]);
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "$%06X", (unsigned)uiTarget);
    }
    else
    {
        /* 8-bit short displacement */
        uiTarget = (st_u32_t)((st_i32_t)(uiAddr + 2) + iDisp8) & 0x00FFFFFFu;
        ptR->iWordCount = 1;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                 "%s.S", g_aszBcc[iCond]);
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "$%06X", (unsigned)uiTarget);
    }

    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC13: BTST/BCHG/BCLR/BSET — static (#imm) form
 *
 * Encoding: 0000 1000 tt MMMRRR  + extension word #bit
 * tt = bits 7-6: 00=BTST, 01=BCHG, 10=BCLR, 11=BSET
 * ------------------------------------------------------------------ */

static void disasm_bit_static(const st_u8_t *pBuf, size_t uiBufLen,
                               st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iOp   = (uiOpc >> 6) & 3;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = uiOpc & 7;
    st_u16_t uiBitW;
    int      iBit, n;
    char     szEA[40];

    if (uiBufLen < 4) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
    /* An mode not valid for bit ops */
    if (iMode == 1) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    uiBitW = disasm_rw(pBuf + 2);
    iBit   = (int)(uiBitW & 0xFF);

    n = disasm_fmt_ea(pBuf + 4, uiBufLen - 4, uiAddr, 1,
                       iMode, iReg, 1, szEA, sizeof(szEA));
    if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->auWords[1] = uiBitW;
    ptR->iWordCount = 2 + n;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s", g_aszBitOp[iOp]);
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "#%d,%s", iBit, szEA);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC13: BTST/BCHG/BCLR/BSET — dynamic (Dn) form
 *
 * Encoding: 0000 DDD 1 tt MMMRRR
 * tt = bits 7-6, DDD = bits 11-9
 * ------------------------------------------------------------------ */

static void disasm_bit_dynamic(const st_u8_t *pBuf, size_t uiBufLen,
                                st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iDn   = (uiOpc >> 9) & 7;
    int      iOp   = (uiOpc >> 6) & 3;
    int      iMode = (uiOpc >> 3) & 7;
    int      iReg  = uiOpc & 7;
    char     szEA[40];
    int      n;

    /* UC14B: MOVEP — 0000 DDD 1 d s 001 AAA + d16
     * bit 7 (iOp>>1): direction 0=mem→reg, 1=reg→mem
     * bit 6 (iOp&1):  size 0=word, 1=long */
    if (iMode == 1)
    {
        int      iDir16 = (iOp >> 1) & 1;
        int      iSzM   = iOp & 1;
        st_i16_t iDisp16;
        char     szDisp[32];
        if (uiBufLen < 4)
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        iDisp16 = (st_i16_t)disasm_rw(pBuf + 2);
        if (iDisp16 < 0)
            snprintf(szDisp, sizeof(szDisp), "-$%X(%s)",
                     (unsigned)(-(int)iDisp16), g_aszAn[iReg]);
        else
            snprintf(szDisp, sizeof(szDisp), "$%X(%s)",
                     (unsigned)(int)iDisp16, g_aszAn[iReg]);
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 2;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                 "MOVEP.%c", iSzM ? 'L' : 'W');
        if (iDir16 == 0)
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s,%s", szDisp, g_aszDn[iDn]);
        else
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s,%s", g_aszDn[iDn], szDisp);
        disasm_fill_words(ptR, pBuf, uiBufLen);
        disasm_fmt_line(ptR);
        return;
    }

    n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                       iMode, iReg, 1, szEA, sizeof(szEA));
    if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1 + n;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s", g_aszBitOp[iOp]);
    snprintf(ptR->szOperands, sizeof(ptR->szOperands),
             "%s,%s", g_aszDn[iDn], szEA);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC13: Group 0xE — shifts and rotations
 *
 * Register form: 1110 cccc d ss i tt rrr   (bits 7-6 ≠ 11)
 *   cccc = count (if i=1) or source Dn (if i=0)
 *   d    = direction (bit 8): 0=right, 1=left
 *   ss   = size: 00=B, 01=W, 10=L
 *   i    = 1=immediate count, 0=register count (bit 5)
 *   tt   = type: 00=AS, 01=LS, 10=ROX, 11=RO (bits 4-3)
 *   rrr  = destination Dn
 *
 * Memory form: 1110 cccc 11 EA   (bits 7-6 = 11)
 *   bits 10-8 = index into g_aszShiftMemMnem[]
 * ------------------------------------------------------------------ */

static void disasm_groupE(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc = disasm_rw(pBuf);
    int      iSz   = (uiOpc >> 6) & 3;
    int      iIdx, iDir, iType, iIR, iCount, iDn;
    int      iMode, iReg, n;
    char     szEA[40];

    if (iSz == 3)
    {
        /* Memory shift: bits 10-8 select mnemonic; word size; count=1 */
        iIdx = (uiOpc >> 8) & 7;
        if ((uiOpc >> 11) & 1)
        {
            /* bit 11 set → not a valid memory shift encoding */
            disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
            return;
        }
        iMode = (uiOpc >> 3) & 7;
        iReg  = uiOpc & 7;
        /* Reject Dn, An, PC-relative, immediate */
        if (iMode == 0 || iMode == 1
        || (iMode == 7 && iReg >= 2))
        {
            disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
            return;
        }
        n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                           iMode, iReg, 1, szEA, sizeof(szEA));
        if (n < 0) { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1 + n;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                 "%s.W", g_aszShiftMemMnem[iIdx]);
        snprintf(ptR->szOperands, sizeof(ptR->szOperands), "%s", szEA);
        disasm_fill_words(ptR, pBuf, uiBufLen);
        disasm_fmt_line(ptR);
        return;
    }

    /* Register shift */
    iCount = (uiOpc >> 9) & 7;
    iDir   = (uiOpc >> 8) & 1;
    iIR    = (uiOpc >> 5) & 1;
    iType  = (uiOpc >> 3) & 3;
    iDn    = uiOpc & 7;

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s%s", g_aszShiftMnem[iType][iDir], g_aszSzSfx[iSz]);

    if (iIR)
    {
        /* Register mode (i=1): count is in D{iCount} */
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,%s", g_aszDn[iCount], g_aszDn[iDn]);
    }
    else
    {
        /* Immediate mode (i=0): 3-bit count, 0 encodes as 8 */
        if (iCount == 0) iCount = 8;
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "#%d,%s", iCount, g_aszDn[iDn]);
    }
    disasm_fmt_line(ptR);
}

/* ------------------------------------------------------------------
 * UC12: Group 0x0 — immediate arithmetic
 * UC13: extended with BTST/BCHG/BCLR/BSET (static and dynamic)
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
    case 0x8: disasm_bit_static(pBuf, uiBufLen, uiAddr, ptR);     break;
    case 0xA: disasm_imm_ea(pBuf, uiBufLen, uiAddr, "EORI", ptR); break;
    case 0xC: disasm_imm_ea(pBuf, uiBufLen, uiAddr, "CMPI", ptR); break;
    default:
        /* Odd values (bit 8 = 1): dynamic bit ops Dn */
        if ((uiOpc >> 8) & 1)
            disasm_bit_dynamic(pBuf, uiBufLen, uiAddr, ptR);
        else
            disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR);
        break;
    }
}

/* ------------------------------------------------------------------
 * UC12/UC14A: Group 0x5 — ADDQ / SUBQ / Scc / DBcc / DBRA
 * ------------------------------------------------------------------ */

static void disasm_group5(const st_u8_t *pBuf, size_t uiBufLen,
                           st_u32_t uiAddr, disasm_result_t *ptR)
{
    st_u16_t uiOpc  = disasm_rw(pBuf);
    int      iCond  = (uiOpc >> 8) & 0xF;
    int      iSz    = (uiOpc >> 6) & 3;
    int      iMode  = (uiOpc >> 3) & 7;
    int      iReg   = (uiOpc     ) & 7;
    st_i16_t iDisp16;
    st_u32_t uiTarget;
    char     szEA[40];
    int      n;

    if (iSz != 3)
    {
        disasm_addq_subq(pBuf, uiBufLen, uiAddr, ptR);
        return;
    }

    /* --- UC14A: DBcc — mode 001 (Dn direct) -------------------------- */
    if (iMode == 1)
    {
        if (uiBufLen < 4)
        { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }
        iDisp16  = (st_i16_t)disasm_rw(pBuf + 2);
        uiTarget = (st_u32_t)((st_i32_t)(uiAddr + 2) + (st_i32_t)iDisp16)
                   & 0x00FFFFFFu;
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 2;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
                 "%s", g_aszDBcc[iCond]);
        snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                 "%s,$%06X", g_aszDn[iReg], (unsigned)uiTarget);
        disasm_fill_words(ptR, pBuf, uiBufLen);
        disasm_fmt_line(ptR);
        return;
    }

    /* --- UC14A: Scc — byte set-on-condition -------------------------  */
    /* #imm (mode 7, reg 4) is not a valid destination for Scc */
    if (iMode == 7 && iReg == 4)
    { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    n = disasm_fmt_ea(pBuf + 2, uiBufLen - 2, uiAddr, 0,
                      iMode, iReg, 0, szEA, sizeof(szEA));
    if (n < 0)
    { disasm_dc_word(pBuf, uiBufLen, uiAddr, ptR); return; }

    ptR->uiAddr     = uiAddr;
    ptR->auWords[0] = uiOpc;
    ptR->iWordCount = 1 + n;
    ptR->bValid     = ST_TRUE;
    snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic),
             "%s", g_aszScc[iCond]);
    snprintf(ptR->szOperands, sizeof(ptR->szOperands), "%s", szEA);
    disasm_fill_words(ptR, pBuf, uiBufLen);
    disasm_fmt_line(ptR);
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
    /* UC14B: SBCD — bit8=1, sz=00, mode 000 (Dn,Dn) or 001 (-(An),-(An)) */
    if (iDir == 1 && iSz == 0 && (iMode == 0 || iMode == 1))
    {
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "SBCD");
        if (iMode == 0)
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s,%s", g_aszDn[iReg], g_aszDn[iDn]);
        else
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "-(%s),-(%s)", g_aszAn[iReg], g_aszAn[iDn]);
        disasm_fmt_line(ptR);
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
    /* UC14B: ABCD — bit8=1, sz=00, mode 000 (Dn,Dn) or 001 (-(An),-(An)) */
    if (iDir == 1 && iSz == 0 && (iMode == 0 || iMode == 1))
    {
        ptR->uiAddr     = uiAddr;
        ptR->auWords[0] = uiOpc;
        ptR->iWordCount = 1;
        ptR->bValid     = ST_TRUE;
        snprintf(ptR->szMnemonic, sizeof(ptR->szMnemonic), "ABCD");
        if (iMode == 0)
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "%s,%s", g_aszDn[iReg], g_aszDn[iDn]);
        else
            snprintf(ptR->szOperands, sizeof(ptR->szOperands),
                     "-(%s),-(%s)", g_aszAn[iReg], g_aszAn[iDn]);
        disasm_fmt_line(ptR);
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
    case 0x6: disasm_group6(pBuf, uiBufLen, uiAddr, ptResult); break;
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
    case 0xE: disasm_groupE(pBuf, uiBufLen, uiAddr, ptResult); break;

    default:
        disasm_dc_word(pBuf, uiBufLen, uiAddr, ptResult);
        break;
    }

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * disasm_range() — unchanged
 * ------------------------------------------------------------------ */

st_error_t disasm_range(const st_u8_t   *pBuf,
                        size_t           uiBufLen,
                        st_u32_t         uiAddr,
                        disasm_result_t *ptResults,
                        size_t           uiMaxLines,
                        size_t          *puiLines)
{
    size_t     uiOffset;
    size_t     uiLine;
    st_error_t eR;

    /* -- [DISAMS]1. Reject any NULL pointer in parameter with ST_ERROR -- */
    if (pBuf == NULL || ptResults == NULL || puiLines == NULL)
    {
        LOG_ERROR("NULL parameter: pBuf=%p ptResults=%p puiLines=%p",
                  (void *)pBuf, (void *)ptResults, (void *)puiLines);
        return ST_ERROR;
    }

    uiOffset = 0;
    uiLine   = 0;

    /* -- [DISAMS]2. Disassemble each instruction until end of buffer -- */
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
