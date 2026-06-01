/*
 * use_case_07.c - UC7: load command (load.h/load.c + P11 dir indicator)
 *
 * Tests run from the project root (make tests requirement — R12).
 *
 * Fixtures:
 *   use_cases/UC07/hello.txt      — plain text, ~48 bytes
 *   use_cases/UC07/hello.bin      — raw binary, 16 bytes (0x00..0x0F)
 *   use_cases/UC07/bad_magic.prg  — PRG with wrong magic (0xDEAD)
 *   use_cases/UC07/test.st        — fake disk image (extension check)
 *   use_cases/UC01/hello.prg      — valid PRG: magic 0x601A, 4-byte .text
 *
 * TEST MATRIX - UC7:
 *   [N] Nominal    : 19 tests - load_init/shutdown lifecycle;
 *                               load_file binary/txt/prg;
 *                               load_get_state fields;
 *                               RAM content after binary load;
 *                               RAM content after PRG load;
 *                               load replaces previous state;
 *                               rejected: dir / disk image;
 *                               get_state always non-NULL
 *   [R] Robustness :  8 tests - NULL params (init/file);
 *                               load without init;
 *                               non-existent file;
 *                               bad PRG magic;
 *                               double shutdown
 *   [S] Skipped    :  4 tests - P11 dir visual indicator (make manual)
 *
 * Traceability:
 *   INT-LOD-001..010 → TC-LOD-001..027 → REQ-LOD-001..014
 *   UFR-LOD-001..005
 *   INT-DIR-011 → TC-DIR-031..034 → REQ-DIR-021 (P11 secondary sel)
 */

#include "use_cases.h"
#include "../src/load.h"

int g_uc_fails = 0;

#define UC07_TXT_PATH    "use_cases/UC07/hello.txt"
#define UC07_BIN_PATH    "use_cases/UC07/hello.bin"
#define UC07_BADPRG_PATH "use_cases/UC07/bad_magic.prg"
#define UC07_ST_PATH     "use_cases/UC07/test.st"
#define UC01_PRG_PATH    "use_cases/UC01/hello.prg"

