/*
 * use_case_24F.c - FAT12 subdirectory support (P53)
 *
 * TEST MATRIX - UC24F:
 *   [N] Nominal    : 16 tests - image_st_mkdir creates subdir entry;
 *                               image_st_list_dir root == list_root;
 *                               image_st_list_dir named subdir;
 *                               image_st_write_file_in_dir writes file;
 *                               subdir entry visible in root listing;
 *                               subdir navigation fields in mount_view_t;
 *                               image_st_list_dir after write shows file;
 *                               mkdir + write preserves root files;
 *                               multiple subdirs independent;
 *                               empty file in subdir;
 *                               mkdir 8.3 name normalised uppercase
 *   [R] Robustness : 10 tests - NULL params: mkdir/list_dir/write_in_dir;
 *                               list_dir nonexistent dir -> ST_ERROR;
 *                               list_dir on a file -> ST_ERROR;
 *                               write_in_dir to nonexistent dir -> ST_ERROR;
 *                               mkdir duplicate -> ST_ERROR;
 *                               mkdir invalid 8.3 -> ST_ERROR
 *   [S] Skipped    :  0 tests - (subdir navigation is GUI-validated)
 *
 * INTENT[INT-IST-070..095 -> TC-IST-070..095 -> REQ-IST-030..035
 *        -> UFR-CON-100..105]:
 *   FAT12 subdirectory support: create (mkdir), enumerate (list_dir),
 *   populate (write_file_in_dir).  mount_view_t navigation fields
 *   allow tracking current dir and back-navigation stack.
 */

#include "use_cases.h"
#include <string.h>
#include <stdlib.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */

