/*
 * load.c - ST4Ever file load into emulated ST RAM
 *
 * UC7: file type detection + raw binary load + PRG header validation.
 *
 * TODO(UC15): Full PRG section layout + fixup relocation table parse.
 * TODO(UC24): Allocate load address from memory map (above TOS vars).
 */

#include "load.h"
#include "file.h"
#include "trace.h"

#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Module globals
 * ------------------------------------------------------------------ */

static st_machine_t *g_load_ptMachine = NULL;
static load_state_t  g_load_tState;

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
        memcpy(&g_load_ptMachine->aRam[ST_LOAD_BASE + uiTotal],
               aBuf, uiRead);
        uiTotal += uiRead;
    }
    while (uiRead == sizeof(aBuf));

    file_close(&ptFile);

    memset(&g_load_tState, 0, sizeof(g_load_tState));
    g_load_tState.bLoaded    = ST_TRUE;
    g_load_tState.eType      = eType;
    strncpy(g_load_tState.szPath, szPath, ST_MAX_PATH - 1);
    g_load_tState.uiLoadAddr = ST_LOAD_BASE;
    g_load_tState.uiSize     = (st_u32_t)uiTotal;

    LOG_INFO("loaded '%s' at ST:0x%06X (%u bytes, binary)",
             szPath, ST_LOAD_BASE, (unsigned)uiTotal);
    return ST_NO_ERROR;
}

/*
 * load_do_prg() - Validate PRG header and load .text+.data into RAM.
 *
 * UC7 stub: magic check + section sizes only.
 * TODO(UC15): Parse fixup table and relocate addresses.
 */
static st_error_t load_do_prg(const char *szPath)
{
    file_t  *ptFile;
    st_u8_t  aHdr[ST_PRG_HEADER_SIZE];
    size_t   uiRead;
    st_u16_t uiMagic;
    st_u32_t uiTextSize;
    st_u32_t uiDataSize;
    st_u32_t uiContentSize;

    ptFile = NULL;

    if (file_open(szPath, FILE_MODE_READ, &ptFile) != ST_NO_ERROR)
        return ST_ERROR;

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

    uiTextSize    = load_u32be(aHdr + 2);
    uiDataSize    = load_u32be(aHdr + 6);
    uiContentSize = uiTextSize + uiDataSize;

    if (uiContentSize > ST_LOAD_MAX_SIZE)
    {
        file_close(&ptFile);
        LOG_ERROR("'%s': .text+.data too large (%lu bytes)",
                  szPath, (unsigned long)uiContentSize);
        return ST_ERROR;
    }

    if (uiContentSize > 0)
    {
        uiRead = 0;
        if (file_read(ptFile,
                      &g_load_ptMachine->aRam[ST_LOAD_BASE],
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

    file_close(&ptFile);

    LOG_TODO("load_do_prg: fixup relocation not yet implemented (UC15) "
             "— '%s' loaded at ST:0x%06X without relocation",
             szPath, ST_LOAD_BASE);

    memset(&g_load_tState, 0, sizeof(g_load_tState));
    g_load_tState.bLoaded    = ST_TRUE;
    g_load_tState.eType      = LOAD_TYPE_PRG;
    strncpy(g_load_tState.szPath, szPath, ST_MAX_PATH - 1);
    g_load_tState.uiLoadAddr = ST_LOAD_BASE;
    g_load_tState.uiSize     = uiContentSize;

    LOG_INFO("loaded PRG '%s' at ST:0x%06X (.text=%u .data=%u bytes, "
             "stub — fixup UC15)",
             szPath, ST_LOAD_BASE,
             (unsigned)uiTextSize, (unsigned)uiDataSize);
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_error_t load_init(st_machine_t *ptMachine)
{
    LOG_TRACE("ptMachine=%p", (void *)ptMachine);

    if (ptMachine == NULL)
    {
        LOG_ERROR("NULL ptMachine");
        return ST_ERROR;
    }

    g_load_ptMachine = ptMachine;
    memset(&g_load_tState, 0, sizeof(g_load_tState));
    LOG_INFO("load module initialised");
    return ST_NO_ERROR;
}

st_error_t load_shutdown(void)
{
    LOG_TRACE("shutdown");
    g_load_ptMachine = NULL;
    memset(&g_load_tState, 0, sizeof(g_load_tState));
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

    if (g_load_ptMachine == NULL)
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

const load_state_t *load_get_state(void)
{
    return &g_load_tState;
}
