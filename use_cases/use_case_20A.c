/*
 * use_case_20A.c - Test suite for UC20A
 *
 * Tests:
 *   st2msa: single-file conversion (.st -> .msa) via image APIs
 *   msa2st: single-file conversion (.msa -> .st) via image APIs
 *   Round-trip: .st -> .msa -> .st preserves file content
 *   st2msa --all batch via script: directory scan produces .msa files
 *   msa2st --all batch via script: directory scan produces .st files
 *   Command table: st2msa and msa2st appear in completion
 *   Robustness: extension checks, missing file, empty selection
 *
 * TEST MATRIX - UC20A:
 *   [N] Nominal    : 18 tests - API round-trips, batch via script,
 *                               command table entries, extension path
 *   [R] Robustness :  6 tests - wrong extension warning, missing file,
 *                               non-existent --dir, no selection
 *   [S] Skipped    :  0 tests - no visual tests (headless only)
 */

#include "use_cases.h"
#include "../src/file.h"
#include "../src/image_st.h"
#include "../src/image_msa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Write a .st fixture with one 64-byte file using iSeed pattern */
static st_bool_t fixture_write_st(const char *szPath, int iSeed)
{
    image_st_t *ptImg = NULL;
    st_u8_t     aBuf[64];
    int         i;
    st_bool_t   bOk = ST_FALSE;

    for (i = 0; i < 64; i++) aBuf[i] = (st_u8_t)((iSeed + i) & 0xFF);

    if (image_st_create(&ptImg) != ST_NO_ERROR)
        return ST_FALSE;

    if (image_st_write_file(ptImg, "DATA.PRG", aBuf, 64) == ST_NO_ERROR
    &&  image_st_save(ptImg, szPath) == ST_NO_ERROR)
        bOk = ST_TRUE;

    image_st_close(&ptImg);
    return bOk;
}

/* Write a .msa fixture with one 64-byte file using iSeed pattern */
static st_bool_t fixture_write_msa(const char *szPath, int iSeed)
{
    image_st_t *ptImg = NULL;
    st_u8_t     aBuf[64];
    int         i;
    st_bool_t   bOk = ST_FALSE;

    for (i = 0; i < 64; i++) aBuf[i] = (st_u8_t)((iSeed + i) & 0xFF);

    if (image_st_create(&ptImg) != ST_NO_ERROR)
        return ST_FALSE;

    if (image_st_write_file(ptImg, "DATA.PRG", aBuf, 64) == ST_NO_ERROR
    &&  image_msa_save(ptImg, szPath) == ST_NO_ERROR)
        bOk = ST_TRUE;

    image_st_close(&ptImg);
    return bOk;
}

/* Read DATA.PRG content from a .st file; returns file size or 0 */
static st_u32_t read_st_data_prg(const char  *szPath,
                                  st_u8_t     *pBuf,
                                  st_u32_t     uiBufLen)
{
    image_st_t        *ptImg  = NULL;
    image_st_dirent_t  aDir[8];
    int                iCount = 0;
    st_u32_t           uiSize = 0;

    if (image_st_load(szPath, &ptImg) != ST_NO_ERROR)
        return 0;

    image_st_list_root(ptImg, aDir, 8, &iCount);
    if (iCount == 1 && aDir[0].uiSize <= uiBufLen)
    {
        if (image_st_read_file(ptImg, aDir[0].uiCluster,
                               aDir[0].uiSize,
                               pBuf, uiBufLen) == ST_NO_ERROR)
            uiSize = aDir[0].uiSize;
    }
    image_st_close(&ptImg);
    return uiSize;
}

