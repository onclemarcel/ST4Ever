/*
 * CPU.c - MC68000 CPU emulator (stub)
 *
 * TODO(UC21): MOVE/MOVEQ/LEA/CLR/SWAP
 * TODO(UC22): ADD/SUB/CMP/AND/OR/EOR/shifts
 * TODO(UC23): BRA/Bcc/JSR/RTS/TRAP + exception vectors
 */
#include "CPU.h"
#include "trace.h"
#include <string.h>

st_error_t cpu_init(cpu68k_t *ptCpu, const st_machine_t *ptMachine)
{
    st_u32_t uiSSP;
    st_u32_t uiPC;
    st_error_t eR;

    LOG_TRACE("ptCpu=%p ptMachine=%p",
              (void *)ptCpu, (void *)ptMachine);
    if (ptCpu == NULL || ptMachine == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    memset(ptCpu, 0, sizeof(cpu68k_t));

    /* Read reset vectors from address space */
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

    ptCpu->uiSSP  = uiSSP;
    ptCpu->auAn[7] = uiSSP;   /* A7 = USP = SSP at reset */
    ptCpu->uiPC   = uiPC;
    /* SR: supervisor mode, interrupt mask = 7 */
    ptCpu->uiSR   = CPU_SR_S | CPU_SR_I_MASK;
    ptCpu->eState = CPU_STATE_RUNNING;

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
    st_error_t eR;

    LOG_TRACE("PC=0x%08X", ptCpu ? ptCpu->uiPC : 0u);
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

    /* Fetch opcode word */
    eR = st_read_word(ptMachine, ptCpu->uiPC, &uiOpcode);
    if (eR != ST_NO_ERROR)
    {
        LOG_ERROR("bus error fetching opcode at 0x%08X", ptCpu->uiPC);
        return ST_ERROR;
    }

    if (ptResult != NULL)
    {
        memset(ptResult, 0, sizeof(cpu_step_result_t));
        ptResult->uiOpcode   = uiOpcode;
        ptResult->uiPCBefore = ptCpu->uiPC;
    }

    LOG_TODO("cpu_step: opcode dispatch table not yet implemented "
             "(UC21) - opcode=0x%04X at PC=0x%08X",
             (unsigned)uiOpcode, ptCpu->uiPC);

    /* Advance PC by 2 so we don't loop forever in stubs */
    ptCpu->uiPC += 2;
    ptCpu->ulInstrCount++;

    if (ptResult != NULL)
    {
        ptResult->uiPCAfter = ptCpu->uiPC;
        ptResult->iCycles   = 4;
    }

    return ST_NO_ERROR;
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
