/*
 * image_msa.c - ST4Ever MSA disk image compression/decompression
 *
 * MSA RLE algorithm:
 *   Decompression: bytes other than 0xE5 are copied as-is; 0xE5 begins
 *   a 3-byte escape sequence (run_byte + count_hi + count_lo) that emits
 *   run_byte repeated count times.
 *
 *   Compression: runs of ≥5 identical bytes, and any run of 0xE5 bytes
 *   (even length 1, because literal 0xE5 is not allowed in the stream),
 *   are encoded as escape sequences.  All other bytes are stored raw.
 *   If the compressed form of a track is not shorter than the raw form
 *   (IMSA_TRACK_BYTES = 4608 bytes), the raw form is written instead.
 */

#include "image_msa.h"
#include "file.h"
#include "trace.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal BE-16 helpers                                               */
/* ------------------------------------------------------------------ */

static st_u16_t imsa_rd16be(const st_u8_t *p)
{
    return (st_u16_t)((st_u16_t)p[0] << 8 | (st_u16_t)p[1]);
}

static void imsa_wr16be(st_u8_t *p, st_u16_t v)
{
    p[0] = (st_u8_t)((v >> 8) & 0xFFu);
    p[1] = (st_u8_t)(v & 0xFFu);
}

/* ------------------------------------------------------------------ */
/* Decompression                                                         */
/* ------------------------------------------------------------------ */

