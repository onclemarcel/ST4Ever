/*
 * file.c - ST4Ever portable file system abstraction
 *
 * UC6: file_stat, file_open/read/write/close, file_mkdir,
 *      file_list_dir.
 *
 * Implementation strategy:
 *   - File I/O  : CRT fopen/fread/fwrite/fclose (fully portable).
 *   - Directory : POSIX opendir/readdir/stat + closedir, available
 *                 via MinGW on Windows without Win32 headers in src/.
 *   - mkdir     : platform-specific (#ifdef), thin wrapper.
 *
 * The file_t struct is intentionally opaque (defined here, not in
 * file.h) so callers cannot manipulate FILE * directly.
 */

#include "file.h"
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <sys/stat.h>
#include <dirent.h>

#ifdef ST_PLATFORM_WINDOWS
    #include <direct.h>   /* _mkdir */
    #define FILE_MKDIR(p) _mkdir(p)
#else
    #include <unistd.h>
    #define FILE_MKDIR(p) mkdir((p), 0755)
#endif

/* ------------------------------------------------------------------
 * Opaque file handle
 * ------------------------------------------------------------------ */

struct file_s
{
    FILE       *pHandle;
    file_mode_t eMode;
    char        szPath[ST_MAX_PATH];
};

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/*
 * file_extract_ext() - Fill szExt with the lowercase extension of
 * szName (without the dot).  Sets szExt[0]='\0' if none found.
 */
static void file_extract_ext(const char *szName,
                               char       *szExt,
                               size_t      uiMax)
{
    const char *pDot;
    const char *p;
    size_t      uiIdx;

    szExt[0] = '\0';

    if (szName == NULL || uiMax == 0)
    {
        return;
    }

    /* Find the last '.' after the last path separator */
    pDot = NULL;
    for (p = szName; *p != '\0'; p++)
    {
        if (*p == '.')
        {
            pDot = p;
        }
        else if (*p == '/' || *p == '\\')
        {
            pDot = NULL; /* reset: dot was in a directory component */
        }
    }

    if (pDot == NULL || pDot[1] == '\0')
    {
        return; /* no extension or trailing dot */
    }

    pDot++; /* skip the dot */

    uiIdx = 0;
    while (*pDot != '\0' && uiIdx < uiMax - 1)
    {
        szExt[uiIdx++] = (char)tolower((unsigned char)*pDot);
        pDot++;
    }
    szExt[uiIdx] = '\0';
}

/* ------------------------------------------------------------------
 * Public API — query
 * ------------------------------------------------------------------ */

