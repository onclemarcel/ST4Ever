/*
 * use_case_15.c - UC15: PRG full load — header parse, BSS clear,
 *                 fixup relocation (load.h / load.c).
 *
 * Tests run from the project root (make tests requirement — R12).
 *
 * Fixtures:
 *   use_cases/UC01/hello.prg       — valid PRG, no fixups, 4-byte .text
 *   use_cases/UC15/fixup.prg       — 1 fixup: JMP abs.L $0 → $0800 after reloc
 *   use_cases/UC15/bss.prg         — .text=RTS, bss=16, no fixups
 *   use_cases/UC15/absolute.prg    — abs_flag=1, no fixup table in file
 *   use_cases/UC15/multi_fixup.prg — 2 fixups at offsets 2 and 8 in .text
 *   use_cases/UC15/bad_fixup.prg   — fixup offset=100 out of range → ST_ERROR
 *
 * TEST MATRIX - UC15:
 *   [N] Nominal    : 33 tests — PRG no-fixups / UC7 regression (7);
 *                               single fixup relocation (8);
 *                               BSS zeroing (5); absolute flag (4);
 *                               multiple fixups (6); state fields (2);
 *                               load_shutdown nominal (1)
 *   [R] Robustness :  5 tests — bad fixup offset + state preserved (2);
 *                               NULL path (1); non-existent file (1);
 *                               load_file after shutdown (1)
 *   [S] Skipped    :  0 tests
 *
 * Traceability:
 *   INT-LOD-009..015 → TC-LOD-018..055 → REQ-LOD-015..022 → UFR-LOD-004,006..008
 *
 * Note: The UC7 regression for hello.prg (Block 1) exercises the same
 * load_do_prg() code path — the stub TODO(UC15) is now the real implementation.
 * TC-LOD-012 (ADAPTED:UC15) — uiSize for hello.prg still == 4 because bss=0.
 */

#include "use_cases.h"
#include "../src/load.h"

int g_uc_fails = 0;

#define UC01_PRG_PATH       "use_cases/UC01/hello.prg"
#define UC15_FIXUP_PATH     "use_cases/UC15/fixup.prg"
#define UC15_BSS_PATH       "use_cases/UC15/bss.prg"
#define UC15_ABS_PATH       "use_cases/UC15/absolute.prg"
#define UC15_MULTI_PATH     "use_cases/UC15/multi_fixup.prg"
#define UC15_BADFX_PATH     "use_cases/UC15/bad_fixup.prg"

