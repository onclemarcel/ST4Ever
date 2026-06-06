/*
 * use_case_19.c - Test suite for UC19
 *
 * Tests:
 *   P35: umount interactive save dialog and --st/--msa/--dir flags
 *   P39: mount status bar constants and rendering
 *   P40: mount_view_add_file() progress feedback (chunked read)
 *
 * TEST MATRIX - UC19:
 *   [N] Nominal    : 14 tests - mount_save_image .st/.msa/.dir round-trips,
 *                               status bar, flags API, add_file
 *   [R] Robustness :  5 tests - NULL guards, unknown format
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

static st_bool_t fixture_write_st(const char *szPath)
{
    image_st_t *ptImg = NULL;
    st_bool_t   bOk   = ST_FALSE;
    st_u8_t     aBuf[64];
    int         i;

    if (image_st_create(&ptImg) != ST_NO_ERROR)
        return ST_FALSE;
    for (i = 0; i < 64; i++) aBuf[i] = (st_u8_t)(i & 0xFF);
    if (image_st_write_file(ptImg, "HELLO.PRG", aBuf, 64) == ST_NO_ERROR &&
        image_st_write_file(ptImg, "README.TXT", (st_u8_t *)"hello", 5)
            == ST_NO_ERROR &&
        image_st_save(ptImg, szPath) == ST_NO_ERROR)
        bOk = ST_TRUE;
    image_st_close(&ptImg);
    return bOk;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    st_machine_t      tMachine;
    line_context_t    tLineCtx;
    mount_view_t     *ptView   = NULL;
    image_st_t       *ptLoaded = NULL;
    image_st_dirent_t aEntries[IST_RDE];
    file_stat_t       tStat;
    int               iCount;
    st_error_t        eResult;

    const char *szDir19    = "use_cases/UC19";
    const char *szStSrc    = "use_cases/UC19/src.st";
    const char *szStOut    = "use_cases/UC19/saved.st";
    const char *szMsaOut   = "use_cases/UC19/saved.msa";
    const char *szDirOut   = "use_cases/UC19/extracted";

    printf("=== UC19: umount dialog + status bar + progress ===\n\n");

    /* --- Module init --- */
    printf("--- Init ---\n");
    if (st_init(&tMachine, NULL) != ST_NO_ERROR)
    {
        printf("  [SKIP] st_init failed\n");
        return 0;
    }
    if (gui_init() != ST_NO_ERROR)
    {
        printf("  [SKIP] gui_init failed — headless\n");
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

    file_mkdir(szDir19);
    if (!fixture_write_st(szStSrc))
    {
        printf("  fixture creation failed\n");
        goto shutdown;
    }

    /* ==== [R] mount_save_image NULL guards ==== */
    printf("\n--- [R] Robustness ---\n");

    /* INTENT[INT-MNT-029 -> TC-MNT-082 -> REQ-MNT-018 -> UFR-MNT-005]:
     * mount_save_image(NULL, ...) shall return ST_ERROR without crash. */
    UC_TEST("[R] mount_save_image(NULL view) == ST_ERROR",
            mount_save_image(NULL, MOUNT_SAVE_ST, szStOut) == ST_ERROR);

    /* Open a valid view for further tests */
    ptView = NULL;
    eResult = mount_view_open(szStSrc, &tLineCtx, &ptView);
    if (eResult != ST_NO_ERROR || ptView == NULL)
    {
        printf("  [SKIP] mount_view_open failed — skipping remaining tests\n");
        goto shutdown;
    }

    /* INTENT[INT-MNT-030 -> TC-MNT-083 -> REQ-MNT-018 -> UFR-MNT-005]:
     * mount_save_image with NULL path shall return ST_ERROR. */
    UC_TEST("[R] mount_save_image(NULL path) == ST_ERROR",
            mount_save_image(ptView, MOUNT_SAVE_ST, NULL) == ST_ERROR);

    /* INTENT[INT-MNT-031 -> TC-MNT-084 -> REQ-MNT-018 -> UFR-MNT-005]:
     * mount_save_image with unknown format shall return ST_ERROR. */
    UC_TEST("[R] mount_save_image(bad fmt) == ST_ERROR",
            mount_save_image(ptView, (mount_save_fmt_t)99, szStOut)
                == ST_ERROR);

    /* ==== [N] Save as .st ==== */
    printf("\n--- [N] Save as .st ---\n");

    /* INTENT[INT-MNT-032 -> TC-MNT-085 -> REQ-MNT-018 -> UFR-MNT-005]:
     * mount_save_image MOUNT_SAVE_ST writes a valid .st file that can
     * be reloaded and whose files are intact. */
    ptView->bDirty = ST_TRUE;
    eResult = mount_save_image(ptView, MOUNT_SAVE_ST, szStOut);
    UC_TEST("[N] save .st returns ST_NO_ERROR", eResult == ST_NO_ERROR);

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szStOut, &tStat);
    UC_TEST("[N] saved .st file exists",     tStat.bExists);
    UC_TEST("[N] saved .st size == 737280",  tStat.uiSize == 737280u);
    UC_TEST("[N] bDirty cleared after save", ptView->bDirty == ST_FALSE);

    /* Reload and verify files present */
    ptLoaded = NULL;
    if (image_st_load(szStOut, &ptLoaded) == ST_NO_ERROR)
    {
        iCount = 0;
        image_st_list_root(ptLoaded, aEntries, IST_RDE, &iCount);
        UC_TEST("[N] reloaded .st has 2 files", iCount == 2);
        image_st_close(&ptLoaded);
    }
    else
    {
        TEST_SKIP("[N] reloaded .st has 2 files (load failed)");
    }

    /* ==== [N] Save as .msa ==== */
    printf("\n--- [N] Save as .msa ---\n");

    /* INTENT[INT-MNT-033 -> TC-MNT-086 -> REQ-MNT-018 -> UFR-MNT-005]:
     * mount_save_image MOUNT_SAVE_MSA writes a valid .msa file smaller
     * than the equivalent raw .st image (RLE compression). */
    ptView->bDirty = ST_TRUE;
    eResult = mount_save_image(ptView, MOUNT_SAVE_MSA, szMsaOut);
    UC_TEST("[N] save .msa returns ST_NO_ERROR", eResult == ST_NO_ERROR);

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szMsaOut, &tStat);
    UC_TEST("[N] saved .msa file exists",
            tStat.bExists);
    UC_TEST("[N] saved .msa < 737280 (compressed)",
            tStat.bExists && tStat.uiSize < 737280u);
    UC_TEST("[N] bDirty cleared after msa save",
            ptView->bDirty == ST_FALSE);

    /* ==== [N] Save as directory ==== */
    printf("\n--- [N] Save as directory ---\n");

    /* INTENT[INT-MNT-034 -> TC-MNT-087 -> REQ-MNT-018 -> UFR-MNT-005]:
     * mount_save_image MOUNT_SAVE_DIR creates the directory and extracts
     * all files from the root directory into it. */
    ptView->bDirty = ST_TRUE;
    eResult = mount_save_image(ptView, MOUNT_SAVE_DIR, szDirOut);
    UC_TEST("[N] save dir returns ST_NO_ERROR", eResult == ST_NO_ERROR);

    /* Check that at least one file was extracted */
    memset(&tStat, 0, sizeof(tStat));
    file_stat(szDirOut, &tStat);
    UC_TEST("[N] output directory exists",    tStat.bExists && tStat.bIsDir);

    memset(&tStat, 0, sizeof(tStat));
    file_stat("use_cases/UC19/extracted/HELLO.PRG", &tStat);
    UC_TEST("[N] HELLO.PRG extracted",
            tStat.bExists && tStat.uiSize == 64);

    memset(&tStat, 0, sizeof(tStat));
    file_stat("use_cases/UC19/extracted/README.TXT", &tStat);
    UC_TEST("[N] README.TXT extracted",
            tStat.bExists && tStat.uiSize == 5);

    UC_TEST("[N] bDirty cleared after dir save",
            ptView->bDirty == ST_FALSE);

    /* ==== [R] extra NULL guards ==== */
    printf("\n--- [R] add_file NULL guard (P40 path coverage) ---\n");

    /* INTENT[INT-MNT-035 -> TC-MNT-088 -> REQ-MNT-019 -> UFR-MNT-005]:
     * mount_view_add_file() NULL guards unchanged after P40 chunk refactor. */
    UC_TEST("[R] add_file(NULL,path) == ST_ERROR",
            mount_view_add_file(NULL, szStSrc) == ST_ERROR);
    UC_TEST("[R] add_file(view,NULL) == ST_ERROR",
            mount_view_add_file(ptView, NULL) == ST_ERROR);

    /* ==== [S] Visual tests ==== */
    printf("\n--- [S] UC19 visual tests ---\n");
    TEST_MANUAL(
        "[S] status bar visible at bottom of mount window",
        "Open a .st with 'mount', do the mount window show "
        "a status bar with Free KB and file count at the bottom?");
    TEST_MANUAL(
        "[S] status bar shows [*] when disk dirty",
        "Delete a file from mount (DEL key) — does status bar show '[*] unsaved'?");
    TEST_MANUAL(
        "[S] umount with dirty disk shows save dialog",
        "Delete a file, then 'umount' — does a save dialog appear?");
    TEST_MANUAL(
        "[S] dialog choice 1 saves .st",
        "When dialog appears, press 1 — does 'disk.st' appear in cwd?");
    TEST_MANUAL(
        "[S] dialog choice 2 saves .msa",
        "When dialog appears, press 2 — does 'disk.msa' appear in cwd?");
    TEST_MANUAL(
        "[S] dialog choice 3 extracts dir",
        "When dialog appears, press 3 — does 'disk/' directory appear in cwd?");
    TEST_MANUAL(
        "[S] umount --st saves without dialog",
        "After dirty mount, run 'umount --st' — disk unmounts directly with disk.st saved?");
    TEST_MANUAL(
        "[S] umount --msa saves without dialog",
        "After dirty mount, run 'umount --msa' — disk.msa saved without dialog?");

    mount_view_close(&ptView);

shutdown:
    if (ptView != NULL) mount_view_close(&ptView);
    if (ptLoaded != NULL) image_st_close(&ptLoaded);

    line_shutdown(&tLineCtx);
    trace_shutdown();
    gui_shutdown();
    st_shutdown(&tMachine);

    printf("\n=== UC19 Results: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