int main(void)
{
    st_machine_t        tMachine;
    const load_state_t *ptState;

    printf("=== UC7: load command (load.h/load.c + P11 dir) ===\n\n");

    /* ================================================================
     * BLOCK 1 — lifecycle: load_init / load_shutdown
     * ================================================================ */

    /* INTENT[INT-LOD-001 → TC-LOD-001 → REQ-LOD-001]:
     * load_init() with NULL must be rejected. */
    UC_TEST("[R] load_init(NULL) → ST_ERROR",
            load_init(NULL) == ST_ERROR);

    /* INTENT[INT-LOD-001 → TC-LOD-002 → REQ-LOD-001]:
     * load_init() with a valid machine must succeed and reset state. */
    memset(&tMachine, 0, sizeof(tMachine));
    tMachine.bPoweredOn = ST_TRUE;
    UC_TEST("[N] load_init(valid) → ST_NO_ERROR",
            load_init(&tMachine) == ST_NO_ERROR);

    /* INTENT[INT-LOD-002 → TC-LOD-003 → REQ-LOD-002]:
     * After load_init(), get_state reports nothing loaded. */
    ptState = load_get_state();
    UC_TEST("[N] get_state != NULL",
            ptState != NULL);
    UC_TEST("[N] get_state after init: bLoaded=FALSE",
            ptState->bLoaded == ST_FALSE);
    UC_TEST("[N] get_state after init: eType=NONE",
            ptState->eType == LOAD_TYPE_NONE);

    /* INTENT[INT-LOD-001 → TC-LOD-004 → REQ-LOD-001]:
     * load_shutdown() resets the module cleanly. */
    UC_TEST("[N] load_shutdown() → ST_NO_ERROR",
            load_shutdown() == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] get_state after shutdown: bLoaded=FALSE",
            ptState->bLoaded == ST_FALSE);

    /* INTENT[INT-LOD-001 → TC-LOD-005 → REQ-LOD-001]:
     * Double shutdown is idempotent (no crash, ST_NO_ERROR). */
    UC_TEST("[R] double load_shutdown() → ST_NO_ERROR",
            load_shutdown() == ST_NO_ERROR);

    /* ================================================================
     * BLOCK 2 — load_file without prior load_init
     * ================================================================ */

    /* INTENT[INT-LOD-003 → TC-LOD-006 → REQ-LOD-003]:
     * Calling load_file before load_init must fail. */
    UC_TEST("[R] load_file without init → ST_ERROR",
            load_file(UC07_TXT_PATH) == ST_ERROR);

    /* ================================================================
     * Re-init for remaining tests
     * ================================================================ */

    memset(&tMachine, 0, sizeof(tMachine));
    tMachine.bPoweredOn = ST_TRUE;
    UC_TEST("[N] load_init re-init → ST_NO_ERROR",
            load_init(&tMachine) == ST_NO_ERROR);

    /* ================================================================
     * BLOCK 3 — NULL and non-existent paths
     * ================================================================ */

    /* INTENT[INT-LOD-003 → TC-LOD-007 → REQ-LOD-003]:
     * NULL szPath is always rejected. */
    UC_TEST("[R] load_file(NULL) → ST_ERROR",
            load_file(NULL) == ST_ERROR);

    /* INTENT[INT-LOD-003 → TC-LOD-008 → REQ-LOD-004]:
     * Non-existent path must return ST_ERROR. */
    UC_TEST("[R] load_file non-existent → ST_ERROR",
            load_file("use_cases/UC07/no_such_file.bin") == ST_ERROR);

    /* ================================================================
     * BLOCK 4 — binary load (hello.bin, 16 bytes 0x00..0x0F)
     * ================================================================ */

    /* INTENT[INT-LOD-004 → TC-LOD-009 → REQ-LOD-005,REQ-LOD-006]:
     * A plain binary file is loaded at ST_LOAD_BASE as LOAD_TYPE_BINARY
     * with content copied verbatim into ST RAM. */
    UC_TEST("[N] load_file binary → ST_NO_ERROR",
            load_file(UC07_BIN_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] binary: bLoaded=TRUE",
            ptState->bLoaded == ST_TRUE);
    UC_TEST("[N] binary: eType=LOAD_TYPE_BINARY",
            ptState->eType == LOAD_TYPE_BINARY);
    UC_TEST("[N] binary: uiLoadAddr=ST_LOAD_BASE",
            ptState->uiLoadAddr == ST_LOAD_BASE);
    UC_TEST("[N] binary: uiSize=16",
            ptState->uiSize == 16u);
    UC_TEST("[N] binary: szPath contains 'hello.bin'",
            strstr(ptState->szPath, "hello.bin") != NULL);

    /* Verify verbatim content: hello.bin = 0x00..0x0F */
    UC_TEST("[N] binary: RAM[LOAD_BASE+0] == 0x00",
            tMachine.aRam[ST_LOAD_BASE + 0] == 0x00u);
    UC_TEST("[N] binary: RAM[LOAD_BASE+7] == 0x07",
            tMachine.aRam[ST_LOAD_BASE + 7] == 0x07u);
    UC_TEST("[N] binary: RAM[LOAD_BASE+15] == 0x0F",
            tMachine.aRam[ST_LOAD_BASE + 15] == 0x0Fu);

    /* ================================================================
     * BLOCK 5 — text load (hello.txt)
     * ================================================================ */

    /* INTENT[INT-LOD-004 → TC-LOD-010 → REQ-LOD-005]:
     * A text file (no special extension) is treated as LOAD_TYPE_BINARY. */
    UC_TEST("[N] load_file txt → ST_NO_ERROR",
            load_file(UC07_TXT_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] txt: eType=LOAD_TYPE_BINARY",
            ptState->eType == LOAD_TYPE_BINARY);
    UC_TEST("[N] txt: RAM[LOAD_BASE] == 'H' (start of 'Hello')",
            tMachine.aRam[ST_LOAD_BASE] == (st_u8_t)'H');

    /* ================================================================
     * BLOCK 6 — PRG load (hello.prg: magic 0x601A, 4-byte .text)
     * ================================================================ */

    /* INTENT[INT-LOD-005 → TC-LOD-011 → REQ-LOD-007,REQ-LOD-008]:
     * Valid PRG: magic validated, .text+.data loaded at ST_LOAD_BASE. */
    memset(tMachine.aRam, 0, sizeof(tMachine.aRam));
    UC_TEST("[N] load_file PRG → ST_NO_ERROR",
            load_file(UC01_PRG_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] PRG: eType=LOAD_TYPE_PRG",
            ptState->eType == LOAD_TYPE_PRG);
    UC_TEST("[N] PRG: bLoaded=TRUE",
            ptState->bLoaded == ST_TRUE);
    UC_TEST("[N] PRG: uiLoadAddr=ST_LOAD_BASE",
            ptState->uiLoadAddr == ST_LOAD_BASE);
    /* hello.prg: 4-byte .text (MOVEQ #42,D0 + RTS), 0-byte .data */
    UC_TEST("[N] PRG: uiSize==4 (.text only, no .data)",
            ptState->uiSize == 4u);

    /* Verify .text: first word is MOVEQ #42,D0 = 0x702A */
    UC_TEST("[N] PRG: RAM[LOAD_BASE+0] == 0x70 (MOVEQ opcode hi)",
            tMachine.aRam[ST_LOAD_BASE + 0] == 0x70u);
    UC_TEST("[N] PRG: RAM[LOAD_BASE+1] == 0x2A (MOVEQ #42)",
            tMachine.aRam[ST_LOAD_BASE + 1] == 0x2Au);

    /* ================================================================
     * BLOCK 7 — load replaces previous state
     * ================================================================ */

    /* INTENT[INT-LOD-006 → TC-LOD-012 → REQ-LOD-009]:
     * A new successful load replaces the previous load state. */
    UC_TEST("[N] second load (bin) replaces PRG state",
            load_file(UC07_BIN_PATH) == ST_NO_ERROR);
    ptState = load_get_state();
    UC_TEST("[N] state is now BINARY (replaced PRG)",
            ptState->eType == LOAD_TYPE_BINARY);

    /* ================================================================
     * BLOCK 8 — rejected: directory and disk image
     * ================================================================ */

    /* INTENT[INT-LOD-007 → TC-LOD-013 → REQ-LOD-010]:
     * Directories must be rejected (use mount). */
    UC_TEST("[N] load_file dir → ST_ERROR",
            load_file("use_cases/UC07") == ST_ERROR);

    /* INTENT[INT-LOD-007 → TC-LOD-014 → REQ-LOD-011]:
     * .st disk images must be rejected (use mount). */
    UC_TEST("[N] load_file .st disk image → ST_ERROR",
            load_file(UC07_ST_PATH) == ST_ERROR);

    /* ================================================================
     * BLOCK 9 — bad PRG magic
     * ================================================================ */

    /* INTENT[INT-LOD-005 → TC-LOD-015 → REQ-LOD-008]:
     * PRG with bad magic is rejected; previous state is preserved. */
    UC_TEST("[R] load_file bad PRG magic → ST_ERROR",
            load_file(UC07_BADPRG_PATH) == ST_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] state unchanged after failed PRG load (still BINARY)",
            ptState->eType == LOAD_TYPE_BINARY);

    /* ================================================================
     * BLOCK 10 — shutdown and invariants
     * ================================================================ */

    UC_TEST("[N] load_shutdown() final → ST_NO_ERROR",
            load_shutdown() == ST_NO_ERROR);

    /* INTENT[INT-LOD-002 → TC-LOD-016 → REQ-LOD-002]:
     * load_get_state() never returns NULL (safe at any time). */
    UC_TEST("[N] load_get_state() != NULL after shutdown",
            load_get_state() != NULL);

    /* ================================================================
     * BLOCK 11 — [S] P11 visual indicator (manual only)
     * ================================================================ */

    TEST_MANUAL(
        "[S] P11: ENTER on file → green secondary background",
        "Open 'dir', press ENTER on a file. Does it show a dark green "
        "background on that row (distinct from the nav-cursor blue) ?");

    TEST_MANUAL(
        "[S] P11: cursor away keeps green on committed row",
        "After ENTER on a file, move cursor with arrows. Does the "
        "previously selected file keep its green tint ?");

    TEST_MANUAL(
        "[S] P11: SPACE also marks secondary selection",
        "Navigate to a file, press SPACE. Does that file get the "
        "dark green secondary background ?");

    TEST_MANUAL(
        "[S] P11: new ENTER moves green indicator",
        "With a green-highlighted row, press ENTER on a different file. "
        "Does the green move to the new selection ?");

    /* ================================================================
     * Results
     * ================================================================ */

    printf("\n");
    if (g_uc_fails == 0)
    {
        printf("  All tests PASSED.\n\n");
    }
    else
    {
        printf("  %d test(s) FAILED.\n\n", g_uc_fails);
    }

    return (g_uc_fails == 0) ? 0 : 1;
}