int main(void)
{
    st_machine_t        tMachine;
    const load_state_t *ptState;
    int                 i;

    printf("=== UC15: PRG full load — header, BSS, fixup relocation ===\n\n");

    memset(&tMachine, 0, sizeof(tMachine));
    tMachine.bPoweredOn = ST_TRUE;

    /* ================================================================
     * BLOCK 1 — UC7 regression: hello.prg (no fixups, first_offset=0)
     * ================================================================ */

    /* INTENT[INT-LOD-007 → TC-LOD-018 → REQ-LOD-015 → UFR-LOD-004]:
     * hello.prg has no fixups (fixup header first_offset == 0).  The new
     * load_do_prg() must still load the sections correctly and leave the
     * RAM content unchanged (uiFixupCount == 0). */
    UC_TEST("[N] load_init → ST_NO_ERROR",
            load_init(&tMachine) == ST_NO_ERROR);

    memset(tMachine.aRam, 0xAA, sizeof(tMachine.aRam)); /* sentinel fill */

    UC_TEST("[N] hello.prg (no fixups) → ST_NO_ERROR",
            load_file(UC01_PRG_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] hello.prg: eType=LOAD_TYPE_PRG",
            ptState->eType == LOAD_TYPE_PRG);
    /* ADAPTED: UC15 — uiSize now = text+data+bss; hello.prg bss=0 so still 4 */
    UC_TEST("[N] hello.prg: uiSize=4 (text=4, data=0, bss=0)",
            ptState->uiSize == 4u);
    UC_TEST("[N] hello.prg: uiFixupCount=0",
            ptState->uiFixupCount == 0u);
    UC_TEST("[N] hello.prg: RAM[LOAD_BASE+0]=0x70 (MOVEQ opcode)",
            tMachine.aRam[ST_LOAD_BASE + 0] == 0x70u);
    UC_TEST("[N] hello.prg: RAM[LOAD_BASE+1]=0x2A (MOVEQ #42)",
            tMachine.aRam[ST_LOAD_BASE + 1] == 0x2Au);

    /* ================================================================
     * BLOCK 2 — single fixup: fixup.prg
     * JMP abs.L $0  →  after relocation: longword@.text[2] = ST_LOAD_BASE
     * ================================================================ */

    /* INTENT[INT-LOD-008 → TC-LOD-019..025 → REQ-LOD-016..017 → UFR-LOD-004]:
     * fixup.prg has 1 fixup at .text offset 2.  After loading at
     * ST_LOAD_BASE (0x0800), the longword stored at RAM[ST_LOAD_BASE+2]
     * must equal 0x00000800 (0x00000000 + 0x0800). */
    memset(tMachine.aRam, 0xAA, sizeof(tMachine.aRam));
    UC_TEST("[N] fixup.prg → ST_NO_ERROR",
            load_file(UC15_FIXUP_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] fixup.prg: eType=PRG",
            ptState->eType == LOAD_TYPE_PRG);
    UC_TEST("[N] fixup.prg: uiTextSize=6",
            ptState->uiTextSize == 6u);
    UC_TEST("[N] fixup.prg: uiDataSize=0",
            ptState->uiDataSize == 0u);
    UC_TEST("[N] fixup.prg: uiFixupCount=1",
            ptState->uiFixupCount == 1u);

    /* JMP opcode at RAM[ST_LOAD_BASE+0..1] must be unchanged */
    UC_TEST("[N] fixup.prg: RAM[LOAD_BASE+0]=0x4E (JMP hi)",
            tMachine.aRam[ST_LOAD_BASE + 0] == 0x4Eu);
    UC_TEST("[N] fixup.prg: RAM[LOAD_BASE+1]=0xF9 (JMP lo)",
            tMachine.aRam[ST_LOAD_BASE + 1] == 0xF9u);

    /* Relocated longword at offset 2: was 0x00000000, now 0x00000800 */
    UC_TEST("[N] fixup.prg: RAM[LOAD_BASE+2..5]=0x00000800 (relocated)",
            tMachine.aRam[ST_LOAD_BASE + 2] == 0x00u &&
            tMachine.aRam[ST_LOAD_BASE + 3] == 0x00u &&
            tMachine.aRam[ST_LOAD_BASE + 4] == 0x08u &&
            tMachine.aRam[ST_LOAD_BASE + 5] == 0x00u);

    /* ================================================================
     * BLOCK 3 — BSS zeroing: bss.prg
     * .text=RTS(2), .bss=16, no fixups; BSS area must be all 0
     * ================================================================ */

    /* INTENT[INT-LOD-009 → TC-LOD-026..030 → REQ-LOD-018 → UFR-LOD-004]:
     * bss.prg has 16-byte BSS.  load_do_prg() must zero the BSS area
     * (ST_LOAD_BASE + text_sz + data_sz .. + bss_sz - 1) regardless of
     * what was in RAM before. */
    memset(tMachine.aRam, 0xFF, sizeof(tMachine.aRam)); /* non-zero sentinel */
    UC_TEST("[N] bss.prg → ST_NO_ERROR",
            load_file(UC15_BSS_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] bss.prg: uiTextSize=2, uiBssSize=16",
            ptState->uiTextSize == 2u && ptState->uiBssSize == 16u);
    /* uiSize = text+data+bss = 2+0+16 = 18 */
    UC_TEST("[N] bss.prg: uiSize=18 (text+bss)",
            ptState->uiSize == 18u);

    /* RTS at .text[0] must be loaded */
    UC_TEST("[N] bss.prg: RAM[LOAD_BASE+0]=0x4E, [1]=0x75 (RTS)",
            tMachine.aRam[ST_LOAD_BASE + 0] == 0x4Eu &&
            tMachine.aRam[ST_LOAD_BASE + 1] == 0x75u);

    /* BSS area (offsets 2..17 from LOAD_BASE) must be zeroed */
    {
        int bBssOk = 1;
        for (i = 2; i < 18; i++)
        {
            if (tMachine.aRam[ST_LOAD_BASE + i] != 0x00u)
            {
                bBssOk = 0;
                break;
            }
        }
        UC_TEST("[N] bss.prg: BSS area (offsets 2..17) all zeroed",
                bBssOk);
    }

    /* ================================================================
     * BLOCK 4 — absolute PRG: absolute.prg
     * abs_flag=1 → no fixup table in file, no relocation applied
     * ================================================================ */

    /* INTENT[INT-LOD-010 → TC-LOD-031..034 → REQ-LOD-019 → UFR-LOD-004]:
     * When abs_flag == 1, load_do_prg() must skip fixup processing
     * entirely (the fixup table is absent from the file).
     * uiFixupCount must remain 0. */
    memset(tMachine.aRam, 0xAA, sizeof(tMachine.aRam));
    UC_TEST("[N] absolute.prg → ST_NO_ERROR",
            load_file(UC15_ABS_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] absolute.prg: eType=PRG",
            ptState->eType == LOAD_TYPE_PRG);
    UC_TEST("[N] absolute.prg: uiFixupCount=0 (abs, no fixups)",
            ptState->uiFixupCount == 0u);
    UC_TEST("[N] absolute.prg: RAM[LOAD_BASE+0..1]=RTS",
            tMachine.aRam[ST_LOAD_BASE + 0] == 0x4Eu &&
            tMachine.aRam[ST_LOAD_BASE + 1] == 0x75u);

    /* ================================================================
     * BLOCK 5 — multiple fixups: multi_fixup.prg
     * 2 fixups at .text offsets 2 and 8 (advance byte = 6 between them)
     * pre-reloc: [2..5]=0x00000000, [8..11]=0x00000006
     * post-reloc: [2..5]=0x00000800, [8..11]=0x00000806
     * ================================================================ */

    /* INTENT[INT-LOD-011 → TC-LOD-035..041 → REQ-LOD-020 → UFR-LOD-004]:
     * Two fixups in sequence; each longword must be independently
     * incremented by ST_LOAD_BASE.  The advance byte (0x06) correctly
     * positions the second fixup from the first. */
    memset(tMachine.aRam, 0xAA, sizeof(tMachine.aRam));
    UC_TEST("[N] multi_fixup.prg → ST_NO_ERROR",
            load_file(UC15_MULTI_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] multi_fixup.prg: uiTextSize=12",
            ptState->uiTextSize == 12u);
    UC_TEST("[N] multi_fixup.prg: uiFixupCount=2",
            ptState->uiFixupCount == 2u);

    /* First fixup at offset 2: was 0x00000000 → 0x00000800 */
    UC_TEST("[N] multi_fixup: RAM[LOAD_BASE+2..5]=0x00000800",
            tMachine.aRam[ST_LOAD_BASE + 2] == 0x00u &&
            tMachine.aRam[ST_LOAD_BASE + 3] == 0x00u &&
            tMachine.aRam[ST_LOAD_BASE + 4] == 0x08u &&
            tMachine.aRam[ST_LOAD_BASE + 5] == 0x00u);

    /* Second fixup at offset 8: was 0x00000006 → 0x00000806 */
    UC_TEST("[N] multi_fixup: RAM[LOAD_BASE+8..11]=0x00000806",
            tMachine.aRam[ST_LOAD_BASE +  8] == 0x00u &&
            tMachine.aRam[ST_LOAD_BASE +  9] == 0x00u &&
            tMachine.aRam[ST_LOAD_BASE + 10] == 0x08u &&
            tMachine.aRam[ST_LOAD_BASE + 11] == 0x06u);

    /* Opcodes between fixups (offset 6-7 = second JMP opcode) unchanged */
    UC_TEST("[N] multi_fixup: RAM[LOAD_BASE+6..7]=JMP opcode unchanged",
            tMachine.aRam[ST_LOAD_BASE + 6] == 0x4Eu &&
            tMachine.aRam[ST_LOAD_BASE + 7] == 0xF9u);

    /* ================================================================
     * BLOCK 6 — state fields from last load (multi_fixup.prg)
     * ================================================================ */

    /* INTENT[INT-LOD-012 → TC-LOD-042 → REQ-LOD-021 → UFR-LOD-004]:
     * uiLoadAddr is always ST_LOAD_BASE; uiSize = text+data+bss. */
    UC_TEST("[N] multi_fixup: uiLoadAddr=ST_LOAD_BASE",
            ptState->uiLoadAddr == ST_LOAD_BASE);
    UC_TEST("[N] multi_fixup: uiSize=12 (text=12, data=0, bss=0)",
            ptState->uiSize == 12u);

    /* ================================================================
     * BLOCK 7 — robustness
     * ================================================================ */

    /* INTENT[INT-LOD-012 → TC-LOD-043 → REQ-LOD-022 → UFR-LOD-004]:
     * A fixup offset that points beyond .text+.data must be rejected
     * with ST_ERROR; the previous load state must be preserved. */
    memset(tMachine.aRam, 0xAA, sizeof(tMachine.aRam));
    /* Load a good file first to set a known state */
    load_file(UC01_PRG_PATH);
    UC_TEST("[R] bad_fixup.prg → ST_ERROR",
            load_file(UC15_BADFX_PATH) == ST_ERROR);

    ptState = load_get_state();
    UC_TEST("[R] state unchanged after bad fixup (still hello.prg)",
            ptState->eType == LOAD_TYPE_PRG &&
            ptState->uiTextSize == 4u);

    /* UC7 regression robustness */
    UC_TEST("[R] load_file(NULL) → ST_ERROR",
            load_file(NULL) == ST_ERROR);
    UC_TEST("[R] load_file non-existent → ST_ERROR",
            load_file("use_cases/UC15/no_such.prg") == ST_ERROR);

    UC_TEST("[N] load_shutdown → ST_NO_ERROR",
            load_shutdown() == ST_NO_ERROR);
    UC_TEST("[R] load_file after shutdown → ST_ERROR",
            load_file(UC15_FIXUP_PATH) == ST_ERROR);

    /* ================================================================
     * Results
     * ================================================================ */

    printf("\n");
    if (g_uc_fails == 0)
        printf("  All tests PASSED.\n\n");
    else
        printf("  %d test(s) FAILED.\n\n", g_uc_fails);

    return (g_uc_fails == 0) ? 0 : 1;
}
