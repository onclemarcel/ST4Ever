/*
 * annotate.c — 68000 disassembly annotation engine
 *
 * Pipeline architecture: passes write only to annot_db_t; ptResults[]
 * is never modified. annot_emit() renders both at output time.
 *
 * Pass order matters:
 *   0 realignment — creates fix regions and virtual DB entries first,
 *                   so subsequent label passes can reach those addresses.
 *   1 fixups      — fix_/dat_/bss_ labels from PRG reloc table.
 *   2 calls       — sub_ labels at JSR/BSR targets.
 *   3 branches    — loc_ labels at Bcc/DBcc targets.
 *   4 gotos       — bra_ labels at BRA targets.
 *   5 patterns    — prologue/epilogue/return comments.
 */

#include "annotate.h"
#include "trace.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ====================================================================
 * 1. PATTERN TABLE — add new patterns here, no other change needed
 * ==================================================================== */

static const annot_pattern_t g_annot_aPatterns[] =
{
    /* --- Return values --- */
    {
        "return_ok_moveq",
        { "MOVEQ", "RTS", NULL },
        { "#0",    NULL,  NULL },
        "; return 0  [MOVEQ #0,D0 + RTS]", 0.95f
    },
    {
        "return_error_moveq",
        { "MOVEQ", "RTS", NULL },
        { "#-1",   NULL,  NULL },
        "; return -1  [MOVEQ #-1,D0 + RTS]", 0.95f
    },
    {
        "return_ok_clr",
        { "CLR", "RTS", NULL },
        { NULL,   NULL, NULL },
        "; return 0  [CLR D0 + RTS]", 0.90f
    },
    {
        "return_value_tst",
        { "TST", "RTS", NULL },
        { NULL,   NULL, NULL },
        "; return D0  [TST D0 + RTS]", 0.80f
    },
    /* --- Prologues --- */
    {
        "prologue_link",
        { "LINK", NULL },
        { NULL,   NULL },
        "; --- prologue: stack frame ---", 0.95f
    },
    {
        "prologue_movem_sp",
        { "MOVEM", NULL },
        { "-(SP)", NULL },
        "; --- prologue: save registers ---", 0.90f
    },
    /* --- Epilogues --- */
    {
        "epilogue_unlk_rts",
        { "UNLK", "RTS", NULL },
        { NULL,    NULL, NULL },
        "; --- epilogue: destroy frame ---", 0.95f
    },
    {
        "epilogue_movem_rts",
        { "MOVEM", "RTS", NULL },
        { "(SP)+",  NULL, NULL },
        "; --- epilogue: restore registers ---", 0.92f
    },
    {
        "epilogue_unlk_movem_rts",
        { "UNLK",  "MOVEM", "RTS", NULL },
        { NULL,    "(SP)+",  NULL,  NULL },
        "; --- epilogue: destroy frame + restore registers ---", 0.97f
    },
    /* Sentinel */
    { NULL, { NULL }, { NULL }, NULL, 0.0f }
};

/* ====================================================================
 * 2. PASS TABLE — add new passes here, no other change needed
 * ==================================================================== */

/* Forward declarations */
static st_error_t annot_pass_realign (annot_db_t *, disasm_result_t *,
                                       int, const st_u8_t *, size_t);
static st_error_t annot_pass_fixups  (annot_db_t *, disasm_result_t *,
                                       int, const st_u8_t *, size_t);
static st_error_t annot_pass_calls   (annot_db_t *, disasm_result_t *,
                                       int, const st_u8_t *, size_t);
static st_error_t annot_pass_branches(annot_db_t *, disasm_result_t *,
                                       int, const st_u8_t *, size_t);
static st_error_t annot_pass_bra     (annot_db_t *, disasm_result_t *,
                                       int, const st_u8_t *, size_t);
static st_error_t annot_pass_patterns(annot_db_t *, disasm_result_t *,
                                       int, const st_u8_t *, size_t);

static const annot_pass_t g_annot_aPasses[] =
{
    { "realignment",   annot_pass_realign  }, /* 0: fix regions first */
    { "fixup labels",  annot_pass_fixups   }, /* 1: fix_/dat_/bss_    */
    { "call labels",   annot_pass_calls    }, /* 2: sub_              */
    { "branch labels", annot_pass_branches }, /* 3: loc_              */
    { "goto labels",   annot_pass_bra      }, /* 4: bra_              */
    { "patterns",      annot_pass_patterns }, /* 5: prologue/epilogue */
    /* { "hw registers", annot_pass_hw_regs }, -- future */
    /* { "jump tables",  annot_pass_jmptbl  }, -- future */
};

#define ANNOT_PASS_COUNT \
    ((int)(sizeof(g_annot_aPasses) / sizeof(g_annot_aPasses[0])))

/* ====================================================================
 * 3. STATIC HELPERS
 * ==================================================================== */

