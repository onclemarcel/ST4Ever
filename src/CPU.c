/*
 * CPU.c - MC68000 CPU emulator
 *
 * UC21: MOVE.B/W/L, MOVEA.W/L, MOVEQ, LEA, CLR.B/W/L, SWAP Dn
 * UC22: ADD/SUB/CMP/AND/OR/EOR/shifts + NEG/NEGX/NOT/TST/EXT +
 *        ADDI/SUBI/CMPI/ANDI/ORI/EORI + ADDQ/SUBQ +
 *        ADDX/SUBX + MULU/MULS/DIVU/DIVS
 * UC23: BRA/BSR/Bcc + NOP/STOP/RTE/RTS/RTR/TRAP/LINK/UNLK/JSR/JMP +
 *        Scc/DBcc + cpu_raise_exception (exception frame stacking)
 * UC28: case 0xA -> linea_dispatch(); case 0xF -> cpu_raise_exception(LINE_F)
 * UC29: TRAP #1 -> tos_gemdos(); TRAP #14 -> tos_xbios()
 */
#include "CPU.h"
#include "linea.h"
#include "tos.h"
#include "trace.h"
#include <string.h>

/* ------------------------------------------------------------------
 * Size constants
 * ------------------------------------------------------------------ */

#define SZ_BYTE 0
#define SZ_WORD 1
#define SZ_LONG 2

static const st_u32_t g_auiMask[3] = { 0xFFu, 0xFFFFu, 0xFFFFFFFFu };
static const st_u32_t g_auiMsb[3]  = { 0x80u, 0x8000u, 0x80000000u };

/* ------------------------------------------------------------------
 * Global CPU Context - there is only one CPU in the ATARI ST
 * ------------------------------------------------------------------ */

 static cpu_context_t g_cpu_ptCtx = {.ulMagic = 0xCAFEDECA,
                                     .eObject = ST_CPU_CTX};

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/*
 * cpu_fetch_word() - Read word at PC and advance PC by 2.
 * @req REQ-CPU-008
 */
static st_u16_t cpu_fetch_word()
{
    st_u16_t   uiW;
    st_error_t eR;

    eR = st_read_word(g_cpu_ptCtx.uiPC, &uiW);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("bus error at PC=0x%06X", g_cpu_ptCtx.uiPC);
        uiW = 0xFFFFu;
    }
    g_cpu_ptCtx.uiPC += 2u;
    return uiW;
}

/*
 * cpu_flags_nz() - Update N and Z flags from masked result.
 * @req REQ-CPU-021, REQ-CPU-022, REQ-CPU-023
 */
static void cpu_flags_nz(st_u32_t uiVal, int iSz)
{
    st_u16_t uiSR = g_cpu_ptCtx.uiSR;

    uiVal &= g_auiMask[iSz];
    if (uiVal == 0u)
        uiSR |= (st_u16_t)CPU_SR_Z;
    else
        uiSR &= (st_u16_t)~CPU_SR_Z;

    if (uiVal & g_auiMsb[iSz])
        uiSR |= (st_u16_t)CPU_SR_N;
    else
        uiSR &= (st_u16_t)~CPU_SR_N;

    g_cpu_ptCtx.uiSR = uiSR;
}

/*
 * cpu_flags_clr_vc() - Clear V and C flags.
 * @req REQ-CPU-021
 */
static void cpu_flags_clr_vc()
{
    g_cpu_ptCtx.uiSR &= (st_u16_t)~(CPU_SR_V | CPU_SR_C);
}

/*
 * cpu_flags_add() - Set N/Z/V/C/X for addition (dst + src = res).
 * @req REQ-CPU-022
 *
 * C = unsigned carry out; V = signed overflow; X = C.
 */
static void cpu_flags_add( st_u32_t  uiSrc,
                           st_u32_t  uiDst,
                           st_u32_t  uiRes,
                           int       iSz)
{
    st_u32_t uiMsb  = g_auiMsb[iSz];
    st_u32_t uiMask = g_auiMask[iSz];
    st_u16_t uiSR   = g_cpu_ptCtx.uiSR;

    uiSrc &= uiMask;
    uiDst &= uiMask;
    uiRes &= uiMask;

    /* N and Z */
    if (uiRes == 0u)
        uiSR |=  (st_u16_t)CPU_SR_Z;
    else
        uiSR &= (st_u16_t)~CPU_SR_Z;
    if (uiRes & uiMsb)
        uiSR |=  (st_u16_t)CPU_SR_N;
    else
        uiSR &= (st_u16_t)~CPU_SR_N;

    /* C: carry out — result smaller than either operand (unsigned) */
    if (uiRes < uiSrc || uiRes < uiDst)
        uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
    else
        uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);

    /* V: signed overflow — both operands same sign, result different */
    if (!((uiSrc ^ uiDst) & uiMsb) && ((uiRes ^ uiSrc) & uiMsb))
        uiSR |=  (st_u16_t)CPU_SR_V;
    else
        uiSR &= (st_u16_t)~CPU_SR_V;

    g_cpu_ptCtx.uiSR = uiSR;
}

/*
 * cpu_flags_sub() - Set N/Z/V/C/X for subtraction (dst - src = res).
 * @req REQ-CPU-023
 *
 * C = borrow (src > dst unsigned); V = signed overflow; X = C.
 */
static void cpu_flags_sub( st_u32_t  uiSrc,
                           st_u32_t  uiDst,
                           st_u32_t  uiRes,
                           int       iSz)
{
    st_u32_t uiMsb  = g_auiMsb[iSz];
    st_u32_t uiMask = g_auiMask[iSz];
    st_u16_t uiSR   = g_cpu_ptCtx.uiSR;

    uiSrc &= uiMask;
    uiDst &= uiMask;
    uiRes &= uiMask;

    /* N and Z */
    if (uiRes == 0u)
        uiSR |=  (st_u16_t)CPU_SR_Z;
    else
        uiSR &= (st_u16_t)~CPU_SR_Z;
    if (uiRes & uiMsb)
        uiSR |=  (st_u16_t)CPU_SR_N;
    else
        uiSR &= (st_u16_t)~CPU_SR_N;

    /* C: borrow — source larger than dest (unsigned) */
    if (uiSrc > uiDst)
        uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
    else
        uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);

    /* V: signed overflow — operands different signs, result sign != dst */
    if (((uiSrc ^ uiDst) & uiMsb) && ((uiRes ^ uiDst) & uiMsb))
        uiSR |=  (st_u16_t)CPU_SR_V;
    else
        uiSR &= (st_u16_t)~CPU_SR_V;

    g_cpu_ptCtx.uiSR = uiSR;
}

/* ------------------------------------------------------------------
 * Stack push / pop helpers (use active A7 as stack pointer)
 * ------------------------------------------------------------------ */

/*
 * cpu_push_long() - Pre-decrement A7 by 4 and write long-word to stack.
 * @req REQ-CPU-040
 */
static st_error_t cpu_push_long( st_u32_t      uiVal)
{
    g_cpu_ptCtx.auAn[7] -= 4u;
    return st_write_long(g_cpu_ptCtx.auAn[7] & 0x00FFFFFFu, uiVal);
}

/*
 * cpu_push_word() - Pre-decrement A7 by 2 and write word to stack.
 * @req REQ-CPU-040
 */
static st_error_t cpu_push_word( st_u16_t      uiVal)
{
    g_cpu_ptCtx.auAn[7] -= 2u;
    return st_write_word(g_cpu_ptCtx.auAn[7] & 0x00FFFFFFu, uiVal);
}

/*
 * cpu_pop_long() - Read long-word from stack and post-increment A7 by 4.
 * @req REQ-CPU-035, REQ-CPU-036
 */
static st_error_t cpu_pop_long( st_u32_t     *puiVal)
{
    st_error_t eR;
    eR = st_read_long(g_cpu_ptCtx.auAn[7] & 0x00FFFFFFu, puiVal);
    if (eR == ST_NO_ERROR)
        g_cpu_ptCtx.auAn[7] += 4u;
    return eR;
}

/*
 * cpu_pop_word() - Read word from stack and post-increment A7 by 2.
 * @req REQ-CPU-035, REQ-CPU-036
 */
static st_error_t cpu_pop_word( st_u16_t     *puiVal)
{
    st_error_t eR;
    eR = st_read_word(g_cpu_ptCtx.auAn[7] & 0x00FFFFFFu, puiVal);
    if (eR == ST_NO_ERROR)
        g_cpu_ptCtx.auAn[7] += 2u;
    return eR;
}

/* ------------------------------------------------------------------
 * Condition code evaluator  (Bcc / Scc / DBcc)
 * ------------------------------------------------------------------ */

/*
 * cpu_eval_cc() - Evaluate a 68000 condition code against SR flags.
 * @req REQ-CPU-033, REQ-CPU-043, REQ-CPU-044
 */
static st_bool_t cpu_eval_cc(int iCc)
{
    int bN = CPU_FLAG_N(g_cpu_ptCtx.uiSR) ? 1 : 0;
    int bZ = CPU_FLAG_Z(g_cpu_ptCtx.uiSR) ? 1 : 0;
    int bV = CPU_FLAG_V(g_cpu_ptCtx.uiSR) ? 1 : 0;
    int bC = CPU_FLAG_C(g_cpu_ptCtx.uiSR) ? 1 : 0;

    switch (iCc & 0xF)
    {
    case  0: return ST_TRUE;                            /* T  */
    case  1: return ST_FALSE;                           /* F  */
    case  2: return (st_bool_t)(!bC && !bZ);            /* HI */
    case  3: return (st_bool_t)( bC ||  bZ);            /* LS */
    case  4: return (st_bool_t)(!bC);                   /* CC/HS */
    case  5: return (st_bool_t)( bC);                   /* CS/LO */
    case  6: return (st_bool_t)(!bZ);                   /* NE */
    case  7: return (st_bool_t)( bZ);                   /* EQ */
    case  8: return (st_bool_t)(!bV);                   /* VC */
    case  9: return (st_bool_t)( bV);                   /* VS */
    case 10: return (st_bool_t)(!bN);                   /* PL */
    case 11: return (st_bool_t)( bN);                   /* MI */
    case 12: return (st_bool_t)(bN == bV);              /* GE */
    case 13: return (st_bool_t)(bN != bV);              /* LT */
    case 14: return (st_bool_t)(!bZ && (bN == bV));     /* GT */
    default: return (st_bool_t)( bZ || (bN != bV));     /* LE */
    }
}

/* ------------------------------------------------------------------
 * EA decoder
 * ------------------------------------------------------------------ */

/*
 * cpu_ea_read() - Decode EA, consume extension words, return value.
 *
 * @req REQ-CPU-012, REQ-CPU-013
 *
 * iMode : bits 5-3 of opcode (source) or bits 8-6 (dest, already
 *          extracted by caller).
 * iReg  : bits 2-0 of opcode (source) or bits 11-9 (dest).
 * iSz   : SZ_BYTE / SZ_WORD / SZ_LONG
 */
