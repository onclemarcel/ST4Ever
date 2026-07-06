/*
 * linea.h - ST4Ever Line-A ($Axxx) opcode dispatcher
 *
 * On the real Atari ST, Line-A instructions ($Axxx opcodes) raise an
 * exception via vector $28 (CPU_VEC_LINE_A) and the TOS handler
 * dispatches the requested function.
 *
 * ST4Ever implements an emulator-level dispatcher: when cpu_step sees
 * a $Axxx opcode, it calls linea_dispatch() directly (no exception
 * frame push).  This gives identical visible results for programs that
 * use Line-A to draw pixels or initialise graphics, without requiring
 * a TOS ROM.
 *
 * Line-A parameter block:
 *   After LINEA_INIT ($A000), register A0 points to the parameter
 *   block.  Fields are at NEGATIVE offsets from A0 (TOS convention).
 *   The block is stored at a fixed RAM address (LINEA_PARAM_ADDR).
 *   Positive offsets (A0+) hold the system font pointers — stubbed.
 *
 * Key negative offsets from A0 (see Atari ST Internals, pp.167-179):
 *   -2   V_CEL_MX   columns - 1 (39 for 80-col text mode)
 *   -4   V_CEL_MY   rows - 1    (24 for 25-line mode)
 *   -6   V_CEL_WR   bytes per text row (screen_width_bytes * cel_height)
 *   -8   V_CEL_HT   character cell height in pixels (8 for 8x8 font)
 *   -10  V_PLANES   number of bitplanes (4 low, 2 med, 1 high)
 *   -12  V_LIN_WR   bytes per scan line (160 low, 160 med, 80 high)
 *   -20  V_BAS_AD   pointer to screen buffer in RAM (32-bit)
 *
 * UC28: LINEA_INIT + stub for all other Line-A functions.
 * UC29: PUT_PIXEL ($A001) implemented via registers D0/D1/D2 (color/y/x).
 */

#ifndef LINEA_H
#define LINEA_H

#include "common.h"
#include "ST.h"
#include "CPU.h"

/* ------------------------------------------------------------------
 * RAM addresses for Line-A infrastructure
 * ------------------------------------------------------------------ */

/* Line-A parameter block base pointer (positive offset area).
 * Actual usable fields start at LINEA_PARAM_ADDR - LINEA_PARAM_NEG_SIZE.
 * Chosen above the exception vector table, below program load area. */
#define LINEA_PARAM_ADDR    0x0900u   /* A0 points here after LINEA_INIT */
#define LINEA_PARAM_NEG_SIZE 32u      /* 32 bytes of negative-offset vars */

/* Total block size in RAM: negative area + positive area (font ptrs) */
#define LINEA_BLOCK_SIZE    (LINEA_PARAM_NEG_SIZE + 32u)

/* Negative-offset field positions (as byte offsets from LINEA_PARAM_ADDR).
 * E.g. V_CEL_MX is at RAM address LINEA_PARAM_ADDR - 2 = 0x08FE.       */
#define LINEA_V_CEL_MX_OFF  2u    /* columns - 1 (word)                  */
#define LINEA_V_CEL_MY_OFF  4u    /* rows - 1 (word)                     */
#define LINEA_V_CEL_WR_OFF  6u    /* bytes per text row (word)           */
#define LINEA_V_CEL_HT_OFF  8u    /* cell height in pixels (word)        */
#define LINEA_V_PLANES_OFF  10u   /* number of bitplanes (word)          */
#define LINEA_V_LIN_WR_OFF  12u   /* bytes per scan line (word)          */
#define LINEA_V_BAS_AD_OFF  20u   /* screen buffer address (long)        */

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * linea_init() - Initialise the Line-A parameter block in ST RAM.
 *
 * Writes default values for the current resolution into the parameter
 * block at LINEA_PARAM_ADDR.  Must be called after st_init() and
 * after the resolution / screen-base registers are set.
 *
 * Call this once at exec_open() to prepare the block before program
 * execution; programs that call LINEA_INIT ($A000) will refresh A0.
 *
 * Parameters:
 *   ptMachine [in/out] : Initialised ST machine.
 *
 * Returns:
 *   ST_NO_ERROR  always (non-fatal: falls back to zero block).
 *   ST_ERROR     if ptMachine is NULL.
 */
st_error_t linea_init();

/*
 * linea_dispatch() - Handle a Line-A opcode in the CPU dispatcher.
 *
 * Called by cpu_step() for any opcode with top nibble 0xA.
 * Extracts the function number from bits[7:0] of the opcode word and
 * executes the corresponding Line-A service:
 *
 *   $A000  LINEA_INIT  : refresh parameter block, set A0 = LINEA_PARAM_ADDR
 *   $A001  PUT_PIXEL   : write pixel to bitplane RAM
 *                        D0.w=color index, D1.w=y, D2.w=x (register conv.)
 *   $A002  GET_PIXEL   : stub (returns 0 in D0)
 *   others             : stub (LOG_TODO, no-op)
 *
 * Parameters:
 *   ptCpu     [in/out] : CPU state (A0 updated on LINEA_INIT).
 *   ptMachine [in/out] : ST machine (parameter block read/written).
 *   uiOpcode  [in]     : Raw Line-A opcode word ($Axxx).
 *
 * Returns:
 *   ST_NO_ERROR  always (stubs are non-fatal).
 *   ST_ERROR     if ptCpu or ptMachine is NULL.
 */
st_error_t linea_dispatch(cpu_context_t     *ptCpu,
                           st_u16_t      uiOpcode);

#endif /* LINEA_H */
