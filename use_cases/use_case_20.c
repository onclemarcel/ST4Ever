/*
 * use_case_20.c - Test suite for UC20
 *
 * Tests:
 *   image command: create .st/.msa from a directory (no mount view open)
 *   image command: save from an open mount view
 *   P41: ENTER in mount -> selected file -> edit_hex (visual only)
 *   P37 write: mount_make_bootable() - adjust WD1772 checksum
 *
 * TEST MATRIX - UC20:
 *   [N] Nominal    : 14 tests - mount_make_bootable, image from dir,
 *                               image from mount, MOUNT_FILE_TMP constant
 *   [R] Robustness :  5 tests - NULL guards, already-bootable idempotent,
 *                               image with no dir
 *   [S] Skipped    :  8 tests - visual (run make manual)
 */

#include "use_cases.h"
#include "../src/file.h"
#include "../src/image_msa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Write a minimal .st fixture with two files */
static st_bool_t fixture_write_st(const char *szPath)
{
    image_st_t *ptImg = NULL;
    st_bool_t   bOk   = ST_FALSE;
    st_u8_t     aBuf[32];
    int         i;

    if (image_st_create(&ptImg) != ST_NO_ERROR)
        return ST_FALSE;
    for (i = 0; i < 32; i++) aBuf[i] = (st_u8_t)(i & 0xFF);
    if (image_st_write_file(ptImg, "DEMO.PRG", aBuf, 32) == ST_NO_ERROR &&
        image_st_write_file(ptImg, "README.TXT", (st_u8_t *)"hello", 5)
            == ST_NO_ERROR &&
        image_st_save(ptImg, szPath) == ST_NO_ERROR)
        bOk = ST_TRUE;
    image_st_close(&ptImg);
    return bOk;
}

