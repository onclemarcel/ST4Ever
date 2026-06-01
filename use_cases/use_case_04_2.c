/*
 * use_case_04_2.c - UC4.2: raw console input + rich line editor
 *
 * Tests console_set_raw() / console_restore() / console_read_key()
 * and the line editor behaviour (line_read_rich, CTRL shortcuts,
 * cursor movement, Backspace/Delete).
 *
 * Most editor behaviour requires a live terminal and user interaction,
 * so the bulk of the validation is under TEST_MANUAL (make manual).
 *
 * TEST MATRIX - UC4.2:
 *   [N] Nominal    : 4 tests  - set_raw/restore roundtrip (when TTY),
 *                               restore without prior set (idempotent),
 *                               CON_KEY_* constant sanity
 *   [R] Robustness : 2 tests  - console_read_key(NULL) -> ST_ERROR,
 *                               console_restore() without set_raw
 *   [S] Skipped    : 8 tests  - run make manual (interactive terminal)
 */

#include "use_cases.h"

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * INTENT[INT-CON-001 -> TC-CON-001 -> REQ-CON-001 -> UFR-CON-001]:
 * CON_KEY_* constants must be in their documented ranges so that
 * line_read_rich() switch logic works unambiguously.
 * ------------------------------------------------------------------ */
static void test_key_constants(void)
{
    printf("\n  -- Key constant ranges --\n");

    /* Control codes must be in 0x01..0x1F */
    UC_TEST("[N] CON_KEY_CTRL_C in 0x01..0x1F",
            CON_KEY_CTRL_C >= 0x01 && CON_KEY_CTRL_C <= 0x1F);
    UC_TEST("[N] CON_KEY_ENTER  in 0x01..0x1F",
            CON_KEY_ENTER  >= 0x01 && CON_KEY_ENTER  <= 0x1F);
    UC_TEST("[N] CON_KEY_ESC    == 0x1B",
            CON_KEY_ESC == 0x1B);
    UC_TEST("[N] CON_KEY_BACKSPACE == 0x7F",
            CON_KEY_BACKSPACE == 0x7F);

    /* Extended keys must be >= 0x200 (no overlap with ASCII or control) */
    UC_TEST("[N] CON_KEY_UP    >= 0x200", CON_KEY_UP    >= 0x200);
    UC_TEST("[N] CON_KEY_DOWN  >= 0x200", CON_KEY_DOWN  >= 0x200);
    UC_TEST("[N] CON_KEY_LEFT  >= 0x200", CON_KEY_LEFT  >= 0x200);
    UC_TEST("[N] CON_KEY_RIGHT >= 0x200", CON_KEY_RIGHT >= 0x200);
    UC_TEST("[N] CON_KEY_HOME  >= 0x200", CON_KEY_HOME  >= 0x200);
    UC_TEST("[N] CON_KEY_END   >= 0x200", CON_KEY_END   >= 0x200);
    UC_TEST("[N] CON_KEY_DELETE >= 0x200", CON_KEY_DELETE >= 0x200);
}

/* ------------------------------------------------------------------
 * INTENT[INT-CON-002 -> TC-CON-002 -> REQ-CON-002 -> UFR-CON-002]:
 * console_read_key(NULL) must return ST_ERROR without crashing or
 * blocking, regardless of raw-mode state.
 * ------------------------------------------------------------------ */
static void test_read_key_null(void)
{
    st_error_t eResult;

    printf("\n  -- console_read_key NULL param --\n");

    eResult = console_read_key(NULL);
    UC_TEST("[R] console_read_key(NULL) -> ST_ERROR",
            eResult == ST_ERROR);
}

/* ------------------------------------------------------------------
 * INTENT[INT-CON-003 -> TC-CON-003 -> REQ-CON-003 -> UFR-CON-003]:
 * console_restore() must be idempotent: calling it without a prior
 * successful console_set_raw() must return ST_NO_ERROR (no-op).
 * ------------------------------------------------------------------ */
static void test_restore_idempotent(void)
{
    st_error_t eResult;

    printf("\n  -- console_restore() idempotent --\n");

    /* Fresh state: no raw mode active */
    eResult = console_restore();
    UC_TEST("[R] console_restore() without set_raw -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);

    /* Second call must also succeed */
    eResult = console_restore();
    UC_TEST("[N] console_restore() second call -> ST_NO_ERROR",
            eResult == ST_NO_ERROR);
}

/* ------------------------------------------------------------------
 * INTENT[INT-CON-004 -> TC-CON-004 -> REQ-CON-004 -> UFR-CON-004]:
 * When stdin IS a TTY, console_set_raw() / console_restore() must
 * complete without error.  When stdin is not a TTY (piped, CI) the
 * test is informational only and does not fail.
 * ------------------------------------------------------------------ */
static void test_set_restore_roundtrip(void)
{
    st_error_t eSet;
    st_error_t eRestore;

    printf("\n  -- console_set_raw / restore roundtrip --\n");

    eSet = console_set_raw();
    if (eSet == ST_NO_ERROR)
    {
        eRestore = console_restore();
        UC_TEST("[N] console_restore() after set_raw -> ST_NO_ERROR",
                eRestore == ST_NO_ERROR);
        printf("  [INFO] console_set_raw() succeeded (interactive TTY)\n");
    }
    else
    {
        printf("  [INFO] console_set_raw() returned ST_ERROR "
               "(stdin not a TTY - expected in CI / piped input)\n");
    }
}

/* ------------------------------------------------------------------
 * Interactive / visual tests (TEST_MANUAL — run via make manual)
 * ------------------------------------------------------------------ */
static void test_manual_editor(void)
{
    printf("\n  -- Manual line editor tests (make manual) --\n");

    TEST_MANUAL(
        "[S] Typing characters",
        "Type 'hello' then ENTER. Did 'hello' appear character "
        "by character at the prompt?");

    TEST_MANUAL(
        "[S] Cursor left/right",
        "Type 'abc', press LEFT twice, type 'X'. "
        "Is the result 'aXbc'?");

    TEST_MANUAL(
        "[S] Home / End",
        "Type 'abc', press Home, type 'Z'. "
        "Is the result 'Zabc'? Now press End, type '!'. "
        "Is the result 'Zabc!'?");

    TEST_MANUAL(
        "[S] Backspace",
        "Type 'hello', press Backspace twice. "
        "Is the result 'hel'?");

    TEST_MANUAL(
        "[S] Delete",
        "Type 'hello', press Home, press Delete twice. "
        "Is the result 'llo'?");

    TEST_MANUAL(
        "[S] ESC clears line",
        "Type 'hello', press ESC. "
        "Is the line cleared (prompt only, empty input)?");

    TEST_MANUAL(
        "[S] CTRL+Q quits",
        "Press CTRL+Q. Did 'quit' appear at the prompt and "
        "the application exit cleanly?");

    TEST_MANUAL(
        "[S] CTRL+T opens trace",
        "Press CTRL+T. Did 'trace' appear at the prompt and "
        "the trace toggle execute?");
}

/* ------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("========================================\n");
    printf(" UC4.2 - Raw console + rich line editor\n");
    printf("========================================\n");

    test_key_constants();
    test_read_key_null();
    test_restore_idempotent();
    test_set_restore_roundtrip();
    test_manual_editor();

    printf("\n========================================\n");
    if (g_uc_fails == 0)
    {
        printf(" All tests passed.\n");
    }
    else
    {
        printf(" %d test(s) FAILED.\n", g_uc_fails);
    }
    printf("========================================\n\n");

    return (g_uc_fails == 0) ? 0 : 1;
}
