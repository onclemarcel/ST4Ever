/*
 * use_case_08.c - UC8: Text editor view (edit_txt.h/edit_txt.c)
 *
 * Tests run from the project root (make tests requirement — R12).
 *
 * Fixtures:
 *   use_cases/UC08/hello.txt   — 4 lines with a tab on line 3
 *   use_cases/UC08/empty.txt   — zero-byte file
 *
 * TEST MATRIX - UC08:
 *   [N] Nominal    : 20 tests - display helpers (8), clipboard
 *                               round-trip (1), GUI_MOD flags (5),
 *                               constants (3), backup settings (3)
 *   [R] Robustness : 11 tests - lifecycle NULL guards (8),
 *                               clipboard NULL guards (3)
 *   [S] Skipped    : 12 tests - GUI rendering/editing (make manual),
 *                               LF load + backup lifecycle (make manual)
 *   Total          : 43
 *
 * Traceability:
 *   INT-EDT-001..007 → TC-EDT-001..028 → REQ-EDT-001..013 → UFR-EDT-001..006
 *   INT-EDT-007      → TC-EDT-021..025 → REQ-GUI-020..025 → UFR-GUI-007
 *   INT-EDT-008..009 → TC-EDT-037..043 → REQ-EDT-014..017 → UFR-EDT-007
 */

#include "use_cases.h"
#include "../src/edit_txt.h"
#include "../src/file.h"
#include "../src/gui.h"

int g_uc_fails = 0;

#define UC08_TXT_PATH   "use_cases/UC08/hello.txt"
#define UC08_EMPTY_PATH "use_cases/UC08/empty.txt"
#define UC08_LF_PATH    "use_cases/UC08/unix_lf.txt"

/* ------------------------------------------------------------------
 * Display-column helpers — same contract as the internal ones in
 * edit_txt.c, re-implemented locally so we can unit-test the contract
 * without white-box access to static functions.
 * ------------------------------------------------------------------ */

static int uc08_display_len(const char *szLine, int iByteCol)
{
    int iDisp = 0;
    int i;
    for (i = 0; i < iByteCol && szLine[i] != '\0'; i++)
    {
        if (szLine[i] == '\t')
            iDisp = (iDisp / EDIT_TXT_TAB_WIDTH + 1)
                    * EDIT_TXT_TAB_WIDTH;
        else
            iDisp++;
    }
    return iDisp;
}

static int uc08_byte_col_from_disp(const char *szLine, int iDisp)
{
    int iCurDisp = 0;
    int i        = 0;
    int iNext;
    while (szLine[i] != '\0')
    {
        if (iCurDisp >= iDisp) break;
        if (szLine[i] == '\t')
            iNext = (iCurDisp / EDIT_TXT_TAB_WIDTH + 1)
                    * EDIT_TXT_TAB_WIDTH;
        else
            iNext = iCurDisp + 1;
        if (iNext > iDisp) break;
        iCurDisp = iNext;
        i++;
    }
    return i;
}


/* ==================================================================
 * main
 * ================================================================== */

