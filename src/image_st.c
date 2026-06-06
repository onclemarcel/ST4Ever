/*
 * image_st.c - ST4Ever Atari ST .st disk image read/write
 *
 * Implements FAT12 load / create / save / list / read / write / delete
 * for standard Atari ST DD 3.5" images (IST_DISK_SIZE = 737,280 bytes).
 *
 * The complete disk is held in a heap-allocated image_st_t; all mutations
 * stay in memory until image_st_save() writes them back to disk.
 *
 * FAT12 encoding (little-endian pairs of 12-bit fields):
 *   Even cluster N : bytes[N*3/2] | (bytes[N*3/2+1] & 0x0F) << 8
 *   Odd  cluster N : bytes[N*3/2] >> 4 | bytes[N*3/2+1] << 4
 */

#include "image_st.h"
#include "file.h"
#include "trace.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------
 * Internal structure
 * ------------------------------------------------------------------ */

struct image_st_s
{
    st_u8_t aDisk[IST_DISK_SIZE]; /* complete in-memory disk image */
};

/* ------------------------------------------------------------------
 * Internal helpers — LE integer I/O
 * ------------------------------------------------------------------ */

static st_u16_t ist_rd16(const st_u8_t *p)
{
    return (st_u16_t)(p[0] | ((st_u16_t)p[1] << 8));
}

static void ist_wr16(st_u8_t *p, st_u16_t v)
{
    p[0] = (st_u8_t)(v & 0xFFu);
    p[1] = (st_u8_t)((v >> 8) & 0xFFu);
}

static void ist_wr32(st_u8_t *p, st_u32_t v)
{
    p[0] = (st_u8_t)(v & 0xFFu);
    p[1] = (st_u8_t)((v >> 8)  & 0xFFu);
    p[2] = (st_u8_t)((v >> 16) & 0xFFu);
    p[3] = (st_u8_t)((v >> 24) & 0xFFu);
}

/* ------------------------------------------------------------------
 * Internal helpers — FAT12 access
 * ------------------------------------------------------------------ */

static st_u16_t ist_fat_get(const image_st_t *ptImg, st_u16_t uiCluster)
{
    const st_u8_t *pFAT;
    st_u32_t       uiByte;

    pFAT   = ptImg->aDisk + (st_u32_t)IST_FAT1_SECTOR * IST_BPS;
    uiByte = ((st_u32_t)uiCluster * 3u) / 2u;

    if (uiCluster & 1u)
        return (st_u16_t)(((st_u16_t)pFAT[uiByte] >> 4) |
                           ((st_u16_t)pFAT[uiByte + 1u] << 4));
    else
        return (st_u16_t)((st_u16_t)pFAT[uiByte] |
                          (((st_u16_t)pFAT[uiByte + 1u] & 0x0Fu) << 8));
}

static void ist_fat_set(image_st_t *ptImg,
                        st_u16_t    uiCluster,
                        st_u16_t    uiVal)
{
    st_u8_t  *pFAT1;
    st_u8_t  *pFAT2;
    st_u32_t  uiByte;

    pFAT1  = ptImg->aDisk + (st_u32_t)IST_FAT1_SECTOR * IST_BPS;
    pFAT2  = ptImg->aDisk + (st_u32_t)IST_FAT2_SECTOR * IST_BPS;
    uiByte = ((st_u32_t)uiCluster * 3u) / 2u;

    if (uiCluster & 1u)
    {
        pFAT1[uiByte]      = (st_u8_t)((pFAT1[uiByte] & 0x0Fu) |
                                         ((uiVal & 0x0Fu) << 4));
        pFAT1[uiByte + 1u] = (st_u8_t)(uiVal >> 4);
    }
    else
    {
        pFAT1[uiByte]      = (st_u8_t)(uiVal & 0xFFu);
        pFAT1[uiByte + 1u] = (st_u8_t)((pFAT1[uiByte + 1u] & 0xF0u) |
                                         ((uiVal >> 8) & 0x0Fu));
    }
    /* Mirror to FAT2 */
    pFAT2[uiByte]      = pFAT1[uiByte];
    pFAT2[uiByte + 1u] = pFAT1[uiByte + 1u];
}