/* --- Big-endian reads --- */
static st_u32_t annot_r32be(const st_u8_t *p)
{
    return  ((st_u32_t)p[0] << 24) | ((st_u32_t)p[1] << 16)
          | ((st_u32_t)p[2] <<  8) |  (st_u32_t)p[3];
}

/* --- PRG/TTP header --- */
typedef struct annot_prg_hdr_s
{
    st_u32_t uiTextSize;
    st_u32_t uiDataSize;
    st_u32_t uiBssSize;
    st_u32_t uiSymSize;
    int      bHasReloc;
    size_t   uiFixStart; /* byte offset in file */
} annot_prg_hdr_t;

static int annot_parse_prg_hdr(const st_u8_t *pBin, size_t uiSz,
                                 annot_prg_hdr_t *ptHdr)
{
    st_u16_t uiMagic;
    size_t   uiPos;

    if (!pBin || !ptHdr || uiSz < ANNOT_PRG_HDR_SIZE) return 0;

    uiMagic = (st_u16_t)(((st_u16_t)pBin[0] << 8) | pBin[1]);
    if (uiMagic != 0x601A) return 0;

    ptHdr->uiTextSize = annot_r32be(pBin + 0x02);
    ptHdr->uiDataSize = annot_r32be(pBin + 0x06);
    ptHdr->uiBssSize  = annot_r32be(pBin + 0x0A);
    ptHdr->uiSymSize  = annot_r32be(pBin + 0x0E);

    uiPos = (size_t)ANNOT_PRG_HDR_SIZE
          + ptHdr->uiTextSize + ptHdr->uiDataSize + ptHdr->uiSymSize;
    if (uiPos + 4 > uiSz) return 0;

    ptHdr->uiFixStart = uiPos;
    ptHdr->bHasReloc  = (annot_r32be(pBin + uiPos) != 0) ? 1 : 0;
    return 1;
}

/* --- Fixup table (source offset + target value pairs) --- */
typedef struct annot_fixup_s
{
    st_u32_t uiSrcOff; /* offset in .text where the longword lives */
    st_u32_t uiTarget; /* raw longword value (unmodified from file)  */
} annot_fixup_t;

#define ANNOT_MAX_FIXUPS 512

static int annot_decode_fixups(const st_u8_t *pBin, size_t uiSz,
                                const annot_prg_hdr_t *ptHdr,
                                annot_fixup_t *ptOut, int iMaxOut)
{
    st_u32_t uiCur;
    size_t   uiPos;
    st_u8_t  uiDelta;
    int      iCount;

    if (!ptHdr->bHasReloc) return 0;

    uiPos  = ptHdr->uiFixStart;
    if (uiPos + 4 > uiSz) return 0;

    uiCur  = annot_r32be(pBin + uiPos);
    uiPos += 4;
    iCount = 0;

    while (iCount < iMaxOut)
    {
        size_t uiFileOff = (size_t)ANNOT_PRG_HDR_SIZE + uiCur;
        if (uiFileOff + 4 <= uiSz)
        {
            ptOut[iCount].uiSrcOff = uiCur;
            ptOut[iCount].uiTarget = annot_r32be(pBin + uiFileOff);
            iCount++;
        }
        if (uiPos >= uiSz) break;
        uiDelta = pBin[uiPos++];
        if (uiDelta == 0x00) break;
        if (uiDelta == 0x01) { uiCur += 254; continue; }
        uiCur += uiDelta;
    }
    return iCount;
}

/* --- Address set (sorted) helpers --- */
static int annot_cmp_u32(const void *a, const void *b)
{
    st_u32_t ua = *(const st_u32_t *)a;
    st_u32_t ub = *(const st_u32_t *)b;
    return (ua > ub) - (ua < ub);
}

static int annot_in_addrset(const st_u32_t *puSet, int iCount,
                              st_u32_t uiAddr)
{
    int iLo = 0, iHi = iCount - 1;
    while (iLo <= iHi)
    {
        int iMid = (iLo + iHi) / 2;
        if (puSet[iMid] == uiAddr) return 1;
        if (puSet[iMid] <  uiAddr) iLo = iMid + 1;
        else                        iHi = iMid - 1;
    }
    return 0;
}

/* Returns index in puSet matching uiAddr, or -1 if not found. */
static int annot_find_addr_idx(const st_u32_t *puSet, int iCount,
                                st_u32_t uiAddr)
{
    int iLo = 0, iHi = iCount - 1;
    while (iLo <= iHi)
    {
        int iMid = (iLo + iHi) / 2;
        if (puSet[iMid] == uiAddr) return iMid;
        if (puSet[iMid] <  uiAddr) iLo = iMid + 1;
        else                        iHi = iMid - 1;
    }
    return -1;
}