/* Read DATA.PRG content from a .msa file; returns file size or 0 */
static st_u32_t read_msa_data_prg(const char  *szPath,
                                   st_u8_t     *pBuf,
                                   st_u32_t     uiBufLen)
{
    image_st_t        *ptImg  = NULL;
    image_st_dirent_t  aDir[8];
    int                iCount = 0;
    st_u32_t           uiSize = 0;

    if (image_msa_load(szPath, &ptImg) != ST_NO_ERROR)
        return 0;

    image_st_list_root(ptImg, aDir, 8, &iCount);
    if (iCount == 1 && aDir[0].uiSize <= uiBufLen)
    {
        if (image_st_read_file(ptImg, aDir[0].uiCluster,
                               aDir[0].uiSize,
                               pBuf, uiBufLen) == ST_NO_ERROR)
            uiSize = aDir[0].uiSize;
    }
    image_st_close(&ptImg);
    return uiSize;
}

/* Write a minimal script file */
static st_bool_t write_script(const char *szPath, const char *szContent)
{
    file_t *ptF = NULL;
    if (file_open(szPath, FILE_MODE_WRITE, &ptF) != ST_NO_ERROR)
        return ST_FALSE;
    file_write(ptF, (const st_u8_t *)szContent, (st_u32_t)strlen(szContent));
    file_close(&ptF);
    return ST_TRUE;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    st_machine_t   tMachine;
    line_context_t tLineCtx;
    file_stat_t    tStat;
    st_error_t     eResult;
    st_u8_t        aBufOrig[64];
    st_u8_t        aBufRt[64];
    st_u32_t       uiSize;
    int            iCand;
    char           aaCands[32][ST_MAX_PATH];
    int            i;

    const char *szDir20A    = "use_cases/UC20A";
    const char *szBatchDir  = "use_cases/UC20A/batch";
    /* single-file paths */
    const char *szOneSt     = "use_cases/UC20A/one.st";
    const char *szOneMsa    = "use_cases/UC20A/one.msa";   /* st2msa output */
    const char *szOneStRt   = "use_cases/UC20A/one_rt.st"; /* msa2st round-trip */
    /* batch paths */
    const char *szBatchA_st = "use_cases/UC20A/batch/a.st";
    const char *szBatchB_st = "use_cases/UC20A/batch/b.st";
    const char *szBatchA_ms = "use_cases/UC20A/batch/a.msa";
    const char *szBatchB_ms = "use_cases/UC20A/batch/b.msa";
    const char *szBatchC_ms = "use_cases/UC20A/batch/c.msa";
    const char *szBatchC_st = "use_cases/UC20A/batch/c.st";
    /* script paths */
    const char *szScriptSt  = "use_cases/UC20A/script_st2msa.txt";
    const char *szScriptMs  = "use_cases/UC20A/script_msa2st.txt";
    char        szScriptBuf[ST_MAX_PATH * 4];

    printf("=== UC20A: st2msa / msa2st batch conversion ===\n\n");

    /* --- Module init --- */
    printf("--- Init ---\n");

    if (st_init(&tMachine, NULL) != ST_NO_ERROR)
    {
        printf("  [SKIP] st_init failed\n");
        return 0;
    }
    if (gui_init() != ST_NO_ERROR)
    {
        printf("  [SKIP] gui_init failed\n");
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

    /* Setup fixture directories */
    file_mkdir(szDir20A);
    file_mkdir(szBatchDir);

    /* Reference data pattern */
    for (i = 0; i < 64; i++) aBufOrig[i] = (st_u8_t)((0xCC + i) & 0xFF);

    /* ==== [N] Command table entries ==== */
    printf("\n--- [N] Command table entries ---\n");

    /* INTENT[INT-CON-030 -> TC-CON-080]:
     * st2msa and msa2st shall appear in tab-completion. */
    iCand = line_complete_cmd_query("st2msa", aaCands, 32);
    UC_TEST("[N] st2msa in completion table (full match)", iCand >= 1);

    iCand = line_complete_cmd_query("msa2st", aaCands, 32);
    UC_TEST("[N] msa2st in completion table (full match)", iCand >= 1);

    iCand = line_complete_cmd_query("s", aaCands, 32);
    UC_TEST("[N] 's' shortcut completes to st2msa", iCand >= 1
            && strstr(aaCands[0], "st2msa") != NULL);

    iCand = line_complete_cmd_query("a", aaCands, 32);
    UC_TEST("[N] 'a' shortcut completes to msa2st", iCand >= 1
            && strstr(aaCands[0], "msa2st") != NULL);

    /* ==== [N] Single-file st2msa via image APIs ==== */
    printf("\n--- [N] Single-file st2msa (API level) ---\n");

    /* INTENT[INT-CON-031 -> TC-CON-081]:
     * image_st_load + image_msa_save (the core of st2msa) shall produce
     * a valid .msa file from a .st file. */
    if (!fixture_write_st(szOneSt, 0xCC))
    {
        printf("  [SKIP] .st fixture creation failed\n");
        goto test_script;
    }

    {
        image_st_t *ptImg = NULL;
        eResult = image_st_load(szOneSt, &ptImg);
        UC_TEST("[N] image_st_load .st == ST_NO_ERROR",
                eResult == ST_NO_ERROR);

        if (ptImg != NULL)
        {
            eResult = image_msa_save(ptImg, szOneMsa);
            UC_TEST("[N] image_msa_save .msa == ST_NO_ERROR",
                    eResult == ST_NO_ERROR);
            image_st_close(&ptImg);
        }
        else
        {
            TEST_SKIP("[N] image_msa_save .msa == ST_NO_ERROR");
        }
    }

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szOneMsa, &tStat);
    UC_TEST("[N] .msa output exists", tStat.bExists);
    UC_TEST("[N] .msa output < 737280 (compressed)",
            tStat.bExists && tStat.uiSize < 737280u);

    /* Verify content survives st2msa: read from .msa */
    memset(aBufRt, 0, sizeof(aBufRt));
    uiSize = read_msa_data_prg(szOneMsa, aBufRt, sizeof(aBufRt));
    UC_TEST("[N] st2msa content preserved (size)",  uiSize == 64);
    UC_TEST("[N] st2msa content preserved (bytes)",
            uiSize == 64 && memcmp(aBufOrig, aBufRt, 64) == 0);

    /* ==== [N] Single-file msa2st (round-trip) ==== */
    printf("\n--- [N] Single-file msa2st (round-trip) ---\n");

    /* INTENT[INT-CON-032 -> TC-CON-082]:
     * image_msa_load + image_st_save (the core of msa2st) shall produce
     * a .st identical to the original input. */
    {
        image_st_t *ptImg = NULL;
        eResult = image_msa_load(szOneMsa, &ptImg);
        UC_TEST("[N] image_msa_load .msa == ST_NO_ERROR",
                eResult == ST_NO_ERROR);

        if (ptImg != NULL)
        {
            eResult = image_st_save(ptImg, szOneStRt);
            UC_TEST("[N] image_st_save round-trip .st == ST_NO_ERROR",
                    eResult == ST_NO_ERROR);
            image_st_close(&ptImg);
        }
        else
        {
            TEST_SKIP("[N] image_st_save round-trip .st == ST_NO_ERROR");
        }
    }

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szOneStRt, &tStat);
    UC_TEST("[N] round-trip .st == 737280",
            tStat.bExists && tStat.uiSize == 737280u);

    /* Verify bytes survived the full round-trip .st -> .msa -> .st */
    memset(aBufRt, 0, sizeof(aBufRt));
    uiSize = read_st_data_prg(szOneStRt, aBufRt, sizeof(aBufRt));
    UC_TEST("[N] round-trip content size preserved", uiSize == 64);
    UC_TEST("[N] round-trip content bytes match original",
            uiSize == 64 && memcmp(aBufOrig, aBufRt, 64) == 0);

