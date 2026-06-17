/*
 * use_case_30G.c - UC30G: 68000 annotation engine
 *
 * Tests the full annotation pipeline on GEN.TTP:
 *   Pass 0: realignment detection (EVEN pads at BAF8, BD0A)
 *   Pass 1: fixup labels (fix_/dat_/bss_)
 *   Pass 2: call labels (sub_)
 *   Pass 3: branch labels (loc_)
 *   Pass 4: goto labels (bra_)
 *   Pass 5: pattern matching (prologue/epilogue/return)
 *
 * Produces use_cases/UC30F/GEN_DISASM_annotated.s as output.
 *
 * TEST MATRIX - UC30G:
 *   [N] Nominal    : 14 tests
 *   [R] Robustness :  4 tests
 *   [S] Skipped    :  0
 *   Total          : 18 tests
 *
 * Traceability:
 *   INT-ANN-001..018 → TC-ANN-001..018 → REQ-ANN-001..009
 */

#include "use_cases.h"
#include "../src/annotate.h"
#include "../src/disassemble.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_uc_fails = 0;

/* --- Fixture paths --- */
#define GEN_TTP_PATH  "tools/ST/GEN.TTP"
#define GEN_OUT_PATH  "use_cases/UC30F/GEN_DISASM_annotated.s"

/* GEN.TTP known constants (from UC30F tests) */
#define GEN_TEXT_SIZE  69370u
#define GEN_DISASM_MIN 20000   /* minimum instruction count            */

/* Known misaligned JSR targets */
#define GEN_MISS_BAF8  0x0000BAF8u
#define GEN_MISS_BD0A  0x0000BD0Au

/* Known JSR source address and its fix region pad */
#define GEN_JSR_E0     0x000000E0u  /* JSR $BAF8 — fixup #7          */
#define GEN_PAD_BAF6   0x0000BAF6u  /* predecessor instruction (pad) */