/* Returns largest index where puSet[idx] < uiAddr, or -1 if none. */
static int annot_pred_addr_idx(const st_u32_t *puSet, int iCount,
                                st_u32_t uiAddr)
{
    int iLo = 0, iHi = iCount - 1, iBest = -1;
    while (iLo <= iHi)
    {
        int iMid = (iLo + iHi) / 2;
        if (puSet[iMid] < uiAddr) { iBest = iMid; iLo = iMid + 1; }
        else                        iHi = iMid - 1;
    }
    return iBest;
}

/* --- Mnemonic predicates --- */
static int annot_is_jsr_bsr(const char *sz)
{
    return (strcmp(sz, "JSR") == 0 || strncmp(sz, "BSR", 3) == 0);
}

static const char *g_apszBcc[] = {
    "BHI","BLS","BCC","BCS","BNE","BEQ","BVC","BVS",
    "BPL","BMI","BGE","BLT","BGT","BLE",NULL
};

static int annot_is_bcc(const char *sz)
{
    int i;
    for (i = 0; g_apszBcc[i]; i++)
        if (strncmp(sz, g_apszBcc[i], 3) == 0) return 1;
    /* Also include DBcc loops as branch targets */
    if (strncmp(sz, "DB", 2) == 0) return 1;
    return 0;
}

static int annot_is_bra(const char *sz)
{
    return strncmp(sz, "BRA", 3) == 0;
}

/* Extract hex target from operand — uses last '$' to handle "D0,$XXXX". */
static int annot_extract_hex_target(const char *szOp, st_u32_t *puiTarget)
{
    const char *p = strrchr(szOp, '$');
    unsigned    uVal;
    if (!p) return 0;
    if (sscanf(p + 1, "%x", &uVal) != 1) return 0;
    *puiTarget = (st_u32_t)uVal;
    return 1;
}

/* Set region type for a DB entry by address (silently ignores unknown addr). */
static void annot_set_region(annot_db_t *ptDb, st_u32_t uiAddr,
                               annot_region_t eRegion)
{
    annot_entry_t *ptE = annot_find_entry(ptDb, uiAddr);
    if (ptE) ptE->eRegion = eRegion;
}

/* ====================================================================
 * 4. PUBLIC DB FUNCTIONS
 * ==================================================================== */

st_error_t annot_db_init(annot_db_t *ptDb, int iCount)
{
    if (!ptDb || iCount <= 0)
    {
        LOG_ERROR("NULL ptDb or iCount=%d", iCount);
        return ST_ERROR;
    }
    memset(ptDb, 0, sizeof(*ptDb));

    ptDb->ptEntries = (annot_entry_t *)calloc(
                          (size_t)iCount, sizeof(annot_entry_t));
    if (!ptDb->ptEntries)
    {
        LOG_ERROR("calloc failed for ptEntries count=%d", iCount);
        return ST_ERROR;
    }

    ptDb->puAddrSet = (st_u32_t *)calloc((size_t)iCount, sizeof(st_u32_t));
    if (!ptDb->puAddrSet)
    {
        free(ptDb->ptEntries);
        ptDb->ptEntries = NULL;
        LOG_ERROR("calloc failed for puAddrSet count=%d", iCount);
        return ST_ERROR;
    }

    ptDb->iCount = iCount;
    return ST_NO_ERROR;
}

void annot_db_shutdown(annot_db_t *ptDb)
{
    annot_fix_region_t *ptRgn;
    annot_fix_region_t *ptNext;

    if (!ptDb) return;

    free(ptDb->ptEntries);
    free(ptDb->puAddrSet);
    ptDb->ptEntries = NULL;
    ptDb->puAddrSet = NULL;

    ptRgn = ptDb->ptFixList;
    while (ptRgn)
    {
        ptNext = ptRgn->ptNext;
        free(ptRgn->ptFixed);
        free(ptRgn);
        ptRgn = ptNext;
    }
    ptDb->ptFixList = NULL;
}

annot_entry_t *annot_find_entry(annot_db_t *ptDb, st_u32_t uiAddr)
{
    int iIdx;
    int i;

    if (!ptDb) return NULL;

    /* Primary: binary search in the sorted address set */
    iIdx = annot_find_addr_idx(ptDb->puAddrSet, ptDb->iAddrCount, uiAddr);
    if (iIdx >= 0) return &ptDb->ptEntries[iIdx];

    /* Secondary: linear scan in extra entries (fix region addresses) */
    for (i = 0; i < ptDb->iExtraCount; i++)
    {
        if (ptDb->atExtra[i].uiAddr == uiAddr)
            return &ptDb->atExtra[i];
    }
    return NULL;
}

