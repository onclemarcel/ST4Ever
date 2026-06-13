/*
 * as.c - DEVPAC3 68000 assembler
 *
 * UC30A: two-pass infrastructure — directives only.
 *   Pass 1: lex source, build symbol table, compute section sizes.
 *   Pass 2: emit binary, resolve labels.
 *   Output: 28-byte PRG header + .text + .data + fixup table.
 *
 * Supported in UC30A: SECTION, DC.B/W/L, DS.B/W/L, EVEN, EQU,
 *                     SET, END.
 * UC30B: EA encoder + MOVE.B/W/L / MOVEA / MOVEQ / LEA / CLR / SWAP.
 * UC30C-D: ALU, branch, shift instructions (TODO).
 */

#include "as.h"
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Internal constants                                                   */
/* ------------------------------------------------------------------ */

#define AS_MAX_LINE     1024     /* max source line length             */
#define AS_MAX_OPSZ     768      /* max operands string                */
#define AS_MAX_WORD     64       /* max identifier / token length      */

#define AS_SEC_NONE    -1        /* absolute (EQU) symbol              */
#define AS_SEC_TEXT     0
#define AS_SEC_DATA     1
#define AS_SEC_BSS      2

/* PRG header: 28 bytes, all big-endian (matches load.c) */
#define AS_PRG_HDR_SZ   28u

/* ------------------------------------------------------------------ */
/* Internal types                                                       */
/* ------------------------------------------------------------------ */

typedef struct as_buf_s
{
    st_u8_t  *pData;
    st_u32_t  uiSize;
    st_u32_t  uiCap;
} as_buf_t;

typedef struct as_sym_s
{
    char      szName[AS_MAX_WORD];
    st_u32_t  uiValue;     /* offset within section, or absolute     */
    int       iSection;    /* AS_SEC_TEXT/DATA/BSS or AS_SEC_NONE    */
    st_bool_t bDefined;
} as_sym_t;

typedef struct as_fix_s
{
    st_u32_t uiOffset;     /* byte offset from .text start in output */
} as_fix_t;

typedef struct as_tok_s
{
    char szLabel[AS_MAX_WORD];  /* label (empty if none)             */
    char szMnem[AS_MAX_WORD];   /* mnemonic / directive (uppercased) */
    char cSize;                 /* 'B','W','L' or 0                  */
    char szOps[AS_MAX_OPSZ];    /* all operands (comment stripped)   */
    int  iLine;
} as_tok_t;

typedef struct as_st_s
{
    as_sym_t  aSyms[AS_MAX_SYMBOLS];
    int       iSymCount;

    as_buf_t  tText;
    as_buf_t  tData;

    int       iCurSec;
    st_u32_t  auPCPerSec[3];   /* saved PC per section (pass 1)     */
    st_u32_t  uiPC;            /* current PC in current section     */

    /* Sizes locked at end of pass 1                                 */
    st_u32_t  uiTextSzP1;
    st_u32_t  uiDataSzP1;
    st_u32_t  uiBssSzP1;

    as_fix_t *ptFix;
    int       iFixCount;
    int       iFixCap;

    int       iPass;
} as_st_t;

/* ------------------------------------------------------------------ */
/* Buffer helpers                                                       */
/* ------------------------------------------------------------------ */

static st_error_t as_buf_need(as_buf_t *pt, st_u32_t uiExtra)
{
    st_u8_t  *pNew;
    st_u32_t  uiNewCap;

    if (pt->uiSize + uiExtra <= pt->uiCap)
        return ST_NO_ERROR;

    uiNewCap = (pt->uiCap < 4096u) ? 4096u : pt->uiCap * 2u;
    while (uiNewCap < pt->uiSize + uiExtra)
        uiNewCap *= 2u;

    pNew = (st_u8_t *)realloc(pt->pData, uiNewCap);
    if (pNew == NULL)
        return ST_ERROR;

    pt->pData = pNew;
    pt->uiCap = uiNewCap;
    return ST_NO_ERROR;
}

static st_error_t as_buf_emit_b(as_buf_t *pt, st_u8_t b)
{
    if (as_buf_need(pt, 1u) != ST_NO_ERROR) return ST_ERROR;
    pt->pData[pt->uiSize++] = b;
    return ST_NO_ERROR;
}

static st_error_t as_buf_emit_be16(as_buf_t *pt, st_u16_t w)
{
    if (as_buf_need(pt, 2u) != ST_NO_ERROR) return ST_ERROR;
    pt->pData[pt->uiSize++] = (st_u8_t)(w >> 8);
    pt->pData[pt->uiSize++] = (st_u8_t)(w & 0xFFu);
    return ST_NO_ERROR;
}

static st_error_t as_buf_emit_be32(as_buf_t *pt, st_u32_t l)
{
    if (as_buf_need(pt, 4u) != ST_NO_ERROR) return ST_ERROR;
    pt->pData[pt->uiSize++] = (st_u8_t)((l >> 24) & 0xFFu);
    pt->pData[pt->uiSize++] = (st_u8_t)((l >> 16) & 0xFFu);
    pt->pData[pt->uiSize++] = (st_u8_t)((l >>  8) & 0xFFu);
    pt->pData[pt->uiSize++] = (st_u8_t)( l        & 0xFFu);
    return ST_NO_ERROR;
}

static void as_buf_free(as_buf_t *pt)
{
    if (pt->pData != NULL) { free(pt->pData); pt->pData = NULL; }
    pt->uiSize = pt->uiCap = 0u;
}

/* ------------------------------------------------------------------ */
/* Error helper                                                         */
/* ------------------------------------------------------------------ */

static void as_err(as_context_t *ptCtx, int iLine,
                   const char *szFmt, ...)
{
    va_list ap;
    as_error_t *ptNew;
    size_t uiNewCap;

    if (ptCtx->uiErrorCount >= AS_MAX_ERRORS)
        return;

    uiNewCap = ptCtx->uiErrorCount + 1u;
    ptNew = (as_error_t *)realloc(ptCtx->ptErrors,
                                   uiNewCap * sizeof(as_error_t));
    if (ptNew == NULL)
        return;

    ptCtx->ptErrors = ptNew;
    ptCtx->ptErrors[ptCtx->uiErrorCount].iLine = iLine;

    va_start(ap, szFmt);
    vsnprintf(ptCtx->ptErrors[ptCtx->uiErrorCount].szMsg,
              ST_MAX_MSG, szFmt, ap);
    va_end(ap);

    LOG_ERROR("line %d: %s", iLine,
              ptCtx->ptErrors[ptCtx->uiErrorCount].szMsg);
    ptCtx->uiErrorCount++;
}

/* ------------------------------------------------------------------ */
/* Symbol table                                                         */
/* ------------------------------------------------------------------ */

static int as_sym_find(const as_st_t *ptS, const char *szName)
{
    int i;
    for (i = 0; i < ptS->iSymCount; i++)
        if (strcmp(ptS->aSyms[i].szName, szName) == 0)
            return i;
    return -1;
}

static st_error_t as_sym_define(as_st_t *ptS, as_context_t *ptCtx,
                                  const char *szName, st_u32_t uiVal,
                                  int iSec, int iLine)
{
    int iIdx;
    as_sym_t *ptSym;

    iIdx = as_sym_find(ptS, szName);
    if (iIdx >= 0)
    {
        ptSym = &ptS->aSyms[iIdx];
        if (ptSym->bDefined && ptS->iPass == 1)
        {
            as_err(ptCtx, iLine, "symbol '%s' already defined", szName);
            return ST_ERROR;
        }
        ptSym->uiValue  = uiVal;
        ptSym->iSection = iSec;
        ptSym->bDefined = ST_TRUE;
        return ST_NO_ERROR;
    }

    if (ptS->iSymCount >= AS_MAX_SYMBOLS)
    {
        as_err(ptCtx, iLine, "symbol table full (max %d)", AS_MAX_SYMBOLS);
        return ST_ERROR;
    }

    ptSym = &ptS->aSyms[ptS->iSymCount++];
    memset(ptSym, 0, sizeof(*ptSym));
    strncpy(ptSym->szName, szName, AS_MAX_WORD - 1);
    ptSym->uiValue  = uiVal;
    ptSym->iSection = iSec;
    ptSym->bDefined = ST_TRUE;
    return ST_NO_ERROR;
}

