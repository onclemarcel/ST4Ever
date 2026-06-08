/*
 * sector_analyze.c - ST4Ever sector fingerprint engine
 *
 * Implements feature extraction and cosine-similarity classification
 * for 512-byte Atari ST disk sectors.
 *
 * Feature computation strategy:
 *   - Structural features (BPB, FAT, directory) use byte-level parsing
 *   - Entropy uses a 256-bin histogram + Shannon formula
 *   - Opcode features use disasm_one() (UC11-14) for a forward scan
 *   - HW address features use raw byte-pattern scan
 *   - Demo features use specific byte signatures
 *
 * Weighted cosine similarity: each feature dimension is pre-scaled
 * by a static weight array before the dot product is computed.
 */

#include "sector_analyze.h"
#include "disassemble.h"
#include "trace.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------
 * Feature weights  (indexed by position in sector_features_t)
 * Higher weight = more discriminating for classification.
 * ------------------------------------------------------------------ */

static const float g_weights[SA_FEATURE_DIM] =
{
    8.0f,  /* fBpbValid        */
    8.0f,  /* fWd1772Bootable  */
    9.0f,  /* fFatPattern      */
    6.0f,  /* fDirDensity      */
    3.0f,  /* fEntropy         */
    6.0f,  /* fZeroRuns        */
    7.0f,  /* fE5Runs          */
    6.0f,  /* fFfRuns          */
    3.0f,  /* fAsciiRatio      */
    4.0f,  /* fRepeating       */
    6.0f,  /* fOpcodeDensity   */
    5.0f,  /* fWordAlignRatio  */
    5.0f,  /* fBranchDensity   */
    5.0f,  /* fRelBranchValid  */
    5.0f,  /* fHwImmediate     */
    7.0f,  /* fHwInContext     */
    6.0f,  /* fTimingLoops     */
    8.0f,  /* fVblInstall      */
    6.0f,  /* fSinusProfile    */
    6.0f,  /* fGraphicsPattern */
    7.0f,  /* fPackerEntropy   */
    7.0f,  /* fYmAccess        */
    7.0f,  /* fVideoAccess     */
    6.0f   /* fFdcAccess       */
};

/* ------------------------------------------------------------------
 * Internal helpers — feature computations
 * ------------------------------------------------------------------ */

static float feat_bpb_valid(const st_u8_t *p)
{
    st_u16_t uiBps  = (st_u16_t)(p[0x0B] | ((st_u16_t)p[0x0C] << 8));
    st_u8_t  uiSpc  = p[0x0D];
    st_u8_t  uiNFat = p[0x10];
    st_u16_t uiTs   = (st_u16_t)(p[0x13] | ((st_u16_t)p[0x14] << 8));
    st_u16_t uiSpt  = (st_u16_t)(p[0x18] | ((st_u16_t)p[0x19] << 8));
    st_u16_t uiHds  = (st_u16_t)(p[0x1A] | ((st_u16_t)p[0x1B] << 8));

    if (uiBps != 512)                        return 0.0f;
    if (uiSpc != 1 && uiSpc != 2)           return 0.0f;
    if (uiNFat < 1 || uiNFat > 2)           return 0.0f;
    if (uiTs < 720  || uiTs  > 1440)        return 0.0f;
    if (uiSpt < 8   || uiSpt > 11)          return 0.0f;
    if (uiHds < 1   || uiHds > 2)           return 0.0f;
    return 1.0f;
}

static float feat_wd1772_bootable(const st_u8_t *p)
{
    st_u32_t uiSum = 0u;
    int      i;

    for (i = 0; i < 256; i++)
        uiSum += ((st_u32_t)p[i * 2] << 8) | (st_u32_t)p[i * 2 + 1];
    return ((uiSum & 0xFFFFu) == 0x1234u) ? 1.0f : 0.0f;
}

static float feat_fat_pattern(const st_u8_t *p)
{
    /* FAT12: byte 0 = media descriptor (0xF0-0xFF),
     * bytes 1-2 = 0xFF 0xFF (cluster 0 EOC nibbles) */
    if (p[0] >= 0xF0u && p[1] == 0xFFu && p[2] == 0xFFu)
        return 1.0f;
    return 0.0f;
}