st_error_t annot_set_label(annot_db_t *ptDb, st_u32_t uiAddr,
                             const char *szLabel)
{
    annot_entry_t *ptE;

    if (!ptDb || !szLabel)
    {
        LOG_ERROR("NULL parameter: ptDb=%p szLabel=%p",
                  (void *)ptDb, (void *)szLabel);
        return ST_ERROR;
    }
    ptE = annot_find_entry(ptDb, uiAddr);
    if (!ptE)
    {
        LOG_ERROR("address $%06X not found in DB", (unsigned)uiAddr);
        return ST_ERROR;
    }
    snprintf(ptE->szLabel, ANNOT_LABEL_LEN, "%s", szLabel);
    return ST_NO_ERROR;
}

st_error_t annot_append_comment(annot_db_t *ptDb, st_u32_t uiAddr,
                                  const char *szComment)
{
    annot_entry_t *ptE;
    size_t         uiLen;

    if (!ptDb || !szComment)
    {
        LOG_ERROR("NULL parameter: ptDb=%p szComment=%p",
                  (void *)ptDb, (void *)szComment);
        return ST_ERROR;
    }
    ptE = annot_find_entry(ptDb, uiAddr);
    if (!ptE)
    {
        LOG_ERROR("address $%06X not found in DB", (unsigned)uiAddr);
        return ST_ERROR;
    }
    uiLen = strlen(ptE->szComment);
    if (uiLen > 0 && uiLen < ANNOT_COMMENT_LEN - 2)
    {
        ptE->szComment[uiLen]     = ' ';
        ptE->szComment[uiLen + 1] = '\0';
        uiLen++;
    }
    snprintf(ptE->szComment + uiLen,
             ANNOT_COMMENT_LEN - uiLen, "%s", szComment);
    return ST_NO_ERROR;
}

/* ====================================================================
 * 5. PASS 0 — REALIGNMENT
 *
 * Scans ptResults for JSR/BSR whose target is NOT in the linear
 * address set. For each such target T:
 *   - The EVEN pad word (0x0000) is at the predecessor instruction.
 *   - Re-disassemble from T to find the resync point.
 *   - Store as annot_fix_region_t + create a virtual DB entry for T.
 * ==================================================================== */

#define ANNOT_REALIGN_MAX_FIXED  64

static st_error_t annot_pass_realign(
    annot_db_t *ptDb, disasm_result_t *ptResults, int iCount,
    const st_u8_t *pBinary, size_t uiBinarySize)
{
    const st_u8_t     *pText;
    int                i, k;
    st_u32_t           uiTarget;
    st_u32_t           uiCur;
    st_u32_t           uiAddrPad;
    int                iPredIdx;
    int                iFixCount;
    int                iResyncIdx;
    size_t             uiRemain;
    disasm_result_t    atFixed[ANNOT_REALIGN_MAX_FIXED];
    disasm_result_t    tR;
    annot_fix_region_t *ptRgn;
    annot_entry_t      *ptExtra;

    if (!ptDb || !ptResults || !pBinary) return ST_NO_ERROR;
    if (uiBinarySize < (size_t)ANNOT_PRG_HDR_SIZE + 2) return ST_NO_ERROR;

    pText = pBinary + ANNOT_PRG_HDR_SIZE;

    for (i = 0; i < iCount; i++)
    {
        if (!annot_is_jsr_bsr(ptResults[i].szMnemonic)) continue;
        if (!annot_extract_hex_target(ptResults[i].szOperands, &uiTarget))
            continue;
        if (annot_in_addrset(ptDb->puAddrSet, ptDb->iAddrCount, uiTarget))
            continue; /* already correctly aligned */
        if (uiTarget >= ptDb->uiTextSize) continue;

        /* === Misaligned JSR/BSR target at uiTarget === */

        /* Find the predecessor instruction — it "ate" the EVEN pad */
        iPredIdx = annot_pred_addr_idx(ptDb->puAddrSet,
                                        ptDb->iAddrCount, uiTarget);
        if (iPredIdx < 0) continue;
        uiAddrPad = ptDb->puAddrSet[iPredIdx];

        /* Re-disassemble from uiTarget, find resync */
        iFixCount  = 0;
        iResyncIdx = -1;
        uiCur      = uiTarget;

        while (iFixCount < ANNOT_REALIGN_MAX_FIXED
               && uiCur < ptDb->uiTextSize)
        {
            uiRemain = ptDb->uiTextSize - uiCur;
            memset(&tR, 0, sizeof(tR));
            if (disasm_one(pText + uiCur, uiRemain, uiCur, &tR) != ST_NO_ERROR)
                break;

            /* Re-sync check: skip the very first instruction (= uiTarget itself),
             * then check if the current re-disasm address matches a linear addr */
            if (uiCur != uiTarget)
            {
                int iIdx = annot_find_addr_idx(ptDb->puAddrSet,
                                                ptDb->iAddrCount, uiCur);
                if (iIdx >= 0
                    && ptResults[iIdx].auWords[0] == tR.auWords[0])
                {
                    /* Structural agreement: same opcode word at same addr */
                    iResyncIdx = iIdx;
                    break; /* don't include resync instr in ptFixed */
                }
            }

            atFixed[iFixCount++] = tR;
            uiCur += (st_u32_t)(tR.iWordCount * 2);
        }

        /* Build fix region */
        ptRgn = (annot_fix_region_t *)calloc(1, sizeof(annot_fix_region_t));
        if (!ptRgn)
        {
            LOG_ERROR("calloc failed for annot_fix_region_t");
            return ST_ERROR;
        }
        ptRgn->ptFixed = (disasm_result_t *)calloc(
                          (size_t)(iFixCount ? iFixCount : 1),
                          sizeof(disasm_result_t));
        if (!ptRgn->ptFixed)
        {
            free(ptRgn);
            LOG_ERROR("calloc failed for ptFixed iFixCount=%d", iFixCount);
            return ST_ERROR;
        }

        ptRgn->uiAddrPad    = uiAddrPad;
        ptRgn->uiAddrTarget = uiTarget;
        ptRgn->uiAddrResync = (iResyncIdx >= 0)
                               ? ptDb->puAddrSet[iResyncIdx]
                               : uiCur;
        ptRgn->iFixedCount  = iFixCount;
        for (k = 0; k < iFixCount; k++)
            ptRgn->ptFixed[k] = atFixed[k];

        /* Prepend to list */
        ptRgn->ptNext   = ptDb->ptFixList;
        ptDb->ptFixList = ptRgn;

        /* Mark PAD address in main DB */
        annot_set_region(ptDb, uiAddrPad, ANNOT_RGN_PAD);

        /* Create virtual DB entries for the re-disassembled addresses
         * so that label passes can reach them. */
        if (ptDb->iExtraCount < ANNOT_EXTRA_MAX)
        {
            ptExtra = &ptDb->atExtra[ptDb->iExtraCount++];
            memset(ptExtra, 0, sizeof(*ptExtra));
            ptExtra->uiAddr  = uiTarget;
            ptExtra->eRegion = ANNOT_RGN_CODE;
        }
        for (k = 1; k < iFixCount && ptDb->iExtraCount < ANNOT_EXTRA_MAX; k++)
        {
            ptExtra = &ptDb->atExtra[ptDb->iExtraCount++];
            memset(ptExtra, 0, sizeof(*ptExtra));
            ptExtra->uiAddr  = atFixed[k].uiAddr;
            ptExtra->eRegion = ANNOT_RGN_CODE;
        }

        LOG_INFO("realign: target=$%06X pad=$%06X resync=$%06X fixed=%d",
                 (unsigned)uiTarget, (unsigned)uiAddrPad,
                 (unsigned)ptRgn->uiAddrResync, iFixCount);
    }
    return ST_NO_ERROR;
}