/* ------------------------------------------------------------------ */
int main(void)
{
    FILE             *f;
    st_u8_t          *pBinary;
    size_t            uiFileSize;
    size_t            uiRead;
    disasm_result_t  *ptResults;
    size_t            uiLines;
    annot_db_t        tDb;
    st_error_t        eRet;
    annot_entry_t    *ptE;
    annot_fix_region_t *ptRgn;
    int               iFixRegions;
    int               bFoundBAF8;
    int               bFoundBD0A;

    printf("=== UC30G: 68000 annotation engine — GEN.TTP ===\n");

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-001 → TC-ANN-001 → REQ-ANN-001]:
     * annot_db_init NULL ptDb must return ST_ERROR.
     * ---------------------------------------------------------------- */
    UC_TEST("[R] annot_db_init NULL ptDb",
            annot_db_init(NULL, 100) == ST_ERROR);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-002 → TC-ANN-002 → REQ-ANN-001]:
     * annot_run NULL ptDb must return ST_ERROR.
     * ---------------------------------------------------------------- */
    UC_TEST("[R] annot_run NULL ptDb",
            annot_run(NULL, NULL, 0, NULL, 0) == ST_ERROR);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-003 → TC-ANN-003 → REQ-ANN-001]:
     * annot_emit NULL parameters must return ST_ERROR.
     * ---------------------------------------------------------------- */
    UC_TEST("[R] annot_emit NULL ptDb",
            annot_emit(NULL, NULL, 0, "x.s") == ST_ERROR);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-004 → TC-ANN-004 → REQ-ANN-002]:
     * GEN.TTP binary opens successfully.
     * ---------------------------------------------------------------- */
    f = fopen(GEN_TTP_PATH, "rb");
    UC_TEST("[N] GEN.TTP opens", f != NULL);
    if (!f)
    {
        printf("  FATAL: cannot open %s — abort\n", GEN_TTP_PATH);
        return 1;
    }

    /* Determine file size */
    fseek(f, 0, SEEK_END);
    uiFileSize = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    pBinary = (st_u8_t *)malloc(uiFileSize);
    UC_TEST("[N] binary buffer allocated", pBinary != NULL);
    if (!pBinary) { fclose(f); return 1; }

    uiRead = fread(pBinary, 1, uiFileSize, f);
    fclose(f);
    UC_TEST("[N] binary fully read", uiRead == uiFileSize);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-005 → TC-ANN-005 → REQ-ANN-002]:
     * Wrong PRG magic must be rejected by annot_run.
     * ---------------------------------------------------------------- */
    {
        st_u8_t aBadBin[4] = { 0xDE, 0xAD, 0x00, 0x00 };
        annot_db_t tBadDb;
        disasm_result_t tBadR;
        memset(&tBadR, 0, sizeof(tBadR));
        UC_TEST("[R] bad magic rejected",
                annot_db_init(&tBadDb, 1) == ST_NO_ERROR);
        tBadDb.ptEntries[0].uiAddr = 0;
        tBadDb.puAddrSet[0]        = 0;
        tBadDb.iAddrCount          = 1;
        UC_TEST("[R] annot_run bad magic → ST_ERROR",
                annot_run(&tBadDb, &tBadR, 1, aBadBin, 4) == ST_ERROR);
        annot_db_shutdown(&tBadDb);
    }

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-006 → TC-ANN-006 → REQ-ANN-003]:
     * disasm_range on .text produces at least GEN_DISASM_MIN instructions.
     * ---------------------------------------------------------------- */
    ptResults = (disasm_result_t *)calloc(ANNOT_MAX_RESULTS,
                                           sizeof(disasm_result_t));
    UC_TEST("[N] results buffer allocated", ptResults != NULL);
    if (!ptResults) { free(pBinary); return 1; }

    uiLines = 0;
    eRet = disasm_range(pBinary + ANNOT_PRG_HDR_SIZE,
                         GEN_TEXT_SIZE,
                         0,             /* virtual address 0 = .text offset */
                         ptResults,
                         ANNOT_MAX_RESULTS,
                         &uiLines);
    UC_TEST("[N] disasm_range succeeds", eRet == ST_NO_ERROR);
    UC_TEST("[N] instruction count >= 20000", (int)uiLines >= GEN_DISASM_MIN);
    printf("    instruction count: %zu\n", uiLines);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-007 → TC-ANN-007 → REQ-ANN-004]:
     * annot_db_init allocates parallel entries for all instructions.
     * ---------------------------------------------------------------- */
    eRet = annot_db_init(&tDb, (int)uiLines);
    UC_TEST("[N] annot_db_init succeeds", eRet == ST_NO_ERROR);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-008 → TC-ANN-008 → REQ-ANN-005]:
     * annot_run completes without error on GEN.TTP.
     * ---------------------------------------------------------------- */
    eRet = annot_run(&tDb, ptResults, (int)uiLines, pBinary, uiFileSize);
    UC_TEST("[N] annot_run succeeds", eRet == ST_NO_ERROR);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-009 → TC-ANN-009 → REQ-ANN-006]:
     * Two fix regions created: one for BAF8, one for BD0A.
     * ---------------------------------------------------------------- */
    iFixRegions = 0;
    bFoundBAF8  = 0;
    bFoundBD0A  = 0;
    ptRgn = tDb.ptFixList;
    while (ptRgn)
    {
        iFixRegions++;
        if (ptRgn->uiAddrTarget == GEN_MISS_BAF8) bFoundBAF8 = 1;
        if (ptRgn->uiAddrTarget == GEN_MISS_BD0A) bFoundBD0A = 1;
        ptRgn = ptRgn->ptNext;
    }
    UC_TEST("[N] fix region for BAF8 created", bFoundBAF8);
    UC_TEST("[N] fix region for BD0A created", bFoundBD0A);
    printf("    total fix regions: %d\n", iFixRegions);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-010 → TC-ANN-010 → REQ-ANN-006]:
     * The PAD address (BAF6) is marked ANNOT_RGN_PAD in the DB.
     * ---------------------------------------------------------------- */
    ptE = annot_find_entry(&tDb, GEN_PAD_BAF6);
    UC_TEST("[N] PAD addr BAF6 region = ANNOT_RGN_PAD",
            ptE != NULL && ptE->eRegion == ANNOT_RGN_PAD);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-011 → TC-ANN-011 → REQ-ANN-007]:
     * Address BAF8 has label "sub_BAF8" (set by Pass 2 via virtual entry).
     * ---------------------------------------------------------------- */
    ptE = annot_find_entry(&tDb, GEN_MISS_BAF8);
    UC_TEST("[N] BAF8 has sub_ label",
            ptE != NULL && strncmp(ptE->szLabel, "sub_", 4) == 0);
    if (ptE) printf("    BAF8 label: '%s'\n", ptE->szLabel);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-012 → TC-ANN-012 → REQ-ANN-007]:
     * JSR at 0xE0 has [fixup] comment (fixup #7 at offset 0xE2).
     * ---------------------------------------------------------------- */
    ptE = annot_find_entry(&tDb, GEN_JSR_E0);
    UC_TEST("[N] JSR at E0 has [fixup] comment",
            ptE != NULL && strstr(ptE->szComment, "[fixup") != NULL);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-013 → TC-ANN-013 → REQ-ANN-008]:
     * MOVEQ #-1,D0 + RTS at 0x7F0 gets a pattern comment.
     * ---------------------------------------------------------------- */
    ptE = annot_find_entry(&tDb, 0x0007F0u);
    UC_TEST("[N] return pattern at 0x7F0",
            ptE != NULL && ptE->szComment[0] != '\0');
    if (ptE && ptE->szComment[0])
        printf("    0x7F0 comment: '%s'\n", ptE->szComment);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-014 → TC-ANN-014 → REQ-ANN-009]:
     * annot_emit produces a non-empty output file.
     * ---------------------------------------------------------------- */
    eRet = annot_emit(&tDb, ptResults, (int)uiLines, GEN_OUT_PATH);
    UC_TEST("[N] annot_emit succeeds", eRet == ST_NO_ERROR);

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-015 → TC-ANN-015 → REQ-ANN-009]:
     * Output file contains the "sub_BAF8" label.
     * ---------------------------------------------------------------- */
    {
        FILE *fOut = fopen(GEN_OUT_PATH, "r");
        char  szLine[256];
        int   bFound = 0;
        if (fOut)
        {
            while (fgets(szLine, sizeof(szLine), fOut))
            {
                if (strstr(szLine, "sub_BAF8")) { bFound = 1; break; }
            }
            fclose(fOut);
        }
        UC_TEST("[N] output contains sub_BAF8", bFound);
    }

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-016 → TC-ANN-016 → REQ-ANN-009]:
     * Output file contains "EVEN alignment pad" annotation.
     * ---------------------------------------------------------------- */
    {
        FILE *fOut = fopen(GEN_OUT_PATH, "r");
        char  szLine[256];
        int   bFound = 0;
        if (fOut)
        {
            while (fgets(szLine, sizeof(szLine), fOut))
            {
                if (strstr(szLine, "EVEN alignment pad"))
                { bFound = 1; break; }
            }
            fclose(fOut);
        }
        UC_TEST("[N] output contains EVEN pad annotation", bFound);
    }

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-017 → TC-ANN-017 → REQ-ANN-009]:
     * Output file is larger than 500 KB (full disassembly + annotations).
     * ---------------------------------------------------------------- */
    {
        FILE *fOut = fopen(GEN_OUT_PATH, "rb");
        long lSize = 0;
        if (fOut) { fseek(fOut, 0, SEEK_END); lSize = ftell(fOut); fclose(fOut); }
        UC_TEST("[N] output file > 500 KB", lSize > 500000L);
        printf("    output file size: %ld bytes\n", lSize);
    }

    /* ----------------------------------------------------------------
     * INTENT[INT-ANN-018 → TC-ANN-018 → REQ-ANN-007]:
     * annot_set_label on unknown address returns ST_ERROR.
     * ---------------------------------------------------------------- */
    UC_TEST("[R] set_label unknown addr → ST_ERROR",
            annot_set_label(&tDb, 0xFFFF00u, "bad") == ST_ERROR);

    /* Cleanup */
    annot_db_shutdown(&tDb);
    free(ptResults);
    free(pBinary);

    printf("\n--- UC30G: %d failure(s) ---\n", g_uc_fails);
    return g_uc_fails ? 1 : 0;
}
