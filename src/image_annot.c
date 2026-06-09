/*
 * image_annot.c - Atari ST disk image JSON annotation module
 *
 * UC24C: JSON annotation colocated with .st image (P47) and info band
 * notes (P48).  Hand-rolled minimal JSON parser — no external library.
 * Only the schema fields used by the hex editor band are parsed/written.
 *
 * JSON parser limitations:
 *   - Null-terminated input; max 64 KB file size enforced.
 *   - "labeled_sectors" objects searched forward from object start;
 *     pathological string values containing "key": are not handled.
 *   - No Unicode escape (\uXXXX) expansion beyond pass-through.
 */

#include "image_annot.h"
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------
 * Minimal JSON scanner helpers
 * ------------------------------------------------------------------ */

static const char *json_skip_ws(const char *p)
{
    while (*p && isspace((unsigned char)*p))
        p++;
    return p;
}

/*
 * Scan forward from p for the pattern  "szKey" :  and return a pointer
 * to the first non-whitespace char of the value, or NULL if not found.
 * Simple forward scan; does not track nesting depth — callers must pass
 * a pointer within the appropriate object scope.
 */
static const char *json_find_key(const char *p, const char *szKey)
{
    size_t uiKlen = strlen(szKey);

    while (*p)
    {
        if (*p == '"')
        {
            const char *q = p + 1;
            if (strncmp(q, szKey, uiKlen) == 0
            &&  q[uiKlen] == '"')
            {
                const char *v = json_skip_ws(q + uiKlen + 1);
                if (*v == ':')
                    return json_skip_ws(v + 1);
            }
            /* Skip this quoted string */
            p = q;
            while (*p && *p != '"')
            {
                if (*p == '\\' && *(p + 1))
                    p++;
                p++;
            }
            if (*p == '"')
                p++;
            continue;
        }
        p++;
    }
    return NULL;
}

/*
 * Read a JSON quoted string value at *p (must point to opening '"').
 * Copies at most uiLen-1 chars to buf; handles \", \\, \n, \t, \r.
 * Returns pointer past the closing '"'.
 */
static const char *json_read_string(const char *p,
                                     char       *buf,
                                     size_t      uiLen)
{
    size_t i = 0;

    if (*p != '"')
    {
        buf[0] = '\0';
        return p;
    }
    p++;

    while (*p && *p != '"')
    {
        char c;
        if (*p == '\\')
        {
            p++;
            switch (*p)
            {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case 'n':  c = '\n'; break;
            case 't':  c = '\t'; break;
            case 'r':  c = '\r'; break;
            default:   c = *p;   break;
            }
        }
        else
        {
            c = *p;
        }
        if (i < uiLen - 1)
            buf[i++] = c;
        p++;
    }
    buf[i] = '\0';
    if (*p == '"')
        p++;
    return p;
}

/*
 * Read a JSON integer value (possibly negative) at p.
 * Returns pointer past the digits.
 */
static const char *json_read_int(const char *p, int *piOut)
{
    int iNeg = 0;
    int iVal = 0;

    p = json_skip_ws(p);
    if (*p == '-')
    {
        iNeg = 1;
        p++;
    }
    while (*p >= '0' && *p <= '9')
    {
        iVal = iVal * 10 + (*p - '0');
        p++;
    }
    *piOut = iNeg ? -iVal : iVal;
    return p;
}

/* ------------------------------------------------------------------
 * labeled_sectors array parser
 * ------------------------------------------------------------------ */

/*
 * Parse the labeled_sectors JSON array starting at p (p → '[').
 * Appends found entries to ptAnnot via image_annot_set_sector().
 */
static void json_parse_labeled_sectors(const char    *p,
                                        image_annot_t *ptAnnot)
{
    char       szType[ANNOT_TYPE_MAX];
    char       szNotes[ANNOT_NOTE_MAX];
    int        iLba;
    const char *obj;
    const char *val;
    const char *end;
    int         iDepth;

    if (*p != '[')
        return;
    p++;

    while (*p)
    {
        p = json_skip_ws(p);
        if (*p == ']')
            break;
        if (*p != '{')
        {
            p++;
            continue;
        }

        /* Locate end of this object */
        iDepth = 1;
        end    = p + 1;
        while (*end && iDepth > 0)
        {
            if (*end == '"')
            {
                end++;
                while (*end && *end != '"')
                {
                    if (*end == '\\' && *(end + 1))
                        end++;
                    end++;
                }
                if (*end == '"')
                    end++;
                continue;
            }
            if (*end == '{')
                iDepth++;
            if (*end == '}')
                iDepth--;
            end++;
        }

        /* Extract lba, type, notes from this object */
        iLba      = -1;
        szType[0] = szNotes[0] = '\0';

        obj = p;
        val = json_find_key(obj, "lba");
        if (val)
            json_read_int(val, &iLba);

        obj = p;
        val = json_find_key(obj, "type");
        if (val && *val == '"')
            json_read_string(val, szType, ANNOT_TYPE_MAX);

        obj = p;
        val = json_find_key(obj, "notes");
        if (val && *val == '"')
            json_read_string(val, szNotes, ANNOT_NOTE_MAX);

        if (iLba >= 0)
            image_annot_set_sector(ptAnnot, iLba, szType, szNotes);

        p = end;
        p = json_skip_ws(p);
        if (*p == ',')
            p++;
    }
}