st_error_t file_stat(const char *szPath, file_stat_t *ptStat)
{
    struct stat tSt;
    int         iRet;

    if (szPath == NULL || ptStat == NULL)
    {
        LOG_ERROR("NULL parameter: szPath=%p ptStat=%p",
                  (void *)szPath, (void *)ptStat);
        return ST_ERROR;
    }

    memset(ptStat, 0, sizeof(file_stat_t));

    iRet = stat(szPath, &tSt);

    if (iRet != 0)
    {
        /* Non-existent path is not an error — bExists stays ST_FALSE */
        ptStat->bExists = ST_FALSE;
        return ST_NO_ERROR;
    }

    ptStat->bExists = ST_TRUE;
    ptStat->bIsDir  = S_ISDIR(tSt.st_mode) ? ST_TRUE : ST_FALSE;
    ptStat->uiSize  = (st_u64_t)tSt.st_size;

    file_extract_ext(szPath, ptStat->szExt, sizeof(ptStat->szExt));

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API — file I/O
 * ------------------------------------------------------------------ */

st_error_t file_open(const char  *szPath,
                      file_mode_t  eMode,
                      file_t     **pptFile)
{
    file_t     *ptFile;
    const char *szFopenMode;

    LOG_TRACE("szPath='%s' eMode=%d", szPath ? szPath : "(null)", (int)eMode);

    if (szPath == NULL || pptFile == NULL)
    {
        LOG_ERROR("NULL parameter: szPath=%p pptFile=%p",
                  (void *)szPath, (void *)pptFile);
        return ST_ERROR;
    }

    switch (eMode)
    {
    case FILE_MODE_READ:   szFopenMode = "rb";  break;
    case FILE_MODE_WRITE:  szFopenMode = "wb";  break;
    case FILE_MODE_APPEND: szFopenMode = "ab";  break;
    default:
        LOG_ERROR("unknown file mode %d", (int)eMode);
        return ST_ERROR;
    }

    ptFile = (file_t *)malloc(sizeof(file_t));
    if (ptFile == NULL)
    {
        LOG_ERROR("malloc failed for file_t");
        return ST_ERROR;
    }

    ptFile->pHandle = fopen(szPath, szFopenMode);
    if (ptFile->pHandle == NULL)
    {
        LOG_ERROR("fopen('%s', '%s') failed: %s",
                  szPath, szFopenMode, strerror(errno));
        free(ptFile);
        return ST_ERROR;
    }

    ptFile->eMode = eMode;
    strncpy(ptFile->szPath, szPath, ST_MAX_PATH - 1);
    ptFile->szPath[ST_MAX_PATH - 1] = '\0';

    *pptFile = ptFile;

    LOG_INFO("opened '%s' mode=%d", szPath, (int)eMode);
    return ST_NO_ERROR;
}

st_error_t file_read(file_t *ptFile,
                      void   *pBuf,
                      size_t  uiLen,
                      size_t *puiRead)
{
    size_t uiGot;

    if (ptFile == NULL || pBuf == NULL || puiRead == NULL)
    {
        LOG_ERROR("NULL parameter: ptFile=%p pBuf=%p puiRead=%p",
                  (void *)ptFile, pBuf, (void *)puiRead);
        return ST_ERROR;
    }

    if (uiLen == 0)
    {
        *puiRead = 0;
        return ST_NO_ERROR;
    }

    uiGot    = fread(pBuf, 1, uiLen, ptFile->pHandle);
    *puiRead = uiGot;

    if (uiGot < uiLen && ferror(ptFile->pHandle))
    {
        LOG_ERROR("fread error on '%s': %s",
                  ptFile->szPath, strerror(errno));
        return ST_ERROR;
    }

    return ST_NO_ERROR;
}

st_error_t file_write(file_t     *ptFile,
                       const void *pBuf,
                       size_t      uiLen)
{
    size_t uiWritten;

    if (ptFile == NULL || pBuf == NULL)
    {
        LOG_ERROR("NULL parameter: ptFile=%p pBuf=%p",
                  (void *)ptFile, pBuf);
        return ST_ERROR;
    }

    if (uiLen == 0)
    {
        LOG_ERROR("file_write called with uiLen == 0");
        return ST_ERROR;
    }

    uiWritten = fwrite(pBuf, 1, uiLen, ptFile->pHandle);

    if (uiWritten != uiLen)
    {
        LOG_ERROR("fwrite short write on '%s': wrote %zu of %zu: %s",
                  ptFile->szPath, uiWritten, uiLen, strerror(errno));
        return ST_ERROR;
    }

    return ST_NO_ERROR;
}

st_error_t file_close(file_t **pptFile)
{
    int iRet;

    if (pptFile == NULL)
    {
        LOG_ERROR("pptFile is NULL");
        return ST_ERROR;
    }

    if (*pptFile == NULL)
    {
        return ST_NO_ERROR; /* idempotent */
    }

    iRet = fclose((*pptFile)->pHandle);
    if (iRet != 0)
    {
        LOG_ERROR("fclose('%s') failed: %s",
                  (*pptFile)->szPath, strerror(errno));
        free(*pptFile);
        *pptFile = NULL;
        return ST_ERROR;
    }

    LOG_INFO("closed '%s'", (*pptFile)->szPath);
    free(*pptFile);
    *pptFile = NULL;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API — directory operations
 * ------------------------------------------------------------------ */

st_error_t file_mkdir(const char *szPath)
{
    int iRet;

    LOG_TRACE("szPath='%s'", szPath ? szPath : "(null)");

    if (szPath == NULL)
    {
        LOG_ERROR("szPath is NULL");
        return ST_ERROR;
    }

    iRet = FILE_MKDIR(szPath);

    if (iRet != 0 && errno != EEXIST)
    {
        LOG_ERROR("mkdir('%s') failed: %s", szPath, strerror(errno));
        return ST_ERROR;
    }

    LOG_INFO("mkdir '%s' %s",
             szPath, (errno == EEXIST) ? "(already exists)" : "OK");
    return ST_NO_ERROR;
}

st_error_t file_list_dir(const char   *szPath,
                           st_bool_t    bShowHidden,
                           file_list_fn pfnCallback,
                           void        *pCtx)
{
    DIR           *pDir;
    struct dirent *ptEnt;
    file_stat_t    tStat;
    char           szFull[ST_MAX_PATH];
    int            iLen;

    LOG_TRACE("szPath='%s' bShowHidden=%d",
              szPath ? szPath : "(null)", (int)bShowHidden);

    if (szPath == NULL || pfnCallback == NULL)
    {
        LOG_ERROR("NULL parameter: szPath=%p pfnCallback=%p",
                  (void *)szPath, (void *)(size_t)pfnCallback);
        return ST_ERROR;
    }

    pDir = opendir(szPath);
    if (pDir == NULL)
    {
        LOG_ERROR("opendir('%s') failed: %s", szPath, strerror(errno));
        return ST_ERROR;
    }

    while ((ptEnt = readdir(pDir)) != NULL)
    {
        if (strcmp(ptEnt->d_name, ".")  == 0) continue;
        if (strcmp(ptEnt->d_name, "..") == 0) continue;

        if (bShowHidden == ST_FALSE && ptEnt->d_name[0] == '.') continue;

        /* Build full path */
        iLen = snprintf(szFull, sizeof(szFull), "%s/%s",
                        szPath, ptEnt->d_name);
        if (iLen < 0 || iLen >= (int)sizeof(szFull))
        {
            LOG_ERROR("path too long: '%s/%s' — skipping",
                      szPath, ptEnt->d_name);
            continue;
        }

        if (file_stat(szFull, &tStat) != ST_NO_ERROR)
        {
            continue; /* stat failed — skip silently */
        }

        pfnCallback(szFull, ptEnt->d_name, &tStat, pCtx);
    }

    closedir(pDir);
    LOG_INFO("list_dir '%s' complete", szPath);
    return ST_NO_ERROR;
}
