/*
 * use_case_24E.c - Console evolution: P50/P51/P52/P54/P55/P56
 *
 * TEST MATRIX - UC24E:
 *   [N] Nominal    : 15 tests - CMD_SCRIPT in table, 'r' alias present,
 *                               'script' completion match, CON_KEY_CTRL_O
 *                               == 0x0F, 'umount' absent from table,
 *                               batch 'where' runs OK, batch 'dir --select
 *                               use_cases' sets selected, batch 'image
 *                               --dir' no-source runs OK, 'edit' accepts
 *                               -h/--hex (no crash via batch), CMD_SCRIPT
 *                               > CMD_MOUNT, script missing-arg batch OK
 *   [R] Robustness :  4 tests - 'umount' completion 0 matches, 'u' prefix
 *                               0 matches, missing script ST_ERROR, batch
 *                               dir --select nonexistent no crash
 *   [S] Skipped    :  0 tests
 *
 * Module-level traceability:
 *   UFR-CON-080..087 -> REQ-CON-100..114 -> TC-CON-100..118
 *   -> INT-CON-100..118
 *
 * INTENT[INT-CON-100..118 -> TC-CON-100..118 -> REQ-CON-100..114
 *        -> UFR-CON-080..087]:
 *   The console must support: headless file selection (dir --select),
 *   interactive script execution (script <file>), CTRL+O shortcut for
 *   mount, forced hex editing (edit -h/--hex), and image extraction to
 *   directory (image --dir). The umount command must be removed.
 */

#include "use_cases.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ST_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/stat.h>
#endif

int g_uc_fails = 0;

/* Helper: write a small script file */
static st_bool_t uc24e_write_script(const char *szPath,
                                      const char *szContent)
{
    FILE *pF = fopen(szPath, "w");
    if (pF == NULL)
        return ST_FALSE;
    fputs(szContent, pF);
    fclose(pF);
    return ST_TRUE;
}

/* Helper: run a one-shot batch script in a fresh context */
static st_error_t uc24e_run_script(const char *szScriptPath)
{
    line_context_t tCtx;
    st_error_t     eResult;

    if (line_init(&tCtx) != ST_NO_ERROR)
        return ST_ERROR;

    strncpy(tCtx.szScriptFile, szScriptPath, ST_MAX_PATH - 1);
    tCtx.szScriptFile[ST_MAX_PATH - 1] = '\0';
    eResult = line_run(&tCtx);
    line_shutdown(&tCtx);
    return eResult;
}

/* Helper: run a batch script and capture the post-run selected path */
static st_error_t uc24e_run_and_get_sel(const char *szScriptPath,
                                          char       *szSel,
                                          size_t      uiSelLen)
{
    line_context_t tCtx;
    st_error_t     eResult;

    if (line_init(&tCtx) != ST_NO_ERROR)
        return ST_ERROR;

    strncpy(tCtx.szScriptFile, szScriptPath, ST_MAX_PATH - 1);
    tCtx.szScriptFile[ST_MAX_PATH - 1] = '\0';
    eResult = line_run(&tCtx);
    if (szSel != NULL)
        line_get_selected(&tCtx, szSel, uiSelLen);
    line_shutdown(&tCtx);
    return eResult;
}

/* ------------------------------------------------------------------ */

static void test_p52_ctrl_o(void)
{
    printf("\n--- P52: CTRL+O shortcut for mount ---\n");

    /* INTENT[INT-CON-100 -> TC-CON-155 -> REQ-CON-037 -> UFR-CON-096]:
     * CTRL+O raw byte value is 0x0F; replaces removed CTRL+U (umount). */
    TEST_ASSERT("[N] CON_KEY_CTRL_O == 0x0F",
                CON_KEY_CTRL_O == 0x0F);
}

/* ------------------------------------------------------------------ */

static void test_p55_umount_removed(void)
{
    char aCands[LINE_COMPLETE_MAX][ST_MAX_PATH];
    int  iCount;

    printf("\n--- P55: umount command removed ---\n");

    /* INTENT[INT-CON-101 -> TC-CON-156 -> REQ-CON-040 -> UFR-CON-099]:
     * 'umount' is no longer a recognised command. */
    iCount = line_complete_cmd_query("umount", aCands, LINE_COMPLETE_MAX);
    TEST_ASSERT("[R] 'umount' not in command table (0 matches)",
                iCount == 0);

    /* INTENT[INT-CON-102 -> TC-CON-157 -> REQ-CON-040 -> UFR-CON-099]:
     * 'u' prefix returns no candidates (umount was the only u-command). */
    iCount = line_complete_cmd_query("u", aCands, LINE_COMPLETE_MAX);
    TEST_ASSERT("[R] 'u' prefix returns 0 commands (umount gone)",
                iCount == 0);
}

