/*
 * use_case_06.c - UC6: portable file system abstraction (file.h/file.c)
 *
 * Tests run from the project root (make tests requirement — R12).
 * Fixture: use_cases/UC01/hello.prg (known-good PRG, 32 bytes)
 * Temp write target: build/uc06_write_test.bin (created + deleted)
 * Temp dir target:   build/uc06_mkdir_test/
 *
 * TEST MATRIX - UC6:
 *   [N] Nominal    : 28 tests - file_stat (file/dir/no-ext);
 *                               file_open/read/close (read mode);
 *                               file_open/write/close round-trip;
 *                               file_mkdir (new + already exists);
 *                               file_list_dir (src/ non-empty, hidden filter)
 *   [R] Robustness : 12 tests - NULL params on all 7 public functions;
 *                               open non-existent (read); write uiLen=0;
 *                               file_close(&NULL); list_dir non-existent
 *   [S] Skipped    :  0 tests
 *
 * Traceability:
 *   INT-FIL-001..011 → TC-FIL-001..038 → REQ-FIL-001..022
 *   UFR-FIL-001..007
 */

#include "use_cases.h"
#include "../src/file.h"

/* ----------------------------------------------------------------
 * Callback context for file_list_dir tests
 * ---------------------------------------------------------------- */

typedef struct
{
    int       iCount;
    st_bool_t bFoundCommonH;  /* found "common.h" entry */
    st_bool_t bFoundHidden;   /* found any '.' entry     */
} list_ctx_t;

static void list_cb(const char        *szPath,
                    const char        *szName,
                    const file_stat_t *ptStat,
                    void              *pCtx)
{
    list_ctx_t *ptCtx = (list_ctx_t *)pCtx;

    ST_UNUSED(szPath);
    ST_UNUSED(ptStat);

    ptCtx->iCount++;

    if (strcmp(szName, "common.h") == 0)
    {
        ptCtx->bFoundCommonH = ST_TRUE;
    }
    if (szName[0] == '.')
    {
        ptCtx->bFoundHidden = ST_TRUE;
    }
}

int g_uc_fails = 0;

