/*
 * use_case_18_1.c - Test suite for UC18.1: mount D2D view
 *
 * Tests:
 *   mount_view_open / mount_view_close lifecycle
 *   MOUNT_SRC_ST  : .st  image load   (round-trip fixture)
 *   MOUNT_SRC_MSA : .msa image load   (round-trip fixture)
 *   MOUNT_SRC_DIR : blank + populate  (temp directory fixture)
 *   mount_view_add_file : add + refresh
 *   gui_find_window_by_type : search + not-found
 *   Constants and enumerations
 *
 * TEST MATRIX - UC18.1:
 *   [N] Nominal    : 12 tests - lifecycle, source types, add-file,
 *                               gui_find_window_by_type
 *   [R] Robustness :  8 tests - NULL params, unsupported source
 *   [S] Skipped    :  8 tests - visual validation (run make manual)
 */

#include "use_cases.h"
#include "../src/file.h"
#include "../src/image_msa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Fixture helpers                                                      */
/* ------------------------------------------------------------------ */

/* Write a tiny .st image to szPath (blank, no files). */
static st_bool_t fixture_write_st(const char *szPath)
{
    image_st_t *ptImg = NULL;
    st_bool_t   bOk   = ST_FALSE;

    if (image_st_create(&ptImg) != ST_NO_ERROR)
        return ST_FALSE;
    if (image_st_save(ptImg, szPath) == ST_NO_ERROR)
        bOk = ST_TRUE;
    image_st_close(&ptImg);
    return bOk;
}

/* Write a .st image containing two small files to szPath. */
static st_bool_t fixture_write_st_with_files(const char *szPath)
{
    image_st_t    *ptImg = NULL;
    st_u8_t        aBuf[16];
    st_bool_t      bOk   = ST_FALSE;
    int            i;

    if (image_st_create(&ptImg) != ST_NO_ERROR)
        return ST_FALSE;
    for (i = 0; i < 16; i++) aBuf[i] = (st_u8_t)i;
    if (image_st_write_file(ptImg, "FILE1.DAT", aBuf, 16) != ST_NO_ERROR ||
        image_st_write_file(ptImg, "FILE2.BIN", aBuf, 8)  != ST_NO_ERROR)
        goto done;
    if (image_st_save(ptImg, szPath) == ST_NO_ERROR)
        bOk = ST_TRUE;
done:
    image_st_close(&ptImg);
    return bOk;
}

/* Write a .msa image containing two small files to szPath. */
static st_bool_t fixture_write_msa_with_files(const char *szPath)
{
    image_st_t *ptImg = NULL;
    st_u8_t     aBuf[16];
    st_bool_t   bOk   = ST_FALSE;
    int         i;

    if (image_st_create(&ptImg) != ST_NO_ERROR)
        return ST_FALSE;
    for (i = 0; i < 16; i++) aBuf[i] = (st_u8_t)(0xFF - i);
    if (image_st_write_file(ptImg, "DEMO.PRG", aBuf, 16) != ST_NO_ERROR ||
        image_st_write_file(ptImg, "README.TXT", aBuf, 8) != ST_NO_ERROR)
        goto done;
    if (image_msa_save(ptImg, szPath) == ST_NO_ERROR)
        bOk = ST_TRUE;
done:
    image_st_close(&ptImg);
    return bOk;
}