static st_error_t cpu_ea_read( int           iMode,
                               int           iReg,
                               int           iSz,
                               st_u32_t     *puiVal)
{
    st_u16_t   uiExt;
    st_u32_t   uiAddr;
    st_u32_t   uiRaw;
    st_error_t eR;
    int        iDisp;

    *puiVal = 0u;

    switch (iMode)
    {
    case 0: /* Dn */
        *puiVal = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        return ST_NO_ERROR;

    case 1: /* An — reads always as long */
        *puiVal = g_cpu_ptCtx.auAn[iReg];
        return ST_NO_ERROR;

    case 2: /* (An) */
        uiAddr = g_cpu_ptCtx.auAn[iReg] & 0x00FFFFFFu;
        break;

    case 3: /* (An)+ */
        uiAddr = g_cpu_ptCtx.auAn[iReg] & 0x00FFFFFFu;
        /* A7 with byte: increment by 2 */
        if (iSz == SZ_BYTE && iReg == 7)
            g_cpu_ptCtx.auAn[iReg] += 2u;
        else
            g_cpu_ptCtx.auAn[iReg] += (st_u32_t)(1 << iSz);
        break;

    case 4: /* -(An) */
        if (iSz == SZ_BYTE && iReg == 7)
            g_cpu_ptCtx.auAn[iReg] -= 2u;
        else
            g_cpu_ptCtx.auAn[iReg] -= (st_u32_t)(1 << iSz);
        uiAddr = g_cpu_ptCtx.auAn[iReg] & 0x00FFFFFFu;
        break;

    case 5: /* d16(An) */
        uiExt = cpu_fetch_word();
        iDisp = (st_i16_t)uiExt;
        uiAddr = (g_cpu_ptCtx.auAn[iReg] + (st_u32_t)(st_i32_t)iDisp)
                 & 0x00FFFFFFu;
        break;

    case 6: /* d8(An,Xn) — brief extension word */
        uiExt  = cpu_fetch_word();
        iDisp  = (st_i8_t)(uiExt & 0xFFu);
        {
            st_u32_t uiIdx;
            int iXn  = (uiExt >> 12) & 7;
            int iXSz = (uiExt >> 11) & 1; /* 0=.W, 1=.L */

            if ((uiExt >> 15) & 1) /* An index */
                uiIdx = g_cpu_ptCtx.auAn[iXn];
            else
                uiIdx = g_cpu_ptCtx.auDn[iXn];

            if (!iXSz) /* sign-extend .W */
                uiIdx = (st_u32_t)(st_i32_t)(st_i16_t)(uiIdx & 0xFFFFu);

            uiAddr = (g_cpu_ptCtx.auAn[iReg]
                      + (st_u32_t)(st_i32_t)iDisp
                      + uiIdx)
                     & 0x00FFFFFFu;
        }
        break;

    case 7:
        switch (iReg)
        {
        case 0: /* abs.W */
            uiExt  = cpu_fetch_word();
            uiAddr = (st_u32_t)(st_i32_t)(st_i16_t)uiExt & 0x00FFFFFFu;
            break;

        case 1: /* abs.L */
            uiExt  = cpu_fetch_word();
            uiAddr = (st_u32_t)uiExt << 16u;
            uiExt  = cpu_fetch_word();
            uiAddr = (uiAddr | uiExt) & 0x00FFFFFFu;
            break;

        case 2: /* d16(PC) */
            uiAddr = g_cpu_ptCtx.uiPC; /* PC after fetch of this ext word */
            uiExt  = cpu_fetch_word();
            iDisp  = (st_i16_t)uiExt;
            /* The extension word address was already advanced; EA
             * = address_of_extension_word + displacement */
            uiAddr = (uiAddr + (st_u32_t)(st_i32_t)iDisp)
                     & 0x00FFFFFFu;
            break;

        case 3: /* d8(PC,Xn) */
            {
                st_u32_t uiBase = g_cpu_ptCtx.uiPC;
                uiExt  = cpu_fetch_word();
                iDisp  = (st_i8_t)(uiExt & 0xFFu);
                {
                    st_u32_t uiIdx;
                    int iXn  = (uiExt >> 12) & 7;
                    int iXSz = (uiExt >> 11) & 1;

                    if ((uiExt >> 15) & 1)
                        uiIdx = g_cpu_ptCtx.auAn[iXn];
                    else
                        uiIdx = g_cpu_ptCtx.auDn[iXn];

                    if (!iXSz)
                        uiIdx = (st_u32_t)(st_i32_t)
                                (st_i16_t)(uiIdx & 0xFFFFu);

                    uiAddr = (uiBase
                              + (st_u32_t)(st_i32_t)iDisp
                              + uiIdx)
                             & 0x00FFFFFFu;
                }
            }
            break;

        case 4: /* #imm */
            uiExt = cpu_fetch_word();
            if (iSz == SZ_LONG)
            {
                st_u32_t uiHi = uiExt;
                uiExt   = cpu_fetch_word();
                *puiVal = (uiHi << 16u) | uiExt;
            }
            else if (iSz == SZ_BYTE)
            {
                *puiVal = uiExt & 0xFFu;
            }
            else
            {
                *puiVal = uiExt;
            }
            return ST_NO_ERROR;

        default:
            LOG_ERROR("invalid EA mode 7 reg %d", iReg);
            return ST_ERROR;
        }
        break;

    default:
        LOG_ERROR("invalid EA mode %d", iMode);
        return ST_ERROR;
    }

    /* Memory read for modes 2-7 */
    switch (iSz)
    {
    case SZ_BYTE:
        {
            st_u8_t uiB;
            eR = st_read_byte(uiAddr, &uiB);
            uiRaw = uiB;
        }
        break;
    case SZ_WORD:
        {
            st_u16_t uiW;
            eR = st_read_word(uiAddr, &uiW);
            uiRaw = uiW;
        }
        break;
    default:
        eR = st_read_long(uiAddr, &uiRaw);
        break;
    }

    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("bus error read EA addr=0x%06X sz=%d", uiAddr, iSz);
        return ST_ERROR;
    }
    *puiVal = uiRaw;
    return ST_NO_ERROR;
}

/*
 * cpu_ea_write() - Write value to EA, consuming extension words.
 * @req REQ-CPU-014
 */
static st_error_t cpu_ea_write( int           iMode,
                                int           iReg,
                                int           iSz,
                                st_u32_t      uiVal)
{
    st_u16_t   uiExt;
    st_u32_t   uiAddr;
    st_error_t eR;
    int        iDisp;

    uiVal &= g_auiMask[iSz];

    switch (iMode)
    {
    case 0: /* Dn — partial write preserves upper bits */
        if (iSz == SZ_LONG)
            g_cpu_ptCtx.auDn[iReg] = uiVal;
        else if (iSz == SZ_WORD)
            g_cpu_ptCtx.auDn[iReg] = (g_cpu_ptCtx.auDn[iReg] & 0xFFFF0000u) | uiVal;
        else
            g_cpu_ptCtx.auDn[iReg] = (g_cpu_ptCtx.auDn[iReg] & 0xFFFFFF00u) | uiVal;
        return ST_NO_ERROR;

    case 1: /* An — always full 32-bit */
        g_cpu_ptCtx.auAn[iReg] = uiVal;
        return ST_NO_ERROR;

    case 2: /* (An) */
        uiAddr = g_cpu_ptCtx.auAn[iReg] & 0x00FFFFFFu;
        break;

    case 3: /* (An)+ */
        uiAddr = g_cpu_ptCtx.auAn[iReg] & 0x00FFFFFFu;
        if (iSz == SZ_BYTE && iReg == 7)
            g_cpu_ptCtx.auAn[iReg] += 2u;
        else
            g_cpu_ptCtx.auAn[iReg] += (st_u32_t)(1 << iSz);
        break;

    case 4: /* -(An) */
        if (iSz == SZ_BYTE && iReg == 7)
            g_cpu_ptCtx.auAn[iReg] -= 2u;
        else
            g_cpu_ptCtx.auAn[iReg] -= (st_u32_t)(1 << iSz);
        uiAddr = g_cpu_ptCtx.auAn[iReg] & 0x00FFFFFFu;
        break;

    case 5: /* d16(An) */
        uiExt = cpu_fetch_word();
        iDisp = (st_i16_t)uiExt;
        uiAddr = (g_cpu_ptCtx.auAn[iReg] + (st_u32_t)(st_i32_t)iDisp)
                 & 0x00FFFFFFu;
        break;

    case 6: /* d8(An,Xn) */
        uiExt  = cpu_fetch_word();
        iDisp  = (st_i8_t)(uiExt & 0xFFu);
        {
            st_u32_t uiIdx;
            int iXn  = (uiExt >> 12) & 7;
            int iXSz = (uiExt >> 11) & 1;

            if ((uiExt >> 15) & 1)
                uiIdx = g_cpu_ptCtx.auAn[iXn];
            else
                uiIdx = g_cpu_ptCtx.auDn[iXn];

            if (!iXSz)
                uiIdx = (st_u32_t)(st_i32_t)(st_i16_t)(uiIdx & 0xFFFFu);

            uiAddr = (g_cpu_ptCtx.auAn[iReg]
                      + (st_u32_t)(st_i32_t)iDisp
                      + uiIdx)
                     & 0x00FFFFFFu;
        }
        break;

    case 7:
        switch (iReg)
        {
        case 0: /* abs.W */
            uiExt  = cpu_fetch_word();
            uiAddr = (st_u32_t)(st_i32_t)(st_i16_t)uiExt & 0x00FFFFFFu;
            break;

        case 1: /* abs.L */
            uiExt  = cpu_fetch_word();
            uiAddr = (st_u32_t)uiExt << 16u;
            uiExt  = cpu_fetch_word();
            uiAddr = (uiAddr | uiExt) & 0x00FFFFFFu;
            break;

        default:
            LOG_ERROR("invalid EA write mode 7 reg %d (not writable)",
                      iReg);
            return ST_ERROR;
        }
        break;

    default:
        LOG_ERROR("invalid EA write mode %d", iMode);
        return ST_ERROR;
    }

    /* Memory write */
    switch (iSz)
    {
    case SZ_BYTE:
        eR = st_write_byte(uiAddr, (st_u8_t)uiVal);
        break;
    case SZ_WORD:
        eR = st_write_word(uiAddr, (st_u16_t)uiVal);
        break;
    default:
        eR = st_write_long(uiAddr, uiVal);
        break;
    }

    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("bus error write EA addr=0x%06X sz=%d", uiAddr, iSz);
        return ST_ERROR;
    }
    return ST_NO_ERROR;
}

/*
 * cpu_ea_calc_addr() - Compute EA address (for LEA, no value read).
 *
 * @req REQ-CPU-019
 *
 * Only valid for control-type EAs: modes 2/5/6 and 7.0/7.1/7.2/7.3.
 */