/* Return the first free cluster (>= IST_CLUSTER_FIRST), or 0 if full. */
static st_u16_t ist_fat_alloc(image_st_t *ptImg)
{
    st_u16_t uiMax;
    st_u16_t i;

    uiMax = (st_u16_t)(IST_CLUSTER_FIRST + IST_DATA_CLUSTERS);
    for (i = (st_u16_t)IST_CLUSTER_FIRST; i < uiMax; i++)
    {
        if (ist_fat_get(ptImg, i) == (st_u16_t)IST_FAT_FREE)
            return i;
    }
    return 0u; /* disk full */
}

/* Free the entire FAT12 chain starting at uiCluster. */
static void ist_fat_free_chain(image_st_t *ptImg, st_u16_t uiCluster)
{
    st_u16_t uiNext;
    st_u16_t uiMax;

    uiMax = (st_u16_t)(IST_CLUSTER_FIRST + IST_DATA_CLUSTERS);
    while (uiCluster >= (st_u16_t)IST_CLUSTER_FIRST &&
           uiCluster < uiMax)
    {
        uiNext = ist_fat_get(ptImg, uiCluster);
        ist_fat_set(ptImg, uiCluster, (st_u16_t)IST_FAT_FREE);
        if (uiNext >= (st_u16_t)IST_FAT_EOC)
            break;
        uiCluster = uiNext;
    }
}

/* Byte offset into aDisk for the start of a data cluster. */
static st_u32_t ist_cluster_offset(st_u16_t uiCluster)
{
    return (st_u32_t)(IST_DATA_SECTOR +
                       ((st_u32_t)uiCluster - IST_CLUSTER_FIRST) * IST_SPC)
           * IST_BPS;
}

/* ------------------------------------------------------------------
 * Internal helpers — 8.3 name conversion
 * ------------------------------------------------------------------ */

/*
 * Convert raw 11-byte FAT name (space-padded, no dot) to "NAME.EXT\0".
 * Returns szDst.
 */
static char *ist_raw_to_name(const st_u8_t *pRaw, char *szDst)
{
    int iNameLen;
    int iExtLen;
    int iPos;
    int i;

    iNameLen = 0;
    iExtLen  = 0;
    for (i = 7; i >= 0; i--)
    {
        if (pRaw[i] != ' ')
        {
            iNameLen = i + 1;
            break;
        }
    }
    for (i = 2; i >= 0; i--)
    {
        if (pRaw[8 + i] != ' ')
        {
            iExtLen = i + 1;
            break;
        }
    }

    iPos = 0;
    for (i = 0; i < iNameLen; i++)
        szDst[iPos++] = (char)pRaw[i];
    if (iExtLen > 0)
    {
        szDst[iPos++] = '.';
        for (i = 0; i < iExtLen; i++)
            szDst[iPos++] = (char)pRaw[8 + i];
    }
    szDst[iPos] = '\0';
    return szDst;
}

/*
 * Convert human filename to 11-byte FAT 8.3 form (uppercase, padded).
 * Returns ST_TRUE on success, ST_FALSE if name is invalid.
 */
static st_bool_t ist_name_to_83(const char *szName, st_u8_t *pOut)
{
    const char *pDot;
    const char *pBase;
    const char *pExt;
    int         iNameLen;
    int         iExtLen;
    int         i;
    int         iLen;

    if (szName == NULL || pOut == NULL)
        return ST_FALSE;

    memset(pOut, ' ', 11);
    pDot     = NULL;
    iLen     = 0;

    for (i = 0; szName[i] != '\0'; i++)
    {
        if (szName[i] == '.')
            pDot = szName + i;
        iLen++;
    }

    if (pDot != NULL)
    {
        pBase    = szName;
        iNameLen = (int)(pDot - szName);
        pExt     = pDot + 1;
        iExtLen  = (int)strlen(pExt);
    }
    else
    {
        pBase    = szName;
        iNameLen = iLen;
        pExt     = NULL;
        iExtLen  = 0;
    }

    if (iNameLen == 0 || iNameLen > 8 || iExtLen > 3)
        return ST_FALSE;

    for (i = 0; i < iNameLen; i++)
        pOut[i] = (st_u8_t)toupper((unsigned char)pBase[i]);
    for (i = 0; i < iExtLen; i++)
        pOut[8 + i] = (st_u8_t)toupper((unsigned char)pExt[i]);

    return ST_TRUE;
}

