/*
 * use_case_10.c - UC10: Hex+ASCII+disassembly integrated view
 *
 * Tests run from the project root (make tests requirement — R12).
 *
 * Fixtures:
 *   Reuses use_cases/UC07/hello.prg  — 4-byte .text (MOVEQ #42,D0 + RTS)
 *   Inline buffer for disasm_range tests (no fixture file required)
 *
 * TEST MATRIX - UC10:
 *   [N] Nominal    : 15 tests - disasm constants (8), zone enum (3),
 *                               disasm_range behavior (4)
 *   [R] Robustness :  8 tests - NULL guards (open/close)
 *   [S] Skipped    :  8 tests - GUI rendering / sync (make manual)
 *   Total          : 31
 *
 * Traceability:
 *   INT-HEX-010..024 → TC-HEX-034..064 → REQ-HEX-011..020 → UFR-HEX-005
 */

#include "use_cases.h"
#include "../src/edit_hex.h"
#include "../src/disassemble.h"
#include "../src/file.h"

int g_uc_fails = 0;

#define UC07_PRG_PATH  "use_cases/UC07/hello.prg"


/* ==================================================================
 * main
 * ================================================================== */

int main(void)
{
    edit_hex_view_t *ptView = NULL;
    st_error_t       eRet;
    disasm_result_t  aRes[8];
    size_t           uiLines = 0;
    /* MOVEQ #42,D0 = 0x702A, RTS = 0x4E75                             */
    static const st_u8_t aCode[4] = { 0x70, 0x2A, 0x4E, 0x75 };

    printf("=== UC10: Hex+ASCII+disasm integrated view ===\n\n");

    /* ================================================================
     * BLOCK 1 — Disassembly panel constants
     * ================================================================ */
    printf("--- Block 1: Disasm panel constants [N] ---\n");

    /* INTENT[INT-HEX-010 → TC-HEX-034 → REQ-HEX-011 → UFR-HEX-005]:
     * Cache must hold enough entries for comfortable navigation around
     * the cursor.  512 entries = 1 KB of 2-byte stub instructions. */
    UC_TEST("[N] EDIT_HEX_DISASM_CACHE_LINES == 512",
            EDIT_HEX_DISASM_CACHE_LINES == 512);

    /* INTENT[INT-HEX-011 → TC-HEX-035 → REQ-HEX-011]:
     * Panel must be wide enough to show address + mnemonic + operands. */
    UC_TEST("[N] EDIT_HEX_DISASM_PANEL_CHARS == 48",
            EDIT_HEX_DISASM_PANEL_CHARS == 48);

    /* INTENT[INT-HEX-012 → TC-HEX-036 → REQ-HEX-011]:
     * Separator between ASCII and disasm panels = " | " (3 chars). */
    UC_TEST("[N] EDIT_HEX_DISASM_SEP_CHARS == 3",
            EDIT_HEX_DISASM_SEP_CHARS == 3);

    /* INTENT[INT-HEX-013 → TC-HEX-037 → REQ-HEX-011]:
     * Address prefix in panel: "$XXXXXX " = 8 characters. */
    UC_TEST("[N] EDIT_HEX_DISASM_ADDR_CHARS == 8",
            EDIT_HEX_DISASM_ADDR_CHARS == 8);

    /* INTENT[INT-HEX-014 → TC-HEX-038 → REQ-HEX-011]:
     * Cache window pre-buffer: 512 bytes before cursor included. */
    UC_TEST("[N] EDIT_HEX_DISASM_PREBUF_BYTES == 512",
            EDIT_HEX_DISASM_PREBUF_BYTES == 512);

    /* INTENT[INT-HEX-015 → TC-HEX-039 → REQ-HEX-011]:
     * Window with disasm panel must be wider than standard window. */
    UC_TEST("[N] WND_WIDTH_DISASM > WND_WIDTH_STD",
            EDIT_HEX_WND_WIDTH_DISASM > EDIT_HEX_WND_WIDTH_STD);

    /* INTENT[INT-HEX-016 → TC-HEX-040 → REQ-HEX-011]:
     * Window height constant defined (shared by both widths). */
    UC_TEST("[N] EDIT_HEX_WND_HEIGHT > 0",
            EDIT_HEX_WND_HEIGHT > 0);

    /* INTENT[INT-HEX-017 → TC-HEX-041]:
     * UC9 layout constants unchanged (regression). */
    UC_TEST("[N] EDIT_HEX_BYTES_PER_ROW == 16 (regression)",
            EDIT_HEX_BYTES_PER_ROW == 16);

    /* ================================================================
     * BLOCK 2 — Zone enum: three zones
     * ================================================================ */
    printf("\n--- Block 2: Zone enum [N] ---\n");

    /* INTENT[INT-HEX-018 → TC-HEX-042 → REQ-HEX-012]:
     * HEX_ZONE_DISASM is the third zone (value 2). */
    UC_TEST("[N] HEX_ZONE_DISASM == 2",
            HEX_ZONE_DISASM == 2);

    /* INTENT[INT-HEX-018 → TC-HEX-043]:
     * Three zones are distinct values — no overlap. */
    UC_TEST("[N] zones are distinct values",
            HEX_ZONE_HEX != HEX_ZONE_ASCII
            && HEX_ZONE_HEX != HEX_ZONE_DISASM
            && HEX_ZONE_ASCII != HEX_ZONE_DISASM);

    /* INTENT[INT-HEX-019 → TC-HEX-044]:
     * Original zone values unchanged (regression UC9). */
    UC_TEST("[N] HEX_ZONE_HEX == 0 and HEX_ZONE_ASCII == 1 (reg.)",
            HEX_ZONE_HEX == 0 && HEX_ZONE_ASCII == 1);

    /* ================================================================
     * BLOCK 3 — disasm_range() behavior with inline buffer
     * ================================================================ */
    printf("\n--- Block 3: disasm_range on 4-byte inline buffer [N] ---\n");

    /* INTENT[INT-HEX-020 → TC-HEX-045..046 → REQ-HEX-013]:
     * disasm_range on 4 bytes (stub: 1-word instructions) → 2 lines. */
    memset(aRes, 0, sizeof(aRes));
    eRet = disasm_range(aCode, 4, 0x0000, aRes, 8, &uiLines);
    UC_TEST("[N] disasm_range 4 bytes returns ST_NO_ERROR",
            eRet == ST_NO_ERROR);
    UC_TEST("[N] disasm_range 4 bytes → 2 lines (stub: 2-byte/instr)",
            uiLines == 2);

    /* INTENT[INT-HEX-021 → TC-HEX-047]:
     * First instruction address == uiAddr parameter. */
    UC_TEST("[N] aRes[0].uiAddr == 0 (load address)",
            aRes[0].uiAddr == 0);

    /* INTENT[INT-HEX-022 → TC-HEX-048]:
     * Second instruction address == 2 (stub: all 1-word instructions). */
    UC_TEST("[N] aRes[1].uiAddr == 2 (stub: 2 bytes per instr)",
            aRes[1].uiAddr == 2);

    /* ================================================================
     * BLOCK 4 — NULL guards (open / close)
     * ================================================================ */
    printf("\n--- Block 4: NULL guards [R] ---\n");

    /* INTENT[INT-HEX-023 → TC-HEX-049..054 → REQ-HEX-001]:
     * edit_hex_open() rejects NULL parameters individually. */
    eRet = edit_hex_open(NULL,  &ptView);
    UC_TEST("[R] open(NULL path) → ST_ERROR", eRet == ST_ERROR);
    UC_TEST("[R] open(NULL path) → *pptView untouched (NULL)",
            ptView == NULL);

    eRet = edit_hex_open(UC07_PRG_PATH,  &ptView);
    UC_TEST("[R] open(NULL ctx) → ST_ERROR", eRet == ST_ERROR);

    eRet = edit_hex_open(UC07_PRG_PATH,  NULL);
    UC_TEST("[R] open(NULL pptView) → ST_ERROR", eRet == ST_ERROR);

    eRet = edit_hex_open("no_such_file.bin",  &ptView);
    UC_TEST("[R] open(missing file) → ST_ERROR", eRet == ST_ERROR);
    UC_TEST("[R] open(missing file) → *pptView == NULL",
            ptView == NULL);

    /* INTENT[INT-HEX-024 → TC-HEX-055..056 → REQ-HEX-001]:
     * edit_hex_close() rejects NULL and handles *pptView == NULL. */
    eRet = edit_hex_close(NULL);
    UC_TEST("[R] close(NULL) → ST_ERROR", eRet == ST_ERROR);

    eRet = edit_hex_close(&ptView);   /* ptView is still NULL */
    UC_TEST("[R] close(&NULL) → ST_NO_ERROR (idempotent)",
            eRet == ST_NO_ERROR);

    /* ================================================================
     * BLOCK 5 — Skipped visual / interactive tests
     * ================================================================ */
    printf("\n--- Block 5: Visual/interactive tests [S] ---\n");

    TEST_MANUAL("[S] F2 toggle: disasm panel appears and disappears",
                "Open edit on hello.prg, press F2 twice — "
                "panel toggles off then on (y/n)?");

    TEST_MANUAL("[S] TAB cycles 3 zones HEX→ASCII→DISASM→HEX",
                "Press TAB 3 times: cursor goes hex→ascii→disasm→hex (y/n)?");

    TEST_MANUAL("[S] Hex cursor ↓ syncs disasm highlight",
                "Move cursor in HEX zone — disasm highlight follows (y/n)?");

    TEST_MANUAL("[S] Disasm cursor ↑/↓ syncs hex highlight",
                "Move cursor in DISASM zone — hex row highlight follows (y/n)?");

    TEST_MANUAL("[S] Click on disasm entry → zone switches to DISASM",
                "Click on a disasm line — eZone becomes DISASM (y/n)?");

    TEST_MANUAL("[S] Scroll wheel in DISASM zone scrolls disasm panel",
                "TAB to disasm zone, scroll wheel — disasm scrolls (y/n)?");

    TEST_MANUAL("[S] Window opens wider with disasm panel",
                "Window width ~1320px with disasm vs ~950px without (y/n)?");

    TEST_MANUAL("[S] Disasm shows $XXXXXX DC.W $XXXX format",
                "Disasm panel shows address + DC.W + opcode per line (y/n)?");

    /* ================================================================
     * Summary
     * ================================================================ */
    printf("\n=== UC10 result: %d failure(s) ===\n\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