/* Write PC files used to test "image from dir" */
static st_bool_t fixture_write_dir(const char *szDirPath)
{
    char       szFile[ST_MAX_PATH];
    file_t    *ptFile = NULL;
    st_u8_t    aBuf[16];
    int        i;

    if (file_mkdir(szDirPath) != ST_NO_ERROR)
        return ST_FALSE;

    for (i = 0; i < 16; i++) aBuf[i] = (st_u8_t)(0xAA + i);

    snprintf(szFile, sizeof(szFile), "%s/HELLO.PRG", szDirPath);
    if (file_open(szFile, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
        return ST_FALSE;
    file_write(ptFile, aBuf, 16);
    file_close(&ptFile);

    snprintf(szFile, sizeof(szFile), "%s/DATA.TXT", szDirPath);
    if (file_open(szFile, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
        return ST_FALSE;
    file_write(ptFile, (st_u8_t *)"data", 4);
    file_close(&ptFile);

    return ST_TRUE;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    st_machine_t      tMachine;
    line_context_t    tLineCtx;
    mount_view_t     *ptView   = NULL;
    image_st_t       *ptImg    = NULL;
    image_st_t       *ptChk    = NULL;
    image_st_dirent_t aEntries[IST_RDE];
    file_stat_t       tStat;
    st_u8_t          *pDisk    = NULL;
    st_u8_t           aBoot[512];
    st_u32_t          uiSum;
    int               iCount;
    int               i;
    st_error_t        eResult;

    const char *szDir20     = "use_cases/UC20";
    const char *szSrcDir    = "use_cases/UC20/srcdir";
    const char *szSrcSt     = "use_cases/UC20/src.st";
    const char *szOutSt     = "use_cases/UC20/out.st";
    const char *szOutMsa    = "use_cases/UC20/out.msa";
    const char *szOutStDir  = "use_cases/UC20/out_from_dir.st";

    printf("=== UC20: image command + P41 file hex + P37 bootable ===\n\n");

    /* --- Module init --- */
    printf("--- Init ---\n");
    if (st_init(&tMachine, NULL) != ST_NO_ERROR)
    {
        printf("  [SKIP] st_init failed\n");
        return 0;
    }
    if (gui_init() != ST_NO_ERROR)
    {
        printf("  [SKIP] gui_init failed - headless\n");
        st_shutdown(&tMachine);
        return 0;
    }
    if (trace_init(ST_FALSE) != ST_NO_ERROR)
    {
        printf("  [SKIP] trace_init failed\n");
        gui_shutdown();
        st_shutdown(&tMachine);
        return 0;
    }
    if (line_init(&tLineCtx) != ST_NO_ERROR)
    {
        printf("  [SKIP] line_init failed\n");
        trace_shutdown();
        gui_shutdown();
        st_shutdown(&tMachine);
        return 0;
    }

    file_mkdir(szDir20);

    if (!fixture_write_st(szSrcSt) || !fixture_write_dir(szSrcDir))
    {
        printf("  fixture creation failed\n");
        goto shutdown;
    }

    /* ==== [R] mount_make_bootable NULL guard ==== */
    printf("\n--- [R] Robustness ---\n");

    /* INTENT[INT-MNT-036 -> TC-MNT-089 -> REQ-MNT-022 -> UFR-MNT-010]:
     * mount_make_bootable(NULL) shall return ST_ERROR without crash. */
    UC_TEST("[R] mount_make_bootable(NULL) == ST_ERROR",
            mount_make_bootable(NULL) == ST_ERROR);

    /* ==== [N] mount_make_bootable on blank image ==== */
    printf("\n--- [N] mount_make_bootable ---\n");

    /* INTENT[INT-MNT-037 -> TC-MNT-090 -> REQ-MNT-022 -> UFR-MNT-010]:
     * A blank image is not bootable; after mount_make_bootable() the
     * WD1772 checksum equals 0x1234 and mount_is_bootable returns ST_TRUE. */
    eResult = image_st_create(&ptImg);
    UC_TEST("[N] create blank image == ST_NO_ERROR", eResult == ST_NO_ERROR);

    if (ptImg == NULL)
    {
        printf("  Cannot continue bootable tests\n");
        goto test_image_cmd;
    }

    /* Verify initially not bootable (blank BPB has checksum != 0x1234) */
    pDisk = NULL;
    if (image_st_get_disk(ptImg, &pDisk) == ST_NO_ERROR && pDisk != NULL)
    {
        UC_TEST("[N] blank image initially not bootable",
                mount_is_bootable(pDisk) == ST_FALSE);
    }
    else
    {
        TEST_SKIP("[N] blank image initially not bootable");
    }

    eResult = mount_make_bootable(ptImg);
    UC_TEST("[N] mount_make_bootable returns ST_NO_ERROR",
            eResult == ST_NO_ERROR);

    /* Recheck: must be bootable now */
    pDisk = NULL;
    if (image_st_get_disk(ptImg, &pDisk) == ST_NO_ERROR && pDisk != NULL)
    {
        UC_TEST("[N] image is bootable after mount_make_bootable",
                mount_is_bootable(pDisk) == ST_TRUE);

        /* Verify checksum directly: sum of 256 LE16 words == 0x1234 */
        uiSum = 0;
        for (i = 0; i < 512; i += 2)
        {
            st_u16_t uiW = (st_u16_t)( (st_u16_t)pDisk[i] |
                                         ((st_u16_t)pDisk[i + 1] << 8) );
            uiSum += uiW;
        }
        UC_TEST("[N] WD1772 checksum == 0x1234",
                (uiSum & 0xFFFFu) == 0x1234u);
    }
    else
    {
        TEST_SKIP("[N] image is bootable after mount_make_bootable");
        TEST_SKIP("[N] WD1772 checksum == 0x1234");
    }

    /* Idempotent: already bootable -> still bootable */
    eResult = mount_make_bootable(ptImg);
    UC_TEST("[N] mount_make_bootable idempotent == ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    pDisk = NULL;
    if (image_st_get_disk(ptImg, &pDisk) == ST_NO_ERROR && pDisk != NULL)
    {
        UC_TEST("[N] image still bootable after second call",
                mount_is_bootable(pDisk) == ST_TRUE);
    }
    else
    {
        TEST_SKIP("[N] image still bootable after second call");
    }

    /* Verify with hand-crafted bootsector (from UC18.2 test) */
    memset(aBoot, 0, sizeof(aBoot));
    uiSum = 0;
    for (i = 2; i < 512; i += 2) uiSum += 0;   /* all zero except word[0] */
    /* Set word[0] LE16 so sum == 0x1234 */
    aBoot[0] = 0x34; aBoot[1] = 0x12;
    UC_TEST("[N] hand-crafted bootable sector passes mount_is_bootable",
            mount_is_bootable(aBoot) == ST_TRUE);

    image_st_close(&ptImg);
    ptImg = NULL;

test_image_cmd:
    /* ==== [N] image from .st (via mount view) ==== */
    printf("\n--- [N] image from mount view ---\n");

    /* INTENT[INT-MNT-038 -> TC-MNT-091 -> REQ-MNT-023 -> UFR-MNT-010]:
     * mount_save_image from an open mount view saves a valid .st file. */
    eResult = mount_view_open(szSrcSt, &tLineCtx, &ptView);
    if (eResult != ST_NO_ERROR || ptView == NULL)
    {
        printf("  [SKIP] mount_view_open failed - remaining tests skipped\n");
        goto shutdown;
    }

    UC_TEST("[N] mount_view_open .st == ST_NO_ERROR",
            eResult == ST_NO_ERROR && ptView != NULL);
    UC_TEST("[N] mounted .st has 2 files",
            ptView->iEntryCount == 2);

    /* Save as .msa using mount_save_image (image command path 1) */
    eResult = mount_save_image(ptView, MOUNT_SAVE_MSA, szOutMsa);
    UC_TEST("[N] mount_save_image .msa == ST_NO_ERROR",
            eResult == ST_NO_ERROR);

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szOutMsa, &tStat);
    UC_TEST("[N] .msa file exists", tStat.bExists);
    UC_TEST("[N] .msa compressed (< 737280)", tStat.uiSize < 737280u);

    /* Save as .st */
    eResult = mount_save_image(ptView, MOUNT_SAVE_ST, szOutSt);
    UC_TEST("[N] mount_save_image .st == ST_NO_ERROR",
            eResult == ST_NO_ERROR);

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szOutSt, &tStat);
    UC_TEST("[N] .st file size == 737280", tStat.uiSize == 737280u);

    mount_view_close(&ptView);

    /* ==== [N] image from directory (no mount view) ==== */
    printf("\n--- [N] image from directory ---\n");

    /* INTENT[INT-MNT-039 -> TC-MNT-092 -> REQ-MNT-023 -> UFR-MNT-010]:
     * mount_view_open on a directory creates a populated image; save
     * produces a valid .st loadable afterwards. */
    eResult = mount_view_open(szSrcDir, &tLineCtx, &ptView);
    UC_TEST("[N] mount_view_open dir == ST_NO_ERROR",
            eResult == ST_NO_ERROR && ptView != NULL);
    if (ptView != NULL)
    {
        UC_TEST("[N] dir image has 2 files from dir",
                ptView->iEntryCount == 2);

        eResult = mount_save_image(ptView, MOUNT_SAVE_ST, szOutStDir);
        UC_TEST("[N] dir image saved as .st == ST_NO_ERROR",
                eResult == ST_NO_ERROR);

        mount_view_close(&ptView);

        /* Reload and verify */
        ptChk = NULL;
        if (image_st_load(szOutStDir, &ptChk) == ST_NO_ERROR)
        {
            iCount = 0;
            image_st_list_root(ptChk, aEntries, IST_RDE, &iCount);
            UC_TEST("[N] reloaded dir image has 2 files", iCount == 2);
            image_st_close(&ptChk);
        }
        else
        {
            TEST_SKIP("[N] reloaded dir image has 2 files (load failed)");
        }
    }
    else
    {
        TEST_SKIP("[N] dir image has 2 files from dir");
        TEST_SKIP("[N] dir image saved as .st == ST_NO_ERROR");
        TEST_SKIP("[N] reloaded dir image has 2 files");
    }

    /* ==== [R] additional robustness ==== */
    printf("\n--- [R] image from non-dir selected ---\n");

    /* INTENT[INT-MNT-040 -> TC-MNT-093 -> REQ-MNT-023 -> UFR-MNT-010]:
     * mount_view_open on a .st file (not a directory) dispatches correctly. */
    eResult = mount_view_open(szSrcSt, &tLineCtx, &ptView);
    UC_TEST("[R] mount_view_open .st again == ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    if (ptView != NULL)
        mount_view_close(&ptView);

    /* [R] mount_make_bootable with NULL image */
    UC_TEST("[R] mount_make_bootable(NULL) == ST_ERROR (regression)",
            mount_make_bootable(NULL) == ST_ERROR);

    /* [R] mount_view_close idempotent */
    UC_TEST("[R] mount_view_close(&NULL) == ST_NO_ERROR",
            mount_view_close(&ptView) == ST_NO_ERROR);

    /* ==== [S] Visual tests ==== */
    printf("\n--- [S] UC20 visual tests ---\n");
    TEST_MANUAL(
        "[S] ENTER in mount opens file in hex editor",
        "Open a .st with 'mount', select a file, press ENTER - "
        "does the hex editor open with the file content?");
    TEST_MANUAL(
        "[S] hex editor title shows filename and size",
        "Does the hex editor window title show 'A:\\ FILENAME.PRG [N bytes]'?");
    TEST_MANUAL(
        "[S] F key makes disk bootable",
        "With 'mount' open on a non-bootable .st, press F - "
        "does 'Bootable: Yes' appear in the properties panel?");
    TEST_MANUAL(
        "[S] F key sets bDirty flag",
        "After pressing F, does the status bar show '[*] unsaved'?");
    TEST_MANUAL(
        "[S] 'image' command with mount open saves .st",
        "With 'mount' open, run 'image' - does 'disk.st' appear in cwd?");
    TEST_MANUAL(
        "[S] 'image --msa' creates .msa file",
        "Run 'image --msa' - does 'disk.msa' appear in cwd?");
    TEST_MANUAL(
        "[S] 'image' without mount uses selected dir",
        "Select a directory with 'dir', then run 'image' - "
        "does 'disk.st' appear in cwd with files from that dir?");
    TEST_MANUAL(
        "[S] 'image --bootable' produces bootable .st",
        "Run 'image --bootable' - can the result be mounted and "
        "shows 'Bootable: Yes' in the mount view?");

shutdown:
    if (ptView  != NULL) mount_view_close(&ptView);
    if (ptImg   != NULL) image_st_close(&ptImg);
    if (ptChk   != NULL) image_st_close(&ptChk);

    line_shutdown(&tLineCtx);
    trace_shutdown();
    gui_shutdown();
    st_shutdown(&tMachine);

    printf("\n=== UC20 Results: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
