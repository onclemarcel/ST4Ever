/*
 * image_st.h - ST4Ever Atari ST .st disk image read/write
 *
 * Provides a FAT12 layer over the standard Atari ST DD 3.5" raw sector
 * image format (.st files, exactly 737,280 bytes):
 *
 *   Sector   0      : boot sector (BPB at offset +11, LE16/LE32)
 *   Sectors  1..5   : FAT1  (5 sectors, 12-bit entries)
 *   Sectors  6..10  : FAT2  (backup)
 *   Sectors 11..17  : root directory (112 entries × 32 bytes)
 *   Sectors 18..    : data area (clusters 2..712)
 *
 * Only the root directory is supported (no subdirectories); this
 * covers the vast majority of Atari ST demos and utilities.
 *
 * All in-memory manipulation operates on a heap-allocated image_st_t
 * that holds the complete 737,280-byte disk image.  Changes are not
 * written to disk until image_st_save() is called.
 */

#ifndef IMAGE_ST_H
#define IMAGE_ST_H

#include "common.h"

/* ------------------------------------------------------------------
 * Geometry constants — standard Atari ST DD 3.5"
 * ------------------------------------------------------------------ */

#define IST_TRACKS          80
#define IST_SIDES           2
#define IST_SECTORS         9
#define IST_SECTOR_SIZE     512
#define IST_DISK_SIZE       (IST_TRACKS * IST_SIDES * \
                             IST_SECTORS * IST_SECTOR_SIZE) /* 737280 */

/* ------------------------------------------------------------------
 * FAT12 BPB values
 * ------------------------------------------------------------------ */

#define IST_BPS             512    /* bytes per sector                 */
#define IST_SPC             2      /* sectors per cluster              */
#define IST_RES             1      /* reserved sectors (boot sector)   */
#define IST_NFATS           2      /* number of FAT copies             */
#define IST_RDE             112    /* root directory entries           */
#define IST_TS              1440   /* total sectors (80×2×9)           */
#define IST_MEDIA           0xF9   /* media descriptor byte            */
#define IST_SPF             5      /* sectors per FAT                  */
#define IST_SPT             9      /* sectors per track                */
#define IST_HEADS           2      /* number of heads                  */

/* Derived layout positions (sector numbers) */
#define IST_FAT1_SECTOR     1
#define IST_FAT2_SECTOR     (IST_FAT1_SECTOR + IST_SPF)
#define IST_ROOT_SECTOR     (IST_FAT2_SECTOR + IST_SPF)
#define IST_ROOT_SECTORS    (IST_RDE * 32 / IST_BPS)         /* 7     */
#define IST_DATA_SECTOR     (IST_ROOT_SECTOR + IST_ROOT_SECTORS) /* 18 */
#define IST_DATA_CLUSTERS   ((IST_TS - IST_DATA_SECTOR) / IST_SPC) /* 711 */
#define IST_CLUSTER_FIRST   2      /* clusters 0 and 1 are reserved   */

/* ------------------------------------------------------------------
 * FAT12 special cluster values
 * ------------------------------------------------------------------ */

#define IST_FAT_FREE        0x000u
#define IST_FAT_BAD         0xFF7u
#define IST_FAT_EOC         0xFF8u  /* 0xFF8..0xFFF = end-of-chain    */
#define IST_FAT_MEDIA_BYTE  0xFF9u  /* written to cluster 0           */

/* ------------------------------------------------------------------
 * Directory entry attribute flags
 * ------------------------------------------------------------------ */

#define IST_ATTR_READONLY   0x01u
#define IST_ATTR_HIDDEN     0x02u
#define IST_ATTR_SYSTEM     0x04u
#define IST_ATTR_VOLNAME    0x08u
#define IST_ATTR_SUBDIR     0x10u
#define IST_ATTR_ARCHIVE    0x20u

/* Maximum 8.3 name including dot and NUL: "FILENAME.EXT\0" = 13 */
#define IST_NAME_MAX        13

/* ------------------------------------------------------------------
 * Public types
 * ------------------------------------------------------------------ */

/*
 * image_st_dirent_t - One entry returned by image_st_list_root().
 */
typedef struct image_st_dirent_s
{
    char      szName[IST_NAME_MAX]; /* "FILENAME.EXT\0" (uppercase)   */
    st_u32_t  uiSize;               /* file size in bytes             */
    st_u8_t   uiAttr;               /* raw FAT attribute byte         */
    st_u16_t  uiCluster;            /* starting cluster number        */
    st_bool_t bIsDir;               /* ST_TRUE if subdirectory        */
} image_st_dirent_t;

/* Opaque image handle (defined in image_st.c) */
typedef struct image_st_s image_st_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * image_st_create() - Allocate and format a blank DD image in memory.
 *
 * Writes a valid BPB boot sector, initialises FAT1 and FAT2 with the
 * media-byte marker (cluster 0 = 0xFF9) and end-of-chain (cluster 1 =
 * 0xFFF).  The root directory is zeroed.  Nothing is written to disk
 * until image_st_save() is called.
 *
 * Parameters:
 *   pptImg [out] : Receives the allocated handle.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptImg is NULL or malloc fails.
 */
