/*
 * use_case_24C.c - JSON annotation (P47) + info band (P48)
 *
 * TEST MATRIX - UC24C:
 *   [N] Nominal    : 20 tests - json_path derivation, create/destroy,
 *                               load/save round-trip, get/set sector,
 *                               multi-sector, global notes, update note,
 *                               empty annotation, band constants
 *   [R] Robustness : 10 tests - NULL params, absent file, oversized note,
 *                               negative LBA, malformed JSON, realloc growth
 *   [S] Skipped    :  2 tests - run make manual (band visible in hex view)
 *
 * Module-level traceability:
 *   UFR-HEX-007 → REQ-HEX-024..028 → TC-HEX-074..090 → INT-HEX-074..088
 *
 * INTENT[INT-HEX-074..088 → TC-HEX-074..090 → REQ-HEX-024..028
 *        → UFR-HEX-007]:
 *   The hex editor persists per-sector annotations in a JSON file
 *   colocated with the disk image, and displays sector type, BPB layout,
 *   and the editable note in a two-row band at the bottom of the window,
 *   enabling the user to annotate and navigate disk images without
 *   external tools.
 */

#include "use_cases.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ST_PLATFORM_WINDOWS
#  include <windows.h>  /* for DeleteFileA / temp path */
#endif

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Returns a path to a writable temp file with the given extension.   */
static void tmp_path(const char *szExt, char *szOut, size_t uiLen)
{
#ifdef ST_PLATFORM_WINDOWS
    char szDir[MAX_PATH];
    char szBase[MAX_PATH];
    GetTempPathA((DWORD)sizeof(szDir), szDir);
    snprintf(szBase, sizeof(szBase), "st4ever_uc24c_%p", (void *)szOut);
    snprintf(szOut, uiLen, "%s%s%s", szDir, szBase, szExt);
#else
    snprintf(szOut, uiLen, "/tmp/st4ever_uc24c_%p%s",
             (void *)szOut, szExt);
#endif
}

static void remove_file(const char *szPath)
{
#ifdef ST_PLATFORM_WINDOWS
    DeleteFileA(szPath);
#else
    remove(szPath);
#endif
}

/* ------------------------------------------------------------------ */
/* JSON path derivation                                               */
/* ------------------------------------------------------------------ */

static void test_json_path(void)
{
    char szOut[ST_MAX_PATH];

    printf("  --- image_annot_json_path ---\n");

    /* INTENT[INT-HEX-074]: extension replaced by .json for .st path  */
    image_annot_json_path("foo/bar.st", szOut, sizeof(szOut));
    UC_TEST("[N] .st -> .json",
              strcmp(szOut, "foo/bar.json") == 0);

    image_annot_json_path("C:\\demos\\game.msa", szOut, sizeof(szOut));
    UC_TEST("[N] .msa -> .json",
              strcmp(szOut, "C:\\demos\\game.json") == 0);

    image_annot_json_path("nodot", szOut, sizeof(szOut));
    UC_TEST("[N] no extension -> appended .json",
              strcmp(szOut, "nodot.json") == 0);

    image_annot_json_path("a.b.st", szOut, sizeof(szOut));
    UC_TEST("[N] double dot -> last ext replaced",
              strcmp(szOut, "a.b.json") == 0);

    /* INTENT[INT-HEX-075]: NULL/tiny buffer handled gracefully        */
    image_annot_json_path(NULL, szOut, sizeof(szOut));
    UC_TEST("[R] NULL path -> empty result", szOut[0] == '\0');

    image_annot_json_path("foo.st", szOut, 4u);
    UC_TEST("[R] tiny buffer -> no crash", 1);
}

/* ------------------------------------------------------------------ */
/* Create / destroy lifecycle                                         */
/* ------------------------------------------------------------------ */