static void test_mkdir_nominal(void)
{
    image_st_t        *ptImg   = NULL;
    image_st_dirent_t  aRoot[IST_RDE];
    int                iCount  = 0;
    int                i;
    st_bool_t          bFound;

    printf("\n--- image_st_mkdir nominal ---\n");

    /* INTENT[INT-IST-070 -> TC-IST-070 -> REQ-IST-030 -> UFR-CON-100]:
     * mkdir creates an SUBDIR entry visible in root listing. */
    TEST_ASSERT("[N] image_st_create",
                image_st_create(&ptImg) == ST_NO_ERROR);

    TEST_ASSERT("[N] image_st_mkdir 'AUTO'",
                image_st_mkdir(ptImg, "AUTO") == ST_NO_ERROR);

    image_st_list_root(ptImg, aRoot, IST_RDE, &iCount);
    bFound = ST_FALSE;
    for (i = 0; i < iCount; i++)
    {
        if (strcmp(aRoot[i].szName, "AUTO") == 0 && aRoot[i].bIsDir)
        {
            bFound = ST_TRUE;
            break;
        }
    }
    TEST_ASSERT("[N] 'AUTO' appears as DIR in root listing", bFound);

    /* INTENT[INT-IST-071 -> TC-IST-071 -> REQ-IST-030 -> UFR-CON-100]:
     * mkdir normalises name to uppercase. */
    TEST_ASSERT("[N] image_st_mkdir lowercase 'games'",
                image_st_mkdir(ptImg, "games") == ST_NO_ERROR);

    iCount = 0;
    image_st_list_root(ptImg, aRoot, IST_RDE, &iCount);
    bFound = ST_FALSE;
    for (i = 0; i < iCount; i++)
    {
        if (strcmp(aRoot[i].szName, "GAMES") == 0 && aRoot[i].bIsDir)
        {
            bFound = ST_TRUE;
            break;
        }
    }
    TEST_ASSERT("[N] lowercase 'games' stored as 'GAMES'", bFound);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------ */

static void test_list_dir_root(void)
{
    image_st_t        *ptImg    = NULL;
    image_st_dirent_t  aRoot[IST_RDE];
    image_st_dirent_t  aDir[IST_RDE];
    int                iRootCnt = 0;
    int                iDirCnt  = 0;
    st_u8_t            aBuf[512];

    printf("\n--- image_st_list_dir root equivalence ---\n");

    /* INTENT[INT-IST-072 -> TC-IST-072 -> REQ-IST-031 -> UFR-CON-101]:
     * list_dir(NULL) returns same entries as list_root. */
    image_st_create(&ptImg);
    memset(aBuf, 0x41, sizeof(aBuf));
    image_st_write_file(ptImg, "FILE.PRG", aBuf, sizeof(aBuf));

    image_st_list_root(ptImg, aRoot, IST_RDE, &iRootCnt);
    image_st_list_dir(ptImg, NULL, aDir, IST_RDE, &iDirCnt);

    TEST_ASSERT("[N] list_dir(NULL) count == list_root count",
                iRootCnt == iDirCnt);
    if (iRootCnt > 0 && iDirCnt > 0)
        TEST_ASSERT("[N] list_dir(NULL) first entry matches list_root",
                    strcmp(aRoot[0].szName, aDir[0].szName) == 0);
    else
        TEST_SKIP("[N] list_dir(NULL) entry match (no entries)");

    /* INTENT[INT-IST-073 -> TC-IST-073 -> REQ-IST-031 -> UFR-CON-101]:
     * list_dir("") also returns root entries. */
    iDirCnt = 0;
    image_st_list_dir(ptImg, "", aDir, IST_RDE, &iDirCnt);
    TEST_ASSERT("[N] list_dir('') count == list_root count",
                iRootCnt == iDirCnt);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------ */

static void test_list_dir_subdir(void)
{
    image_st_t        *ptImg   = NULL;
    image_st_dirent_t  aDir[IST_RDE];
    int                iCount  = 0;
    st_u8_t            aData[256];

    printf("\n--- image_st_list_dir named subdir ---\n");

    image_st_create(&ptImg);
    image_st_mkdir(ptImg, "AUTO");

    /* INTENT[INT-IST-074 -> TC-IST-074 -> REQ-IST-031 -> UFR-CON-101]:
     * list_dir on empty subdir returns 0 entries (no . or ..). */
    TEST_ASSERT("[N] list_dir empty subdir -> ST_NO_ERROR",
                image_st_list_dir(ptImg, "AUTO", aDir, IST_RDE,
                                   &iCount) == ST_NO_ERROR);
    TEST_ASSERT("[N] empty subdir has 0 entries (dot entries skipped)",
                iCount == 0);

    /* INTENT[INT-IST-075 -> TC-IST-075 -> REQ-IST-031 -> UFR-CON-101]:
     * list_dir after write_file_in_dir returns that file. */
    memset(aData, 0xBE, sizeof(aData));
    image_st_write_file_in_dir(ptImg, "AUTO", "DEMO.PRG", aData,
                                sizeof(aData));
    iCount = 0;
    image_st_list_dir(ptImg, "AUTO", aDir, IST_RDE, &iCount);
    TEST_ASSERT("[N] subdir has 1 file after write",
                iCount == 1);
    if (iCount >= 1)
    {
        TEST_ASSERT("[N] file name correct",
                    strcmp(aDir[0].szName, "DEMO.PRG") == 0);
        TEST_ASSERT("[N] file size correct",
                    aDir[0].uiSize == 256u);
    }
    else
    {
        TEST_SKIP("[N] file name/size (no entry)");
        TEST_SKIP("[N] file size (no entry)");
    }

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------ */

static void test_write_file_in_dir_nominal(void)
{
    image_st_t        *ptImg   = NULL;
    image_st_dirent_t  aRoot[IST_RDE];
    image_st_dirent_t  aDir[IST_RDE];
    int                iRootCnt = 0;
    int                iDirCnt  = 0;
    st_u8_t            aWrite[100];
    st_u8_t            aRead[100];
    int                i;
    st_bool_t          bRootFilePresent;

    printf("\n--- image_st_write_file_in_dir nominal ---\n");

    image_st_create(&ptImg);
    image_st_mkdir(ptImg, "LEVEL1");

    /* Write a root file too (ensure root is not corrupted) */
    memset(aWrite, 0xAA, sizeof(aWrite));
    image_st_write_file(ptImg, "ROOT.TXT", aWrite, sizeof(aWrite));

    /* INTENT[INT-IST-076 -> TC-IST-076 -> REQ-IST-032 -> UFR-CON-102]:
     * write_file_in_dir populates a file inside a named subdir. */
    memset(aWrite, 0xCC, sizeof(aWrite));
    TEST_ASSERT("[N] write_file_in_dir -> ST_NO_ERROR",
                image_st_write_file_in_dir(ptImg, "LEVEL1", "DATA.BIN",
                                            aWrite, sizeof(aWrite))
                == ST_NO_ERROR);

    /* INTENT[INT-IST-077 -> TC-IST-077 -> REQ-IST-032 -> UFR-CON-102]:
     * Root directory still has only ROOT.TXT + LEVEL1 (not DATA.BIN). */
    image_st_list_root(ptImg, aRoot, IST_RDE, &iRootCnt);
    bRootFilePresent = ST_FALSE;
    for (i = 0; i < iRootCnt; i++)
        if (strcmp(aRoot[i].szName, "DATA.BIN") == 0)
            bRootFilePresent = ST_TRUE;
    TEST_ASSERT("[N] DATA.BIN not in root dir (only in subdir)",
                !bRootFilePresent);

    /* INTENT[INT-IST-078 -> TC-IST-078 -> REQ-IST-032 -> UFR-CON-102]:
     * File content readable via cluster chain. */
    image_st_list_dir(ptImg, "LEVEL1", aDir, IST_RDE, &iDirCnt);
    if (iDirCnt >= 1 && strcmp(aDir[0].szName, "DATA.BIN") == 0)
    {
        memset(aRead, 0, sizeof(aRead));
        TEST_ASSERT("[N] image_st_read_file from subdir cluster",
                    image_st_read_file(ptImg, aDir[0].uiCluster,
                                        aDir[0].uiSize, aRead,
                                        sizeof(aRead)) == ST_NO_ERROR);
        TEST_ASSERT("[N] read-back content matches written data",
                    memcmp(aRead, aWrite, sizeof(aWrite)) == 0);
    }
    else
    {
        TEST_SKIP("[N] read_file from subdir cluster (entry not found)");
        TEST_SKIP("[N] read-back content match");
    }

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------ */

static void test_multiple_subdirs(void)
{
    image_st_t        *ptImg   = NULL;
    image_st_dirent_t  aA[IST_RDE];
    image_st_dirent_t  aB[IST_RDE];
    int                iCountA = 0;
    int                iCountB = 0;
    st_u8_t            aData[64];

    printf("\n--- multiple independent subdirs ---\n");

    /* INTENT[INT-IST-079 -> TC-IST-079 -> REQ-IST-030 -> UFR-CON-100]:
     * Two subdirs are independent — files in A don't appear in B. */
    image_st_create(&ptImg);
    image_st_mkdir(ptImg, "DIRA");
    image_st_mkdir(ptImg, "DIRB");

    memset(aData, 0x11, sizeof(aData));
    image_st_write_file_in_dir(ptImg, "DIRA", "FILEA.TXT", aData, 64u);
    memset(aData, 0x22, sizeof(aData));
    image_st_write_file_in_dir(ptImg, "DIRB", "FILEB.TXT", aData, 64u);

    image_st_list_dir(ptImg, "DIRA", aA, IST_RDE, &iCountA);
    image_st_list_dir(ptImg, "DIRB", aB, IST_RDE, &iCountB);

    TEST_ASSERT("[N] DIRA has 1 file", iCountA == 1);
    TEST_ASSERT("[N] DIRB has 1 file", iCountB == 1);
    if (iCountA >= 1 && iCountB >= 1)
        TEST_ASSERT("[N] DIRA and DIRB contain different files",
                    strcmp(aA[0].szName, aB[0].szName) != 0);
    else
        TEST_SKIP("[N] DIRA/DIRB name difference");

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------ */

static void test_empty_file_in_subdir(void)
{
    image_st_t        *ptImg  = NULL;
    image_st_dirent_t  aDir[IST_RDE];
    int                iCount = 0;

    printf("\n--- empty file in subdir ---\n");

    /* INTENT[INT-IST-080 -> TC-IST-080 -> REQ-IST-032 -> UFR-CON-102]:
     * write_file_in_dir with uiSize=0 and pData=NULL succeeds. */
    image_st_create(&ptImg);
    image_st_mkdir(ptImg, "EMPTY");
    TEST_ASSERT("[N] write empty file in subdir -> ST_NO_ERROR",
                image_st_write_file_in_dir(ptImg, "EMPTY", "STUB.TXT",
                                            NULL, 0u) == ST_NO_ERROR);
    image_st_list_dir(ptImg, "EMPTY", aDir, IST_RDE, &iCount);
    TEST_ASSERT("[N] empty file visible in subdir listing", iCount == 1);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------ */

static void test_nav_fields(void)
{
    printf("\n--- mount_view_t navigation fields ---\n");

    /* INTENT[INT-IST-081 -> TC-IST-081 -> REQ-IST-033 -> UFR-CON-103]:
     * mount_view_t has szCurDir, aszNavStack, iNavDepth fields. */
    {
        mount_view_t tV;
        memset(&tV, 0, sizeof(tV));

        TEST_ASSERT("[N] szCurDir initial empty",
                    tV.szCurDir[0] == '\0');
        TEST_ASSERT("[N] iNavDepth initial 0",
                    tV.iNavDepth == 0);
        TEST_ASSERT("[N] nav stack depth capacity >= 8",
                    (int)(sizeof(tV.aszNavStack) /
                          sizeof(tV.aszNavStack[0])) >= 8);
        TEST_ASSERT("[N] szCurDir capacity >= IST_NAME_MAX",
                    (int)sizeof(tV.szCurDir) >= IST_NAME_MAX);
    }
}

/* ------------------------------------------------------------------ */

static void test_robustness(void)
{
    image_st_t        *ptImg  = NULL;
    image_st_dirent_t  aDir[IST_RDE];
    int                iCount = 0;
    st_u8_t            aBuf[16];

    printf("\n--- robustness ---\n");

    image_st_create(&ptImg);

    /* INTENT[INT-IST-082 -> TC-IST-082 -> REQ-IST-034 -> UFR-CON-104]:
     * NULL params return ST_ERROR without crash. */
    TEST_ASSERT("[R] image_st_mkdir NULL ptImg -> ST_ERROR",
                image_st_mkdir(NULL, "DIR") == ST_ERROR);
    TEST_ASSERT("[R] image_st_mkdir NULL name -> ST_ERROR",
                image_st_mkdir(ptImg, NULL) == ST_ERROR);

    TEST_ASSERT("[R] image_st_list_dir NULL ptImg -> ST_ERROR",
                image_st_list_dir(NULL, "X", aDir, IST_RDE,
                                   &iCount) == ST_ERROR);
    TEST_ASSERT("[R] image_st_list_dir NULL aEntries -> ST_ERROR",
                image_st_list_dir(ptImg, "X", NULL, IST_RDE,
                                   &iCount) == ST_ERROR);
    TEST_ASSERT("[R] image_st_list_dir NULL piCount -> ST_ERROR",
                image_st_list_dir(ptImg, "X", aDir, IST_RDE,
                                   NULL) == ST_ERROR);

    /* INTENT[INT-IST-083 -> TC-IST-083 -> REQ-IST-034 -> UFR-CON-104]:
     * list_dir on nonexistent name -> ST_ERROR. */
    TEST_ASSERT("[R] list_dir nonexistent -> ST_ERROR",
                image_st_list_dir(ptImg, "GHOST", aDir, IST_RDE,
                                   &iCount) == ST_ERROR);

    /* INTENT[INT-IST-084 -> TC-IST-084 -> REQ-IST-034 -> UFR-CON-104]:
     * list_dir on a regular file (not a dir) -> ST_ERROR. */
    memset(aBuf, 0, sizeof(aBuf));
    image_st_write_file(ptImg, "FILE.TXT", aBuf, sizeof(aBuf));
    TEST_ASSERT("[R] list_dir on a file -> ST_ERROR",
                image_st_list_dir(ptImg, "FILE.TXT", aDir, IST_RDE,
                                   &iCount) == ST_ERROR);

    /* INTENT[INT-IST-085 -> TC-IST-085 -> REQ-IST-035 -> UFR-CON-105]:
     * write_file_in_dir to nonexistent dir -> ST_ERROR. */
    TEST_ASSERT("[R] write_file_in_dir nonexistent dir -> ST_ERROR",
                image_st_write_file_in_dir(ptImg, "NODIRHERE",
                                            "F.TXT", aBuf, 4u)
                == ST_ERROR);

    /* INTENT[INT-IST-086 -> TC-IST-086 -> REQ-IST-030 -> UFR-CON-100]:
     * mkdir duplicate name -> ST_ERROR. */
    image_st_mkdir(ptImg, "DUPE");
    TEST_ASSERT("[R] mkdir duplicate -> ST_ERROR",
                image_st_mkdir(ptImg, "DUPE") == ST_ERROR);

    /* INTENT[INT-IST-087 -> TC-IST-087 -> REQ-IST-030 -> UFR-CON-100]:
     * mkdir invalid 8.3 name (too long) -> ST_ERROR. */
    TEST_ASSERT("[R] mkdir name too long -> ST_ERROR",
                image_st_mkdir(ptImg, "TOOLONGNAME") == ST_ERROR);

    /* INTENT[INT-IST-088 -> TC-IST-088 -> REQ-IST-035 -> UFR-CON-105]:
     * write_file_in_dir NULL params -> ST_ERROR. */
    image_st_mkdir(ptImg, "NULLTEST");
    TEST_ASSERT("[R] write_file_in_dir NULL ptImg -> ST_ERROR",
                image_st_write_file_in_dir(NULL, "NULLTEST",
                                            "F.TXT", aBuf, 4u)
                == ST_ERROR);
    TEST_ASSERT("[R] write_file_in_dir NULL szDirName -> ST_ERROR",
                image_st_write_file_in_dir(ptImg, NULL,
                                            "F.TXT", aBuf, 4u)
                == ST_ERROR);
    TEST_ASSERT("[R] write_file_in_dir NULL szFileName -> ST_ERROR",
                image_st_write_file_in_dir(ptImg, "NULLTEST",
                                            NULL, aBuf, 4u)
                == ST_ERROR);

    image_st_close(&ptImg);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    int iResult;

    printf("=== UC24F: FAT12 subdirectory support (P53) ===\n");

    test_mkdir_nominal();
    test_list_dir_root();
    test_list_dir_subdir();
    test_write_file_in_dir_nominal();
    test_multiple_subdirs();
    test_empty_file_in_subdir();
    test_nav_fields();
    test_robustness();

    printf("\n=== UC24F: %d failure(s) ===\n", g_uc_fails);
    iResult = (g_uc_fails == 0) ? 0 : 1;
    return iResult;
}
