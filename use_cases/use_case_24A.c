/*
 * use_case_24A.c - Sector Fingerprint Engine tests
 *
 * TEST MATRIX - UC24A:
 *   [N] Nominal    : 32 tests - feature extraction, DB lifecycle,
 *                               classification, packer signature scan,
 *                               persistence, type name API
 *   [R] Robustness :  8 tests - NULL params, empty DB, bad file
 *   [S] Skipped    :  2 tests - whatisit.st sectors (gitignored image)
 *
 * Module-level traceability:
 *   UFR-EXE-010 → REQ-SAN-001..010 → TC-SAN-001..020 → INT-SAN-001..018
 *
 * INTENT[INT-SAN-001..018 → TC-SAN-001..020 → REQ-SAN-001..010
 *        → UFR-EXE-010]:
 *   Static sector analysis identifies the type of any 512-byte Atari
 *   ST disk sector via a 24-D weighted feature vector compared against
 *   a reference database using cosine similarity, without executing
 *   the disk content.
 */

#include "use_cases.h"
#include <math.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

static void print_features(const sector_features_t *pt)
{
    printf("    BpbValid=%.2f  WD1772=%.2f  FatPat=%.2f"
           "  DirDens=%.2f\n",
           pt->fBpbValid, pt->fWd1772Bootable,
           pt->fFatPattern, pt->fDirDensity);
    printf("    Entropy=%.2f  Zero=%.2f  E5=%.2f  FF=%.2f"
           "  Ascii=%.2f  Rep=%.2f\n",
           pt->fEntropy, pt->fZeroRuns, pt->fE5Runs, pt->fFfRuns,
           pt->fAsciiRatio, pt->fRepeating);
    printf("    OpcodeDens=%.2f  WordAlgn=%.2f"
           "  Branch=%.2f  RelBr=%.2f\n",
           pt->fOpcodeDensity, pt->fWordAlignRatio,
           pt->fBranchDensity, pt->fRelBranchValid);
    printf("    HwImm=%.2f  HwCtx=%.2f  Timing=%.2f  Vbl=%.2f\n",
           pt->fHwImmediate, pt->fHwInContext,
           pt->fTimingLoops, pt->fVblInstall);
    printf("    Sinus=%.2f  Gfx=%.2f  Packer=%.2f"
           "  YM=%.2f  Vid=%.2f  FDC=%.2f\n",
           pt->fSinusProfile, pt->fGraphicsPattern, pt->fPackerEntropy,
           pt->fYmAccess, pt->fVideoAccess, pt->fFdcAccess);
}

static int near_equal(float a, float b, float tol)
{
    float diff = a - b;
    if (diff < 0.0f) diff = -diff;
    return diff <= tol;
}

/* ------------------------------------------------------------------
 * Test 1 — feature extraction: all-zero sector (BSS)
 * ------------------------------------------------------------------ */
