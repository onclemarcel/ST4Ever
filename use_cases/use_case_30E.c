/* use_case_30E.c - UC30E: DEVPAC3 assembler torture test
 *
 * Assembles use_cases/UC15A/SOURCE.S (4072-line 68000 full-coverage source)
 * and compares the .text output byte-for-byte against the reference binary
 * use_cases/UC15A/SOURCE.PRG (10218 bytes of .text produced by real DEVPAC3
 * on a real Atari ST).
 *
 * Goal: 0 byte difference (same principle as UC15A for the disassembler).
 *
 * TEST MATRIX - UC30E:
 *   [N] Nominal    : 6  tests - assemble succeeds, size matches, 0 byte diff
 *   [R] Robustness : 2  tests - NULL params, missing source file
 *   [S] Skipped    : 0  tests
 *
 * INT map:
 *   INT-AS-141 -> TC-AS-211: as_assemble(NULL) -> ST_ERROR
 *   INT-AS-142 -> TC-AS-212: bad source path -> ST_ERROR
 *   INT-AS-143 -> TC-AS-213: SOURCE.S assembles without errors
 *   INT-AS-144 -> TC-AS-214: assembled .text size == 10218 bytes
 *   INT-AS-145 -> TC-AS-215: 0 byte difference vs SOURCE.PRG
 *   INT-AS-146 -> TC-AS-216: SOURCE.PRG reference can be read (sanity)
 *   INT-AS-147 -> TC-AS-217: assembled .text[0..3] matches PRG reference
 *   INT-AS-148 -> TC-AS-218: all 10218 .text bytes identical
 */

#include "use_cases.h"
#include "../src/as.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int g_uc_fails = 0;

#define SOURCE_S   "use_cases/UC15A/SOURCE.S"
#define SOURCE_PRG "use_cases/UC15A/SOURCE.PRG"
#define OUT_PRG    "use_cases/UC30E/out_source.prg"

#define PRG_HEADER_SZ  28u
#define TEXT_SZ_OFFSET  2u   /* big-endian u32 at offset 2 in PRG header */
#define EXPECTED_TEXT  10218u

/* ------------------------------------------------------------------ */
/* Helpers                                                               */
/* ------------------------------------------------------------------ */

static st_u32_t read_be32(const st_u8_t *p)
{
    return ((st_u32_t)p[0] << 24) |
           ((st_u32_t)p[1] << 16) |
           ((st_u32_t)p[2] <<  8) |
            (st_u32_t)p[3];
}

/* Load a file into a heap buffer. Caller must free(). Returns NULL on err. */
static st_u8_t *uc30e_load_file(const char *szPath, size_t *puiLen)
{
    FILE    *pF;
    st_u8_t *pBuf;
    size_t   uiLen;

    pF = fopen(szPath, "rb");
    if (!pF) return NULL;
    fseek(pF, 0, SEEK_END);
    uiLen = (size_t)ftell(pF);
    fseek(pF, 0, SEEK_SET);
    pBuf = (st_u8_t *)malloc(uiLen + 1u);
    if (!pBuf) { fclose(pF); return NULL; }
    if (fread(pBuf, 1u, uiLen, pF) != uiLen)
    { free(pBuf); fclose(pF); return NULL; }
    fclose(pF);
    *puiLen = uiLen;
    return pBuf;
}

/* ------------------------------------------------------------------ */
/* Robustness tests                                                      */
/* ------------------------------------------------------------------ */

static void test_robustness(void)
{
    as_context_t tCtx;
    FILE        *pF;
    const char  *szBad = "use_cases/UC30E/no_such_file.S";
    const char  *szOut = "use_cases/UC30E/tmp_bad.prg";

    printf("\n[UC30E] Robustness\n");

    /* INTENT[INT-AS-141 -> TC-AS-211 -> REQ-AS-001]:
     * as_assemble(NULL) -> ST_ERROR, no crash */
    TEST_ASSERT("[R] as_assemble(NULL) returns ST_ERROR",
        as_assemble(NULL) == ST_ERROR);

    /* INTENT[INT-AS-142 -> TC-AS-212 -> REQ-AS-002]:
     * Non-existent source file -> ST_ERROR */
    (void)remove(szBad);
    pF = fopen(szBad, "r");
    TEST_ASSERT("[R] bad source path returns ST_ERROR",
        pF == NULL &&
        as_init(&tCtx, szBad, szOut, ST_TRUE) == ST_NO_ERROR &&
        as_assemble(&tCtx) == ST_ERROR);
    as_shutdown(&tCtx);
}

/* ------------------------------------------------------------------ */
/* Main torture test: SOURCE.S -> 0 byte diff vs SOURCE.PRG            */
/* ------------------------------------------------------------------ */

