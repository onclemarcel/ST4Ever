/*
 * CPU.c - MC68000 CPU emulator
 *
 * UC21: MOVE.B/W/L, MOVEA.W/L, MOVEQ, LEA, CLR.B/W/L, SWAP Dn
 * UC22: ADD/SUB/CMP/AND/OR/EOR/shifts + NEG/NEGX/NOT/TST/EXT +
 *        ADDI/SUBI/CMPI/ANDI/ORI/EORI + ADDQ/SUBQ +
 *        ADDX/SUBX + MULU/MULS/DIVU/DIVS (LOG_TODO)
 * TODO(UC23): BRA/Bcc/JSR/RTS/TRAP + exception vectors
 */
#include "CPU.h"
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
 * Internal helpers
 * ------------------------------------------------------------------ */

/*
 * cpu_fetch_word() - Read word at PC and advance PC by 2.
 */
static st_u16_t cpu_fetch_word(cpu68k_t     *ptCpu,
                                st_machine_t *ptMachine)
{
    st_u16_t   uiW;
    st_error_t eR;

    eR = st_read_word(ptMachine, ptCpu->uiPC, &uiW);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("bus error at PC=0x%06X", ptCpu->uiPC);
        uiW = 0xFFFFu;
    }
    ptCpu->uiPC += 2u;
    return uiW;
}

/*
 * cpu_flags_nz() - Update N and Z flags from masked result.
 */
static void cpu_flags_nz(cpu68k_t *ptCpu, st_u32_t uiVal, int iSz)
{
    st_u16_t uiSR = ptCpu->uiSR;

    uiVal &= g_auiMask[iSz];
    if (uiVal == 0u)
        uiSR |= (st_u16_t)CPU_SR_Z;
    else
        uiSR &= (st_u16_t)~CPU_SR_Z;

    if (uiVal & g_auiMsb[iSz])
        uiSR |= (st_u16_t)CPU_SR_N;
    else
        uiSR &= (st_u16_t)~CPU_SR_N;

    ptCpu->uiSR = uiSR;
}

/*
 * cpu_flags_clr_vc() - Clear V and C flags.
 */
static void cpu_flags_clr_vc(cpu68k_t *ptCpu)
{
    ptCpu->uiSR &= (st_u16_t)~(CPU_SR_V | CPU_SR_C);
}

/*
 * cpu_flags_add() - Set N/Z/V/C/X for addition (dst + src = res).
 *
 * C = unsigned carry out; V = signed overflow; X = C.
 */
static void cpu_flags_add(cpu68k_t *ptCpu,
                           st_u32_t  uiSrc,
                           st_u32_t  uiDst,
                           st_u32_t  uiRes,
                           int       iSz)
{
    st_u32_t uiMsb  = g_auiMsb[iSz];
    st_u32_t uiMask = g_auiMask[iSz];
    st_u16_t uiSR   = ptCpu->uiSR;

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

    ptCpu->uiSR = uiSR;
}

/*
 * cpu_flags_sub() - Set N/Z/V/C/X for subtraction (dst - src = res).
 *
 * C = borrow (src > dst unsigned); V = signed overflow; X = C.
 */
static void cpu_flags_sub(cpu68k_t *ptCpu,
                           st_u32_t  uiSrc,
                           st_u32_t  uiDst,
                           st_u32_t  uiRes,
                           int       iSz)
{
    st_u32_t uiMsb  = g_auiMsb[iSz];
    st_u32_t uiMask = g_auiMask[iSz];
    st_u16_t uiSR   = ptCpu->uiSR;

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

    ptCpu->uiSR = uiSR;
}

/* ------------------------------------------------------------------
 * EA decoder
 * ------------------------------------------------------------------ */

/*
 * cpu_ea_read() - Decode EA, consume extension words, return value.
 *
 * iMode : bits 5-3 of opcode (source) or bits 8-6 (dest, already
 *          extracted by caller).
 * iReg  : bits 2-0 of opcode (source) or bits 11-9 (dest).
 * iSz   : SZ_BYTE / SZ_WORD / SZ_LONG
 */