static float feat_dir_density(const st_u8_t *p)
{
    int iValid = 0;
    int iTotal = (int)(SA_SECTOR_SIZE / 32u);
    int i, j;

    for (i = 0; i < iTotal; i++)
    {
        const st_u8_t *pE    = p + i * 32;
        st_u8_t        uiF   = pE[0];
        st_u8_t        uiA   = pE[11];

        /* End-marker, deleted, or dot-entry are valid dir positions */
        if (uiF == 0x00u || uiF == 0xE5u || uiF == 0x05u)
        {
            iValid++;
            continue;
        }
        /* Otherwise: name chars 0-10 must be printable + attribute valid */
        {
            int bNameOk = 1;
            for (j = 0; j < 11 && bNameOk; j++)
            {
                st_u8_t c = pE[j];
                bNameOk = (c >= 0x20u && c <= 0x7Eu);
            }
            if (bNameOk && (uiA & 0xC0u) == 0u)
                iValid++;
        }
    }
    return (float)iValid / (float)iTotal;
}

static float feat_entropy(const st_u8_t *p)
{
    int   aHist[256];
    float fH = 0.0f;
    int   i;

    memset(aHist, 0, sizeof(aHist));
    for (i = 0; i < (int)SA_SECTOR_SIZE; i++)
        aHist[p[i]]++;

    for (i = 0; i < 256; i++)
    {
        if (aHist[i] == 0) continue;
        {
            float fPi = (float)aHist[i] / (float)SA_SECTOR_SIZE;
            fH -= fPi * (logf(fPi) / 0.693147f); /* log2(x)=ln(x)/ln(2) */
        }
    }
    return fH / 8.0f;
}

static float feat_byte_ratio(const st_u8_t *p, st_u8_t uiVal)
{
    int iCount = 0, i;
    for (i = 0; i < (int)SA_SECTOR_SIZE; i++)
        if (p[i] == uiVal) iCount++;
    return (float)iCount / (float)SA_SECTOR_SIZE;
}

static float feat_ascii_ratio(const st_u8_t *p)
{
    int iCount = 0, i;
    for (i = 0; i < (int)SA_SECTOR_SIZE; i++)
        if (p[i] >= 0x20u && p[i] <= 0x7Eu) iCount++;
    return (float)iCount / (float)SA_SECTOR_SIZE;
}

static float feat_repeating(const st_u8_t *p)
{
    int aHist[256];
    int iMax = 0, i;

    memset(aHist, 0, sizeof(aHist));
    for (i = 0; i < (int)SA_SECTOR_SIZE; i++)
        aHist[p[i]]++;
    for (i = 0; i < 256; i++)
        if (aHist[i] > iMax) iMax = aHist[i];
    return (float)iMax / (float)SA_SECTOR_SIZE;
}

/* Forward-scan from byte 0, returns bytes covered by valid instrs */
static size_t opcode_fwd_scan(const st_u8_t *p, st_u32_t uiBase,
                               int *piInstrs, int *piBranches,
                               int *piRelBranchValid)
{
    disasm_result_t tR;
    size_t          uiPos     = 0u;
    size_t          uiCovered = 0u;
    int             iInstrs   = 0;
    int             iBranches = 0;
    int             iRelOk    = 0;

    *piInstrs        = 0;
    *piBranches      = 0;
    *piRelBranchValid = 0;

    while (uiPos + 2u <= SA_SECTOR_SIZE)
    {
        disasm_one(p + uiPos, SA_SECTOR_SIZE - uiPos,
                   uiBase + (st_u32_t)uiPos, &tR);

        if (!tR.bValid || tR.iWordCount <= 0)
            break;

        uiCovered += (size_t)tR.iWordCount * 2u;
        uiPos     += (size_t)tR.iWordCount * 2u;
        iInstrs++;

        /* Detect branches (mnemonic starts with 'B' and is not BTST etc.) */
        {
            const char *sz = tR.szMnemonic;
            if ((sz[0] == 'B' && sz[1] != 'T' && sz[1] != 'C' &&
                 sz[1] != 'S' && sz[1] != 'I') ||
                sz[0] == 'J')  /* JMP / JSR count as branches too */
            {
                iBranches++;
                /* Check if branch target is within sector */
                if (tR.iWordCount == 1 || tR.iWordCount == 2)
                {
                    /* Relative offset is in the low byte (short) or
                     * the second word (word-length) */
                    st_i32_t iOff = 0;
                    if (tR.iWordCount == 1)
                    {
                        /* short branch: bits 7:0 of opcode word */
                        iOff = (st_i32_t)(st_i8_t)(tR.auWords[0] & 0xFFu);
                    }
                    else
                    {
                        /* word branch */
                        iOff = (st_i32_t)(st_i16_t)tR.auWords[1];
                    }
                    st_i32_t iTarget = (st_i32_t)uiPos + iOff;
                    if (iTarget >= 0 && iTarget < (st_i32_t)SA_SECTOR_SIZE)
                        iRelOk++;
                }
            }
        }
    }

    *piInstrs         = iInstrs;
    *piBranches       = iBranches;
    *piRelBranchValid = iRelOk;
    return uiCovered;
}