/* Return combined offset from .text start for a symbol */
static st_u32_t as_sym_combined(const as_sym_t *ptSym,
                                  const as_st_t  *ptS)
{
    switch (ptSym->iSection)
    {
    case AS_SEC_TEXT: return ptSym->uiValue;
    case AS_SEC_DATA: return ptS->uiTextSzP1 + ptSym->uiValue;
    case AS_SEC_BSS:  return ptS->uiTextSzP1 + ptS->uiDataSzP1
                             + ptSym->uiValue;
    default:          return ptSym->uiValue;
    }
}

/* ------------------------------------------------------------------ */
/* String / char helpers                                                */
/* ------------------------------------------------------------------ */

static void as_upper(char *sz)
{
    while (*sz) { *sz = (char)toupper((unsigned char)*sz); sz++; }
}

static const char *as_skip_sp(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static st_bool_t as_is_ident(char c, st_bool_t bFirst)
{
    if (c == '_' || c == '.' || isalpha((unsigned char)c))
        return ST_TRUE;
    if (!bFirst && isdigit((unsigned char)c))
        return ST_TRUE;
    return ST_FALSE;
}

static int as_read_word(const char *p, char *szOut, int iMax)
{
    int i = 0;
    while (i < iMax - 1 && as_is_ident(p[i], (st_bool_t)(i == 0)))
    {
        szOut[i] = p[i];
        i++;
    }
    szOut[i] = '\0';
    return i;
}

/* Directives recognised at column 0 (not treated as labels) */
static const char *g_as_aDirs[] =
{
    "SECTION", "DC", "DS", "EVEN", "EQU", "SET", "END", "ORG",
    "INCLUDE", "MACRO", "ENDM", "REPT", "ENDR",
    "IFD", "IFND", "IFEQ", "IFNE", "IFGT", "IFGE", "IFLT", "IFLE",
    "ELSE", "ENDIF", NULL
};

static st_bool_t as_is_directive(const char *sz)
{
    int i;
    for (i = 0; g_as_aDirs[i] != NULL; i++)
        if (strcmp(sz, g_as_aDirs[i]) == 0)
            return ST_TRUE;
    return ST_FALSE;
}

/* ------------------------------------------------------------------ */
/* Lexer                                                                */
/* ------------------------------------------------------------------ */

static void as_lex_mnem(const char **pp, char *szMnem, char *pcSize)
{
    const char *p = *pp;
    int         i = 0;
    char        c;

    *pcSize  = 0;
    szMnem[0] = '\0';

    while (i < AS_MAX_WORD - 1 &&
           (isalnum((unsigned char)*p) || *p == '_'))
        szMnem[i++] = (char)toupper((unsigned char)*p++);
    szMnem[i] = '\0';

    if (*p == '.')
    {
        c = (char)toupper((unsigned char)p[1]);
        if (c == 'B' || c == 'W' || c == 'L') { *pcSize = c; p += 2; }
    }
    *pp = p;
}

static void as_lex_ops(const char *p, char *szOps)
{
    const char *pEnd;
    int         iLen;

    p    = as_skip_sp(p);
    pEnd = p;
    while (*pEnd && *pEnd != ';' && *pEnd != '\r' && *pEnd != '\n')
        pEnd++;
    while (pEnd > p && (pEnd[-1] == ' ' || pEnd[-1] == '\t'))
        pEnd--;

    iLen = (int)(pEnd - p);
    if (iLen >= AS_MAX_OPSZ) iLen = AS_MAX_OPSZ - 1;
    memcpy(szOps, p, (size_t)iLen);
    szOps[iLen] = '\0';
}

static void as_lex_line(const char *szLine, as_tok_t *ptTok, int iLine)
{
    const char *p = szLine;
    char        szWord[AS_MAX_WORD];
    char        szUp[AS_MAX_WORD];
    int         iLen;

    memset(ptTok, 0, sizeof(*ptTok));
    ptTok->iLine = iLine;

    /* Blank, comment-only, or carriage-return lines */
    if (*p == ';' || *p == '\0' || *p == '\r' || *p == '\n')
        return;
    if (*p == '*') /* DEVPAC3 full-line comment */
        return;

    if (*p != ' ' && *p != '\t')
    {
        /* Token at column 0 */
        iLen = as_read_word(p, szWord, AS_MAX_WORD);
        if (iLen == 0) return;

        memcpy(szUp, szWord, sizeof(szWord));
        as_upper(szUp);

        if (as_is_directive(szUp))
        {
            /* Directive at col 0 — parse size suffix then operands */
            p += iLen;
            as_lex_mnem(&p, ptTok->szMnem, &ptTok->cSize);
            /* szMnem already uppercase from as_lex_mnem,
             * but we extracted it manually above; copy szUp */
            strncpy(ptTok->szMnem, szUp, AS_MAX_WORD - 1);
            /* re-check size after the word we already consumed */
            if (*p == '.')
            {
                char c = (char)toupper((unsigned char)p[1]);
                if (c == 'B' || c == 'W' || c == 'L')
                { ptTok->cSize = c; p += 2; }
            }
            p = as_skip_sp(p);
            as_lex_ops(p, ptTok->szOps);
        }
        else
        {
            /* Label at col 0 */
            strncpy(ptTok->szLabel, szWord, AS_MAX_WORD - 1);
            p += iLen;
            if (*p == ':') p++;
            p = as_skip_sp(p);

            if (*p == '\0' || *p == ';' || *p == '\r' || *p == '\n')
                return; /* label-only line */

            as_lex_mnem(&p, ptTok->szMnem, &ptTok->cSize);
            p = as_skip_sp(p);
            as_lex_ops(p, ptTok->szOps);
        }
    }
    else
    {
        /* Indented line — no label */
        p = as_skip_sp(p);
        if (*p == '\0' || *p == ';') return;
        as_lex_mnem(&p, ptTok->szMnem, &ptTok->cSize);
        p = as_skip_sp(p);
        as_lex_ops(p, ptTok->szOps);
    }
}

/* ------------------------------------------------------------------ */
/* Expression evaluator                                                 */
/* ------------------------------------------------------------------ */

/*
 * Evaluate one expression token.  Supports:
 *   $FF / 0xFF / 255  integer constants
 *   'A'               character literal
 *   identifier        symbol reference
 *   expr+N / expr-N   simple offset arithmetic
 *
 * Returns ST_TRUE on success (puiVal / piSec set).
 * Returns ST_FALSE if a symbol is not yet defined (pass 1 only).
 */
static st_bool_t as_eval(const as_st_t *ptS, const char *szExpr,
                           st_u32_t *puiVal, int *piSec)
{
    const char *p = as_skip_sp(szExpr);
    char        szName[AS_MAX_WORD];
    int         iIdx;
    st_u32_t    uiBase;
    int         iBaseSec;
    int         iLen;
    char        cOp;
    long        lOff;

    *puiVal = 0u;
    *piSec  = AS_SEC_NONE;

    if (*p == '\0')
        return ST_FALSE;

    /* Hex: $xx */
    if (*p == '$')
    {
        *puiVal = (st_u32_t)strtoul(p + 1, NULL, 16);
        *piSec  = AS_SEC_NONE;
        return ST_TRUE;
    }

    /* Hex: 0x / 0X */
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        *puiVal = (st_u32_t)strtoul(p + 2, NULL, 16);
        *piSec  = AS_SEC_NONE;
        return ST_TRUE;
    }

    /* Character literal: 'A' */
    if (*p == '\'')
    {
        if (p[1] != '\0' && p[2] == '\'')
        {
            *puiVal = (st_u32_t)(unsigned char)p[1];
            *piSec  = AS_SEC_NONE;
            return ST_TRUE;
        }
        return ST_FALSE;
    }

    /* Negative decimal: -N (used in MOVEQ / immediate operands) */
    if (*p == '-' && isdigit((unsigned char)p[1]))
    {
        *puiVal = (st_u32_t)(st_i32_t)strtol(p, NULL, 10);
        *piSec  = AS_SEC_NONE;
        return ST_TRUE;
    }

    /* Decimal constant */
    if (isdigit((unsigned char)*p))
    {
        *puiVal = (st_u32_t)strtoul(p, NULL, 10);
        *piSec  = AS_SEC_NONE;
        return ST_TRUE;
    }

    /* Identifier (possibly followed by +/- offset) */
    if (as_is_ident(*p, ST_TRUE))
    {
        iLen = as_read_word(p, szName, AS_MAX_WORD);
        p += iLen;

        iIdx = as_sym_find(ptS, szName);
        if (iIdx < 0 || !ptS->aSyms[iIdx].bDefined)
            return ST_FALSE;

        uiBase  = ptS->aSyms[iIdx].uiValue;
        iBaseSec = ptS->aSyms[iIdx].iSection;

        /* Optional +/- constant */
        p = as_skip_sp(p);
        cOp = *p;
        if (cOp == '+' || cOp == '-')
        {
            p++;
            p = as_skip_sp(p);
            lOff = strtol(p, NULL, 0);
            if (cOp == '+')
                uiBase += (st_u32_t)lOff;
            else
                uiBase -= (st_u32_t)lOff;
        }

        *puiVal = uiBase;
        *piSec  = iBaseSec;
        return ST_TRUE;
    }

    return ST_FALSE;
}