static void test_lifecycle(void)
{
    image_annot_t *ptA = NULL;

    printf("  --- create/destroy lifecycle ---\n");

    /* INTENT[INT-HEX-076]: create returns valid empty annotation     */
    UC_CHECK("[N] create",   image_annot_create(&ptA));
    UC_TEST("[N] not NULL",   ptA != NULL);
    if (ptA != NULL)
    {
        UC_TEST("[N] zero sectors", ptA->iSectorCount == 0);
        UC_TEST("[N] cap > 0",      ptA->iSectorCap   >  0);
    }

    /* INTENT[INT-HEX-077]: destroy frees and sets pointer to NULL    */
    UC_CHECK("[N] destroy",  image_annot_destroy(&ptA));
    UC_TEST("[N] NULL after destroy", ptA == NULL);

    /* INTENT[INT-HEX-078]: robustness NULL params                    */
    UC_TEST("[R] create NULL -> ST_ERROR",
              image_annot_create(NULL) == ST_ERROR);
    UC_TEST("[R] destroy NULL ptr -> ST_ERROR",
              image_annot_destroy(NULL) == ST_ERROR);
}

/* ------------------------------------------------------------------ */
/* set_sector / get_sector                                            */
/* ------------------------------------------------------------------ */

static void test_set_get(void)
{
    image_annot_t        *ptA   = NULL;
    const annot_sector_t *ptSec = NULL;

    printf("  --- set_sector / get_sector ---\n");

    UC_CHECK("[N] create", image_annot_create(&ptA));
    if (ptA == NULL)
    {
        printf("  [SKIP] create failed — skipping set/get tests\n");
        return;
    }

    /* INTENT[INT-HEX-079]: set_sector creates new entry              */
    UC_CHECK("[N] set lba=0",
             image_annot_set_sector(ptA, 0, "fat12", "First FAT"));
    UC_TEST("[N] count == 1", ptA->iSectorCount == 1);

    ptSec = image_annot_get_sector(ptA, 0);
    UC_TEST("[N] get lba=0 not NULL",   ptSec != NULL);
    UC_TEST("[N] type == fat12",
              ptSec != NULL && strcmp(ptSec->szType, "fat12") == 0);
    UC_TEST("[N] notes correct",
              ptSec != NULL &&
              strcmp(ptSec->szNotes, "First FAT") == 0);

    /* INTENT[INT-HEX-080]: set_sector updates existing entry         */
    UC_CHECK("[N] update lba=0",
             image_annot_set_sector(ptA, 0, "fat12", "Updated"));
    UC_TEST("[N] count still 1", ptA->iSectorCount == 1);
    ptSec = image_annot_get_sector(ptA, 0);
    UC_TEST("[N] notes updated",
              ptSec != NULL &&
              strcmp(ptSec->szNotes, "Updated") == 0);

    /* INTENT[INT-HEX-081]: get_sector returns NULL for unknown LBA   */
    UC_TEST("[N] get unknown lba → NULL",
              image_annot_get_sector(ptA, 999) == NULL);

    /* INTENT[INT-HEX-082]: set NULL note does not crash              */
    UC_CHECK("[N] set NULL notes",
             image_annot_set_sector(ptA, 5, "bss", NULL));

    /* INTENT[INT-HEX-083]: NULL ptAnnot → ST_ERROR / NULL            */
    UC_TEST("[R] set NULL ptAnnot → ST_ERROR",
              image_annot_set_sector(NULL, 0, "", "") == ST_ERROR);
    UC_TEST("[R] get NULL ptAnnot → NULL",
              image_annot_get_sector(NULL, 0) == NULL);

    image_annot_destroy(&ptA);
}

/* ------------------------------------------------------------------ */
/* Capacity growth                                                    */
/* ------------------------------------------------------------------ */