test_script:
    /* ==== [N] Batch via script ==== */
    printf("\n--- [N] Batch st2msa --all via script ---\n");

    /* INTENT[INT-CON-033 -> TC-CON-083]:
     * st2msa --all --dir <batchdir> shall convert all .st files in that
     * directory to .msa. */
    if (!fixture_write_st(szBatchA_st, 0x11)
    ||  !fixture_write_st(szBatchB_st, 0x22))
    {
        printf("  [SKIP] batch .st fixtures failed\n");
        goto robustness;
    }

    snprintf(szScriptBuf, sizeof(szScriptBuf),
             "st2msa --all --dir %s\nquit\n", szBatchDir);
    if (!write_script(szScriptSt, szScriptBuf))
    {
        printf("  [SKIP] script write failed\n");
        goto robustness;
    }

    /* Run the script via line_run */
    strncpy(tLineCtx.szScriptFile, szScriptSt,
            sizeof(tLineCtx.szScriptFile) - 1);
    tLineCtx.bRunning = ST_TRUE;
    eResult = line_run(&tLineCtx);
    tLineCtx.szScriptFile[0] = '\0';

    UC_TEST("[N] st2msa --all script == ST_NO_ERROR",
            eResult == ST_NO_ERROR);

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szBatchA_ms, &tStat);
    UC_TEST("[N] batch a.msa produced", tStat.bExists);

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szBatchB_ms, &tStat);
    UC_TEST("[N] batch b.msa produced", tStat.bExists);

    printf("\n--- [N] Batch msa2st --all via script ---\n");

    /* INTENT[INT-CON-034 -> TC-CON-084]:
     * msa2st --all --dir <batchdir> shall convert all .msa files. */
    if (!fixture_write_msa(szBatchC_ms, 0x33))
    {
        printf("  [SKIP] batch .msa fixture failed\n");
        goto robustness;
    }

    snprintf(szScriptBuf, sizeof(szScriptBuf),
             "msa2st --all --dir %s\nquit\n", szBatchDir);
    if (!write_script(szScriptMs, szScriptBuf))
    {
        printf("  [SKIP] script write failed\n");
        goto robustness;
    }

    strncpy(tLineCtx.szScriptFile, szScriptMs,
            sizeof(tLineCtx.szScriptFile) - 1);
    tLineCtx.bRunning = ST_TRUE;
    eResult = line_run(&tLineCtx);
    tLineCtx.szScriptFile[0] = '\0';

    UC_TEST("[N] msa2st --all script == ST_NO_ERROR",
            eResult == ST_NO_ERROR);

    memset(&tStat, 0, sizeof(tStat));
    file_stat(szBatchC_st, &tStat);
    UC_TEST("[N] batch c.st produced", tStat.bExists);
    UC_TEST("[N] batch c.st == 737280",
            tStat.bExists && tStat.uiSize == 737280u);