/* ------------------------------------------------------------------ */
/* Fixup list                                                           */
/* ------------------------------------------------------------------ */

static st_error_t as_fix_add(as_st_t *ptS, as_context_t *ptCtx,
                               st_u32_t uiOff, int iLine)
{
    as_fix_t *ptNew;
    int       iNewCap;

    if (ptS->iFixCount >= ptS->iFixCap)
    {
        iNewCap = (ptS->iFixCap == 0) ? 64 : ptS->iFixCap * 2;
        ptNew = (as_fix_t *)realloc(ptS->ptFix,
                                     (size_t)iNewCap * sizeof(as_fix_t));
        if (ptNew == NULL)
        {
            as_err(ptCtx, iLine, "out of memory (fixup list)");
            return ST_ERROR;
        }
        ptS->ptFix    = ptNew;
        ptS->iFixCap  = iNewCap;
    }
    ptS->ptFix[ptS->iFixCount++].uiOffset = uiOff;
    return ST_NO_ERROR;
}

/* Sort helper for qsort */
static int as_fix_cmp(const void *a, const void *b)
{
    st_u32_t uA = ((const as_fix_t *)a)->uiOffset;
    st_u32_t uB = ((const as_fix_t *)b)->uiOffset;
    return (uA < uB) ? -1 : (uA > uB) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Get current section buffer (pass 2 only)                            */
/* ------------------------------------------------------------------ */

static as_buf_t *as_cur_buf(as_st_t *ptS)
{
    return (ptS->iCurSec == AS_SEC_TEXT) ? &ptS->tText : &ptS->tData;
}

/* ------------------------------------------------------------------ */
/* Advance PC (pass 1) or emit byte (pass 2)                           */
/* ------------------------------------------------------------------ */

static st_error_t as_emit_b(as_st_t *ptS, as_context_t *ptCtx,
                              st_u8_t b, int iLine)
{
    if (ptS->iCurSec == AS_SEC_BSS)
    {
        ptS->uiPC++;
        return ST_NO_ERROR; /* BSS: count only, no data */
    }
    if (ptS->iPass == 2)
    {
        if (as_buf_emit_b(as_cur_buf(ptS), b) != ST_NO_ERROR)
        {
            as_err(ptCtx, iLine, "out of memory");
            return ST_ERROR;
        }
    }
    ptS->uiPC++;
    return ST_NO_ERROR;
}

static st_error_t as_emit_be16(as_st_t *ptS, as_context_t *ptCtx,
                                 st_u16_t w, int iLine)
{
    if (ptS->iCurSec == AS_SEC_BSS) { ptS->uiPC += 2u; return ST_NO_ERROR; }
    if (ptS->iPass == 2)
    {
        if (as_buf_emit_be16(as_cur_buf(ptS), w) != ST_NO_ERROR)
        { as_err(ptCtx, iLine, "out of memory"); return ST_ERROR; }
    }
    ptS->uiPC += 2u;
    return ST_NO_ERROR;
}

static st_error_t as_emit_be32(as_st_t *ptS, as_context_t *ptCtx,
                                 st_u32_t l, int iLine)
{
    if (ptS->iCurSec == AS_SEC_BSS) { ptS->uiPC += 4u; return ST_NO_ERROR; }
    if (ptS->iPass == 2)
    {
        if (as_buf_emit_be32(as_cur_buf(ptS), l) != ST_NO_ERROR)
        { as_err(ptCtx, iLine, "out of memory"); return ST_ERROR; }
    }
    ptS->uiPC += 4u;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------ */
/* Directive handlers                                                   */
/* ------------------------------------------------------------------ */

static st_error_t as_do_section(as_st_t *ptS, const char *szOps)
{
    char szSec[AS_MAX_WORD];
    int  iNewSec;

    strncpy(szSec, szOps, AS_MAX_WORD - 1);
    szSec[AS_MAX_WORD - 1] = '\0';
    as_upper(szSec);

    if      (strcmp(szSec, "TEXT") == 0 || strcmp(szSec, "CODE") == 0)
        iNewSec = AS_SEC_TEXT;
    else if (strcmp(szSec, "DATA") == 0)
        iNewSec = AS_SEC_DATA;
    else if (strcmp(szSec, "BSS")  == 0)
        iNewSec = AS_SEC_BSS;
    else
        return ST_ERROR; /* unknown section name */

    /* Save current section PC, restore new section PC */
    ptS->auPCPerSec[ptS->iCurSec] = ptS->uiPC;
    ptS->iCurSec = iNewSec;
    ptS->uiPC    = ptS->auPCPerSec[iNewSec];
    return ST_NO_ERROR;
}

/*
 * Parse and emit one DC directive.
 * szOps is comma-separated: "$1234, label, 'A', ..."
 */
static st_error_t as_do_dc(as_st_t *ptS, as_context_t *ptCtx,
                             const as_tok_t *ptTok)
{
    const char *p = ptTok->szOps;
    char        szTok[AS_MAX_WORD];
    int         iLen;
    int         i;
    st_u32_t    uiVal;
    int         iSec;
    st_u32_t    uiFixOff;
    char        cSz = ptTok->cSize;
    st_bool_t   bFirst = ST_TRUE;

    if (cSz == 0) cSz = 'W'; /* default size */

    while (*p != '\0')
    {
        p = as_skip_sp(p);
        if (*p == '\0') break;
        if (!bFirst && *p == ',') { p++; p = as_skip_sp(p); }
        bFirst = ST_FALSE;

        /* Extract one operand token (to next comma or end) */
        iLen = 0;
        while (p[iLen] && p[iLen] != ',' &&
               !(p[iLen] == ' ' && p[iLen+1] == ','))
            iLen++;
        if (iLen >= AS_MAX_WORD) iLen = AS_MAX_WORD - 1;
        memcpy(szTok, p, (size_t)iLen);
        szTok[iLen] = '\0';
        /* Trim trailing spaces */
        i = iLen - 1;
        while (i >= 0 && (szTok[i] == ' ' || szTok[i] == '\t'))
            szTok[i--] = '\0';
        p += iLen;

        if (szTok[0] == '\0') continue;

        if (!as_eval(ptS, szTok, &uiVal, &iSec))
        {
            if (ptS->iPass == 2)
                as_err(ptCtx, ptTok->iLine,
                       "undefined symbol in DC: '%s'", szTok);
            /* In pass 1 undefined symbols → still advance PC */
            uiVal = 0u;
            iSec  = AS_SEC_NONE;
        }

        switch (cSz)
        {
        case 'B':
            if (as_emit_b(ptS, ptCtx, (st_u8_t)(uiVal & 0xFFu),
                          ptTok->iLine) != ST_NO_ERROR)
                return ST_ERROR;
            break;

        case 'W':
            if (as_emit_be16(ptS, ptCtx, (st_u16_t)(uiVal & 0xFFFFu),
                              ptTok->iLine) != ST_NO_ERROR)
                return ST_ERROR;
            break;

        case 'L':
            if (ptS->iPass == 2 && iSec != AS_SEC_NONE &&
                ptCtx->bRelocatable)
            {
                /* Compute fixup offset from .text start */
                uiFixOff = (ptS->iCurSec == AS_SEC_TEXT)
                    ? ptS->tText.uiSize
                    : ptS->uiTextSzP1 + ptS->tData.uiSize;

                if (as_fix_add(ptS, ptCtx, uiFixOff,
                               ptTok->iLine) != ST_NO_ERROR)
                    return ST_ERROR;

                /* Emit combined section-relative value */
                uiVal = as_sym_combined(
                    &ptS->aSyms[as_sym_find(ptS, szTok)], ptS);
            }
            if (as_emit_be32(ptS, ptCtx, uiVal,
                              ptTok->iLine) != ST_NO_ERROR)
                return ST_ERROR;
            break;

        default:
            as_err(ptCtx, ptTok->iLine,
                   "DC requires size suffix .B/.W/.L");
            return ST_ERROR;
        }
    }
    return ST_NO_ERROR;
}

static st_error_t as_do_ds(as_st_t *ptS, as_context_t *ptCtx,
                             const as_tok_t *ptTok)
{
    st_u32_t  uiCount;
    st_u32_t  uiBytes;
    st_u32_t  i;
    int       iSec;
    char      cSz = ptTok->cSize;

    if (cSz == 0) cSz = 'W';

    if (!as_eval(ptS, ptTok->szOps, &uiCount, &iSec))
    {
        as_err(ptCtx, ptTok->iLine,
               "DS: cannot evaluate count '%s'", ptTok->szOps);
        return ST_ERROR;
    }

    uiBytes = uiCount * ((cSz == 'L') ? 4u : (cSz == 'W') ? 2u : 1u);

    for (i = 0u; i < uiBytes; i++)
    {
        if (as_emit_b(ptS, ptCtx, 0x00u, ptTok->iLine) != ST_NO_ERROR)
            return ST_ERROR;
    }
    return ST_NO_ERROR;
}

static st_error_t as_do_even(as_st_t *ptS, as_context_t *ptCtx,
                               int iLine)
{
    if (ptS->uiPC & 1u)
    {
        if (as_emit_b(ptS, ptCtx, 0x00u, iLine) != ST_NO_ERROR)
            return ST_ERROR;
    }
    return ST_NO_ERROR;
}

static st_error_t as_do_equ(as_st_t *ptS, as_context_t *ptCtx,
                              const as_tok_t *ptTok)
{
    st_u32_t uiVal;
    int      iSec;

    if (ptTok->szLabel[0] == '\0')
    {
        as_err(ptCtx, ptTok->iLine,
               "EQU without a label");
        return ST_ERROR;
    }
    if (!as_eval(ptS, ptTok->szOps, &uiVal, &iSec))
    {
        as_err(ptCtx, ptTok->iLine,
               "EQU: cannot evaluate '%s'", ptTok->szOps);
        return ST_ERROR;
    }
    return as_sym_define(ptS, ptCtx, ptTok->szLabel,
                          uiVal, iSec, ptTok->iLine);
}

/* Forward declaration — defined after EA/instruction block (UC30B) */
static st_error_t as_do_instruction(as_st_t *ptS, as_context_t *ptCtx,
                                     const as_tok_t *ptTok);

/* Master directive dispatcher */
static st_error_t as_directive(as_st_t *ptS, as_context_t *ptCtx,
                                 const as_tok_t *ptTok)
{
    const char *sz = ptTok->szMnem;

    if (strcmp(sz, "SECTION") == 0)
    {
        if (as_do_section(ptS, ptTok->szOps) != ST_NO_ERROR)
        {
            as_err(ptCtx, ptTok->iLine,
                   "unknown section '%s'", ptTok->szOps);
            return ST_ERROR;
        }
        return ST_NO_ERROR;
    }

    if (strcmp(sz, "DC") == 0)
        return as_do_dc(ptS, ptCtx, ptTok);

    if (strcmp(sz, "DS") == 0)
        return as_do_ds(ptS, ptCtx, ptTok);

    if (strcmp(sz, "EVEN") == 0)
        return as_do_even(ptS, ptCtx, ptTok->iLine);

    if (strcmp(sz, "EQU") == 0 || strcmp(sz, "SET") == 0)
        return as_do_equ(ptS, ptCtx, ptTok);

    if (strcmp(sz, "END") == 0 || strcmp(sz, "ORG") == 0)
        return ST_NO_ERROR; /* END handled in pass loop; ORG ignored */

    /* Not a directive — try instruction encoding (UC30B+) */
    return as_do_instruction(ptS, ptCtx, ptTok);
}

/* ================================================================== */
/* EA Parser and Instruction Encoder (UC30B)                           */
/* ================================================================== */

/* ------------------------------------------------------------------
 * 68000 effective address modes
 * ------------------------------------------------------------------ */

typedef enum as_ea_mode_e
{
    AS_EA_DN = 0,    /* Dn                 000 rrr */
    AS_EA_AN,        /* An                 001 rrr */
    AS_EA_AN_IND,    /* (An)               010 rrr */
    AS_EA_AN_POST,   /* (An)+              011 rrr */
    AS_EA_AN_PRE,    /* -(An)              100 rrr */
    AS_EA_AN_DISP,   /* d16(An)            101 rrr */
    AS_EA_AN_IDX,    /* d8(An,Xn)          110 rrr */
    AS_EA_ABS_W,     /* abs.W              111 000 */
    AS_EA_ABS_L,     /* abs.L              111 001 */
    AS_EA_PC_DISP,   /* d16(PC)            111 010 */
    AS_EA_PC_IDX,    /* d8(PC,Xn)          111 011 */
    AS_EA_IMM,       /* #imm               111 100 */
    AS_EA_INVALID
} as_ea_mode_t;

typedef struct as_ea_s
{
    as_ea_mode_t eMode;
    int          iReg;        /* base register 0-7         */
    st_i32_t     iDisp;       /* displacement / imm / addr */
    int          iIdxReg;     /* index register 0-7        */
    st_bool_t    bIdxIsAn;    /* ST_TRUE = An, else Dn     */
    st_bool_t    bIdxIsLong;  /* ST_TRUE = .L, else .W     */
} as_ea_t;

/* ------------------------------------------------------------------ */
/* Register parsing helpers                                             */
/* ------------------------------------------------------------------ */

static int as_parse_dn_str(const char *sz)
{
    if ((sz[0] == 'D') && sz[1] >= '0' && sz[1] <= '7' && sz[2] == '\0')
        return sz[1] - '0';
    return -1;
}

static int as_parse_an_str(const char *sz)
{
    if ((sz[0] == 'A') && sz[1] >= '0' && sz[1] <= '7' && sz[2] == '\0')
        return sz[1] - '0';
    if (strcmp(sz, "SP") == 0)
        return 7;
    return -1;
}

/* ------------------------------------------------------------------ */
/* Operand splitter — split "srcEA,dstEA" respecting parens            */
/* ------------------------------------------------------------------ */

static void as_split_ops(const char *szOps,
                          char *szA, int iSzA,
                          char *szB, int iSzB)
{
    const char *p    = szOps;
    int         i    = 0;
    int         iDepth = 0;
    int         n;

    while (*p && i < iSzA - 1)
    {
        if      (*p == '(') iDepth++;
        else if (*p == ')') iDepth--;
        else if (*p == ',' && iDepth == 0) { p++; break; }
        szA[i++] = *p++;
    }
    szA[i] = '\0';
    while (i > 0 && (szA[i-1]==' '||szA[i-1]=='\t')) szA[--i] = '\0';

    while (*p == ' ' || *p == '\t') p++;
    n = (int)strlen(p);
    while (n > 0 && (p[n-1]==' '||p[n-1]=='\t')) n--;
    if (n >= iSzB) n = iSzB - 1;
    memcpy(szB, p, (size_t)n);
    szB[n] = '\0';
}

/* ------------------------------------------------------------------ */
/* EA parser                                                            */
/* ------------------------------------------------------------------ */

/*
 * Parse the content between parens (already in uppercase).
 * szContent = "AN" | "PC" | "AN,XN.sz" | "PC,XN.sz"
 * On success sets *piBase (An reg or -1 for PC), *pbIsPC,
 * *piIdx, *pbIdxIsAn, *pbIdxIsLong.
 * Returns ST_FALSE on parse error.
 */
static st_bool_t as_parse_inner(const char *szContent,
                                  int *piBase, st_bool_t *pbIsPC,
                                  int *piIdx,  st_bool_t *pbIdxIsAn,
                                  st_bool_t *pbIdxIsLong)
{
    const char *pComma;
    char        szBase[16];
    char        szIdx[16];
    int         n;
    int         dn, an;

    *piIdx       = -1;
    *pbIdxIsAn   = ST_FALSE;
    *pbIdxIsLong = ST_FALSE;
    *pbIsPC      = ST_FALSE;

    pComma = strchr(szContent, ',');
    if (pComma == NULL)
    {
        if (strcmp(szContent, "PC") == 0)
        { *pbIsPC = ST_TRUE; *piBase = -1; return ST_TRUE; }
        an = as_parse_an_str(szContent);
        if (an < 0) return ST_FALSE;
        *piBase = an;
        return ST_TRUE;
    }

    /* Base register */
    n = (int)(pComma - szContent);
    if (n <= 0 || n >= 16) return ST_FALSE;
    memcpy(szBase, szContent, (size_t)n); szBase[n] = '\0';

    if (strcmp(szBase, "PC") == 0)
        *pbIsPC = ST_TRUE;
    else
    {
        an = as_parse_an_str(szBase);
        if (an < 0) return ST_FALSE;
        *piBase = an;
    }

    /* Index register: "DN" or "AN" with optional ".W"/".L" */
    pComma++;
    while (*pComma == ' ') pComma++;
    strncpy(szIdx, pComma, 15); szIdx[15] = '\0';
    n = (int)strlen(szIdx);
    if (n >= 2 && szIdx[n-2] == '.')
    {
        *pbIdxIsLong = (szIdx[n-1] == 'L');
        szIdx[n-2] = '\0';
    }

    dn = as_parse_dn_str(szIdx);
    if (dn >= 0) { *piIdx = dn; *pbIdxIsAn = ST_FALSE; return ST_TRUE; }
    an = as_parse_an_str(szIdx);
    if (an >= 0) { *piIdx = an; *pbIdxIsAn = ST_TRUE;  return ST_TRUE; }
    return ST_FALSE;
}

/*
 * as_parse_ea() - Parse one EA operand string into as_ea_t.
 *
 * szRaw may contain mixed case; it is uppercased internally.
 * Forward-reference immediates yield iDisp=0 (pass 1 tolerance).
 * Returns ST_TRUE on success, ST_FALSE on parse error.
 */
static st_bool_t as_parse_ea(const char    *szRaw,
                               as_ea_t       *ptEA,
                               const as_st_t *ptS)
{
    char        szOp[AS_MAX_WORD];
    char        szInner[AS_MAX_WORD];
    char        szExpr[AS_MAX_WORD];
    const char *p;
    const char *q;
    int         n;
    int         iDepth;
    int         dn, an, iBase;
    int         iIdx;
    st_bool_t   bIsPC, bIdxIsAn, bIdxIsLong;
    st_u32_t    uiVal;
    int         iSec;
    char        cSz;

    memset(ptEA, 0, sizeof(*ptEA));
    ptEA->eMode   = AS_EA_INVALID;
    ptEA->iIdxReg = -1;
    iBase         = 0;

    p = as_skip_sp(szRaw);
    n = 0;
    while (*p && n < AS_MAX_WORD - 1)
        szOp[n++] = (char)toupper((unsigned char)*p++);
    while (n > 0 && (szOp[n-1]==' '||szOp[n-1]=='\t')) n--;
    szOp[n] = '\0';
    if (szOp[0] == '\0') return ST_FALSE;

    /* ---- Immediate: #expr ---- */
    if (szOp[0] == '#')
    {
        if (!as_eval(ptS, szOp + 1, &uiVal, &iSec))
            uiVal = 0u;
        ptEA->eMode = AS_EA_IMM;
        ptEA->iDisp = (st_i32_t)uiVal;
        return ST_TRUE;
    }

    /* ---- Pre-decrement: -(An) ---- */
    if (szOp[0] == '-' && szOp[1] == '(')
    {
        p = szOp + 2;
        q = strchr(p, ')');
        if (!q) return ST_FALSE;
        n = (int)(q - p);
        if (n <= 0 || n >= 16) return ST_FALSE;
        memcpy(szInner, p, (size_t)n); szInner[n] = '\0';
        an = as_parse_an_str(szInner);
        if (an < 0) return ST_FALSE;
        ptEA->eMode = AS_EA_AN_PRE;
        ptEA->iReg  = an;
        return ST_TRUE;
    }

    /* ---- Plain register: Dn / An / SP ---- */
    dn = as_parse_dn_str(szOp);
    if (dn >= 0) { ptEA->eMode = AS_EA_DN; ptEA->iReg = dn; return ST_TRUE; }
    an = as_parse_an_str(szOp);
    if (an >= 0) { ptEA->eMode = AS_EA_AN; ptEA->iReg = an; return ST_TRUE; }

    /* ---- Starts with '(': (An) / (An)+ / (An,Xn.sz) ---- */
    if (szOp[0] == '(')
    {
        iDepth = 0; q = szOp;
        while (*q)
        {
            if (*q == '(') iDepth++;
            else if (*q == ')') { iDepth--; if (!iDepth) break; }
            q++;
        }
        if (*q != ')') return ST_FALSE;
        n = (int)(q - szOp - 1);
        if (n <= 0 || n >= AS_MAX_WORD) return ST_FALSE;
        memcpy(szInner, szOp + 1, (size_t)n); szInner[n] = '\0';

        if (!as_parse_inner(szInner, &iBase, &bIsPC, &iIdx,
                             &bIdxIsAn, &bIdxIsLong))
            return ST_FALSE;

        if (iIdx >= 0)
        {
            ptEA->iIdxReg    = iIdx;
            ptEA->bIdxIsAn   = bIdxIsAn;
            ptEA->bIdxIsLong = bIdxIsLong;
            ptEA->iDisp      = 0;
            ptEA->eMode      = bIsPC ? AS_EA_PC_IDX : AS_EA_AN_IDX;
            ptEA->iReg       = bIsPC ? 0 : iBase;
        }
        else if (bIsPC)
        {
            ptEA->eMode = AS_EA_PC_DISP;
            ptEA->iDisp = 0;
        }
        else
        {
            ptEA->iReg  = iBase;
            ptEA->eMode = (*(q+1) == '+') ? AS_EA_AN_POST : AS_EA_AN_IND;
        }
        return ST_TRUE;
    }

    /* ---- disp(An) / disp(An,Xn) / absolute ---- */
    {
        const char *pParen = strchr(szOp, '(');
        if (pParen != NULL)
        {
            /* Extract displacement string */
            n = (int)(pParen - szOp);
            if (n >= AS_MAX_WORD) return ST_FALSE;
            memcpy(szExpr, szOp, (size_t)n); szExpr[n] = '\0';

            if (szExpr[0] == '\0')
                ptEA->iDisp = 0;
            else
            {
                if (!as_eval(ptS, szExpr, &uiVal, &iSec)) uiVal = 0u;
                ptEA->iDisp = (st_i32_t)uiVal;
            }

            /* Find matching ')' */
            iDepth = 0; q = pParen;
            while (*q)
            {
                if (*q == '(') iDepth++;
                else if (*q == ')') { iDepth--; if (!iDepth) break; }
                q++;
            }
            if (*q != ')') return ST_FALSE;
            n = (int)(q - pParen - 1);
            if (n <= 0 || n >= AS_MAX_WORD) return ST_FALSE;
            memcpy(szInner, pParen + 1, (size_t)n); szInner[n] = '\0';

            if (!as_parse_inner(szInner, &iBase, &bIsPC, &iIdx,
                                 &bIdxIsAn, &bIdxIsLong))
                return ST_FALSE;

            if (iIdx >= 0)
            {
                ptEA->iIdxReg    = iIdx;
                ptEA->bIdxIsAn   = bIdxIsAn;
                ptEA->bIdxIsLong = bIdxIsLong;
                ptEA->eMode      = bIsPC ? AS_EA_PC_IDX : AS_EA_AN_IDX;
                ptEA->iReg       = bIsPC ? 0 : iBase;
            }
            else if (bIsPC)
            {
                ptEA->eMode = AS_EA_PC_DISP;
            }
            else
            {
                ptEA->iReg  = iBase;
                ptEA->eMode = AS_EA_AN_DISP;
            }
            return ST_TRUE;
        }

        /* ---- Absolute address: $val.W / $val.L / $val / label ---- */
        {
            st_bool_t bForceW = ST_FALSE;
            st_bool_t bForceL = ST_FALSE;

            strncpy(szExpr, szOp, AS_MAX_WORD - 1);
            szExpr[AS_MAX_WORD - 1] = '\0';
            n = (int)strlen(szExpr);
            if (n >= 2 && szExpr[n-2] == '.')
            {
                cSz = szExpr[n-1];
                bForceW = (cSz == 'W');
                bForceL = (cSz == 'L');
                szExpr[n-2] = '\0';
            }

            if (!as_eval(ptS, szExpr, &uiVal, &iSec))
            {
                ptEA->eMode = bForceW ? AS_EA_ABS_W : AS_EA_ABS_L;
                ptEA->iDisp = 0;
                return ST_TRUE;
            }

            ptEA->iDisp = (st_i32_t)uiVal;
            if      (bForceW) ptEA->eMode = AS_EA_ABS_W;
            else if (bForceL) ptEA->eMode = AS_EA_ABS_L;
            else
            {
                st_i32_t iS = (st_i32_t)uiVal;
                ptEA->eMode = (iS >= -32768 && iS <= 32767)
                              ? AS_EA_ABS_W : AS_EA_ABS_L;
            }
            return ST_TRUE;
        }
    }
}

/* ------------------------------------------------------------------ */
/* EA mode+register 6-bit word                                          */
/* ------------------------------------------------------------------ */

static int as_ea_modeword(const as_ea_t *pt)
{
    switch (pt->eMode)
    {
    case AS_EA_DN:      return (0 << 3) | pt->iReg;
    case AS_EA_AN:      return (1 << 3) | pt->iReg;
    case AS_EA_AN_IND:  return (2 << 3) | pt->iReg;
    case AS_EA_AN_POST: return (3 << 3) | pt->iReg;
    case AS_EA_AN_PRE:  return (4 << 3) | pt->iReg;
    case AS_EA_AN_DISP: return (5 << 3) | pt->iReg;
    case AS_EA_AN_IDX:  return (6 << 3) | pt->iReg;
    case AS_EA_ABS_W:   return (7 << 3) | 0;
    case AS_EA_ABS_L:   return (7 << 3) | 1;
    case AS_EA_PC_DISP: return (7 << 3) | 2;
    case AS_EA_PC_IDX:  return (7 << 3) | 3;
    case AS_EA_IMM:     return (7 << 3) | 4;
    default:            return -1;
    }
}

/* ------------------------------------------------------------------ */
/* EA extension word emitter                                            */
/* ------------------------------------------------------------------ */

static st_error_t as_ea_emit(const as_ea_t *pt, char cSz,
                               as_st_t *ptS, as_context_t *ptCtx,
                               int iLine)
{
    st_u16_t uiBriefExt;

    switch (pt->eMode)
    {
    case AS_EA_DN:
    case AS_EA_AN:
    case AS_EA_AN_IND:
    case AS_EA_AN_POST:
    case AS_EA_AN_PRE:
        return ST_NO_ERROR; /* no extension words */

    case AS_EA_AN_DISP:
        return as_emit_be16(ptS, ptCtx,
                            (st_u16_t)(st_i16_t)pt->iDisp, iLine);

    case AS_EA_AN_IDX:
    case AS_EA_PC_IDX:
        /* Brief extension: D/A | reg[2:0] | W/L | scale=00 | 0 | d8 */
        uiBriefExt = (st_u16_t)(
            (pt->bIdxIsAn   ? 0x8000u : 0u) |
            ((st_u16_t)pt->iIdxReg << 12)   |
            (pt->bIdxIsLong ? 0x0800u : 0u) |
            ((st_u16_t)((st_u8_t)pt->iDisp))
        );
        return as_emit_be16(ptS, ptCtx, uiBriefExt, iLine);

    case AS_EA_ABS_W:
        return as_emit_be16(ptS, ptCtx,
                            (st_u16_t)(st_i16_t)pt->iDisp, iLine);

    case AS_EA_ABS_L:
        return as_emit_be32(ptS, ptCtx, (st_u32_t)pt->iDisp, iLine);

    case AS_EA_PC_DISP:
        return as_emit_be16(ptS, ptCtx,
                            (st_u16_t)(st_i16_t)pt->iDisp, iLine);

    case AS_EA_IMM:
        switch (cSz)
        {
        case 'B':
            /* .B immediate: full word, upper byte = 0 */
            return as_emit_be16(ptS, ptCtx,
                                (st_u16_t)((st_u8_t)pt->iDisp), iLine);
        case 'L':
            return as_emit_be32(ptS, ptCtx,
                                (st_u32_t)pt->iDisp, iLine);
        default: /* 'W' */
            return as_emit_be16(ptS, ptCtx,
                                (st_u16_t)(pt->iDisp & 0xFFFF), iLine);
        }

    default:
        as_err(ptCtx, iLine, "EA emit: invalid mode %d", (int)pt->eMode);
        return ST_ERROR;
    }
}

/* ------------------------------------------------------------------ */
/* Instruction encoders                                                 */
/* ------------------------------------------------------------------ */

/* MOVE.B/W/L and MOVEA (destination is An) */
static st_error_t as_encode_move(as_st_t *ptS, as_context_t *ptCtx,
                                   const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz     = ptTok->cSize;
    int      iLine   = ptTok->iLine;
    int      iSrcMR;
    int      iDstMR;
    st_u16_t uiBase;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "MOVE: bad source EA '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "MOVE: bad dest EA '%s'", szDst); return ST_ERROR; }

    iSrcMR = as_ea_modeword(&tSrc);
    iDstMR = as_ea_modeword(&tDst);
    if (iSrcMR < 0 || iDstMR < 0)
    { as_err(ptCtx, iLine, "MOVE: invalid EA"); return ST_ERROR; }

    /* MOVE size encoding: B=01, L=10, W=11 */
    switch (cSz)
    {
    case 'B': uiBase = 0x1000u; break;
    case 'L': uiBase = 0x2000u; break;
    default:  uiBase = 0x3000u; break;
    }

    /* Destination EA in MOVE: bits[11:9]=dst_reg, bits[8:6]=dst_mode
     * (reversed compared to source EA encoding)                       */
    uiOp = (st_u16_t)(
        uiBase                          |
        (((st_u16_t)iDstMR & 7u) << 9) |  /* dst_reg  */
        ((((st_u16_t)iDstMR >> 3) & 7u) << 6) | /* dst_mode */
        ((st_u16_t)(iSrcMR & 0x3F))
    );

    if (as_emit_be16(ptS, ptCtx, uiOp, iLine)            != ST_NO_ERROR)
        return ST_ERROR;
    if (as_ea_emit(&tSrc, cSz, ptS, ptCtx, iLine)        != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tDst, cSz, ptS, ptCtx, iLine);
}

/* MOVEQ #data8,Dn — 0111 rrr 0 dddddddd */
static st_error_t as_encode_moveq(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;
    st_u16_t uiOp;

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS) || tSrc.eMode != AS_EA_IMM)
    { as_err(ptCtx, iLine, "MOVEQ: source must be #data8"); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "MOVEQ: destination must be Dn"); return ST_ERROR; }

    uiOp = (st_u16_t)(
        0x7000u                          |
        ((st_u16_t)tDst.iReg << 9)      |
        ((st_u16_t)((st_u8_t)tSrc.iDisp))
    );
    return as_emit_be16(ptS, ptCtx, uiOp, iLine);
}