static st_error_t cpu_ea_read(cpu68k_t     *ptCpu,
                               st_machine_t *ptMachine,
                               int           iMode,
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
        *puiVal = ptCpu->auDn[iReg] & g_auiMask[iSz];
        return ST_NO_ERROR;

    case 1: /* An — reads always as long */
        *puiVal = ptCpu->auAn[iReg];
        return ST_NO_ERROR;

    case 2: /* (An) */
        uiAddr = ptCpu->auAn[iReg] & 0x00FFFFFFu;
        break;

    case 3: /* (An)+ */
        uiAddr = ptCpu->auAn[iReg] & 0x00FFFFFFu;
        /* A7 with byte: increment by 2 */
        if (iSz == SZ_BYTE && iReg == 7)
            ptCpu->auAn[iReg] += 2u;
        else
            ptCpu->auAn[iReg] += (st_u32_t)(1 << iSz);
        break;

    case 4: /* -(An) */
        if (iSz == SZ_BYTE && iReg == 7)
            ptCpu->auAn[iReg] -= 2u;
        else
            ptCpu->auAn[iReg] -= (st_u32_t)(1 << iSz);
        uiAddr = ptCpu->auAn[iReg] & 0x00FFFFFFu;
        break;

    case 5: /* d16(An) */
        uiExt = cpu_fetch_word(ptCpu, ptMachine);
        iDisp = (st_i16_t)uiExt;
        uiAddr = (ptCpu->auAn[iReg] + (st_u32_t)(st_i32_t)iDisp)
                 & 0x00FFFFFFu;
        break;

    case 6: /* d8(An,Xn) — brief extension word */
        uiExt  = cpu_fetch_word(ptCpu, ptMachine);
        iDisp  = (st_i8_t)(uiExt & 0xFFu);
        {
            st_u32_t uiIdx;
            int iXn  = (uiExt >> 12) & 7;
            int iXSz = (uiExt >> 11) & 1; /* 0=.W, 1=.L */

            if ((uiExt >> 15) & 1) /* An index */
                uiIdx = ptCpu->auAn[iXn];
            else
                uiIdx = ptCpu->auDn[iXn];

            if (!iXSz) /* sign-extend .W */
                uiIdx = (st_u32_t)(st_i32_t)(st_i16_t)(uiIdx & 0xFFFFu);

            uiAddr = (ptCpu->auAn[iReg]
                      + (st_u32_t)(st_i32_t)iDisp
                      + uiIdx)
                     & 0x00FFFFFFu;
        }
        break;

    case 7:
        switch (iReg)
        {
        case 0: /* abs.W */
            uiExt  = cpu_fetch_word(ptCpu, ptMachine);
            uiAddr = (st_u32_t)(st_i32_t)(st_i16_t)uiExt & 0x00FFFFFFu;
            break;

        case 1: /* abs.L */
            uiExt  = cpu_fetch_word(ptCpu, ptMachine);
            uiAddr = (st_u32_t)uiExt << 16u;
            uiExt  = cpu_fetch_word(ptCpu, ptMachine);
            uiAddr = (uiAddr | uiExt) & 0x00FFFFFFu;
            break;

        case 2: /* d16(PC) */
            uiAddr = ptCpu->uiPC; /* PC after fetch of this ext word */
            uiExt  = cpu_fetch_word(ptCpu, ptMachine);
            iDisp  = (st_i16_t)uiExt;
            /* The extension word address was already advanced; EA
             * = address_of_extension_word + displacement */
            uiAddr = (uiAddr + (st_u32_t)(st_i32_t)iDisp)
                     & 0x00FFFFFFu;
            break;

        case 3: /* d8(PC,Xn) */
            {
                st_u32_t uiBase = ptCpu->uiPC;
                uiExt  = cpu_fetch_word(ptCpu, ptMachine);
                iDisp  = (st_i8_t)(uiExt & 0xFFu);
                {
                    st_u32_t uiIdx;
                    int iXn  = (uiExt >> 12) & 7;
                    int iXSz = (uiExt >> 11) & 1;

                    if ((uiExt >> 15) & 1)
                        uiIdx = ptCpu->auAn[iXn];
                    else
                        uiIdx = ptCpu->auDn[iXn];

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
            uiExt = cpu_fetch_word(ptCpu, ptMachine);
            if (iSz == SZ_LONG)
            {
                st_u32_t uiHi = uiExt;
                uiExt   = cpu_fetch_word(ptCpu, ptMachine);
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
            eR = st_read_byte(ptMachine, uiAddr, &uiB);
            uiRaw = uiB;
        }
        break;
    case SZ_WORD:
        {
            st_u16_t uiW;
            eR = st_read_word(ptMachine, uiAddr, &uiW);
            uiRaw = uiW;
        }
        break;
    default:
        eR = st_read_long(ptMachine, uiAddr, &uiRaw);
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
 */
static st_error_t cpu_ea_write(cpu68k_t     *ptCpu,
                                st_machine_t *ptMachine,
                                int           iMode,
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
            ptCpu->auDn[iReg] = uiVal;
        else if (iSz == SZ_WORD)
            ptCpu->auDn[iReg] = (ptCpu->auDn[iReg] & 0xFFFF0000u) | uiVal;
        else
            ptCpu->auDn[iReg] = (ptCpu->auDn[iReg] & 0xFFFFFF00u) | uiVal;
        return ST_NO_ERROR;

    case 1: /* An — always full 32-bit */
        ptCpu->auAn[iReg] = uiVal;
        return ST_NO_ERROR;

    case 2: /* (An) */
        uiAddr = ptCpu->auAn[iReg] & 0x00FFFFFFu;
        break;

    case 3: /* (An)+ */
        uiAddr = ptCpu->auAn[iReg] & 0x00FFFFFFu;
        if (iSz == SZ_BYTE && iReg == 7)
            ptCpu->auAn[iReg] += 2u;
        else
            ptCpu->auAn[iReg] += (st_u32_t)(1 << iSz);
        break;

    case 4: /* -(An) */
        if (iSz == SZ_BYTE && iReg == 7)
            ptCpu->auAn[iReg] -= 2u;
        else
            ptCpu->auAn[iReg] -= (st_u32_t)(1 << iSz);
        uiAddr = ptCpu->auAn[iReg] & 0x00FFFFFFu;
        break;

    case 5: /* d16(An) */
        uiExt = cpu_fetch_word(ptCpu, ptMachine);
        iDisp = (st_i16_t)uiExt;
        uiAddr = (ptCpu->auAn[iReg] + (st_u32_t)(st_i32_t)iDisp)
                 & 0x00FFFFFFu;
        break;

    case 6: /* d8(An,Xn) */
        uiExt  = cpu_fetch_word(ptCpu, ptMachine);
        iDisp  = (st_i8_t)(uiExt & 0xFFu);
        {
            st_u32_t uiIdx;
            int iXn  = (uiExt >> 12) & 7;
            int iXSz = (uiExt >> 11) & 1;

            if ((uiExt >> 15) & 1)
                uiIdx = ptCpu->auAn[iXn];
            else
                uiIdx = ptCpu->auDn[iXn];

            if (!iXSz)
                uiIdx = (st_u32_t)(st_i32_t)(st_i16_t)(uiIdx & 0xFFFFu);

            uiAddr = (ptCpu->auAn[iReg]
                      + (st_u32_t)(st_i32_t)iDisp
                      + uiIdx)
                     & 0x00FFFFFFu;
        }
        break;

    case 7:
        switch (iReg)
        {
        case 0: /* abs.W */
            uiExt  = cpu_fetch_word(ptCpu, ptMachine);
            uiAddr = (st_u32_t)(st_i32_t)(st_i16_t)uiExt & 0x00FFFFFFu;
            break;

        case 1: /* abs.L */
            uiExt  = cpu_fetch_word(ptCpu, ptMachine);
            uiAddr = (st_u32_t)uiExt << 16u;
            uiExt  = cpu_fetch_word(ptCpu, ptMachine);
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
        eR = st_write_byte(ptMachine, uiAddr, (st_u8_t)uiVal);
        break;
    case SZ_WORD:
        eR = st_write_word(ptMachine, uiAddr, (st_u16_t)uiVal);
        break;
    default:
        eR = st_write_long(ptMachine, uiAddr, uiVal);
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
 * Only valid for control-type EAs: modes 2/5/6 and 7.0/7.1/7.2/7.3.
 */
static st_error_t cpu_ea_calc_addr(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    int           iMode,
                                    int           iReg,
                                    st_u32_t     *puiAddr)
{
    st_u16_t uiExt;
    int      iDisp;

    switch (iMode)
    {
    case 2: /* (An) */
        *puiAddr = ptCpu->auAn[iReg] & 0x00FFFFFFu;
        return ST_NO_ERROR;

    case 5: /* d16(An) */
        uiExt  = cpu_fetch_word(ptCpu, ptMachine);
        iDisp  = (st_i16_t)uiExt;
        *puiAddr = (ptCpu->auAn[iReg] + (st_u32_t)(st_i32_t)iDisp)
                   & 0x00FFFFFFu;
        return ST_NO_ERROR;

    case 6: /* d8(An,Xn) */
        uiExt  = cpu_fetch_word(ptCpu, ptMachine);
        iDisp  = (st_i8_t)(uiExt & 0xFFu);
        {
            st_u32_t uiIdx;
            int iXn  = (uiExt >> 12) & 7;
            int iXSz = (uiExt >> 11) & 1;

            if ((uiExt >> 15) & 1)
                uiIdx = ptCpu->auAn[iXn];
            else
                uiIdx = ptCpu->auDn[iXn];

            if (!iXSz)
                uiIdx = (st_u32_t)(st_i32_t)(st_i16_t)(uiIdx & 0xFFFFu);

            *puiAddr = (ptCpu->auAn[iReg]
                        + (st_u32_t)(st_i32_t)iDisp
                        + uiIdx)
                       & 0x00FFFFFFu;
        }
        return ST_NO_ERROR;

    case 7:
        switch (iReg)
        {
        case 0: /* abs.W */
            uiExt    = cpu_fetch_word(ptCpu, ptMachine);
            *puiAddr = (st_u32_t)(st_i32_t)(st_i16_t)uiExt
                       & 0x00FFFFFFu;
            return ST_NO_ERROR;

        case 1: /* abs.L */
            uiExt    = cpu_fetch_word(ptCpu, ptMachine);
            *puiAddr = (st_u32_t)uiExt << 16u;
            uiExt    = cpu_fetch_word(ptCpu, ptMachine);
            *puiAddr = (*puiAddr | uiExt) & 0x00FFFFFFu;
            return ST_NO_ERROR;

        case 2: /* d16(PC) */
            {
                st_u32_t uiBase = ptCpu->uiPC;
                uiExt   = cpu_fetch_word(ptCpu, ptMachine);
                iDisp   = (st_i16_t)uiExt;
                *puiAddr = (uiBase + (st_u32_t)(st_i32_t)iDisp)
                           & 0x00FFFFFFu;
            }
            return ST_NO_ERROR;

        case 3: /* d8(PC,Xn) */
            {
                st_u32_t uiBase = ptCpu->uiPC;
                st_u32_t uiIdx;
                int      iXn, iXSz;

                uiExt  = cpu_fetch_word(ptCpu, ptMachine);
                iDisp  = (st_i8_t)(uiExt & 0xFFu);
                iXn    = (uiExt >> 12) & 7;
                iXSz   = (uiExt >> 11) & 1;

                if ((uiExt >> 15) & 1)
                    uiIdx = ptCpu->auAn[iXn];
                else
                    uiIdx = ptCpu->auDn[iXn];

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
 * Encoding:
 *   [15-12] size: 0x1=byte, 0x3=word, 0x2=long
 *   [11- 9] dest reg (bits in reversed position)
 *   [ 8- 6] dest mode
 *   [ 5- 3] src mode
 *   [ 2- 0] src reg
 */
static st_error_t cpu_exec_move(cpu68k_t     *ptCpu,
                                 st_machine_t *ptMachine,
                                 st_u16_t      uiOpc)
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

    eR = cpu_ea_read(ptCpu, ptMachine, iSrcMode, iSrcReg, iSz, &uiVal);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    if (iDstMode == 1)
    {
        /* MOVEA — sign-extend word, no flags */
        if (iSz == SZ_WORD)
            uiVal = (st_u32_t)(st_i32_t)(st_i16_t)(uiVal & 0xFFFFu);
        ptCpu->auAn[iDstReg] = uiVal;
        return ST_NO_ERROR;
    }

    /* Normal MOVE — set flags, then write destination */
    cpu_flags_nz(ptCpu, uiVal, iSz);
    cpu_flags_clr_vc(ptCpu);

    eR = cpu_ea_write(ptCpu, ptMachine, iDstMode, iDstReg, iSz, uiVal);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    return ST_NO_ERROR;
}

/*
 * cpu_exec_moveq() - MOVEQ #imm8,Dn (group 0x7).
 *
 * Encoding: 0111 DDD 0 IIIIIIII
 */
static st_error_t cpu_exec_moveq(cpu68k_t *ptCpu, st_u16_t uiOpc)
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
    ptCpu->auDn[iDn] = uiVal;

    cpu_flags_nz(ptCpu, uiVal, SZ_LONG);
    cpu_flags_clr_vc(ptCpu);

    return ST_NO_ERROR;
}

/*
 * cpu_exec_clr() - CLR.sz EA (within group 0x4).
 *
 * Encoding: 0100 0010 SS MMMRRR
 */
static st_error_t cpu_exec_clr(cpu68k_t     *ptCpu,
                                 st_machine_t *ptMachine,
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

    eR = cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, 0u);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    /* CLR: N=0, Z=1, V=0, C=0 */
    ptCpu->uiSR &= (st_u16_t)~(CPU_SR_N | CPU_SR_V | CPU_SR_C);
    ptCpu->uiSR |= (st_u16_t)CPU_SR_Z;

    return ST_NO_ERROR;
}

/*
 * cpu_exec_lea() - LEA EA,An (within group 0x4).
 *
 * Encoding: 0100 AAA 111 MMMRRR   (mask 0xF1C0 == 0x41C0)
 */
static st_error_t cpu_exec_lea(cpu68k_t     *ptCpu,
                                 st_machine_t *ptMachine,
                                 st_u16_t      uiOpc)
{
    int        iAn   = (uiOpc >> 9) & 7;
    int        iMode = (uiOpc >> 3) & 7;
    int        iReg  = uiOpc & 7;
    st_u32_t   uiAddr;
    st_error_t eR;

    eR = cpu_ea_calc_addr(ptCpu, ptMachine, iMode, iReg, &uiAddr);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    ptCpu->auAn[iAn] = uiAddr;
    /* LEA does not affect flags */
    return ST_NO_ERROR;
}

/*
 * cpu_exec_swap() - SWAP Dn (within group 0x4).
 *
 * Encoding: 0100 1000 0100 0DDD   (mask 0xFFF8 == 0x4840)
 */
static st_error_t cpu_exec_swap(cpu68k_t *ptCpu, st_u16_t uiOpc)
{
    int      iDn  = uiOpc & 7;
    st_u32_t uiOld = ptCpu->auDn[iDn];
    st_u32_t uiNew;

    uiNew = ((uiOld & 0x0000FFFFu) << 16u)
            | ((uiOld & 0xFFFF0000u) >> 16u);
    ptCpu->auDn[iDn] = uiNew;

    /* SWAP: N, Z from full 32-bit result; V=0, C=0 */
    cpu_flags_nz(ptCpu, uiNew, SZ_LONG);
    cpu_flags_clr_vc(ptCpu);

    return ST_NO_ERROR;
}

/*
 * cpu_exec_unary() - NEG / NEGX / NOT / TST  (group 0x4 unary ops).
 *
 * szMnem : mnemonic string for logging
 * iOp    : 0=NEG, 1=NEGX, 2=NOT, 3=TST
 * Encoding: 0100 oo10 SS MMMRRR   (oo = 01/10/11/00 for TST/NOT/NEG/NEGX)
 */
static st_error_t cpu_exec_unary(cpu68k_t     *ptCpu,
                                   st_machine_t *ptMachine,
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

    eR = cpu_ea_read(ptCpu, ptMachine, iMode, iReg, iSz, &uiSrc);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    uiSrc &= uiMask;

    switch (iOp)
    {
    case 0: /* NEG: 0 - src */
        uiRes = (st_u32_t)(-(st_i32_t)uiSrc) & uiMask;
        cpu_flags_sub(ptCpu, uiSrc, 0u, uiRes, iSz);
        eR = cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);
        break;

    case 1: /* NEGX: 0 - src - X */
        uiX   = CPU_FLAG_X(ptCpu) ? 1u : 0u;
        uiRes = (st_u32_t)(-(st_i32_t)uiSrc - (st_i32_t)uiX) & uiMask;
        cpu_flags_sub(ptCpu, uiSrc + uiX, 0u, uiRes, iSz);
        /* NEGX: Z cleared only if result != 0 (not set if result == 0) */
        if (uiRes != 0u)
            ptCpu->uiSR &= (st_u16_t)~CPU_SR_Z;
        eR = cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);
        break;

    case 2: /* NOT: ~src */
        uiRes = (~uiSrc) & uiMask;
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        eR = cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);
        break;

    default: /* TST: result discarded, flags only */
        cpu_flags_nz(ptCpu, uiSrc, iSz);
        cpu_flags_clr_vc(ptCpu);
        eR = ST_NO_ERROR;
        break;
    }

    return eR;
}

/*
 * cpu_exec_group0() - Immediate ops (ADDI/SUBI/CMPI/ANDI/ORI/EORI).
 *
 * Encoding: 0000 TTTT SS MMMRRR  + imm word(s)
 *   TTTT: 0000=ORI, 0010=ANDI, 0100=SUBI, 0110=ADDI, 1010=EORI, 1100=CMPI
 */
static st_error_t cpu_exec_group0(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    st_u16_t      uiOpc)
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
    uiExtW = cpu_fetch_word(ptCpu, ptMachine);
    if (iSz == SZ_LONG)
    {
        st_u32_t uiHi = uiExtW;
        uiExtW = cpu_fetch_word(ptCpu, ptMachine);
        uiImm  = (uiHi << 16u) | uiExtW;
    }
    else if (iSz == SZ_BYTE)
        uiImm = uiExtW & 0xFFu;
    else
        uiImm = uiExtW;

    /* Read destination EA */
    eR = cpu_ea_read(ptCpu, ptMachine, iMode, iReg, iSz, &uiDst);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    switch (iOp)
    {
    case 0x0: /* ORI */
        uiRes = (uiDst | uiImm) & g_auiMask[iSz];
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);

    case 0x2: /* ANDI */
        uiRes = (uiDst & uiImm) & g_auiMask[iSz];
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);

    case 0x4: /* SUBI */
        uiRes = (uiDst - uiImm) & g_auiMask[iSz];
        cpu_flags_sub(ptCpu, uiImm, uiDst, uiRes, iSz);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);

    case 0x6: /* ADDI */
        uiRes = (uiDst + uiImm) & g_auiMask[iSz];
        cpu_flags_add(ptCpu, uiImm, uiDst, uiRes, iSz);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);

    case 0xA: /* EORI */
        uiRes = (uiDst ^ uiImm) & g_auiMask[iSz];
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);

    case 0xC: /* CMPI — result discarded, flags only */
        uiRes = (uiDst - uiImm) & g_auiMask[iSz];
        cpu_flags_sub(ptCpu, uiImm, uiDst, uiRes, iSz);
        return ST_NO_ERROR;

    default:
        LOG_TODO("group0 op=0x%X DC.W 0x%04X", iOp, (unsigned)uiOpc);
        return ST_NO_ERROR;
    }
}