/* Count even offsets whose first word decodes as a valid opcode */
static float feat_word_align_ratio(const st_u8_t *p, st_u32_t uiBase)
{
    disasm_result_t tR;
    int             iValid = 0;
    size_t          uiPos;

    for (uiPos = 0u; uiPos + 2u <= SA_SECTOR_SIZE; uiPos += 2u)
    {
        disasm_one(p + uiPos, SA_SECTOR_SIZE - uiPos,
                   uiBase + (st_u32_t)uiPos, &tR);
        if (tR.bValid)
            iValid++;
    }
    return (float)iValid / 256.0f;
}

/* Count 2-byte BE sequences 0xFF 0x8x..0xFF */
static float feat_hw_immediate(const st_u8_t *p)
{
    int iCount = 0, i;
    for (i = 0; i + 1 < (int)SA_SECTOR_SIZE; i++)
    {
        if (p[i] == 0xFFu && p[i + 1] >= 0x80u)
            iCount++;
    }
    return (iCount >= 20) ? 1.0f : (float)iCount / 20.0f;
}

/* HW immediate that falls inside a valid MOVE/LEA opcode operand */
static float feat_hw_in_context(const st_u8_t *p, st_u32_t uiBase)
{
    disasm_result_t tR;
    int             iHwCtx = 0;
    int             iInstrs = 0;
    size_t          uiPos  = 0u;

    while (uiPos + 2u <= SA_SECTOR_SIZE)
    {
        size_t uiILen;
        int    iW;

        disasm_one(p + uiPos, SA_SECTOR_SIZE - uiPos,
                   uiBase + (st_u32_t)uiPos, &tR);

        if (!tR.bValid || tR.iWordCount <= 0)
            break;

        iInstrs++;
        uiILen = (size_t)tR.iWordCount * 2u;

        /* Scan extension words for HW address immediates */
        for (iW = 1; iW < tR.iWordCount && iW < 5; iW++)
        {
            st_u32_t uiExt = (st_u32_t)tR.auWords[iW];
            /* 16-bit extension containing 0xFF8x (high-byte = 0xFF,
             * low-byte >= 0x80) indicates a short HW address */
            if ((uiExt >> 8u) == 0xFFu && (uiExt & 0xFFu) >= 0x80u)
                iHwCtx++;
        }
        uiPos += uiILen;
    }

    if (iInstrs == 0) return 0.0f;
    return (iHwCtx >= 4) ? 1.0f : (float)iHwCtx / 4.0f;
}

static float feat_timing_loops(const st_u8_t *p)
{
    int iCount = 0;
    size_t i;
    for (i = 0u; i + 1u < SA_SECTOR_SIZE; i += 2u)
    {
        /* DBcc Dn: opcode = 0x51C8..0x51CF (DBF D0..D7) */
        if (p[i] == 0x51u &&
            p[i + 1] >= 0xC8u && p[i + 1] <= 0xCFu)
            iCount++;
    }
    return (iCount >= 4) ? 1.0f : (float)iCount / 4.0f;
}

static float feat_vbl_install(const st_u8_t *p)
{
    int i;
    /* MOVE.L #imm,$70.W = 21 FC ?? ?? ?? ?? 00 70 */
    for (i = 0; i + 7 < (int)SA_SECTOR_SIZE; i++)
    {
        if (p[i]     == 0x21u && p[i + 1] == 0xFCu &&
            p[i + 6] == 0x00u && p[i + 7] == 0x70u)
            return 1.0f;
    }
    return 0.0f;
}

static float feat_sinus_profile(const st_u8_t *p)
{
    int    iSmall = 0;
    int    iPrev  = (int)p[0];
    size_t i;

    for (i = 1u; i < SA_SECTOR_SIZE; i++)
    {
        int iDiff = (int)p[i] - iPrev;
        if (iDiff < 0) iDiff = -iDiff;
        if (iDiff <= 10) iSmall++;
        iPrev = (int)p[i];
    }
    return (float)iSmall / (float)(SA_SECTOR_SIZE - 1u);
}