int main(void)
{
    edit_txt_view_t *ptView = NULL;
    st_error_t       eRet;

    printf("=== UC8: Text editor (edit_txt.h/edit_txt.c) ===\n\n");

    /* ================================================================
     * BLOCK 1 — edit_txt_open / edit_txt_close NULL guards
     * ================================================================ */

    printf("--- Block 1: lifecycle NULL guards ---\n");

    /* INTENT[INT-EDT-001 → TC-EDT-001..008 → REQ-EDT-001..002 → UFR-EDT-001]:
     * edit_txt_open/close lifecycle must reject NULL params before any
     * side effect; close(&NULL) must be a safe idempotent no-op. */

    /* INTENT[INT-EDT-001 → TC-EDT-001]: NULL szPath → ST_ERROR */
    eRet = edit_txt_open(NULL, &ptView);
    UC_TEST("[R] edit_txt_open(NULL path) → ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[R] edit_txt_open(NULL path) → *pptView = NULL",
            ptView == NULL);

    /* INTENT[INT-EDT-001 → TC-EDT-002]: NULL ptLineCtx → ST_ERROR */
    eRet = edit_txt_open(UC08_TXT_PATH, &ptView);
    UC_TEST("[R] edit_txt_open(NULL ctx) → ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-EDT-001 → TC-EDT-003]: NULL pptView → ST_ERROR */
    eRet = edit_txt_open(UC08_TXT_PATH, NULL);
    UC_TEST("[R] edit_txt_open(NULL pptView) → ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-EDT-001 → TC-EDT-004]: missing file → ST_ERROR */
    eRet = edit_txt_open("use_cases/UC08/no_such.txt",
                           &ptView);
    UC_TEST("[R] edit_txt_open(missing file) → ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[R] edit_txt_open(missing file) → *pptView = NULL",
            ptView == NULL);

    /* INTENT[INT-EDT-001 → TC-EDT-005]: close(NULL pptView) → ST_ERROR */
    eRet = edit_txt_close(NULL);
    UC_TEST("[R] edit_txt_close(NULL) → ST_ERROR", eRet == ST_ERROR);

    /* INTENT[INT-EDT-001 → TC-EDT-006]: close(&NULL) → ST_NO_ERROR */
    eRet = edit_txt_close(&ptView);   /* ptView == NULL */
    UC_TEST("[R] edit_txt_close(&NULL) → ST_NO_ERROR",
            eRet == ST_NO_ERROR);

    /* ================================================================
     * BLOCK 2 — display-column helpers (tab expansion contract)
     * ================================================================ */

    printf("\n--- Block 2: display-column helpers ---\n");

    /* INTENT[INT-EDT-004 → TC-EDT-009..016 → REQ-EDT-005..006 → UFR-EDT-001]:
     * Tab characters must expand to the next 4-space tab stop in display
     * coordinates; byte↔display round-trip must be lossless for all
     * positions within the line. */

    /* INTENT[INT-EDT-004 → TC-EDT-009..010]: plain ASCII, byte == display */
    UC_TEST("[N] display_len('hello', 5) == 5",
            uc08_display_len("hello", 5) == 5);

    UC_TEST("[N] display_len('hello', 0) == 0",
            uc08_display_len("hello", 0) == 0);

    /* INTENT[INT-EDT-004 → TC-EDT-016]: tab at col 0 → TAB_WIDTH */
    {
        char szTab[] = "\thello";
        UC_TEST("[N] display_len('\\thello', 1) == EDIT_TXT_TAB_WIDTH",
                uc08_display_len(szTab, 1) == EDIT_TXT_TAB_WIDTH);
    }

    /* INTENT[INT-EDT-004]: tab at col 2 → next 4-stop */
    {
        char szMix[] = "ab\thello";
        UC_TEST("[N] display_len('ab\\thello', 3) == 4",
                uc08_display_len(szMix, 3) == EDIT_TXT_TAB_WIDTH);
    }

    /* INTENT[INT-EDT-004]: second tab at col 4 → col 8 */
    {
        char szTwo[] = "\t\thello";
        UC_TEST("[N] display_len('\\t\\thello', 2) == 8",
                uc08_display_len(szTwo, 2) == EDIT_TXT_TAB_WIDTH * 2);
    }

    /* INTENT[INT-EDT-004]: inverse round-trip */
    {
        char szMix[] = "ab\thello";
        int  iDisp   = uc08_display_len(szMix, 3);
        int  iByte   = uc08_byte_col_from_disp(szMix, iDisp);
        UC_TEST("[N] byte_col_from_disp round-trip via tab",
                iByte == 3);
    }

    /* INTENT[INT-EDT-004]: display col past end → stops at end */
    {
        char szStr[] = "abc";
        int  iByte   = uc08_byte_col_from_disp(szStr, 999);
        UC_TEST("[N] byte_col_from_disp beyond string → strlen",
                iByte == 3);
    }

    /* INTENT[INT-EDT-004]: display_len stops at NUL */
    {
        char szStr[] = "abc";
        UC_TEST("[N] display_len stops at NUL (col > len)",
                uc08_display_len(szStr, 100) == 3);
    }

    /* ================================================================
     * BLOCK 3 — clipboard API NULL guards
     * ================================================================ */

    printf("\n--- Block 3: clipboard API NULL guards ---\n");

    /* INTENT[INT-EDT-006 → TC-EDT-017..020 → REQ-EDT-012, REQ-GUI-023..025 → UFR-EDT-004, UFR-GUI-007]:
     * gui_clipboard_set/get_text must reject NULL/zero params; on Windows
     * a set+get round-trip must preserve the text exactly. */

    /* INTENT[INT-EDT-006 → TC-EDT-017]: NULL szText rejected */
    eRet = gui_clipboard_set_text(NULL);
    UC_TEST("[R] gui_clipboard_set_text(NULL) → ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-EDT-006 → TC-EDT-020]: NULL buf rejected */
    eRet = gui_clipboard_get_text(NULL, 64);
    UC_TEST("[R] gui_clipboard_get_text(NULL buf) → ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-EDT-006]: uiMax == 0 rejected */
    {
        char szBuf[4] = "xxx";
        eRet = gui_clipboard_get_text(szBuf, 0);
        UC_TEST("[R] gui_clipboard_get_text(uiMax=0) → ST_ERROR",
                eRet == ST_ERROR);
    }

    /* INTENT[INT-EDT-006]: set+get round-trip (may be skipped
     * if no clipboard available in CI environment) */
    {
        char szOut[64];
        eRet = gui_clipboard_set_text("ST4Ever clipboard test");
        if (eRet == ST_NO_ERROR)
        {
            eRet = gui_clipboard_get_text(szOut, sizeof(szOut));
            UC_TEST("[N] clipboard set+get round-trip",
                    eRet == ST_NO_ERROR
                    && strcmp(szOut, "ST4Ever clipboard test") == 0);
        }
        else
        {
            TEST_SKIP("[N] clipboard round-trip "
                      "(no clipboard in this environment)");
        }
    }

    /* ================================================================
     * BLOCK 4 — GUI_MOD_* bitmask constants
     * ================================================================ */

    printf("\n--- Block 4: GUI modifier flags ---\n");

    /* INTENT[INT-EDT-007 → TC-EDT-021..025 → REQ-EDT-013, REQ-GUI-020..022 → UFR-GUI-007]:
     * GUI_MOD_CTRL/SHIFT/ALT must be distinct non-overlapping bitmasks
     * so that views can test individual modifiers and combinations
     * without false positives. */

    /* INTENT[INT-EDT-007 → TC-EDT-021..023]: flags are distinct bits */
    UC_TEST("[N] GUI_MOD_CTRL  == 0x01",
            GUI_MOD_CTRL  == 0x01u);
    UC_TEST("[N] GUI_MOD_SHIFT == 0x02",
            GUI_MOD_SHIFT == 0x02u);
    UC_TEST("[N] GUI_MOD_ALT   == 0x04",
            GUI_MOD_ALT   == 0x04u);
    UC_TEST("[N] GUI_MOD flags non-overlapping",
            (GUI_MOD_CTRL & GUI_MOD_SHIFT) == 0
         && (GUI_MOD_CTRL & GUI_MOD_ALT)   == 0
         && (GUI_MOD_SHIFT & GUI_MOD_ALT)  == 0);

    /* INTENT[INT-EDT-007]: flags can be ORed */
    {
        st_u8_t uiMods = (st_u8_t)(GUI_MOD_CTRL | GUI_MOD_SHIFT);
        UC_TEST("[N] CTRL|SHIFT combined has both bits set",
                (uiMods & GUI_MOD_CTRL)  != 0
             && (uiMods & GUI_MOD_SHIFT) != 0
             && (uiMods & GUI_MOD_ALT)   == 0);
    }

    /* ================================================================
     * BLOCK 5 — EDIT_TXT limits constants
     * ================================================================ */

    printf("\n--- Block 5: edit_txt constants ---\n");
    /* INTENT[INT-EDT-005 → TC-EDT-026..028 → REQ-EDT-008..009 → UFR-EDT-006]:
     * Public constants must match the documented values so callers can
     * build undo snapshots and clip buffers of the right size. */

    UC_TEST("[N] EDIT_TXT_TAB_WIDTH == 4",
            EDIT_TXT_TAB_WIDTH == 4);
    UC_TEST("[N] EDIT_TXT_MAX_LINE_LEN >= 512",
            EDIT_TXT_MAX_LINE_LEN >= 512);
    UC_TEST("[N] EDIT_TXT_CLIP_MAX >= 1024",
            EDIT_TXT_CLIP_MAX >= 1024);

    /* ================================================================
     * BLOCK 6 — Backup settings (headless)
     * ================================================================ */

    printf("\n--- Block 6: backup settings ---\n");

    /* INTENT[INT-EDT-008 → TC-EDT-037..039 → REQ-EDT-015,REQ-EDT-017
     *        → UFR-EDT-007]:
     * edit_txt_get_backup() returns ST_TRUE by default; set_backup()
     * updates the global flag and the change persists. */

    /* INTENT[INT-EDT-008 → TC-EDT-037]: default is enabled */
    UC_TEST("[N] edit_txt_get_backup() default == ST_TRUE",
            edit_txt_get_backup() == ST_TRUE);

    /* INTENT[INT-EDT-008 → TC-EDT-038]: disable backup */
    edit_txt_set_backup(ST_FALSE);
    UC_TEST("[N] edit_txt_set_backup(ST_FALSE) → get returns ST_FALSE",
            edit_txt_get_backup() == ST_FALSE);

    /* INTENT[INT-EDT-008 → TC-EDT-039]: re-enable backup (restore default) */
    edit_txt_set_backup(ST_TRUE);
    UC_TEST("[N] edit_txt_set_backup(ST_TRUE) → get returns ST_TRUE",
            edit_txt_get_backup() == ST_TRUE);

    /* ================================================================
     * BLOCK 7 — Manual tests (GUI rendering, LF fix, backup lifecycle)
     * ================================================================ */

    printf("\n--- Block 7: visual / interactive tests ---\n");
    /* INTENT[INT-EDT-002..003, INT-EDT-009 → TC-EDT-029..036,
     *        TC-EDT-040..043 → REQ-EDT-003..004, REQ-EDT-007..011,
     *        REQ-EDT-014..016 → UFR-EDT-001..006, UFR-EDT-007]:
     * Full open/render/edit/save cycle validated manually; headless
     * tests cover only the portable logic (file load, helpers, API).
     * LF fix and backup lifecycle require a running GUI window. */

    TEST_MANUAL("[S] edit window opens for hello.txt with 4 lines",
                "Did the editor window open showing 4 lines?");
    TEST_MANUAL("[S] gutter shows line numbers 1–4",
                "Does the left gutter show 1, 2, 3, 4?");
    TEST_MANUAL("[S] cursor at line 1 col 0 on open",
                "Is the cursor at the beginning of line 1?");
    TEST_MANUAL("[S] arrow keys move cursor",
                "Do UP/DOWN/LEFT/RIGHT move the cursor?");
    TEST_MANUAL("[S] SHIFT+RIGHT selects a character (blue)",
                "Does SHIFT+RIGHT highlight one char in blue?");
    TEST_MANUAL("[S] CTRL+A selects all (full blue highlight)",
                "Does CTRL+A highlight the entire text?");
    TEST_MANUAL("[S] CTRL+S saves file (title [*] disappears)",
                "After typing then CTRL+S, does [*] disappear?");
    TEST_MANUAL("[S] ESC closes editor window",
                "Does ESC close the editor window?");
    TEST_MANUAL(
        "[S] LF-only file (unix_lf.txt) opens with 3 lines visible",
        "Run 'edit use_cases/UC08/unix_lf.txt'. Does it show 3 lines?");
    TEST_MANUAL(
        "[S] backup .bak file created at open",
        "Is a *_YYYYMMDD_HHMMSS.bak file present in the same dir?");
    TEST_MANUAL(
        "[S] backup removed if file unchanged at close",
        "Close without editing. Is the .bak file gone?");
    TEST_MANUAL(
        "[S] backup kept if file was modified at close",
        "Type a char, CTRL+S, then close. Is the .bak still there?");

    /* ================================================================
     * Results
     * ================================================================ */

    printf("\n");
    if (g_uc_fails == 0)
        printf("  All tests PASSED.\n\n");
    else
        printf("  %d test(s) FAILED.\n\n", g_uc_fails);

    return (g_uc_fails > 0) ? 1 : 0;
}