/* Case-insensitive check: does szName match the raw 11-byte FAT name? */
static st_bool_t ist_name_match(const char *szName, const st_u8_t *pRaw)
{
    st_u8_t aCanon[11];

    if (!ist_name_to_83(szName, aCanon))
        return ST_FALSE;
    return (memcmp(aCanon, pRaw, 11) == 0) ? ST_TRUE : ST_FALSE;
}

/* ------------------------------------------------------------------
 * Internal helpers — root directory
 * ------------------------------------------------------------------ */

static st_u8_t *ist_root_entry(image_st_t *ptImg, int iIdx)
{
    return ptImg->aDisk +
           (st_u32_t)IST_ROOT_SECTOR * IST_BPS +
           (st_u32_t)iIdx * 32u;
}

/* Return index of the named entry, or -1 if not found. */
static int ist_root_find(image_st_t *ptImg, const char *szName)
{
    st_u8_t *pE;
    int      i;

    for (i = 0; i < IST_RDE; i++)
    {
        pE = ist_root_entry(ptImg, i);
        if (pE[0] == 0x00u)
            break;
        if (pE[0] == 0xE5u)
            continue;
        if (ist_name_match(szName, pE))
            return i;
    }
    return -1;
}

/* Return index of a free (empty or deleted) root slot, or -1 if full. */
static int ist_root_alloc_slot(image_st_t *ptImg)
{
    st_u8_t *pE;
    int      i;

    for (i = 0; i < IST_RDE; i++)
    {
        pE = ist_root_entry(ptImg, i);
        if (pE[0] == 0x00u || pE[0] == 0xE5u)
            return i;
    }
    return -1;
}

/* ------------------------------------------------------------------
 * Internal — boot sector format and validation
 * ------------------------------------------------------------------ */

static void ist_format_boot(image_st_t *ptImg)
{
    st_u8_t *pBoot;

    pBoot = ptImg->aDisk; /* sector 0 */
    memset(pBoot, 0, IST_BPS);

    pBoot[0] = 0xEB; /* JMP SHORT */
    pBoot[1] = 0x3C;
    pBoot[2] = 0x90; /* NOP */
    memcpy(pBoot + 3, "ST4EVER ", 8);

    ist_wr16(pBoot + 0x0B, (st_u16_t)IST_BPS);
    pBoot[0x0D] = (st_u8_t)IST_SPC;
    ist_wr16(pBoot + 0x0E, (st_u16_t)IST_RES);
    pBoot[0x10] = (st_u8_t)IST_NFATS;
    ist_wr16(pBoot + 0x11, (st_u16_t)IST_RDE);
    ist_wr16(pBoot + 0x13, (st_u16_t)IST_TS);
    pBoot[0x15] = (st_u8_t)IST_MEDIA;
    ist_wr16(pBoot + 0x16, (st_u16_t)IST_SPF);
    ist_wr16(pBoot + 0x18, (st_u16_t)IST_SPT);
    ist_wr16(pBoot + 0x1A, (st_u16_t)IST_HEADS);
    ist_wr16(pBoot + 0x1C, 0u); /* hidden sectors */

    pBoot[510] = 0x55u;
    pBoot[511] = 0xAAu;
}

