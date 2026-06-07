/*
 * CPU.h - ST4Ever Motorola 68000 CPU emulator
 *
 * Models the MC68000 register file and execution state.
 *
 * Register file:
 *   D0-D7   Data registers (32-bit, byte/word/long operations)
 *   A0-A7   Address registers (32-bit; A7 = USP stack pointer)
 *   A7'     Supervisor stack pointer (SSP, accessible in supervisor mode)
 *   PC      Program counter (24-bit effective, stored as 32-bit)
 *   SR      Status register: T S . . I2 I1 I0 . . . X N Z V C
 *             T  = trace mode
 *             S  = supervisor mode (0=user, 1=supervisor)
 *             I2-I0 = interrupt mask (0-7)
 *             X  = extend flag
 *             N  = negative flag
 *             Z  = zero flag
 *             V  = overflow flag
 *             C  = carry flag
 *
 * Opcode dispatch strategy (R2):
 *   Primary table: 256 entries indexed by (opcode >> 8).
 *   Each entry points to a handler group that decodes the full
 *   opcode including effective address mode and size field.
 *   See CPU.c for the table definitions.
 *
 * UC21: MOVE/MOVEQ/LEA/CLR/SWAP.
 * UC22: ADD/SUB/CMP/AND/OR/EOR/shifts + NEG/NOT/TST/EXT/ADDQ/SUBQ.
 * TODO(UC23): Implement BRA/Bcc/JSR/RTS/TRAP + exception vectors.
 */

#ifndef CPU_H
#define CPU_H

#include "common.h"
#include "ST.h"

/* ------------------------------------------------------------------
 * Status register bit masks
 * ------------------------------------------------------------------ */

#define CPU_SR_C    0x0001u   /* Carry                               */
#define CPU_SR_V    0x0002u   /* Overflow                            */
#define CPU_SR_Z    0x0004u   /* Zero                                */
#define CPU_SR_N    0x0008u   /* Negative                            */
#define CPU_SR_X    0x0010u   /* Extend                              */
#define CPU_SR_I0   0x0100u   /* Interrupt mask bit 0                */
#define CPU_SR_I1   0x0200u   /* Interrupt mask bit 1                */
#define CPU_SR_I2   0x0400u   /* Interrupt mask bit 2                */
#define CPU_SR_I_MASK 0x0700u /* Full interrupt mask field           */
#define CPU_SR_S    0x2000u   /* Supervisor mode                     */
#define CPU_SR_T    0x8000u   /* Trace mode                          */

/* Helper macros for flag testing */
#define CPU_FLAG_C(pCpu)  (((pCpu)->uiSR & CPU_SR_C) != 0)
#define CPU_FLAG_V(pCpu)  (((pCpu)->uiSR & CPU_SR_V) != 0)
#define CPU_FLAG_Z(pCpu)  (((pCpu)->uiSR & CPU_SR_Z) != 0)
#define CPU_FLAG_N(pCpu)  (((pCpu)->uiSR & CPU_SR_N) != 0)
#define CPU_FLAG_X(pCpu)  (((pCpu)->uiSR & CPU_SR_X) != 0)

/* ------------------------------------------------------------------
 * Exception vector table addresses
 * ------------------------------------------------------------------ */

#define CPU_VEC_RESET_SSP   0x000000u  /* Initial SSP value          */
#define CPU_VEC_RESET_PC    0x000004u  /* Initial PC value           */
#define CPU_VEC_BUS_ERR     0x000008u  /* Bus error                  */
#define CPU_VEC_ADDR_ERR    0x00000Cu  /* Address error              */
#define CPU_VEC_ILLEGAL     0x000010u  /* Illegal instruction        */
#define CPU_VEC_DIV_ZERO    0x000014u  /* Division by zero           */
#define CPU_VEC_CHK         0x000018u  /* CHK instruction            */
#define CPU_VEC_TRAPV       0x00001Cu  /* TRAPV instruction          */
#define CPU_VEC_PRIV_VIOL   0x000020u  /* Privilege violation        */
#define CPU_VEC_TRACE       0x000024u  /* Trace exception            */
#define CPU_VEC_LINE_A      0x000028u  /* Line 1010 emulator (Line-A)*/
#define CPU_VEC_LINE_F      0x00002Cu  /* Line 1111 emulator         */
#define CPU_VEC_TRAP_BASE   0x000080u  /* TRAP #0 vector             */
#define CPU_VEC_TRAP(n)     (CPU_VEC_TRAP_BASE + (st_u32_t)(n) * 4)

