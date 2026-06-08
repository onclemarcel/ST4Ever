/*
 * sector_analyze.h - ST4Ever sector fingerprint engine
 *
 * Extracts a 24-dimensional feature vector from a 512-byte Atari ST
 * disk sector and classifies it by weighted cosine similarity against
 * a reference database of known sector types.
 *
 * Feature extraction is purely static (UC24A): byte distribution,
 * BPB/FAT structure, and 68000 opcode forward-scan using the
 * disassemble module (UC11-14).
 *
 * Dynamic enrichment (execution coverage overlay) is planned UC25+.
 *
 * Reference database:
 *   sector_db_bootstrap_defaults() - minimal hard-bootstrapped DB
 *   sector_db_learn()              - accumulate labeled sample
 *   sector_db_finalize()           - compute centroids from samples
 *   sector_db_load() / save()      - persist to db/sector_db.bin
 *
 * Typical workflow (analysis tool):
 *   sector_db_create(&ptDb);
 *   sector_db_load(ptDb, "db/sector_db.bin");    // or bootstrap
 *   sector_features_extract(aSector, &tFeat);
 *   sector_classify(ptDb, &tFeat, aMatches, 3, &iN);
 *   // aMatches[0] = best match
 *   sector_db_destroy(&ptDb);
 */

#ifndef SECTOR_ANALYZE_H
#define SECTOR_ANALYZE_H

#include "common.h"

/* ------------------------------------------------------------------
 * Geometry
 * ------------------------------------------------------------------ */

#define SA_SECTOR_SIZE   512u   /* bytes per Atari ST sector            */
#define SA_FEATURE_DIM    24    /* dimension of the feature vector      */
#define SA_DB_MAX_TYPES   20    /* max sector types in one DB           */
#define SA_DB_MAGIC  0x53544442u  /* 'STDB' - binary file magic        */

/* ------------------------------------------------------------------
 * Sector type taxonomy
 * ------------------------------------------------------------------ */

typedef enum sector_type_e
{
    SECTOR_UNKNOWN            =  0,
    SECTOR_BOOTSECTOR_NOBOOT  =  1, /* BPB valid, WD1772 cksum wrong   */
    SECTOR_BOOTSECTOR_BOOT    =  2, /* BPB valid, WD1772 cksum == 1234 */
    SECTOR_FAT12              =  3, /* FAT12 chain table                */
    SECTOR_DIRECTORY          =  4, /* FAT12 directory entries          */
    SECTOR_DIRECTORY_DELETED  =  5, /* mostly 0xE5 deleted entries      */
    SECTOR_CODE_DEMO          =  6, /* 68k code with HW register access */
    SECTOR_CODE_GEMDOS        =  7, /* 68k code, TRAP #1/#14, no HW    */
    SECTOR_CODE_PACKER        =  8, /* self-decompressor / boot code    */
    SECTOR_DATA_SINUS         =  9, /* sinusoid lookup table            */
    SECTOR_DATA_ASCII         = 10, /* printable text / strings         */
    SECTOR_DATA_BINARY        = 11, /* binary data, unclassified        */
    SECTOR_DATA_PACKED        = 12, /* high entropy (RLE/LZ compressed) */
    SECTOR_BSS_ZERO           = 13, /* zero-filled (BSS / padding)      */
    SECTOR_UNFORMATTED        = 14, /* 0xFF fill (erased / virgin)      */
    SECTOR_PROTECTION         = 15, /* known copy-protection signature  */
    SECTOR_TYPE_COUNT         = 16
} sector_type_t;

/* ------------------------------------------------------------------
 * Feature vector — 24 dimensions, all in [0.0, 1.0]
 * ------------------------------------------------------------------ */