static st_error_t cpu_ea_calc_addr( int           iMode,
                                    int           iReg,
                                    st_u32_t     *puiAddr)
{
    st_u16_t uiExt;
    int      iDisp;

    switch (iMode)
    {
    case 2: /* (An) */
        *puiAddr = g_cpu_ptCtx.auAn[iReg] & 0x00FFFFFFu;
        return ST_NO_ERROR;

    case 5: /* d16(An) */
        uiExt  = cpu_fetch_word();
        iDisp  = (st_i16_t)uiExt;
        *puiAddr = (g_cpu_ptCtx.auAn[iReg] + (st_u32_t)(st_i32_t)iDisp)
                   & 0x00FFFFFFu;
        return ST_NO_ERROR;

    case 6: /* d8(An,Xn) */
        uiExt  = cpu_fetch_word();
        iDisp  = (st_i8_t)(uiExt & 0xFFu);
        {
            st_u32_t uiIdx;
            int iXn  = (uiExt >> 12) & 7;
            int iXSz = (uiExt >> 11) & 1;

            if ((uiExt >> 15) & 1)
                uiIdx = g_cpu_ptCtx.auAn[iXn];
            else
                uiIdx = g_cpu_ptCtx.auDn[iXn];

            if (!iXSz)
                uiIdx = (st_u32_t)(st_i32_t)(st_i16_t)(uiIdx & 0xFFFFu);

            *puiAddr = (g_cpu_ptCtx.auAn[iReg]
                        + (st_u32_t)(st_i32_t)iDisp
                        + uiIdx)
                       & 0x00FFFFFFu;
        }
        return ST_NO_ERROR;

    case 7:
        switch (iReg)
        {
        case 0: /* abs.W */
            uiExt    = cpu_fetch_word();
            *puiAddr = (st_u32_t)(st_i32_t)(st_i16_t)uiExt
                       & 0x00FFFFFFu;
            return ST_NO_ERROR;

        case 1: /* abs.L */
            uiExt    = cpu_fetch_word();
            *puiAddr = (st_u32_t)uiExt << 16u;
            uiExt    = cpu_fetch_word();
            *puiAddr = (*puiAddr | uiExt) & 0x00FFFFFFu;
            return ST_NO_ERROR;

        case 2: /* d16(PC) */
            {
                st_u32_t uiBase = g_cpu_ptCtx.uiPC;
                uiExt   = cpu_fetch_word();
                iDisp   = (st_i16_t)uiExt;
                *puiAddr = (uiBase + (st_u32_t)(st_i32_t)iDisp)
                           & 0x00FFFFFFu;
            }
            return ST_NO_ERROR;

        case 3: /* d8(PC,Xn) */
            {
                st_u32_t uiBase = g_cpu_ptCtx.uiPC;
                st_u32_t uiIdx;
                int      iXn, iXSz;

                uiExt  = cpu_fetch_word();
                iDisp  = (st_i8_t)(uiExt & 0xFFu);
                iXn    = (uiExt >> 12) & 7;
                iXSz   = (uiExt >> 11) & 1;

                if ((uiExt >> 15) & 1)
                    uiIdx = g_cpu_ptCtx.auAn[iXn];
                else
                    uiIdx = g_cpu_ptCtx.auDn[iXn];

                if (!iXSz)
                    uiIdx = (st_u32_t)(st_i32_t)
                            (st_i16_t)(uiIdx & 0xFFFFu);

                *puiAddr = (uiBase
                            + (st_u32_t)(st_i32_t)iDisp
                            + uiIdx)
                           & 0x00FFFFFFu;
            }
            return ST_NO_ERROR;

        default:
            LOG_ERROR("LEA: invalid EA mode 7 reg %d", iReg);
            return ST_ERROR;
        }

    default:
        LOG_ERROR("LEA: EA mode %d not a control EA", iMode);
        return ST_ERROR;
    }
}

/* ------------------------------------------------------------------
 * Instruction handlers
 * ------------------------------------------------------------------ */

/*
 * cpu_exec_move() - MOVE and MOVEA (groups 0x1/0x2/0x3).
 *
 * @req REQ-CPU-009, REQ-CPU-015, REQ-CPU-016, REQ-CPU-021
 *
 * Encoding:
 *   [15-12] size: 0x1=byte, 0x3=word, 0x2=long
 *   [11- 9] dest reg (bits in reversed position)
 *   [ 8- 6] dest mode
 *   [ 5- 3] src mode
 *   [ 2- 0] src reg
 */
static st_error_t cpu_exec_move(st_u16_t      uiOpc)
{
    int        iNibble = (uiOpc >> 12) & 0xFu;
    int        iSrcMode = (uiOpc >> 3) & 7;
    int        iSrcReg  = uiOpc & 7;
    int        iDstMode = (uiOpc >> 6) & 7;
    int        iDstReg  = (uiOpc >> 9) & 7;
    int        iSz;
    st_u32_t   uiVal;
    st_error_t eR;

    /* Map top nibble to size */
    if (iNibble == 1)       iSz = SZ_BYTE;
    else if (iNibble == 3)  iSz = SZ_WORD;
    else                    iSz = SZ_LONG;

    /* MOVEA.B is illegal */
    if (iDstMode == 1 && iSz == SZ_BYTE)
    {
        LOG_TODO("MOVEA.B is illegal — DC.W 0x%04X", (unsigned)uiOpc);
        return ST_NO_ERROR;
    }

    eR = cpu_ea_read(iSrcMode, iSrcReg, iSz, &uiVal);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    if (iDstMode == 1)
    {
        /* MOVEA — sign-extend word, no flags */
        if (iSz == SZ_WORD)
            uiVal = (st_u32_t)(st_i32_t)(st_i16_t)(uiVal & 0xFFFFu);
        g_cpu_ptCtx.auAn[iDstReg] = uiVal;
        return ST_NO_ERROR;
    }

    /* Normal MOVE — set flags, then write destination */
    cpu_flags_nz(uiVal, iSz);
    cpu_flags_clr_vc();

    eR = cpu_ea_write(iDstMode, iDstReg, iSz, uiVal);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    return ST_NO_ERROR;
}

/*
 * cpu_exec_moveq() - MOVEQ #imm8,Dn (group 0x7).
 *
 * @req REQ-CPU-009, REQ-CPU-017, REQ-CPU-021
 *
 * Encoding: 0111 DDD 0 IIIIIIII
 */
static st_error_t cpu_exec_moveq(st_u16_t uiOpc)
{
    int      iDn  = (uiOpc >> 9) & 7;
    st_i8_t  iByte = (st_i8_t)(uiOpc & 0xFFu);
    st_u32_t uiVal;

    /* Bit 8 must be 0 */
    if (uiOpc & 0x0100u)
    {
        LOG_TODO("MOVEQ: bit 8 set — not MOVEQ, DC.W 0x%04X",
                 (unsigned)uiOpc);
        return ST_NO_ERROR;
    }

    uiVal = (st_u32_t)(st_i32_t)iByte; /* sign-extend to 32 bits */
    g_cpu_ptCtx.auDn[iDn] = uiVal;

    cpu_flags_nz(uiVal, SZ_LONG);
    cpu_flags_clr_vc();

    return ST_NO_ERROR;
}

/*
 * cpu_exec_clr() - CLR.sz EA (within group 0x4).
 *
 * @req REQ-CPU-009, REQ-CPU-018, REQ-CPU-021
 *
 * Encoding: 0100 0010 SS MMMRRR
 */
static st_error_t cpu_exec_clr(
                                 
                                 st_u16_t      uiOpc)
{
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iReg    = uiOpc & 7;
    int        iSz;
    st_error_t eR;

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else if (iSzCode == 2) iSz = SZ_LONG;
    else
    {
        LOG_TODO("CLR: size=3 invalid — DC.W 0x%04X", (unsigned)uiOpc);
        return ST_NO_ERROR;
    }

    eR = cpu_ea_write(iMode, iReg, iSz, 0u);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    /* CLR: N=0, Z=1, V=0, C=0 */
    g_cpu_ptCtx.uiSR &= (st_u16_t)~(CPU_SR_N | CPU_SR_V | CPU_SR_C);
    g_cpu_ptCtx.uiSR |= (st_u16_t)CPU_SR_Z;

    return ST_NO_ERROR;
}

/*
 * cpu_exec_lea() - LEA EA,An (within group 0x4).
 *
 * @req REQ-CPU-009, REQ-CPU-019
 *
 * Encoding: 0100 AAA 111 MMMRRR   (mask 0xF1C0 == 0x41C0)
 */
static st_error_t cpu_exec_lea(st_u16_t      uiOpc)
{
    int        iAn   = (uiOpc >> 9) & 7;
    int        iMode = (uiOpc >> 3) & 7;
    int        iReg  = uiOpc & 7;
    st_u32_t   uiAddr;
    st_error_t eR;

    eR = cpu_ea_calc_addr(iMode, iReg, &uiAddr);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    g_cpu_ptCtx.auAn[iAn] = uiAddr;
    /* LEA does not affect flags */
    return ST_NO_ERROR;
}

/*
 * cpu_exec_swap() - SWAP Dn (within group 0x4).
 *
 * @req REQ-CPU-009, REQ-CPU-020
 *
 * Encoding: 0100 1000 0100 0DDD   (mask 0xFFF8 == 0x4840)
 */
static st_error_t cpu_exec_swap(st_u16_t uiOpc)
{
    int      iDn  = uiOpc & 7;
    st_u32_t uiOld = g_cpu_ptCtx.auDn[iDn];
    st_u32_t uiNew;

    uiNew = ((uiOld & 0x0000FFFFu) << 16u)
            | ((uiOld & 0xFFFF0000u) >> 16u);
    g_cpu_ptCtx.auDn[iDn] = uiNew;

    /* SWAP: N, Z from full 32-bit result; V=0, C=0 */
    cpu_flags_nz(uiNew, SZ_LONG);
    cpu_flags_clr_vc();

    return ST_NO_ERROR;
}

/*
 * cpu_exec_unary() - NEG / NEGX / NOT / TST  (group 0x4 unary ops).
 *
 * @req REQ-CPU-010, REQ-CPU-025
 *
 * szMnem : mnemonic string for logging
 * iOp    : 0=NEG, 1=NEGX, 2=NOT, 3=TST
 * Encoding: 0100 oo10 SS MMMRRR   (oo = 01/10/11/00 for TST/NOT/NEG/NEGX)
 */
static st_error_t cpu_exec_unary(
                                   
                                   st_u16_t      uiOpc,
                                   int           iOp)
{
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iReg    = uiOpc & 7;
    int        iSz;
    st_u32_t   uiSrc;
    st_u32_t   uiRes;
    st_error_t eR;
    st_u32_t   uiMask;
    st_u32_t   uiX;

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else if (iSzCode == 2) iSz = SZ_LONG;
    else
    {
        LOG_TODO("unary sz=3: DC.W 0x%04X", (unsigned)uiOpc);
        return ST_NO_ERROR;
    }

    uiMask = g_auiMask[iSz];

    eR = cpu_ea_read(iMode, iReg, iSz, &uiSrc);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    uiSrc &= uiMask;

    switch (iOp)
    {
    case 0: /* NEG: 0 - src */
        uiRes = (st_u32_t)(-(st_i32_t)uiSrc) & uiMask;
        cpu_flags_sub(uiSrc, 0u, uiRes, iSz);
        eR = cpu_ea_write(iMode, iReg, iSz, uiRes);
        break;

    case 1: /* NEGX: 0 - src - X */
        uiX   = CPU_FLAG_X(g_cpu_ptCtx.uiSR) ? 1u : 0u;
        uiRes = (st_u32_t)(-(st_i32_t)uiSrc - (st_i32_t)uiX) & uiMask;
        cpu_flags_sub(uiSrc + uiX, 0u, uiRes, iSz);
        /* NEGX: Z cleared only if result != 0 (not set if result == 0) */
        if (uiRes != 0u)
            g_cpu_ptCtx.uiSR &= (st_u16_t)~CPU_SR_Z;
        eR = cpu_ea_write(iMode, iReg, iSz, uiRes);
        break;

    case 2: /* NOT: ~src */
        uiRes = (~uiSrc) & uiMask;
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        eR = cpu_ea_write(iMode, iReg, iSz, uiRes);
        break;

    default: /* TST: result discarded, flags only */
        cpu_flags_nz(uiSrc, iSz);
        cpu_flags_clr_vc();
        eR = ST_NO_ERROR;
        break;
    }

    return eR;
}