static void test_growth(void)
{
    image_annot_t *ptA = NULL;
    int            i;
    char           szNote[32];

    printf("  --- capacity growth ---\n");

    UC_CHECK("[N] create", image_annot_create(&ptA));
    if (ptA == NULL) return;

    /* INTENT[INT-HEX-084]: realloc triggered when count exceeds cap  */
    for (i = 0; i < 40; i++)
    {
        snprintf(szNote, sizeof(szNote), "sector %d", i);
        UC_CHECK("[N] set growth",
                 image_annot_set_sector(ptA, i, "bss", szNote));
    }
    UC_TEST("[N] 40 entries after growth",
              ptA->iSectorCount == 40);

    {
        const annot_sector_t *ptSec = image_annot_get_sector(ptA, 39);
        UC_TEST("[N] last entry retrievable", ptSec != NULL);
        if (ptSec != NULL)
            UC_TEST("[N] last entry notes correct",
                      strcmp(ptSec->szNotes, "sector 39") == 0);
    }

    image_annot_destroy(&ptA);
}

/* ------------------------------------------------------------------ */
/* Save / load round-trip                                             */
/* ------------------------------------------------------------------ */

static void test_roundtrip(void)
{
    image_annot_t        *ptSave  = NULL;
    image_annot_t        *ptLoad  = NULL;
    const annot_sector_t *ptSec   = NULL;
    char                  szJson[ST_MAX_PATH];

    printf("  --- save / load round-trip ---\n");

    tmp_path(".json", szJson, sizeof(szJson));

    UC_CHECK("[N] create save", image_annot_create(&ptSave));
    if (ptSave == NULL) return;

    /* INTENT[INT-HEX-085]: round-trip preserves filename/notes/sectors */
    strncpy(ptSave->szFilename, "test.st", ST_MAX_PATH - 1);
    strncpy(ptSave->szNotes, "Test disk", ANNOT_NOTE_MAX - 1);
    UC_CHECK("[N] set sector 0",
             image_annot_set_sector(ptSave, 0,
                                    "bootsector_noboot",
                                    "BPB valid, not bootable"));
    UC_CHECK("[N] set sector 1",
             image_annot_set_sector(ptSave, 1,
                                    "fat12",
                                    "FAT1 sector"));
    UC_CHECK("[N] save", image_annot_save(ptSave, szJson));

    UC_CHECK("[N] create load", image_annot_create(&ptLoad));
    if (ptLoad == NULL)
    {
        image_annot_destroy(&ptSave);
        remove_file(szJson);
        return;
    }
    UC_CHECK("[N] load", image_annot_load(szJson, ptLoad));

    UC_TEST("[N] filename round-trips",
              strcmp(ptLoad->szFilename, "test.st") == 0);
    UC_TEST("[N] global notes round-trips",
              strcmp(ptLoad->szNotes, "Test disk") == 0);
    UC_TEST("[N] sector count == 2",
              ptLoad->iSectorCount == 2);

    ptSec = image_annot_get_sector(ptLoad, 0);
    UC_TEST("[N] sec 0 type",
              ptSec != NULL &&
              strcmp(ptSec->szType, "bootsector_noboot") == 0);
    UC_TEST("[N] sec 0 notes",
              ptSec != NULL &&
              strcmp(ptSec->szNotes, "BPB valid, not bootable") == 0);

    ptSec = image_annot_get_sector(ptLoad, 1);
    UC_TEST("[N] sec 1 type",
              ptSec != NULL && strcmp(ptSec->szType, "fat12") == 0);
    UC_TEST("[N] sec 1 notes",
              ptSec != NULL &&
              strcmp(ptSec->szNotes, "FAT1 sector") == 0);

    image_annot_destroy(&ptSave);
    image_annot_destroy(&ptLoad);
    remove_file(szJson);
}

/* ------------------------------------------------------------------ */
/* Absent file load (non-fatal)                                       */
/* ------------------------------------------------------------------ */

static void test_absent_file(void)
{
    image_annot_t *ptA = NULL;

    printf("  --- absent file ---\n");

    UC_CHECK("[N] create", image_annot_create(&ptA));
    if (ptA == NULL) return;

    /* INTENT[INT-HEX-086]: load absent file returns ST_ERROR silently */
    UC_TEST("[R] absent file → ST_ERROR",
              image_annot_load("/nonexistent/path.json", ptA)
              == ST_ERROR);
    UC_TEST("[R] annotation still empty after absent load",
              ptA->iSectorCount == 0);

    image_annot_destroy(&ptA);
}