typedef struct sector_features_s
{
    /* Structural — 4 */
    float fBpbValid;        /* BPB geometry consistent (BPS/SPC/NFAT)  */
    float fWd1772Bootable;  /* 1.0 iff sum of 256 BE words == 0x1234   */
    float fFatPattern;      /* FAT media byte followed by 0xFF chain   */
    float fDirDensity;      /* fraction of 32-B slots with dir struct  */

    /* Byte distribution — 6 */
    float fEntropy;         /* Shannon H/8.0  normalized [0..1]        */
    float fZeroRuns;        /* ratio of 0x00 bytes                     */
    float fE5Runs;          /* ratio of 0xE5 (FAT deleted marker)      */
    float fFfRuns;          /* ratio of 0xFF (unformatted)             */
    float fAsciiRatio;      /* ratio of printable ASCII [0x20..0x7E]   */
    float fRepeating;       /* dominant byte frequency / 512           */

    /* Code analysis — 8 */
    float fOpcodeDensity;   /* bytes covered by fwd-decode from off 0  */
    float fWordAlignRatio;  /* even offsets with valid opcode / 256    */
    float fBranchDensity;   /* BRA/BSR/Bcc fraction in decoded instrs  */
    float fRelBranchValid;  /* branches whose target is in-sector      */
    float fHwImmediate;     /* byte pairs 0xFF 0x8x..0xFx in sector    */
    float fHwInContext;     /* HW immediates in valid MOVE/LEA opcodes  */
    float fTimingLoops;     /* DBcc (0x51C8-CF) pattern density        */
    float fVblInstall;      /* MOVE.L #imm,$70.W  pattern present      */

    /* Demo-specific — 6 */
    float fSinusProfile;    /* first-difference near-zero ratio        */
    float fGraphicsPattern; /* word pairs with bits (F888) == 0        */
    float fPackerEntropy;   /* entropy>0.87 AND opcode<0.10            */
    float fYmAccess;        /* 0xFF88xx byte patterns                  */
    float fVideoAccess;     /* 0xFF82xx byte patterns                  */
    float fFdcAccess;       /* 0xFFFA/0xFFFC byte patterns             */
} sector_features_t;

/* ------------------------------------------------------------------
 * Classification result
 * ------------------------------------------------------------------ */

typedef struct sector_match_s
{
    sector_type_t eType;
    char          szLabel[32];
    float         fScore;       /* cosine similarity [0.0 .. 1.0]      */
} sector_match_t;

/* ------------------------------------------------------------------
 * Packer signature (byte-pattern override)
 * ------------------------------------------------------------------ */

#define SA_SIG_MAX_LEN   16
#define SA_SIG_MAX_SIGS  32

typedef struct sector_packer_sig_s
{
    char     szName[48];
    sector_type_t eType;
    int      iOffset;           /* byte offset in sector (-1 = search) */
    st_u8_t  aPattern[SA_SIG_MAX_LEN];
    st_u8_t  aMask[SA_SIG_MAX_LEN];  /* 0xFF = must match, 0x00 = any  */
    int      iPatternLen;
} sector_packer_sig_t;

/* ------------------------------------------------------------------
 * Database
 * ------------------------------------------------------------------ */

typedef struct sector_centroid_s
{
    sector_type_t eType;
    char          szLabel[32];
    float         afFeatures[SA_FEATURE_DIM];
    float         afAccum[SA_FEATURE_DIM];  /* accumulator for learn() */
    int           iSamples;
} sector_centroid_t;

typedef struct sector_db_s
{
    sector_centroid_t   atTypes[SA_DB_MAX_TYPES];
    int                 iTypeCount;
    sector_packer_sig_t atSigs[SA_SIG_MAX_SIGS];
    int                 iSigCount;
} sector_db_t;

/* ------------------------------------------------------------------
 * Public API — feature extraction
 * ------------------------------------------------------------------ */

/*
 * sector_features_extract() - Compute the 24-D feature vector.
 *
 * Parameters:
 *   pSector  [in]  : 512-byte sector buffer.
 *   uiBase   [in]  : LBA address of this sector (for branch targets).
 *   ptFeat   [out] : Receives computed features.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pSector or ptFeat is NULL.
 */
st_error_t sector_features_extract(const st_u8_t    *pSector,
                                    st_u32_t          uiBase,
                                    sector_features_t *ptFeat);

/* ------------------------------------------------------------------
 * Public API — database management
 * ------------------------------------------------------------------ */

/*
 * sector_db_create() - Allocate an empty sector database.
 *
 * Parameters:
 *   pptDb [out] : Receives the allocated DB handle.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if pptDb is NULL or malloc fails.
 */
st_error_t sector_db_create(sector_db_t **pptDb);

