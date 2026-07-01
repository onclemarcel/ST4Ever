/*
 * tos.h - ST4Ever GEMDOS / XBIOS minimal TOS dispatcher (UC29)
 *
 * The Atari ST Operating System exposes two key TRAP interfaces:
 *   TRAP #1  (GEMDOS) - File system and process management.
 *   TRAP #14 (XBIOS)  - Hardware access: palette, screen, VBL.
 *
 * ST4Ever intercepts these traps in cpu_step before the 68000 exception
 * frame is pushed, allowing the emulator to handle them without a TOS ROM.
 *
 * Stack layout at TRAP intercept (auAn[7] = user SP):
 *   SP+0  function number (word)
 *   SP+2  first parameter
 *   SP+4  second parameter (if any)
 *   ...
 *
 * GEMDOS functions implemented:
 *   0   Pterm0  - Terminate process (exit code 0)
 *   76  Pterm   - Terminate process (exit code from stack)
 *
 * XBIOS functions implemented:
 *   3   Setscreen  - Set physical/logical screen base and resolution
 *   5   Setpalette - Set all 16 palette colours from a RAM array
 *   7   Setcolor   - Set one palette colour by index
 *   37  Vsync      - Wait for vertical blank (no-op in emulator)
 *
 * All unimplemented function numbers emit LOG_TODO and return ST_NO_ERROR.
 */

#ifndef TOS_H
#define TOS_H

#include "common.h"
#include "ST.h"
#include "CPU.h"

/* ------------------------------------------------------------------
 * GEMDOS (TRAP #1) function numbers
 * ------------------------------------------------------------------ */

#define TOS_GEMDOS_PTERM0    0   /* Terminate with exit code 0    */
#define TOS_GEMDOS_CCONWS    9   /* Write string to console       */
#define TOS_GEMDOS_PTERM     76  /* Terminate with given exit code*/

/* ------------------------------------------------------------------
 * XBIOS (TRAP #14) function numbers
 * ------------------------------------------------------------------ */

#define TOS_XBIOS_SETSCREEN  3   /* Set screen base + resolution  */
#define TOS_XBIOS_SETPALETTE 5   /* Set all 16 palette entries    */
#define TOS_XBIOS_SETCOLOR   7   /* Set one palette entry         */
#define TOS_XBIOS_VSYNC      37  /* Wait for VBL (no-op stub)     */

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * tos_gemdos() - Handle a TRAP #1 (GEMDOS) call.
 *
 * Called by cpu_step() when a TRAP #1 instruction is encountered,
 * before the 68000 exception frame is pushed.  The function number
 * is read from the top of the user stack (auAn[7]).
 *
 * Parameters:
 *   ptCpu     [in/out] : CPU state (eState updated on termination).
 *   ptMachine [in/out] : ST machine (stack is read-only here).
 *
 * Returns:
 *   ST_NO_ERROR  Trap handled (including stubs and unknown functions).
 *   ST_ERROR     ptCpu or ptMachine is NULL, or stack read fails.
 */
st_error_t tos_gemdos(cpu68k_t     *ptCpu);

/*
 * tos_xbios() - Handle a TRAP #14 (XBIOS) call.
 *
 * Called by cpu_step() when a TRAP #14 instruction is encountered.
 * The function number is read from auAn[7]; parameters follow at
 * SP+2, SP+6, SP+10 depending on the function.
 *
 * Parameters:
 *   ptCpu     [in/out] : CPU state (D0 set to return value).
 *   ptMachine [in/out] : ST machine (palette/resolution updated).
 *
 * Returns:
 *   ST_NO_ERROR  Trap handled.
 *   ST_ERROR     ptCpu or ptMachine is NULL, or stack read fails.
 */
st_error_t tos_xbios(cpu68k_t     *ptCpu);

#endif /* TOS_H */
