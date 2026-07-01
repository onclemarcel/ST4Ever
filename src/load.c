/*
 * load.c - ST4Ever file load into emulated ST RAM
 *
 * UC15: Full PRG header parse + fixup relocation table + BSS clear.
 *
 * PRG header layout (28 bytes, all fields big-endian):
 *   Offset  0 : magic    0x601A  (2 bytes)
 *   Offset  2 : text_sz          (4 bytes) — .text section size
 *   Offset  6 : data_sz          (4 bytes) — .data section size
 *   Offset 10 : bss_sz           (4 bytes) — .bss  section size
 *   Offset 14 : sym_sz           (4 bytes) — symbol table size
 *   Offset 18 : reserved         (4 bytes)
 *   Offset 22 : flags            (4 bytes)
 *   Offset 26 : abs_flag         (2 bytes) — 0=relocatable, 1=absolute
 *
 * File layout after header:
 *   .text (text_sz bytes) + .data (data_sz bytes)
 *   symbol table (sym_sz bytes)
 *   fixup table (if abs_flag == 0):
 *     4 bytes  first fixup offset from .text start (0 = no fixups)
 *     1 byte   0x00 = end ; 0x01 = advance 254 ; other = advance N + fix
 *
 * Load address is fixed at ST_LOAD_ADDR (0x800); UC25 will allocate
 * dynamically above TOS variables when the execution engine is active.
 */

#include "load.h"
#include "file.h"
#include "trace.h"

#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Load Context global structure
 * ------------------------------------------------------------------ */

static load_context_t  g_load_ptCtx = {.ulMagic = 0xCAFEDECA, 
                                       .eObject = ST_LOAD_CTX};

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/*
 * load_is_prg_ext() - ST_TRUE if the lowercase extension is a PRG type.
 */
static st_bool_t load_is_prg_ext(const char *szExt)
{
    if (szExt == NULL || szExt[0] == '\0') return ST_FALSE;
    return (strcmp(szExt, "prg") == 0
         || strcmp(szExt, "ttp") == 0
         || strcmp(szExt, "tos") == 0)
           ? ST_TRUE : ST_FALSE;
}

/*
 * load_is_image_ext() - ST_TRUE if the lowercase extension is a disk image.
 */
static st_bool_t load_is_image_ext(const char *szExt)
{
    if (szExt == NULL || szExt[0] == '\0') return ST_FALSE;
    return (strcmp(szExt, "st")  == 0
         || strcmp(szExt, "msa") == 0
         || strcmp(szExt, "stx") == 0)
           ? ST_TRUE : ST_FALSE;
}

/*
 * load_u16be() - Read big-endian 16-bit word from a byte buffer.
 */
static st_u16_t load_u16be(const st_u8_t *p)
{
    return (st_u16_t)(((st_u16_t)p[0] << 8) | p[1]);
}

/*
 * load_u32be() - Read big-endian 32-bit word from a byte buffer.
 */
static st_u32_t load_u32be(const st_u8_t *p)
{
    return ((st_u32_t)p[0] << 24)
         | ((st_u32_t)p[1] << 16)
         | ((st_u32_t)p[2] <<  8)
         |  (st_u32_t)p[3];
}

/*
 * load_skip_bytes() - Read and discard uiCount bytes from a file.
 *
 * Used to skip the PRG symbol table before the fixup table.
 */
static st_error_t load_skip_bytes(file_t *ptFile, st_u32_t uiCount)
{
    st_u8_t  aBuf[512];
    st_u32_t uiRemain;
    size_t   uiChunk;
    size_t   uiRead;

    uiRemain = uiCount;
    while (uiRemain > 0)
    {
        uiChunk = (uiRemain > (st_u32_t)sizeof(aBuf))
                  ? sizeof(aBuf) : (size_t)uiRemain;
        uiRead = 0;
        if (file_read(ptFile, aBuf, uiChunk, &uiRead) != ST_NO_ERROR
            || uiRead < uiChunk)
            return ST_ERROR;
        uiRemain -= (st_u32_t)uiRead;
    }
    return ST_NO_ERROR;
}

/*
 * load_apply_fixups() - Walk the fixup table and relocate longwords in RAM.
 *
 * At each fixup position P (relative to start of .text), the longword
 * stored in RAM at ST_LOAD_BASE + P is incremented by ST_LOAD_BASE.
 *
 * Parameters:
 *   ptFile     [in]  : file open at the start of the fixup table
 *   szPath     [in]  : path (for error messages)
 *   uiTotal    [in]  : relocatable area size (text_sz + data_sz)
 *   puiCount   [out] : receives the number of fixups applied
 *
 * Returns:
 *   ST_NO_ERROR on success (including "no fixups").
 *   ST_ERROR    on I/O error or out-of-range fixup offset.
 */