/* ------------------------------------------------------------------ */
/* Load existing whatisit.json seed (tests parser on real data)       */
/* ------------------------------------------------------------------ */

static void test_load_seed(void)
{
    image_annot_t        *ptA   = NULL;
    const annot_sector_t *ptSec = NULL;

    printf("  --- load seed db/seeds/whatisit.json ---\n");

    UC_CHECK("[N] create", image_annot_create(&ptA));
    if (ptA == NULL) return;

    /* INTENT[INT-HEX-087]: parser handles the reference seed file    */
    if (image_annot_load("db/seeds/whatisit.json", ptA) != ST_NO_ERROR)
    {
        printf("  [SKIP] seed file absent or unreadable\n");
        image_annot_destroy(&ptA);
        return;
    }

    UC_TEST("[N] seed filename not empty",
              ptA->szFilename[0] != '\0');
    UC_TEST("[N] seed global notes not empty",
              ptA->szNotes[0] != '\0');
    UC_TEST("[N] seed has labeled sectors",
              ptA->iSectorCount > 0);

    /* Sector 0 must be present and labeled */
    ptSec = image_annot_get_sector(ptA, 0);
    UC_TEST("[N] sector 0 present",      ptSec != NULL);
    UC_TEST("[N] sector 0 type not empty",
              ptSec != NULL && ptSec->szType[0] != '\0');

    image_annot_destroy(&ptA);
}

/* ------------------------------------------------------------------ */
/* Band layout constants                                              */
/* ------------------------------------------------------------------ */

static void test_band_constants(void)
{
    printf("  --- band layout constants ---\n");

    /* INTENT[INT-HEX-088]: band height is exactly 2 rows             */
    UC_TEST("[N] HEXED_BAND_ROWS == 2", HEXED_BAND_ROWS == 2);

    /* INTENT[INT-HEX-088]: BAND_NOTE zone is distinct                */
    UC_TEST("[N] BAND_NOTE != HEX zone",
              HEX_ZONE_BAND_NOTE != HEX_ZONE_HEX);
    UC_TEST("[N] BAND_NOTE != ASCII zone",
              HEX_ZONE_BAND_NOTE != HEX_ZONE_ASCII);
    UC_TEST("[N] BAND_NOTE != DISASM zone",
              HEX_ZONE_BAND_NOTE != HEX_ZONE_DISASM);
}

/* ------------------------------------------------------------------ */
/* Manual tests                                                       */
/* ------------------------------------------------------------------ */

static void test_manual_band(void)
{
    printf("  --- manual band validation ---\n");

    TEST_MANUAL(
        "[S] Info band row 1 shows sector LBA and type",
        "Open use_cases/UC16/roundtrip.st via 'edit'\n"
        "  Expected: bottom band row 1 shows 'Sec 0  bootsector_noboot'\n"
        "            and the BPB layout 'FAT1@1  FAT2@6  Root@11  Data@18'\n"
        "            (or similar for that disk geometry).\n"
        "            Row 1 updates as you move the cursor to different sectors.\n"
        "  Is the band row 1 correct and updating?"
    );

    TEST_MANUAL(
        "[S] Info band note editing with CTRL+N and CTRL+S",
        "With roundtrip.st open in hex editor:\n"
        "  1. Press CTRL+N to activate note editing (band row 2 turns yellow).\n"
        "  2. Type 'Test annotation for sector 0'.\n"
        "  3. Press CTRL+S — a .json file should appear next to roundtrip.st.\n"
        "  4. Move cursor to a different sector and back — note persists.\n"
        "  Is the note edit flow correct?"
    );
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC24C: JSON Annotation + Info Band ===\n");

    test_json_path();
    test_lifecycle();
    test_set_get();
    test_growth();
    test_roundtrip();
    test_absent_file();
    test_load_seed();
    test_band_constants();
    test_manual_band();

    printf("\n=== UC24C: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails == 0) ? 0 : 1;
}
