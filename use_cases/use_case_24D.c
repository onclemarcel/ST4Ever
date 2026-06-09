/*
 * use_case_24D.c - Clickable band labels + edit_hex_set_cursor_pos (P49)
 *
 * TEST MATRIX - UC24D:
 *   [N] Nominal    :  6 tests - HEXED_BAND_MAX_LABELS constant,
 *                               zone enum distinct, set_cursor_pos valid
 *                               (offset 0, mid, last byte),
 *                               clamping out-of-bounds offset
 *   [R] Robustness :  2 tests - NULL ptView -> ST_ERROR,
 *                               zero-size view clamping
 *   [S] Skipped    :  2 tests - run make manual (click FAT1 label,
 *                               click JSON chip)
 *
 * Module-level traceability:
 *   UFR-HEX-008 -> REQ-HEX-029..031 -> TC-HEX-091..101
 *   -> INT-HEX-091..099
 *
 * INTENT[INT-HEX-091..099 -> TC-HEX-091..101 -> REQ-HEX-029..031
 *        -> UFR-HEX-008]:
 *   The hex editor exposes clickable navigation labels (BPB auto-labels
 *   and JSON labeled-sector chips) in band row 1, enabling the user to
 *   jump directly to key disk structures without manual offset arithmetic.
 *   edit_hex_set_cursor_pos() provides a programmatic equivalent.
 */

#include "use_cases.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Band layout constants                                              */
/* ------------------------------------------------------------------ */

static void test_band_constants(void)
{
    printf("  --- band constants ---\n");

    /* INTENT[INT-HEX-091]: HEXED_BAND_MAX_LABELS covers 4 BPB + extras */
    UC_TEST("[N] HEXED_BAND_MAX_LABELS >= 4",
              HEXED_BAND_MAX_LABELS >= 4);
    UC_TEST("[N] HEXED_BAND_MAX_LABELS <= 16",
              HEXED_BAND_MAX_LABELS <= 16);

    /* INTENT[INT-HEX-092]: label arrays sized by the constant          */
    UC_TEST("[N] array sizes match HEXED_BAND_MAX_LABELS",
              (int)ST_ARRAY_SIZE(
                  ((edit_hex_view_t *)NULL)->aiBandLabelX)
              == HEXED_BAND_MAX_LABELS);
}

/* ------------------------------------------------------------------ */
/* Zone enum still distinct after UC24D                               */
/* ------------------------------------------------------------------ */

static void test_zone_enum(void)
{
    printf("  --- zone enum distinct ---\n");

    /* INTENT[INT-HEX-093]: existing zones unchanged by UC24D additions */
    UC_TEST("[N] HEX_ZONE_HEX   == 0", HEX_ZONE_HEX       == 0);
    UC_TEST("[N] HEX_ZONE_ASCII == 1", HEX_ZONE_ASCII     == 1);
    UC_TEST("[N] HEX_ZONE_DISASM== 2", HEX_ZONE_DISASM    == 2);
    UC_TEST("[N] BAND_NOTE      == 3", HEX_ZONE_BAND_NOTE == 3);
}

/* ------------------------------------------------------------------ */
/* edit_hex_set_cursor_pos — public API                               */
/* ------------------------------------------------------------------ */

static void test_set_cursor_pos(void)
{
    edit_hex_view_t tV;

    printf("  --- edit_hex_set_cursor_pos ---\n");

    /* INTENT[INT-HEX-094]: NULL ptView returns ST_ERROR without crash  */
    UC_TEST("[R] NULL ptView -> ST_ERROR",
              edit_hex_set_cursor_pos(NULL, 0) == ST_ERROR);

    /* Set up a minimal view on the stack for non-GUI tests             */
    memset(&tV, 0, sizeof(tV));

    /* INTENT[INT-HEX-095]: zero-size view leaves cursor at 0           */
    tV.uiSize   = 0;
    tV.uiCursor = 99u;
    UC_CHECK("[R] zero-size: set_cursor_pos(42)",
             edit_hex_set_cursor_pos(&tV, 42u));
    UC_TEST("[R] zero-size: cursor unchanged at 0", tV.uiCursor == 0u);

    /* INTENT[INT-HEX-096]: offset 0 on valid buffer                   */
    tV.uiSize   = 512u;
    tV.uiCursor = 100u;
    UC_CHECK("[N] offset 0",
             edit_hex_set_cursor_pos(&tV, 0u));
    UC_TEST("[N] cursor == 0", tV.uiCursor == 0u);
    UC_TEST("[N] zone == HEX_ZONE_HEX",
              tV.eZone == HEX_ZONE_HEX);
    UC_TEST("[N] nibble == 0", tV.iNibble == 0);

    /* INTENT[INT-HEX-097]: mid-buffer offset                          */
    UC_CHECK("[N] offset 256",
             edit_hex_set_cursor_pos(&tV, 256u));
    UC_TEST("[N] cursor == 256", tV.uiCursor == 256u);

    /* INTENT[INT-HEX-098]: last valid byte (uiSize - 1)               */
    UC_CHECK("[N] offset uiSize-1",
             edit_hex_set_cursor_pos(&tV, 511u));
    UC_TEST("[N] cursor == 511", tV.uiCursor == 511u);

    /* INTENT[INT-HEX-099]: out-of-bounds offset clamped to uiSize-1   */
    UC_CHECK("[N] clamping beyond uiSize",
             edit_hex_set_cursor_pos(&tV, 9999u));
    UC_TEST("[N] cursor clamped to 511", tV.uiCursor == 511u);
}

/* ------------------------------------------------------------------ */
/* Manual tests                                                       */
/* ------------------------------------------------------------------ */

static void test_manual_labels(void)
{
    printf("  --- manual label navigation ---\n");

    TEST_MANUAL(
        "[S] Clicking FAT1@1 in band row 1 jumps to offset 512",
        "Open use_cases/UC16/roundtrip.st via 'edit'.\n"
        "  Expected: band row 1 shows FAT1@1 in a brighter cyan colour.\n"
        "  Click on FAT1@1 — the hex cursor should jump to offset 0x200\n"
        "  (row starting 000200:) and the sector highlight changes.\n"
        "  Is the click-to-navigate behaviour correct?"
    );

    TEST_MANUAL(
        "[S] Clicking a JSON [N] chip in band row 1 jumps to that sector",
        "With roundtrip.st open, use CTRL+N to annotate sector 0.\n"
        "  Save with CTRL+S. Reopen the file with 'edit'.\n"
        "  Expected: band row 1 shows a cyan [0] chip after the BPB labels.\n"
        "  Click [0] — cursor jumps back to byte 0.\n"
        "  Is the JSON chip click-to-navigate correct?"
    );
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC24D: Clickable Band Labels + set_cursor_pos ===\n");

    test_band_constants();
    test_zone_enum();
    test_set_cursor_pos();
    test_manual_labels();

    printf("\n=== UC24D: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
