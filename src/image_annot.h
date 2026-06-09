/*
 * image_annot.h - Atari ST disk image JSON annotation module
 *
 * Loads and saves a <basename>.json annotation file colocated with
 * a disk image file.  Stores a global disk description and per-sector
 * notes, consumed by the hex editor info band (UC24C / P47 + P48).
 *
 * JSON schema subset loaded / saved:
 *   {
 *     "filename": "...",
 *     "notes":    "...",           <- global disk description
 *     "labeled_sectors": [
 *       { "lba": N, "type": "...", "notes": "..." }
 *     ]
 *   }
 *
 * If the JSON file is absent, image_annot_load() returns ST_ERROR
 * silently — the caller keeps an empty (but valid) annotation.
 * Unrecognised JSON fields are silently ignored.
 *
 * UC24C: initial implementation.
 */

#ifndef IMAGE_ANNOT_H
#define IMAGE_ANNOT_H

#include "common.h"

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define ANNOT_NOTE_MAX    256   /* max chars in a note string (incl NUL) */
#define ANNOT_TYPE_MAX     48   /* max chars in a sector type label       */
#define ANNOT_INIT_CAP     16   /* initial capacity of atSectors[]        */

/* ------------------------------------------------------------------ */
/* Types                                                              */
/* ------------------------------------------------------------------ */

/* Per-sector annotation entry */
typedef struct annot_sector_s
{
    int  iLba;
    char szType[ANNOT_TYPE_MAX];
    char szNotes[ANNOT_NOTE_MAX];
} annot_sector_t;

/* Disk-level annotation (heap-allocated via image_annot_create()) */
typedef struct image_annot_s
{
    char            szFilename[ST_MAX_PATH]; /* basename of the image     */
    char            szNotes[ANNOT_NOTE_MAX]; /* global disk description   */
    annot_sector_t *atSectors;               /* heap array                */
    int             iSectorCount;            /* used entries              */
    int             iSectorCap;              /* allocated capacity        */
} image_annot_t;

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/*
 * image_annot_create() - Allocate an empty annotation structure.
 *
 * Parameters:
 *   pptAnnot [out] : Receives the allocated annotation.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptAnnot is NULL or malloc fails.
 */
st_error_t image_annot_create(image_annot_t **pptAnnot);

/*
 * image_annot_destroy() - Free an annotation structure.
 *
 * Sets *pptAnnot to NULL on return.
 *
 * Parameters:
 *   pptAnnot [in/out] : Annotation to free.
 *
 * Returns:
 *   ST_NO_ERROR always (except NULL pptAnnot -> ST_ERROR).
 */
st_error_t image_annot_destroy(image_annot_t **pptAnnot);

/*
 * image_annot_json_path() - Derive the JSON annotation path from a
 *                            file path by replacing the extension.
 *
 * Example: "foo/bar.st" -> "foo/bar.json".
 * If no extension is found, ".json" is appended.
 *
 * Parameters:
 *   szFilePath [in]  : Absolute or relative path to the image file.
 *   szJsonPath [out] : Buffer receiving the derived JSON path.
 *   uiLen      [in]  : Capacity of szJsonPath (bytes).
 */
void image_annot_json_path(const char *szFilePath,
                            char       *szJsonPath,
                            size_t      uiLen);

/*
 * image_annot_load() - Parse a JSON annotation file into ptAnnot.
 *
 * Returns ST_ERROR (non-fatal) if the file is absent or unreadable;
 * the caller proceeds with the empty annotation returned by create().
 *
 * Parameters:
 *   szJsonPath [in]     : Path to the JSON file.
 *   ptAnnot    [in/out] : Annotation to populate (must be create()'d).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if szJsonPath or ptAnnot is NULL, or file absent.
 */
st_error_t image_annot_load(const char    *szJsonPath,
                              image_annot_t *ptAnnot);

/*
 * image_annot_save() - Write ptAnnot to a JSON annotation file.
 *
 * Overwrites the file if it exists.  Creates it if absent.
 *
 * Parameters:
 *   ptAnnot    [in] : Annotation to serialize.
 *   szJsonPath [in] : Destination path.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL or the write fails.
 */
st_error_t image_annot_save(const image_annot_t *ptAnnot,
                              const char          *szJsonPath);

/*
 * image_annot_get_sector() - Find the annotation entry for a LBA.
 *
 * Parameters:
 *   ptAnnot [in] : Annotation to search (may be NULL).
 *   iLba    [in] : Logical block address to look up.
 *
 * Returns:
 *   Pointer to the matching annot_sector_t, or NULL if not found.
 */
const annot_sector_t *image_annot_get_sector(
                            const image_annot_t *ptAnnot,
                            int                  iLba);

/*
 * image_annot_set_sector() - Insert or update a per-sector annotation.
 *
 * If an entry for iLba exists it is updated in-place; otherwise a new
 * entry is appended (growing atSectors[] via realloc as needed).
 *
 * Parameters:
 *   ptAnnot  [in] : Annotation to modify.
 *   iLba     [in] : Logical block address.
 *   szType   [in] : Sector type label (may be NULL / "").
 *   szNotes  [in] : Notes text (may be NULL / "").
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptAnnot is NULL or realloc fails.
 */
st_error_t image_annot_set_sector(image_annot_t *ptAnnot,
                                   int            iLba,
                                   const char    *szType,
                                   const char    *szNotes);

#endif /* IMAGE_ANNOT_H */