static st_error_t imsa_decompress(
    const st_u8_t *pIn,  st_u16_t uiInLen,
    st_u8_t       *pOut, st_u16_t uiOutLen)
{
    st_u16_t uiIn  = 0;
    st_u16_t uiOut = 0;
    st_u8_t  bByte;
    st_u8_t  bRun;
    st_u16_t uiCnt;
    st_u16_t uiK;

    while (uiIn < uiInLen)
    {
        bByte = pIn[uiIn++];
        if (bByte != (st_u8_t)IMSA_ESCAPE)
        {
            if (uiOut >= uiOutLen)
            {
                LOG_ERROR("MSA decompress: output overflow at %u",
                          (unsigned)uiOut);
                return ST_ERROR;
            }
            pOut[uiOut++] = bByte;
        }
        else
        {
            /* Need 3 more bytes: run_byte + count_hi + count_lo */
            if ((st_u16_t)(uiInLen - uiIn) < 3u)
            {
                LOG_ERROR("MSA decompress: truncated escape sequence");
                return ST_ERROR;
            }
            bRun  = pIn[uiIn++];
            uiCnt = imsa_rd16be(pIn + uiIn);
            uiIn += 2;
            if (uiCnt == 0u ||
                (st_u32_t)uiOut + (st_u32_t)uiCnt > (st_u32_t)uiOutLen)
            {
                LOG_ERROR(
                    "MSA decompress: run overflow cnt=%u out=%u max=%u",
                    (unsigned)uiCnt, (unsigned)uiOut, (unsigned)uiOutLen);
                return ST_ERROR;
            }
            for (uiK = 0; uiK < uiCnt; uiK++)
                pOut[uiOut++] = bRun;
        }
    }
    if (uiOut != uiOutLen)
    {
        LOG_ERROR("MSA decompress: short output %u != %u",
                  (unsigned)uiOut, (unsigned)uiOutLen);
        return ST_ERROR;
    }
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------ */
/* Compression                                                           */
/* ------------------------------------------------------------------ */

/*
 * Compress pIn[uiInLen] into pOut[0..uiOutMax-1] using MSA RLE.
 * Returns compressed length, or 0 if the compressed form does not fit
 * in uiOutMax bytes (caller should write the raw track instead).
 */
static st_u16_t imsa_compress(
    const st_u8_t *pIn,  st_u16_t uiInLen,
    st_u8_t       *pOut, st_u16_t uiOutMax)
{
    st_u16_t uiIn  = 0;
    st_u16_t uiOut = 0;
    st_u16_t uiRun;
    st_u16_t uiK;
    st_u8_t  bVal;

    while (uiIn < uiInLen)
    {
        bVal  = pIn[uiIn];
        uiRun = 1;
        while ((st_u16_t)(uiIn + uiRun) < uiInLen &&
               pIn[uiIn + uiRun] == bVal &&
               uiRun < 0xFFFFu)
            uiRun++;

        if (bVal == (st_u8_t)IMSA_ESCAPE || uiRun >= 5u)
        {
            /* Encode as RLE escape sequence (4 bytes) */
            if ((st_u16_t)(uiOut + 4u) > uiOutMax)
                return 0u;  /* compressed >= raw */
            pOut[uiOut++] = (st_u8_t)IMSA_ESCAPE;
            pOut[uiOut++] = bVal;
            pOut[uiOut++] = (st_u8_t)((uiRun >> 8) & 0xFFu);
            pOut[uiOut++] = (st_u8_t)(uiRun & 0xFFu);
        }
        else
        {
            /* Store run of non-escape bytes literally */
            if ((st_u16_t)(uiOut + uiRun) > uiOutMax)
                return 0u;  /* compressed >= raw */
            for (uiK = 0; uiK < uiRun; uiK++)
                pOut[uiOut++] = bVal;
        }
        uiIn += uiRun;
    }
    return uiOut;
}

/* ================================================================== */
/* Public API                                                           */
/* ================================================================== */

st_error_t image_msa_load(const char *szPath, image_st_t **pptImg)
{
    st_u8_t     aHdr[IMSA_HDR_SIZE];
    st_u8_t     aBuf[IMSA_TRACK_BYTES]; /* compressed or raw track data  */
    st_u8_t     aLenBuf[2];
    file_t     *ptFile    = NULL;
    image_st_t *ptImg     = NULL;
    st_u8_t    *pDisk     = NULL;
    size_t      uiRead;
    st_u16_t    uiMagic;
    st_u16_t    uiSpt;
    st_u16_t    uiSides;
    st_u16_t    uiFirst;
    st_u16_t    uiLast;
    st_u16_t    uiDataLen;
    st_u32_t    uiOffset;
    int         iTrack;
    int         iSide;
    int         iNumSides;

    LOG_TRACE("szPath=%s", szPath ? szPath : "NULL");

    if (szPath == NULL || pptImg == NULL)
    {
        LOG_ERROR("NULL parameter: szPath=%p pptImg=%p",
                  (void *)szPath, (void *)pptImg);
        return ST_ERROR;
    }

    if (file_open(szPath, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
        return ST_ERROR;

    /* --- Read and validate the 10-byte header --- */
    if (file_read(ptFile, aHdr, IMSA_HDR_SIZE, &uiRead) != ST_NO_ERROR ||
        uiRead != IMSA_HDR_SIZE)
    {
        LOG_ERROR("MSA load: cannot read header from '%s'", szPath);
        file_close(&ptFile);
        return ST_ERROR;
    }

    uiMagic = imsa_rd16be(aHdr + 0);
    uiSpt   = imsa_rd16be(aHdr + 2);
    uiSides = imsa_rd16be(aHdr + 4);
    uiFirst = imsa_rd16be(aHdr + 6);
    uiLast  = imsa_rd16be(aHdr + 8);

    if (uiMagic != IMSA_MAGIC)
    {
        LOG_ERROR("MSA load: bad magic $%04X (expected $%04X) in '%s'",
                  (unsigned)uiMagic, IMSA_MAGIC, szPath);
        file_close(&ptFile);
        return ST_ERROR;
    }
    if (uiSpt == 0u || uiSides > 1u ||
        uiFirst > uiLast || uiLast >= (st_u16_t)IST_TRACKS)
    {
        LOG_ERROR(
            "MSA load: bad geometry spt=%u sides=%u first=%u last=%u",
            (unsigned)uiSpt,   (unsigned)uiSides,
            (unsigned)uiFirst, (unsigned)uiLast);
        file_close(&ptFile);
        return ST_ERROR;
    }

    /* --- Allocate destination image --- */
    if (image_st_create(&ptImg) != ST_NO_ERROR)
    {
        file_close(&ptFile);
        return ST_ERROR;
    }
    if (image_st_get_disk(ptImg, &pDisk) != ST_NO_ERROR)
    {
        image_st_close(&ptImg);
        file_close(&ptFile);
        return ST_ERROR;
    }

    iNumSides = (int)uiSides + 1;

    /* --- Read and decode each track --- */
    for (iTrack = (int)uiFirst; iTrack <= (int)uiLast; iTrack++)
    {
        for (iSide = 0; iSide < iNumSides; iSide++)
        {
            uiOffset =
                (st_u32_t)(iTrack * IST_SIDES + iSide) *
                (st_u32_t)(IST_SPT * IST_SECTOR_SIZE);

            /* Read 2-byte data length */
            if (file_read(ptFile, aLenBuf, 2u, &uiRead) != ST_NO_ERROR
                || uiRead != 2u)
            {
                LOG_ERROR(
                    "MSA load: short read on length t=%d s=%d",
                    iTrack, iSide);
                image_st_close(&ptImg);
                file_close(&ptFile);
                return ST_ERROR;
            }
            uiDataLen = imsa_rd16be(aLenBuf);

            if ((st_u32_t)uiDataLen > (st_u32_t)IMSA_TRACK_BYTES)
            {
                LOG_ERROR(
                    "MSA load: data_len=%u > track_bytes=%u t=%d s=%d",
                    (unsigned)uiDataLen, IMSA_TRACK_BYTES,
                    iTrack, iSide);
                image_st_close(&ptImg);
                file_close(&ptFile);
                return ST_ERROR;
            }

            /* Read compressed / raw data */
            if (file_read(ptFile, aBuf, (size_t)uiDataLen, &uiRead)
                    != ST_NO_ERROR ||
                uiRead != (size_t)uiDataLen)
            {
                LOG_ERROR(
                    "MSA load: short read t=%d s=%d wanted=%u got=%u",
                    iTrack, iSide,
                    (unsigned)uiDataLen, (unsigned)uiRead);
                image_st_close(&ptImg);
                file_close(&ptFile);
                return ST_ERROR;
            }

            /* Decode into the raw disk buffer */
            if ((st_u32_t)uiDataLen == (st_u32_t)IMSA_TRACK_BYTES)
            {
                memcpy(pDisk + uiOffset, aBuf, IMSA_TRACK_BYTES);
            }
            else
            {
                if (imsa_decompress(aBuf, uiDataLen,
                                    pDisk + uiOffset,
                                    (st_u16_t)IMSA_TRACK_BYTES)
                        != ST_NO_ERROR)
                {
                    image_st_close(&ptImg);
                    file_close(&ptFile);
                    return ST_ERROR;
                }
            }
        }
    }

    file_close(&ptFile);
    *pptImg = ptImg;
    LOG_INFO("MSA loaded '%s' tracks %u-%u sides=%d",
             szPath, (unsigned)uiFirst, (unsigned)uiLast, iNumSides);
    return ST_NO_ERROR;
}

st_error_t image_msa_save(image_st_t *ptImg, const char *szPath)
{
    st_u8_t        aHdr[IMSA_HDR_SIZE];
    st_u8_t        aBuf[IMSA_TRACK_BYTES]; /* compression workspace      */
    st_u8_t        aLenBuf[2];
    file_t        *ptFile    = NULL;
    st_u8_t       *pDisk     = NULL;
    const st_u8_t *pTrack;
    const st_u8_t *pData;
    st_u16_t       uiCompLen;
    st_u16_t       uiDataLen;
    int            iTrack;
    int            iSide;

    LOG_TRACE("ptImg=%p szPath=%s",
              (void *)ptImg, szPath ? szPath : "NULL");

    if (ptImg == NULL || szPath == NULL)
    {
        LOG_ERROR("NULL parameter: ptImg=%p szPath=%p",
                  (void *)ptImg, (void *)szPath);
        return ST_ERROR;
    }
    if (image_st_get_disk(ptImg, &pDisk) != ST_NO_ERROR)
        return ST_ERROR;

    /* --- Build and write the 10-byte MSA header --- */
    imsa_wr16be(aHdr + 0, (st_u16_t)IMSA_MAGIC);
    imsa_wr16be(aHdr + 2, (st_u16_t)IMSA_SPT);
    imsa_wr16be(aHdr + 4, (st_u16_t)IMSA_SIDES_HDR);
    imsa_wr16be(aHdr + 6, (st_u16_t)IMSA_TRACK_FIRST);
    imsa_wr16be(aHdr + 8, (st_u16_t)IMSA_TRACK_LAST);

    if (file_open(szPath, FILE_MODE_WRITE, &ptFile) != ST_NO_ERROR)
        return ST_ERROR;

    if (file_write(ptFile, aHdr, IMSA_HDR_SIZE) != ST_NO_ERROR)
    {
        file_close(&ptFile);
        return ST_ERROR;
    }

    /* --- Encode and write each track --- */
    for (iTrack = 0; iTrack < IST_TRACKS; iTrack++)
    {
        for (iSide = 0; iSide < IST_SIDES; iSide++)
        {
            pTrack = pDisk +
                     (st_u32_t)(iTrack * IST_SIDES + iSide) *
                     (st_u32_t)(IST_SPT * IST_SECTOR_SIZE);

            uiCompLen = imsa_compress(pTrack, (st_u16_t)IMSA_TRACK_BYTES,
                                      aBuf,   (st_u16_t)IMSA_TRACK_BYTES);

            if (uiCompLen > 0u && uiCompLen < (st_u16_t)IMSA_TRACK_BYTES)
            {
                uiDataLen = uiCompLen;
                pData     = aBuf;
            }
            else
            {
                uiDataLen = (st_u16_t)IMSA_TRACK_BYTES;
                pData     = pTrack;
            }

            imsa_wr16be(aLenBuf, uiDataLen);
            if (file_write(ptFile, aLenBuf, 2u) != ST_NO_ERROR ||
                file_write(ptFile, pData, (size_t)uiDataLen) != ST_NO_ERROR)
            {
                file_close(&ptFile);
                return ST_ERROR;
            }
        }
    }

    file_close(&ptFile);
    LOG_INFO("MSA saved '%s'", szPath);
    return ST_NO_ERROR;
}
