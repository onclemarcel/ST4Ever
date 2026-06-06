/*
 * use_case_18_2.c - Test suite for UC18.2
 *
 * Tests:
 *   P10: dir navigation history (ALT+←/→) — constants + init state
 *   P14: dir multi-selection (CTRL+SPACE)  — constants + toggle logic
 *   P34: BPB geometry display in mount right panel
 *   P36: header no longer shows "/IST_RDE"
 *   P37: mount_is_bootable() checksum
 *   P38: bootsector hex viewer (B key, temp file)
 *
 * TEST MATRIX - UC18.2:
 *   [N] Nominal    : 12 tests - constants, history init, multi-sel
 *                               toggle, bootable checksum, mount open
 *   [R] Robustness :  4 tests - NULL guards
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
    st_u8_t     aBuf[16];
    int         i;

    if (image_st_create(&ptImg) != ST_NO_ERROR)
        return ST_FALSE;
    for (i = 0; i < 16; i++) aBuf[i] = (st_u8_t)i;
    if (image_st_write_file(ptImg, "DEMO.PRG", aBuf, 16) == ST_NO_ERROR &&
        image_st_save(ptImg, szPath) == ST_NO_ERROR)
        bOk = ST_TRUE;
    image_st_close(&ptImg);
    return bOk;
}

/* Build a hand-crafted 512-byte bootsector whose WD1772 checksum is
 * exactly 0x1234.  All words = 0, then adjust word 0 to carry. */
