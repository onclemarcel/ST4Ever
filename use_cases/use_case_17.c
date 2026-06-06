/*
 * use_case_17.c - UC17: MSA image round-trip (RLE compression)
 *
 * Tests image_msa_save() + image_msa_load() on image_st_t handles.
 * Validates both NULL-guard robustness and end-to-end round-trip
 * fidelity including the 0xE5 escape byte.
 * All fixture files are generated at runtime in use_cases/UC17/.
 *
 * TEST MATRIX - UC17:
 *   [N] Nominal    : 15 tests - blank round-trip, files round-trip,
 *                               escape byte, compression ratio,
 *                               image_st_get_disk
 *   [R] Robustness : 10 tests - NULL params (load/save/get_disk),
 *                               nonexistent file, bad magic,
 *                               truncated, bad geometry
 *   [S] Skipped    :  0 tests
 *
 * Traceability:
 *   INT-MSA-001..033 → TC-MSA-001..033 → REQ-MSA-001..010 → UFR-DSK-005..007
 */

#include "use_cases.h"
#include "../src/image_msa.h"
#include "../src/file.h"
#include <string.h>

int g_uc_fails = 0;

#define UC17_DIR        "use_cases/UC17"
#define UC17_BLANK_MSA  UC17_DIR "/blank.msa"
#define UC17_FILES_MSA  UC17_DIR "/files.msa"
#define UC17_BAD_MAGIC  UC17_DIR "/bad_magic.msa"
#define UC17_TRUNCATED  UC17_DIR "/truncated.msa"
#define UC17_BAD_GEO    UC17_DIR "/bad_geo.msa"