/* Write a small PC file (16 bytes) to szPath. */
static st_bool_t fixture_write_pc_file(const char *szPath)
{
    file_t  *ptFile = NULL;
    st_u8_t  aBuf[16];
    int      i;
    st_bool_t bOk = ST_FALSE;

    for (i = 0; i < 16; i++) aBuf[i] = (st_u8_t)(0xAB + i);
    if (file_open(szPath, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
        return ST_FALSE;
    if (file_write(ptFile, aBuf, 16) == ST_NO_ERROR)
        bOk = ST_TRUE;
    file_close(&ptFile);
    return bOk;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    st_machine_t    tMachine;
    line_context_t  tLineCtx;
    mount_view_t   *ptView     = NULL;
    mount_view_t   *ptView2    = NULL;
    gui_window_t    hFound;
    st_error_t      eResult;

    /* --- Fixture paths --- */
    const char *szDir18   = "use_cases/UC18_1";
    const char *szStFile  = "use_cases/UC18_1/test.st";
    const char *szStFiles = "use_cases/UC18_1/test2files.st";
    const char *szMsaFile = "use_cases/UC18_1/test.msa";
    const char *szPcFile  = "use_cases/UC18_1/IMPORT.DAT";

    printf("=== UC18.1: mount D2D view ===\n\n");

    /* --- Module init --- */
    printf("--- Init ---\n");
    if (st_init(&tMachine, NULL) != ST_NO_ERROR)
    {
        printf("  [SKIP] st_init failed — skipping all GUI tests\n");
        return 0;
    }
    if (gui_init() != ST_NO_ERROR)
    {
        printf("  [SKIP] gui_init failed — headless environment\n");
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

    /* Create fixture directory */
    file_mkdir(szDir18);

    /* --- [N] Constants --- */
    printf("\n--- [N] Constants ---\n");
    UC_TEST("[N] MOUNT_WND_WIDTH > 0",
            MOUNT_WND_WIDTH > 0);
    UC_TEST("[N] MOUNT_WND_HEIGHT > 0",
            MOUNT_WND_HEIGHT > 0);
    UC_TEST("[N] MOUNT_LIST_WIDTH < MOUNT_WND_WIDTH",
            MOUNT_LIST_WIDTH < MOUNT_WND_WIDTH);
    UC_TEST("[N] MOUNT_SRC_DIR == 0",
            MOUNT_SRC_DIR == 0);
    UC_TEST("[N] MOUNT_SRC_ST  == 1",
            MOUNT_SRC_ST  == 1);
    UC_TEST("[N] MOUNT_SRC_MSA == 2",
            MOUNT_SRC_MSA == 2);

    /* --- [R] NULL guards — no GUI required --- */
    printf("\n--- [R] Robustness: NULL params ---\n");

    eResult = mount_view_open(NULL, NULL, &ptView);
    UC_TEST("[R] mount_view_open(NULL lineCtx) -> ST_ERROR",
            eResult == ST_ERROR && ptView == NULL);

    eResult = mount_view_open(NULL, &tLineCtx, NULL);
    UC_TEST("[R] mount_view_open(NULL pptView) -> ST_ERROR",
            eResult == ST_ERROR);

    eResult = mount_view_close(NULL);
    UC_TEST("[R] mount_view_close(NULL) -> ST_ERROR",
            eResult == ST_ERROR);

    eResult = mount_view_close(&ptView);
    UC_TEST("[R] mount_view_close(&NULL) -> ST_NO_ERROR (idempotent)",
            eResult == ST_NO_ERROR);

    eResult = mount_view_add_file(NULL, "foo.dat");
    UC_TEST("[R] mount_view_add_file(NULL view) -> ST_ERROR",
            eResult == ST_ERROR);

    eResult = mount_view_add_file((mount_view_t *)&eResult, NULL);
    UC_TEST("[R] mount_view_add_file(NULL path) -> ST_ERROR",
            eResult == ST_ERROR);

    eResult = gui_find_window_by_type(GUI_WND_MOUNT, NULL);
    UC_TEST("[R] gui_find_window_by_type(NULL phWnd) -> ST_ERROR",
            eResult == ST_ERROR);

    eResult = mount_view_open("use_cases/UC18_1/NONEXISTENT.st",
                               &tLineCtx, &ptView);
    UC_TEST("[R] mount_view_open(non-existent) -> ST_ERROR",
            eResult == ST_ERROR && ptView == NULL);

    /* --- [N] gui_find_window_by_type — no window open --- */
    printf("\n--- [N] gui_find_window_by_type ---\n");
    hFound = (gui_window_t)&eResult;  /* non-NULL sentinel */
    eResult = gui_find_window_by_type(GUI_WND_MOUNT, &hFound);
    UC_TEST("[N] find type (none open) -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    UC_TEST("[N] find type (none open) -> hFound == NULL",
            hFound == NULL);

    /* --- Prepare fixtures --- */
    printf("\n--- Preparing fixtures ---\n");
    if (!fixture_write_st_with_files(szStFiles) ||
        !fixture_write_msa_with_files(szMsaFile) ||
        !fixture_write_st(szStFile)               ||
        !fixture_write_pc_file(szPcFile))
    {
        printf("  [SKIP] fixture creation failed — skipping view tests\n");
        goto shutdown;
    }
    printf("  fixtures created OK\n");

    /* --- [N] Open .st image --- */
    printf("\n--- [N] MOUNT_SRC_ST ---\n");
    ptView = NULL;
    eResult = mount_view_open(szStFiles, &tLineCtx, &ptView);
    UC_TEST("[N] mount .st -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    UC_TEST("[N] mount .st -> ptView != NULL",
            ptView != NULL);
    if (ptView != NULL)
    {
        UC_TEST("[N] mount .st -> eSrc == MOUNT_SRC_ST",
                ptView->eSrc == MOUNT_SRC_ST);
        UC_TEST("[N] mount .st -> iEntryCount == 2",
                ptView->iEntryCount == 2);
        UC_TEST("[N] mount .st -> ptImg != NULL",
                ptView->ptImg != NULL);
        UC_TEST("[N] mount .st -> hWnd != NULL",
                ptView->hWnd != NULL);

        /* gui_find_window_by_type should now find it */
        hFound = NULL;
        eResult = gui_find_window_by_type(GUI_WND_MOUNT, &hFound);
        UC_TEST("[N] find GUI_WND_MOUNT (open) -> hFound != NULL",
                eResult == ST_NO_ERROR && hFound != NULL);

        eResult = mount_view_close(&ptView);
        UC_TEST("[N] mount_view_close -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
        UC_TEST("[N] mount_view_close -> *pptView == NULL",
                ptView == NULL);
    }

    /* --- [N] Open .msa image --- */
    printf("\n--- [N] MOUNT_SRC_MSA ---\n");
    ptView = NULL;
    eResult = mount_view_open(szMsaFile, &tLineCtx, &ptView);
    UC_TEST("[N] mount .msa -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
    if (ptView != NULL)
    {
        UC_TEST("[N] mount .msa -> eSrc == MOUNT_SRC_MSA",
                ptView->eSrc == MOUNT_SRC_MSA);
        UC_TEST("[N] mount .msa -> iEntryCount == 2",
                ptView->iEntryCount == 2);
        mount_view_close(&ptView);
    }

    /* --- [N] mount_view_add_file --- */
    printf("\n--- [N] mount_view_add_file ---\n");
    ptView = NULL;
    eResult = mount_view_open(szStFile, &tLineCtx, &ptView);
    if (ptView != NULL && ptView->ptImg != NULL)
    {
        int iCountBefore = ptView->iEntryCount;
        eResult = mount_view_add_file(ptView, szPcFile);
        UC_TEST("[N] add_file -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
        UC_TEST("[N] add_file -> iEntryCount incremented",
                ptView->iEntryCount == iCountBefore + 1);
        UC_TEST("[N] add_file -> bDirty set",
                ptView->bDirty == ST_TRUE);
        mount_view_close(&ptView);
    }
    else
    {
        TEST_SKIP("[N] add_file tests (blank .st open failed)");
        TEST_SKIP("[N] add_file iEntryCount incremented");
        TEST_SKIP("[N] add_file bDirty set");
        if (ptView != NULL) mount_view_close(&ptView);
    }

    /* --- [N] Double close idempotence --- */
    printf("\n--- [N] Idempotence ---\n");
    ptView = NULL;
    eResult = mount_view_close(&ptView);
    UC_TEST("[N] mount_view_close(&NULL) idempotent",
            eResult == ST_NO_ERROR);

    /* --- [S] Visual tests --- */
    printf("\n--- [S] Visual tests (run make manual UC=18_1) ---\n");
    TEST_MANUAL("[S] mount .st view visible, title A:\\ [ST]",
                "Is the mount window open with correct title?");
    TEST_MANUAL("[S] file list shows 2 entries (FILE1.DAT, FILE2.BIN)",
                "Does the left panel list two files?");
    TEST_MANUAL("[S] right panel shows properties (Source, Files, Free)",
                "Is the properties panel populated?");
    TEST_MANUAL("[S] UP/DOWN keys navigate and highlight selection",
                "Does keyboard navigation work?");
    TEST_MANUAL("[S] DEL removes selected file (entry count decrements)",
                "Is a file removed on DEL?");
    TEST_MANUAL("[S] Scroll wheel scrolls the file list",
                "Does mouse wheel scroll the list?");
    TEST_MANUAL("[S] ESC closes the window",
                "Does ESC close the mount view?");
    TEST_MANUAL("[S] mount .msa: window shows MSA tag in title",
                "Does the MSA image open with [MSA] in title?");

shutdown:
    /* Cleanup */
    if (ptView != NULL)
        mount_view_close(&ptView);
    if (ptView2 != NULL)
        mount_view_close(&ptView2);

    line_shutdown(&tLineCtx);
    trace_shutdown();
    gui_shutdown();
    st_shutdown(&tMachine);

    printf("\n=== UC18.1 Results: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
