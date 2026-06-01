/*
 * file.h - ST4Ever portable file system abstraction
 *
 * Provides a consistent st_error_t API over CRT + POSIX file I/O so
 * that higher-level modules (load, edit, mount, image…) are insulated
 * from platform differences.
 *
 * Design principles:
 *   - All functions follow the project NULL-guard + ST_ERROR convention.
 *   - file_stat() on a non-existent path returns ST_NO_ERROR with
 *     ptStat->bExists == ST_FALSE — querying existence is not an error.
 *   - file_read() fills *puiRead with the byte count actually read;
 *     a partial read at EOF is ST_NO_ERROR with *puiRead < uiLen.
 *   - file_list_dir() enumerates entries in OS order (no sort
 *     guarantee) via a callback; '.' and '..' are always skipped.
 *   - The file_t handle is opaque (defined in file.c).
 *
 * Implementation: CRT fopen/fread/fwrite/fclose for file I/O;
 * POSIX opendir/readdir/stat/mkdir for directory operations
 * (available on both MSYS2/MinGW and Linux without Win32 calls).
 */

#ifndef FILE_H
#define FILE_H

#include "common.h"

/* ------------------------------------------------------------------
 * File information
 * ------------------------------------------------------------------ */

typedef struct file_stat_s
{
    st_bool_t bExists;        /* ST_FALSE if path does not exist       */
    st_bool_t bIsDir;         /* ST_TRUE if the path is a directory    */
    st_u64_t  uiSize;         /* File size in bytes (0 for dirs)       */
    char      szExt[16];      /* Lowercase extension without dot
                               * ("prg", "txt", "st") or "" if none   */
} file_stat_t;

/* ------------------------------------------------------------------
 * Open modes
 * ------------------------------------------------------------------ */

typedef enum file_mode_e
{
    FILE_MODE_READ   = 0,  /* Open existing file for reading           */
    FILE_MODE_WRITE  = 1,  /* Create / truncate for writing            */
    FILE_MODE_APPEND = 2   /* Open for append (create if absent)       */
} file_mode_t;

/* ------------------------------------------------------------------
 * Opaque file handle
 * ------------------------------------------------------------------ */

typedef struct file_s file_t;

/* ------------------------------------------------------------------
 * Directory listing callback
 * ------------------------------------------------------------------ */

/*
 * file_list_fn - called once per directory entry.
 *
 * Parameters:
 *   szPath  [in] : Full absolute (or relative) path of the entry.
 *   szName  [in] : Entry name only (no directory part).
 *   ptStat  [in] : Pre-filled stat for the entry.
 *   pCtx    [in] : User context passed to file_list_dir().
 */
typedef void (*file_list_fn)(const char        *szPath,
                              const char        *szName,
                              const file_stat_t *ptStat,
                              void              *pCtx);

/* ------------------------------------------------------------------
 * API — query
 * ------------------------------------------------------------------ */

/*
 * file_stat() - Query existence, type, size, and extension.
 *
 * A non-existent path is not an error: ptStat->bExists == ST_FALSE.
 *
 * Parameters:
 *   szPath [in]  : Path to query.
 *   ptStat [out] : Receives file info (zeroed then filled).
 *
 * Returns:
 *   ST_NO_ERROR on success (including non-existent path).
 *   ST_ERROR    if szPath or ptStat is NULL.
 */
st_error_t file_stat(const char *szPath, file_stat_t *ptStat);

/* ------------------------------------------------------------------
 * API — file I/O
 * ------------------------------------------------------------------ */

/*
 * file_open() - Open a file.
 *
 * Parameters:
 *   szPath  [in]  : File path.
 *   eMode   [in]  : FILE_MODE_READ / WRITE / APPEND.
 *   pptFile [out] : Receives an allocated file_t handle.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL, or if the OS open fails.
 */
st_error_t file_open(const char  *szPath,
                      file_mode_t  eMode,
                      file_t     **pptFile);

/*
 * file_read() - Read bytes from an open file.
 *
 * A short read at EOF is not an error: *puiRead < uiLen with
 * ST_NO_ERROR.  *puiRead == 0 at end-of-file.
 *
 * Parameters:
 *   ptFile  [in]  : Open file handle (FILE_MODE_READ).
 *   pBuf    [out] : Destination buffer.
 *   uiLen   [in]  : Maximum bytes to read.
 *   puiRead [out] : Bytes actually read.
 *
 * Returns:
 *   ST_NO_ERROR on success (including EOF).
 *   ST_ERROR    if any parameter is NULL or an I/O error occurs.
 */
st_error_t file_read(file_t *ptFile,
                      void   *pBuf,
                      size_t  uiLen,
                      size_t *puiRead);

/*
 * file_write() - Write bytes to an open file.
 *
 * Parameters:
 *   ptFile [in] : Open file handle (FILE_MODE_WRITE / APPEND).
 *   pBuf   [in] : Source buffer.
 *   uiLen  [in] : Number of bytes to write.
 *
 * Returns:
 *   ST_NO_ERROR if all uiLen bytes were written.
 *   ST_ERROR    if any parameter is NULL, uiLen == 0, or a short
 *               write / I/O error occurs.
 */
st_error_t file_write(file_t     *ptFile,
                       const void *pBuf,
                       size_t      uiLen);

/*
 * file_close() - Close a file and free the handle.
 *
 * *pptFile is set to NULL on return.  file_close(&NULL) is a safe
 * no-op returning ST_NO_ERROR.
 *
 * Parameters:
 *   pptFile [in/out] : Address of the file handle.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptFile is NULL, or if fclose() fails.
 */
st_error_t file_close(file_t **pptFile);

/* ------------------------------------------------------------------
 * API — directory operations
 * ------------------------------------------------------------------ */

/*
 * file_mkdir() - Create a directory (single level, no recursion).
 *
 * Succeeds silently if the directory already exists.
 *
 * Parameters:
 *   szPath [in] : Directory path to create.
 *
 * Returns:
 *   ST_NO_ERROR on success or if the directory already exists.
 *   ST_ERROR    if szPath is NULL or creation fails.
 */
st_error_t file_mkdir(const char *szPath);

/*
 * file_list_dir() - Enumerate the entries of a directory.
 *
 * Calls pfnCallback once per entry (excluding '.' and '..').
 * When bShowHidden == ST_FALSE, entries whose name starts with '.'
 * are also excluded.  Entries are delivered in OS order.
 *
 * Parameters:
 *   szPath      [in] : Directory to list.
 *   bShowHidden [in] : Include hidden ('.*') entries.
 *   pfnCallback [in] : Called once per visible entry.
 *   pCtx        [in] : Passed verbatim to each pfnCallback call.
 *
 * Returns:
 *   ST_NO_ERROR on success (even if the directory is empty).
 *   ST_ERROR    if szPath or pfnCallback is NULL, or if the
 *               directory cannot be opened.
 */
st_error_t file_list_dir(const char   *szPath,
                           st_bool_t    bShowHidden,
                           file_list_fn pfnCallback,
                           void        *pCtx);

#endif /* FILE_H */
