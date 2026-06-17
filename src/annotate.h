/*
 * annotate.h — 68000 disassembly annotation engine
 *
 * Annotation pipeline over a disasm_result_t[] array.
 * Passes run in order; each writes only to annot_db_t (ptResults is
 * read-only throughout). annot_emit() combines both at output time.
 *
 * Extensibility:
 *   New pass    : implement pfn_annot_pass_t, add row to g_annot_aPasses[]
 *   New pattern : add annot_pattern_t row to g_annot_aPatterns[]
 */

#ifndef ANNOTATE_H
#define ANNOTATE_H

#include "common.h"
#include "disassemble.h"

/* Max instructions the engine can handle */
#define ANNOT_MAX_RESULTS        32768
/* Max mnemonics per pattern window */
#define ANNOT_PATTERN_MAX_LINES      8
/* String buffer sizes */
#define ANNOT_LABEL_LEN             80
#define ANNOT_COMMENT_LEN          128
/* PRG/TTP header size */
#define ANNOT_PRG_HDR_SIZE        0x1C
/* Extra entries for addresses not in linear disassembly (fix regions) */
#define ANNOT_EXTRA_MAX             32
/* Output column where inline comments start */
#define ANNOT_COMMENT_COL           70

/* ------------------------------------------------------------------
 * Region type — assigned by the realignment pass
 * ------------------------------------------------------------------ */
typedef enum annot_region_e
{
    ANNOT_RGN_UNKNOWN = 0,
    ANNOT_RGN_CODE,          /* confirmed valid 68000 instructions        */
    ANNOT_RGN_PAD,           /* EVEN alignment pad (0x0000 word)          */
    ANNOT_RGN_DATA,          /* inline data (string, DC.B/W/L)            */
} annot_region_t;

/* ------------------------------------------------------------------
 * Per-address annotation record (parallel to disasm_result_t[])
 * ------------------------------------------------------------------ */
typedef struct annot_entry_s
{
    st_u32_t        uiAddr;
    char            szLabel  [ANNOT_LABEL_LEN];   /* emitted before line  */
    char            szComment[ANNOT_COMMENT_LEN]; /* appended after line  */
    annot_region_t  eRegion;
    float           fConfidence;
} annot_entry_t;

/* ------------------------------------------------------------------
 * Corrected region: replaces a misaligned linear range in output
 *
 * When the linear disassembler ate an EVEN pad as an opcode and then
 * misread subsequent bytes, the fix region stores:
 *   [uiAddrPad]    — the wrong linear instruction that consumed the pad
 *   [uiAddrTarget] — the true code entry (= uiAddrPad + 2)
 *   [uiAddrResync] — first address where linear and re-disasm agree
 *   ptFixed[]      — correct instructions from uiAddrTarget to uiAddrResync
 * ------------------------------------------------------------------ */
typedef struct annot_fix_region_s
{
    st_u32_t                    uiAddrPad;    /* linear instr eating the pad */
    st_u32_t                    uiAddrTarget; /* true code entry             */
    st_u32_t                    uiAddrResync; /* resume linear output here   */
    disasm_result_t            *ptFixed;      /* [iFixedCount]               */
    int                         iFixedCount;
    struct annot_fix_region_s  *ptNext;
} annot_fix_region_t;

/* ------------------------------------------------------------------
 * Annotation database
 * ------------------------------------------------------------------ */
typedef struct annot_db_s
{
    annot_entry_t       *ptEntries;              /* [iCount] parallel       */
    annot_fix_region_t  *ptFixList;              /* linked list, head       */
    int                  iCount;
    st_u32_t            *puAddrSet;              /* sorted addr for bsearch */
    int                  iAddrCount;
    st_u32_t             uiTextSize;             /* from PRG header         */
    /* Extra entries for addresses in fix regions (not in linear disasm) */
    annot_entry_t        atExtra[ANNOT_EXTRA_MAX];
    int                  iExtraCount;
} annot_db_t;

/* ------------------------------------------------------------------
 * Multi-instruction pattern (table-driven, add rows to extend)
 * ------------------------------------------------------------------ */