st_error_t image_st_create(image_st_t **pptImg);

/*
 * image_st_load() - Load an existing .st image file into memory.
 *
 * The file must be exactly IST_DISK_SIZE bytes and contain a valid BPB
 * (bytes-per-sector == 512, total-sectors == 1440).
 *
 * Parameters:
 *   szPath [in]  : Path to a .st image file.
 *   pptImg [out] : Receives the allocated handle.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL, file not found, wrong size,
 *               or BPB validation fails.
 */
st_error_t image_st_load(const char *szPath, image_st_t **pptImg);

/*
 * image_st_save() - Write the in-memory image to a .st file.
 *
 * Creates or overwrites szPath with exactly IST_DISK_SIZE bytes.
 *
 * Parameters:
 *   ptImg  [in] : Image handle.
 *   szPath [in] : Destination file path.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL or the write fails.
 */
st_error_t image_st_save(image_st_t *ptImg, const char *szPath);

/*
 * image_st_close() - Free an image handle.
 *
 * Sets *pptImg to NULL.  image_st_close(&NULL) is a safe no-op.
 *
 * Parameters:
 *   pptImg [in/out] : Address of the handle to free.
 *
 * Returns:
 *   ST_NO_ERROR always (except NULL pptImg → ST_ERROR).
 */
st_error_t image_st_close(image_st_t **pptImg);

/*
 * image_st_list_root() - Enumerate root directory entries.
 *
 * Deleted entries (0xE5) and volume-label entries (IST_ATTR_VOLNAME)
 * are skipped.  Enumeration stops at the first unused entry (0x00).
 *
 * Parameters:
 *   ptImg       [in]  : Image handle.
 *   aEntries    [out] : Caller-supplied array to receive entries.
 *   iMaxEntries [in]  : Capacity of aEntries (must be > 0).
 *   piCount     [out] : Number of entries written.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL or iMaxEntries <= 0.
 */
st_error_t image_st_list_root(image_st_t        *ptImg,
                               image_st_dirent_t *aEntries,
                               int                iMaxEntries,
                               int               *piCount);

/*
 * image_st_read_file() - Read file content by following the FAT12 chain.
 *
 * Parameters:
 *   ptImg      [in]  : Image handle.
 *   uiCluster  [in]  : Starting cluster (from the directory entry).
 *   uiSize     [in]  : Expected size in bytes (from the directory entry).
 *   pBuf       [out] : Destination buffer; must be at least uiBufSize bytes.
 *   uiBufSize  [in]  : Buffer capacity; must be >= uiSize.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL, uiSize > uiBufSize, a cluster
 *               number is out of range, or the FAT chain ends early.
 */
st_error_t image_st_read_file(image_st_t *ptImg,
                               st_u16_t    uiCluster,
                               st_u32_t    uiSize,
                               st_u8_t    *pBuf,
                               st_u32_t    uiBufSize);

/*
 * image_st_write_file() - Add or replace a file in the root directory.
 *
 * szName is normalised to 8.3 uppercase.  If a file with the same name
 * already exists it is deleted (FAT chain freed, entry marked 0xE5) before
 * the new entry is created.  For an empty file (uiSize == 0) no clusters
 * are allocated; pData may be NULL.
 *
 * Parameters:
 *   ptImg  [in] : Image handle.
 *   szName [in] : Filename in any case (e.g. "HELLO.PRG" or "hello.prg").
 *   pData  [in] : File content (may be NULL iff uiSize == 0).
 *   uiSize [in] : Byte count to write.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptImg or szName is NULL, pData is NULL while uiSize>0,
 *               the name is not valid 8.3, the disk is full, or the root
 *               directory is full.
 */
st_error_t image_st_write_file(image_st_t    *ptImg,
                                const char    *szName,
                                const st_u8_t *pData,
                                st_u32_t       uiSize);

/*
 * image_st_delete_file() - Remove a file from the root directory.
 *
 * Marks the directory entry as deleted (first byte = 0xE5) and frees
 * the entire FAT12 chain.
 *
 * Parameters:
 *   ptImg  [in] : Image handle.
 *   szName [in] : Filename to delete (case-insensitive 8.3 match).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL or the file is not found.
 */
st_error_t image_st_delete_file(image_st_t *ptImg, const char *szName);

/*
 * image_st_free_bytes() - Count free space on the image.
 *
 * Counts FAT_FREE (0x000) entries in FAT1 and multiplies by the
 * cluster size.
 *
 * Parameters:
 *   ptImg        [in]  : Image handle.
 *   puiFreeBytes [out] : Free bytes (multiple of IST_SPC * IST_BPS).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL.
 */
st_error_t image_st_free_bytes(image_st_t *ptImg,
                                st_u32_t   *puiFreeBytes);

#endif /* IMAGE_ST_H */