/* ====================================================================
 * 6. PASS 1 — FIXUP LABELS
 *
 * Uses the PRG reloc table. Targets in .text → fix_XXXX label.
 * Targets in .data → dat_XXXX. Targets in .bss → bss_XXXX.
 * These are later upgraded to sub_ by Pass 2 for JSR/BSR targets.
 * Also appends a [fixup $src] comment to the instruction containing
 * the fixup longword.
 * ==================================================================== */

static st_error_t annot_pass_fixups(
    annot_db_t *ptDb, disasm_result_t *ptResults, int iCount,
    const st_u8_t *pBinary, size_t uiBinarySize)
{
    annot_prg_hdr_t         tHdr;
    annot_fixup_t           atFix[ANNOT_MAX_FIXUPS];
    int                     iFixCount;
    int                     n, j;
    st_u32_t                uiDataEnd;
    st_u32_t                uiBssEnd;
    char                    szLabel[ANNOT_LABEL_LEN];
    char                    szComment[ANNOT_COMMENT_LEN];
    const char             *szPrefix;
    (void)ptResults; (void)iCount;
    int                     iIdx;

    if (!pBinary || uiBinarySize < ANNOT_PRG_HDR_SIZE) return ST_NO_ERROR;
    if (!annot_parse_prg_hdr(pBinary, uiBinarySize, &tHdr)) return ST_NO_ERROR;

    iFixCount = annot_decode_fixups(pBinary, uiBinarySize, &tHdr,
                                     atFix, ANNOT_MAX_FIXUPS);
    uiDataEnd = tHdr.uiTextSize + tHdr.uiDataSize;
    uiBssEnd  = uiDataEnd + tHdr.uiBssSize;

    for (n = 0; n < iFixCount; n++)
    {
        /* Determine label prefix from target segment */
        if      (atFix[n].uiTarget < tHdr.uiTextSize) szPrefix = "fix_";
        else if (atFix[n].uiTarget < uiDataEnd)        szPrefix = "dat_";
        else if (atFix[n].uiTarget < uiBssEnd)         szPrefix = "bss_";
        else                                            szPrefix = "ext_";

        snprintf(szLabel, sizeof(szLabel), "%s%X",
                 szPrefix, (unsigned)atFix[n].uiTarget);

        /* Set label at target address (silently ignored if not in DB) */
        annot_set_label(ptDb, atFix[n].uiTarget, szLabel);

        /* Add [fixup] comment to the instruction containing the source */
        iIdx = annot_pred_addr_idx(ptDb->puAddrSet, ptDb->iAddrCount,
                                    atFix[n].uiSrcOff + 1);
        if (iIdx < 0)
        {
            /* src offset might BE the instruction start */
            iIdx = annot_find_addr_idx(ptDb->puAddrSet,
                                        ptDb->iAddrCount,
                                        atFix[n].uiSrcOff);
        }
        if (iIdx >= 0)
        {
            /* Verify the src is within this instruction's word span */
            st_u32_t uiInstrEnd = ptResults[iIdx].uiAddr
                + (st_u32_t)(ptResults[iIdx].iWordCount * 2);
            if (atFix[n].uiSrcOff < uiInstrEnd)
            {
                snprintf(szComment, sizeof(szComment),
                         "[fixup $%06X]", (unsigned)atFix[n].uiSrcOff);
                annot_append_comment(ptDb, ptResults[iIdx].uiAddr, szComment);
            }
        }

        /* Mark data-segment targets as DATA region */
        if (atFix[n].uiTarget >= tHdr.uiTextSize)
        {
            j = 0; /* suppress unused var warning */
            (void)j;
        }
    }
    return ST_NO_ERROR;
}