/* LEA src,An — 0100 AAA 111 MMMRRR */
static st_error_t as_encode_lea(as_st_t *ptS, as_context_t *ptCtx,
                                  const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    int      iSrcMR;
    int      iLine = ptTok->iLine;
    st_u16_t uiOp;

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "LEA: bad source EA '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_AN)
    { as_err(ptCtx, iLine, "LEA: destination must be An"); return ST_ERROR; }

    iSrcMR = as_ea_modeword(&tSrc);
    if (iSrcMR < 0)
    { as_err(ptCtx, iLine, "LEA: invalid source EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(
        0x4000u                          |
        ((st_u16_t)tDst.iReg << 9)      |
        (7u << 6)                        |
        ((st_u16_t)(iSrcMR & 0x3F))
    );
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tSrc, 'L', ptS, ptCtx, iLine);
}

/* CLR.B/W/L dst — 0100 0010 SS MMMRRR */
static st_error_t as_encode_clr(as_st_t *ptS, as_context_t *ptCtx,
                                  const as_tok_t *ptTok)
{
    as_ea_t  tDst;
    char     cSz   = ptTok->cSize;
    int      iLine = ptTok->iLine;
    int      iDstMR;
    int      iSzCode;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';

    if (!as_parse_ea(ptTok->szOps, &tDst, ptS))
    { as_err(ptCtx, iLine, "CLR: bad EA '%s'", ptTok->szOps); return ST_ERROR; }

    iDstMR = as_ea_modeword(&tDst);
    if (iDstMR < 0)
    { as_err(ptCtx, iLine, "CLR: invalid EA"); return ST_ERROR; }

    switch (cSz)
    {
    case 'B': iSzCode = 0; break;
    case 'W': iSzCode = 1; break;
    case 'L': iSzCode = 2; break;
    default:  iSzCode = 1; break;
    }

    uiOp = (st_u16_t)(
        0x4200u                          |
        ((st_u16_t)iSzCode << 6)        |
        ((st_u16_t)(iDstMR & 0x3F))
    );
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tDst, cSz, ptS, ptCtx, iLine);
}

