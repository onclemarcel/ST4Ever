/*
 * CPU.c - MC68000 CPU emulator
 *
 * UC21: MOVE.B/W/L, MOVEA.W/L, MOVEQ, LEA, CLR.B/W/L, SWAP Dn
 * TODO(UC22): ADD/SUB/CMP/AND/OR/EOR/shifts
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
 * cpu_exec_misc4() - Dispatch group 0x4 instructions.
 *
 * Handles CLR, LEA, SWAP.  Other group-4 instructions (RTS, NOP,
 * TRAP, etc.) are logged as TODO for UC23.
 */
static st_error_t cpu_exec_misc4(cpu68k_t     *ptCpu,
                                   st_machine_t *ptMachine,
                                   st_u16_t      uiOpc)
{
    /* LEA: 0100 AAA 111 MMMRRR */
    if ((uiOpc & 0xF1C0u) == 0x41C0u)
        return cpu_exec_lea(ptCpu, ptMachine, uiOpc);

    /* SWAP: 0100 1000 0100 0DDD */
    if ((uiOpc & 0xFFF8u) == 0x4840u)
        return cpu_exec_swap(ptCpu, uiOpc);

    /* CLR: 0100 0010 SS MMMRRR */
    if ((uiOpc & 0xFF00u) == 0x4200u)
        return cpu_exec_clr(ptCpu, ptMachine, uiOpc);

    LOG_TODO("cpu_exec_misc4: unimplemented 0x%04X (UC22/UC23)",
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
    case 0x1: /* MOVE.B */
    case 0x2: /* MOVE.L */
    case 0x3: /* MOVE.W / MOVEA.W */
        eR = cpu_exec_move(ptCpu, ptMachine, uiOpcode);
        break;

    case 0x4: /* Misc: LEA / CLR / SWAP + others (UC22/UC23) */
        eR = cpu_exec_misc4(ptCpu, ptMachine, uiOpcode);
        break;

    case 0x7: /* MOVEQ */
        eR = cpu_exec_moveq(ptCpu, uiOpcode);
        break;

    default:
        LOG_TODO("cpu_step: unimplemented opcode 0x%04X "
                 "at PC=0x%06X (UC22/UC23)",
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