static void test_feat_bss_zero(void)
{
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;

    /* INTENT[INT-SAN-001 → TC-SAN-001 → REQ-SAN-001,002 → UFR-EXE-010]:
     * All-zero sector (BSS) must have fZeroRuns=1, fEntropy=0,
     * fFatPattern=0, fHwImmediate=0, fVblInstall=0 */
    printf("\n--- test_feat_bss_zero ---\n");
    memset(aSec, 0x00, sizeof(aSec));

    UC_CHECK("[N] extract returns ST_NO_ERROR",
             sector_features_extract(aSec, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fBpbValid == 0 (BPS=0 not 512)",
                near_equal(tF.fBpbValid, 0.0f, 0.01f));
    TEST_ASSERT("[N] fFatPattern == 0 (first byte 0x00 not 0xF0+)",
                near_equal(tF.fFatPattern, 0.0f, 0.01f));
    TEST_ASSERT("[N] fZeroRuns == 1.0",
                near_equal(tF.fZeroRuns, 1.0f, 0.01f));
    TEST_ASSERT("[N] fFfRuns == 0.0",
                near_equal(tF.fFfRuns, 0.0f, 0.01f));
    TEST_ASSERT("[N] fE5Runs == 0.0",
                near_equal(tF.fE5Runs, 0.0f, 0.01f));
    TEST_ASSERT("[N] fEntropy == 0.0 (single byte value)",
                near_equal(tF.fEntropy, 0.0f, 0.01f));
    TEST_ASSERT("[N] fHwImmediate == 0.0",
                near_equal(tF.fHwImmediate, 0.0f, 0.01f));
    TEST_ASSERT("[N] fVblInstall == 0.0",
                near_equal(tF.fVblInstall, 0.0f, 0.01f));
    TEST_ASSERT("[N] fYmAccess == 0.0",
                near_equal(tF.fYmAccess, 0.0f, 0.01f));
    TEST_ASSERT("[N] fRepeating == 1.0 (all same byte)",
                near_equal(tF.fRepeating, 1.0f, 0.01f));
}

/* ------------------------------------------------------------------
 * Test 2 — feature extraction: all-0xFF sector (unformatted)
 * ------------------------------------------------------------------ */
static void test_feat_unformatted(void)
{
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;

    /* INTENT[INT-SAN-002 → TC-SAN-002 → REQ-SAN-001 → UFR-EXE-010]:
     * All-0xFF sector (unformatted) must have fFfRuns=1, fEntropy=0,
     * fBpbValid=0, fRepeating=1 */
    printf("\n--- test_feat_unformatted ---\n");
    memset(aSec, 0xFF, sizeof(aSec));

    UC_CHECK("[N] extract returns ST_NO_ERROR",
             sector_features_extract(aSec, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fFfRuns == 1.0",
                near_equal(tF.fFfRuns, 1.0f, 0.01f));
    TEST_ASSERT("[N] fZeroRuns == 0.0",
                near_equal(tF.fZeroRuns, 0.0f, 0.01f));
    TEST_ASSERT("[N] fEntropy == 0.0 (single byte value)",
                near_equal(tF.fEntropy, 0.0f, 0.01f));
    TEST_ASSERT("[N] fRepeating == 1.0",
                near_equal(tF.fRepeating, 1.0f, 0.01f));
    TEST_ASSERT("[N] fBpbValid == 0.0",
                near_equal(tF.fBpbValid, 0.0f, 0.01f));
}

/* ------------------------------------------------------------------
 * Test 3 — feature extraction: FAT12 sector header
 * ------------------------------------------------------------------ */
static void test_feat_fat12(void)
{
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;

    /* INTENT[INT-SAN-003 → TC-SAN-003 → REQ-SAN-004 → UFR-EXE-010]:
     * FAT12 header sector (0xF9 FF FF...) must detect fFatPattern=1.0;
     * it is not a boot sector so fBpbValid must remain 0. */
    printf("\n--- test_feat_fat12 ---\n");
    memset(aSec, 0x00, sizeof(aSec));
    aSec[0] = 0xF9u; aSec[1] = 0xFFu; aSec[2] = 0xFFu;
    aSec[3] = 0xFFu; aSec[4] = 0xFFu;

    UC_CHECK("[N] extract returns ST_NO_ERROR",
             sector_features_extract(aSec, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fFatPattern == 1.0",
                near_equal(tF.fFatPattern, 1.0f, 0.01f));
    TEST_ASSERT("[N] fBpbValid == 0.0 (not a boot sector)",
                near_equal(tF.fBpbValid, 0.0f, 0.01f));
}

/* ------------------------------------------------------------------
 * Test 4 — feature extraction: all-0xE5 sector (deleted dir)
 * ------------------------------------------------------------------ */
static void test_feat_deleted_dir(void)
{
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;

    /* INTENT[INT-SAN-004 → TC-SAN-004 → REQ-SAN-001 → UFR-EXE-010]:
     * All-0xE5 sector (deleted directory entries) must have
     * fE5Runs=1, fRepeating=1, fFfRuns=0 */
    printf("\n--- test_feat_deleted_dir ---\n");
    memset(aSec, 0xE5u, sizeof(aSec));

    UC_CHECK("[N] extract returns ST_NO_ERROR",
             sector_features_extract(aSec, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fE5Runs == 1.0",
                near_equal(tF.fE5Runs, 1.0f, 0.01f));
    TEST_ASSERT("[N] fRepeating == 1.0",
                near_equal(tF.fRepeating, 1.0f, 0.01f));
    TEST_ASSERT("[N] fFfRuns == 0.0",
                near_equal(tF.fFfRuns, 0.0f, 0.01f));
}

/* ------------------------------------------------------------------
 * Test 5 — feature extraction: synthetic VBL install code
 * ------------------------------------------------------------------ */
static void test_feat_vbl_install(void)
{
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;

    /* INTENT[INT-SAN-005 → TC-SAN-005 → REQ-SAN-005 → UFR-EXE-010]:
     * Pattern 21 FC ?? ?? ?? ?? 00 70 (MOVE.L #handler,$70.W) must
     * set fVblInstall=1.0 regardless of surrounding code */
    printf("\n--- test_feat_vbl_install ---\n");
    memset(aSec, 0x4Eu, sizeof(aSec));  /* NOP fill */

    /* MOVE.L #handler,$70.W at offset 0 */
    aSec[0]  = 0x21u; aSec[1]  = 0xFCu;  /* opcode */
    aSec[2]  = 0x00u; aSec[3]  = 0x00u;  /* handler hi */
    aSec[4]  = 0x40u; aSec[5]  = 0x00u;  /* handler lo */
    aSec[6]  = 0x00u; aSec[7]  = 0x70u;  /* $0070.W */

    UC_CHECK("[N] extract returns ST_NO_ERROR",
             sector_features_extract(aSec, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fVblInstall == 1.0",
                near_equal(tF.fVblInstall, 1.0f, 0.01f));
}

/* ------------------------------------------------------------------
 * Test 6 — feature extraction: synthetic palette (graphics pattern)
 * ------------------------------------------------------------------ */
static void test_feat_graphics_palette(void)
{
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;
    int               i;

    /* INTENT[INT-SAN-006 → TC-SAN-006 → REQ-SAN-001 → UFR-EXE-010]:
     * A sector filled with valid ST palette words (nibbles 0-7,
     * bits F888=0) must set fGraphicsPattern=1.0 */
    printf("\n--- test_feat_graphics_palette ---\n");

    /* 256 valid ST palette words: each nibble 0-7, bits F888 = 0 */
    for (i = 0; i < (int)(SA_SECTOR_SIZE / 2u); i++)
    {
        st_u16_t uiW = (st_u16_t)(0x0123u + i * 0x0011u) & 0x0777u;
        aSec[i * 2]     = (st_u8_t)(uiW >> 8u);
        aSec[i * 2 + 1] = (st_u8_t)(uiW & 0xFFu);
    }

    UC_CHECK("[N] extract returns ST_NO_ERROR",
             sector_features_extract(aSec, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fGraphicsPattern == 1.0",
                near_equal(tF.fGraphicsPattern, 1.0f, 0.01f));
}

/* ------------------------------------------------------------------
 * Test 7 — feature extraction: HW register access pattern
 * ------------------------------------------------------------------ */
static void test_feat_hw_access(void)
{
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;

    /* INTENT[INT-SAN-007 → TC-SAN-007 → REQ-SAN-001 → UFR-EXE-010]:
     * Sectors containing 0xFF 0x8x byte pairs (HW register immediates)
     * must set fVideoAccess=1 (≥4 video pairs) and fHwImmediate>0 */
    printf("\n--- test_feat_hw_access ---\n");
    memset(aSec, 0x00, sizeof(aSec));

    /* Plant 8 occurrences of 0xFF 0x82 (Shifter video access) */
    {
        int i;
        for (i = 0; i < 8; i++)
        {
            aSec[i * 4]     = 0xFFu;
            aSec[i * 4 + 1] = 0x82u;
            aSec[i * 4 + 2] = (st_u8_t)(i * 4 + 2);
            aSec[i * 4 + 3] = 0x00u;
        }
    }

    UC_CHECK("[N] extract returns ST_NO_ERROR",
             sector_features_extract(aSec, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fVideoAccess == 1.0 (8 >= 4 occurrences)",
                near_equal(tF.fVideoAccess, 1.0f, 0.01f));
    TEST_ASSERT("[N] fHwImmediate > 0.0",
                tF.fHwImmediate > 0.0f);
}

/* ------------------------------------------------------------------
 * Test 8 — feature extraction: high-entropy sector (packer)
 * ------------------------------------------------------------------ */
static void test_feat_packer_entropy(void)
{
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;
    int               i;

    /* INTENT[INT-SAN-008 → TC-SAN-008 → REQ-SAN-001 → UFR-EXE-010]:
     * Pseudo-random high-entropy data (no valid opcodes) must have
     * fEntropy > 0.9; fPackerEntropy is a derived feature (UC24A
     * static analysis cannot distinguish packer from random data) */
    printf("\n--- test_feat_packer_entropy ---\n");

    /* Pseudo-random data with high entropy, no valid opcodes */
    for (i = 0; i < (int)SA_SECTOR_SIZE; i++)
        aSec[i] = (st_u8_t)(i * 7 + 37);

    UC_CHECK("[N] extract returns ST_NO_ERROR",
             sector_features_extract(aSec, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fEntropy > 0.9 (high entropy sequence)",
                tF.fEntropy > 0.9f);
}

/* ------------------------------------------------------------------
 * Test 9 — DB lifecycle: create / bootstrap / destroy
 * ------------------------------------------------------------------ */
static void test_db_lifecycle(void)
{
    sector_db_t *ptDb = NULL;

    /* INTENT[INT-SAN-009 → TC-SAN-009 → REQ-SAN-006 → UFR-EXE-010]:
     * DB lifecycle (create/bootstrap/destroy) must succeed atomically;
     * bootstrap must populate >0 types derived from synthetic sectors */
    printf("\n--- test_db_lifecycle ---\n");

    UC_CHECK("[N] sector_db_create",
             sector_db_create(&ptDb));
    TEST_ASSERT("[N] ptDb not NULL", ptDb != NULL);

    UC_CHECK("[N] sector_db_bootstrap_defaults",
             sector_db_bootstrap_defaults(ptDb));
    TEST_ASSERT("[N] iTypeCount > 0 after bootstrap",
                ptDb->iTypeCount > 0);

    printf("    iTypeCount=%d  iSigCount=%d\n",
           ptDb->iTypeCount, ptDb->iSigCount);

    UC_CHECK("[N] sector_db_destroy",
             sector_db_destroy(&ptDb));
    TEST_ASSERT("[N] ptDb NULL after destroy", ptDb == NULL);
}

/* ------------------------------------------------------------------
 * Test 10 — DB learn + finalize: identical sample -> score ~1.0
 * ------------------------------------------------------------------ */
static void test_db_learn_classify(void)
{
    sector_db_t     *ptDb    = NULL;
    st_u8_t          aSec[SA_SECTOR_SIZE];
    sector_features_t tFeat;
    sector_match_t   aM[3];
    int              iCount  = 0;

    /* INTENT[INT-SAN-010 → TC-SAN-010 → REQ-SAN-006 → UFR-EXE-010]:
     * After learn+finalize, classifying the exact learned sample must
     * return cosine score > 0.99 with the correct type */
    printf("\n--- test_db_learn_classify ---\n");

    /* Build a DB with a single known type (all-zero = BSS_ZERO) */
    memset(aSec, 0x00, sizeof(aSec));
    UC_CHECK("[N] db_create", sector_db_create(&ptDb));
    UC_CHECK("[N] db_learn bss_zero",
             sector_db_learn(ptDb, aSec, 0u,
                              SECTOR_BSS_ZERO, "bss_zero"));
    UC_CHECK("[N] db_finalize", sector_db_finalize(ptDb));

    /* Classify the same sector -> should score very close to 1.0 */
    UC_CHECK("[N] features_extract",
             sector_features_extract(aSec, 0u, &tFeat));
    UC_CHECK("[N] sector_classify",
             sector_classify(ptDb, &tFeat, aSec, aM, 3, &iCount));

    printf("    iCount=%d  top: eType=%d score=%.4f label=%s\n",
           iCount, (int)aM[0].eType, aM[0].fScore, aM[0].szLabel);

    TEST_ASSERT("[N] iCount >= 1", iCount >= 1);
    TEST_ASSERT("[N] top type == SECTOR_BSS_ZERO",
                aM[0].eType == SECTOR_BSS_ZERO);
    TEST_ASSERT("[N] top score > 0.99 (identical vector)",
                aM[0].fScore > 0.99f);

    sector_db_destroy(&ptDb);
}

/* ------------------------------------------------------------------
 * Test 11 — classify with bootstrapped DB: trivial types
 * ------------------------------------------------------------------ */
static void test_db_classify_trivial(void)
{
    sector_db_t     *ptDb = NULL;
    st_u8_t          aSec[SA_SECTOR_SIZE];
    sector_features_t tF;
    sector_match_t   aM[3];
    int              iN   = 0;

    /* INTENT[INT-SAN-011 → TC-SAN-011 → REQ-SAN-006 → UFR-EXE-010]:
     * Bootstrap DB must classify synthetic trivial sectors correctly:
     * all-0xFF → UNFORMATTED; all-0xE5 → DIRECTORY_DELETED */
    printf("\n--- test_db_classify_trivial ---\n");

    UC_CHECK("[N] db_create + bootstrap",
             sector_db_create(&ptDb));
    UC_CHECK("[N] bootstrap_defaults",
             sector_db_bootstrap_defaults(ptDb));

    /* All-0xFF → UNFORMATTED */
    memset(aSec, 0xFF, sizeof(aSec));
    UC_CHECK("[N] extract unformatted",
             sector_features_extract(aSec, 0u, &tF));
    UC_CHECK("[N] classify unformatted",
             sector_classify(ptDb, &tF, aSec, aM, 3, &iN));
    printf("    [0xFF] top: type=%s score=%.3f\n",
           aM[0].szLabel, aM[0].fScore);
    TEST_ASSERT("[N] all-0xFF classified as unformatted",
                aM[0].eType == SECTOR_UNFORMATTED);

    /* All-0xE5 → DIRECTORY_DELETED */
    memset(aSec, 0xE5u, sizeof(aSec));
    UC_CHECK("[N] extract deleted",
             sector_features_extract(aSec, 0u, &tF));
    UC_CHECK("[N] classify deleted",
             sector_classify(ptDb, &tF, aSec, aM, 3, &iN));
    printf("    [0xE5] top: type=%s score=%.3f\n",
           aM[0].szLabel, aM[0].fScore);
    TEST_ASSERT("[N] all-0xE5 classified as directory_deleted",
                aM[0].eType == SECTOR_DIRECTORY_DELETED);

    sector_db_destroy(&ptDb);
}

/* ------------------------------------------------------------------
 * Test 12 — persist DB to file and reload
 * ------------------------------------------------------------------ */
static void test_db_save_load(void)
{
    sector_db_t     *ptDb1 = NULL;
    sector_db_t     *ptDb2 = NULL;
    st_u8_t          aSec[SA_SECTOR_SIZE];
    sector_features_t tF;
    sector_match_t   aM[3];
    int              iN    = 0;
    const char      *szTmp = "build/uc24a_test_db.bin";

    /* INTENT[INT-SAN-012 → TC-SAN-012 → REQ-SAN-008 → UFR-EXE-010]:
     * DB save+load round-trip must preserve iTypeCount, iSigCount,
     * signature name/type, and produce correct classification after
     * reload; the binary format is guarded by SA_DB_MAGIC */
    printf("\n--- test_db_save_load ---\n");

    /* Build + save */
    UC_CHECK("[N] db_create + bootstrap",
             sector_db_create(&ptDb1));
    UC_CHECK("[N] bootstrap_defaults",
             sector_db_bootstrap_defaults(ptDb1));

    /* Add a packer signature */
    {
        sector_packer_sig_t *ps = &ptDb1->atSigs[ptDb1->iSigCount++];
        strncpy(ps->szName, "TestPacker", sizeof(ps->szName) - 1u);
        ps->eType       = SECTOR_CODE_PACKER;
        ps->iOffset     = 0;
        ps->aPattern[0] = 0xABu;
        ps->aPattern[1] = 0xCDu;
        ps->aMask[0]    = 0xFFu;
        ps->aMask[1]    = 0xFFu;
        ps->iPatternLen = 2;
    }

    UC_CHECK("[N] db_save",
             sector_db_save(ptDb1, szTmp));
    sector_db_destroy(&ptDb1);

    /* Reload */
    UC_CHECK("[N] db_create for load", sector_db_create(&ptDb2));
    UC_CHECK("[N] db_load",
             sector_db_load(ptDb2, szTmp));

    TEST_ASSERT("[N] loaded iTypeCount > 0", ptDb2->iTypeCount > 0);
    TEST_ASSERT("[N] loaded iSigCount == 1", ptDb2->iSigCount == 1);
    printf("    loaded: iTypeCount=%d  iSigCount=%d\n",
           ptDb2->iTypeCount, ptDb2->iSigCount);
    printf("    sig[0].szName=%s  eType=%d\n",
           ptDb2->atSigs[0].szName, (int)ptDb2->atSigs[0].eType);

    TEST_ASSERT("[N] sig name preserved",
                strcmp(ptDb2->atSigs[0].szName, "TestPacker") == 0);
    TEST_ASSERT("[N] sig type preserved",
                ptDb2->atSigs[0].eType == SECTOR_CODE_PACKER);

    /* Verify classification still works */
    memset(aSec, 0xFF, sizeof(aSec));
    UC_CHECK("[N] extract after reload",
             sector_features_extract(aSec, 0u, &tF));
    UC_CHECK("[N] classify after reload",
             sector_classify(ptDb2, &tF, aSec, aM, 3, &iN));
    TEST_ASSERT("[N] classify correct after load",
                aM[0].eType == SECTOR_UNFORMATTED);

    sector_db_destroy(&ptDb2);
    remove(szTmp);
}

/* ------------------------------------------------------------------
 * Test 13 — packer signature detection overrides cosine
 * ------------------------------------------------------------------ */
static void test_packer_sig_override(void)
{
    sector_db_t     *ptDb = NULL;
    st_u8_t          aSec[SA_SECTOR_SIZE];
    sector_features_t tF;
    sector_match_t   aM[3];
    int              iN   = 0;

    /* INTENT[INT-SAN-013 → TC-SAN-013 → REQ-SAN-007 → UFR-EXE-010]:
     * When a packer byte-pattern signature matches, the classified
     * type must be the signature's eType with score=1.0, overriding
     * the cosine result regardless of feature values */
    printf("\n--- test_packer_sig_override ---\n");

    memset(aSec, 0x00, sizeof(aSec));

    UC_CHECK("[N] db_create + bootstrap",
             sector_db_create(&ptDb));
    UC_CHECK("[N] bootstrap_defaults",
             sector_db_bootstrap_defaults(ptDb));

    /* Add a signature that matches offset 0: 0xDE 0xAD */
    {
        sector_packer_sig_t *ps = &ptDb->atSigs[ptDb->iSigCount++];
        strncpy(ps->szName, "TestSig", sizeof(ps->szName) - 1u);
        ps->eType       = SECTOR_PROTECTION;
        ps->iOffset     = 0;
        ps->aPattern[0] = 0xDEu;
        ps->aPattern[1] = 0xADu;
        ps->aMask[0]    = 0xFFu;
        ps->aMask[1]    = 0xFFu;
        ps->iPatternLen = 2;
    }

    /* Plant the signature in the sector */
    aSec[0] = 0xDEu;
    aSec[1] = 0xADu;

    UC_CHECK("[N] extract",
             sector_features_extract(aSec, 0u, &tF));
    UC_CHECK("[N] classify",
             sector_classify(ptDb, &tF, aSec, aM, 3, &iN));

    printf("    top: type=%d label=%s score=%.3f\n",
           (int)aM[0].eType, aM[0].szLabel, aM[0].fScore);

    TEST_ASSERT("[N] signature match overrides cosine (type=PROTECTION)",
                aM[0].eType == SECTOR_PROTECTION);
    TEST_ASSERT("[N] signature score == 1.0",
                near_equal(aM[0].fScore, 1.0f, 0.01f));

    sector_db_destroy(&ptDb);
}

/* ------------------------------------------------------------------
 * Test 14 — sector_type_name() API
 * ------------------------------------------------------------------ */
static void test_type_name(void)
{
    /* INTENT[INT-SAN-014 → TC-SAN-014 → REQ-SAN-009 → UFR-EXE-010]:
     * sector_type_name() must return correct static strings for all
     * valid enum values; an out-of-range value must return "unknown"
     * without crash */
    printf("\n--- test_type_name ---\n");

    TEST_ASSERT("[N] SECTOR_UNKNOWN -> 'unknown'",
                strcmp(sector_type_name(SECTOR_UNKNOWN), "unknown") == 0);
    TEST_ASSERT("[N] SECTOR_FAT12 -> 'fat12'",
                strcmp(sector_type_name(SECTOR_FAT12), "fat12") == 0);
    TEST_ASSERT("[N] SECTOR_BSS_ZERO -> 'bss_zero'",
                strcmp(sector_type_name(SECTOR_BSS_ZERO), "bss_zero") == 0);
    TEST_ASSERT("[N] SECTOR_UNFORMATTED -> 'unformatted'",
                strcmp(sector_type_name(SECTOR_UNFORMATTED),
                       "unformatted") == 0);
    TEST_ASSERT("[N] SECTOR_CODE_DEMO -> 'code_demo'",
                strcmp(sector_type_name(SECTOR_CODE_DEMO),
                       "code_demo") == 0);
    TEST_ASSERT("[N] out-of-range -> 'unknown'",
                strcmp(sector_type_name((sector_type_t)99), "unknown") == 0);
}

/* ------------------------------------------------------------------
 * Test 15 — robustness: NULL parameters
 * ------------------------------------------------------------------ */
static void test_robustness_null(void)
{
    st_u8_t          aSec[SA_SECTOR_SIZE];
    sector_features_t tF;
    sector_db_t      *ptDb  = NULL;
    sector_match_t   aM[1];
    int              iN     = 0;

    /* INTENT[INT-SAN-015 → TC-SAN-015..017 → REQ-SAN-001,006,007
     *        → UFR-EXE-010]:
     * All public functions must reject NULL parameters with ST_ERROR
     * and leave state unchanged; iMaxMatches=0 must also be rejected */
    printf("\n--- test_robustness_null ---\n");
    memset(aSec, 0x00, sizeof(aSec));

    TEST_ASSERT("[R] extract NULL sector -> ST_ERROR",
                sector_features_extract(NULL, 0u, &tF) == ST_ERROR);
    TEST_ASSERT("[R] extract NULL feat -> ST_ERROR",
                sector_features_extract(aSec, 0u, NULL) == ST_ERROR);
    TEST_ASSERT("[R] db_create NULL pptDb -> ST_ERROR",
                sector_db_create(NULL) == ST_ERROR);
    TEST_ASSERT("[R] db_destroy NULL pptDb -> ST_ERROR",
                sector_db_destroy(NULL) == ST_ERROR);
    TEST_ASSERT("[R] db_bootstrap NULL -> ST_ERROR",
                sector_db_bootstrap_defaults(NULL) == ST_ERROR);

    /* Create a valid DB for further NULL tests */
    sector_db_create(&ptDb);
    sector_db_bootstrap_defaults(ptDb);

    {
        sector_features_t tFeat;
        sector_features_extract(aSec, 0u, &tFeat);

        TEST_ASSERT("[R] classify NULL ptDb -> ST_ERROR",
                    sector_classify(NULL, &tFeat, aSec,
                                    aM, 1, &iN) == ST_ERROR);
        TEST_ASSERT("[R] classify NULL ptFeat -> ST_ERROR",
                    sector_classify(ptDb, NULL, aSec,
                                    aM, 1, &iN) == ST_ERROR);
        TEST_ASSERT("[R] classify NULL aMatches -> ST_ERROR",
                    sector_classify(ptDb, &tFeat, aSec,
                                    NULL, 1, &iN) == ST_ERROR);
        TEST_ASSERT("[R] classify iMaxMatches==0 -> ST_ERROR",
                    sector_classify(ptDb, &tFeat, aSec,
                                    aM, 0, &iN) == ST_ERROR);
    }

    sector_db_destroy(&ptDb);
}

/* ------------------------------------------------------------------
 * Test 16 — robustness: load from nonexistent file
 * ------------------------------------------------------------------ */
static void test_robustness_bad_file(void)
{
    sector_db_t *ptDb = NULL;

    /* INTENT[INT-SAN-016 → TC-SAN-018 → REQ-SAN-008 → UFR-EXE-010]:
     * sector_db_load() on a nonexistent file must return ST_ERROR
     * silently without modifying the DB (iTypeCount stays 0) */
    printf("\n--- test_robustness_bad_file ---\n");

    UC_CHECK("[R] db_create", sector_db_create(&ptDb));
    TEST_ASSERT("[R] db_load nonexistent -> ST_ERROR",
                sector_db_load(ptDb,
                    "build/nonexistent_uc24a.bin") == ST_ERROR);
    TEST_ASSERT("[R] iTypeCount still 0 after failed load",
                ptDb->iTypeCount == 0);

    sector_db_destroy(&ptDb);
}

/* ------------------------------------------------------------------
 * Test 17 — whatisit.st bootsector (conditional: file may be absent)
 * ------------------------------------------------------------------ */
static void test_whatisit_bootsector(void)
{
    const char   *szPath = "use_cases/UC20A/st/whatisit.st";
    image_st_t   *ptImg  = NULL;
    st_u8_t      *pDisk  = NULL;
    sector_features_t tF;
    sector_db_t  *ptDb   = NULL;
    sector_match_t aM[3];
    int            iN    = 0;

    /* INTENT[INT-SAN-017 → TC-SAN-019 → REQ-SAN-001..003,006
     *        → UFR-EXE-010]:
     * whatisit.st sector 0 must have fBpbValid=1 (valid BPB geometry)
     * and fWd1772Bootable=0 (checksum=0x1235 ≠ 0x1234); it must
     * classify as SECTOR_BOOTSECTOR_NOBOOT after teaching the DB */
    printf("\n--- test_whatisit_bootsector ---\n");

    if (image_st_load(szPath, &ptImg) != ST_NO_ERROR)
    {
        TEST_SKIP("[S] whatisit.st not available (gitignored) "
                  "— run with image in use_cases/UC20A/st/");
        return;
    }

    image_st_get_disk(ptImg, &pDisk);

    /* Extract features from sector 0 */
    UC_CHECK("[N] extract sector 0",
             sector_features_extract(pDisk, 0u, &tF));
    print_features(&tF);

    TEST_ASSERT("[N] fBpbValid == 1.0 (valid BPB)",
                near_equal(tF.fBpbValid, 1.0f, 0.01f));
    TEST_ASSERT("[N] fWd1772Bootable == 0.0 (checksum 0x1235)",
                near_equal(tF.fWd1772Bootable, 0.0f, 0.01f));

    /* Classify */
    sector_db_create(&ptDb);
    sector_db_bootstrap_defaults(ptDb);

    /* Teach the DB about this disk's bootsector */
    sector_db_learn(ptDb, pDisk, 0u,
                    SECTOR_BOOTSECTOR_NOBOOT, "bootsector_noboot");
    sector_db_finalize(ptDb);

    UC_CHECK("[N] classify",
             sector_classify(ptDb, &tF, pDisk, aM, 3, &iN));
    printf("    top: type=%s score=%.3f\n", aM[0].szLabel, aM[0].fScore);

    TEST_ASSERT("[N] sector 0 classified as bootsector_noboot",
                aM[0].eType == SECTOR_BOOTSECTOR_NOBOOT);

    sector_db_destroy(&ptDb);
    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------
 * Test 18 — dump-features mode: print all 24 features (informational)
 * ------------------------------------------------------------------ */
static void test_dump_feature_names(void)
{
    static const char *aszNames[SA_FEATURE_DIM] =
    {
        "fBpbValid",      "fWd1772Bootable", "fFatPattern",
        "fDirDensity",    "fEntropy",        "fZeroRuns",
        "fE5Runs",        "fFfRuns",         "fAsciiRatio",
        "fRepeating",     "fOpcodeDensity",  "fWordAlignRatio",
        "fBranchDensity", "fRelBranchValid", "fHwImmediate",
        "fHwInContext",   "fTimingLoops",    "fVblInstall",
        "fSinusProfile",  "fGraphicsPattern","fPackerEntropy",
        "fYmAccess",      "fVideoAccess",    "fFdcAccess"
    };
    st_u8_t           aSec[SA_SECTOR_SIZE];
    sector_features_t tF;
    const float      *aFArr;
    int               i;

    /* INTENT[INT-SAN-018 → TC-SAN-020 → REQ-SAN-010 → UFR-EXE-010]:
     * SA_FEATURE_DIM must equal 24; sizeof(sector_features_t)/sizeof(float)
     * must equal SA_FEATURE_DIM — the struct is a plain flat array of
     * 24 floats with no padding, enabling cast to (const float *) */
    printf("\n--- test_dump_feature_names ---\n");
    memset(aSec, 0x00, sizeof(aSec));
    aSec[0] = 0xF9u; aSec[1] = 0xFFu; aSec[2] = 0xFFu;  /* FAT hdr */

    sector_features_extract(aSec, 0u, &tF);
    aFArr = (const float *)&tF;

    printf("    Feature vector for FAT12-header sector:\n");
    for (i = 0; i < SA_FEATURE_DIM; i++)
        printf("    [%2d] %-20s = %.4f\n", i, aszNames[i], aFArr[i]);

    TEST_ASSERT("[N] SA_FEATURE_DIM == 24", SA_FEATURE_DIM == 24);
    TEST_ASSERT("[N] sector_features_t size matches feature dim",
                (int)(sizeof(sector_features_t) / sizeof(float))
                == SA_FEATURE_DIM);
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */
int main(void)
{
    printf("========================================================\n");
    printf(" UC24A - Sector Fingerprint Engine\n");
    printf("========================================================\n");

    test_feat_bss_zero();
    test_feat_unformatted();
    test_feat_fat12();
    test_feat_deleted_dir();
    test_feat_vbl_install();
    test_feat_graphics_palette();
    test_feat_hw_access();
    test_feat_packer_entropy();
    test_db_lifecycle();
    test_db_learn_classify();
    test_db_classify_trivial();
    test_db_save_load();
    test_packer_sig_override();
    test_type_name();
    test_robustness_null();
    test_robustness_bad_file();
    test_whatisit_bootsector();
    test_dump_feature_names();

    printf("\n========================================================\n");
    printf(" UC24A results: %d failure(s)\n", g_uc_fails);
    printf("========================================================\n");
    return (g_uc_fails == 0) ? 0 : 1;
}
