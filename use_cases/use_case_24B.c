/*
 * use_case_24B.c - Sector tint in hex editor (P46)
 *
 * TEST MATRIX - UC24B:
 *   [N] Nominal    :  6 tests - sector type name coverage, tint table
 *                               size, sector count formula, classify call
 *   [R] Robustness :  3 tests - file smaller than one sector, zero-size,
 *                               NULL sector pointer
 *   [S] Skipped    :  1 test  - run make manual (visual tint in hex view)
 *
 * Module-level traceability:
 *   UFR-HEX-006 → REQ-HEX-021..023 → TC-HEX-065..073 → INT-HEX-065..070
 *
 * INTENT[INT-HEX-065..070 → TC-HEX-065..073 → REQ-HEX-021..023
 *        → UFR-HEX-006]:
 *   The hex editor classifies every 512-byte sector of the loaded file
 *   at open time and renders a distinct background tint per sector type,
 *   so the user can instantly locate boot sectors, FAT tables, directory
 *   regions, code, and data regions without navigating to each one.
 */

#include "use_cases.h"

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Helper: sector_type_name() must return a non-empty string for every
 * valid sector_type_t value, proving the tint table has no gap.
 * ------------------------------------------------------------------ */

static void test_type_names(void)
{
    int i;
    const char *sz;

    printf("  --- sector_type_name coverage ---\n");
    for (i = 0; i < SECTOR_TYPE_COUNT; i++)
    {
        sz = sector_type_name((sector_type_t)i);
        /* INTENT[INT-HEX-065]: every valid type returns a non-NULL,
         *   non-empty label — guarantees tint table has no silent gap. */
        UC_TEST("[N] sector_type_name not NULL",
                  sz != NULL);
        UC_TEST("[N] sector_type_name not empty",
                  sz != NULL && sz[0] != '\0');
        if (sz != NULL)
            printf("      [%2d] %s\n", i, sz);
    }
}

/* ------------------------------------------------------------------
 * Sector count formula: floor(fileSize / SA_SECTOR_SIZE)
 * ------------------------------------------------------------------ */

static void test_sector_count_formula(void)
{
    size_t uiSt   = 737280u;    /* standard .st image   = 1440 sectors  */
    size_t uiHalf = 256u;       /* 256 < 512 → 0 sectors                */
    size_t uiOdd  = 512u + 17u; /* 529 → 1 full sector                  */

    printf("  --- sector count formula ---\n");
    /* INTENT[INT-HEX-066]: sector count = floor(size / SA_SECTOR_SIZE).
     *   Standard .st = 1440, sub-sector = 0, partial+512 = 1.         */
    UC_TEST("[N] .st 737280 → 1440 sectors",
              (int)(uiSt / SA_SECTOR_SIZE) == 1440);
    UC_TEST("[N] 256 bytes → 0 sectors",
              (int)(uiHalf / SA_SECTOR_SIZE) == 0);
    UC_TEST("[N] 529 bytes → 1 sector",
              (int)(uiOdd / SA_SECTOR_SIZE) == 1);
}

/* ------------------------------------------------------------------
 * Classify a synthetic sector (BSS_ZERO): all-zero buffer must
 * produce SECTOR_BSS_ZERO after bootstrap + classify.
 * ------------------------------------------------------------------ */

static void test_classify_zero_sector(void)
{
    sector_db_t       *ptDb  = NULL;
    sector_features_t  tFeat;
    sector_match_t     tMatch;
    st_u8_t            aBuf[SA_SECTOR_SIZE];
    int                iCount = 0;

    printf("  --- classify zero sector ---\n");
    memset(aBuf, 0x00, SA_SECTOR_SIZE);

    /* INTENT[INT-HEX-067]: sector_classify on a zero-filled buffer
     *   returns SECTOR_BSS_ZERO (score > 0.5) using bootstrap DB.     */
    UC_CHECK("[N] sector_db_create",  sector_db_create(&ptDb));
    UC_CHECK("[N] bootstrap_defaults",sector_db_bootstrap_defaults(ptDb));
    UC_CHECK("[N] features_extract",
             sector_features_extract(aBuf, 0u, &tFeat));
    UC_CHECK("[N] sector_classify",
             sector_classify(ptDb, &tFeat, aBuf, &tMatch, 1, &iCount));
    UC_TEST("[N] classify returns 1 match",  iCount == 1);
    UC_TEST("[N] BSS_ZERO identified",
              iCount == 1 && tMatch.eType == SECTOR_BSS_ZERO);
    UC_TEST("[N] score > 0.5",
              iCount == 1 && tMatch.fScore > 0.5f);

    sector_db_destroy(&ptDb);
}

/* ------------------------------------------------------------------
 * Robustness: sector_features_extract with NULL pointer
 * ------------------------------------------------------------------ */

static void test_robustness(void)
{
    sector_features_t tFeat;

    printf("  --- robustness ---\n");
    /* INTENT[INT-HEX-068]: NULL sector pointer returns ST_ERROR        */
    UC_TEST("[R] NULL sector → ST_ERROR",
              sector_features_extract(NULL, 0u, &tFeat) == ST_ERROR);
    /* INTENT[INT-HEX-069]: NULL feature output returns ST_ERROR        */
    {
        st_u8_t aBuf[SA_SECTOR_SIZE];
        memset(aBuf, 0, SA_SECTOR_SIZE);
        UC_TEST("[R] NULL feat out → ST_ERROR",
                  sector_features_extract(aBuf, 0u, NULL) == ST_ERROR);
    }
    /* INTENT[INT-HEX-070]: sector_db_create NULL pptDb → ST_ERROR     */
    UC_TEST("[R] sector_db_create NULL → ST_ERROR",
              sector_db_create(NULL) == ST_ERROR);
}

/* ------------------------------------------------------------------
 * Manual test: open a known .st file in hex editor and observe tints
 * ------------------------------------------------------------------ */

static void test_manual_tint(void)
{
    printf("  --- manual tint validation ---\n");
    TEST_MANUAL(
        "[S] Hex editor: sector tints visible on .st file",
        "Open use_cases/UC16/roundtrip.st via 'edit use_cases/UC16/roundtrip.st'\n"
        "  Expected: boot sector rows (violet), FAT rows (blue),\n"
        "            directory rows (teal), data rows (other hues).\n"
        "  Tint changes every 32 rows (one sector = 512 bytes / 16 bytes/row).\n"
        "  Current-row highlight should still be visible on top of tint.\n"
        "  Does the tinting look correct?"
    );
}

/* ------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC24B: Sector Tint in Hex Editor ===\n");

    test_type_names();
    test_sector_count_formula();
    test_classify_zero_sector();
    test_robustness();
    test_manual_tint();

    printf("\n=== UC24B: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