static st_bool_t ist_validate_bpb(const image_st_t *ptImg)
{
    const st_u8_t *pBoot;

    pBoot = ptImg->aDisk;
    if (ist_rd16(pBoot + 0x0B) != (st_u16_t)IST_BPS)
        return ST_FALSE;
    if (ist_rd16(pBoot + 0x13) != (st_u16_t)IST_TS)
        return ST_FALSE;
    return ST_TRUE;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_error_t image_st_create(image_st_t **pptImg)
{
    image_st_t *ptImg;

    LOG_TRACE("pptImg=%p", (void *)pptImg);
    if (pptImg == NULL)
    {
        LOG_ERROR("NULL parameter: pptImg");
        return ST_ERROR;
    }
    *pptImg = NULL;

    ptImg = (image_st_t *)malloc(sizeof(image_st_t));
    if (ptImg == NULL)
    {
        LOG_ERROR("malloc failed for image_st_t");
        return ST_ERROR;
    }
    memset(ptImg->aDisk, 0, IST_DISK_SIZE);

    ist_format_boot(ptImg);
    ist_fat_set(ptImg, 0u, (st_u16_t)IST_FAT_MEDIA_BYTE);
    ist_fat_set(ptImg, 1u, 0xFFFu);

    *pptImg = ptImg;
    LOG_INFO("blank .st image created in memory (%u bytes)", IST_DISK_SIZE);
    return ST_NO_ERROR;
}

st_error_t image_st_load(const char *szPath, image_st_t **pptImg)
{
    image_st_t *ptImg;
    file_t     *ptFile;
    size_t      uiRead;

    LOG_TRACE("szPath='%s'", szPath ? szPath : "(null)");
    if (szPath == NULL || pptImg == NULL)
    {
        LOG_ERROR("NULL parameter: szPath=%p pptImg=%p",
                  (void *)szPath, (void *)pptImg);
        return ST_ERROR;
    }
    *pptImg = NULL;

    ptImg = (image_st_t *)malloc(sizeof(image_st_t));
    if (ptImg == NULL)
    {
        LOG_ERROR("malloc failed for image_st_t");
        return ST_ERROR;
    }

    if (file_open(szPath, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
    {
        LOG_ERROR("cannot open '%s'", szPath);
        free(ptImg);
        return ST_ERROR;
    }

    uiRead = 0;
    if (file_read(ptFile, ptImg->aDisk, IST_DISK_SIZE, &uiRead)
        != ST_NO_ERROR || uiRead != IST_DISK_SIZE)
    {
        LOG_ERROR("'%s': expected %u bytes, got %zu",
                  szPath, (unsigned)IST_DISK_SIZE, uiRead);
        file_close(&ptFile);
        free(ptImg);
        return ST_ERROR;
    }
    file_close(&ptFile);

    if (!ist_validate_bpb(ptImg))
    {
        LOG_ERROR("'%s': BPB validation failed (not a standard ST DD image)",
                  szPath);
        free(ptImg);
        return ST_ERROR;
    }

    *pptImg = ptImg;
    LOG_INFO("loaded '%s'", szPath);
    return ST_NO_ERROR;
}

st_error_t image_st_save(image_st_t *ptImg, const char *szPath)
{
    file_t *ptFile;

    LOG_TRACE("ptImg=%p szPath='%s'",
              (void *)ptImg, szPath ? szPath : "(null)");
    if (ptImg == NULL || szPath == NULL)
    {
        LOG_ERROR("NULL parameter: ptImg=%p szPath=%p",
                  (void *)ptImg, (void *)szPath);
        return ST_ERROR;
    }

    if (file_open(szPath, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
    {
        LOG_ERROR("cannot create '%s'", szPath);
        return ST_ERROR;
    }

    if (file_write(ptFile, ptImg->aDisk, IST_DISK_SIZE) != ST_NO_ERROR)
    {
        LOG_ERROR("write error for '%s'", szPath);
        file_close(&ptFile);
        return ST_ERROR;
    }
    file_close(&ptFile);

    LOG_INFO("saved '%s' (%u bytes)", szPath, (unsigned)IST_DISK_SIZE);
    return ST_NO_ERROR;
}

st_error_t image_st_close(image_st_t **pptImg)
{
    LOG_TRACE("pptImg=%p", (void *)pptImg);
    if (pptImg == NULL)
    {
        LOG_ERROR("NULL parameter: pptImg");
        return ST_ERROR;
    }
    if (*pptImg != NULL)
    {
        free(*pptImg);
        *pptImg = NULL;
    }
    return ST_NO_ERROR;
}

st_error_t image_st_list_root(image_st_t        *ptImg,
                               image_st_dirent_t *aEntries,
                               int                iMaxEntries,
                               int               *piCount)
{
    st_u8_t *pE;
    char     szName[IST_NAME_MAX];
    int      i;
    int      iCount;

    LOG_TRACE("ptImg=%p iMax=%d", (void *)ptImg, iMaxEntries);
    if (ptImg == NULL || aEntries == NULL || piCount == NULL ||
        iMaxEntries <= 0)
    {
        LOG_ERROR("invalid parameter");
        return ST_ERROR;
    }

    iCount = 0;
    for (i = 0; i < IST_RDE && iCount < iMaxEntries; i++)
    {
        pE = ist_root_entry(ptImg, i);

        if (pE[0] == 0x00u)
            break;
        if (pE[0] == 0xE5u)
            continue;
        if (pE[11] & IST_ATTR_VOLNAME)
            continue;

        ist_raw_to_name(pE, szName);
        strncpy(aEntries[iCount].szName, szName, IST_NAME_MAX - 1);
        aEntries[iCount].szName[IST_NAME_MAX - 1] = '\0';

        aEntries[iCount].uiSize =
            (st_u32_t)pE[28]               |
            ((st_u32_t)pE[29] << 8)        |
            ((st_u32_t)pE[30] << 16)       |
            ((st_u32_t)pE[31] << 24);

        aEntries[iCount].uiAttr    = pE[11];
        aEntries[iCount].uiCluster = ist_rd16(pE + 26);
        aEntries[iCount].bIsDir    = (pE[11] & IST_ATTR_SUBDIR) ?
                                     ST_TRUE : ST_FALSE;
        iCount++;
    }

    *piCount = iCount;
    return ST_NO_ERROR;
}

st_error_t image_st_read_file(image_st_t *ptImg,
                               st_u16_t    uiCluster,
                               st_u32_t    uiSize,
                               st_u8_t    *pBuf,
                               st_u32_t    uiBufSize)
{
    st_u32_t uiClusterBytes;
    st_u32_t uiRemaining;
    st_u32_t uiCopy;
    st_u16_t uiMaxCluster;

    LOG_TRACE("ptImg=%p cluster=%u size=%u",
              (void *)ptImg, (unsigned)uiCluster, (unsigned)uiSize);
    if (ptImg == NULL || pBuf == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    if (uiSize > uiBufSize)
    {
        LOG_ERROR("buffer too small: uiSize=%u uiBufSize=%u",
                  (unsigned)uiSize, (unsigned)uiBufSize);
        return ST_ERROR;
    }

    uiClusterBytes = (st_u32_t)IST_SPC * IST_BPS;
    uiMaxCluster   = (st_u16_t)(IST_CLUSTER_FIRST + IST_DATA_CLUSTERS);
    uiRemaining    = uiSize;

    while (uiRemaining > 0)
    {
        st_u8_t *pSrc;

        if (uiCluster < (st_u16_t)IST_CLUSTER_FIRST ||
            uiCluster >= uiMaxCluster)
        {
            LOG_ERROR("cluster %u out of range", (unsigned)uiCluster);
            return ST_ERROR;
        }

        pSrc   = ptImg->aDisk + ist_cluster_offset(uiCluster);
        uiCopy = ST_MIN(uiRemaining, uiClusterBytes);
        memcpy(pBuf, pSrc, uiCopy);
        pBuf        += uiCopy;
        uiRemaining -= uiCopy;

        if (uiRemaining == 0)
            break;

        uiCluster = ist_fat_get(ptImg, uiCluster);
        if (uiCluster >= (st_u16_t)IST_FAT_EOC)
        {
            LOG_ERROR("FAT chain ended early, %u bytes remaining",
                      (unsigned)uiRemaining);
            return ST_ERROR;
        }
    }
    return ST_NO_ERROR;
}

st_error_t image_st_write_file(image_st_t    *ptImg,
                                const char    *szName,
                                const st_u8_t *pData,
                                st_u32_t       uiSize)
{
    st_u8_t  aName83[11];
    st_u8_t *pEntry;
    st_u32_t uiClusterBytes;
    st_u32_t uiRemaining;
    st_u32_t uiCopy;
    st_u16_t uiFirst;
    st_u16_t uiPrev;
    st_u16_t uiNew;
    int      iSlot;

    LOG_TRACE("ptImg=%p name='%s' size=%u",
              (void *)ptImg, szName ? szName : "(null)", (unsigned)uiSize);
    if (ptImg == NULL || szName == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    if (uiSize > 0u && pData == NULL)
    {
        LOG_ERROR("pData is NULL for non-empty file");
        return ST_ERROR;
    }

    if (!ist_name_to_83(szName, aName83))
    {
        LOG_ERROR("invalid 8.3 name: '%s'", szName);
        return ST_ERROR;
    }

    /* Remove existing file with the same name */
    {
        int iEx;
        iEx = ist_root_find(ptImg, szName);
        if (iEx >= 0)
        {
            pEntry = ist_root_entry(ptImg, iEx);
            ist_fat_free_chain(ptImg, ist_rd16(pEntry + 26));
            pEntry[0] = 0xE5u;
        }
    }

    /* Allocate a root directory slot */
    iSlot = ist_root_alloc_slot(ptImg);
    if (iSlot < 0)
    {
        LOG_ERROR("root directory is full");
        return ST_ERROR;
    }

    /* Allocate FAT clusters and write data */
    uiClusterBytes = (st_u32_t)IST_SPC * IST_BPS;
    uiFirst        = 0u;
    uiPrev         = 0u;
    uiRemaining    = uiSize;

    while (uiRemaining > 0u)
    {
        st_u8_t *pDst;

        uiNew = ist_fat_alloc(ptImg);
        if (uiNew == 0u)
        {
            if (uiFirst != 0u)
                ist_fat_free_chain(ptImg, uiFirst);
            LOG_ERROR("disk full writing '%s'", szName);
            return ST_ERROR;
        }

        if (uiPrev != 0u)
            ist_fat_set(ptImg, uiPrev, uiNew);
        else
            uiFirst = uiNew;

        ist_fat_set(ptImg, uiNew, 0xFFFu); /* tentative EOC */

        pDst   = ptImg->aDisk + ist_cluster_offset(uiNew);
        uiCopy = ST_MIN(uiRemaining, uiClusterBytes);
        memcpy(pDst, pData, uiCopy);
        if (uiCopy < uiClusterBytes)
            memset(pDst + uiCopy, 0, uiClusterBytes - uiCopy);
        pData       += uiCopy;
        uiRemaining -= uiCopy;
        uiPrev       = uiNew;
    }

    /* Write root directory entry */
    pEntry = ist_root_entry(ptImg, iSlot);
    memset(pEntry, 0, 32);
    memcpy(pEntry, aName83, 11);
    pEntry[11] = IST_ATTR_ARCHIVE;
    ist_wr16(pEntry + 26, uiFirst);
    ist_wr32(pEntry + 28, uiSize);

    LOG_INFO("written '%s' (%u bytes)", szName, (unsigned)uiSize);
    return ST_NO_ERROR;
}

st_error_t image_st_delete_file(image_st_t *ptImg, const char *szName)
{
    st_u8_t *pEntry;
    int      iSlot;

    LOG_TRACE("ptImg=%p name='%s'",
              (void *)ptImg, szName ? szName : "(null)");
    if (ptImg == NULL || szName == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    iSlot = ist_root_find(ptImg, szName);
    if (iSlot < 0)
    {
        LOG_ERROR("file not found: '%s'", szName);
        return ST_ERROR;
    }

    pEntry = ist_root_entry(ptImg, iSlot);
    ist_fat_free_chain(ptImg, ist_rd16(pEntry + 26));
    pEntry[0] = 0xE5u;

    LOG_INFO("deleted '%s'", szName);
    return ST_NO_ERROR;
}

st_error_t image_st_free_bytes(image_st_t *ptImg, st_u32_t *puiFreeBytes)
{
    st_u16_t uiMax;
    st_u16_t uiFree;
    st_u16_t i;

    LOG_TRACE("ptImg=%p", (void *)ptImg);
    if (ptImg == NULL || puiFreeBytes == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    uiMax  = (st_u16_t)(IST_CLUSTER_FIRST + IST_DATA_CLUSTERS);
    uiFree = 0u;
    for (i = (st_u16_t)IST_CLUSTER_FIRST; i < uiMax; i++)
    {
        if (ist_fat_get(ptImg, i) == (st_u16_t)IST_FAT_FREE)
            uiFree++;
    }

    *puiFreeBytes = (st_u32_t)uiFree * (st_u32_t)IST_SPC * IST_BPS;
    return ST_NO_ERROR;
}

st_error_t image_st_get_disk(image_st_t *ptImg, st_u8_t **ppDisk)
{
    if (ptImg == NULL || ppDisk == NULL)
    {
        LOG_ERROR("NULL parameter: ptImg=%p ppDisk=%p",
                  (void *)ptImg, (void *)ppDisk);
        return ST_ERROR;
    }
    *ppDisk = ptImg->aDisk;
    return ST_NO_ERROR;
}