static st_error_t load_apply_fixups(file_t     *ptFile,
                                     const char *szPath,
                                     st_u32_t    uiTotal,
                                     st_u32_t   *puiCount)
{
    st_u8_t  aBuf4[4];
    st_u8_t  bByte;
    size_t   uiRead;
    st_u32_t uiOffset;
    st_u32_t uiOld;
    st_u32_t uiNew;
    st_u8_t *pRam;

    *puiCount = 0;

    /* Read the first fixup offset (4 bytes big-endian) */
    uiRead = 0;
    if (file_read(ptFile, aBuf4, 4, &uiRead) != ST_NO_ERROR
        || uiRead < 4)
    {
        LOG_ERROR("'%s': cannot read fixup table header", szPath);
        return ST_ERROR;
    }

    uiOffset = load_u32be(aBuf4);
    if (uiOffset == 0)
    {
        /* Conventional signal: no fixups in this PRG */
        LOG_INFO("PRG '%s': no fixups", szPath);
        return ST_NO_ERROR;
    }

    /* Walk the fixup list */
    for (;;)
    {
        /* Validate: fixup must be a longword entirely within .text+.data */
        if (uiOffset + 4 > uiTotal)
        {
            LOG_ERROR("'%s': fixup offset 0x%06X out of range (max 0x%06X)",
                      szPath,
                      (unsigned)uiOffset,
                      (unsigned)(uiTotal >= 4 ? uiTotal - 4 : 0));
            return ST_ERROR;
        }

        /* Relocate: longword at ST_LOAD_BASE + uiOffset += ST_LOAD_BASE */
        pRam  = st_get_ram_pointer(ST_LOAD_BASE + uiOffset);
        uiOld = load_u32be(pRam);
        uiNew = uiOld + ST_LOAD_BASE;
        pRam[0] = (st_u8_t)(uiNew >> 24);
        pRam[1] = (st_u8_t)(uiNew >> 16);
        pRam[2] = (st_u8_t)(uiNew >>  8);
        pRam[3] = (st_u8_t)(uiNew);
        (*puiCount)++;

        /* Read next byte */
        uiRead = 0;
        if (file_read(ptFile, &bByte, 1, &uiRead) != ST_NO_ERROR
            || uiRead < 1)
        {
            /* EOF without explicit 0x00 terminator — treat as end */
            break;
        }

        if (bByte == 0x00)
            break;             /* Explicit end of fixup list */

        if (bByte == 0x01)
            uiOffset += 254;   /* Long-distance skip */
        else
            uiOffset += (st_u32_t)bByte; /* Normal advance */
    }

    return ST_NO_ERROR;
}

/*
 * load_do_raw() - Copy raw file content into ST RAM at ST_LOAD_BASE.
 */