/*
 * cpu_exec_group0() - Immediate ops (ADDI/SUBI/CMPI/ANDI/ORI/EORI).
 *
 * @req REQ-CPU-010
 *
 * Encoding: 0000 TTTT SS MMMRRR  + imm word(s)
 *   TTTT: 0000=ORI, 0010=ANDI, 0100=SUBI, 0110=ADDI, 1010=EORI, 1100=CMPI
 */
static st_error_t cpu_exec_group0(st_u16_t      uiOpc)
{
    int        iOp     = (uiOpc >> 8) & 0xFu;
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iReg    = uiOpc & 7;
    int        iSz;
    st_u32_t   uiImm;
    st_u32_t   uiDst;
    st_u32_t   uiRes;
    st_u16_t   uiExtW;
    st_error_t eR;

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else if (iSzCode == 2) iSz = SZ_LONG;
    else
    {
        LOG_TODO("group0: sz=3: DC.W 0x%04X", (unsigned)uiOpc);
        return ST_NO_ERROR;
    }

    /* Read immediate value */
    uiExtW = cpu_fetch_word();
    if (iSz == SZ_LONG)
    {
        st_u32_t uiHi = uiExtW;
        uiExtW = cpu_fetch_word();
        uiImm  = (uiHi << 16u) | uiExtW;
    }
    else if (iSz == SZ_BYTE)
        uiImm = uiExtW & 0xFFu;
    else
        uiImm = uiExtW;

    /* Read destination EA */
    eR = cpu_ea_read(iMode, iReg, iSz, &uiDst);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    switch (iOp)
    {
    case 0x0: /* ORI */
        uiRes = (uiDst | uiImm) & g_auiMask[iSz];
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        return cpu_ea_write(iMode, iReg, iSz, uiRes);

    case 0x2: /* ANDI */
        uiRes = (uiDst & uiImm) & g_auiMask[iSz];
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        return cpu_ea_write(iMode, iReg, iSz, uiRes);

    case 0x4: /* SUBI */
        uiRes = (uiDst - uiImm) & g_auiMask[iSz];
        cpu_flags_sub(uiImm, uiDst, uiRes, iSz);
        return cpu_ea_write(iMode, iReg, iSz, uiRes);

    case 0x6: /* ADDI */
        uiRes = (uiDst + uiImm) & g_auiMask[iSz];
        cpu_flags_add(uiImm, uiDst, uiRes, iSz);
        return cpu_ea_write(iMode, iReg, iSz, uiRes);

    case 0xA: /* EORI */
        uiRes = (uiDst ^ uiImm) & g_auiMask[iSz];
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        return cpu_ea_write(iMode, iReg, iSz, uiRes);

    case 0xC: /* CMPI — result discarded, flags only */
        uiRes = (uiDst - uiImm) & g_auiMask[iSz];
        cpu_flags_sub(uiImm, uiDst, uiRes, iSz);
        return ST_NO_ERROR;

    default:
        LOG_TODO("group0 op=0x%X DC.W 0x%04X", iOp, (unsigned)uiOpc);
        return ST_NO_ERROR;
    }
}

/*
 * cpu_exec_group5() - ADDQ / SUBQ / Scc / DBcc.
 *
 * @req REQ-CPU-010, REQ-CPU-043, REQ-CPU-044
 *
 * Encoding: 0101 DDD Q SS MMMRRR
 *   Q=0 → ADDQ, Q=1 → SUBQ; DDD=immediate (0=8); SS=size
 */