/*
 * sector_db_destroy() - Free a sector database.
 *
 * Sets *pptDb to NULL.
 *
 * Parameters:
 *   pptDb [in/out] : DB to destroy.
 *
 * Returns:
 *   ST_NO_ERROR always (except NULL pptDb -> ST_ERROR).
 */
st_error_t sector_db_destroy(sector_db_t **pptDb);

/*
 * sector_db_bootstrap_defaults() - Seed DB from synthetic sectors.
 *
 * Populates centroids for trivially-computable types (BSS_ZERO,
 * UNFORMATTED, FAT12) by computing actual feature vectors from
 * synthetic buffers.  Call before sector_db_load() if the .bin
 * file may be absent.
 *
 * Parameters:
 *   ptDb [in] : DB to populate (must be freshly created).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptDb is NULL.
 */
st_error_t sector_db_bootstrap_defaults(sector_db_t *ptDb);

/*
 * sector_db_learn() - Accumulate one labeled sector sample.
 *
 * Extracts features from pSector and adds them to the accumulator
 * for eType.  Call sector_db_finalize() when done.
 *
 * Parameters:
 *   ptDb    [in] : Target database.
 *   pSector [in] : 512-byte sector buffer.
 *   uiBase  [in] : LBA address of the sector.
 *   eType   [in] : Ground-truth label.
 *   szLabel [in] : Human-readable label (e.g. "bss_zero").
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL or eType out of range.
 */
st_error_t sector_db_learn(sector_db_t       *ptDb,
                            const st_u8_t     *pSector,
                            st_u32_t           uiBase,
                            sector_type_t      eType,
                            const char        *szLabel);

/*
 * sector_db_finalize() - Compute centroids from accumulated samples.
 *
 * Divides each accumulator by its sample count.  Must be called after
 * all sector_db_learn() calls and before sector_classify().
 *
 * Parameters:
 *   ptDb [in] : Database to finalize.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptDb is NULL.
 */
st_error_t sector_db_finalize(sector_db_t *ptDb);

/*
 * sector_db_load() - Load a database from a binary .bin file.
 *
 * Replaces any previously finalized centroids.  The file must start
 * with magic SA_DB_MAGIC.  Silently returns ST_ERROR (no crash) if
 * the file is absent — the caller keeps the bootstrapped defaults.
 *
 * Parameters:
 *   ptDb   [in] : Target database.
 *   szPath [in] : Path to the .bin file.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptDb or szPath is NULL, or the file is invalid.
 */
st_error_t sector_db_load(sector_db_t *ptDb, const char *szPath);

/*
 * sector_db_save() - Persist the database to a binary .bin file.
 *
 * Parameters:
 *   ptDb   [in] : Source database.
 *   szPath [in] : Destination path.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL or the write fails.
 */
st_error_t sector_db_save(const sector_db_t *ptDb, const char *szPath);

/* ------------------------------------------------------------------
 * Public API — classification
 * ------------------------------------------------------------------ */

/*
 * sector_classify() - Classify a feature vector against the database.
 *
 * Returns up to iMaxMatches results sorted by descending score.
 * Packer signatures are checked first and override cosine scores
 * when matched.
 *
 * Parameters:
 *   ptDb       [in]  : Finalized database.
 *   ptFeat     [in]  : Feature vector (from sector_features_extract).
 *   pSector    [in]  : Raw sector bytes (for packer sig matching).
 *   aMatches   [out] : Result array, capacity iMaxMatches.
 *   iMaxMatches[in]  : Capacity of aMatches (>= 1).
 *   piCount    [out] : Number of matches written.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any pointer is NULL or iMaxMatches < 1.
 */
st_error_t sector_classify(const sector_db_t       *ptDb,
                             const sector_features_t *ptFeat,
                             const st_u8_t           *pSector,
                             sector_match_t          *aMatches,
                             int                      iMaxMatches,
                             int                     *piCount);

/*
 * sector_type_name() - Return a static string for a sector type enum.
 *
 * Parameters:
 *   eType [in] : Sector type.
 *
 * Returns:
 *   A static null-terminated string (never NULL).
 */
const char *sector_type_name(sector_type_t eType);

#endif /* SECTOR_ANALYZE_H */