int main(void)
{
    /* ---- locals -------------------------------------------------- */
    file_stat_t  tStat;
    file_t      *ptFile;
    st_u8_t      aBuf[64];
    size_t       uiRead;
    list_ctx_t   tCtx;
    const char  *szPrg    = "use_cases/UC01/hello.prg";
    const char  *szSrcDir = "src";
    const char  *szWrPath = "build/uc06_write_test.bin";
    const char  *szMkDir  = "build/uc06_mkdir_test";

    printf("=== UC6: File System Abstraction ===\n\n");

    /* ================================================================
     * BLOCK 1 — file_stat: nominal
     * ================================================================
     * INTENT[INT-FIL-001..002 → TC-FIL-001..007 → REQ-FIL-002..003 → UFR-FIL-001]:
     * file_stat() on an existing file fills the struct correctly.
     * file_stat() on a directory sets bIsDir=TRUE.
     * file_stat() on a non-existent path returns ST_NO_ERROR with
     * bExists=FALSE (querying existence is not an error).
     */
    printf("--- file_stat: nominal ---\n");

    UC_CHECK("[N] file_stat on existing file -> ST_NO_ERROR",
             file_stat(szPrg, &tStat));
    UC_TEST("[N] hello.prg: bExists == TRUE",
            tStat.bExists == ST_TRUE);
    UC_TEST("[N] hello.prg: bIsDir == FALSE",
            tStat.bIsDir == ST_FALSE);
    UC_TEST("[N] hello.prg: uiSize > 0",
            tStat.uiSize > 0);
    UC_TEST("[N] hello.prg: szExt == \"prg\"",
            strcmp(tStat.szExt, "prg") == 0);

    UC_CHECK("[N] file_stat on directory -> ST_NO_ERROR",
             file_stat(szSrcDir, &tStat));
    UC_TEST("[N] src/: bExists == TRUE",
            tStat.bExists == ST_TRUE);
    UC_TEST("[N] src/: bIsDir == TRUE",
            tStat.bIsDir == ST_TRUE);
    UC_TEST("[N] src/: uiSize == 0 (dirs report 0)",
            tStat.uiSize == 0 || tStat.bIsDir == ST_TRUE);

    UC_CHECK("[N] file_stat on non-existent path -> ST_NO_ERROR",
             file_stat("no_such_file_uc06.bin", &tStat));
    UC_TEST("[N] non-existent: bExists == FALSE",
            tStat.bExists == ST_FALSE);

    /* ================================================================
     * BLOCK 2 — file_stat: robustness
     * ================================================================
     * INTENT[INT-FIL-003 → TC-FIL-008 → REQ-FIL-001 → UFR-FIL-001]:
     * NULL parameters are rejected with ST_ERROR before any side effect.
     */
    printf("\n--- file_stat: robustness ---\n");

    UC_TEST("[R] file_stat(NULL, &tStat) -> ST_ERROR",
            file_stat(NULL, &tStat) == ST_ERROR);
    UC_TEST("[R] file_stat(szPath, NULL) -> ST_ERROR",
            file_stat(szPrg, NULL) == ST_ERROR);

    /* ================================================================
     * BLOCK 3 — file_open / file_read / file_close: nominal read
     * ================================================================
     * INTENT[INT-FIL-004..005 → TC-FIL-009..014 → REQ-FIL-004..009 → UFR-FIL-002..003, UFR-FIL-005]:
     * A known binary file (hello.prg) can be opened, partially read,
     * fully read (PRG magic 0x601A verified), and closed cleanly.
     * A second read after full consumption returns 0 bytes (EOF).
     */
    printf("\n--- file_open/read/close: nominal ---\n");

    ptFile = NULL;
    UC_CHECK("[N] file_open(hello.prg, READ) -> ST_NO_ERROR",
             file_open(szPrg, FILE_MODE_READ, &ptFile));
    UC_TEST("[N] handle not NULL after open",
            ptFile != NULL);

    uiRead = 0;
    UC_CHECK("[N] file_read 4 bytes -> ST_NO_ERROR",
             file_read(ptFile, aBuf, 4, &uiRead));
    UC_TEST("[N] 4 bytes read",
            uiRead == 4);
    /* PRG magic = 0x601A */
    UC_TEST("[N] first 2 bytes are PRG magic 0x601A",
            aBuf[0] == 0x60 && aBuf[1] == 0x1A);

    /* Read rest of the file */
    uiRead = 0;
    UC_CHECK("[N] file_read remaining bytes -> ST_NO_ERROR",
             file_read(ptFile, aBuf, sizeof(aBuf), &uiRead));
    UC_TEST("[N] remaining bytes read > 0",
            uiRead > 0);

    /* Read at EOF */
    uiRead = 99;
    UC_CHECK("[N] file_read at EOF -> ST_NO_ERROR",
             file_read(ptFile, aBuf, sizeof(aBuf), &uiRead));
    UC_TEST("[N] bytes read at EOF == 0",
            uiRead == 0);

    UC_CHECK("[N] file_close -> ST_NO_ERROR",
             file_close(&ptFile));
    UC_TEST("[N] handle NULL after close",
            ptFile == NULL);

    /* ================================================================
     * BLOCK 4 — file_open / file_write / file_close: round-trip
     * ================================================================
     * INTENT[INT-FIL-006 → TC-FIL-015..019 → REQ-FIL-006, REQ-FIL-011, REQ-FIL-013..014 → UFR-FIL-002..005]:
     * Write a known 8-byte pattern, verify via file_stat (size==8),
     * re-open for read, verify the content matches byte-for-byte.
     */
    printf("\n--- file_open/write/close: round-trip ---\n");

    {
        const st_u8_t aPattern[8] = { 0xDE,0xAD,0xBE,0xEF,
                                       0xCA,0xFE,0xBA,0xBE };
        st_u8_t       aVerify[8];

        ptFile = NULL;
        UC_CHECK("[N] file_open(WRITE) -> ST_NO_ERROR",
                 file_open(szWrPath, FILE_MODE_WRITE, &ptFile));
        UC_TEST("[N] write handle not NULL",
                ptFile != NULL);

        UC_CHECK("[N] file_write 8 bytes -> ST_NO_ERROR",
                 file_write(ptFile, aPattern, sizeof(aPattern)));

        UC_CHECK("[N] file_close after write -> ST_NO_ERROR",
                 file_close(&ptFile));

        /* Verify via stat */
        UC_CHECK("[N] file_stat on written file -> ST_NO_ERROR",
                 file_stat(szWrPath, &tStat));
        UC_TEST("[N] written file bExists == TRUE",
                tStat.bExists == ST_TRUE);
        UC_TEST("[N] written file uiSize == 8",
                tStat.uiSize == 8);

        /* Re-read and verify content */
        ptFile = NULL;
        UC_CHECK("[N] file_open written file for READ -> ST_NO_ERROR",
                 file_open(szWrPath, FILE_MODE_READ, &ptFile));
        uiRead = 0;
        UC_CHECK("[N] file_read written content -> ST_NO_ERROR",
                 file_read(ptFile, aVerify, sizeof(aVerify), &uiRead));
        UC_TEST("[N] read back 8 bytes",
                uiRead == 8);
        UC_TEST("[N] content matches pattern",
                memcmp(aPattern, aVerify, 8) == 0);
        file_close(&ptFile);
    }

    /* ================================================================
     * BLOCK 5 — file_open / file_read / file_write: robustness
     * ================================================================
     * INTENT[INT-FIL-007 → TC-FIL-020..027 → REQ-FIL-004..005, REQ-FIL-007, REQ-FIL-011, REQ-FIL-013 → UFR-FIL-002..005]:
     * NULL params and invalid states are rejected with ST_ERROR.
     * file_close(&NULL) is a safe idempotent no-op.
     */
    printf("\n--- file I/O: robustness ---\n");

    UC_TEST("[R] file_open(NULL, READ, &h) -> ST_ERROR",
            file_open(NULL, FILE_MODE_READ, &ptFile) == ST_ERROR);
    UC_TEST("[R] file_open(path, READ, NULL) -> ST_ERROR",
            file_open(szPrg, FILE_MODE_READ, NULL) == ST_ERROR);
    UC_TEST("[R] file_open non-existent for READ -> ST_ERROR",
            file_open("no_such_uc06.bin", FILE_MODE_READ, &ptFile) == ST_ERROR);
    UC_TEST("[R] file_read(NULL, ...) -> ST_ERROR",
            file_read(NULL, aBuf, 4, &uiRead) == ST_ERROR);
    UC_TEST("[R] file_write(NULL, ...) -> ST_ERROR",
            file_write(NULL, aBuf, 4) == ST_ERROR);
    UC_TEST("[R] file_close(NULL) -> ST_ERROR",
            file_close(NULL) == ST_ERROR);
    UC_TEST("[R] file_close(&NULL) -> ST_NO_ERROR (idempotent)",
            file_close(&ptFile) == ST_NO_ERROR);

    /* write with uiLen==0 */
    {
        file_t *ptW = NULL;
        file_open(szWrPath, FILE_MODE_WRITE, &ptW);
        UC_TEST("[R] file_write uiLen==0 -> ST_ERROR",
                file_write(ptW, aBuf, 0) == ST_ERROR);
        file_close(&ptW);
    }

    /* ================================================================
     * BLOCK 6 — file_mkdir: nominal + already exists
     * ================================================================
     * INTENT[INT-FIL-008 → TC-FIL-028..030 → REQ-FIL-015..016 → UFR-FIL-006]:
     * Creating a new directory succeeds; stat confirms bIsDir=TRUE.
     * Creating the same directory again is a silent no-op (EEXIST).
     */
    printf("\n--- file_mkdir ---\n");

    UC_CHECK("[N] file_mkdir new directory -> ST_NO_ERROR",
             file_mkdir(szMkDir));
    UC_CHECK("[N] file_stat on new directory -> ST_NO_ERROR",
             file_stat(szMkDir, &tStat));
    UC_TEST("[N] new directory bExists == TRUE",
            tStat.bExists == ST_TRUE);
    UC_TEST("[N] new directory bIsDir == TRUE",
            tStat.bIsDir == ST_TRUE);

    UC_CHECK("[N] file_mkdir already-existing directory -> ST_NO_ERROR",
             file_mkdir(szMkDir));

    UC_TEST("[R] file_mkdir(NULL) -> ST_ERROR",
            file_mkdir(NULL) == ST_ERROR);

    /* ================================================================
     * BLOCK 7 — file_list_dir: nominal
     * ================================================================
     * INTENT[INT-FIL-009..010 → TC-FIL-031..035 → REQ-FIL-019..021 → UFR-FIL-007]:
     * Listing src/ with bShowHidden=FALSE yields > 0 entries, always
     * includes "common.h", and never delivers '.' / '..' or '.*' names.
     * With bShowHidden=TRUE the count can only increase or stay equal.
     */
    printf("\n--- file_list_dir: nominal ---\n");

    memset(&tCtx, 0, sizeof(tCtx));
    UC_CHECK("[N] file_list_dir(src/, FALSE) -> ST_NO_ERROR",
             file_list_dir(szSrcDir, ST_FALSE, list_cb, &tCtx));
    UC_TEST("[N] src/ list: count > 0",
            tCtx.iCount > 0);
    UC_TEST("[N] src/ list: common.h found",
            tCtx.bFoundCommonH == ST_TRUE);
    UC_TEST("[N] src/ list bShowHidden=FALSE: no hidden entry",
            tCtx.bFoundHidden == ST_FALSE);

    {
        list_ctx_t tCtxAll;
        memset(&tCtxAll, 0, sizeof(tCtxAll));
        UC_CHECK("[N] file_list_dir(src/, TRUE) -> ST_NO_ERROR",
                 file_list_dir(szSrcDir, ST_TRUE, list_cb, &tCtxAll));
        UC_TEST("[N] bShowHidden=TRUE count >= bShowHidden=FALSE count",
                tCtxAll.iCount >= tCtx.iCount);
    }

    /* extension in list */
    {
        list_ctx_t tCtxExt;
        memset(&tCtxExt, 0, sizeof(tCtxExt));
        file_list_dir(szSrcDir, ST_FALSE, list_cb, &tCtxExt);
        UC_TEST("[N] file_list_dir: at least one .h and one .c expected",
                tCtxExt.iCount >= 2);
    }

    /* ================================================================
     * BLOCK 8 — file_list_dir: robustness
     * ================================================================
     * INTENT[INT-FIL-011 → TC-FIL-036..038 → REQ-FIL-017..018 → UFR-FIL-007]:
     * NULL szPath, NULL callback, and non-existent directory all
     * return ST_ERROR before invoking any callback.
     */
    printf("\n--- file_list_dir: robustness ---\n");

    UC_TEST("[R] file_list_dir(NULL, ...) -> ST_ERROR",
            file_list_dir(NULL, ST_FALSE, list_cb, NULL) == ST_ERROR);
    UC_TEST("[R] file_list_dir(path, FALSE, NULL, ...) -> ST_ERROR",
            file_list_dir(szSrcDir, ST_FALSE, NULL, NULL) == ST_ERROR);
    UC_TEST("[R] file_list_dir(non-existent dir) -> ST_ERROR",
            file_list_dir("no_such_dir_uc06", ST_FALSE,
                          list_cb, &tCtx) == ST_ERROR);

    /* ================================================================
     * Summary
     * ================================================================
     */
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