/*
 * cpu_exec_group5() - ADDQ / SUBQ.
 *
 * Encoding: 0101 DDD Q SS MMMRRR
 *   Q=0 → ADDQ, Q=1 → SUBQ; DDD=immediate (0=8); SS=size
 */
static st_error_t cpu_exec_group5(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    st_u16_t      uiOpc)
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
        LOG_TODO("group5: Scc/DBcc DC.W 0x%04X", (unsigned)uiOpc);
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    uiImm = (iData == 0) ? 8u : (st_u32_t)iData;

    eR = cpu_ea_read(ptCpu, ptMachine, iMode, iReg, iSz, &uiDst);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;

    if (iDir == 0)
    {
        uiRes = (uiDst + uiImm) & g_auiMask[iSz];
        /* An destination: no flags */
        if (iMode != 1)
            cpu_flags_add(ptCpu, uiImm, uiDst, uiRes, iSz);
    }
    else
    {
        uiRes = (uiDst - uiImm) & g_auiMask[iSz];
        if (iMode != 1)
            cpu_flags_sub(ptCpu, uiImm, uiDst, uiRes, iSz);
    }

    return cpu_ea_write(ptCpu, ptMachine, iMode, iReg, iSz, uiRes);
}

/*
 * cpu_exec_addx_subx() - ADDX / SUBX (register and memory forms).
 *
 * Register: MMMRRR = 000 DDD (mode=0), Memory: MMMRRR = 001 DDD
 * iDir: 0=ADDX, 1=SUBX
 */