/* ------------------------------------------------------------------ */

static void test_p51_script_table(void)
{
    char aCands[LINE_COMPLETE_MAX][ST_MAX_PATH];
    int  iCount;

    printf("\n--- P51: script command in table ---\n");

    /* INTENT[INT-CON-103 -> TC-CON-158 -> REQ-CON-036 -> UFR-CON-095]:
     * 'script' appears in command completion. */
    iCount = line_complete_cmd_query("script", aCands, LINE_COMPLETE_MAX);
    TEST_ASSERT("[N] 'script' found in command table",
                iCount > 0);

    /* INTENT[INT-CON-104 -> TC-CON-159 -> REQ-CON-036 -> UFR-CON-095]:
     * 'r' is the short alias for script and the only r-prefix command. */
    iCount = line_complete_cmd_query("r", aCands, LINE_COMPLETE_MAX);
    TEST_ASSERT("[N] 'r' prefix resolves to exactly 1 match (script)",
                iCount == 1);
    if (iCount == 1)
        TEST_ASSERT("[N] 'r' match is 'script'",
                    strcmp(aCands[0], "script") == 0);
    else
        TEST_SKIP("[N] 'r' match content (no candidate)");

    /* INTENT[INT-CON-105 -> TC-CON-160 -> REQ-CON-036 -> UFR-CON-095]:
     * CMD_SCRIPT was inserted after CMD_MOUNT without renumbering. */
    TEST_ASSERT("[N] CMD_SCRIPT > CMD_MOUNT (enum order preserved)",
                (int)CMD_SCRIPT > (int)CMD_MOUNT);
}

/* ------------------------------------------------------------------ */

static void test_p51_script_run(void)
{
    st_error_t eResult;
    const char *szGood    = "use_cases/UC24E/batch_where.txt";
    const char *szMissing = "use_cases/UC24E/nonexistent_xyz.txt";

    printf("\n--- P51: script batch execution ---\n");

    /* INTENT[INT-CON-106 -> TC-CON-161 -> REQ-CON-036 -> UFR-CON-095]:
     * Batch script with valid commands runs to completion. */
    if (uc24e_write_script(szGood, "# comment\nwhere\n") == ST_TRUE)
    {
        eResult = uc24e_run_script(szGood);
        TEST_ASSERT("[N] batch 'where' script -> ST_NO_ERROR",
                    eResult == ST_NO_ERROR);
        remove(szGood);
    }
    else
    {
        TEST_SKIP("[N] batch 'where' (cannot create file)");
    }

    /* INTENT[INT-CON-107 -> TC-CON-162 -> REQ-CON-036 -> UFR-CON-095]:
     * Missing script file returns ST_ERROR. */
    eResult = uc24e_run_script(szMissing);
    TEST_ASSERT("[R] batch missing script -> ST_ERROR",
                eResult == ST_ERROR);
}

/* ------------------------------------------------------------------ */

static void test_p50_dir_select(void)
{
    char       szSel[ST_MAX_PATH];
    st_error_t eResult;
    const char *szScript      = "use_cases/UC24E/dir_select.txt";
    const char *szScriptBad   = "use_cases/UC24E/dir_select_bad.txt";

    printf("\n--- P50: dir --select headless ---\n");

    /* INTENT[INT-CON-108 -> TC-CON-163 -> REQ-CON-035 -> UFR-CON-094]:
     * dir --select <existing path> sets selection without GUI. */
    if (uc24e_write_script(szScript,
                            "dir --select use_cases\n") == ST_TRUE)
    {
        szSel[0] = '\0';
        eResult  = uc24e_run_and_get_sel(szScript, szSel, sizeof(szSel));
        TEST_ASSERT("[N] 'dir --select use_cases' -> ST_NO_ERROR",
                    eResult == ST_NO_ERROR);
        TEST_ASSERT("[N] selected path contains 'use_cases'",
                    strstr(szSel, "use_cases") != NULL);
        remove(szScript);
    }
    else
    {
        TEST_SKIP("[N] dir --select (cannot create file)");
    }

    /* INTENT[INT-CON-109 -> TC-CON-164 -> REQ-CON-035 -> UFR-CON-094]:
     * dir --select nonexistent path -> warning + ST_NO_ERROR (no crash). */
    if (uc24e_write_script(szScriptBad,
                            "dir --select /nonexistent_xyz_abc\n") == ST_TRUE)
    {
        eResult = uc24e_run_script(szScriptBad);
        TEST_ASSERT("[R] 'dir --select nonexistent' -> ST_NO_ERROR (no crash)",
                    eResult == ST_NO_ERROR);
        remove(szScriptBad);
    }
    else
    {
        TEST_SKIP("[R] dir --select nonexistent (cannot create file)");
    }
}