/* ====================================================================
 * 7. PASS 2 — CALL LABELS (JSR/BSR → sub_)
 * ==================================================================== */

static st_error_t annot_pass_calls(
    annot_db_t *ptDb, disasm_result_t *ptResults, int iCount,
    const st_u8_t *pBinary, size_t uiBinarySize)
{
    int       i;
    st_u32_t  uiTarget;
    char      szLabel[ANNOT_LABEL_LEN];

    (void)pBinary; (void)uiBinarySize;

    for (i = 0; i < iCount; i++)
    {
        if (!annot_is_jsr_bsr(ptResults[i].szMnemonic)) continue;
        if (!annot_extract_hex_target(ptResults[i].szOperands, &uiTarget))
            continue;

        snprintf(szLabel, sizeof(szLabel), "sub_%X", (unsigned)uiTarget);
        /* Silently ignores if address not in DB (e.g. external address) */
        annot_set_label(ptDb, uiTarget, szLabel);
        annot_append_comment(ptDb, ptResults[i].uiAddr, "[call]");
    }
    return ST_NO_ERROR;
}

/* ====================================================================
 * 8. PASS 3 — BRANCH LABELS (Bcc/DBcc → loc_)
 * ==================================================================== */

static st_error_t annot_pass_branches(
    annot_db_t *ptDb, disasm_result_t *ptResults, int iCount,
    const st_u8_t *pBinary, size_t uiBinarySize)
{
    int       i;
    st_u32_t  uiTarget;
    char      szLabel[ANNOT_LABEL_LEN];

    (void)pBinary; (void)uiBinarySize;

    for (i = 0; i < iCount; i++)
    {
        if (!annot_is_bcc(ptResults[i].szMnemonic)) continue;
        if (!annot_extract_hex_target(ptResults[i].szOperands, &uiTarget))
            continue;

        snprintf(szLabel, sizeof(szLabel), "loc_%X", (unsigned)uiTarget);
        annot_set_label(ptDb, uiTarget, szLabel);
        annot_append_comment(ptDb, ptResults[i].uiAddr, "[branch]");
    }
    return ST_NO_ERROR;
}

/* ====================================================================
 * 9. PASS 4 — GOTO LABELS (BRA → bra_)
 * ==================================================================== */

static st_error_t annot_pass_bra(
    annot_db_t *ptDb, disasm_result_t *ptResults, int iCount,
    const st_u8_t *pBinary, size_t uiBinarySize)
{
    int       i;
    st_u32_t  uiTarget;
    char      szLabel[ANNOT_LABEL_LEN];

    (void)pBinary; (void)uiBinarySize;

    for (i = 0; i < iCount; i++)
    {
        if (!annot_is_bra(ptResults[i].szMnemonic)) continue;
        if (!annot_extract_hex_target(ptResults[i].szOperands, &uiTarget))
            continue;

        /* Only label targets that are in the DB (internal addresses) */
        if (!annot_in_addrset(ptDb->puAddrSet, ptDb->iAddrCount, uiTarget))
            continue;

        snprintf(szLabel, sizeof(szLabel), "bra_%X", (unsigned)uiTarget);
        annot_set_label(ptDb, uiTarget, szLabel);
        annot_append_comment(ptDb, ptResults[i].uiAddr, "[goto]");
    }
    return ST_NO_ERROR;
}