/* SWAP Dn — 0100 1000 0100 0 rrr */
static st_error_t as_encode_swap(as_st_t *ptS, as_context_t *ptCtx,
                                   const as_tok_t *ptTok)
{
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;

    if (!as_parse_ea(ptTok->szOps, &tDst, ptS) || tDst.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "SWAP: operand must be Dn"); return ST_ERROR; }

    return as_emit_be16(ptS, ptCtx,
                         (st_u16_t)(0x4840u | (st_u16_t)tDst.iReg), iLine);
}

/* ------------------------------------------------------------------ */
/* Master instruction dispatcher (UC30B)                               */
/* ------------------------------------------------------------------ */

static st_error_t as_do_instruction(as_st_t *ptS, as_context_t *ptCtx,
                                     const as_tok_t *ptTok)
{
    const char *sz = ptTok->szMnem;

    if (strcmp(sz, "MOVE") == 0 || strcmp(sz, "MOVEA") == 0)
        return as_encode_move(ptS, ptCtx, ptTok);

    if (strcmp(sz, "MOVEQ") == 0)
        return as_encode_moveq(ptS, ptCtx, ptTok);

    if (strcmp(sz, "LEA") == 0)
        return as_encode_lea(ptS, ptCtx, ptTok);

    if (strcmp(sz, "CLR") == 0)
        return as_encode_clr(ptS, ptCtx, ptTok);

    if (strcmp(sz, "SWAP") == 0)
        return as_encode_swap(ptS, ptCtx, ptTok);

    as_err(ptCtx, ptTok->iLine,
           "unknown mnemonic '%s' (UC30C+ will add more instructions)", sz);
    return ST_ERROR;
}