/* ------------------------------------------------------------------
 * CPU execution state
 * ------------------------------------------------------------------ */

typedef enum cpu_state_e
{
    CPU_STATE_STOPPED  = 0, /* STOP instruction executed             */
    CPU_STATE_RUNNING  = 1, /* Normal execution                      */
    CPU_STATE_HALTED   = 2, /* Double bus fault / unrecoverable      */
    CPU_STATE_TRACE    = 3  /* Single-step trace mode                */
} cpu_state_t;

/* ------------------------------------------------------------------
 * Register file
 * ------------------------------------------------------------------ */

typedef struct cpu68k_s
{
    st_u32_t   auDn[8];     /* D0-D7 data registers                  */
    st_u32_t   auAn[8];     /* A0-A7 address registers               */
    st_u32_t   uiSSP;       /* Supervisor stack pointer (A7')        */
    st_u32_t   uiPC;        /* Program counter                       */
    st_u16_t   uiSR;        /* Status register                       */
    cpu_state_t eState;     /* Execution state                       */

    /* Performance counters */
    st_u64_t   ulInstrCount;    /* Total instructions executed        */
    st_u64_t   ulCycleCount;    /* Total cycles emulated              */
} cpu68k_t;

/* ------------------------------------------------------------------
 * Step result  (returned by cpu_step)
 * ------------------------------------------------------------------ */

typedef struct cpu_step_result_s
{
    st_u16_t  uiOpcode;        /* Raw opcode word fetched             */
    st_u32_t  uiPCBefore;      /* PC before execution                 */
    st_u32_t  uiPCAfter;       /* PC after execution                  */
    int       iCycles;         /* Cycles consumed (0 if unknown)      */
    st_bool_t bException;      /* ST_TRUE if an exception was raised  */
    st_u32_t  uiExceptionVec;  /* Exception vector address if bException */
} cpu_step_result_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * cpu_init() - Initialise the CPU register file.
 *
 * Reads the reset vector from ptMachine (SSP at 0x000000, PC at
 * 0x000004) and sets SR to supervisor mode, interrupt mask = 7.
 *
 * Parameters:
 *   ptCpu     [out] : CPU state to initialise.
 *   ptMachine [in]  : Initialised ST machine (provides reset vector).
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if either pointer is NULL.
 */
st_error_t cpu_init(cpu68k_t *ptCpu, const st_machine_t *ptMachine);

/*
 * cpu_reset() - Perform a CPU reset (re-read reset vectors).
 *
 * Parameters:
 *   ptCpu     [in/out] : CPU state.
 *   ptMachine [in]     : ST machine (provides new reset vector).
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR if either pointer is NULL.
 */
st_error_t cpu_reset(cpu68k_t *ptCpu, const st_machine_t *ptMachine);

/*
 * cpu_step() - Fetch, decode and execute one instruction.
 *
 * Parameters:
 *   ptCpu     [in/out] : CPU state (registers updated).
 *   ptMachine [in/out] : ST machine (memory read/written).
 *   ptResult  [out]    : Execution result (may be NULL).
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on fatal emulation error.
 */
st_error_t cpu_step(cpu68k_t           *ptCpu,
                     st_machine_t       *ptMachine,
                     cpu_step_result_t  *ptResult);

/*
 * cpu_raise_exception() - Push the exception frame and jump to vector.
 *
 * Parameters:
 *   ptCpu     [in/out] : CPU state.
 *   ptMachine [in/out] : ST machine.
 *   uiVector  [in]     : Exception vector address.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on stack overflow.
 */
st_error_t cpu_raise_exception(cpu68k_t     *ptCpu,
                                 st_machine_t *ptMachine,
                                 st_u32_t      uiVector);

#endif /* CPU_H */