robustness:
    /* ==== [R] Robustness ==== */
    printf("\n--- [R] Robustness ---\n");

    /* INTENT[INT-CON-035 -> TC-CON-085]:
     * st2msa on a file with the wrong extension shall warn and
     * return ST_NO_ERROR (non-fatal). */
    {
        const char *szWrongExt = "use_cases/UC20A/wrong.txt";
        file_t     *ptF        = NULL;

        if (file_open(szWrongExt, FILE_MODE_WRITE, &ptF) == ST_NO_ERROR)
        {
            file_write(ptF, (const st_u8_t *)"x", 1);
            file_close(&ptF);
        }

        snprintf(szScriptBuf, sizeof(szScriptBuf),
                 "st2msa %s\nquit\n", szWrongExt);
        if (write_script(szScriptSt, szScriptBuf))
        {
            strncpy(tLineCtx.szScriptFile, szScriptSt,
                    sizeof(tLineCtx.szScriptFile) - 1);
            tLineCtx.bRunning = ST_TRUE;
            eResult = line_run(&tLineCtx);
            tLineCtx.szScriptFile[0] = '\0';
            UC_TEST("[R] st2msa wrong extension == ST_NO_ERROR",
                    eResult == ST_NO_ERROR);
        }
        else
        {
            TEST_SKIP("[R] st2msa wrong extension == ST_NO_ERROR");
        }
    }

    /* INTENT[INT-CON-036 -> TC-CON-086]:
     * st2msa on a non-existent file shall warn and return ST_NO_ERROR. */
    snprintf(szScriptBuf, sizeof(szScriptBuf),
             "st2msa %s/phantom.st\nquit\n", szDir20A);
    if (write_script(szScriptSt, szScriptBuf))
    {
        strncpy(tLineCtx.szScriptFile, szScriptSt,
                sizeof(tLineCtx.szScriptFile) - 1);
        tLineCtx.bRunning = ST_TRUE;
        eResult = line_run(&tLineCtx);
        tLineCtx.szScriptFile[0] = '\0';
        UC_TEST("[R] st2msa missing file == ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    }
    else
    {
        TEST_SKIP("[R] st2msa missing file == ST_NO_ERROR");
    }

    /* INTENT[INT-CON-037 -> TC-CON-087]:
     * st2msa --all with non-existent --dir shall warn, ST_NO_ERROR. */
    snprintf(szScriptBuf, sizeof(szScriptBuf),
             "st2msa --all --dir %s/nope\nquit\n", szDir20A);
    if (write_script(szScriptSt, szScriptBuf))
    {
        strncpy(tLineCtx.szScriptFile, szScriptSt,
                sizeof(tLineCtx.szScriptFile) - 1);
        tLineCtx.bRunning = ST_TRUE;
        eResult = line_run(&tLineCtx);
        tLineCtx.szScriptFile[0] = '\0';
        UC_TEST("[R] st2msa --all non-existent dir == ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    }
    else
    {
        TEST_SKIP("[R] st2msa --all non-existent dir == ST_NO_ERROR");
    }

    /* INTENT[INT-CON-038 -> TC-CON-088]:
     * msa2st on a file with wrong extension shall warn, ST_NO_ERROR. */
    snprintf(szScriptBuf, sizeof(szScriptBuf),
             "msa2st %s/wrong.txt\nquit\n", szDir20A);
    if (write_script(szScriptMs, szScriptBuf))
    {
        strncpy(tLineCtx.szScriptFile, szScriptMs,
                sizeof(tLineCtx.szScriptFile) - 1);
        tLineCtx.bRunning = ST_TRUE;
        eResult = line_run(&tLineCtx);
        tLineCtx.szScriptFile[0] = '\0';
        UC_TEST("[R] msa2st wrong extension == ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    }
    else
    {
        TEST_SKIP("[R] msa2st wrong extension == ST_NO_ERROR");
    }

    /* INTENT[INT-CON-039 -> TC-CON-089]:
     * st2msa with no args and no selection shall warn, ST_NO_ERROR. */
    snprintf(szScriptBuf, sizeof(szScriptBuf), "st2msa\nquit\n");
    if (write_script(szScriptSt, szScriptBuf))
    {
        strncpy(tLineCtx.szScriptFile, szScriptSt,
                sizeof(tLineCtx.szScriptFile) - 1);
        tLineCtx.bRunning = ST_TRUE;
        eResult = line_run(&tLineCtx);
        tLineCtx.szScriptFile[0] = '\0';
        UC_TEST("[R] st2msa no args no selection == ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    }
    else
    {
        TEST_SKIP("[R] st2msa no args no selection == ST_NO_ERROR");
    }

    /* INTENT[INT-CON-040 -> TC-CON-090]:
     * msa2st with no args and no selection shall warn, ST_NO_ERROR. */
    snprintf(szScriptBuf, sizeof(szScriptBuf), "msa2st\nquit\n");
    if (write_script(szScriptMs, szScriptBuf))
    {
        strncpy(tLineCtx.szScriptFile, szScriptMs,
                sizeof(tLineCtx.szScriptFile) - 1);
        tLineCtx.bRunning = ST_TRUE;
        eResult = line_run(&tLineCtx);
        tLineCtx.szScriptFile[0] = '\0';
        UC_TEST("[R] msa2st no args no selection == ST_NO_ERROR",
                eResult == ST_NO_ERROR);
    }
    else
    {
        TEST_SKIP("[R] msa2st no args no selection == ST_NO_ERROR");
    }

    line_shutdown(&tLineCtx);
    trace_shutdown();
    gui_shutdown();
    st_shutdown(&tMachine);

    printf("\n=== UC20A Results: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