/* ------------------------------------------------------------------
 * JSON writer helpers
 * ------------------------------------------------------------------ */

static void json_write_str(FILE *f, const char *sz)
{
    fputc('"', f);
    while (*sz)
    {
        switch (*sz)
        {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n",  f); break;
        case '\t': fputs("\\t",  f); break;
        case '\r': fputs("\\r",  f); break;
        default:
            if ((unsigned char)*sz < 0x20u)
                fprintf(f, "\\u%04x", (unsigned char)*sz);
            else
                fputc(*sz, f);
            break;
        }
        sz++;
    }
    fputc('"', f);
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

st_error_t image_annot_create(image_annot_t **pptAnnot)
{
    image_annot_t  *ptA  = NULL;
    annot_sector_t *atSec = NULL;

    if (pptAnnot == NULL)
    {
        LOG_ERROR("NULL parameter: pptAnnot=%p", (void *)pptAnnot);
        return ST_ERROR;
    }

    ptA = (image_annot_t *)malloc(sizeof(*ptA));
    if (ptA == NULL)
    {
        LOG_ERROR("malloc failed for image_annot_t");
        return ST_ERROR;
    }
    memset(ptA, 0, sizeof(*ptA));

    atSec = (annot_sector_t *)malloc(
                ANNOT_INIT_CAP * sizeof(annot_sector_t));
    if (atSec == NULL)
    {
        LOG_ERROR("malloc failed for annot_sector_t[%d]",
                  ANNOT_INIT_CAP);
        free(ptA);
        return ST_ERROR;
    }

    ptA->atSectors    = atSec;
    ptA->iSectorCount = 0;
    ptA->iSectorCap   = ANNOT_INIT_CAP;
    *pptAnnot         = ptA;
    return ST_NO_ERROR;
}

st_error_t image_annot_destroy(image_annot_t **pptAnnot)
{
    if (pptAnnot == NULL)
    {
        LOG_ERROR("NULL parameter: pptAnnot=%p", (void *)pptAnnot);
        return ST_ERROR;
    }
    if (*pptAnnot == NULL)
        return ST_NO_ERROR;

    free((*pptAnnot)->atSectors);
    free(*pptAnnot);
    *pptAnnot = NULL;
    return ST_NO_ERROR;
}

void image_annot_json_path(const char *szFilePath,
                            char       *szJsonPath,
                            size_t      uiLen)
{
    const char *p;
    const char *pDot    = NULL;
    size_t      uiBase;

    if (szFilePath == NULL || szJsonPath == NULL || uiLen < 6u)
    {
        if (szJsonPath && uiLen > 0u)
            szJsonPath[0] = '\0';
        return;
    }

    /* Find the last dot that follows the last path separator */
    for (p = szFilePath; *p; p++)
    {
        if (*p == '/')
        {
            pDot = NULL;
        }
        else if (*p == '\\')
        {
            pDot = NULL;
        }
        else if (*p == '.' && *(p + 1) != '\0'
        &&       *(p + 1) != '/' && *(p + 1) != '\\')
        {
            pDot = p;
        }
    }

    uiBase = (pDot != NULL)
             ? (size_t)(pDot - szFilePath)
             : strlen(szFilePath);

    if (uiBase + 5u >= uiLen)
        uiBase = uiLen - 6u;

    memcpy(szJsonPath, szFilePath, uiBase);
    memcpy(szJsonPath + uiBase, ".json", 6u); /* includes NUL */
}

st_error_t image_annot_load(const char    *szJsonPath,
                              image_annot_t *ptAnnot)
{
    FILE       *f      = NULL;
    char       *pBuf   = NULL;
    long        lSize;
    size_t      uiRead;
    const char *val;

    if (szJsonPath == NULL || ptAnnot == NULL)
    {
        LOG_ERROR("NULL parameter: szJsonPath=%p ptAnnot=%p",
                  (void *)szJsonPath, (void *)ptAnnot);
        return ST_ERROR;
    }

    f = fopen(szJsonPath, "r");
    if (f == NULL)
        return ST_ERROR; /* absent — non-fatal */

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return ST_ERROR;
    }
    lSize = ftell(f);
    if (lSize <= 0L || lSize > 65536L)
    {
        fclose(f);
        return ST_ERROR;
    }
    rewind(f);

    pBuf = (char *)malloc((size_t)lSize + 1u);
    if (pBuf == NULL)
    {
        LOG_ERROR("malloc failed for JSON buffer (%ld bytes)", lSize);
        fclose(f);
        return ST_ERROR;
    }

    uiRead      = fread(pBuf, 1u, (size_t)lSize, f);
    pBuf[uiRead] = '\0';
    fclose(f);

    /* Parse "filename" */
    val = json_find_key(pBuf, "filename");
    if (val && *val == '"')
        json_read_string(val, ptAnnot->szFilename, ST_MAX_PATH);

    /* Parse global "notes" */
    val = json_find_key(pBuf, "notes");
    if (val && *val == '"')
        json_read_string(val, ptAnnot->szNotes, ANNOT_NOTE_MAX);

    /* Parse "labeled_sectors" array */
    val = json_find_key(pBuf, "labeled_sectors");
    if (val && *val == '[')
        json_parse_labeled_sectors(val, ptAnnot);

    free(pBuf);
    LOG_INFO("annotation loaded '%s' (%d sectors)",
             szJsonPath, ptAnnot->iSectorCount);
    return ST_NO_ERROR;
}