static st_error_t cpu_exec_addx_subx(cpu68k_t     *ptCpu,
                                       st_machine_t *ptMachine,
                                       st_u16_t      uiOpc,
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

    uiX = CPU_FLAG_X(ptCpu) ? 1u : 0u;

    if (iRm == 0)
    {
        /* Register form: Dy, Dx */
        uiSrc = ptCpu->auDn[iRy] & g_auiMask[iSz];
        uiDst = ptCpu->auDn[iRx] & g_auiMask[iSz];
    }
    else
    {
        /* Memory form: -(Ay), -(Ax) */
        eR = cpu_ea_read(ptCpu, ptMachine, 4, iRy, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        eR = cpu_ea_read(ptCpu, ptMachine, 4, iRx, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
    }

    if (iDir == 0)
    {
        /* ADDX */
        uiRes = (uiDst + uiSrc + uiX) & g_auiMask[iSz];
        cpu_flags_add(ptCpu, uiSrc + uiX, uiDst, uiRes, iSz);
        /* ADDX: Z cleared only if result != 0 */
        if (uiRes != 0u)
            ptCpu->uiSR &= (st_u16_t)~CPU_SR_Z;
    }
    else
    {
        /* SUBX */
        uiRes = (uiDst - uiSrc - uiX) & g_auiMask[iSz];
        cpu_flags_sub(ptCpu, uiSrc + uiX, uiDst, uiRes, iSz);
        if (uiRes != 0u)
            ptCpu->uiSR &= (st_u16_t)~CPU_SR_Z;
    }

    if (iRm == 0)
    {
        eR = cpu_ea_write(ptCpu, ptMachine, 0, iRx, iSz, uiRes);
    }
    else
    {
        /* Write back to the pre-decremented address — re-read adjusted An */
        uiDst = ptCpu->auAn[iRx] & 0x00FFFFFFu;
        switch (iSz)
        {
        case SZ_BYTE:
            eR = st_write_byte(ptMachine, uiDst, (st_u8_t)uiRes);
            break;
        case SZ_WORD:
            eR = st_write_word(ptMachine, uiDst, (st_u16_t)uiRes);
            break;
        default:
            eR = st_write_long(ptMachine, uiDst, uiRes);
            break;
        }
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
    }
    return eR;
}

/*
 * cpu_exec_groupD() - ADD / ADDA / ADDX (group 0xD).
 */
static st_error_t cpu_exec_groupD(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    st_u16_t      uiOpc)
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

    /* ADDA: size code 3 (word) or when dest is An and sz=long */
    if (iSzCode == 3)
    {
        /* ADDA.L */
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, SZ_LONG, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        ptCpu->auAn[iReg] += uiSrc;
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    /* Check ADDX: bit8=1, mode field <= 1 (Dn or -(An)) */
    if (iDir == 1 && iMode <= 1)
        return cpu_exec_addx_subx(ptCpu, ptMachine, uiOpc, 0);

    if (iDir == 0)
    {
        /* ADD EA,Dn  (bit8=0) */
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = ptCpu->auDn[iReg] & g_auiMask[iSz];
        uiRes = (uiDst + uiSrc) & g_auiMask[iSz];
        cpu_flags_add(ptCpu, uiSrc, uiDst, uiRes, iSz);
        return cpu_ea_write(ptCpu, ptMachine, 0, iReg, iSz, uiRes);
    }
    else
    {
        /* ADD Dn,EA  (bit8=1, mode>1) */
        uiSrc = ptCpu->auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst + uiSrc) & g_auiMask[iSz];
        cpu_flags_add(ptCpu, uiSrc, uiDst, uiRes, iSz);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iRn, iSz, uiRes);
    }
}