typedef struct annot_pattern_s
{
    const char  *szName;
    /* NULL-terminated mnemonic list; matched with strcmp */
    const char  *apszMnem  [ANNOT_PATTERN_MAX_LINES];
    /* optional operand substring hint (strstr); NULL = don't check */
    const char  *apszOpHint[ANNOT_PATTERN_MAX_LINES];
    /* comment injected at first matched instruction */
    const char  *szComment;
    float        fConfidence;
} annot_pattern_t;

/* ------------------------------------------------------------------
 * Pass function signature
 * ------------------------------------------------------------------ */
typedef st_error_t (*pfn_annot_pass_t)(
    annot_db_t          *ptDb,
    disasm_result_t     *ptResults,
    int                  iCount,
    const st_u8_t       *pBinary,
    size_t               uiBinarySize
);

/* ------------------------------------------------------------------
 * Pass descriptor — add new rows to g_annot_aPasses[] in annotate.c
 * ------------------------------------------------------------------ */
typedef struct annot_pass_s
{
    const char       *szName;
    pfn_annot_pass_t  pfnRun;
} annot_pass_t;

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

/*
 * annot_db_init() - Allocate DB parallel to ptResults[iCount].
 *
 * Parameters:
 *   ptDb   [out] : DB to initialise (caller owns the struct).
 *   iCount [in]  : Number of disassembled instructions.
 *
 * Returns: ST_NO_ERROR or ST_ERROR (malloc failure).
 */
st_error_t annot_db_init(annot_db_t *ptDb, int iCount);

/*
 * annot_db_shutdown() - Free all DB resources.
 *
 * Parameters:
 *   ptDb [in] : DB to free.
 */
void annot_db_shutdown(annot_db_t *ptDb);

/*
 * annot_find_entry() - Find entry by address (linear disasm or fix region).
 *                      Returns NULL if not found.
 *
 * Parameters:
 *   ptDb   [in] : Annotation DB.
 *   uiAddr [in] : Address to look up.
 */
annot_entry_t *annot_find_entry(annot_db_t *ptDb, st_u32_t uiAddr);

/*
 * annot_set_label() - Set label for the given address.
 *                     If label already set, replaces it.
 *
 * Parameters:
 *   ptDb    [in] : Annotation DB.
 *   uiAddr  [in] : Target address.
 *   szLabel [in] : Label string (e.g. "sub_BAF8").
 *
 * Returns: ST_NO_ERROR or ST_ERROR if address not found.
 */
st_error_t annot_set_label(annot_db_t *ptDb,
                            st_u32_t    uiAddr,
                            const char *szLabel);

/*
 * annot_append_comment() - Append text to inline comment for an address.
 *
 * Parameters:
 *   ptDb      [in] : Annotation DB.
 *   uiAddr    [in] : Target address.
 *   szComment [in] : Text to append (space-separated if already set).
 *
 * Returns: ST_NO_ERROR or ST_ERROR if address not found.
 */
st_error_t annot_append_comment(annot_db_t *ptDb,
                                 st_u32_t    uiAddr,
                                 const char *szComment);

/*
 * annot_run() - Run the full annotation pipeline on ptResults[iCount].
 *
 * ptResults is never modified; all output goes to ptDb.
 *
 * Parameters:
 *   ptDb         [in/out] : Initialised DB (iCount must match ptResults).
 *   ptResults    [in]     : Linear disassembly (read-only).
 *   iCount       [in]     : Number of entries.
 *   pBinary      [in]     : Raw PRG/TTP file data.
 *   uiBinarySize [in]     : Size of pBinary in bytes.
 *
 * Returns: ST_NO_ERROR or ST_ERROR.
 */
st_error_t annot_run(annot_db_t      *ptDb,
                     disasm_result_t *ptResults,
                     int              iCount,
                     const st_u8_t   *pBinary,
                     size_t           uiBinarySize);

/*
 * annot_emit() - Write annotated disassembly to szOutPath.
 *
 * Fix regions replace their linear counterparts in the output.
 *
 * Parameters:
 *   ptDb      [in] : Populated annotation DB.
 *   ptResults [in] : Linear disassembly.
 *   iCount    [in] : Number of entries.
 *   szOutPath [in] : Output file path.
 *
 * Returns: ST_NO_ERROR or ST_ERROR (fopen/fwrite failure).
 */
st_error_t annot_emit(const annot_db_t      *ptDb,
                      const disasm_result_t *ptResults,
                      int                    iCount,
                      const char            *szOutPath);

#endif /* ANNOTATE_H */