/* ====================================================================
 * 10. PASS 5 — PATTERN MATCHING (prologue/epilogue/return)
 *
 * Sliding window over ptResults. For each position, tries all patterns.
 * Match = consecutive mnemonics match by strcmp, with optional operand
 * hint checked by strstr.
 * ==================================================================== */

static st_error_t annot_pass_patterns(
    annot_db_t *ptDb, disasm_result_t *ptResults, int iCount,
    const st_u8_t *pBinary, size_t uiBinarySize)
{
    int                      i, p, m;
    int                      iPatCount;
    const annot_pattern_t   *ptPat;
    int                      iMatch;
    int                      iWindowLen;

    (void)pBinary; (void)uiBinarySize;

    iPatCount = 0;
    while (g_annot_aPatterns[iPatCount].szName) iPatCount++;

    for (i = 0; i < iCount; i++)
    {
        for (p = 0; p < iPatCount; p++)
        {
            ptPat  = &g_annot_aPatterns[p];
            iMatch = 1;

            /* Count pattern length */
            iWindowLen = 0;
            while (ptPat->apszMnem[iWindowLen]) iWindowLen++;

            if (i + iWindowLen > iCount) { iMatch = 0; continue; }

            /* Match each mnemonic in the window */
            for (m = 0; m < iWindowLen && iMatch; m++)
            {
                /* Mnemonic: exact match by strcmp */
                if (strcmp(ptResults[i + m].szMnemonic,
                            ptPat->apszMnem[m]) != 0)
                {
                    iMatch = 0;
                    break;
                }
                /* Optional operand hint: strstr */
                if (ptPat->apszOpHint[m]
                    && strstr(ptResults[i + m].szOperands,
                               ptPat->apszOpHint[m]) == NULL)
                {
                    iMatch = 0;
                    break;
                }
            }

            if (iMatch)
            {
                annot_append_comment(ptDb, ptResults[i].uiAddr,
                                      ptPat->szComment);
                LOG_INFO("pattern '%s' at $%06X conf=%.2f",
                         ptPat->szName,
                         (unsigned)ptResults[i].uiAddr,
                         ptPat->fConfidence);
                break; /* only first matching pattern per position */
            }
        }
    }
    return ST_NO_ERROR;
}

/* ====================================================================
 * 11. annot_run — orchestrate the pipeline
 * ==================================================================== */

st_error_t annot_run(annot_db_t *ptDb, disasm_result_t *ptResults,
                      int iCount, const st_u8_t *pBinary,
                      size_t uiBinarySize)
{
    annot_prg_hdr_t     tHdr;
    int                 i, p;
    st_error_t          eRet;

    LOG_TRACE("ptDb=%p ptResults=%p iCount=%d",
              (void *)ptDb, (void *)ptResults, iCount);

    if (!ptDb || !ptResults || !pBinary || iCount <= 0)
    {
        LOG_ERROR("NULL or invalid parameter");
        return ST_ERROR;
    }
    if (ptDb->iCount != iCount)
    {
        LOG_ERROR("DB count mismatch: db=%d results=%d",
                  ptDb->iCount, iCount);
        return ST_ERROR;
    }

    /* Parse header for text size (needed by realign pass) */
    if (!annot_parse_prg_hdr(pBinary, uiBinarySize, &tHdr))
    {
        LOG_ERROR("invalid PRG/TTP header");
        return ST_ERROR;
    }
    ptDb->uiTextSize = tHdr.uiTextSize;

    /* Build sorted address set + populate ptEntries[i].uiAddr */
    for (i = 0; i < iCount; i++)
    {
        ptDb->ptEntries[i].uiAddr = ptResults[i].uiAddr;
        ptDb->puAddrSet[i]        = ptResults[i].uiAddr;
    }
    qsort(ptDb->puAddrSet, (size_t)iCount, sizeof(st_u32_t), annot_cmp_u32);
    ptDb->iAddrCount = iCount;

    /* Run all passes in order */
    for (p = 0; p < ANNOT_PASS_COUNT; p++)
    {
        LOG_INFO("pass %d: %s", p, g_annot_aPasses[p].szName);
        eRet = g_annot_aPasses[p].pfnRun(ptDb, ptResults, iCount,
                                           pBinary, uiBinarySize);
        if (eRet != ST_NO_ERROR)
        {
            LOG_ERROR("pass %d '%s' failed",
                      p, g_annot_aPasses[p].szName);
            return ST_ERROR;
        }
    }

    LOG_INFO("annotation complete: %d instructions, %d extra entries",
             iCount, ptDb->iExtraCount);
    return ST_NO_ERROR;
}