/* ------------------------------------------------------------------ */

static void test_p54_edit_hex_flag(void)
{
    char       aCands[LINE_COMPLETE_MAX][ST_MAX_PATH];
    st_error_t eResult;
    const char *szScript = "use_cases/UC24E/edit_hex.txt";
    int        iCount;

    printf("\n--- P54: edit -h/--hex force hex ---\n");

    /* INTENT[INT-CON-110 -> TC-CON-165 -> REQ-CON-038 -> UFR-CON-097]:
     * 'edit' still in command table after P55 changes. */
    iCount = line_complete_cmd_query("edit", aCands, LINE_COMPLETE_MAX);
    TEST_ASSERT("[N] 'edit' still in command table",
                iCount > 0);

    /* INTENT[INT-CON-111 -> TC-CON-166 -> REQ-CON-038 -> UFR-CON-097]:
     * 'edit -h file' with nonexistent file -> graceful error, no crash. */
    if (uc24e_write_script(szScript,
                            "edit -h nonexistent_xyz.txt\n") == ST_TRUE)
    {
        eResult = uc24e_run_script(szScript);
        TEST_ASSERT("[N] 'edit -h nonexistent' -> ST_NO_ERROR (graceful)",
                    eResult == ST_NO_ERROR);
        remove(szScript);
    }
    else
    {
        TEST_SKIP("[N] edit -h test (cannot create file)");
    }

    /* INTENT[INT-CON-111 -> TC-CON-167 -> REQ-CON-038 -> UFR-CON-097]:
     * 'edit --hex file' is equivalent to 'edit -h file'. */
    if (uc24e_write_script(szScript,
                            "edit --hex nonexistent_xyz.txt\n") == ST_TRUE)
    {
        eResult = uc24e_run_script(szScript);
        TEST_ASSERT("[N] 'edit --hex nonexistent' -> ST_NO_ERROR (graceful)",
                    eResult == ST_NO_ERROR);
        remove(szScript);
    }
    else
    {
        TEST_SKIP("[N] edit --hex test (cannot create file)");
    }
}

/* ------------------------------------------------------------------ */

static void test_p56_image_dir(void)
{
    st_error_t eResult;
    const char *szScript  = "use_cases/UC24E/image_dir.txt";
    const char *szScript2 = "use_cases/UC24E/image_dir2.txt";

    printf("\n--- P56: image --dir extraction ---\n");

    /* INTENT[INT-CON-112 -> TC-CON-168 -> REQ-CON-039 -> UFR-CON-098]:
     * 'image --dir' with no source prints warning + ST_NO_ERROR. */
    if (uc24e_write_script(szScript, "image --dir\n") == ST_TRUE)
    {
        eResult = uc24e_run_script(szScript);
        TEST_ASSERT("[N] 'image --dir' no source -> ST_NO_ERROR",
                    eResult == ST_NO_ERROR);
        remove(szScript);
    }
    else
    {
        TEST_SKIP("[N] image --dir no-source (cannot create file)");
    }

    /* INTENT[INT-CON-113 -> TC-CON-169 -> REQ-CON-039 -> UFR-CON-098]:
     * 'image --dir dest' with no source also returns ST_NO_ERROR. */
    if (uc24e_write_script(szScript2, "image --dir /tmp/testout\n") == ST_TRUE)
    {
        eResult = uc24e_run_script(szScript2);
        TEST_ASSERT("[N] 'image --dir dest' no source -> ST_NO_ERROR",
                    eResult == ST_NO_ERROR);
        remove(szScript2);
    }
    else
    {
        TEST_SKIP("[N] image --dir dest no-source (cannot create file)");
    }
}

/* ------------------------------------------------------------------ */

int main(void)
{
    int iResult;

    /* Create test output directory */
#ifdef ST_PLATFORM_WINDOWS
    CreateDirectoryA("use_cases/UC24E", NULL);
#else
    mkdir("use_cases/UC24E", 0755);
#endif

    printf("=== UC24E: Console evolution "
           "(P50/P51/P52/P54/P55/P56) ===\n");

    test_p52_ctrl_o();
    test_p55_umount_removed();
    test_p51_script_table();
    test_p51_script_run();
    test_p50_dir_select();
    test_p54_edit_hex_flag();
    test_p56_image_dir();

    printf("\n=== UC24E: %d failure(s) ===\n", g_uc_fails);
    iResult = (g_uc_fails == 0) ? 0 : 1;
    return iResult;
}