static float feat_graphics_pattern(const st_u8_t *p)
{
    int    iCount = 0;
    size_t i;

    for (i = 0u; i + 1u < SA_SECTOR_SIZE; i += 2u)
    {
        st_u16_t uiW = ((st_u16_t)p[i] << 8u) | (st_u16_t)p[i + 1];
        /* ST palette: bits F888 must all be zero (R/G/B each 0-7) */
        if ((uiW & 0xF888u) == 0u)
            iCount++;
    }
    return (float)iCount / 256.0f;
}

static float feat_hw_byte_pair(const st_u8_t *p,
                                st_u8_t uiHi1, st_u8_t uiHi2)
{
    int iCount = 0, i;
    for (i = 0; i + 1 < (int)SA_SECTOR_SIZE; i++)
    {
        if (p[i] == 0xFFu &&
            (p[i + 1] == uiHi1 || p[i + 1] == uiHi2))
            iCount++;
    }
    return (iCount >= 4) ? 1.0f : (float)iCount / 4.0f;
}

/* ------------------------------------------------------------------
 * sector_features_extract()
 * ------------------------------------------------------------------ */

st_error_t sector_features_extract(const st_u8_t    *pSector,
                                    st_u32_t          uiBase,
                                    sector_features_t *ptFeat)
{
    size_t uiCovered;
    int    iInstrs   = 0;
    int    iBranches = 0;
    int    iRelBr    = 0;

    if (pSector == NULL || ptFeat == NULL)
    {
        LOG_ERROR("NULL parameter: pSector=%p ptFeat=%p",
                  (void *)pSector, (void *)ptFeat);
        return ST_ERROR;
    }

    memset(ptFeat, 0, sizeof(*ptFeat));

    /* Structural */
    ptFeat->fBpbValid       = feat_bpb_valid(pSector);
    ptFeat->fWd1772Bootable = feat_wd1772_bootable(pSector);
    ptFeat->fFatPattern     = feat_fat_pattern(pSector);
    ptFeat->fDirDensity     = feat_dir_density(pSector);

    /* Byte distribution */
    ptFeat->fEntropy    = feat_entropy(pSector);
    ptFeat->fZeroRuns   = feat_byte_ratio(pSector, 0x00u);
    ptFeat->fE5Runs     = feat_byte_ratio(pSector, 0xE5u);
    ptFeat->fFfRuns     = feat_byte_ratio(pSector, 0xFFu);
    ptFeat->fAsciiRatio = feat_ascii_ratio(pSector);
    ptFeat->fRepeating  = feat_repeating(pSector);

    /* Code analysis */
    uiCovered = opcode_fwd_scan(pSector, uiBase,
                                 &iInstrs, &iBranches, &iRelBr);

    ptFeat->fOpcodeDensity  = (float)uiCovered / (float)SA_SECTOR_SIZE;
    ptFeat->fWordAlignRatio = feat_word_align_ratio(pSector, uiBase);

    ptFeat->fBranchDensity = (iInstrs > 0)
        ? (float)iBranches / (float)iInstrs : 0.0f;
    ptFeat->fRelBranchValid = (iBranches > 0)
        ? (float)iRelBr / (float)iBranches : 0.0f;

    ptFeat->fHwImmediate = feat_hw_immediate(pSector);
    ptFeat->fHwInContext = feat_hw_in_context(pSector, uiBase);
    ptFeat->fTimingLoops = feat_timing_loops(pSector);
    ptFeat->fVblInstall  = feat_vbl_install(pSector);

    /* Demo-specific */
    ptFeat->fSinusProfile    = feat_sinus_profile(pSector);
    ptFeat->fGraphicsPattern = feat_graphics_pattern(pSector);
    ptFeat->fYmAccess        = feat_hw_byte_pair(pSector, 0x88u, 0x89u);
    ptFeat->fVideoAccess     = feat_hw_byte_pair(pSector, 0x82u, 0x83u);
    ptFeat->fFdcAccess       = feat_hw_byte_pair(pSector, 0xFAu, 0xFCu);

    /* Derived: packer = high entropy AND low opcode density */
    ptFeat->fPackerEntropy =
        (ptFeat->fEntropy > 0.87f && ptFeat->fOpcodeDensity < 0.10f)
        ? ptFeat->fEntropy : 0.0f;

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Cosine similarity (weighted)
 * ------------------------------------------------------------------ */

static float cosine_similarity(const float *aA, const float *aB)
{
    float fDot = 0.0f;
    float fNa  = 0.0f;
    float fNb  = 0.0f;
    int   i;

    for (i = 0; i < SA_FEATURE_DIM; i++)
    {
        float fWa = aA[i] * g_weights[i];
        float fWb = aB[i] * g_weights[i];
        fDot += fWa * fWb;
        fNa  += fWa * fWa;
        fNb  += fWb * fWb;
    }
    if (fNa < 1e-9f || fNb < 1e-9f) return 0.0f;
    return fDot / (sqrtf(fNa) * sqrtf(fNb));
}

/* Feature vector as plain float array (same memory layout) */
static const float *feat_as_array(const sector_features_t *ptF)
{
    return (const float *)ptF;  /* struct is a flat array of floats */
}

/* ------------------------------------------------------------------
 * Database management
 * ------------------------------------------------------------------ */

st_error_t sector_db_create(sector_db_t **pptDb)
{
    sector_db_t *ptDb;

    if (pptDb == NULL)
    {
        LOG_ERROR("NULL parameter: pptDb=%p", (void *)pptDb);
        return ST_ERROR;
    }

    ptDb = (sector_db_t *)malloc(sizeof(sector_db_t));
    if (ptDb == NULL)
    {
        LOG_ERROR("malloc failed for sector_db_t");
        return ST_ERROR;
    }

    memset(ptDb, 0, sizeof(*ptDb));
    *pptDb = ptDb;
    LOG_TRACE("ptDb=%p", (void *)ptDb);
    return ST_NO_ERROR;
}

st_error_t sector_db_destroy(sector_db_t **pptDb)
{
    if (pptDb == NULL)
    {
        LOG_ERROR("NULL parameter: pptDb=%p", (void *)pptDb);
        return ST_ERROR;
    }
    if (*pptDb != NULL)
    {
        free(*pptDb);
        *pptDb = NULL;
    }
    return ST_NO_ERROR;
}

/* Find or create a slot for eType in the database */
static sector_centroid_t *db_slot(sector_db_t  *ptDb,
                                   sector_type_t eType,
                                   const char   *szLabel)
{
    int i;

    /* Search existing */
    for (i = 0; i < ptDb->iTypeCount; i++)
        if (ptDb->atTypes[i].eType == eType)
            return &ptDb->atTypes[i];

    /* Allocate new slot */
    if (ptDb->iTypeCount >= SA_DB_MAX_TYPES)
        return NULL;

    {
        sector_centroid_t *pt = &ptDb->atTypes[ptDb->iTypeCount++];
        memset(pt, 0, sizeof(*pt));
        pt->eType = eType;
        if (szLabel != NULL)
            strncpy(pt->szLabel, szLabel, sizeof(pt->szLabel) - 1u);
        else
            strncpy(pt->szLabel,
                    sector_type_name(eType), sizeof(pt->szLabel) - 1u);
        return pt;
    }
}

st_error_t sector_db_learn(sector_db_t       *ptDb,
                            const st_u8_t     *pSector,
                            st_u32_t           uiBase,
                            sector_type_t      eType,
                            const char        *szLabel)
{
    sector_features_t tFeat;
    sector_centroid_t *pt;
    const float       *aF;
    int                i;

    if (ptDb == NULL || pSector == NULL)
    {
        LOG_ERROR("NULL parameter: ptDb=%p pSector=%p",
                  (void *)ptDb, (void *)pSector);
        return ST_ERROR;
    }
    if ((int)eType <= 0 || eType >= SECTOR_TYPE_COUNT)
    {
        LOG_ERROR("invalid eType=%d", (int)eType);
        return ST_ERROR;
    }

    if (sector_features_extract(pSector, uiBase, &tFeat) != ST_NO_ERROR)
        return ST_ERROR;

    pt = db_slot(ptDb, eType, szLabel);
    if (pt == NULL)
    {
        LOG_ERROR("DB full (iTypeCount=%d)", ptDb->iTypeCount);
        return ST_ERROR;
    }

    aF = feat_as_array(&tFeat);
    for (i = 0; i < SA_FEATURE_DIM; i++)
        pt->afAccum[i] += aF[i];
    pt->iSamples++;

    return ST_NO_ERROR;
}

st_error_t sector_db_finalize(sector_db_t *ptDb)
{
    int i, j;

    if (ptDb == NULL)
    {
        LOG_ERROR("NULL parameter: ptDb=%p", (void *)ptDb);
        return ST_ERROR;
    }

    for (i = 0; i < ptDb->iTypeCount; i++)
    {
        sector_centroid_t *pt = &ptDb->atTypes[i];
        if (pt->iSamples == 0) continue;
        for (j = 0; j < SA_FEATURE_DIM; j++)
            pt->afFeatures[j] = pt->afAccum[j] / (float)pt->iSamples;
    }
    return ST_NO_ERROR;
}

st_error_t sector_db_bootstrap_defaults(sector_db_t *ptDb)
{
    st_u8_t aSec[SA_SECTOR_SIZE];

    if (ptDb == NULL)
    {
        LOG_ERROR("NULL parameter: ptDb=%p", (void *)ptDb);
        return ST_ERROR;
    }

    /* BSS / zero-filled */
    memset(aSec, 0x00, sizeof(aSec));
    sector_db_learn(ptDb, aSec, 0u, SECTOR_BSS_ZERO, "bss_zero");

    /* Unformatted (erased sector) */
    memset(aSec, 0xFF, sizeof(aSec));
    sector_db_learn(ptDb, aSec, 0u, SECTOR_UNFORMATTED, "unformatted");

    /* FAT12 header pattern (media byte + EOC chain) */
    memset(aSec, 0x00, sizeof(aSec));
    aSec[0] = 0xF9u; aSec[1] = 0xFFu; aSec[2] = 0xFFu;
    aSec[3] = 0xFFu; aSec[4] = 0xFFu;
    sector_db_learn(ptDb, aSec, 0u, SECTOR_FAT12, "fat12");

    /* Directory: 16 end-of-directory entries (all zeros) */
    memset(aSec, 0x00, sizeof(aSec));
    sector_db_learn(ptDb, aSec, 0u, SECTOR_DIRECTORY, "directory");

    /* Deleted directory: all 0xE5 */
    memset(aSec, 0xE5u, sizeof(aSec));
    sector_db_learn(ptDb, aSec, 0u, SECTOR_DIRECTORY_DELETED,
                    "directory_deleted");

    /* Data / binary: mid-range entropy, no special pattern */
    {
        int i;
        for (i = 0; i < (int)SA_SECTOR_SIZE; i++)
            aSec[i] = (st_u8_t)(i & 0xFFu);
    }
    sector_db_learn(ptDb, aSec, 0u, SECTOR_DATA_BINARY, "data_binary");

    return sector_db_finalize(ptDb);
}

/* ------------------------------------------------------------------
 * Persistence
 * ------------------------------------------------------------------ */

/*
 * Binary layout of sector_db.bin:
 *   4  bytes : magic SA_DB_MAGIC
 *   4  bytes : iTypeCount  (int, little-endian)
 *   4  bytes : iSigCount   (int, little-endian)
 *   Per type (iTypeCount):
 *     4  bytes : eType
 *    32  bytes : szLabel
 *     24×4 bytes : afFeatures (float LE)
 *     4  bytes : iSamples
 *   Per sig (iSigCount):
 *    48  bytes : szName
 *     4  bytes : eType
 *     4  bytes : iOffset
 *    16  bytes : aPattern
 *    16  bytes : aMask
 *     4  bytes : iPatternLen
 */

st_error_t sector_db_save(const sector_db_t *ptDb, const char *szPath)
{
    FILE   *pF;
    int     i, j;
    st_u32_t uiMagic = SA_DB_MAGIC;

    if (ptDb == NULL || szPath == NULL)
    {
        LOG_ERROR("NULL parameter: ptDb=%p szPath=%p",
                  (void *)ptDb, (void *)szPath);
        return ST_ERROR;
    }

    pF = fopen(szPath, "wb");
    if (pF == NULL)
    {
        LOG_ERROR("fopen failed: %s", szPath);
        return ST_ERROR;
    }

    fwrite(&uiMagic,          sizeof(uiMagic),       1u, pF);
    fwrite(&ptDb->iTypeCount, sizeof(int),            1u, pF);
    fwrite(&ptDb->iSigCount,  sizeof(int),            1u, pF);

    for (i = 0; i < ptDb->iTypeCount; i++)
    {
        const sector_centroid_t *pt = &ptDb->atTypes[i];
        int eT = (int)pt->eType;
        fwrite(&eT,            sizeof(int),           1u, pF);
        fwrite(pt->szLabel,    sizeof(pt->szLabel),   1u, pF);
        for (j = 0; j < SA_FEATURE_DIM; j++)
            fwrite(&pt->afFeatures[j], sizeof(float), 1u, pF);
        fwrite(&pt->iSamples,  sizeof(int),           1u, pF);
    }

    for (i = 0; i < ptDb->iSigCount; i++)
    {
        const sector_packer_sig_t *ps = &ptDb->atSigs[i];
        int eT = (int)ps->eType;
        fwrite(ps->szName,      sizeof(ps->szName),   1u, pF);
        fwrite(&eT,             sizeof(int),          1u, pF);
        fwrite(&ps->iOffset,    sizeof(int),          1u, pF);
        fwrite(ps->aPattern,    sizeof(ps->aPattern), 1u, pF);
        fwrite(ps->aMask,       sizeof(ps->aMask),    1u, pF);
        fwrite(&ps->iPatternLen,sizeof(int),          1u, pF);
    }

    fclose(pF);
    LOG_INFO("saved %d types %d sigs to %s",
             ptDb->iTypeCount, ptDb->iSigCount, szPath);
    return ST_NO_ERROR;
}

st_error_t sector_db_load(sector_db_t *ptDb, const char *szPath)
{
    FILE    *pF;
    st_u32_t uiMagic = 0u;
    int      iTypes  = 0;
    int      iSigs   = 0;
    int      i, j;

    if (ptDb == NULL || szPath == NULL)
    {
        LOG_ERROR("NULL parameter: ptDb=%p szPath=%p",
                  (void *)ptDb, (void *)szPath);
        return ST_ERROR;
    }

    pF = fopen(szPath, "rb");
    if (pF == NULL)
        return ST_ERROR;   /* file absent — not an error for the caller */

    if (fread(&uiMagic, sizeof(uiMagic), 1u, pF) != 1u ||
        uiMagic != SA_DB_MAGIC)
    {
        LOG_ERROR("bad magic in %s: 0x%08X", szPath, uiMagic);
        fclose(pF);
        return ST_ERROR;
    }

    if (fread(&iTypes, sizeof(int), 1u, pF) != 1u ||
        fread(&iSigs,  sizeof(int), 1u, pF) != 1u ||
        iTypes < 0 || iTypes > SA_DB_MAX_TYPES ||
        iSigs  < 0 || iSigs  > SA_SIG_MAX_SIGS)
    {
        LOG_ERROR("corrupt header in %s", szPath);
        fclose(pF);
        return ST_ERROR;
    }

    memset(ptDb->atTypes, 0, sizeof(ptDb->atTypes));
    ptDb->iTypeCount = 0;

    for (i = 0; i < iTypes; i++)
    {
        sector_centroid_t *pt = &ptDb->atTypes[i];
        int eT = 0;
        fread(&eT,           sizeof(int),          1u, pF);
        fread(pt->szLabel,   sizeof(pt->szLabel),  1u, pF);
        for (j = 0; j < SA_FEATURE_DIM; j++)
            fread(&pt->afFeatures[j], sizeof(float), 1u, pF);
        fread(&pt->iSamples, sizeof(int),          1u, pF);
        pt->eType = (sector_type_t)eT;
    }
    ptDb->iTypeCount = iTypes;

    memset(ptDb->atSigs, 0, sizeof(ptDb->atSigs));
    ptDb->iSigCount = 0;

    for (i = 0; i < iSigs; i++)
    {
        sector_packer_sig_t *ps = &ptDb->atSigs[i];
        int eT = 0;
        fread(ps->szName,      sizeof(ps->szName),   1u, pF);
        fread(&eT,             sizeof(int),          1u, pF);
        fread(&ps->iOffset,    sizeof(int),          1u, pF);
        fread(ps->aPattern,    sizeof(ps->aPattern), 1u, pF);
        fread(ps->aMask,       sizeof(ps->aMask),    1u, pF);
        fread(&ps->iPatternLen,sizeof(int),          1u, pF);
        ps->eType = (sector_type_t)eT;
    }
    ptDb->iSigCount = iSigs;

    fclose(pF);
    LOG_INFO("loaded %d types %d sigs from %s", iTypes, iSigs, szPath);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Packer signature matching
 * ------------------------------------------------------------------ */

static const char *packer_scan(const sector_db_t *ptDb,
                                const st_u8_t     *pSector,
                                sector_type_t     *peType)
{
    int i, j;

    for (i = 0; i < ptDb->iSigCount; i++)
    {
        const sector_packer_sig_t *ps = &ptDb->atSigs[i];
        int iStart, iEnd;

        if (ps->iPatternLen <= 0 ||
            ps->iPatternLen > SA_SIG_MAX_LEN)
            continue;

        if (ps->iOffset >= 0)
        {
            iStart = ps->iOffset;
            iEnd   = ps->iOffset + 1;
        }
        else
        {
            iStart = 0;
            iEnd   = (int)SA_SECTOR_SIZE - ps->iPatternLen;
        }

        for (j = iStart; j < iEnd; j++)
        {
            int    k;
            int    bMatch = 1;

            if (j + ps->iPatternLen > (int)SA_SECTOR_SIZE)
                break;

            for (k = 0; k < ps->iPatternLen && bMatch; k++)
            {
                if ((pSector[j + k] & ps->aMask[k]) !=
                    (ps->aPattern[k] & ps->aMask[k]))
                    bMatch = 0;
            }

            if (bMatch)
            {
                *peType = ps->eType;
                return ps->szName;
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------
 * sector_classify()
 * ------------------------------------------------------------------ */

st_error_t sector_classify(const sector_db_t       *ptDb,
                             const sector_features_t *ptFeat,
                             const st_u8_t           *pSector,
                             sector_match_t          *aMatches,
                             int                      iMaxMatches,
                             int                     *piCount)
{
    const float   *aF;
    sector_type_t  eSigType = SECTOR_UNKNOWN;
    const char    *szSigName;
    int            i, j;

    if (ptDb == NULL || ptFeat == NULL || pSector == NULL ||
        aMatches == NULL || piCount == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }
    if (iMaxMatches < 1)
    {
        LOG_ERROR("iMaxMatches=%d must be >= 1", iMaxMatches);
        return ST_ERROR;
    }

    *piCount = 0;

    /* Check packer signatures first — they override cosine */
    szSigName = packer_scan(ptDb, pSector, &eSigType);
    if (szSigName != NULL)
    {
        aMatches[0].eType  = eSigType;
        aMatches[0].fScore = 1.0f;
        strncpy(aMatches[0].szLabel, szSigName,
                sizeof(aMatches[0].szLabel) - 1u);
        *piCount = 1;
        return ST_NO_ERROR;
    }

    /* Cosine similarity against all centroids */
    aF = feat_as_array(ptFeat);

    /* Insertion-sort into aMatches (small iMaxMatches, no need for qsort) */
    for (i = 0; i < ptDb->iTypeCount; i++)
    {
        const sector_centroid_t *pt = &ptDb->atTypes[i];
        sector_match_t tM;
        int iPos;

        tM.eType  = pt->eType;
        tM.fScore = cosine_similarity(aF, pt->afFeatures);
        strncpy(tM.szLabel, pt->szLabel, sizeof(tM.szLabel) - 1u);

        /* Find insertion position */
        iPos = *piCount;
        while (iPos > 0 && aMatches[iPos - 1].fScore < tM.fScore)
            iPos--;

        if (iPos >= iMaxMatches)
            continue;

        /* Shift down */
        for (j = ST_MIN(*piCount, iMaxMatches - 1); j > iPos; j--)
            aMatches[j] = aMatches[j - 1];

        aMatches[iPos] = tM;
        if (*piCount < iMaxMatches)
            (*piCount)++;
    }

    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * sector_type_name()
 * ------------------------------------------------------------------ */

const char *sector_type_name(sector_type_t eType)
{
    static const char *aszNames[SECTOR_TYPE_COUNT] =
    {
        "unknown",
        "bootsector_noboot",
        "bootsector_boot",
        "fat12",
        "directory",
        "directory_deleted",
        "code_demo",
        "code_gemdos",
        "code_packer",
        "data_sinus",
        "data_ascii",
        "data_binary",
        "data_packed",
        "bss_zero",
        "unformatted",
        "protection"
    };

    if (eType < 0 || (int)eType >= SECTOR_TYPE_COUNT)
        return "unknown";
    return aszNames[(int)eType];
}
