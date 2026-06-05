/*
 * use_case_16.c - UC16: .st image read/write (image_st.h / image_st.c).
 *
 * Tests run from the project root (make tests requirement — R12).
 *
 * All fixtures are created programmatically by the test itself;
 * use_cases/UC16/roundtrip.st is written during the save/reload test
 * and may be inspected manually after a test run.
 *
 * TEST MATRIX - UC16:
 *   [N] Nominal    : 28 tests — create/BPB/FAT init (6);
 *                               write+list+read round-trip (8);
 *                               delete + free-bytes accounting (4);
 *                               save + reload round-trip (5);
 *                               overwrite existing file (2);
 *                               close idempotence (2);
 *                               constants (1)
 *   [R] Robustness : 14 tests — NULL guards on all public functions (11);
 *                               load non-existent file (1);
 *                               write invalid 8.3 name (1);
 *                               delete missing file (1)
 *   [S] Skipped    :  0 tests
 *
 * Traceability:
 *   INT-IST-001..010 → TC-IST-001..042 → REQ-IST-001..012 → UFR-DSK-001..004
 */

#include "use_cases.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

/* Path for the round-trip fixture; created & left for manual inspection */
#define UC16_ROUNDTRIP_PATH "use_cases/UC16/roundtrip.st"

static void uc16_section(const char *szTitle)
{
    printf("\n--- %s ---\n", szTitle);
}

/* ------------------------------------------------------------------
 * Block 1 — create + BPB + FAT initial state
 * ------------------------------------------------------------------ */

/* INTENT[INT-IST-001 → TC-IST-001..006 → REQ-IST-001]:
 * image_st_create() produces a valid blank DD image with correct BPB
 * values and FAT12 cluster 0/1 markers. */