/* ------------------------------------------------------------------ */
/* Two-pass engine                                                       */
/* ------------------------------------------------------------------ */

static st_error_t as_do_pass(as_context_t *ptCtx, as_st_t *ptS,
                               const char *szSrc, int iPass)
{
    char       szLine[AS_MAX_LINE];
    as_tok_t   tTok;
    const char *p    = szSrc;
    const char *pEnd;
    int        iLine = 0;
    size_t     uiLen;

    ptS->iPass   = iPass;
    ptS->iCurSec = AS_SEC_TEXT;

    if (iPass == 1)
    {
        ptS->uiPC = 0u;
        memset(ptS->auPCPerSec, 0, sizeof(ptS->auPCPerSec));
    }
    else
    {
        /* Pass 2: reset buffers, PC mirrors buffer size */
        ptS->tText.uiSize = 0u;
        ptS->tData.uiSize = 0u;
        ptS->uiPC         = 0u;
        memset(ptS->auPCPerSec, 0, sizeof(ptS->auPCPerSec));
        ptS->iFixCount    = 0;
    }

    while (*p != '\0')
    {
        /* Extract one line */
        pEnd = p;
        while (*pEnd && *pEnd != '\n') pEnd++;
        uiLen = (size_t)(pEnd - p);
        if (uiLen >= AS_MAX_LINE) uiLen = AS_MAX_LINE - 1u;
        memcpy(szLine, p, uiLen);
        szLine[uiLen] = '\0';
        /* Strip trailing CR so CRLF sources work on all platforms */
        if (uiLen > 0 && szLine[uiLen - 1] == '\r')
            szLine[--uiLen] = '\0';
        iLine++;
        p = (*pEnd == '\n') ? pEnd + 1 : pEnd;

        as_lex_line(szLine, &tTok, iLine);
        if (tTok.szMnem[0] == '\0' && tTok.szLabel[0] == '\0')
            continue;

        /* Define label (before directive advances PC) */
        if (tTok.szLabel[0] != '\0' &&
            strcmp(tTok.szMnem, "EQU") != 0 &&
            strcmp(tTok.szMnem, "SET") != 0)
        {
            if (iPass == 1)
            {
                if (as_sym_define(ptS, ptCtx, tTok.szLabel,
                                   ptS->uiPC, ptS->iCurSec,
                                   iLine) != ST_NO_ERROR)
                    return ST_ERROR;
            }
        }

        /* Process directive (skip if mnemonic empty / label-only) */
        if (tTok.szMnem[0] != '\0')
        {
            if (strcmp(tTok.szMnem, "END") == 0)
                break;
            as_directive(ptS, ptCtx, &tTok);
            if (ptCtx->uiErrorCount >= AS_MAX_ERRORS)
                return ST_ERROR;
        }
    }

    /* Lock sizes at end of pass 1 */
    if (iPass == 1)
    {
        ptS->auPCPerSec[ptS->iCurSec] = ptS->uiPC;
        ptS->uiTextSzP1 = ptS->auPCPerSec[AS_SEC_TEXT];
        ptS->uiDataSzP1 = ptS->auPCPerSec[AS_SEC_DATA];
        ptS->uiBssSzP1  = ptS->auPCPerSec[AS_SEC_BSS];
    }

    return (ptCtx->uiErrorCount > 0) ? ST_ERROR : ST_NO_ERROR;
}