static st_error_t load_do_raw(const char        *szPath,
                               const file_stat_t *ptStat,
                               load_type_t        eType)
{
    file_t  *ptFile;
    st_u8_t  aBuf[4096];
    size_t   uiRead;
    size_t   uiTotal;

    ptFile = NULL;
    uiTotal = 0;

    if (ptStat->uiSize > (st_u64_t)ST_LOAD_MAX_SIZE)
    {
        LOG_ERROR("'%s': file too large (%lu bytes, max %lu)",
                  szPath,
                  (unsigned long)ptStat->uiSize,
                  (unsigned long)ST_LOAD_MAX_SIZE);
        return ST_ERROR;
    }

    if (file_open(szPath, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
        return ST_ERROR;

    do
    {
        uiRead = 0;
        if (file_read(ptFile, aBuf, sizeof(aBuf), &uiRead)
                != ST_NO_ERROR)
        {
            file_close(&ptFile);
            LOG_ERROR("'%s': read error", szPath);
            return ST_ERROR;
        }
        if (uiRead == 0) break;
        if (uiTotal + uiRead > ST_LOAD_MAX_SIZE)
        {
            file_close(&ptFile);
            LOG_ERROR("'%s': data exceeds RAM during streaming", szPath);
            return ST_ERROR;
        }
        memcpy(st_get_ram_pointer(ST_LOAD_BASE + uiTotal),
               aBuf, uiRead);
        uiTotal += uiRead;
    }
    while (uiRead == sizeof(aBuf));

    file_close(&ptFile);

    g_load_ptCtx.bLoaded      = ST_TRUE;
    g_load_ptCtx.eType        = eType;
    strncpy(g_load_ptCtx.szPath, szPath, ST_MAX_PATH - 1);
    g_load_ptCtx.uiLoadAddr   = ST_LOAD_BASE;
    g_load_ptCtx.uiSize       = (st_u32_t)uiTotal;
    g_load_ptCtx.uiTextSize   = 0;
    g_load_ptCtx.uiDataSize   = 0;
    g_load_ptCtx.uiBssSize    = 0;
    g_load_ptCtx.uiFixupCount = 0;

    LOG_INFO("loaded '%s' at ST:0x%06X (%u bytes, binary)",
             szPath, ST_LOAD_BASE, (unsigned)uiTotal);
    return ST_NO_ERROR;
}

/*
 * load_do_prg() - Full ATARI ST PRG load: header, sections, BSS, fixups.
 */
static st_error_t load_do_prg(const char *szPath)
{
    file_t   *ptFile;
    st_u8_t   aHdr[ST_PRG_HEADER_SIZE];
    size_t    uiRead;
    st_u16_t  uiMagic;
    st_u32_t  uiTextSize;
    st_u32_t  uiDataSize;
    st_u32_t  uiBssSize;
    st_u32_t  uiSymSize;
    st_u16_t  uiAbsFlag;
    st_u32_t  uiContentSize; /* text + data (read from file) */
    st_u32_t  uiTotalSize;   /* text + data + bss (RAM footprint) */
    st_u32_t  uiFixupCount;

    ptFile     = NULL;
    uiFixupCount = 0;

    if (file_open(szPath, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
        return ST_ERROR;

    /* ----------------------------------------------------------------
     * 1. Read and validate the 28-byte header
     * ---------------------------------------------------------------- */
    uiRead = 0;
    if (file_read(ptFile, aHdr, ST_PRG_HEADER_SIZE, &uiRead)
            != ST_NO_ERROR
        || uiRead < ST_PRG_HEADER_SIZE)
    {
        file_close(&ptFile);
        LOG_ERROR("'%s': too short for PRG header (%lu bytes read)",
                  szPath, (unsigned long)uiRead);
        return ST_ERROR;
    }

    uiMagic = load_u16be(aHdr + 0);
    if (uiMagic != ST_PRG_MAGIC)
    {
        file_close(&ptFile);
        LOG_ERROR("'%s': bad PRG magic 0x%04X (expected 0x601A)",
                  szPath, (unsigned)uiMagic);
        return ST_ERROR;
    }

    uiTextSize = load_u32be(aHdr +  2);
    uiDataSize = load_u32be(aHdr +  6);
    uiBssSize  = load_u32be(aHdr + 10);
    uiSymSize  = load_u32be(aHdr + 14);
    /* aHdr[18..21] = reserved, aHdr[22..25] = flags — not used by UC15 */
    uiAbsFlag  = load_u16be(aHdr + 26);

    uiContentSize = uiTextSize + uiDataSize;
    uiTotalSize   = uiContentSize + uiBssSize;

    if (uiTotalSize > ST_LOAD_MAX_SIZE)
    {
        file_close(&ptFile);
        LOG_ERROR("'%s': PRG footprint too large (%lu bytes, max %lu)",
                  szPath,
                  (unsigned long)uiTotalSize,
                  (unsigned long)ST_LOAD_MAX_SIZE);
        return ST_ERROR;
    }

    /* ----------------------------------------------------------------
     * 2. Read .text + .data into ST RAM at ST_LOAD_BASE
     * ---------------------------------------------------------------- */
    if (uiContentSize > 0)
    {
        uiRead = 0;
        if (file_read(ptFile,
                      st_get_ram_pointer(ST_LOAD_BASE),
                      uiContentSize, &uiRead) != ST_NO_ERROR
            || uiRead < uiContentSize)
        {
            file_close(&ptFile);
            LOG_ERROR("'%s': short read: need %lu, got %lu bytes",
                      szPath,
                      (unsigned long)uiContentSize,
                      (unsigned long)uiRead);
            return ST_ERROR;
        }
    }

    /* ----------------------------------------------------------------
     * 3. Zero BSS area immediately after .data in RAM
     * ---------------------------------------------------------------- */
    if (uiBssSize > 0)
    {
        memset(st_get_ram_pointer(ST_LOAD_BASE + uiContentSize),
               0, (size_t)uiBssSize);
    }

    /* ----------------------------------------------------------------
     * 4. Skip symbol table (not needed for execution)
     * ---------------------------------------------------------------- */
    if (uiSymSize > 0)
    {
        if (load_skip_bytes(ptFile, uiSymSize) != ST_NO_ERROR)
        {
            file_close(&ptFile);
            LOG_ERROR("'%s': cannot skip symbol table (%lu bytes)",
                      szPath, (unsigned long)uiSymSize);
            return ST_ERROR;
        }
    }

    /* ----------------------------------------------------------------
     * 5. Apply fixup relocation table (unless abs_flag is set)
     * ---------------------------------------------------------------- */
    if (uiAbsFlag == 0)
    {
        if (load_apply_fixups(ptFile, szPath,
                              uiContentSize, &uiFixupCount)
                != ST_NO_ERROR)
        {
            file_close(&ptFile);
            return ST_ERROR;
        }
    }
    else
    {
        LOG_INFO("PRG '%s': absolute (abs_flag=1), no fixups", szPath);
    }

    file_close(&ptFile);

    /* ----------------------------------------------------------------
     * 6. Update load state
     * ---------------------------------------------------------------- */
    g_load_ptCtx.bLoaded      = ST_TRUE;
    g_load_ptCtx.eType        = LOAD_TYPE_PRG;
    strncpy(g_load_ptCtx.szPath, szPath, ST_MAX_PATH - 1);
    g_load_ptCtx.uiLoadAddr   = ST_LOAD_BASE;
    g_load_ptCtx.uiSize       = uiTotalSize;
    g_load_ptCtx.uiTextSize   = uiTextSize;
    g_load_ptCtx.uiDataSize   = uiDataSize;
    g_load_ptCtx.uiBssSize    = uiBssSize;
    g_load_ptCtx.uiFixupCount = uiFixupCount;

    LOG_INFO("loaded PRG '%s' at ST:0x%06X (.text=%u .data=%u .bss=%u "
             "%u fixup(s))",
             szPath, ST_LOAD_BASE,
             (unsigned)uiTextSize, (unsigned)uiDataSize,
             (unsigned)uiBssSize, (unsigned)uiFixupCount);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_u64_t load_init(st_u64_t ulSTMachineCtx)
{
    st_bool_t             bIsMachine = ST_FALSE;
    st_machine_context_t* ptMachine  = NULL;

    LOG_TRACE("Init load module - attach %p ST Machine",
                (void*)ulSTMachineCtx);

    /* -- [LOAD]1. Attach an existing ST machine -- */
    CHECK_OBJ(ulSTMachineCtx, ST_MACHINE_CTX, bIsMachine);
    if (!bIsMachine)
    {
        LOG_ERROR("ST Machine is invalid (%llu)", ulSTMachineCtx);
        return ST_ERROR;
    }
    ptMachine = (st_machine_context_t*)ulSTMachineCtx;
    g_load_ptCtx.bIsMachineOn = ptMachine->bPoweredOn;
    
    /* -- [LOAD]2. Init returns context sructure -- */
    LOG_INFO("Load module initialised");
    return (st_u64_t)&g_load_ptCtx;
}

st_error_t load_shutdown(void)
{
    LOG_TRACE("shutdown");
    memset(&g_load_ptCtx, 0, sizeof(g_load_ptCtx));
    LOG_INFO("load module shutdown");
    return ST_NO_ERROR;
}

st_error_t load_file(const char *szPath)
{
    file_stat_t tStat;

    LOG_TRACE("szPath='%s'", szPath ? szPath : "(null)");

    if (szPath == NULL)
    {
        LOG_ERROR("NULL szPath");
        return ST_ERROR;
    }

    if (g_load_ptCtx.bIsMachineOn == ST_FALSE)
    {
        LOG_ERROR("load_init() not called");
        return ST_ERROR;
    }

    memset(&tStat, 0, sizeof(tStat));
    if (file_stat(szPath, &tStat) != ST_NO_ERROR)
        return ST_ERROR;

    if (!tStat.bExists)
    {
        LOG_ERROR("'%s': file not found", szPath);
        return ST_ERROR;
    }

    if (tStat.bIsDir)
    {
        LOG_ERROR("'%s' is a directory — use 'mount'", szPath);
        return ST_ERROR;
    }

    if (load_is_image_ext(tStat.szExt))
    {
        LOG_ERROR("'%s' is a disk image — use 'mount'", szPath);
        return ST_ERROR;
    }

    if (load_is_prg_ext(tStat.szExt))
        return load_do_prg(szPath);

    return load_do_raw(szPath, &tStat, LOAD_TYPE_BINARY);
}

const load_context_t *load_get_context(void)
{
    return &g_load_ptCtx;
}