/*
 * cpu_exec_group9() - SUB / SUBA / SUBX (group 0x9).
 */
static st_error_t cpu_exec_group9(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    st_u16_t      uiOpc)
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
        /* SUBA.L */
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, SZ_LONG, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        ptCpu->auAn[iReg] -= uiSrc;
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    if (iDir == 1 && iMode <= 1)
        return cpu_exec_addx_subx(ptCpu, ptMachine, uiOpc, 1);

    if (iDir == 0)
    {
        /* SUB EA,Dn */
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = ptCpu->auDn[iReg] & g_auiMask[iSz];
        uiRes = (uiDst - uiSrc) & g_auiMask[iSz];
        cpu_flags_sub(ptCpu, uiSrc, uiDst, uiRes, iSz);
        return cpu_ea_write(ptCpu, ptMachine, 0, iReg, iSz, uiRes);
    }
    else
    {
        /* SUB Dn,EA */
        uiSrc = ptCpu->auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst - uiSrc) & g_auiMask[iSz];
        cpu_flags_sub(ptCpu, uiSrc, uiDst, uiRes, iSz);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iRn, iSz, uiRes);
    }
}

/*
 * cpu_exec_groupB() - CMP / CMPA / EOR / CMPM (group 0xB).
 */
static st_error_t cpu_exec_groupB(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    st_u16_t      uiOpc)
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
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, SZ_LONG, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = ptCpu->auAn[iReg];
        uiRes = (uiDst - uiSrc) & g_auiMask[SZ_LONG];
        cpu_flags_sub(ptCpu, uiSrc, uiDst, uiRes, SZ_LONG);
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    if (iDir == 1 && iMode == 1)
    {
        /* CMPM (An)+,(An)+ */
        eR = cpu_ea_read(ptCpu, ptMachine, 3, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        eR = cpu_ea_read(ptCpu, ptMachine, 3, iReg, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst - uiSrc) & g_auiMask[iSz];
        cpu_flags_sub(ptCpu, uiSrc, uiDst, uiRes, iSz);
        return ST_NO_ERROR;
    }

    if (iDir == 1)
    {
        /* EOR Dn,EA */
        uiSrc = ptCpu->auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst ^ uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iRn, iSz, uiRes);
    }

    /* CMP EA,Dn (bit8=0) */
    eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiSrc);
    if (eR != ST_NO_ERROR)
        return ST_ERROR;
    uiDst = ptCpu->auDn[iReg] & g_auiMask[iSz];
    uiRes = (uiDst - uiSrc) & g_auiMask[iSz];
    cpu_flags_sub(ptCpu, uiSrc, uiDst, uiRes, iSz);
    return ST_NO_ERROR;
}