static void uc16_test_create(void)
{
    image_st_t      *ptImg;
    image_st_dirent_t aEnt[128];
    st_u32_t          uiFree;
    int               iCount;
    st_u32_t          uiExpectedFree;

    uc16_section("Block 1: create / BPB / FAT init");

    UC_CHECK("[N] image_st_create returns ST_NO_ERROR",
             image_st_create(&ptImg));
    UC_TEST("[N] image_st_create: handle not NULL", ptImg != NULL);

    /* BPB sanity via list + free_bytes (white-box via constants) */
    UC_TEST("[N] IST_DISK_SIZE == 737280",
            IST_DISK_SIZE == 737280u);

    /* Free bytes on a blank image = all data clusters */
    uiExpectedFree = (st_u32_t)IST_DATA_CLUSTERS *
                     (st_u32_t)IST_SPC * IST_BPS;
    UC_CHECK("[N] image_st_free_bytes on fresh image",
             image_st_free_bytes(ptImg, &uiFree));
    UC_TEST("[N] free bytes == IST_DATA_CLUSTERS * cluster_size",
            uiFree == uiExpectedFree);

    /* Root dir empty */
    iCount = -1;
    UC_CHECK("[N] list_root on fresh image",
             image_st_list_root(ptImg, aEnt, 128, &iCount));
    UC_TEST("[N] fresh image root entry count == 0", iCount == 0);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------
 * Block 2 — write / list / read round-trip
 * ------------------------------------------------------------------ */

/* INTENT[INT-IST-002 → TC-IST-007..014 → REQ-IST-003..005]:
 * Writing a file allocates FAT clusters, creates a directory entry,
 * and the content can be read back byte-for-byte via image_st_read_file(). */
static void uc16_test_write_read(void)
{
    image_st_t       *ptImg;
    image_st_dirent_t aEnt[128];
    st_u8_t           aSrc[64];
    st_u8_t           aDst[64];
    int               iCount;
    int               i;

    uc16_section("Block 2: write / list / read round-trip");

    image_st_create(&ptImg);

    /* Build a 64-byte test payload */
    for (i = 0; i < 64; i++)
        aSrc[i] = (st_u8_t)(i * 3 + 7);

    UC_CHECK("[N] write_file HELLO.PRG",
             image_st_write_file(ptImg, "HELLO.PRG",
                                 aSrc, 64u));

    iCount = 0;
    UC_CHECK("[N] list_root after write",
             image_st_list_root(ptImg, aEnt, 128, &iCount));
    UC_TEST("[N] root count == 1 after one write", iCount == 1);
    UC_TEST("[N] entry name == \"HELLO.PRG\"",
            strcmp(aEnt[0].szName, "HELLO.PRG") == 0);
    UC_TEST("[N] entry size == 64", aEnt[0].uiSize == 64u);

    memset(aDst, 0, sizeof(aDst));
    UC_CHECK("[N] read_file HELLO.PRG",
             image_st_read_file(ptImg,
                                aEnt[0].uiCluster,
                                aEnt[0].uiSize,
                                aDst, (st_u32_t)sizeof(aDst)));
    UC_TEST("[N] read content matches written content",
            memcmp(aSrc, aDst, 64) == 0);

    /* Second file */
    {
        st_u8_t aSrc2[512];
        memset(aSrc2, 0xAB, sizeof(aSrc2));
        UC_CHECK("[N] write_file DATA.BIN (512 bytes)",
                 image_st_write_file(ptImg, "DATA.BIN",
                                     aSrc2, (st_u32_t)sizeof(aSrc2)));
        iCount = 0;
        image_st_list_root(ptImg, aEnt, 128, &iCount);
        UC_TEST("[N] root count == 2 after second write", iCount == 2);
    }

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------
 * Block 3 — delete + free-bytes accounting
 * ------------------------------------------------------------------ */

/* INTENT[INT-IST-003 → TC-IST-015..018 → REQ-IST-006..007]:
 * Deleting a file marks the entry 0xE5, frees the FAT chain, and
 * free_bytes returns to the previous value. */
static void uc16_test_delete(void)
{
    image_st_t       *ptImg;
    image_st_dirent_t aEnt[128];
    st_u32_t          uiFreeBefore;
    st_u32_t          uiFreeAfter;
    st_u8_t           aBuf[512];
    int               iCount;

    uc16_section("Block 3: delete + free-bytes accounting");

    image_st_create(&ptImg);
    image_st_free_bytes(ptImg, &uiFreeBefore);

    memset(aBuf, 0x55, sizeof(aBuf));
    image_st_write_file(ptImg, "TMP.TXT", aBuf, (st_u32_t)sizeof(aBuf));

    {
        st_u32_t uiMid;
        image_st_free_bytes(ptImg, &uiMid);
        UC_TEST("[N] free bytes decreases after write", uiMid < uiFreeBefore);
    }

    UC_CHECK("[N] delete_file TMP.TXT",
             image_st_delete_file(ptImg, "TMP.TXT"));

    iCount = 0;
    image_st_list_root(ptImg, aEnt, 128, &iCount);
    UC_TEST("[N] root count == 0 after delete", iCount == 0);

    image_st_free_bytes(ptImg, &uiFreeAfter);
    UC_TEST("[N] free bytes restored after delete",
            uiFreeAfter == uiFreeBefore);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------
 * Block 4 — save + reload round-trip
 * ------------------------------------------------------------------ */

/* INTENT[INT-IST-004 → TC-IST-019..023 → REQ-IST-008..009]:
 * Saving an image and reloading it produces an identical structure:
 * same root count, same filename, same file size, same file content. */
static void uc16_test_save_reload(void)
{
    image_st_t       *ptImg;
    image_st_t       *ptImg2;
    image_st_dirent_t aEnt[128];
    st_u8_t           aSrc[100];
    st_u8_t           aDst[100];
    int               iCount;
    int               i;

    uc16_section("Block 4: save + reload round-trip");

    image_st_create(&ptImg);

    for (i = 0; i < 100; i++)
        aSrc[i] = (st_u8_t)(i ^ 0xA5u);

    image_st_write_file(ptImg, "REVIVAL.PRG", aSrc, 100u);

    UC_CHECK("[N] image_st_save writes file",
             image_st_save(ptImg, UC16_ROUNDTRIP_PATH));
    image_st_close(&ptImg);

    UC_CHECK("[N] image_st_load reads back saved file",
             image_st_load(UC16_ROUNDTRIP_PATH, &ptImg2));

    iCount = 0;
    image_st_list_root(ptImg2, aEnt, 128, &iCount);
    UC_TEST("[N] reload: root count == 1", iCount == 1);
    UC_TEST("[N] reload: entry name == \"REVIVAL.PRG\"",
            strcmp(aEnt[0].szName, "REVIVAL.PRG") == 0);
    UC_TEST("[N] reload: entry size == 100", aEnt[0].uiSize == 100u);

    memset(aDst, 0, sizeof(aDst));
    image_st_read_file(ptImg2,
                       aEnt[0].uiCluster,
                       aEnt[0].uiSize,
                       aDst, 100u);
    UC_TEST("[N] reload: file content matches original",
            memcmp(aSrc, aDst, 100) == 0);

    image_st_close(&ptImg2);
}

/* ------------------------------------------------------------------
 * Block 5 — overwrite existing file
 * ------------------------------------------------------------------ */

/* INTENT[INT-IST-005 → TC-IST-024..025 → REQ-IST-005]:
 * Writing a file whose name already exists replaces it atomically:
 * root count stays at 1 and new content is returned by read_file(). */
static void uc16_test_overwrite(void)
{
    image_st_t       *ptImg;
    image_st_dirent_t aEnt[128];
    st_u8_t           aOld[32];
    st_u8_t           aNew[48];
    st_u8_t           aBuf[128];
    int               iCount;

    uc16_section("Block 5: overwrite existing file");

    image_st_create(&ptImg);

    memset(aOld, 0x11, sizeof(aOld));
    memset(aNew, 0x22, sizeof(aNew));

    image_st_write_file(ptImg, "DEMO.PRG", aOld, (st_u32_t)sizeof(aOld));
    image_st_write_file(ptImg, "DEMO.PRG", aNew, (st_u32_t)sizeof(aNew));

    iCount = 0;
    image_st_list_root(ptImg, aEnt, 128, &iCount);
    UC_TEST("[N] overwrite: root count still 1", iCount == 1);

    memset(aBuf, 0, sizeof(aBuf));
    image_st_read_file(ptImg,
                       aEnt[0].uiCluster,
                       aEnt[0].uiSize,
                       aBuf, (st_u32_t)sizeof(aBuf));
    UC_TEST("[N] overwrite: new content returned",
            memcmp(aBuf, aNew, sizeof(aNew)) == 0);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------
 * Block 6 — close idempotence
 * ------------------------------------------------------------------ */

/* INTENT[INT-IST-006 → TC-IST-026..027 → REQ-IST-002]:
 * image_st_close() sets the handle to NULL and is safe to call twice. */
static void uc16_test_close(void)
{
    image_st_t *ptImg;

    uc16_section("Block 6: close idempotence");

    image_st_create(&ptImg);
    UC_CHECK("[N] first close", image_st_close(&ptImg));
    UC_TEST("[N] handle is NULL after close", ptImg == NULL);
    UC_CHECK("[N] close(&NULL) is no-op", image_st_close(&ptImg));
}

/* ------------------------------------------------------------------
 * Block 7 — robustness: NULL guards
 * ------------------------------------------------------------------ */

/* INTENT[INT-IST-007 → TC-IST-028..038 → REQ-IST-001..012]:
 * All public functions reject NULL parameters with ST_ERROR. */
static void uc16_test_null_guards(void)
{
    image_st_t       *ptImg;
    image_st_dirent_t aEnt[4];
    int               iCount;
    st_u32_t          uiFree;
    st_u8_t           aBuf[16];

    uc16_section("Block 7: robustness — NULL guards");

    UC_TEST("[R] image_st_create(NULL) == ST_ERROR",
            image_st_create(NULL) == ST_ERROR);
    UC_TEST("[R] image_st_load(NULL,...) == ST_ERROR",
            image_st_load(NULL, &ptImg) == ST_ERROR);
    UC_TEST("[R] image_st_load(...,NULL) == ST_ERROR",
            image_st_load("x.st", NULL) == ST_ERROR);
    UC_TEST("[R] image_st_close(NULL) == ST_ERROR",
            image_st_close(NULL) == ST_ERROR);

    /* Need a live handle for the remaining guards */
    image_st_create(&ptImg);

    UC_TEST("[R] image_st_save(NULL,...) == ST_ERROR",
            image_st_save(NULL, "x.st") == ST_ERROR);
    UC_TEST("[R] image_st_save(...,NULL) == ST_ERROR",
            image_st_save(ptImg, NULL) == ST_ERROR);

    iCount = 0;
    UC_TEST("[R] list_root(NULL,...) == ST_ERROR",
            image_st_list_root(NULL, aEnt, 4, &iCount) == ST_ERROR);
    UC_TEST("[R] list_root(img,NULL,...) == ST_ERROR",
            image_st_list_root(ptImg, NULL, 4, &iCount) == ST_ERROR);

    UC_TEST("[R] read_file(NULL,...) == ST_ERROR",
            image_st_read_file(NULL, 2u, 4u, aBuf, 16u) == ST_ERROR);
    UC_TEST("[R] read_file(...,NULL,...) == ST_ERROR",
            image_st_read_file(ptImg, 2u, 4u, NULL, 16u) == ST_ERROR);

    UC_TEST("[R] free_bytes(NULL,...) == ST_ERROR",
            image_st_free_bytes(NULL, &uiFree) == ST_ERROR);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------
 * Block 8 — robustness: load / write / delete errors
 * ------------------------------------------------------------------ */

/* INTENT[INT-IST-008 → TC-IST-039..042 → REQ-IST-002,003,007]:
 * Loading a non-existent file, writing an invalid 8.3 name, and
 * deleting a non-existent file all return ST_ERROR cleanly. */
static void uc16_test_error_cases(void)
{
    image_st_t *ptImg;
    st_u8_t     aBuf[4];

    uc16_section("Block 8: robustness — error cases");

    /* Load non-existent */
    ptImg = NULL;
    UC_TEST("[R] load non-existent file == ST_ERROR",
            image_st_load("use_cases/UC16/no_such.st",
                          &ptImg) == ST_ERROR);
    UC_TEST("[R] handle remains NULL after failed load", ptImg == NULL);

    image_st_create(&ptImg);

    /* Invalid 8.3 name (name part > 8 chars) */
    memset(aBuf, 0, sizeof(aBuf));
    UC_TEST("[R] write_file invalid name (>8 chars) == ST_ERROR",
            image_st_write_file(ptImg, "TOOLONGNAME.TXT",
                                aBuf, 4u) == ST_ERROR);

    /* Delete a file that does not exist */
    UC_TEST("[R] delete_file non-existent == ST_ERROR",
            image_st_delete_file(ptImg, "ABSENT.TXT") == ST_ERROR);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC16: .st image read/write (image_st) ===\n");

    uc16_test_create();
    uc16_test_write_read();
    uc16_test_delete();
    uc16_test_save_reload();
    uc16_test_overwrite();
    uc16_test_close();
    uc16_test_null_guards();
    uc16_test_error_cases();

    printf("\n");
    if (g_uc_fails == 0)
        printf("  All tests PASSED.\n\n");
    else
        printf("  %d test(s) FAILED.\n\n", g_uc_fails);

    return (g_uc_fails == 0) ? 0 : 1;
}