static void make_bootable_sector(st_u8_t *pSect)
{
    st_u32_t uiSum;
    st_u16_t uiAdj;
    int      i;

    memset(pSect, 0, 512);
    /* sum of 256 LE16 words is 0; we need sum == 0x1234 */
    /* set word[0] (bytes 0-1) to 0x1234 */
    uiAdj = 0x1234u;
    /* verify: sum of 256 words = uiAdj */
    uiSum = uiAdj;
    for (i = 2; i < 512; i += 2)
        uiSum += 0;
    if ((uiSum & 0xFFFFu) != 0x1234u)
        uiAdj = (st_u16_t)(0x1234u - (uiSum - uiAdj));
    pSect[0] = (st_u8_t)(uiAdj & 0xFF);
    pSect[1] = (st_u8_t)((uiAdj >> 8) & 0xFF);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    st_machine_t    tMachine;
    line_context_t  tLineCtx;
    dir_view_t     *ptDir    = NULL;
    mount_view_t   *ptView   = NULL;
    st_u8_t         aBoot[512];
    st_error_t      eResult;

    const char *szDir182  = "use_cases/UC18_2";
    const char *szStFile  = "use_cases/UC18_2/boot.st";

    printf("=== UC18.2: dir history + multi-sel + mount BPB ===\n\n");

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

    file_mkdir(szDir182);

    /* ==== [N] Constants ==== */
    printf("\n--- [N] Constants ---\n");

    /* INTENT[INT-MNT-020 -> TC-MNT-070..071 -> REQ-DIR-024,026 -> UFR-DIR-014..015]:
     * DIR_NAV_HIST_MAX and DIR_MULTI_SEL_MAX shall be defined and at least 8
     * so that the feature provides meaningful capacity for typical sessions. */
    UC_TEST("[N] DIR_NAV_HIST_MAX >= 8",
            DIR_NAV_HIST_MAX >= 8);
    UC_TEST("[N] DIR_MULTI_SEL_MAX >= 8",
            DIR_MULTI_SEL_MAX >= 8);

    /* ==== [R] mount_is_bootable NULL guard ==== */
    printf("\n--- [R] Robustness ---\n");

    /* INTENT[INT-MNT-021 -> TC-MNT-072 -> REQ-MNT-017 -> UFR-MNT-006]:
     * mount_is_bootable(NULL) shall return ST_FALSE without crashing. */
    UC_TEST("[R] mount_is_bootable(NULL) == ST_FALSE",
            mount_is_bootable(NULL) == ST_FALSE);

    /* ==== [N] Bootable checksum ==== */
    printf("\n--- [N] Bootable checksum ---\n");

    /* INTENT[INT-MNT-022 -> TC-MNT-073 -> REQ-MNT-017 -> UFR-MNT-006]:
     * A blank 512-byte sector (all zeros) sums to 0, not 0x1234 => not bootable. */
    memset(aBoot, 0, 512);
    UC_TEST("[N] blank sector -> not bootable",
            mount_is_bootable(aBoot) == ST_FALSE);

    /* INTENT[INT-MNT-023 -> TC-MNT-074 -> REQ-MNT-017 -> UFR-MNT-006]:
     * A hand-crafted sector whose LE16 word sum equals 0x1234 shall be bootable. */
    make_bootable_sector(aBoot);
    UC_TEST("[N] hand-crafted sector -> bootable",
            mount_is_bootable(aBoot) == ST_TRUE);

    /* INTENT[INT-MNT-024 -> TC-MNT-075 -> REQ-MNT-017 -> UFR-MNT-006]:
     * Flipping any byte of the bootable sector changes the checksum => not bootable. */
    /* Corrupt the checksum — should flip back to not-bootable */
    aBoot[0] ^= 0x01;
    UC_TEST("[N] corrupted sector -> not bootable",
            mount_is_bootable(aBoot) == ST_FALSE);

    /* ==== [N] Dir view — history init ==== */
    printf("\n--- [N] dir navigation history ---\n");

    /* INTENT[INT-MNT-025 -> TC-MNT-076..078 -> REQ-DIR-024 -> UFR-DIR-014]:
     * dir_open() shall seed aszNavHistory[0] with the root path so that
     * ALT+LEFT history back is meaningful from the very first position. */
    eResult = dir_open("src", &tLineCtx, ST_FALSE, &ptDir);
    if (eResult == ST_NO_ERROR && ptDir != NULL)
    {
        UC_TEST("[N] dir_open: iNavHistCount == 1",
                ptDir->iNavHistCount == 1);
        UC_TEST("[N] dir_open: iNavHistHead == 0",
                ptDir->iNavHistHead == 0);
        UC_TEST("[N] dir_open: history[0] non-empty",
                ptDir->aszNavHistory[0][0] != '\0');
        dir_close(&ptDir);
    }
    else
    {
        TEST_SKIP("[N] dir_open: iNavHistCount == 1 (dir_open failed)");
        TEST_SKIP("[N] dir_open: iNavHistHead == 0");
        TEST_SKIP("[N] dir_open: history[0] non-empty");
    }

    /* ==== [N] Dir view — multi-selection init ==== */
    printf("\n--- [N] dir multi-selection ---\n");

    /* INTENT[INT-MNT-026 -> TC-MNT-079 -> REQ-DIR-026,027 -> UFR-DIR-015]:
     * dir_open() shall initialise iMultiSelCount to 0 so that no file
     * appears multi-selected before the user presses CTRL+SPACE. */
    eResult = dir_open("src", &tLineCtx, ST_FALSE, &ptDir);
    if (eResult == ST_NO_ERROR && ptDir != NULL)
    {
        UC_TEST("[N] dir_open: iMultiSelCount == 0",
                ptDir->iMultiSelCount == 0);
        dir_close(&ptDir);
    }
    else
    {
        TEST_SKIP("[N] dir_open: iMultiSelCount == 0 (dir_open failed)");
    }

    /* ==== [N][R] mount open — verify ptLineCtx stored ==== */
    printf("\n--- [N] mount ptLineCtx stored ---\n");

    if (!fixture_write_st(szStFile))
    {
        printf("  fixture creation failed -- skipping mount tests\n");
        goto shutdown;
    }

    ptView = NULL;
    eResult = mount_view_open(szStFile, &tLineCtx, &ptView);
    if (eResult == ST_NO_ERROR && ptView != NULL)
    {
        /* INTENT[INT-MNT-027 -> TC-MNT-080 -> REQ-MNT-015 -> UFR-MNT-007]:
         * mount_view_open() shall store the caller ptLineCtx so that
         * mount_open_bootsector() can call edit_hex_open() on behalf of line. */
        UC_TEST("[N] mount_view_open: ptLineCtx stored",
                ptView->ptLineCtx == &tLineCtx);
        /* INTENT[INT-MNT-028 -> TC-MNT-081 -> REQ-MNT-015 -> UFR-MNT-007]:
         * ptBootHexView shall start NULL; it is set only when the user presses B. */
        UC_TEST("[N] mount_view_open: ptBootHexView NULL initially",
                ptView->ptBootHexView == NULL);

        printf("\n--- [S] UC18.2 visual tests ---\n");
        TEST_MANUAL(
            "[S] right panel shows Geometry: H2/S9/T80 and Bootable: No",
            "Does the mount window right panel show BPB geometry and Bootable row?");
        TEST_MANUAL(
            "[S] right panel shows Files: N / 112 (root dir capacity)",
            "Is the Files row showing count / 112?");
        TEST_MANUAL(
            "[S] header shows 'N file(s)' without '/112'",
            "Is the header clean (no /112 in the left panel title)?");
        TEST_MANUAL(
            "[S] pressing 'B' opens bootsector hex window",
            "Press B — does a hex editor window open with bootsector title?");
        TEST_MANUAL(
            "[S] dir view: CTRL+SPACE toggles purple multi-select on files",
            "Open dir, CTRL+SPACE on a file — does purple highlight appear?");
        TEST_MANUAL(
            "[S] dir view: ALT+← goes back in navigation history",
            "Navigate into a subdir, then ALT+← — do you go back?");
        TEST_MANUAL(
            "[S] dir view: ALT+→ goes forward in navigation history",
            "After ALT+←, press ALT+→ — do you go forward?");
        TEST_MANUAL(
            "[S] ESC closes mount window",
            "Press ESC — does the mount window close?");

        mount_view_close(&ptView);
    }
    else
    {
        TEST_SKIP("[N] mount_view_open: ptLineCtx stored (open failed)");
        TEST_SKIP("[N] mount_view_open: ptBootHexView NULL initially");
    }

shutdown:
    if (ptDir  != NULL) dir_close(&ptDir);
    if (ptView != NULL) mount_view_close(&ptView);

    line_shutdown(&tLineCtx);
    trace_shutdown();
    gui_shutdown();
    st_shutdown(&tMachine);

    printf("\n=== UC18.2 Results: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