/*
 * cpu_exec_groupC() - AND / MULU / MULS (group 0xC).
 */
static st_error_t cpu_exec_groupC(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    st_u16_t      uiOpc)
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
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, SZ_WORD, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        if (iDir == 0)
        {
            /* MULU.W: unsigned 16x16 → 32 */
            uiRes = (uiSrc & 0xFFFFu) * (ptCpu->auDn[iReg] & 0xFFFFu);
        }
        else
        {
            /* MULS.W: signed 16x16 → 32 */
            uiRes = (st_u32_t)((st_i32_t)(st_i16_t)(uiSrc & 0xFFFFu)
                    * (st_i32_t)(st_i16_t)(ptCpu->auDn[iReg] & 0xFFFFu));
        }
        ptCpu->auDn[iReg] = uiRes;
        cpu_flags_nz(ptCpu, uiRes, SZ_LONG);
        cpu_flags_clr_vc(ptCpu);
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    if (iDir == 0)
    {
        /* AND EA,Dn */
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = ptCpu->auDn[iReg] & g_auiMask[iSz];
        uiRes = (uiDst & uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        return cpu_ea_write(ptCpu, ptMachine, 0, iReg, iSz, uiRes);
    }
    else
    {
        /* AND Dn,EA */
        uiSrc = ptCpu->auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst & uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iRn, iSz, uiRes);
    }
}

/*
 * cpu_exec_group8() - OR / DIVU / DIVS (group 0x8).
 */
static st_error_t cpu_exec_group8(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    st_u16_t      uiOpc)
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
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, SZ_WORD, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiSrc &= 0xFFFFu;
        if (uiSrc == 0u)
        {
            LOG_TODO("DIV by zero — exception UC23");
            return ST_NO_ERROR;
        }
        uiDst = ptCpu->auDn[iReg];
        if (iDir == 0)
        {
            /* DIVU.W */
            st_u32_t uiQ = uiDst / uiSrc;
            st_u32_t uiR = uiDst % uiSrc;
            ptCpu->auDn[iReg] = ((uiR & 0xFFFFu) << 16u)
                                 | (uiQ & 0xFFFFu);
        }
        else
        {
            /* DIVS.W */
            st_i32_t iDst  = (st_i32_t)uiDst;
            st_i32_t iSrc  = (st_i32_t)(st_i16_t)uiSrc;
            st_i32_t iQ    = iDst / iSrc;
            st_i32_t iR    = iDst % iSrc;
            ptCpu->auDn[iReg] = (((st_u32_t)(iR & 0xFFFF) << 16u))
                                 | ((st_u32_t)(iQ & 0xFFFF));
        }
        cpu_flags_nz(ptCpu, ptCpu->auDn[iReg] & 0xFFFFu, SZ_WORD);
        cpu_flags_clr_vc(ptCpu);
        return ST_NO_ERROR;
    }

    if      (iSzCode == 0) iSz = SZ_BYTE;
    else if (iSzCode == 1) iSz = SZ_WORD;
    else                   iSz = SZ_LONG;

    if (iDir == 0)
    {
        /* OR EA,Dn */
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiSrc);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiDst = ptCpu->auDn[iReg] & g_auiMask[iSz];
        uiRes = (uiDst | uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        return cpu_ea_write(ptCpu, ptMachine, 0, iReg, iSz, uiRes);
    }
    else
    {
        /* OR Dn,EA */
        uiSrc = ptCpu->auDn[iReg] & g_auiMask[iSz];
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiDst);
        if (eR != ST_NO_ERROR)
            return ST_ERROR;
        uiRes = (uiDst | uiSrc) & g_auiMask[iSz];
        cpu_flags_nz(ptCpu, uiRes, iSz);
        cpu_flags_clr_vc(ptCpu);
        return cpu_ea_write(ptCpu, ptMachine, iMode, iRn, iSz, uiRes);
    }
}