static void test_source_torture(void)
{
    as_context_t tCtx;
    st_u8_t     *pRef   = NULL;
    size_t       uiRefLen;
    size_t       uiTextSz;
    const st_u8_t *pAsm;
    const st_u8_t *pPrg;
    size_t        i;
    unsigned int  uiDiffs = 0;
    unsigned int  uiShown = 0;

    printf("\n[UC30E] SOURCE.S torture test\n");

    /* INTENT[INT-AS-146 -> TC-AS-216 -> REQ-AS-002]:
     * Reference PRG must be loadable                                   */
    pRef = uc30e_load_file(SOURCE_PRG, &uiRefLen);
    TEST_ASSERT("[N] SOURCE.PRG loaded", pRef != NULL);
    if (!pRef) return;

    /* INTENT[INT-AS-143 -> TC-AS-213 -> REQ-AS-002]:
     * SOURCE.S assembles successfully (0 errors)                       */
    memset(&tCtx, 0, sizeof(tCtx));
    TEST_ASSERT("[N] as_init SOURCE.S",
        as_init(&tCtx, SOURCE_S, OUT_PRG, ST_TRUE) == ST_NO_ERROR);

    if (as_assemble(&tCtx) != ST_NO_ERROR)
    {
        size_t k;
        printf("  ASSEMBLY ERRORS (%u):\n", (unsigned)tCtx.uiErrorCount);
        for (k = 0; k < tCtx.uiErrorCount && k < 20u; k++)
            printf("    line %d: %s\n",
                   tCtx.ptErrors[k].iLine,
                   tCtx.ptErrors[k].szMsg);
        TEST_ASSERT("[N] SOURCE.S assembles without errors", 0);
        as_shutdown(&tCtx);
        free(pRef);
        return;
    }
    TEST_ASSERT("[N] SOURCE.S assembles without errors", 1);

    /* INTENT[INT-AS-144 -> TC-AS-214 -> REQ-AS-002]:
     * Assembled .text size matches expected 10218 bytes.
     * Read from PRG header field (offset 2, big-endian u32) — the binary
     * buffer may include a fixup table after .text, so do not use
     * (uiBinaryLen - PRG_HEADER_SZ) which would over-count.             */
    uiTextSz = (tCtx.uiBinaryLen > PRG_HEADER_SZ + 4u)
               ? (size_t)((((st_u32_t)tCtx.pBinary[2] << 24) |
                            ((st_u32_t)tCtx.pBinary[3] << 16) |
                            ((st_u32_t)tCtx.pBinary[4] <<  8) |
                             (st_u32_t)tCtx.pBinary[5]))
               : 0u;
    if (uiTextSz != EXPECTED_TEXT)
    {
        printf("  .text size: got %u expected %u\n",
               (unsigned)uiTextSz, EXPECTED_TEXT);
    }
    TEST_ASSERT("[N] .text size == 10218", uiTextSz == EXPECTED_TEXT);

    /* INTENT[INT-AS-147 -> TC-AS-217 -> REQ-AS-002]:
     * First 4 bytes match                                               */
    pAsm = tCtx.pBinary + PRG_HEADER_SZ;
    pPrg = pRef          + PRG_HEADER_SZ;
    TEST_ASSERT("[N] .text[0..3] match reference",
        uiRefLen >= PRG_HEADER_SZ + 4u &&
        uiTextSz >= 4u &&
        pAsm[0] == pPrg[0] && pAsm[1] == pPrg[1] &&
        pAsm[2] == pPrg[2] && pAsm[3] == pPrg[3]);

    /* INTENT[INT-AS-145 -> TC-AS-215 -> REQ-AS-002] /
     * INTENT[INT-AS-148 -> TC-AS-218 -> REQ-AS-002]:
     * All 10218 .text bytes identical to real DEVPAC3 output           */
    {
        size_t uiCmp = uiTextSz;
        if (uiRefLen < PRG_HEADER_SZ + uiCmp)
            uiCmp = uiRefLen - PRG_HEADER_SZ;

        for (i = 0; i < uiCmp; i++)
        {
            if (pAsm[i] != pPrg[i])
            {
                uiDiffs++;
                if (uiShown < 32u)
                {
                    printf("  DIFF offset +%04X (byte %u): got %02X expected %02X\n",
                           (unsigned)i, (unsigned)i, pAsm[i], pPrg[i]);
                    uiShown++;
                }
            }
        }
    }

    if (uiDiffs > 0)
        printf("  Total byte differences: %u / %u\n", uiDiffs, EXPECTED_TEXT);

    TEST_ASSERT("[N] 0 byte difference vs SOURCE.PRG", uiDiffs == 0);

    as_shutdown(&tCtx);
    free(pRef);
    (void)read_be32; /* suppress unused-function warning */
}

/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC30E: DEVPAC3 torture test ===\n");

    test_robustness();
    test_source_torture();

    printf("\n--- UC30E: %d failure(s) ---\n", g_uc_fails);
    return g_uc_fails ? 1 : 0;
}