/* ------------------------------------------------------------------ */
/* PRG output                                                           */
/* ------------------------------------------------------------------ */

static st_error_t as_write_fixups(as_st_t *ptS, as_buf_t *ptOut)
{
    int      i;
    st_u32_t uiPrev;
    st_u32_t uiDelta;
    st_u32_t uiRem;

    if (ptS->iFixCount == 0)
    {
        /* No fixups — write 4-byte zero */
        return as_buf_emit_be32(ptOut, 0u);
    }

    qsort(ptS->ptFix, (size_t)ptS->iFixCount, sizeof(as_fix_t),
          as_fix_cmp);

    /* First fixup as absolute 4-byte offset */
    if (as_buf_emit_be32(ptOut, ptS->ptFix[0].uiOffset) != ST_NO_ERROR)
        return ST_ERROR;

    uiPrev = ptS->ptFix[0].uiOffset;

    for (i = 1; i < ptS->iFixCount; i++)
    {
        uiDelta = ptS->ptFix[i].uiOffset - uiPrev;
        uiPrev  = ptS->ptFix[i].uiOffset;

        /* Encode delta: 0x01 bytes advance 254 each */
        uiRem = uiDelta;
        while (uiRem >= 254u)
        {
            if (as_buf_emit_b(ptOut, 0x01u) != ST_NO_ERROR)
                return ST_ERROR;
            uiRem -= 254u;
        }
        if (uiRem > 0u)
        {
            if (as_buf_emit_b(ptOut, (st_u8_t)uiRem) != ST_NO_ERROR)
                return ST_ERROR;
        }
    }

    return as_buf_emit_b(ptOut, 0x00u); /* terminator */
}