/*
 * cpu_exec_groupE() - Shifts and rotations (group 0xE).
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
static st_error_t cpu_exec_groupE(cpu68k_t     *ptCpu,
                                    st_machine_t *ptMachine,
                                    st_u16_t      uiOpc)
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
        eR = cpu_ea_read(ptCpu, ptMachine, iMode, iRn, iSz, &uiVal);
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
            iCount = (int)(ptCpu->auDn[(uiOpc >> 9) & 7] & 63u);
        }

        uiVal = ptCpu->auDn[iRn] & g_auiMask[iSz];
    }

    uiMsb  = g_auiMsb[iSz];
    uiMask = g_auiMask[iSz];
    iX     = CPU_FLAG_X(ptCpu) ? 1 : 0;
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
                        ptCpu->uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        ptCpu->uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                else
                {
                    uiLastBit = uiRes & 1u;
                    uiRes     = ((uiRes >> 1u) | (uiRes & uiMsb)) & uiMask;
                    if (uiLastBit)
                        ptCpu->uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        ptCpu->uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                break;

            case 1: /* LS */
                if (iDir)
                {
                    uiLastBit = (uiRes & uiMsb) ? 1u : 0u;
                    uiRes     = (uiRes << 1u) & uiMask;
                    if (uiLastBit)
                        ptCpu->uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        ptCpu->uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                else
                {
                    uiLastBit = uiRes & 1u;
                    uiRes     = (uiRes >> 1u) & uiMask;
                    if (uiLastBit)
                        ptCpu->uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        ptCpu->uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                break;

            case 2: /* ROX */
                if (iDir)
                {
                    uiLastBit = (uiRes & uiMsb) ? 1u : 0u;
                    uiRes     = ((uiRes << 1u) | (st_u32_t)iX) & uiMask;
                    iX        = (int)uiLastBit;
                    if (iX)
                        ptCpu->uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        ptCpu->uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
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
                        ptCpu->uiSR |=  (st_u16_t)(CPU_SR_C | CPU_SR_X);
                    else
                        ptCpu->uiSR &= (st_u16_t)~(CPU_SR_C | CPU_SR_X);
                }
                break;

            default: /* RO */
                if (iDir)
                {
                    uiLastBit = (uiRes & uiMsb) ? 1u : 0u;
                    uiRes     = ((uiRes << 1u)
                                 | uiLastBit) & uiMask;
                    if (uiLastBit)
                        ptCpu->uiSR |=  (st_u16_t)CPU_SR_C;
                    else
                        ptCpu->uiSR &= (st_u16_t)~CPU_SR_C;
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
                        ptCpu->uiSR |=  (st_u16_t)CPU_SR_C;
                    else
                        ptCpu->uiSR &= (st_u16_t)~CPU_SR_C;
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
            ptCpu->uiSR &= (st_u16_t)~CPU_SR_V; /* simplified */
        else
            ptCpu->uiSR &= (st_u16_t)~CPU_SR_V;
    }
    else
    {
        ptCpu->uiSR &= (st_u16_t)~CPU_SR_V;
    }

    cpu_flags_nz(ptCpu, uiRes, iSz);

    if (iSzCode == 3)
        return cpu_ea_write(ptCpu, ptMachine, iMode, iRn, SZ_WORD, uiRes);
    else
        return cpu_ea_write(ptCpu, ptMachine, 0, iRn, iSz, uiRes);
}

/*
 * cpu_exec_ext() - EXT.W Dn or EXT.L Dn  (group 0x4).
 *
 * EXT.W : 0100 1000 1000 0DDD  (0xFFF8 == 0x4880)
 * EXT.L : 0100 1000 1100 0DDD  (0xFFF8 == 0x48C0)
 */
static st_error_t cpu_exec_ext(cpu68k_t *ptCpu, st_u16_t uiOpc)
{
    int      iDn  = uiOpc & 7;
    st_u32_t uiVal;
    int      iSz;

    if ((uiOpc & 0xFFF8u) == 0x4880u)
    {
        /* EXT.W: sign-extend byte to word */
        uiVal = (st_u32_t)(st_i32_t)(st_i8_t)(ptCpu->auDn[iDn] & 0xFFu);
        ptCpu->auDn[iDn] = (ptCpu->auDn[iDn] & 0xFFFF0000u)
                           | (uiVal & 0xFFFFu);
        iSz = SZ_WORD;
        uiVal &= 0xFFFFu;
    }
    else
    {
        /* EXT.L: sign-extend word to long */
        uiVal = (st_u32_t)(st_i32_t)(st_i16_t)
                (ptCpu->auDn[iDn] & 0xFFFFu);
        ptCpu->auDn[iDn] = uiVal;
        iSz = SZ_LONG;
    }

    cpu_flags_nz(ptCpu, uiVal, iSz);
    cpu_flags_clr_vc(ptCpu);
    return ST_NO_ERROR;
}

/*
 * cpu_exec_misc4() - Dispatch group 0x4 instructions.
 */
static st_error_t cpu_exec_misc4(cpu68k_t     *ptCpu,
                                   st_machine_t *ptMachine,
                                   st_u16_t      uiOpc)
{
    /* LEA: 0100 AAA 111 MMMRRR */
    if ((uiOpc & 0xF1C0u) == 0x41C0u)
        return cpu_exec_lea(ptCpu, ptMachine, uiOpc);

    /* EXT.W / EXT.L */
    if ((uiOpc & 0xFFF8u) == 0x4880u || (uiOpc & 0xFFF8u) == 0x48C0u)
        return cpu_exec_ext(ptCpu, uiOpc);

    /* SWAP: 0100 1000 0100 0DDD */
    if ((uiOpc & 0xFFF8u) == 0x4840u)
        return cpu_exec_swap(ptCpu, uiOpc);

    /* NEGX: 0100 0000 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4000u)
        return cpu_exec_unary(ptCpu, ptMachine, uiOpc, 1);

    /* CLR: 0100 0010 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4200u)
        return cpu_exec_clr(ptCpu, ptMachine, uiOpc);

    /* NEG: 0100 0100 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4400u)
        return cpu_exec_unary(ptCpu, ptMachine, uiOpc, 0);

    /* NOT: 0100 0110 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4600u)
        return cpu_exec_unary(ptCpu, ptMachine, uiOpc, 2);

    /* TST: 0100 1010 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4A00u)
        return cpu_exec_unary(ptCpu, ptMachine, uiOpc, 3);

    LOG_TODO("cpu_exec_misc4: unimplemented 0x%04X (UC23)",
             (unsigned)uiOpc);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_error_t cpu_init(cpu68k_t *ptCpu, const st_machine_t *ptMachine)
{
    st_u32_t   uiSSP;
    st_u32_t   uiPC;
    st_error_t eR;

    LOG_TRACE("ptCpu=%p ptMachine=%p",
              (void *)ptCpu, (void *)ptMachine);
    if (ptCpu == NULL || ptMachine == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    memset(ptCpu, 0, sizeof(cpu68k_t));

    eR = st_read_long(ptMachine, CPU_VEC_RESET_SSP, &uiSSP);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("failed to read reset SSP vector");
        return ST_ERROR;
    }
    eR = st_read_long(ptMachine, CPU_VEC_RESET_PC, &uiPC);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("failed to read reset PC vector");
        return ST_ERROR;
    }

    ptCpu->uiSSP    = uiSSP;
    ptCpu->auAn[7]  = uiSSP;
    ptCpu->uiPC     = uiPC;
    ptCpu->uiSR     = CPU_SR_S | CPU_SR_I_MASK;
    ptCpu->eState   = CPU_STATE_RUNNING;

    LOG_INFO("cpu_init: SSP=0x%08X PC=0x%08X SR=0x%04X",
             uiSSP, uiPC, (unsigned)ptCpu->uiSR);
    return ST_NO_ERROR;
}

st_error_t cpu_reset(cpu68k_t *ptCpu, const st_machine_t *ptMachine)
{
    LOG_TRACE("ptCpu=%p", (void *)ptCpu);
    if (ptCpu == NULL || ptMachine == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    return cpu_init(ptCpu, ptMachine);
}

st_error_t cpu_step(cpu68k_t          *ptCpu,
                     st_machine_t      *ptMachine,
                     cpu_step_result_t *ptResult)
{
    st_u16_t   uiOpcode;
    st_u32_t   uiPCBefore;
    st_error_t eR;

    LOG_TRACE("PC=0x%06X", ptCpu ? ptCpu->uiPC : 0u);
    if (ptCpu == NULL || ptMachine == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    if (ptCpu->eState != CPU_STATE_RUNNING)
    {
        LOG_INFO("cpu_step: CPU not running (state=%d)",
                 (int)ptCpu->eState);
        return ST_NO_ERROR;
    }

    uiPCBefore = ptCpu->uiPC;

    /* Fetch opcode word — advances PC by 2 */
    uiOpcode = cpu_fetch_word(ptCpu, ptMachine);

    if (ptResult != NULL)
    {
        memset(ptResult, 0, sizeof(cpu_step_result_t));
        ptResult->uiOpcode   = uiOpcode;
        ptResult->uiPCBefore = uiPCBefore;
    }

    /* Dispatch on top nibble */
    switch ((uiOpcode >> 12) & 0xFu)
    {
    case 0x0: /* Immediate ops: ORI/ANDI/SUBI/ADDI/EORI/CMPI */
        eR = cpu_exec_group0(ptCpu, ptMachine, uiOpcode);
        break;

    case 0x1: /* MOVE.B */
    case 0x2: /* MOVE.L */
    case 0x3: /* MOVE.W / MOVEA.W */
        eR = cpu_exec_move(ptCpu, ptMachine, uiOpcode);
        break;

    case 0x4: /* Misc: LEA/CLR/SWAP/NEG/NOT/TST/EXT + UC23 */
        eR = cpu_exec_misc4(ptCpu, ptMachine, uiOpcode);
        break;

    case 0x5: /* ADDQ / SUBQ */
        eR = cpu_exec_group5(ptCpu, ptMachine, uiOpcode);
        break;

    case 0x7: /* MOVEQ */
        eR = cpu_exec_moveq(ptCpu, uiOpcode);
        break;

    case 0x8: /* OR / DIVU / DIVS */
        eR = cpu_exec_group8(ptCpu, ptMachine, uiOpcode);
        break;

    case 0x9: /* SUB / SUBA / SUBX */
        eR = cpu_exec_group9(ptCpu, ptMachine, uiOpcode);
        break;

    case 0xB: /* CMP / CMPA / EOR / CMPM */
        eR = cpu_exec_groupB(ptCpu, ptMachine, uiOpcode);
        break;

    case 0xC: /* AND / MULU / MULS */
        eR = cpu_exec_groupC(ptCpu, ptMachine, uiOpcode);
        break;

    case 0xD: /* ADD / ADDA / ADDX */
        eR = cpu_exec_groupD(ptCpu, ptMachine, uiOpcode);
        break;

    case 0xE: /* Shifts / Rotations */
        eR = cpu_exec_groupE(ptCpu, ptMachine, uiOpcode);
        break;

    default:
        LOG_TODO("cpu_step: unimplemented opcode 0x%04X "
                 "at PC=0x%06X (UC23)",
                 (unsigned)uiOpcode, uiPCBefore);
        eR = ST_NO_ERROR;
        break;
    }

    ptCpu->ulInstrCount++;

    if (ptResult != NULL)
    {
        ptResult->uiPCAfter = ptCpu->uiPC;
        ptResult->iCycles   = 4; /* TODO(UC22): accurate cycle counts */
    }

    return eR;
}

st_error_t cpu_raise_exception(cpu68k_t     *ptCpu,
                                 st_machine_t *ptMachine,
                                 st_u32_t      uiVector)
{
    LOG_TRACE("ptCpu=%p vector=0x%08X", (void *)ptCpu, uiVector);
    if (ptCpu == NULL || ptMachine == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    LOG_TODO("cpu_raise_exception: exception stacking not yet "
             "implemented (UC23) - vector=0x%08X", uiVector);
    return ST_NO_ERROR;
}