/* ====================================================================
 * 12. annot_emit — write annotated disassembly to file
 *
 * For each linear instruction:
 *   - If it is the PAD address of a fix region:
 *       emit DC.W $0000 ; EVEN pad
 *       emit all ptFixed[] instructions with their labels/comments
 *   - If it is within a fix region (suppressed range):
 *       skip silently
 *   - Otherwise:
 *       emit label (if any), then the szLine, then comment (if any)
 * ==================================================================== */

static const annot_fix_region_t *annot_find_fix_for_addr(
    const annot_db_t *ptDb, st_u32_t uiAddr)
{
    const annot_fix_region_t *pt = ptDb->ptFixList;
    while (pt)
    {
        if (uiAddr >= pt->uiAddrPad && uiAddr < pt->uiAddrResync)
            return pt;
        pt = pt->ptNext;
    }
    return NULL;
}

static void annot_emit_label(FILE *f, const annot_entry_t *ptE)
{
    if (ptE && ptE->szLabel[0])
        fprintf(f, "%s\n", ptE->szLabel);
}

static void annot_emit_line(FILE *f, const disasm_result_t *ptR,
                             const annot_entry_t *ptE)
{
    int iLen;

    /* 4-space indent + szLine */
    iLen = fprintf(f, "    %s", ptR->szLine);
    if (iLen < 0) iLen = 0;

    if (ptE && ptE->szComment[0])
    {
        /* Pad to ANNOT_COMMENT_COL */
        while (iLen < ANNOT_COMMENT_COL) { fputc(' ', f); iLen++; }
        fprintf(f, " ; %s", ptE->szComment);
    }
    fputc('\n', f);
}

st_error_t annot_emit(const annot_db_t *ptDb,
                       const disasm_result_t *ptResults,
                       int iCount, const char *szOutPath)
{
    FILE                       *f;
    int                         i, k;
    st_u32_t                    uiAddr;
    const annot_fix_region_t   *ptRgn;
    const annot_entry_t        *ptE;
    disasm_result_t             tPadR;

    LOG_TRACE("ptDb=%p iCount=%d szOutPath=%s",
              (void *)ptDb, iCount, szOutPath ? szOutPath : "NULL");

    if (!ptDb || !ptResults || !szOutPath || iCount <= 0)
    {
        LOG_ERROR("NULL or invalid parameter");
        return ST_ERROR;
    }

    f = fopen(szOutPath, "w");
    if (!f)
    {
        LOG_ERROR("fopen failed: %s", szOutPath);
        return ST_ERROR;
    }

    for (i = 0; i < iCount; i++)
    {
        uiAddr = ptResults[i].uiAddr;
        ptRgn  = annot_find_fix_for_addr(ptDb, uiAddr);

        if (ptRgn && uiAddr == ptRgn->uiAddrPad)
        {
            /* --- PAD word: emit DC.W + corrected instructions --- */
            ptE = annot_find_entry((annot_db_t *)ptDb, uiAddr);
            annot_emit_label(f, ptE);

            /* Synthetic DC.W $0000 for the EVEN pad */
            memset(&tPadR, 0, sizeof(tPadR));
            tPadR.uiAddr     = uiAddr;
            tPadR.iWordCount = 1;
            tPadR.bValid     = ST_TRUE;
            snprintf(tPadR.szMnemonic, sizeof(tPadR.szMnemonic), "DC.W");
            snprintf(tPadR.szOperands, sizeof(tPadR.szOperands), "$0000");
            snprintf(tPadR.szLine, DISASM_LINE_MAX,
                     "$%06X:  0000                DC.W     $0000",
                     (unsigned)uiAddr);

            /* Reuse ptE comment if any, then add EVEN pad note */
            {
                annot_entry_t tPadE;
                memset(&tPadE, 0, sizeof(tPadE));
                tPadE.uiAddr = uiAddr;
                snprintf(tPadE.szComment, ANNOT_COMMENT_LEN,
                         "EVEN alignment pad");
                annot_emit_line(f, &tPadR, &tPadE);
            }

            /* Emit re-disassembled instructions */
            for (k = 0; k < ptRgn->iFixedCount; k++)
            {
                ptE = annot_find_entry((annot_db_t *)ptDb,
                                        ptRgn->ptFixed[k].uiAddr);
                annot_emit_label(f, ptE);
                annot_emit_line(f, &ptRgn->ptFixed[k], ptE);
            }
            continue;
        }

        if (ptRgn && uiAddr > ptRgn->uiAddrPad && uiAddr < ptRgn->uiAddrResync)
        {
            /* Suppressed: wrong linear instruction in the misaligned range */
            continue;
        }

        /* --- Normal instruction --- */
        ptE = annot_find_entry((annot_db_t *)ptDb, uiAddr);
        annot_emit_label(f, ptE);
        annot_emit_line(f, &ptResults[i], ptE);
    }

    fclose(f);
    LOG_INFO("emitted %d instructions to %s", iCount, szOutPath);
    return ST_NO_ERROR;
}