static st_error_t cpu_exec_group5(st_u16_t      uiOpc)
{
    int        iData   = (uiOpc >> 9) & 7;
    int        iDir    = (uiOpc >> 8) & 1; /* 0=ADDQ, 1=SUBQ */
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iReg    = uiOpc & 7;
    int        iSz;
    st_u32_t   uiImm;
    st_u32_t   uiDst;
    st_u32_t   uiRes;
    st_error_t eR;

    if (iSzCode == 3)
    {
        /* Scc EA / DBcc Dn,disp16 */
        int iCc3  = (uiOpc >> 8) & 0xFu;
        int iMd3  = (uiOpc >> 3) & 7;
        int iRg3  = uiOpc & 7;

        if (iMd3 == 1) /* DBcc Dn,disp16 : mode=1 (An) repurposed */
        {
            st_u16_t   uiD16;
            st_i16_t   iDisp16;
            st_u32_t   uiPCNext;
            st_i16_t   iWord;

            uiD16    = cpu_fetch_word();
            iDisp16  = (st_i16_t)uiD16;
            uiPCNext = g_cpu_ptCtx.uiPC;   /* PC after extension word */

            if (!cpu_eval_cc(iCc3))
            {
                /* Decrement Dn.W */
                iWord = (st_i16_t)(g_cpu_ptCtx.auDn[iRg3] & 0xFFFFu);
                iWord--;
                g_cpu_ptCtx.auDn[iRg3] = (g_cpu_ptCtx.auDn[iRg3] & 0xFFFF0000u)
                                     | (st_u16_t)iWord;
                if (iWord != (st_i16_t)-1)
                {
                    /* Branch: target = addr_of_ext_word + displacement */
                    g_cpu_ptCtx.uiPC = ((uiPCNext - 2u)
                                   + (st_u32_t)(st_i32_t)iDisp16)
                                  & 0x00FFFFFFu;
                }
            }
            return ST_NO_ERROR;
        }

        /* Scc EA: write 0xFF if condition true, 0x00 if false */
        {
            st_u8_t uiScc = cpu_eval_cc(iCc3) ? 0xFFu : 0x00u;
            return cpu_ea_write(iMd3, iRg3, SZ_BYTE,
                                (st_u32_t)uiScc);
        }
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    uiImm = (iData == 0) ? 8u : (st_u32_t)iData;

    eR = cpu_ea_read(iMode, iReg, iSz, &uiDst);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    if (iDir == 0)
    {
        uiRes = (uiDst + uiImm) & g_auiMask[iSz];
        /* An destination: no flags */
        if (iMode != 1)
            cpu_flags_add(uiImm, uiDst, uiRes, iSz);
    }
    else
    {
        uiRes = (uiDst - uiImm) & g_auiMask[iSz];
        if (iMode != 1)
            cpu_flags_sub(uiImm, uiDst, uiRes, iSz);
    }

    return cpu_ea_write(iMode, iReg, iSz, uiRes);
}

/*
 * cpu_exec_addx_subx() - ADDX / SUBX (register and memory forms).
 *
 * @req REQ-CPU-010, REQ-CPU-024
 *
 * Register: MMMRRR = 000 DDD (mode=0), Memory: MMMRRR = 001 DDD
 * iDir: 0=ADDX, 1=SUBX
 */
static st_error_t cpu_exec_addx_subx(st_u16_t      uiOpc,
                                       int           iDir)
{
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iRy     = uiOpc & 7;
    int        iRx     = (uiOpc >> 9) & 7;
    int        iRm     = (uiOpc >> 3) & 1; /* 0=Dn, 1=-(An) */
    int        iSz;
    st_u32_t   uiSrc;
    st_u32_t   uiDst;
    st_u32_t   uiRes;
    st_u32_t   uiX;
    st_error_t eR;

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else if (iSzCode == 2) iSz = SZ_LONG;
    else
    {
        LOG_TODO("addx_subx sz=3 DC.W 0x%04X", (unsigned)uiOpc);
        return ST_NO_ERROR;
    }

    uiX = CPU_FLAG_X(g_cpu_ptCtx.uiSR) ? 1u : 0u;

    if (iRm == 0)
    {
        /* Register form: Dy, Dx */
        uiSrc = g_cpu_ptCtx.auDn[iRy] & g_auiMask[iSz];
        uiDst = g_cpu_ptCtx.auDn[iRx] & g_auiMask[iSz];
    }
    else
    {
        /* Memory form: -(Ay), -(Ax) */
        eR = cpu_ea_read(4, iRy, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        eR = cpu_ea_read(4, iRx, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
    }

    if (iDir == 0)
    {
        /* ADDX */
        uiRes = (uiDst + uiSrc + uiX) & g_auiMask[iSz];
        cpu_flags_add(uiSrc + uiX, uiDst, uiRes, iSz);
        /* ADDX: Z cleared only if result != 0 */
        if (uiRes != 0u)
            g_cpu_ptCtx.uiSR &= (st_u16_t)~CPU_SR_Z;
    }
    else
    {
        /* SUBX */
        uiRes = (uiDst - uiSrc - uiX) & g_auiMask[iSz];
        cpu_flags_sub(uiSrc + uiX, uiDst, uiRes, iSz);
        if (uiRes != 0u)
            g_cpu_ptCtx.uiSR &= (st_u16_t)~CPU_SR_Z;
    }

    if (iRm == 0)
    {
        eR = cpu_ea_write(0, iRx, iSz, uiRes);
    }
    else
    {
        /* Write back to the pre-decremented address — re-read adjusted An */
        uiDst = g_cpu_ptCtx.auAn[iRx] & 0x00FFFFFFu;
        switch (iSz)
        {
        case SZ_BYTE:
            eR = st_write_byte(uiDst, (st_u8_t)uiRes);
            break;
        case SZ_WORD:
            eR = st_write_word(uiDst, (st_u16_t)uiRes);
            break;
        default:
            eR = st_write_long(uiDst, uiRes);
            break;
        }
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
    }
    return eR;
}

/*
 * cpu_exec_groupD() - ADD / ADDA / ADDX (group 0xD).
 * @req REQ-CPU-010, REQ-CPU-026, REQ-CPU-050
 */
static st_error_t cpu_exec_groupD(st_u16_t      uiOpc)
{
    int        iReg    = (uiOpc >> 9) & 7;
    int        iDir    = (uiOpc >> 8) & 1;
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iRn     = uiOpc & 7;
    int        iSz;
    st_u32_t   uiSrc;
    st_u32_t   uiDst;
    st_u32_t   uiRes;
    st_error_t eR;

    /* ADDA: iSzCode==3 (bits 7-6 == 11); iDir distinguishes .W vs .L */
    if (iSzCode == 3)
    {
        if (iDir == 1)
        {
            /* ADDA.L: read full 32-bit source, add to An */
            eR = cpu_ea_read(iMode, iRn, SZ_LONG, &uiSrc);
            if (eR != ST_NO_ERROR)
                return ST_ERROR;
            g_cpu_ptCtx.auAn[iReg] += uiSrc;
        }
        else
        {
            /* ADDA.W: read 16-bit source, sign-extend, add to An */
            eR = cpu_ea_read(iMode, iRn, SZ_WORD, &uiSrc);
            if (eR != ST_NO_ERROR)
                return ST_ERROR;
            g_cpu_ptCtx.auAn[iReg] +=
                (st_u32_t)(st_i32_t)(st_i16_t)(uiSrc & 0xFFFFu);
        }
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    /* Check ADDX: bit8=1, mode field <= 1 (Dn or -(An)) */
    if (iDir == 1 && iMode <= 1)
        return cpu_exec_addx_subx(uiOpc, 0);

    if (iDir == 0)
    {
        /* ADD EA,Dn  (bit8=0) */
        eR = cpu_ea_read(iMode, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        uiRes = (uiDst + uiSrc) & g_auiMask[iSz];
        cpu_flags_add(uiSrc, uiDst, uiRes, iSz);
        return cpu_ea_write(0, iReg, iSz, uiRes);
    }
    else
    {
        /* ADD Dn,EA  (bit8=1, mode>1) */
        uiSrc = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst + uiSrc) & g_auiMask[iSz];
        cpu_flags_add(uiSrc, uiDst, uiRes, iSz);
        return cpu_ea_write(iMode, iRn, iSz, uiRes);
    }
}

/*
 * cpu_exec_group9() - SUB / SUBA / SUBX (group 0x9).
 * @req REQ-CPU-010, REQ-CPU-026, REQ-CPU-050
 */
static st_error_t cpu_exec_group9(st_u16_t      uiOpc)
{
    int        iReg    = (uiOpc >> 9) & 7;
    int        iDir    = (uiOpc >> 8) & 1;
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iRn     = uiOpc & 7;
    int        iSz;
    st_u32_t   uiSrc;
    st_u32_t   uiDst;
    st_u32_t   uiRes;
    st_error_t eR;

    /* SUBA: iSzCode==3 (bits 7-6 == 11); iDir distinguishes .W vs .L */
    if (iSzCode == 3)
    {
        if (iDir == 1)
        {
            /* SUBA.L: read full 32-bit source, subtract from An */
            eR = cpu_ea_read(iMode, iRn, SZ_LONG, &uiSrc);
            if (eR != ST_NO_ERROR)
                return ST_ERROR;
            g_cpu_ptCtx.auAn[iReg] -= uiSrc;
        }
        else
        {
            /* SUBA.W: read 16-bit source, sign-extend, subtract from An */
            eR = cpu_ea_read(iMode, iRn, SZ_WORD, &uiSrc);
            if (eR != ST_NO_ERROR)
                return ST_ERROR;
            g_cpu_ptCtx.auAn[iReg] -=
                (st_u32_t)(st_i32_t)(st_i16_t)(uiSrc & 0xFFFFu);
        }
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    if (iDir == 1 && iMode <= 1)
        return cpu_exec_addx_subx(uiOpc, 1);

    if (iDir == 0)
    {
        /* SUB EA,Dn */
        eR = cpu_ea_read(iMode, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        uiRes = (uiDst - uiSrc) & g_auiMask[iSz];
        cpu_flags_sub(uiSrc, uiDst, uiRes, iSz);
        return cpu_ea_write(0, iReg, iSz, uiRes);
    }
    else
    {
        /* SUB Dn,EA */
        uiSrc = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst - uiSrc) & g_auiMask[iSz];
        cpu_flags_sub(uiSrc, uiDst, uiRes, iSz);
        return cpu_ea_write(iMode, iRn, iSz, uiRes);
    }
}

/*
 * cpu_exec_groupB() - CMP / CMPA / EOR / CMPM (group 0xB).
 * @req REQ-CPU-010, REQ-CPU-026
 */
static st_error_t cpu_exec_groupB(st_u16_t      uiOpc)
{
    int        iReg    = (uiOpc >> 9) & 7;
    int        iDir    = (uiOpc >> 8) & 1;
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iRn     = uiOpc & 7;
    int        iSz;
    st_u32_t   uiSrc;
    st_u32_t   uiDst;
    st_u32_t   uiRes;
    st_error_t eR;

    if (iSzCode == 3)
    {
        /* CMPA.L */
        eR = cpu_ea_read(iMode, iRn, SZ_LONG, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = g_cpu_ptCtx.auAn[iReg];
        uiRes = (uiDst - uiSrc) & g_auiMask[SZ_LONG];
        cpu_flags_sub(uiSrc, uiDst, uiRes, SZ_LONG);
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    if (iDir == 1 && iMode == 1)
    {
        /* CMPM (An)+,(An)+ */
        eR = cpu_ea_read(3, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        eR = cpu_ea_read(3, iReg, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst - uiSrc) & g_auiMask[iSz];
        cpu_flags_sub(uiSrc, uiDst, uiRes, iSz);
        return ST_NO_ERROR;
    }

    if (iDir == 1)
    {
        /* EOR Dn,EA */
        uiSrc = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst ^ uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        return cpu_ea_write(iMode, iRn, iSz, uiRes);
    }

    /* CMP EA,Dn (bit8=0) */
    eR = cpu_ea_read(iMode, iRn, iSz, &uiSrc);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;
    uiDst = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
    uiRes = (uiDst - uiSrc) & g_auiMask[iSz];
    cpu_flags_sub(uiSrc, uiDst, uiRes, iSz);
    return ST_NO_ERROR;
}

/*
 * cpu_exec_groupC() - AND / MULU / MULS (group 0xC).
 * @req REQ-CPU-010, REQ-CPU-027
 */
static st_error_t cpu_exec_groupC(st_u16_t      uiOpc)
{
    int        iReg    = (uiOpc >> 9) & 7;
    int        iDir    = (uiOpc >> 8) & 1;
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iRn     = uiOpc & 7;
    int        iSz;
    st_u32_t   uiSrc;
    st_u32_t   uiDst;
    st_u32_t   uiRes;
    st_error_t eR;

    if (iSzCode == 3)
    {
        /* MULU.W or MULS.W */
        eR = cpu_ea_read(iMode, iRn, SZ_WORD, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        if (iDir == 0)
        {
            /* MULU.W: unsigned 16x16 → 32 */
            uiRes = (uiSrc & 0xFFFFu) * (g_cpu_ptCtx.auDn[iReg] & 0xFFFFu);
        }
        else
        {
            /* MULS.W: signed 16x16 → 32 */
            uiRes = (st_u32_t)((st_i32_t)(st_i16_t)(uiSrc & 0xFFFFu)
                    * (st_i32_t)(st_i16_t)(g_cpu_ptCtx.auDn[iReg] & 0xFFFFu));
        }
        g_cpu_ptCtx.auDn[iReg] = uiRes;
        cpu_flags_nz(uiRes, SZ_LONG);
        cpu_flags_clr_vc();
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    if (iDir == 0)
    {
        /* AND EA,Dn */
        eR = cpu_ea_read(iMode, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        uiRes = (uiDst & uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        return cpu_ea_write(0, iReg, iSz, uiRes);
    }
    else
    {
        /* AND Dn,EA */
        uiSrc = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst & uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        return cpu_ea_write(iMode, iRn, iSz, uiRes);
    }
}

/*
 * cpu_exec_group8() - OR / DIVU / DIVS (group 0x8).
 * @req REQ-CPU-010, REQ-CPU-028
 */
static st_error_t cpu_exec_group8(st_u16_t      uiOpc)
{
    int        iReg    = (uiOpc >> 9) & 7;
    int        iDir    = (uiOpc >> 8) & 1;
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iRn     = uiOpc & 7;
    int        iSz;
    st_u32_t   uiSrc;
    st_u32_t   uiDst;
    st_u32_t   uiRes;
    st_error_t eR;

    if (iSzCode == 3)
    {
        /* DIVU.W or DIVS.W */
        eR = cpu_ea_read(iMode, iRn, SZ_WORD, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiSrc &= 0xFFFFu;
        if (uiSrc == 0u)
        {
            LOG_TODO("DIV by zero — exception UC23");
            return ST_NO_ERROR;
        }
        uiDst = g_cpu_ptCtx.auDn[iReg];
        if (iDir == 0)
        {
            /* DIVU.W */
            st_u32_t uiQ = uiDst / uiSrc;
            st_u32_t uiR = uiDst % uiSrc;
            g_cpu_ptCtx.auDn[iReg] = ((uiR & 0xFFFFu) << 16u)
                                 | (uiQ & 0xFFFFu);
        }
        else
        {
            /* DIVS.W */
            st_i32_t iDst  = (st_i32_t)uiDst;
            st_i32_t iSrc  = (st_i32_t)(st_i16_t)uiSrc;
            st_i32_t iQ    = iDst / iSrc;
            st_i32_t iR    = iDst % iSrc;
            g_cpu_ptCtx.auDn[iReg] = (((st_u32_t)(iR & 0xFFFF) << 16u))
                                 | ((st_u32_t)(iQ & 0xFFFF));
        }
        cpu_flags_nz(g_cpu_ptCtx.auDn[iReg] & 0xFFFFu, SZ_WORD);
        cpu_flags_clr_vc();
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    if (iDir == 0)
    {
        /* OR EA,Dn */
        eR = cpu_ea_read(iMode, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        uiRes = (uiDst | uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        return cpu_ea_write(0, iReg, iSz, uiRes);
    }
    else
    {
        /* OR Dn,EA */
        uiSrc = g_cpu_ptCtx.auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst | uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(uiRes, iSz);
        cpu_flags_clr_vc();
        return cpu_ea_write(iMode, iRn, iSz, uiRes);
    }
}

/*
 * cpu_exec_groupE() - Shifts and rotations (group 0xE).
 *
 * @req REQ-CPU-010, REQ-CPU-029, REQ-CPU-030, REQ-CPU-031
 *
 * Register form (sz != 3):
 *   1110 cccc d ss i tt rrr
 *   tt: 00=AS, 01=LS, 10=ROX, 11=RO
 *   d: 0=right, 1=left
 *   i: 0=immediate count, 1=register count
 *   cc: count (immediate: 0→8; register: Dreg index)
 *
 * Memory form (sz=3): shift by 1 word, modes 2-7.
 */
static st_error_t cpu_exec_groupE(st_u16_t      uiOpc)
{
    int        iSzCode = (uiOpc >> 6) & 3;
    int        iMode   = (uiOpc >> 3) & 7;
    int        iRn     = uiOpc & 7;
    int        iDir    = (uiOpc >> 8) & 1;    /* 0=right, 1=left */
    int        iType   = (uiOpc >> 3) & 3;    /* register form */
    int        iIR     = (uiOpc >> 5) & 1;    /* 0=imm, 1=reg */
    int        iCount;
    int        iSz;
    st_u32_t   uiVal;
    st_u32_t   uiMsb;
    st_u32_t   uiMask;
    st_u32_t   uiRes;
    st_u32_t   uiLastBit;
    int        iX;
    st_error_t eR;

    if (iSzCode == 3)
    {
        /* Memory shift: 1 bit, word size */
        iType = (uiOpc >> 9) & 7;
        if (iType > 7 || ((uiOpc >> 11) & 1))
        {
            LOG_TODO("groupE mem shift bit11 DC.W 0x%04X",
                     (unsigned)uiOpc);
            return ST_NO_ERROR;
        }
        iDir = (iType & 1); /* even=right, odd=left */
        /* Remap type: 0=ASR,1=ASL,2=LSR,3=LSL,4=ROXR,5=ROXL,6=ROR,7=ROL */
        switch (iType >> 1)
        {
        case 0: iType = 0; break; /* AS */
        case 1: iType = 1; break; /* LS */
        case 2: iType = 2; break; /* ROX */
        default: iType = 3; break; /* RO */
        }
        iCount = 1;
        iSz    = SZ_WORD;

        /* Reject Dn, An destinations */
        if (iMode <= 1)
        {
            LOG_TODO("groupE mem shift Dn/An DC.W 0x%04X",
                     (unsigned)uiOpc);
            return ST_NO_ERROR;
        }
        eR = cpu_ea_read(iMode, iRn, iSz, &uiVal);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
    }
    else
    {
        if      (iSzCode == 0) iSz = SZ_BYTE;
        else if (iSzCode == 1) iSz = SZ_WORD;
        else                   iSz = SZ_LONG;

        /* iType: bits 4-3 = type */
        iType = (uiOpc >> 3) & 3;

        if (iIR == 0)
        {
            iCount = (uiOpc >> 9) & 7;
            if (iCount == 0) iCount = 8;
        }
        else
        {
            iCount = (int)(g_cpu_ptCtx.auDn[(uiOpc >> 9) & 7] & 63u);
        }

        uiVal = g_cpu_ptCtx.auDn[iRn] & g_auiMask[iSz];
    }

    uiMsb  = g_auiMsb[iSz];
    uiMask = g_auiMask[iSz];
    iX     = CPU_FLAG_X(g_cpu_ptCtx.uiSR) ? 1 : 0;
    uiRes  = uiVal;

    {
        int i;
        for (i = 0; i < iCount; i++)
        {
            switch (iType)
            {
            case 0: /* AS */
                if (iDir)
                {
                    uiLastBit = (uiRes & uiMsb) ? 1u : 0u;
                    uiRes     = (uiRes << 1u) & uiMask;
                    if (uiLastBit)
                        g_cpu_ptCtx.uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        g_cpu_ptCtx.uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                else
                {
                    uiLastBit = uiRes & 1u;
                    uiRes     = ((uiRes >> 1u) | (uiRes & uiMsb)) & uiMask;
                    if (uiLastBit)
                        g_cpu_ptCtx.uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        g_cpu_ptCtx.uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                break;

            case 1: /* LS */
                if (iDir)
                {
                    uiLastBit = (uiRes & uiMsb) ? 1u : 0u;
                    uiRes     = (uiRes << 1u) & uiMask;
                    if (uiLastBit)
                        g_cpu_ptCtx.uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        g_cpu_ptCtx.uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                else
                {
                    uiLastBit = uiRes & 1u;
                    uiRes     = (uiRes >> 1u) & uiMask;
                    if (uiLastBit)
                        g_cpu_ptCtx.uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        g_cpu_ptCtx.uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                break;

            case 2: /* ROX */
                if (iDir)
                {
                    uiLastBit = (uiRes & uiMsb) ? 1u : 0u;
                    uiRes     = ((uiRes << 1u) | (st_u32_t)iX) & uiMask;
                    iX        = (int)uiLastBit;
                    if (iX)
                        g_cpu_ptCtx.uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        g_cpu_ptCtx.uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                else
                {
                    uiLastBit = uiRes & 1u;
                    uiRes     = ((uiRes >> 1u)
                                | ((st_u32_t)iX << (st_u32_t)
                                   (iSz == SZ_BYTE ? 7
                                    : iSz == SZ_WORD ? 15 : 31)))
                                & uiMask;
                    iX        = (int)uiLastBit;
                    if (iX)
                        g_cpu_ptCtx.uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        g_cpu_ptCtx.uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                break;

            default: /* RO */
                if (iDir)
                {
                    uiLastBit = (uiRes & uiMsb) ? 1u : 0u;
                    uiRes     = ((uiRes << 1u)
                                 | uiLastBit) & uiMask;
                    if (uiLastBit)
                        g_cpu_ptCtx.uiSR |=  (st_u16_t)CPU_SR_C;
                    else
                        g_cpu_ptCtx.uiSR &= (st_u16_t)~CPU_SR_C;
                }
                else
                {
                    uiLastBit = uiRes & 1u;
                    uiRes     = ((uiRes >> 1u)
                                 | (uiLastBit << (st_u32_t)
                                    (iSz == SZ_BYTE ? 7
                                     : iSz == SZ_WORD ? 15 : 31)))
                                & uiMask;
                    if (uiLastBit)
                        g_cpu_ptCtx.uiSR |=  (st_u16_t)CPU_SR_C;
                    else
                        g_cpu_ptCtx.uiSR &= (st_u16_t)~CPU_SR_C;
                }
                break;
            }
        }
    }

    /* V flag: 1 if any bit shifted out of MSB during AS; else 0 */
    /* Simplified: clear V for all shifts except AS where sign changed */
    if (iType == 0 && iCount > 0)
    {
        /* ASL: V=1 if MSB changed during any shift step */
        st_u32_t uiOrig = uiVal & uiMsb;
        if ((uiRes & uiMsb) != uiOrig || uiRes == 0u)
            g_cpu_ptCtx.uiSR &= (st_u16_t)~CPU_SR_V; /* simplified */
        else
            g_cpu_ptCtx.uiSR &= (st_u16_t)~CPU_SR_V;
    }
    else
    {
        g_cpu_ptCtx.uiSR &= (st_u16_t)~CPU_SR_V;
    }

    cpu_flags_nz(uiRes, iSz);

    if (iSzCode == 3)
        return cpu_ea_write(iMode, iRn, SZ_WORD, uiRes);
    else
        return cpu_ea_write(0, iRn, iSz, uiRes);
}

/*
 * cpu_exec_branch() - BRA / BSR / Bcc (group 0x6).
 *
 * @req REQ-CPU-011, REQ-CPU-032, REQ-CPU-033
 *
 * Encoding: 0110 cccc dddddddd
 *   cccc = 0 → BRA (unconditional)
 *   cccc = 1 → BSR (branch to subroutine)
 *   cccc = 2-15 → Bcc (conditional)
 *   d = 0x00 → 16-bit extension word displacement
 *   d != 0x00 → sign-extended 8-bit displacement
 *
 * Displacement is added to the PC value immediately after the opcode
 * word (before any extension word) — this is the base for the target.
 * The return address for BSR is the PC after all extension words.
 */
static st_error_t cpu_exec_branch(st_u16_t      uiOpc)
{
    int        iCc     = (uiOpc >> 8) & 0xFu;
    st_i8_t    iDisp8  = (st_i8_t)(uiOpc & 0xFFu);
    st_i32_t   iDisp;
    st_u32_t   uiBase;    /* PC after opcode (displacement base) */
    st_u32_t   uiTarget;
    st_error_t eR;

    uiBase = g_cpu_ptCtx.uiPC;   /* PC already advanced past opcode word */

    if (iDisp8 == 0)
    {
        /* Word displacement follows */
        st_u16_t uiExt = cpu_fetch_word();
        iDisp = (st_i32_t)(st_i16_t)uiExt;
    }
    else
    {
        iDisp = (st_i32_t)iDisp8;
    }

    /* Target = base (after opcode) + displacement */
    uiTarget = (uiBase + (st_u32_t)iDisp) & 0x00FFFFFFu;

    if (iCc == 0) /* BRA — unconditional */
    {
        g_cpu_ptCtx.uiPC = uiTarget;
        return ST_NO_ERROR;
    }

    if (iCc == 1) /* BSR — push return address then branch */
    {
        /* Return address = PC after full instruction (past ext word) */
        eR = cpu_push_long(g_cpu_ptCtx.uiPC);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("BSR: stack push failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7] + 4u);
            return ST_ERROR;
        }
        g_cpu_ptCtx.uiPC = uiTarget;
        return ST_NO_ERROR;
    }

    /* Bcc — conditional branch */
    if (cpu_eval_cc(iCc))
        g_cpu_ptCtx.uiPC = uiTarget;
    /* else: fall through, PC already at next instruction */

    return ST_NO_ERROR;
}

/*
 * cpu_exec_ext() - EXT.W Dn or EXT.L Dn  (group 0x4).
 *
 * @req REQ-CPU-010
 *
 * EXT.W : 0100 1000 1000 0DDD  (0xFFF8 == 0x4880)
 * EXT.L : 0100 1000 1100 0DDD  (0xFFF8 == 0x48C0)
 */
static st_error_t cpu_exec_ext(st_u16_t uiOpc)
{
    int      iDn  = uiOpc & 7;
    st_u32_t uiVal;
    int      iSz;

    if ((uiOpc & 0xFFF8u) == 0x4880u)
    {
        /* EXT.W: sign-extend byte to word */
        uiVal = (st_u32_t)(st_i32_t)(st_i8_t)(g_cpu_ptCtx.auDn[iDn] & 0xFFu);
        g_cpu_ptCtx.auDn[iDn] = (g_cpu_ptCtx.auDn[iDn] & 0xFFFF0000u)
                           | (uiVal & 0xFFFFu);
        iSz = SZ_WORD;
        uiVal &= 0xFFFFu;
    }
    else
    {
        /* EXT.L: sign-extend word to long */
        uiVal = (st_u32_t)(st_i32_t)(st_i16_t)
                (g_cpu_ptCtx.auDn[iDn] & 0xFFFFu);
        g_cpu_ptCtx.auDn[iDn] = uiVal;
        iSz = SZ_LONG;
    }

    cpu_flags_nz(uiVal, iSz);
    cpu_flags_clr_vc();
    return ST_NO_ERROR;
}

/*
 * cpu_exec_misc4e() - Group 0x4 instructions with upper byte 0x4E.
 *
 * @req REQ-CPU-011, REQ-CPU-034, REQ-CPU-035, REQ-CPU-036,
 *      REQ-CPU-037, REQ-CPU-038, REQ-CPU-039, REQ-CPU-041, REQ-CPU-042
 *
 * NOP (0x4E71), STOP (0x4E72), RTE (0x4E73), RTS (0x4E75),
 * RTR (0x4E77), TRAP #n (0x4E40-0x4E4F), LINK An,#d16 (0x4E50-0x4E57),
 * UNLK An (0x4E58-0x4E5F), JSR EA (0x4E80-0x4EBF), JMP EA (0x4EC0-0x4EFF).
 */
static st_error_t cpu_exec_misc4e(st_u16_t      uiOpc)
{
    st_u16_t   uiExt;
    st_u32_t   uiAddr;
    st_u32_t   uiTmp;
    st_u16_t   uiW;
    int        iAn;
    int        iN;
    int        iMode;
    int        iReg;
    st_error_t eR;

    /* NOP */
    if (uiOpc == 0x4E71u)
        return ST_NO_ERROR;

    /* STOP #imm — load immediate → SR, halt CPU */
    if (uiOpc == 0x4E72u)
    {
        uiExt = cpu_fetch_word();
        g_cpu_ptCtx.uiSR   = uiExt;
        g_cpu_ptCtx.eState = CPU_STATE_STOPPED;
        LOG_INFO("STOP #0x%04X — CPU halted", (unsigned)uiExt);
        return ST_NO_ERROR;
    }

    /* RTE: pop SR (word) then PC (long) — restore full context */
    if (uiOpc == 0x4E73u)
    {
        eR = cpu_pop_word(&uiW);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("RTE: stack pop SR failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7]);
            return ST_ERROR;
        }
        g_cpu_ptCtx.uiSR = uiW;
        eR = cpu_pop_long(&uiTmp);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("RTE: stack pop PC failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7]);
            return ST_ERROR;
        }
        g_cpu_ptCtx.uiPC = uiTmp & 0x00FFFFFFu;
        return ST_NO_ERROR;
    }

    /* RTS: pop PC from stack */
    if (uiOpc == 0x4E75u)
    {
        eR = cpu_pop_long(&uiTmp);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("RTS: stack pop failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7]);
            return ST_ERROR;
        }
        g_cpu_ptCtx.uiPC = uiTmp & 0x00FFFFFFu;
        return ST_NO_ERROR;
    }

    /* RTR: pop CCR (word) then PC — restores condition codes only */
    if (uiOpc == 0x4E77u)
    {
        eR = cpu_pop_word(&uiW);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("RTR: stack pop CCR failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7]);
            return ST_ERROR;
        }
        /* RTR: only CCR (lower byte: X N Z V C) is restored */
        g_cpu_ptCtx.uiSR = (g_cpu_ptCtx.uiSR & 0xFF00u) | (uiW & 0x00FFu);
        eR = cpu_pop_long(&uiTmp);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("RTR: stack pop PC failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7]);
            return ST_ERROR;
        }
        g_cpu_ptCtx.uiPC = uiTmp & 0x00FFFFFFu;
        return ST_NO_ERROR;
    }

    /* TRAP #n: 0100 1110 0100 NNNN */
    if ((uiOpc & 0xFFF0u) == 0x4E40u)
    {
        iN = uiOpc & 0xFu;
        if (iN == 1)
            return tos_gemdos(&g_cpu_ptCtx);
        if (iN == 14)
            return tos_xbios(&g_cpu_ptCtx);
        return cpu_raise_exception(CPU_VEC_TRAP(iN));
    }

    /* LINK An,#d16: 0100 1110 0101 0AAA */
    if ((uiOpc & 0xFFF8u) == 0x4E50u)
    {
        iAn  = uiOpc & 7;
        uiExt = cpu_fetch_word();
        /* Push An (saved value) to stack */
        eR = cpu_push_long(g_cpu_ptCtx.auAn[iAn]);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("LINK: push An failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7] + 4u);
            return ST_ERROR;
        }
        /* An = SP (frame pointer) */
        g_cpu_ptCtx.auAn[iAn] = g_cpu_ptCtx.auAn[7];
        /* SP += sign_ext(d16) — displacement is typically negative */
        g_cpu_ptCtx.auAn[7] += (st_u32_t)(st_i32_t)(st_i16_t)uiExt;
        return ST_NO_ERROR;
    }

    /* UNLK An: 0100 1110 0101 1AAA */
    if ((uiOpc & 0xFFF8u) == 0x4E58u)
    {
        iAn = uiOpc & 7;
        /* SP = An (frame pointer) */
        g_cpu_ptCtx.auAn[7] = g_cpu_ptCtx.auAn[iAn];
        /* Pop saved An from stack */
        eR = cpu_pop_long(&uiTmp);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("UNLK: pop An failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7]);
            return ST_ERROR;
        }
        g_cpu_ptCtx.auAn[iAn] = uiTmp;
        return ST_NO_ERROR;
    }

    /* JSR EA: 0100 1110 10 MMMRRR */
    if ((uiOpc & 0xFFC0u) == 0x4E80u)
    {
        iMode = (uiOpc >> 3) & 7;
        iReg  = uiOpc & 7;
        eR    = cpu_ea_calc_addr(iMode, iReg, &uiAddr);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        /* Push return address (PC after full instruction + ext words) */
        eR = cpu_push_long(g_cpu_ptCtx.uiPC);
        if (eR != ST_NO_ERROR)
        {
            LOG_ERROR("JSR: stack push failed SP=0x%06X",
                      g_cpu_ptCtx.auAn[7] + 4u);
            return ST_ERROR;
        }
        g_cpu_ptCtx.uiPC = uiAddr;
        return ST_NO_ERROR;
    }

    /* JMP EA: 0100 1110 11 MMMRRR */
    if ((uiOpc & 0xFFC0u) == 0x4EC0u)
    {
        iMode = (uiOpc >> 3) & 7;
        iReg  = uiOpc & 7;
        eR    = cpu_ea_calc_addr(iMode, iReg, &uiAddr);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        g_cpu_ptCtx.uiPC = uiAddr;
        return ST_NO_ERROR;
    }

    LOG_TODO("cpu_exec_misc4e: unimplemented 0x%04X", (unsigned)uiOpc);
    return ST_NO_ERROR;
}

/*
 * cpu_exec_movem() - MOVEM register list save/restore.
 *
 * @req REQ-CPU-045, REQ-CPU-046, REQ-CPU-047, REQ-CPU-048, REQ-CPU-049
 *
 * dir=0 (0x48xx): registers → memory.
 *   mode 4 (-(An)): pre-decrement, reversed mask
 *     bit 0=A7 bit 7=A0 bit 8=D7 bit 15=D0
 *   other modes: sequential write, An unchanged.
 *
 * dir=1 (0x4Cxx): memory → registers.
 *   mode 3 ((An)+): post-increment, standard mask.
 *   other modes: sequential read, An unchanged.
 *
 * Standard mask (all except -(An)):
 *   bit 0=D0 .. bit 7=D7, bit 8=A0 .. bit 15=A7
 *
 * Word transfers (.W) are sign-extended to 32 bits on load.
 */
static st_error_t cpu_exec_movem(st_u16_t      uiOpc)
{
    int        iDir;
    int        iSzCode;
    int        iMode;
    int        iReg;
    int        iStep;
    st_u16_t   uiMask;
    st_u32_t   auSnap[16];
    st_u32_t   uiAddr;
    st_u32_t   uiLong;
    st_u16_t   uiWord;
    int        i;
    st_error_t eR;

    iDir    = (uiOpc >> 10) & 1;
    iSzCode = (uiOpc >>  6) & 1;   /* 0 = word, 1 = long */
    iMode   = (uiOpc >>  3) & 7;
    iReg    = uiOpc & 7;
    iStep   = iSzCode ? 4 : 2;

    if (st_read_word(g_cpu_ptCtx.uiPC, &uiMask) != ST_NO_ERROR)
        return ST_ERROR;
    g_cpu_ptCtx.uiPC += 2u;

    /* Snapshot all registers (handles the An-in-list edge case correctly) */
    for (i = 0; i < 8; i++)
    {
        auSnap[i]     = g_cpu_ptCtx.auDn[i];
        auSnap[i + 8] = g_cpu_ptCtx.auAn[i];
    }

    if (iDir == 0)
    {
        /* --- register → memory --- */
        if (iMode == 4)
        {
            /* -(An): reversed mask, pre-decrement each transfer */
            uiAddr = g_cpu_ptCtx.auAn[iReg];
            for (i = 0; i <= 15; i++)
            {
                if (!((uiMask >> i) & 1u))
                    continue;
                /* reversed: bit 0=A7..bit 7=A0, bit 8=D7..bit 15=D0 */
                uiLong = (i < 8) ? auSnap[8 + (7 - i)] : auSnap[15 - i];
                uiAddr -= (st_u32_t)iStep;
                if (iSzCode)
                    eR = st_write_long(uiAddr, uiLong);
                else
                    eR = st_write_word(uiAddr,
                                       (st_u16_t)(uiLong & 0xFFFFu));
                if (eR != ST_NO_ERROR)
                    return ST_ERROR;
            }
            g_cpu_ptCtx.auAn[iReg] = uiAddr;
        }
        else
        {
            /* control EA: sequential write, An unchanged */
            eR = cpu_ea_calc_addr(iMode, iReg, &uiAddr);
            if (eR != ST_NO_ERROR)
                return ST_ERROR;
            for (i = 0; i <= 15; i++)
            {
                if (!((uiMask >> i) & 1u))
                    continue;
                /* standard: bit 0=D0..bit 7=D7, bit 8=A0..bit 15=A7 */
                uiLong = (i < 8) ? auSnap[i] : auSnap[i];
                if (iSzCode)
                    eR = st_write_long(uiAddr, uiLong);
                else
                    eR = st_write_word(uiAddr,
                                       (st_u16_t)(uiLong & 0xFFFFu));
                if (eR != ST_NO_ERROR)
                    return ST_ERROR;
                uiAddr += (st_u32_t)iStep;
            }
        }
    }
    else
    {
        /* --- memory → register --- */
        if (iMode == 3)
        {
            /* (An)+: post-increment, standard mask */
            uiAddr = g_cpu_ptCtx.auAn[iReg];
            for (i = 0; i <= 15; i++)
            {
                if (!((uiMask >> i) & 1u))
                    continue;
                if (iSzCode)
                {
                    if (st_read_long(uiAddr, &uiLong) != ST_NO_ERROR)
                        return ST_ERROR;
                }
                else
                {
                    if (st_read_word(uiAddr, &uiWord) != ST_NO_ERROR)
                        return ST_ERROR;
                    uiLong = (st_u32_t)(st_i32_t)(st_i16_t)uiWord;
                }
                uiAddr += (st_u32_t)iStep;
                if (i < 8)
                    g_cpu_ptCtx.auDn[i] = uiLong;
                else
                    g_cpu_ptCtx.auAn[i - 8] = uiLong;
            }
            g_cpu_ptCtx.auAn[iReg] = uiAddr;
        }
        else
        {
            /* control EA: sequential read, An unchanged */
            eR = cpu_ea_calc_addr(iMode, iReg, &uiAddr);
            if (eR != ST_NO_ERROR)
                return ST_ERROR;
            for (i = 0; i <= 15; i++)
            {
                if (!((uiMask >> i) & 1u))
                    continue;
                if (iSzCode)
                {
                    if (st_read_long(uiAddr, &uiLong) != ST_NO_ERROR)
                        return ST_ERROR;
                }
                else
                {
                    if (st_read_word(uiAddr, &uiWord) != ST_NO_ERROR)
                        return ST_ERROR;
                    uiLong = (st_u32_t)(st_i32_t)(st_i16_t)uiWord;
                }
                uiAddr += (st_u32_t)iStep;
                if (i < 8)
                    g_cpu_ptCtx.auDn[i] = uiLong;
                else
                    g_cpu_ptCtx.auAn[i - 8] = uiLong;
            }
        }
    }
    return ST_NO_ERROR;
}

/*
 * cpu_exec_misc4() - Dispatch group 0x4 instructions.
 * @req REQ-CPU-045
 */
static st_error_t cpu_exec_misc4(st_u16_t      uiOpc)
{
    /* LEA: 0100 AAA 111 MMMRRR */
    if ((uiOpc & 0xF1C0u) == 0x41C0u)
        return cpu_exec_lea(uiOpc);

    /* EXT.W / EXT.L (must come before MOVEM — same upper byte) */
    if ((uiOpc & 0xFFF8u) == 0x4880u || (uiOpc & 0xFFF8u) == 0x48C0u)
        return cpu_exec_ext(uiOpc);

    /* MOVEM: 0100 1d00 1s MM RRR (d=0: reg→mem, d=1: mem→reg) */
    if ((uiOpc & 0xFB80u) == 0x4880u)
        return cpu_exec_movem(uiOpc);

    /* SWAP: 0100 1000 0100 0DDD */
    if ((uiOpc & 0xFFF8u) == 0x4840u)
        return cpu_exec_swap(uiOpc);

    /* NEGX: 0100 0000 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4000u)
        return cpu_exec_unary(uiOpc, 1);

    /* CLR: 0100 0010 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4200u)
        return cpu_exec_clr(uiOpc);

    /* NEG: 0100 0100 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4400u)
        return cpu_exec_unary(uiOpc, 0);

    /* NOT: 0100 0110 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4600u)
        return cpu_exec_unary(uiOpc, 2);

    /* TST: 0100 1010 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4A00u)
        return cpu_exec_unary(uiOpc, 3);

    /* 0x4Exx: NOP/STOP/RTE/RTS/RTR/TRAP/LINK/UNLK/JSR/JMP */
    if ((uiOpc & 0xFF00u) == 0x4E00u)
        return cpu_exec_misc4e(uiOpc);

    LOG_TODO("cpu_exec_misc4: unimplemented 0x%04X",
             (unsigned)uiOpc);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_u64_t cpu_init()
{
    st_u32_t   uiSSP;
    st_u32_t   uiPC;
    st_error_t eR;

    LOG_TRACE("Using ATARI ST Start-up conventions");
    
    /* -- [CPU]1. Read the reset SSP vector from ST RAM -- */
    eR = st_read_long(CPU_VEC_RESET_SSP, &uiSSP);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("failed to read reset SSP vector");
        return ST_ERROR;
    }
    
    /* -- [CPU]2. Read the reset PC vector from ST RAM -- */
    eR = st_read_long(CPU_VEC_RESET_PC, &uiPC);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("failed to read reset PC vector");
        return ST_ERROR;
    }

    /* -- [CPU]3. Enter supervisor mode and RUNNING state at the reset PC -- */
    g_cpu_ptCtx.uiSSP    = uiSSP;
    g_cpu_ptCtx.auAn[7]  = uiSSP;
    g_cpu_ptCtx.uiPC     = uiPC;
    g_cpu_ptCtx.uiSR     = CPU_SR_S | CPU_SR_I_MASK;
    g_cpu_ptCtx.eState   = CPU_STATE_RUNNING;

    LOG_INFO("cpu_init: SSP=0x%08X PC=0x%08X SR=0x%04X",
             uiSSP, uiPC, (unsigned)g_cpu_ptCtx.uiSR);
    return (st_u64_t)&g_cpu_ptCtx;
}

st_error_t cpu_step(cpu_step_result_t *ptResult)
{
    st_u16_t   uiOpcode;
    st_u32_t   uiPCBefore;
    st_error_t eR;

    LOG_TRACE("PC=0x%06X", g_cpu_ptCtx.uiPC);

    /* -- [CPU]4. Do nothing if the CPU is not RUNNING -- */
    if (g_cpu_ptCtx.eState != CPU_STATE_RUNNING)
    {
        LOG_ERROR("cpu_step: CPU not running (state=%d) - use cpu_init() first",
                 (int)g_cpu_ptCtx.eState);
        return ST_NO_ERROR;
    }

    uiPCBefore = g_cpu_ptCtx.uiPC;

    /* -- [CPU]5. Fetch the opcode word and advance PC -- */
    /* Fetch opcode word — advances PC by 2 */
    uiOpcode = cpu_fetch_word();

    /* -- [CPU]6. Fill the optional result structure when ptResult is provided -- */
    if (ptResult != NULL)
    {
        memset(ptResult, 0, sizeof(cpu_step_result_t));
        ptResult->uiOpcode   = uiOpcode;
        ptResult->uiPCBefore = uiPCBefore;
    }

    /* Dispatch on top nibble */
    switch ((uiOpcode >> 12) & 0xFu)
    {
    /* -- [CPU]7. Dispatch Immediate opcodes (0x0xxx) -- */
    case 0x0: /* Immediate ops: ORI/ANDI/SUBI/ADDI/EORI/CMPI */
        eR = cpu_exec_group0(uiOpcode);
        break;

    /* -- [CPU]8. Dispatch byte MOVE opcodes (0x1xxx) -- */
    /* -- [CPU]9. Dispatch long MOVE opcodes (0x2xxx) -- */
    /* -- [CPU]10. Dispatch word MOVE opcodes (0x3xxx) -- */
    case 0x1: /* MOVE.B */
    case 0x2: /* MOVE.L */
    case 0x3: /* MOVE.W / MOVEA.W */
        eR = cpu_exec_move(uiOpcode);
        break;

    /* -- [CPU]11. Dispatch Misc opcodes(0x4xxx) -- */
    case 0x4: /* Misc: LEA/CLR/SWAP/NEG/NOT/TST/EXT + UC23 */
        eR = cpu_exec_misc4(uiOpcode);
        break;

    /* -- [CPU]12. Dispatch ADDQ/SUBQ/Scc/DBcc opcodes(0x5xxx) -- */
    case 0x5: /* ADDQ / SUBQ / Scc / DBcc */
        eR = cpu_exec_group5(uiOpcode);
        break;

    /* -- [CPU]13. Dispatch BRA/BSR/Bcc opcodes(0x6xxx) -- */
    case 0x6: /* BRA / BSR / Bcc */
        eR = cpu_exec_branch(uiOpcode);
        break;

    /* -- [CPU]14. Dispatch MOVEQ opcodes (0x7xxx) -- */
    case 0x7: /* MOVEQ */
        eR = cpu_exec_moveq(uiOpcode);
        break;

    /* -- [CPU]15. Dispatch OR/DIVU/DIVS opcodes(0x8xxx) -- */
    case 0x8: /* OR / DIVU / DIVS */
        eR = cpu_exec_group8(uiOpcode);
        break;

    /* -- [CPU]16. Dispatch SUB/SUBA/SUBX opcodes(0x9xxx) -- */
    case 0x9: /* SUB / SUBA / SUBX */
        eR = cpu_exec_group9(uiOpcode);
        break;

    /* -- [CPU]17. Dispatch LINE-A traps (0xAxxx) -- */
    case 0xA: /* Line-A traps ($Axxx) */
        eR = linea_dispatch(&g_cpu_ptCtx, uiOpcode);
        break;

    /* -- [CPU]18. Dispatch CMP/CMPA/EOR/CMPM opcodes(0xBxxx) -- */
    case 0xB: /* CMP / CMPA / EOR / CMPM */
        eR = cpu_exec_groupB(uiOpcode);
        break;

    /* -- [CPU]19. Dispatch AND/MULU/MULS opcodes(0xCxxx) -- */
    case 0xC: /* AND / MULU / MULS */
        eR = cpu_exec_groupC(uiOpcode);
        break;

    /* -- [CPU]20. Dispatch ADD/ADDA/ADDX opcodes(0xDxxx) -- */
    case 0xD: /* ADD / ADDA / ADDX */
        eR = cpu_exec_groupD(uiOpcode);
        break;

    /* -- [CPU]21. Dispatch Shifts/Rotates opcodes(0xExxx) -- */
    case 0xE: /* Shifts / Rotations */
        eR = cpu_exec_groupE(uiOpcode);
        break;

    /* -- [CPU]22. Dispatch Line-F traps (0xFxxx) -- */
    case 0xF: /* Line-F ($Fxxx) — raises exception, not yet handled */
        LOG_TODO("cpu_step: Line-F opcode 0x%04X at PC=0x%06X (UC29+)",
                 (unsigned)uiOpcode, uiPCBefore);
        eR = cpu_raise_exception(CPU_VEC_LINE_F);
        break;

    default: // Should never go there anymore - default case kept
        LOG_TODO("cpu_step: unimplemented opcode 0x%04X "
                 "at PC=0x%06X",
                 (unsigned)uiOpcode, uiPCBefore);
        eR = ST_NO_ERROR;
        break;
    }

    g_cpu_ptCtx.ulInstrCount++;

    /* -- [CPU]23. Update result & cycles count for time accuracy -- */
    if (ptResult != NULL)
    {
        ptResult->uiPCAfter = g_cpu_ptCtx.uiPC;
        ptResult->iCycles   = 4; /* TODO(UC22): accurate cycle counts */
    }

    return eR;
}

st_error_t cpu_raise_exception(st_u32_t      uiVector)
{
    st_u16_t   uiSR;
    st_u32_t   uiHandler;
    st_error_t eR;

    LOG_TRACE("vector=0x%08X", uiVector);
    
    /* Save current SR before modification */
    uiSR = g_cpu_ptCtx.uiSR;

    /* Switch to supervisor mode if in user mode */
    if (!(uiSR & CPU_SR_S))
    {
        st_u32_t uiUSP  = g_cpu_ptCtx.auAn[7];
        g_cpu_ptCtx.auAn[7]  = g_cpu_ptCtx.uiSSP;
        g_cpu_ptCtx.uiSSP    = uiUSP;
        g_cpu_ptCtx.uiSR    |= (st_u16_t)CPU_SR_S;
    }

    /* Exception frame: push PC (long) then SR (word)
     * Result on stack (low addr): SR | PC_hi | PC_lo  */
    eR = cpu_push_long(g_cpu_ptCtx.uiPC);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("exception: push PC failed SP=0x%06X",
                  g_cpu_ptCtx.auAn[7] + 4u);
        g_cpu_ptCtx.eState = CPU_STATE_HALTED;
        return ST_ERROR;
    }
    eR = cpu_push_word(uiSR);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("exception: push SR failed SP=0x%06X",
                  g_cpu_ptCtx.auAn[7] + 2u);
        g_cpu_ptCtx.eState = CPU_STATE_HALTED;
        return ST_ERROR;
    }

    /* Load handler address from vector table */
    eR = st_read_long(uiVector, &uiHandler);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("exception: read vector 0x%08X failed", uiVector);
        g_cpu_ptCtx.eState = CPU_STATE_HALTED;
        return ST_ERROR;
    }

    g_cpu_ptCtx.uiPC = uiHandler & 0x00FFFFFFu;
    LOG_INFO("exception vector=0x%06X handler=0x%06X SP=0x%06X",
             uiVector, g_cpu_ptCtx.uiPC, g_cpu_ptCtx.auAn[7]);
    return ST_NO_ERROR;
}