/* Test file contents */
static const st_u8_t g_aHelloPrg[32] =
{
    0x60, 0x1A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x70, 0x2A, 0x4E, 0x75, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 512-byte pattern: bytes 0..255 repeated twice */
static st_u8_t g_aDataBin[512];

/* 16-byte escape fixture: all 0xE5 */
static const st_u8_t g_aEscBin[16] =
{
    0xE5, 0xE5, 0xE5, 0xE5, 0xE5, 0xE5, 0xE5, 0xE5,
    0xE5, 0xE5, 0xE5, 0xE5, 0xE5, 0xE5, 0xE5, 0xE5
};

/* 10-byte bad-magic MSA header */
static const st_u8_t g_aBadMagic[10] =
{
    0xDE, 0xAD, 0x00, 0x09, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4F
};

/* 4-byte truncated MSA (valid magic, header incomplete) */
static const st_u8_t g_aTruncated[4] =
{
    0x0E, 0x0F, 0x00, 0x09
};

/* 10-byte bad-geometry MSA header (sides=5, invalid) */
static const st_u8_t g_aBadGeo[10] =
{
    0x0E, 0x0F, 0x00, 0x09, 0x00, 0x05, 0x00, 0x00, 0x00, 0x4F
};

static void uc17_section(const char *szTitle)
{
    printf("\n--- %s ---\n", szTitle);
}

/* Write a raw byte array to a file — helper for fixture creation */
static void write_fixture(const char    *szPath,
                           const st_u8_t *pData,
                           size_t         uiLen)
{
    file_t *ptFile = NULL;

    if (file_open(szPath, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
        return;
    file_write(ptFile, pData, uiLen);
    file_close(&ptFile);
}

/* Find a named entry in the root directory (returns index or -1) */
static int find_entry(const image_st_dirent_t *aEnt,
                       int                      iCount,
                       const char              *szName)
{
    int i;

    for (i = 0; i < iCount; i++)
    {
        if (strcmp(aEnt[i].szName, szName) == 0)
            return i;
    }
    return -1;
}

/* ------------------------------------------------------------------
 * Block 1 — NULL parameter guards [R]
 * ------------------------------------------------------------------ */

static void uc17_test_null_guards(void)
{
    image_st_t *ptImg  = NULL;
    image_st_t *ptDummy = NULL;
    st_u8_t    *pDisk  = NULL;

    uc17_section("Block 1: NULL parameter guards");

    /* INTENT[INT-MSA-001 → TC-MSA-001 → REQ-MSA-001]:
     * NULL path to image_msa_load must return ST_ERROR immediately */
    UC_TEST("[R] image_msa_load(NULL,&p) → ST_ERROR",
            image_msa_load(NULL, &ptDummy) == ST_ERROR);

    /* INTENT[INT-MSA-002 → TC-MSA-002 → REQ-MSA-001]:
     * NULL pptImg to image_msa_load must return ST_ERROR */
    UC_TEST("[R] image_msa_load(x,NULL) → ST_ERROR",
            image_msa_load("x.msa", NULL) == ST_ERROR);

    /* Create a valid image for the remaining save/get_disk NULL tests */
    UC_CHECK("[R] image_st_create for NULL-guard tests",
             image_st_create(&ptImg));

    /* INTENT[INT-MSA-003 → TC-MSA-003 → REQ-MSA-002]:
     * NULL ptImg to image_msa_save must return ST_ERROR */
    UC_TEST("[R] image_msa_save(NULL,x) → ST_ERROR",
            image_msa_save(NULL, "x.msa") == ST_ERROR);

    /* INTENT[INT-MSA-004 → TC-MSA-004 → REQ-MSA-002]:
     * NULL path to image_msa_save must return ST_ERROR */
    UC_TEST("[R] image_msa_save(ptImg,NULL) → ST_ERROR",
            image_msa_save(ptImg, NULL) == ST_ERROR);

    /* INTENT[INT-MSA-005 → TC-MSA-005 → REQ-MSA-010]:
     * image_st_get_disk with NULL ptImg must return ST_ERROR */
    UC_TEST("[R] image_st_get_disk(NULL,&p) → ST_ERROR",
            image_st_get_disk(NULL, &pDisk) == ST_ERROR);

    /* INTENT[INT-MSA-006 → TC-MSA-006 → REQ-MSA-010]:
     * image_st_get_disk with NULL ppDisk must return ST_ERROR */
    UC_TEST("[R] image_st_get_disk(ptImg,NULL) → ST_ERROR",
            image_st_get_disk(ptImg, NULL) == ST_ERROR);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------
 * Block 2 — blank image round-trip [N]
 * ------------------------------------------------------------------ */

static void uc17_test_blank_roundtrip(void)
{
    image_st_t        *ptImg    = NULL;
    image_st_t        *ptLoaded = NULL;
    image_st_dirent_t  aEnt[16];
    file_stat_t        tStat;
    st_u32_t           uiFreeOrig;
    st_u32_t           uiFreeLoaded;
    int                iCount;

    uc17_section("Block 2: blank image round-trip");

    /* INTENT[INT-MSA-010 → TC-MSA-010 → REQ-MSA-003]:
     * A blank image saved as .msa then reloaded must be identical */
    UC_CHECK("[N] image_st_create for blank round-trip",
             image_st_create(&ptImg));
    image_st_free_bytes(ptImg, &uiFreeOrig);

    UC_CHECK("[N] image_msa_save blank → ST_NO_ERROR",
             image_msa_save(ptImg, UC17_BLANK_MSA));

    /* INTENT[INT-MSA-011 → TC-MSA-011 → REQ-MSA-004]:
     * MSA compression must produce a smaller file than raw .st */
    file_stat(UC17_BLANK_MSA, &tStat);
    UC_TEST("[N] blank MSA file exists", tStat.bExists == ST_TRUE);
    UC_TEST("[N] blank MSA size > header (file written)",
            tStat.uiSize > IMSA_HDR_SIZE);
    /* Blank image is mostly zeros → should compress dramatically.
     * IST_DISK_SIZE = 737280; even being generous: < 737280 / 10 */
    UC_TEST("[N] blank MSA smaller than IST_DISK_SIZE/10",
            tStat.uiSize < (st_u64_t)(IST_DISK_SIZE / 10));

    UC_CHECK("[N] image_msa_load blank → ST_NO_ERROR",
             image_msa_load(UC17_BLANK_MSA, &ptLoaded));

    iCount = -1;
    UC_TEST("[N] list_root on blank loaded → 0 entries",
            image_st_list_root(ptLoaded, aEnt, 16, &iCount)
                == ST_NO_ERROR && iCount == 0);

    image_st_free_bytes(ptLoaded, &uiFreeLoaded);
    UC_TEST("[N] free_bytes preserved after blank round-trip",
            uiFreeLoaded == uiFreeOrig);

    image_st_close(&ptImg);
    image_st_close(&ptLoaded);
}

/* ------------------------------------------------------------------
 * Block 3 — image with files round-trip [N]
 * ------------------------------------------------------------------ */

static void uc17_test_file_roundtrip(void)
{
    image_st_t        *ptImg    = NULL;
    image_st_t        *ptLoaded = NULL;
    image_st_dirent_t  aEnt[16];
    st_u8_t            aReadBuf[512];
    st_u32_t           uiFreeOrig;
    st_u32_t           uiFreeLoaded;
    int                iCount;
    int                iIdx;

    uc17_section("Block 3: image with files round-trip");

    /* INTENT[INT-MSA-020 → TC-MSA-020 → REQ-MSA-003]:
     * Files written to an image must survive a .msa save/load cycle */
    UC_CHECK("[N] image_st_create for file round-trip",
             image_st_create(&ptImg));

    UC_CHECK("[N] write HELLO.PRG (32 bytes)",
             image_st_write_file(ptImg, "HELLO.PRG", g_aHelloPrg, 32u));
    UC_CHECK("[N] write DATA.BIN (512 bytes)",
             image_st_write_file(ptImg, "DATA.BIN", g_aDataBin, 512u));
    UC_CHECK("[N] write ESC.BIN (16 bytes of 0xE5)",
             image_st_write_file(ptImg, "ESC.BIN", g_aEscBin, 16u));

    image_st_free_bytes(ptImg, &uiFreeOrig);

    UC_CHECK("[N] image_msa_save with files → ST_NO_ERROR",
             image_msa_save(ptImg, UC17_FILES_MSA));

    UC_CHECK("[N] image_msa_load with files → ST_NO_ERROR",
             image_msa_load(UC17_FILES_MSA, &ptLoaded));

    /* Verify directory */
    iCount = 0;
    UC_TEST("[N] list_root → 3 entries",
            image_st_list_root(ptLoaded, aEnt, 16, &iCount)
                == ST_NO_ERROR && iCount == 3);

    iIdx = find_entry(aEnt, iCount, "HELLO.PRG");
    UC_TEST("[N] HELLO.PRG found with size 32",
            iIdx >= 0 && aEnt[iIdx].uiSize == 32u);

    iIdx = find_entry(aEnt, iCount, "DATA.BIN");
    UC_TEST("[N] DATA.BIN found with size 512",
            iIdx >= 0 && aEnt[iIdx].uiSize == 512u);

    iIdx = find_entry(aEnt, iCount, "ESC.BIN");
    UC_TEST("[N] ESC.BIN found with size 16",
            iIdx >= 0 && aEnt[iIdx].uiSize == 16u);

    /* Verify content: HELLO.PRG */
    iIdx = find_entry(aEnt, iCount, "HELLO.PRG");
    if (iIdx >= 0)
    {
        memset(aReadBuf, 0, sizeof(aReadBuf));
        image_st_read_file(ptLoaded, aEnt[iIdx].uiCluster, 32u,
                           aReadBuf, 32u);
        UC_TEST("[N] HELLO.PRG content matches original",
                memcmp(aReadBuf, g_aHelloPrg, 32u) == 0);
    }
    else
    {
        UC_TEST("[N] HELLO.PRG content matches original", 0);
    }

    /* Verify content: DATA.BIN */
    iIdx = find_entry(aEnt, iCount, "DATA.BIN");
    if (iIdx >= 0)
    {
        memset(aReadBuf, 0, sizeof(aReadBuf));
        image_st_read_file(ptLoaded, aEnt[iIdx].uiCluster, 512u,
                           aReadBuf, 512u);
        UC_TEST("[N] DATA.BIN content matches original",
                memcmp(aReadBuf, g_aDataBin, 512u) == 0);
    }
    else
    {
        UC_TEST("[N] DATA.BIN content matches original", 0);
    }

    /* INTENT[INT-MSA-021 → TC-MSA-021 → REQ-MSA-005]:
     * Files containing 0xE5 bytes must round-trip exactly; the MSA
     * codec must escape them correctly during save and restore them
     * during load. */
    iIdx = find_entry(aEnt, iCount, "ESC.BIN");
    if (iIdx >= 0)
    {
        memset(aReadBuf, 0, sizeof(aReadBuf));
        image_st_read_file(ptLoaded, aEnt[iIdx].uiCluster, 16u,
                           aReadBuf, 16u);
        UC_TEST("[N] ESC.BIN (0xE5 bytes) content matches original",
                memcmp(aReadBuf, g_aEscBin, 16u) == 0);
    }
    else
    {
        UC_TEST("[N] ESC.BIN (0xE5 bytes) content matches original", 0);
    }

    /* free_bytes should match */
    image_st_free_bytes(ptLoaded, &uiFreeLoaded);
    UC_TEST("[N] free_bytes preserved after file round-trip",
            uiFreeLoaded == uiFreeOrig);

    image_st_close(&ptImg);
    image_st_close(&ptLoaded);
}

/* ------------------------------------------------------------------
 * Block 4 — error paths [R]
 * ------------------------------------------------------------------ */

static void uc17_test_error_paths(void)
{
    image_st_t *ptLoaded = NULL;

    uc17_section("Block 4: error paths");

    /* INTENT[INT-MSA-030 → TC-MSA-030 → REQ-MSA-001]:
     * Loading a nonexistent file must return ST_ERROR */
    ptLoaded = NULL;
    UC_TEST("[R] load nonexistent file → ST_ERROR and handle stays NULL",
            image_msa_load("use_cases/UC17/no_such.msa", &ptLoaded)
                == ST_ERROR && ptLoaded == NULL);

    /* Create bad-magic fixture */
    write_fixture(UC17_BAD_MAGIC, g_aBadMagic, sizeof(g_aBadMagic));

    /* INTENT[INT-MSA-031 → TC-MSA-031 → REQ-MSA-006]:
     * A file with wrong magic must be rejected */
    ptLoaded = NULL;
    UC_TEST("[R] load bad-magic MSA → ST_ERROR and handle stays NULL",
            image_msa_load(UC17_BAD_MAGIC, &ptLoaded) == ST_ERROR &&
            ptLoaded == NULL);

    /* Create truncated fixture (4 bytes: valid magic, header cut off) */
    write_fixture(UC17_TRUNCATED, g_aTruncated, sizeof(g_aTruncated));

    /* INTENT[INT-MSA-032 → TC-MSA-032 → REQ-MSA-006]:
     * A truncated MSA file must be rejected during header read */
    ptLoaded = NULL;
    UC_TEST("[R] load truncated MSA → ST_ERROR and handle stays NULL",
            image_msa_load(UC17_TRUNCATED, &ptLoaded) == ST_ERROR &&
            ptLoaded == NULL);

    /* Create bad-geometry fixture (sides=5, invalid) */
    write_fixture(UC17_BAD_GEO, g_aBadGeo, sizeof(g_aBadGeo));

    /* INTENT[INT-MSA-033 → TC-MSA-033 → REQ-MSA-006]:
     * A MSA file with impossible geometry must be rejected */
    ptLoaded = NULL;
    UC_TEST("[R] load bad-geometry MSA → ST_ERROR and handle stays NULL",
            image_msa_load(UC17_BAD_GEO, &ptLoaded) == ST_ERROR &&
            ptLoaded == NULL);
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    int i;

    printf("=== UC17: MSA image round-trip (image_msa) ===\n");

    /* Build g_aDataBin pattern (0..255 × 2) */
    for (i = 0; i < 512; i++)
        g_aDataBin[i] = (st_u8_t)(i & 0xFF);

    /* Create the UC17 output directory */
    file_mkdir(UC17_DIR);

    uc17_test_null_guards();
    uc17_test_blank_roundtrip();
    uc17_test_file_roundtrip();
    uc17_test_error_paths();

    printf("\n");
    if (g_uc_fails == 0)
        printf("  All tests PASSED.\n\n");
    else
        printf("  %d test(s) FAILED.\n\n", g_uc_fails);

    return (g_uc_fails == 0) ? 0 : 1;
}
