/*
 * disassemble.h - ST4Ever 68000 → DEVPAC3 disassembler
 *
 * Converts raw binary opcodes to Motorola 68000 assembly source
 * in DEVPAC3 Atari ST format.
 *
 * Dispatch strategy: primary table on (opcode >> 12), then per-group
 * secondary decoding of size field and effective address mode.
 *
 * Output format matches DEVPAC3:
 *   MOVE.L  D0,D1
 *   LEA     $FF8200,A0
 *   BRA.S   .loop
 *   DC.W    $4E75     ; unrecognised opcode
 *
 * UC11: MOVE/MOVEQ/LEA/PEA/CLR/EXG/SWAP
 * UC12: ADD/SUB/CMP/MULU/DIVS/AND/OR/EOR/NOT/NEG
 * UC13: ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR/BTST/BCHG/BCLR/BSET
 * UC14: BRA/BSR/Bcc/JMP/JSR/RTS/RTR/RTE/TRAP/NOP/LINK/UNLK
 */

#ifndef DISASSEMBLE_H
#define DISASSEMBLE_H

#include "common.h"

/* Maximum length of one disassembled line (mnemonic + operands) */
#define DISASM_LINE_MAX  160

/* ------------------------------------------------------------------
 * Result of disassembling one instruction
 * ------------------------------------------------------------------ */

typedef struct disasm_result_s
{
    st_u32_t uiAddr;                 /* Address of the opcode word    */
    st_u16_t auWords[5];             /* Raw opcode + extension words  */
    int      iWordCount;             /* Number of words consumed      */
    char     szMnemonic[16];         /* e.g. "MOVE.L"                 */
    char     szOperands[104];         /* e.g. "D0,-(A1)"              */
    char     szLine[DISASM_LINE_MAX];/* Full formatted line           */
    st_bool_t bValid;                /* ST_FALSE = DC.W fallback      */
} disasm_result_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * disasm_one() - Disassemble one instruction from a byte buffer.
 *
 * Parameters:
 *   pBuf     [in]  : Pointer to the first opcode byte.
 *   uiBufLen [in]  : Bytes remaining in buffer.
 *   uiAddr   [in]  : Load address of pBuf[0] (for branch targets).
 *   ptResult [out] : Receives the disassembly result.
 *
 * Returns: ST_NO_ERROR (always; invalid opcodes produce DC.W).
 */
st_error_t disasm_one(const st_u8_t  *pBuf,
                        size_t          uiBufLen,
                        st_u32_t        uiAddr,
                        disasm_result_t *ptResult);

/*
 * disasm_range() - Disassemble consecutive instructions.
 *
 * Parameters:
 *   pBuf      [in]  : Buffer start.
 *   uiBufLen  [in]  : Buffer size in bytes.
 *   uiAddr    [in]  : Load address of pBuf[0].
 *   ptResults [out] : Array of results.
 *   uiMaxLines[in]  : Capacity of ptResults.
 *   puiLines  [out] : Number of results written.
 *
 * Returns: ST_NO_ERROR on success, ST_ERROR on NULL parameter.
 */
st_error_t disasm_range(const st_u8_t  *pBuf,
                          size_t          uiBufLen,
                          st_u32_t        uiAddr,
                          disasm_result_t *ptResults,
                          size_t          uiMaxLines,
                          size_t         *puiLines);

#endif /* DISASSEMBLE_H */