static st_error_t as_write_prg(as_context_t *ptCtx, as_st_t *ptS)
{
    as_buf_t  tOut;
    FILE     *fp;
    size_t    uiWritten;
    st_u16_t  uiAbsFlag;

    memset(&tOut, 0, sizeof(tOut));

    uiAbsFlag = ptCtx->bRelocatable ? 0u : 1u;

    /* 28-byte header (all big-endian) */
    if (as_buf_emit_be16(&tOut, 0x601Au) != ST_NO_ERROR) goto oom;
    if (as_buf_emit_be32(&tOut, ptS->uiTextSzP1)  != ST_NO_ERROR) goto oom;
    if (as_buf_emit_be32(&tOut, ptS->uiDataSzP1)  != ST_NO_ERROR) goto oom;
    if (as_buf_emit_be32(&tOut, ptS->uiBssSzP1)   != ST_NO_ERROR) goto oom;
    if (as_buf_emit_be32(&tOut, 0u) != ST_NO_ERROR) goto oom; /* sym_sz  */
    if (as_buf_emit_be32(&tOut, 0u) != ST_NO_ERROR) goto oom; /* reserved*/
    if (as_buf_emit_be32(&tOut, 0u) != ST_NO_ERROR) goto oom; /* flags   */
    if (as_buf_emit_be16(&tOut, uiAbsFlag) != ST_NO_ERROR)    /* abs_flag*/
        goto oom;

    /* .text section */
    if (ptS->tText.uiSize > 0u)
    {
        if (as_buf_need(&tOut, ptS->tText.uiSize) != ST_NO_ERROR)
            goto oom;
        memcpy(tOut.pData + tOut.uiSize,
               ptS->tText.pData, ptS->tText.uiSize);
        tOut.uiSize += ptS->tText.uiSize;
    }

    /* .data section */
    if (ptS->tData.uiSize > 0u)
    {
        if (as_buf_need(&tOut, ptS->tData.uiSize) != ST_NO_ERROR)
            goto oom;
        memcpy(tOut.pData + tOut.uiSize,
               ptS->tData.pData, ptS->tData.uiSize);
        tOut.uiSize += ptS->tData.uiSize;
    }

    /* Fixup table (only for relocatable) */
    if (ptCtx->bRelocatable)
    {
        if (as_write_fixups(ptS, &tOut) != ST_NO_ERROR)
            goto oom;
    }

    /* Write to file */
    fp = fopen(ptCtx->szOutputPath, "wb");
    if (fp == NULL)
    {
        as_err(ptCtx, 0, "cannot create output file '%s'",
               ptCtx->szOutputPath);
        as_buf_free(&tOut);
        return ST_ERROR;
    }
    uiWritten = fwrite(tOut.pData, 1u, tOut.uiSize, fp);
    fclose(fp);

    if (uiWritten != tOut.uiSize)
    {
        as_err(ptCtx, 0, "write error on '%s'", ptCtx->szOutputPath);
        as_buf_free(&tOut);
        return ST_ERROR;
    }

    /* Hand binary to context */
    ptCtx->pBinary    = tOut.pData;
    ptCtx->uiBinaryLen = tOut.uiSize;
    LOG_INFO("assembled '%s' -> %u bytes",
             ptCtx->szOutputPath, (unsigned)tOut.uiSize);
    return ST_NO_ERROR;

oom:
    as_err(ptCtx, 0, "out of memory building PRG output");
    as_buf_free(&tOut);
    return ST_ERROR;
}

/* ------------------------------------------------------------------ */
/* Source file reader                                                    */
/* ------------------------------------------------------------------ */

static st_error_t as_read_source(const char *szPath,
                                   char **ppSrc)
{
    FILE   *fp;
    long    lSize;
    char   *pBuf;
    size_t  uiRead;

    fp = fopen(szPath, "r");
    if (fp == NULL)
    {
        LOG_ERROR("cannot open source '%s'", szPath);
        return ST_ERROR;
    }

    fseek(fp, 0, SEEK_END);
    lSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (lSize < 0) { fclose(fp); return ST_ERROR; }

    pBuf = (char *)malloc((size_t)lSize + 1u);
    if (pBuf == NULL)
    {
        LOG_ERROR("malloc failed for source file '%s'", szPath);
        fclose(fp);
        return ST_ERROR;
    }

    uiRead = fread(pBuf, 1u, (size_t)lSize, fp);
    fclose(fp);
    pBuf[uiRead] = '\0';

    *ppSrc = pBuf;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

st_error_t as_init(as_context_t *ptCtx, const char *szSourcePath,
                    const char *szOutputPath, st_bool_t bRelocatable)
{
    LOG_TRACE("src='%s' out='%s'",
              szSourcePath ? szSourcePath : "(null)",
              szOutputPath ? szOutputPath : "(null)");

    if (ptCtx == NULL || szSourcePath == NULL || szOutputPath == NULL)
    {
        LOG_ERROR("NULL parameter");
        return ST_ERROR;
    }

    memset(ptCtx, 0, sizeof(as_context_t));
    strncpy(ptCtx->szSourcePath, szSourcePath, ST_MAX_PATH - 1);
    strncpy(ptCtx->szOutputPath, szOutputPath, ST_MAX_PATH - 1);
    ptCtx->bRelocatable = bRelocatable;
    return ST_NO_ERROR;
}

st_error_t as_assemble(as_context_t *ptCtx)
{
    as_st_t  tS;
    char    *pSrc = NULL;
    st_error_t eRet;

    LOG_TRACE("ptCtx=%p src='%s'",
              (void *)ptCtx,
              ptCtx ? ptCtx->szSourcePath : "(null)");

    if (ptCtx == NULL)
    {
        LOG_ERROR("NULL ptCtx");
        return ST_ERROR;
    }

    memset(&tS, 0, sizeof(tS));
    tS.iCurSec = AS_SEC_TEXT;

    if (as_read_source(ptCtx->szSourcePath, &pSrc) != ST_NO_ERROR)
    {
        as_err(ptCtx, 0, "cannot read source '%s'",
               ptCtx->szSourcePath);
        return ST_ERROR;
    }

    /* Pass 1 — build symbol table and compute section sizes */
    eRet = as_do_pass(ptCtx, &tS, pSrc, 1);
    if (eRet != ST_NO_ERROR && ptCtx->uiErrorCount > 0)
        goto done;

    /* Pass 2 — emit binary */
    eRet = as_do_pass(ptCtx, &tS, pSrc, 2);
    if (eRet != ST_NO_ERROR)
        goto done;

    /* Write PRG file */
    eRet = as_write_prg(ptCtx, &tS);

done:
    free(pSrc);
    as_buf_free(&tS.tText);
    as_buf_free(&tS.tData);
    if (tS.ptFix != NULL) free(tS.ptFix);

    return (ptCtx->uiErrorCount > 0) ? ST_ERROR : eRet;
}

st_error_t as_shutdown(as_context_t *ptCtx)
{
    LOG_TRACE("ptCtx=%p", (void *)ptCtx);

    if (ptCtx == NULL)
    {
        LOG_ERROR("NULL ptCtx");
        return ST_ERROR;
    }

    if (ptCtx->pBinary  != NULL) { free(ptCtx->pBinary);  }
    if (ptCtx->ptErrors != NULL) { free(ptCtx->ptErrors); }
    memset(ptCtx, 0, sizeof(as_context_t));
    return ST_NO_ERROR;
}