st_error_t image_annot_save(const image_annot_t *ptAnnot,
                              const char          *szJsonPath)
{
    FILE *f;
    int   i;

    if (ptAnnot == NULL || szJsonPath == NULL)
    {
        LOG_ERROR("NULL parameter: ptAnnot=%p szJsonPath=%p",
                  (void *)ptAnnot, (void *)szJsonPath);
        return ST_ERROR;
    }

    f = fopen(szJsonPath, "w");
    if (f == NULL)
    {
        LOG_ERROR("fopen(WRITE) failed for '%s'", szJsonPath);
        return ST_ERROR;
    }

    fputs("{\n", f);

    fputs("  \"filename\": ", f);
    json_write_str(f, ptAnnot->szFilename);
    fputs(",\n", f);

    fputs("  \"notes\": ", f);
    json_write_str(f, ptAnnot->szNotes);
    fputs(",\n", f);

    fputs("  \"labeled_sectors\": [", f);
    for (i = 0; i < ptAnnot->iSectorCount; i++)
    {
        const annot_sector_t *ptS = &ptAnnot->atSectors[i];
        fputs((i == 0) ? "\n" : ",\n", f);
        fprintf(f, "    { \"lba\": %d, \"type\": ", ptS->iLba);
        json_write_str(f, ptS->szType);
        fputs(", \"notes\": ", f);
        json_write_str(f, ptS->szNotes);
        fputs(" }", f);
    }
    fputs((ptAnnot->iSectorCount > 0) ? "\n  ]\n" : " ]\n", f);
    fputs("}\n", f);

    fclose(f);
    LOG_INFO("annotation saved '%s' (%d sectors)",
             szJsonPath, ptAnnot->iSectorCount);
    return ST_NO_ERROR;
}

const annot_sector_t *image_annot_get_sector(
                            const image_annot_t *ptAnnot,
                            int                  iLba)
{
    int i;

    if (ptAnnot == NULL)
        return NULL;

    for (i = 0; i < ptAnnot->iSectorCount; i++)
    {
        if (ptAnnot->atSectors[i].iLba == iLba)
            return &ptAnnot->atSectors[i];
    }
    return NULL;
}

st_error_t image_annot_set_sector(image_annot_t *ptAnnot,
                                   int            iLba,
                                   const char    *szType,
                                   const char    *szNotes)
{
    annot_sector_t *ptS  = NULL;
    annot_sector_t *pNew = NULL;
    int             i;
    int             iNewCap;

    if (ptAnnot == NULL)
    {
        LOG_ERROR("NULL parameter: ptAnnot=%p", (void *)ptAnnot);
        return ST_ERROR;
    }

    /* Find existing entry */
    for (i = 0; i < ptAnnot->iSectorCount; i++)
    {
        if (ptAnnot->atSectors[i].iLba == iLba)
        {
            ptS = &ptAnnot->atSectors[i];
            break;
        }
    }

    /* Append new entry if not found */
    if (ptS == NULL)
    {
        if (ptAnnot->iSectorCount >= ptAnnot->iSectorCap)
        {
            iNewCap = ptAnnot->iSectorCap * 2;
            pNew = (annot_sector_t *)realloc(
                        ptAnnot->atSectors,
                        (size_t)iNewCap * sizeof(annot_sector_t));
            if (pNew == NULL)
            {
                LOG_ERROR("realloc failed for annot_sector_t[%d]",
                          iNewCap);
                return ST_ERROR;
            }
            ptAnnot->atSectors  = pNew;
            ptAnnot->iSectorCap = iNewCap;
        }
        ptS = &ptAnnot->atSectors[ptAnnot->iSectorCount];
        memset(ptS, 0, sizeof(*ptS));
        ptS->iLba = iLba;
        ptAnnot->iSectorCount++;
    }

    if (szType != NULL)
    {
        strncpy(ptS->szType, szType, ANNOT_TYPE_MAX - 1);
        ptS->szType[ANNOT_TYPE_MAX - 1] = '\0';
    }
    if (szNotes != NULL)
    {
        strncpy(ptS->szNotes, szNotes, ANNOT_NOTE_MAX - 1);
        ptS->szNotes[ANNOT_NOTE_MAX - 1] = '\0';
    }

    return ST_NO_ERROR;
}
