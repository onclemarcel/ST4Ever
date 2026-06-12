/*
 * as.h - ST4Ever DEVPAC3 assembler (text -> Atari ST .PRG)
 *
 * Assembles 68000 source in DEVPAC3 Atari ST syntax into a
 * relocatable .PRG (header + fixup list) or an absolute binary.
 *
 * UC30A - directives:
 *   SECTION TEXT / DATA / BSS
 *   DC.B / DC.W / DC.L     define byte / word / longword constants
 *   DS.B / DS.W / DS.L     reserve uninitialised space
 *   EQU / SET              symbolic constants
 *   EVEN                   align to word boundary
 *   ORG                    set assembly origin (absolute mode only)
 *   END                    terminate assembly
 *
 * UC30B-D - instructions:
 *   MOVE / ADD / SUB / BRA / ... (added incrementally)
 */

#ifndef AS_H
#define AS_H

#include "common.h"

/* Maximum number of symbols in the symbol table */
#define AS_MAX_SYMBOLS  4096

/* Maximum number of errors collected before aborting */
#define AS_MAX_ERRORS   64

/* ------------------------------------------------------------------
 * Error report
 * ------------------------------------------------------------------ */

typedef struct as_error_s
{
    int  iLine;                 /* Source line number (1-based)       */
    char szMsg[ST_MAX_MSG];     /* Human-readable error description   */
} as_error_t;

/* ------------------------------------------------------------------
 * Assembler context (public)
 * ------------------------------------------------------------------ */

typedef struct as_context_s
{
    char      szSourcePath[ST_MAX_PATH]; /* Input .S file             */
    char      szOutputPath[ST_MAX_PATH]; /* Output .PRG or raw file   */
    st_bool_t bRelocatable;             /* ST_TRUE = .PRG with fixups */

    /* Output (filled after successful as_assemble()) */
    st_u8_t  *pBinary;                  /* Heap-allocated binary data */
    size_t    uiBinaryLen;              /* Binary size in bytes       */

    /* Error list (filled on failure) */
    as_error_t *ptErrors;               /* Heap-allocated error array */
    size_t      uiErrorCount;           /* Number of errors           */
} as_context_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * as_init() - Initialise an assembler context.
 *
 * Parameters:
 *   ptCtx         [out] : Context to initialise.
 *   szSourcePath  [in]  : Path to the .S source file.
 *   szOutputPath  [in]  : Destination path for the binary.
 *   bRelocatable  [in]  : ST_TRUE to produce a relocatable .PRG.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer parameter is NULL.
 */
st_error_t as_init(as_context_t *ptCtx,
                    const char   *szSourcePath,
                    const char   *szOutputPath,
                    st_bool_t     bRelocatable);

/*
 * as_assemble() - Run the two-pass assembler.
 *
 * On success ptCtx->pBinary / uiBinaryLen are set and the binary is
 * also written to ptCtx->szOutputPath.
 * On failure ptCtx->ptErrors / uiErrorCount contain diagnostics.
 *
 * Parameters:
 *   ptCtx [in/out] : Initialised context.
 *
 * Returns:
 *   ST_NO_ERROR on success (0 errors).
 *   ST_ERROR    on any error (I/O, syntax, or out-of-memory).
 */
st_error_t as_assemble(as_context_t *ptCtx);

/*
 * as_shutdown() - Free all resources in the context.
 *
 * Parameters:
 *   ptCtx [in/out] : Context to release. Set to zeroed state.
 *
 * Returns:
 *   ST_NO_ERROR always.
 */
st_error_t as_shutdown(as_context_t *ptCtx);

#endif /* AS_H */
