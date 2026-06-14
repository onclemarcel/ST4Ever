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

    /* Location counter: * with optional +/- offset (DEVPAC3 syntax) */
    if (*p == '*')
    {
        uiBase  = ptS->uiPC;
        p++;
        p = as_skip_sp(p);
        cOp = *p;
        if (cOp == '+' || cOp == '-')
        {
            p++;
            p = as_skip_sp(p);
            lOff = strtol(p, NULL, 0);
            if (cOp == '+')
                uiBase += (st_u32_t)(st_i32_t)lOff;
            else
                uiBase -= (st_u32_t)(st_i32_t)lOff;
        }
        *puiVal = uiBase;
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
    {
        /* Brief extension: D/A | reg[2:0] | W/L | scale=00 | 0 | d8
         * For PC_IDX: d8 is relative to the extension word address     */
        st_i32_t iD8 = (pt->eMode == AS_EA_PC_IDX)
                        ? (pt->iDisp - (st_i32_t)ptS->uiPC)
                        : pt->iDisp;
        uiBriefExt = (st_u16_t)(
            (pt->bIdxIsAn   ? 0x8000u : 0u) |
            ((st_u16_t)pt->iIdxReg << 12)   |
            (pt->bIdxIsLong ? 0x0800u : 0u) |
            ((st_u16_t)((st_u8_t)(st_i8_t)iD8))
        );
        return as_emit_be16(ptS, ptCtx, uiBriefExt, iLine);
    }

    case AS_EA_ABS_W:
        return as_emit_be16(ptS, ptCtx,
                            (st_u16_t)(st_i16_t)pt->iDisp, iLine);

    case AS_EA_ABS_L:
        return as_emit_be32(ptS, ptCtx, (st_u32_t)pt->iDisp, iLine);

    case AS_EA_PC_DISP:
    {
        /* Displacement is relative to the extension word address (ptS->uiPC) */
        st_i32_t iRel = pt->iDisp - (st_i32_t)ptS->uiPC;
        return as_emit_be16(ptS, ptCtx, (st_u16_t)(st_i16_t)iRel, iLine);
    }

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

    /* Special forms: MOVE <ea>,SR / MOVE SR,<ea> / MOVE <ea>,CCR
     *                MOVE USP,An / MOVE An,USP                     */
    {
        char szSU[AS_MAX_WORD];
        char szDU[AS_MAX_WORD];
        strncpy(szSU, szSrc, AS_MAX_WORD - 1); szSU[AS_MAX_WORD - 1] = '\0';
        strncpy(szDU, szDst, AS_MAX_WORD - 1); szDU[AS_MAX_WORD - 1] = '\0';
        as_upper(szSU);
        as_upper(szDU);

        if (strcmp(szSU, "USP") == 0)
        {
            /* MOVE USP,An: 0100 1110 0110 1 An */
            if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_AN)
            { as_err(ptCtx, iLine, "MOVE USP: dest must be An"); return ST_ERROR; }
            return as_emit_be16(ptS, ptCtx,
                                (st_u16_t)(0x4E68u | (st_u16_t)tDst.iReg), iLine);
        }
        if (strcmp(szDU, "USP") == 0)
        {
            /* MOVE An,USP: 0100 1110 0110 0 An */
            if (!as_parse_ea(szSrc, &tSrc, ptS) || tSrc.eMode != AS_EA_AN)
            { as_err(ptCtx, iLine, "MOVE USP: source must be An"); return ST_ERROR; }
            return as_emit_be16(ptS, ptCtx,
                                (st_u16_t)(0x4E60u | (st_u16_t)tSrc.iReg), iLine);
        }
        if (strcmp(szDU, "SR") == 0)
        {
            /* MOVE <ea>,SR: 0100 0110 11 MMMRRR */
            if (!as_parse_ea(szSrc, &tSrc, ptS))
            { as_err(ptCtx, iLine, "MOVE SR: bad source EA"); return ST_ERROR; }
            iSrcMR = as_ea_modeword(&tSrc);
            if (iSrcMR < 0)
            { as_err(ptCtx, iLine, "MOVE SR: invalid EA"); return ST_ERROR; }
            if (as_emit_be16(ptS, ptCtx,
                             (st_u16_t)(0x46C0u | (st_u16_t)(iSrcMR & 0x3F)),
                             iLine) != ST_NO_ERROR) return ST_ERROR;
            return as_ea_emit(&tSrc, 'W', ptS, ptCtx, iLine);
        }
        if (strcmp(szSU, "SR") == 0)
        {
            /* MOVE SR,<ea>: 0100 0000 11 MMMRRR */
            if (!as_parse_ea(szDst, &tDst, ptS))
            { as_err(ptCtx, iLine, "MOVE SR: bad dest EA"); return ST_ERROR; }
            iDstMR = as_ea_modeword(&tDst);
            if (iDstMR < 0)
            { as_err(ptCtx, iLine, "MOVE SR: invalid EA"); return ST_ERROR; }
            if (as_emit_be16(ptS, ptCtx,
                             (st_u16_t)(0x40C0u | (st_u16_t)(iDstMR & 0x3F)),
                             iLine) != ST_NO_ERROR) return ST_ERROR;
            return as_ea_emit(&tDst, 'W', ptS, ptCtx, iLine);
        }
        if (strcmp(szDU, "CCR") == 0)
        {
            /* MOVE <ea>,CCR: 0100 0100 11 MMMRRR */
            if (!as_parse_ea(szSrc, &tSrc, ptS))
            { as_err(ptCtx, iLine, "MOVE CCR: bad source EA"); return ST_ERROR; }
            iSrcMR = as_ea_modeword(&tSrc);
            if (iSrcMR < 0)
            { as_err(ptCtx, iLine, "MOVE CCR: invalid EA"); return ST_ERROR; }
            if (as_emit_be16(ptS, ptCtx,
                             (st_u16_t)(0x44C0u | (st_u16_t)(iSrcMR & 0x3F)),
                             iLine) != ST_NO_ERROR) return ST_ERROR;
            return as_ea_emit(&tSrc, 'W', ptS, ptCtx, iLine);
        }
    }

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
/* UC30C helpers                                                        */
/* ------------------------------------------------------------------ */

/* Return 0/1/2 for B/W/L; -1 on unknown */
static int as_sz_code(char cSz)
{
    if (cSz == 'B') return 0;
    if (cSz == 'W') return 1;
    if (cSz == 'L') return 2;
    return -1;
}

/* ------------------------------------------------------------------ */
/* ALU binary: ADD/SUB/AND/OR — 1ooo rrr d ss MMMRRR                  */
/* d=0: EA->Dn  d=1: Dn->EA  (one operand must be Dn)                */
/* ------------------------------------------------------------------ */

#define AS_BASE_OR  0x8000u
#define AS_BASE_SUB 0x9000u
#define AS_BASE_AND 0xC000u
#define AS_BASE_ADD 0xD000u

/* uiImmBase: if non-zero and source is #imm, redirect to immediate ALU form
 * (e.g. ADD #imm,Dn → ADDI; OR 0xFFFFu to signal "no redirect") */
static st_error_t as_encode_alu_binary(as_st_t *ptS, as_context_t *ptCtx,
                                        const as_tok_t *ptTok,
                                        st_u16_t uiBase,
                                        st_u16_t uiImmBase)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz      = ptTok->cSize;
    int      iLine    = ptTok->iLine;
    int      iSzCode;
    int      iEAMR;
    int      iDn;
    int      iDir;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "ALU: invalid size"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "ALU: bad source '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "ALU: bad dest '%s'", szDst); return ST_ERROR; }

    /* DEVPAC3: source #imm → use ADDI/SUBI/ANDI/ORI encoding */
    if (uiImmBase != 0xFFFFu && tSrc.eMode == AS_EA_IMM)
    {
        iEAMR = as_ea_modeword(&tDst);
        if (iEAMR < 0)
        { as_err(ptCtx, iLine, "ALU: invalid dest EA"); return ST_ERROR; }
        uiOp = (st_u16_t)(uiImmBase |
                           ((st_u16_t)iSzCode << 6) |
                           (st_u16_t)(iEAMR & 0x3F));
        if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
            return ST_ERROR;
        if (as_ea_emit(&tSrc, cSz, ptS, ptCtx, iLine) != ST_NO_ERROR)
            return ST_ERROR;
        return as_ea_emit(&tDst, cSz, ptS, ptCtx, iLine);
    }

    if (tDst.eMode == AS_EA_DN)
    {
        /* EA -> Dn */
        iDir  = 0;
        iDn   = tDst.iReg;
        iEAMR = as_ea_modeword(&tSrc);
        if (iEAMR < 0)
        { as_err(ptCtx, iLine, "ALU: invalid source EA"); return ST_ERROR; }
        uiOp = (st_u16_t)(uiBase |
                           ((st_u16_t)iDn << 9)      |
                           ((st_u16_t)iDir << 8)      |
                           ((st_u16_t)iSzCode << 6)   |
                           (st_u16_t)(iEAMR & 0x3F));
        if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
            return ST_ERROR;
        return as_ea_emit(&tSrc, cSz, ptS, ptCtx, iLine);
    }
    else if (tSrc.eMode == AS_EA_DN)
    {
        /* Dn -> EA */
        iDir  = 1;
        iDn   = tSrc.iReg;
        iEAMR = as_ea_modeword(&tDst);
        if (iEAMR < 0)
        { as_err(ptCtx, iLine, "ALU: invalid dest EA"); return ST_ERROR; }
        uiOp = (st_u16_t)(uiBase |
                           ((st_u16_t)iDn << 9)      |
                           ((st_u16_t)iDir << 8)      |
                           ((st_u16_t)iSzCode << 6)   |
                           (st_u16_t)(iEAMR & 0x3F));
        if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
            return ST_ERROR;
        return as_ea_emit(&tDst, cSz, ptS, ptCtx, iLine);
    }

    as_err(ptCtx, iLine, "ALU: one operand must be Dn");
    return ST_ERROR;
}

/* CMP: 1011 rrr 0 ss MMMRRR — direction always EA->Dn
 * DEVPAC3: CMP #imm,Dn uses CMP opcode (not CMPI); CMPA handles An dest */
static st_error_t as_encode_cmp(as_st_t *ptS, as_context_t *ptCtx,
                                  const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz    = ptTok->cSize;
    int      iLine  = ptTok->iLine;
    int      iSzCode;
    int      iEAMR;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "CMP: invalid size"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "CMP: bad source '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "CMP: dest must be Dn"); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tSrc);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "CMP: invalid source EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(0xB000u |
                       ((st_u16_t)tDst.iReg << 9)  |
                       ((st_u16_t)iSzCode << 6)     |
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tSrc, cSz, ptS, ptCtx, iLine);
}

/* EOR: 1011 rrr 1 ss MMMRRR — direction always Dn->EA */
static st_error_t as_encode_eor(as_st_t *ptS, as_context_t *ptCtx,
                                  const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz    = ptTok->cSize;
    int      iLine  = ptTok->iLine;
    int      iSzCode;
    int      iEAMR;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "EOR: invalid size"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS) || tSrc.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "EOR: source must be Dn"); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "EOR: bad dest '%s'", szDst); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tDst);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "EOR: invalid dest EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(0xB000u |
                       ((st_u16_t)tSrc.iReg << 9)  |
                       (1u << 8)                    |
                       ((st_u16_t)iSzCode << 6)     |
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tDst, cSz, ptS, ptCtx, iLine);
}

/* Immediate ALU: 0000 xxxx ss MMMRRR + imm
 * uiOpBits: ORI=0x0000, ANDI=0x0200, SUBI=0x0400,
 *           ADDI=0x0600, EORI=0x0A00, CMPI=0x0C00
 * Special: dest=CCR → MMMRRR=0x3C forced .B, no dest extension
 *          dest=SR  → MMMRRR=0x3C forced .W, no dest extension  */
static st_error_t as_encode_imm_alu(as_st_t *ptS, as_context_t *ptCtx,
                                      const as_tok_t *ptTok, st_u16_t uiOpBits)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    char     szDstUp[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz    = ptTok->cSize;
    int      iLine  = ptTok->iLine;
    int      iSzCode;
    int      iEAMR;
    st_u16_t uiOp;
    size_t   k;

    if (cSz == 0) cSz = 'W';

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    /* Uppercase dest for CCR/SR detection */
    for (k = 0u; k < AS_MAX_WORD - 1u && szDst[k]; k++)
        szDstUp[k] = (char)toupper((unsigned char)szDst[k]);
    szDstUp[k] = '\0';

    /* CCR/SR: fixed MMMRRR=0x3C, emit opword + src imm only */
    if (strcmp(szDstUp, "CCR") == 0 || strcmp(szDstUp, "SR") == 0)
    {
        int  iSpecSz = (szDstUp[0] == 'S') ? 1 : 0; /* SR→.W(1), CCR→.B(0) */
        char cSpecSz = (szDstUp[0] == 'S') ? 'W' : 'B';

        if (!as_parse_ea(szSrc, &tSrc, ptS) || tSrc.eMode != AS_EA_IMM)
        { as_err(ptCtx, iLine, "ImmALU CCR/SR: source must be #imm"); return ST_ERROR; }

        uiOp = (st_u16_t)(uiOpBits | ((st_u16_t)iSpecSz << 6) | 0x003Cu);
        if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
            return ST_ERROR;
        return as_ea_emit(&tSrc, cSpecSz, ptS, ptCtx, iLine);
    }

    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "ImmALU: invalid size"); return ST_ERROR; }

    if (!as_parse_ea(szSrc, &tSrc, ptS) || tSrc.eMode != AS_EA_IMM)
    { as_err(ptCtx, iLine, "ImmALU: source must be #imm"); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "ImmALU: bad dest '%s'", szDst); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tDst);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "ImmALU: invalid dest EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(uiOpBits |
                       ((st_u16_t)iSzCode << 6) |
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    if (as_ea_emit(&tSrc, cSz, ptS, ptCtx, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tDst, cSz, ptS, ptCtx, iLine);
}

/* ADDQ/SUBQ: 0101 ddd Q ss MMMRRR
 * ddd: data 1-7 as-is, 8 -> 000
 * Q: 0=ADDQ 1=SUBQ                            */
static st_error_t as_encode_addq_subq(as_st_t *ptS, as_context_t *ptCtx,
                                        const as_tok_t *ptTok, int iSubQ)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz    = ptTok->cSize;
    int      iLine  = ptTok->iLine;
    int      iSzCode;
    int      iEAMR;
    int      iData;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "ADDQ/SUBQ: invalid size"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS) || tSrc.eMode != AS_EA_IMM)
    { as_err(ptCtx, iLine, "ADDQ/SUBQ: source must be #imm"); return ST_ERROR; }

    iData = tSrc.iDisp;
    if (iData < 1 || iData > 8)
    { as_err(ptCtx, iLine, "ADDQ/SUBQ: immediate must be 1-8"); return ST_ERROR; }

    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "ADDQ/SUBQ: bad dest '%s'", szDst); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tDst);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "ADDQ/SUBQ: invalid dest EA"); return ST_ERROR; }

    /* data=8 encodes as 000 in bits 11-9 */
    uiOp = (st_u16_t)(0x5000u |
                       ((st_u16_t)(iData & 7) << 9) |
                       ((st_u16_t)iSubQ << 8)        |
                       ((st_u16_t)iSzCode << 6)      |
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tDst, cSz, ptS, ptCtx, iLine);
}

/* NEG/NOT/TST: uiBase | (szCode<<6) | MMMRRR
 * NEG=0x4400 NOT=0x4600 TST=0x4A00               */
static st_error_t as_encode_unary(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok, st_u16_t uiBase)
{
    as_ea_t  tDst;
    char     cSz    = ptTok->cSize;
    int      iLine  = ptTok->iLine;
    int      iSzCode;
    int      iEAMR;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "unary: invalid size"); return ST_ERROR; }

    if (!as_parse_ea(ptTok->szOps, &tDst, ptS))
    { as_err(ptCtx, iLine, "unary: bad EA '%s'", ptTok->szOps); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tDst);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "unary: invalid EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(uiBase |
                       ((st_u16_t)iSzCode << 6) |
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tDst, cSz, ptS, ptCtx, iLine);
}

/* EXT.W Dn = 0x4880|Rn   EXT.L Dn = 0x48C0|Rn */
static st_error_t as_encode_ext(as_st_t *ptS, as_context_t *ptCtx,
                                   const as_tok_t *ptTok)
{
    as_ea_t  tDst;
    char     cSz   = ptTok->cSize;
    int      iLine = ptTok->iLine;
    st_u16_t uiOp;

    if (!as_parse_ea(ptTok->szOps, &tDst, ptS) || tDst.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "EXT: operand must be Dn"); return ST_ERROR; }

    if (cSz == 'W')
        uiOp = (st_u16_t)(0x4880u | (st_u16_t)tDst.iReg);
    else if (cSz == 'L')
        uiOp = (st_u16_t)(0x48C0u | (st_u16_t)tDst.iReg);
    else
    { as_err(ptCtx, iLine, "EXT: size must be .W or .L"); return ST_ERROR; }

    return as_emit_be16(ptS, ptCtx, uiOp, iLine);
}

/* ------------------------------------------------------------------ */
/* Branch instructions: BRA/BSR/Bcc — always long form (4 bytes)       */
/* Opword: 0x6000|(cc<<8)   Displacement: target - (opword_addr+2)    */
/* ------------------------------------------------------------------ */

static const struct { const char *sz; int iCC; } g_as_aBcc[] =
{
    { "BRA", 0x00 }, { "BSR", 0x01 },
    { "BHI", 0x02 }, { "BLS", 0x03 },
    { "BCC", 0x04 }, { "BHS", 0x04 },
    { "BCS", 0x05 }, { "BLO", 0x05 },
    { "BNE", 0x06 }, { "BEQ", 0x07 },
    { "BVC", 0x08 }, { "BVS", 0x09 },
    { "BPL", 0x0A }, { "BMI", 0x0B },
    { "BGE", 0x0C }, { "BLT", 0x0D },
    { "BGT", 0x0E }, { "BLE", 0x0F },
    { NULL,   -1  }
};

static int as_bcc_code(const char *szMnem)
{
    int i;
    for (i = 0; g_as_aBcc[i].sz != NULL; i++)
    {
        if (strcmp(szMnem, g_as_aBcc[i].sz) == 0)
            return g_as_aBcc[i].iCC;
    }
    return -1;
}

static st_error_t as_encode_branch(as_st_t *ptS, as_context_t *ptCtx,
                                     const as_tok_t *ptTok)
{
    int       iCC     = as_bcc_code(ptTok->szMnem);
    int       iLine   = ptTok->iLine;
    st_u32_t  uiTgt   = 0u;
    st_u32_t  uiCurPC;
    int       iSec    = 0;
    st_i32_t  iDisp;
    st_u16_t  uiOp;

    if (iCC < 0)
    { as_err(ptCtx, iLine, "branch: unknown mnemonic"); return ST_ERROR; }

    /* In pass 1: just advance PC by 4 (opword + 16-bit disp) */
    if (ptS->iPass == 1)
    {
        ptS->uiPC += 4u;
        return ST_NO_ERROR;
    }

    /* Pass 2: resolve target */
    if (!as_eval(ptS, ptTok->szOps, &uiTgt, &iSec))
    {
        as_err(ptCtx, iLine, "branch: undefined label '%s'", ptTok->szOps);
        return ST_ERROR;
    }

    uiCurPC = ptS->uiPC;          /* address of opword */
    iDisp   = (st_i32_t)uiTgt - (st_i32_t)(uiCurPC + 2u);

    if (iDisp < -32768 || iDisp > 32767)
    {
        as_err(ptCtx, iLine, "branch: displacement %d out of 16-bit range",
               iDisp);
        return ST_ERROR;
    }

    uiOp = (st_u16_t)(0x6000u | ((st_u16_t)iCC << 8));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_emit_be16(ptS, ptCtx, (st_u16_t)(st_i16_t)iDisp, iLine);
}

/* NOP/RTS/RTR/RTE — single opword, no operands */
static st_error_t as_encode_implied(as_st_t *ptS, as_context_t *ptCtx,
                                      int iLine, st_u16_t uiOp)
{
    return as_emit_be16(ptS, ptCtx, uiOp, iLine);
}

/* STOP #imm16 — 0x4E72 + imm16 */
static st_error_t as_encode_stop(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok)
{
    as_ea_t  tSrc;
    int      iLine = ptTok->iLine;

    if (!as_parse_ea(ptTok->szOps, &tSrc, ptS) || tSrc.eMode != AS_EA_IMM)
    { as_err(ptCtx, iLine, "STOP: operand must be #imm16"); return ST_ERROR; }

    if (as_emit_be16(ptS, ptCtx, 0x4E72u, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_emit_be16(ptS, ptCtx,
                        (st_u16_t)(tSrc.iDisp & 0xFFFF), iLine);
}

/* TRAP #n — 0x4E40|n  (n = 0-15) */
static st_error_t as_encode_trap(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok)
{
    as_ea_t  tSrc;
    int      iLine = ptTok->iLine;
    int      iVec;

    if (!as_parse_ea(ptTok->szOps, &tSrc, ptS) || tSrc.eMode != AS_EA_IMM)
    { as_err(ptCtx, iLine, "TRAP: operand must be #n"); return ST_ERROR; }

    iVec = tSrc.iDisp;
    if (iVec < 0 || iVec > 15)
    { as_err(ptCtx, iLine, "TRAP: vector must be 0-15"); return ST_ERROR; }

    return as_emit_be16(ptS, ptCtx,
                        (st_u16_t)(0x4E40u | (st_u16_t)iVec), iLine);
}

/* JMP/JSR: base|MMMRRR + EA extension
 * JMP=0x4EC0 JSR=0x4E80                          */
static st_error_t as_encode_jmp_jsr(as_st_t *ptS, as_context_t *ptCtx,
                                      const as_tok_t *ptTok, st_u16_t uiBase)
{
    as_ea_t  tDst;
    int      iLine  = ptTok->iLine;
    int      iEAMR;
    st_u16_t uiOp;

    if (!as_parse_ea(ptTok->szOps, &tDst, ptS))
    { as_err(ptCtx, iLine, "JMP/JSR: bad EA '%s'", ptTok->szOps);
      return ST_ERROR; }

    iEAMR = as_ea_modeword(&tDst);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "JMP/JSR: invalid EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(uiBase | (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tDst, 'L', ptS, ptCtx, iLine);
}

/* LINK An,#d16 — 0x4E50|Rn + d16 */
static st_error_t as_encode_link(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS) || tSrc.eMode != AS_EA_AN)
    { as_err(ptCtx, iLine, "LINK: first operand must be An"); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_IMM)
    { as_err(ptCtx, iLine, "LINK: second operand must be #d16"); return ST_ERROR; }

    if (as_emit_be16(ptS, ptCtx,
                     (st_u16_t)(0x4E50u | (st_u16_t)tSrc.iReg),
                     iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_emit_be16(ptS, ptCtx,
                        (st_u16_t)(st_i16_t)tDst.iDisp, iLine);
}

/* UNLK An — 0x4E58|Rn */
static st_error_t as_encode_unlk(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok)
{
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;

    if (!as_parse_ea(ptTok->szOps, &tDst, ptS) || tDst.eMode != AS_EA_AN)
    { as_err(ptCtx, iLine, "UNLK: operand must be An"); return ST_ERROR; }

    return as_emit_be16(ptS, ptCtx,
                        (st_u16_t)(0x4E58u | (st_u16_t)tDst.iReg), iLine);
}

/* ================================================================== */
/* UC30D instruction encoders                                          */
/* ================================================================== */

/* ------------------------------------------------------------------
 * Shift/rotate register instructions
 * Syntax: mnem.sz #cnt,Dn  or  mnem.sz Dn,Dn
 * Opword: 1110 CCC D SS I TT RRR
 *   CCC: immediate count (0=8) or source register number
 *   D:   direction 0=right 1=left
 *   SS:  00=B 01=W 10=L
 *   I:   0=immediate count 1=register count
 *   TT:  00=AS 01=LS 10=ROX 11=RO
 *   RRR: destination register
 * ------------------------------------------------------------------ */

static st_error_t as_encode_shift(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok,
                                    int iTT, int iDir)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz   = ptTok->cSize;
    int      iLine = ptTok->iLine;
    int      iSzCode;
    int      iCCC;
    int      iI;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "SHIFT: invalid size"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    /* Memory form: single operand, always .W, 1-bit left or right shift
     * Opcode: 1110 TTT D 11 MMMRRR                                     */
    if (szDst[0] == '\0')
    {
        int iEAMR;
        if (cSz != 'W' && cSz != 0)
        { as_err(ptCtx, iLine, "SHIFT: memory form is .W only"); return ST_ERROR; }
        if (!as_parse_ea(szSrc, &tSrc, ptS))
        { as_err(ptCtx, iLine, "SHIFT: bad EA '%s'", szSrc); return ST_ERROR; }
        iEAMR = as_ea_modeword(&tSrc);
        if (iEAMR < 0 || tSrc.eMode == AS_EA_DN || tSrc.eMode == AS_EA_AN
            || tSrc.eMode == AS_EA_IMM || tSrc.eMode == AS_EA_PC_DISP
            || tSrc.eMode == AS_EA_PC_IDX)
        { as_err(ptCtx, iLine, "SHIFT: memory form requires data-alterable EA"); return ST_ERROR; }
        uiOp = (st_u16_t)(0xE0C0u                    |
                           ((st_u16_t)iTT  << 9)      |
                           ((st_u16_t)iDir << 8)      |
                           (st_u16_t)(iEAMR & 0x3F));
        if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
            return ST_ERROR;
        return as_ea_emit(&tSrc, 'W', ptS, ptCtx, iLine);
    }

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "SHIFT: bad source '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "SHIFT: dest must be Dn"); return ST_ERROR; }

    if (tSrc.eMode == AS_EA_IMM)
    {
        iI   = 0; /* immediate */
        iCCC = tSrc.iDisp;
        if (iCCC < 1 || iCCC > 8)
        { as_err(ptCtx, iLine, "SHIFT: count must be 1-8"); return ST_ERROR; }
        if (iCCC == 8) iCCC = 0; /* 8 encodes as 0 */
    }
    else if (tSrc.eMode == AS_EA_DN)
    {
        iI   = 1; /* register */
        iCCC = tSrc.iReg;
    }
    else
    { as_err(ptCtx, iLine, "SHIFT: source must be #n or Dn"); return ST_ERROR; }

    uiOp = (st_u16_t)(0xE000u                    |
                       ((st_u16_t)iCCC    << 9)   |
                       ((st_u16_t)iDir    << 8)   |
                       ((st_u16_t)iSzCode << 6)   |
                       ((st_u16_t)iI      << 5)   |
                       ((st_u16_t)iTT     << 3)   |
                       (st_u16_t)tDst.iReg);
    return as_emit_be16(ptS, ptCtx, uiOp, iLine);
}

/* ------------------------------------------------------------------
 * Bit manipulation: BTST/BCHG/BCLR/BSET
 * Dynamic (Dn bit#): 0000 nnn 1 TT MMMRRR
 * Static  (#imm):    0000 100 1 TT MMMRRR + 0x00bb
 *   TT: BTST=00 BCHG=01 BCLR=10 BSET=11
 * ------------------------------------------------------------------ */

static st_error_t as_encode_bitop(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok, int iTT)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;
    int      iEAMR;
    st_u16_t uiOp;

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "BITOP: bad source '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "BITOP: bad dest '%s'", szDst); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tDst);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "BITOP: invalid dest EA"); return ST_ERROR; }

    if (tSrc.eMode == AS_EA_DN)
    {
        /* Dynamic: 0000 nnn 1 TT MMMRRR [+ dest EA extension] */
        uiOp = (st_u16_t)(((st_u16_t)tSrc.iReg << 9) |
                           0x0100u                     |
                           ((st_u16_t)iTT << 6)        |
                           (st_u16_t)(iEAMR & 0x3F));
        if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
            return ST_ERROR;
        return as_ea_emit(&tDst, 'B', ptS, ptCtx, iLine);
    }
    else if (tSrc.eMode == AS_EA_IMM)
    {
        /* Static: 0000 100 1 TT MMMRRR + word(bitnumber) [+ dest EA extension] */
        int iBit = tSrc.iDisp;
        /* For Dn dest: bit modulo 32; for memory: modulo 8 — just validate range */
        if (tDst.eMode == AS_EA_DN)
        { if (iBit < 0 || iBit > 31) { as_err(ptCtx, iLine, "BITOP: bit# 0-31 for Dn"); return ST_ERROR; } }
        else
        { if (iBit < 0 || iBit > 7)  { as_err(ptCtx, iLine, "BITOP: bit# 0-7 for mem");  return ST_ERROR; } }

        uiOp = (st_u16_t)(0x0800u                 |
                           ((st_u16_t)iTT << 6)    |
                           (st_u16_t)(iEAMR & 0x3F));
        if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
            return ST_ERROR;
        if (as_emit_be16(ptS, ptCtx, (st_u16_t)(iBit & 0xFF), iLine) != ST_NO_ERROR)
            return ST_ERROR;
        return as_ea_emit(&tDst, 'B', ptS, ptCtx, iLine);
    }

    as_err(ptCtx, iLine, "BITOP: source must be Dn or #n");
    return ST_ERROR;
}

/* ------------------------------------------------------------------
 * Register list parser for MOVEM
 * Parses "D0-D7/A0-A6" style lists.
 * Returns 16-bit mask: bits 0-7 = D0-D7, bits 8-15 = A0-A7.
 * Returns 0 on parse error (empty list is also 0; caller must
 * supply at least one register).
 * ------------------------------------------------------------------ */

static st_u16_t as_parse_reglist(const char *sz)
{
    st_u16_t    uiMask = 0;
    const char *p      = sz;
    char        szTok[8];
    int         i;
    int         iR1;
    int         iR2;
    int         bIsAn;
    int         bIsAn2;

    while (*p)
    {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        /* Read first register token */
        i = 0;
        while (i < 7 && (isalnum((unsigned char)*p))) szTok[i++] = (char)toupper((unsigned char)*p++);
        szTok[i] = '\0';
        if (i == 0) return 0;

        iR1 = as_parse_dn_str(szTok);
        if (iR1 >= 0) { bIsAn = 0; }
        else
        {
            iR1 = as_parse_an_str(szTok);
            if (iR1 < 0) return 0;
            bIsAn = 1;
        }

        if (*p == '-')
        {
            /* Range */
            p++;
            i = 0;
            while (i < 7 && (isalnum((unsigned char)*p))) szTok[i++] = (char)toupper((unsigned char)*p++);
            szTok[i] = '\0';
            if (!bIsAn)
            {
                iR2 = as_parse_dn_str(szTok); bIsAn2 = 0;
            }
            else
            {
                iR2 = as_parse_an_str(szTok); bIsAn2 = 1;
            }
            if (iR2 < 0 || bIsAn2 != bIsAn || iR2 < iR1) return 0;
            for (i = iR1; i <= iR2; i++)
                uiMask |= (st_u16_t)(bIsAn ? (1u << (8 + i)) : (1u << i));
        }
        else
        {
            /* Single register */
            uiMask |= (st_u16_t)(bIsAn ? (1u << (8 + iR1)) : (1u << iR1));
        }

        while (*p == ' ' || *p == '\t') p++;
        if (*p == '/') p++;
    }
    return uiMask;
}

/* Returns ST_TRUE if string looks like a MOVEM register list */
static st_bool_t as_is_reglist(const char *sz)
{
    char c0;
    if (strchr(sz, '/') != NULL) return ST_TRUE;
    /* Range pattern: D?-D? or A?-A? (case-insensitive) */
    c0 = (char)toupper((unsigned char)sz[0]);
    if ((c0 == 'D' || c0 == 'A') && isdigit((unsigned char)sz[1]) && sz[2] == '-')
        return ST_TRUE;
    return ST_FALSE;
}

/* Reverse 16 bits of regmask (for predecrement mode) */
static st_u16_t as_reverse_regmask(st_u16_t uiMask)
{
    st_u16_t uiRev = 0u;
    int      i;
    for (i = 0; i < 16; i++)
        if (uiMask & (st_u16_t)(1u << i))
            uiRev |= (st_u16_t)(1u << (15 - i));
    return uiRev;
}

/* ------------------------------------------------------------------
 * MOVEM.W/L — move multiple registers
 * Store: reglist,ea  →  0100 1000 1 S MMMRRR + mask
 * Load:  ea,reglist  →  0100 1100 1 S MMMRRR + mask
 *   S: 0=W 1=L
 * Predecrement store: mask is bit-reversed.
 * ------------------------------------------------------------------ */

static st_error_t as_encode_movem(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_OPSZ];
    char     szDst[AS_MAX_OPSZ];
    as_ea_t  tEA;
    char     cSz   = ptTok->cSize;
    int      iLine = ptTok->iLine;
    int      iSz;   /* 0=W 1=L */
    int      bStore;
    st_u16_t uiMask;
    int      iEAMR;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'L';
    iSz = (cSz == 'L') ? 1 : 0;
    if (cSz != 'W' && cSz != 'L')
    { as_err(ptCtx, iLine, "MOVEM: size must be W or L"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_OPSZ, szDst, AS_MAX_OPSZ);

    /* Determine direction from which operand looks like a reglist */
    if (as_is_reglist(szSrc))
    {
        /* store: reglist -> ea */
        uiMask = as_parse_reglist(szSrc);
        bStore = 1;
        if (!as_parse_ea(szDst, &tEA, ptS))
        { as_err(ptCtx, iLine, "MOVEM: bad dest EA '%s'", szDst); return ST_ERROR; }
    }
    else if (as_is_reglist(szDst))
    {
        /* load: ea -> reglist */
        uiMask = as_parse_reglist(szDst);
        bStore = 0;
        if (!as_parse_ea(szSrc, &tEA, ptS))
        { as_err(ptCtx, iLine, "MOVEM: bad src EA '%s'", szSrc); return ST_ERROR; }
    }
    else
    {
        /* Single register case: try szDst as EA; if memory EA → store */
        if (as_parse_ea(szDst, &tEA, ptS) &&
            tEA.eMode != AS_EA_DN && tEA.eMode != AS_EA_AN &&
            tEA.eMode != AS_EA_IMM)
        {
            uiMask = as_parse_reglist(szSrc);
            bStore = 1;
        }
        else if (as_parse_ea(szSrc, &tEA, ptS) &&
                 tEA.eMode != AS_EA_DN && tEA.eMode != AS_EA_AN &&
                 tEA.eMode != AS_EA_IMM)
        {
            uiMask = as_parse_reglist(szDst);
            bStore = 0;
        }
        else
        {
            as_err(ptCtx, iLine, "MOVEM: cannot determine direction");
            return ST_ERROR;
        }
    }

    if (uiMask == 0)
    { as_err(ptCtx, iLine, "MOVEM: empty or invalid register list"); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tEA);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "MOVEM: invalid EA"); return ST_ERROR; }

    /* For predecrement store, bit-reverse the mask */
    if (bStore && tEA.eMode == AS_EA_AN_PRE)
        uiMask = as_reverse_regmask(uiMask);

    uiOp = (st_u16_t)((bStore ? 0x4880u : 0x4C80u) |
                       ((st_u16_t)iSz << 6)          |
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    if (as_emit_be16(ptS, ptCtx, uiMask, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tEA, cSz, ptS, ptCtx, iLine);
}

/* ------------------------------------------------------------------
 * ADDA / SUBA — add/subtract to address register
 * ADDA.W src,An: 1101 nnn 011 MMMRRR
 * ADDA.L src,An: 1101 nnn 111 MMMRRR
 * SUBA.W src,An: 1001 nnn 011 MMMRRR
 * SUBA.L src,An: 1001 nnn 111 MMMRRR
 * ------------------------------------------------------------------ */

static st_error_t as_encode_adda_suba(as_st_t *ptS, as_context_t *ptCtx,
                                        const as_tok_t *ptTok, st_u16_t uiBase)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz   = ptTok->cSize;
    int      iLine = ptTok->iLine;
    int      iOoo;
    int      iEAMR;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    if (cSz == 'W') iOoo = 3;
    else if (cSz == 'L') iOoo = 7;
    else { as_err(ptCtx, iLine, "ADDA/SUBA: size must be W or L"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "ADDA/SUBA: bad source '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_AN)
    { as_err(ptCtx, iLine, "ADDA/SUBA: dest must be An"); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tSrc);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "ADDA/SUBA: invalid source EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(uiBase                      |
                       ((st_u16_t)tDst.iReg << 9)  |
                       ((st_u16_t)iOoo       << 6)  |
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tSrc, cSz, ptS, ptCtx, iLine);
}

/* ------------------------------------------------------------------
 * MULU/MULS/DIVU/DIVS — multiply/divide (word src × 32-bit Dn)
 * MULU.W src,Dn: 1100 nnn 011 MMMRRR
 * MULS.W src,Dn: 1100 nnn 111 MMMRRR
 * DIVU.W src,Dn: 1000 nnn 011 MMMRRR
 * DIVS.W src,Dn: 1000 nnn 111 MMMRRR
 * ------------------------------------------------------------------ */

static st_error_t as_encode_mul_div(as_st_t *ptS, as_context_t *ptCtx,
                                      const as_tok_t *ptTok,
                                      st_u16_t uiBase, int iSigned)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;
    int      iEAMR;
    st_u16_t uiOp;

    /* Size is always W (word source operand); size suffix optional */
    if (ptTok->cSize != 0 && ptTok->cSize != 'W')
    { as_err(ptCtx, iLine, "MULU/DIVU: only .W size supported"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "MULU/DIVU: bad source '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "MULU/DIVU: dest must be Dn"); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tSrc);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "MULU/DIVU: invalid source EA"); return ST_ERROR; }

    /* ooo: unsigned=011 signed=111 */
    uiOp = (st_u16_t)(uiBase                      |
                       ((st_u16_t)tDst.iReg << 9)  |
                       (iSigned ? 0x01C0u : 0x00C0u)|
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tSrc, 'W', ptS, ptCtx, iLine);
}

/* ------------------------------------------------------------------
 * ADDX / SUBX — add/subtract with extend
 * ADDX.sz Dx,Dy:     1101 yyy 1 ss 00 0 xxx
 * ADDX.sz -(Ax),-(Ay): 1101 yyy 1 ss 00 1 xxx
 * SUBX same but 1001 base
 * ------------------------------------------------------------------ */

static st_error_t as_encode_addx_subx(as_st_t *ptS, as_context_t *ptCtx,
                                        const as_tok_t *ptTok, st_u16_t uiBase)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz   = ptTok->cSize;
    int      iLine = ptTok->iLine;
    int      iSzCode;
    int      bMem;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "ADDX/SUBX: invalid size"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "ADDX/SUBX: bad source '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "ADDX/SUBX: bad dest '%s'", szDst); return ST_ERROR; }

    if (tSrc.eMode == AS_EA_DN && tDst.eMode == AS_EA_DN)
        bMem = 0;
    else if (tSrc.eMode == AS_EA_AN_PRE && tDst.eMode == AS_EA_AN_PRE)
        bMem = 1;
    else
    { as_err(ptCtx, iLine, "ADDX/SUBX: operands must be Dx,Dy or -(Ax),-(Ay)"); return ST_ERROR; }

    uiOp = (st_u16_t)(uiBase                          |
                       ((st_u16_t)tDst.iReg   << 9)   |
                       (1u                    << 8)    |
                       ((st_u16_t)iSzCode     << 6)    |
                       ((st_u16_t)bMem        << 3)    |
                       (st_u16_t)tSrc.iReg);
    return as_emit_be16(ptS, ptCtx, uiOp, iLine);
}

/* ------------------------------------------------------------------
 * Scc — Set byte on condition
 * 0101 cccc 11 MMMRRR  (always byte, B size implied)
 * ------------------------------------------------------------------ */

static const struct { const char *sz; int iCC; } g_as_aScc[] =
{
    { "ST",  0x00 }, { "SF",  0x01 },
    { "SHI", 0x02 }, { "SLS", 0x03 },
    { "SCC", 0x04 }, { "SHS", 0x04 },
    { "SCS", 0x05 }, { "SLO", 0x05 },
    { "SNE", 0x06 }, { "SEQ", 0x07 },
    { "SVC", 0x08 }, { "SVS", 0x09 },
    { "SPL", 0x0A }, { "SMI", 0x0B },
    { "SGE", 0x0C }, { "SLT", 0x0D },
    { "SGT", 0x0E }, { "SLE", 0x0F },
    { NULL,   -1  }
};

static int as_scc_code(const char *szMnem)
{
    int i;
    for (i = 0; g_as_aScc[i].sz != NULL; i++)
        if (strcmp(szMnem, g_as_aScc[i].sz) == 0)
            return g_as_aScc[i].iCC;
    return -1;
}

static st_error_t as_encode_scc(as_st_t *ptS, as_context_t *ptCtx,
                                   const as_tok_t *ptTok)
{
    int      iCC   = as_scc_code(ptTok->szMnem);
    int      iLine = ptTok->iLine;
    as_ea_t  tDst;
    int      iEAMR;
    st_u16_t uiOp;

    if (iCC < 0)
    { as_err(ptCtx, iLine, "Scc: unknown mnemonic"); return ST_ERROR; }

    if (!as_parse_ea(ptTok->szOps, &tDst, ptS))
    { as_err(ptCtx, iLine, "Scc: bad dest EA '%s'", ptTok->szOps); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tDst);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "Scc: invalid dest EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(0x50C0u                 |
                       ((st_u16_t)iCC   << 8)  |
                       (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tDst, 'B', ptS, ptCtx, iLine);
}

/* ------------------------------------------------------------------
 * DBcc — Decrement and branch on condition
 * 0101 cccc 11 001 rrr + 16-bit displacement
 * DBRA = DBF (cc=0x01)
 * Displacement: target - (opword_addr + 2)
 * ------------------------------------------------------------------ */

static const struct { const char *sz; int iCC; } g_as_aDBcc[] =
{
    { "DBT",  0x00 }, { "DBRA", 0x01 }, { "DBF",  0x01 },
    { "DBHI", 0x02 }, { "DBLS", 0x03 },
    { "DBCC", 0x04 }, { "DBHS", 0x04 },
    { "DBCS", 0x05 }, { "DBLO", 0x05 },
    { "DBNE", 0x06 }, { "DBEQ", 0x07 },
    { "DBVC", 0x08 }, { "DBVS", 0x09 },
    { "DBPL", 0x0A }, { "DBMI", 0x0B },
    { "DBGE", 0x0C }, { "DBLT", 0x0D },
    { "DBGT", 0x0E }, { "DBLE", 0x0F },
    { NULL,    -1  }
};

static int as_dbcc_code(const char *szMnem)
{
    int i;
    for (i = 0; g_as_aDBcc[i].sz != NULL; i++)
        if (strcmp(szMnem, g_as_aDBcc[i].sz) == 0)
            return g_as_aDBcc[i].iCC;
    return -1;
}

static st_error_t as_encode_dbcc(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok)
{
    int       iCC    = as_dbcc_code(ptTok->szMnem);
    int       iLine  = ptTok->iLine;
    char      szReg[AS_MAX_WORD];
    char      szLbl[AS_MAX_WORD];
    as_ea_t   tReg;
    st_u32_t  uiTgt  = 0u;
    st_u32_t  uiCurPC;
    st_i32_t  iDisp;
    int       iSec   = 0;
    st_u16_t  uiOp;

    if (iCC < 0)
    { as_err(ptCtx, iLine, "DBcc: unknown mnemonic"); return ST_ERROR; }

    /* Operands: "Dn,label" */
    as_split_ops(ptTok->szOps, szReg, AS_MAX_WORD, szLbl, AS_MAX_WORD);

    if (!as_parse_ea(szReg, &tReg, ptS) || tReg.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "DBcc: first operand must be Dn"); return ST_ERROR; }

    if (ptS->iPass == 1)
    {
        ptS->uiPC += 4u;  /* opword(2) + displacement(2) */
        return ST_NO_ERROR;
    }

    /* Pass 2: resolve target label */
    if (!as_eval(ptS, szLbl, &uiTgt, &iSec))
    {
        as_err(ptCtx, iLine, "DBcc: undefined label '%s'", szLbl);
        return ST_ERROR;
    }

    uiCurPC = ptS->uiPC;
    iDisp   = (st_i32_t)uiTgt - (st_i32_t)(uiCurPC + 2u);

    if (iDisp < -32768 || iDisp > 32767)
    {
        as_err(ptCtx, iLine, "DBcc: displacement %d out of range", iDisp);
        return ST_ERROR;
    }

    uiOp = (st_u16_t)(0x50C8u                 |
                       ((st_u16_t)iCC   << 8)  |
                       (st_u16_t)tReg.iReg);
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_emit_be16(ptS, ptCtx, (st_u16_t)(st_i16_t)iDisp, iLine);
}

/* ------------------------------------------------------------------
 * EXG — exchange registers
 * EXG Dx,Dy: 1100 xxx 1 01000 yyy  (mode=01000)
 * EXG Ax,Ay: 1100 xxx 1 01001 yyy  (mode=01001)
 * EXG Dx,Ay: 1100 xxx 1 10001 yyy  (mode=10001)
 * ------------------------------------------------------------------ */

static st_error_t as_encode_exg(as_st_t *ptS, as_context_t *ptCtx,
                                   const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;
    int      iMode;
    st_u16_t uiOp;

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "EXG: bad source '%s'", szSrc); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "EXG: bad dest '%s'", szDst); return ST_ERROR; }

    if (tSrc.eMode == AS_EA_DN && tDst.eMode == AS_EA_DN)
        iMode = 0x08; /* 01000: Dx,Dy */
    else if (tSrc.eMode == AS_EA_AN && tDst.eMode == AS_EA_AN)
        iMode = 0x09; /* 01001: Ax,Ay */
    else if (tSrc.eMode == AS_EA_DN && tDst.eMode == AS_EA_AN)
        iMode = 0x11; /* 10001: Dx,Ay */
    else if (tSrc.eMode == AS_EA_AN && tDst.eMode == AS_EA_DN)
    {
        /* EXG An,Dn → swap operands → EXG Dn,An */
        int iTmp = tSrc.iReg;
        tSrc.iReg = tDst.iReg;
        tDst.iReg = iTmp;
        iMode = 0x11;
    }
    else
    { as_err(ptCtx, iLine, "EXG: operands must be Dn/Dn, An/An or Dn/An"); return ST_ERROR; }

    /* DEVPAC3 quirk: for An↔Am (opmode 0x09) the second An goes in the
     * high field (bits 11-9) and the first An goes in the low field.
     * For Dn↔Dm and Dn↔Am the standard order (first op in high) applies. */
    if (iMode == 0x09)
        uiOp = (st_u16_t)(0xC100u                      |
                           ((st_u16_t)tDst.iReg  << 9)  |
                           ((st_u16_t)iMode       << 3)  |
                           (st_u16_t)tSrc.iReg);
    else
        uiOp = (st_u16_t)(0xC100u                      |
                           ((st_u16_t)tSrc.iReg  << 9)  |
                           ((st_u16_t)iMode       << 3)  |
                           (st_u16_t)tDst.iReg);
    return as_emit_be16(ptS, ptCtx, uiOp, iLine);
}

/* ------------------------------------------------------------------
 * PEA — push effective address
 * 0100 100 001 MMMRRR
 * Source must be a control addressing mode (not Dn, An, #imm,
 * predecrement, or postincrement).
 * ------------------------------------------------------------------ */

static st_error_t as_encode_pea(as_st_t *ptS, as_context_t *ptCtx,
                                   const as_tok_t *ptTok)
{
    as_ea_t  tSrc;
    int      iLine = ptTok->iLine;
    int      iEAMR;
    st_u16_t uiOp;

    if (!as_parse_ea(ptTok->szOps, &tSrc, ptS))
    { as_err(ptCtx, iLine, "PEA: bad EA '%s'", ptTok->szOps); return ST_ERROR; }

    /* Control EA: (An), d16(An), d8(An,Xn), abs.W, abs.L, d16(PC), d8(PC,Xn) */
    if (tSrc.eMode == AS_EA_DN     || tSrc.eMode == AS_EA_AN      ||
        tSrc.eMode == AS_EA_AN_POST|| tSrc.eMode == AS_EA_AN_PRE  ||
        tSrc.eMode == AS_EA_IMM)
    { as_err(ptCtx, iLine, "PEA: control EA required"); return ST_ERROR; }

    iEAMR = as_ea_modeword(&tSrc);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "PEA: invalid EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(0x4840u | (st_u16_t)(iEAMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tSrc, 'L', ptS, ptCtx, iLine);
}

/* ------------------------------------------------------------------
 * UC30E new encoders
 * ------------------------------------------------------------------ */

/* TAS <ea>: 0100 1010 11 MMMRRR  (no size suffix)
 * NBCD <ea>: 0100 1000 00 MMMRRR (no size suffix, implicit .B)      */
static st_error_t as_encode_tas_nbcd(as_st_t *ptS, as_context_t *ptCtx,
                                       const as_tok_t *ptTok, st_u16_t uiBase)
{
    as_ea_t  tEA;
    int      iLine = ptTok->iLine;
    int      iEAMR;

    if (!as_parse_ea(ptTok->szOps, &tEA, ptS))
    { as_err(ptCtx, iLine, "TAS/NBCD: bad EA '%s'", ptTok->szOps); return ST_ERROR; }
    iEAMR = as_ea_modeword(&tEA);
    if (iEAMR < 0)
    { as_err(ptCtx, iLine, "TAS/NBCD: invalid EA"); return ST_ERROR; }
    if (as_emit_be16(ptS, ptCtx,
                     (st_u16_t)(uiBase | (st_u16_t)(iEAMR & 0x3F)),
                     iLine) != ST_NO_ERROR) return ST_ERROR;
    return as_ea_emit(&tEA, 'B', ptS, ptCtx, iLine);
}

/* ABCD / SBCD:
 *   Register: base | (Ry<<9) | Rx
 *   Memory:   base | (Ry<<9) | 0x0008 | Rx
 * ABCD base=0xC100, SBCD base=0x8100                                */
static st_error_t as_encode_bcd(as_st_t *ptS, as_context_t *ptCtx,
                                  const as_tok_t *ptTok, st_u16_t uiBase)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;
    st_u16_t uiOp;

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "BCD: bad source EA"); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "BCD: bad dest EA"); return ST_ERROR; }

    if (tSrc.eMode == AS_EA_DN && tDst.eMode == AS_EA_DN)
        uiOp = (st_u16_t)(uiBase |
                           ((st_u16_t)tDst.iReg << 9) |
                           (st_u16_t)tSrc.iReg);
    else if (tSrc.eMode == AS_EA_AN_PRE && tDst.eMode == AS_EA_AN_PRE)
        uiOp = (st_u16_t)(uiBase |
                           ((st_u16_t)tDst.iReg << 9) |
                           0x0008u                     |
                           (st_u16_t)tSrc.iReg);
    else
    { as_err(ptCtx, iLine, "BCD: must be Dn,Dn or -(An),-(An)"); return ST_ERROR; }

    return as_emit_be16(ptS, ptCtx, uiOp, iLine);
}

/* CHK.W <ea>,Dn: 0100 Dn 110 MMMRRR                                */
static st_error_t as_encode_chk(as_st_t *ptS, as_context_t *ptCtx,
                                  const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    int      iLine = ptTok->iLine;
    int      iSrcMR;
    st_u16_t uiOp;

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "CHK: bad source EA"); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_DN)
    { as_err(ptCtx, iLine, "CHK: dest must be Dn"); return ST_ERROR; }

    iSrcMR = as_ea_modeword(&tSrc);
    if (iSrcMR < 0)
    { as_err(ptCtx, iLine, "CHK: invalid source EA"); return ST_ERROR; }

    uiOp = (st_u16_t)(0x4180u |
                       ((st_u16_t)tDst.iReg << 9) |
                       (st_u16_t)(iSrcMR & 0x3F));
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_ea_emit(&tSrc, 'W', ptS, ptCtx, iLine);
}

/* MOVEP Dn,(d16,An) and MOVEP (d16,An),Dn
 *   Dn→mem .W: 0000 Dn 110 001 An  = 0x0188 | (Dn<<9) | An
 *   Dn→mem .L: 0000 Dn 111 001 An  = 0x01C8 | (Dn<<9) | An
 *   mem→Dn .W: 0000 Dn 100 001 An  = 0x0108 | (Dn<<9) | An
 *   mem→Dn .L: 0000 Dn 101 001 An  = 0x0148 | (Dn<<9) | An
 * followed by one extension word: d16                               */
static st_error_t as_encode_movep(as_st_t *ptS, as_context_t *ptCtx,
                                    const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz   = ptTok->cSize;
    int      iLine = ptTok->iLine;
    int      iDn;
    int      iAn;
    st_i32_t iD16;
    st_u16_t uiBase;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS))
    { as_err(ptCtx, iLine, "MOVEP: bad source EA"); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS))
    { as_err(ptCtx, iLine, "MOVEP: bad dest EA"); return ST_ERROR; }

    if (tSrc.eMode == AS_EA_DN && tDst.eMode == AS_EA_AN_DISP)
    {
        iDn   = tSrc.iReg;
        iAn   = tDst.iReg;
        iD16  = tDst.iDisp;
        uiBase = (cSz == 'L') ? 0x01C8u : 0x0188u;
    }
    else if (tSrc.eMode == AS_EA_AN_DISP && tDst.eMode == AS_EA_DN)
    {
        iAn   = tSrc.iReg;
        iD16  = tSrc.iDisp;
        iDn   = tDst.iReg;
        uiBase = (cSz == 'L') ? 0x0148u : 0x0108u;
    }
    else
    { as_err(ptCtx, iLine, "MOVEP: must be Dn,d16(An) or d16(An),Dn"); return ST_ERROR; }

    uiOp = (st_u16_t)(uiBase | ((st_u16_t)iDn << 9) | (st_u16_t)iAn);
    if (as_emit_be16(ptS, ptCtx, uiOp, iLine) != ST_NO_ERROR)
        return ST_ERROR;
    return as_emit_be16(ptS, ptCtx, (st_u16_t)(st_i16_t)iD16, iLine);
}

/* CMPM (Ay)+,(Ax)+: 1011 Ax 1 SS 001 Ay                            */
static st_error_t as_encode_cmpm(as_st_t *ptS, as_context_t *ptCtx,
                                   const as_tok_t *ptTok)
{
    char     szSrc[AS_MAX_WORD];
    char     szDst[AS_MAX_WORD];
    as_ea_t  tSrc;
    as_ea_t  tDst;
    char     cSz   = ptTok->cSize;
    int      iLine = ptTok->iLine;
    int      iSzCode;
    st_u16_t uiOp;

    if (cSz == 0) cSz = 'W';
    iSzCode = as_sz_code(cSz);
    if (iSzCode < 0)
    { as_err(ptCtx, iLine, "CMPM: invalid size"); return ST_ERROR; }

    as_split_ops(ptTok->szOps, szSrc, AS_MAX_WORD, szDst, AS_MAX_WORD);

    if (!as_parse_ea(szSrc, &tSrc, ptS) || tSrc.eMode != AS_EA_AN_POST)
    { as_err(ptCtx, iLine, "CMPM: source must be (Ay)+"); return ST_ERROR; }
    if (!as_parse_ea(szDst, &tDst, ptS) || tDst.eMode != AS_EA_AN_POST)
    { as_err(ptCtx, iLine, "CMPM: dest must be (Ax)+"); return ST_ERROR; }

    uiOp = (st_u16_t)(0xB108u |
                       ((st_u16_t)tDst.iReg << 9) |
                       ((st_u16_t)iSzCode   << 6) |
                       (st_u16_t)tSrc.iReg);
    return as_emit_be16(ptS, ptCtx, uiOp, iLine);
}

/* ------------------------------------------------------------------ */
/* Master instruction dispatcher (UC30C)                               */
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

    if (strcmp(sz, "ADD") == 0)
        return as_encode_alu_binary(ptS, ptCtx, ptTok, AS_BASE_ADD, 0x0600u);
    if (strcmp(sz, "SUB") == 0)
        return as_encode_alu_binary(ptS, ptCtx, ptTok, AS_BASE_SUB, 0x0400u);
    if (strcmp(sz, "AND") == 0)
        return as_encode_alu_binary(ptS, ptCtx, ptTok, AS_BASE_AND, 0x0200u);
    if (strcmp(sz, "OR")  == 0)
        return as_encode_alu_binary(ptS, ptCtx, ptTok, AS_BASE_OR,  0x0000u);
    if (strcmp(sz, "CMP") == 0)
        return as_encode_cmp(ptS, ptCtx, ptTok);
    if (strcmp(sz, "EOR") == 0)
        return as_encode_eor(ptS, ptCtx, ptTok);

    if (strcmp(sz, "ADDI") == 0)
        return as_encode_imm_alu(ptS, ptCtx, ptTok, 0x0600u);
    if (strcmp(sz, "SUBI") == 0)
        return as_encode_imm_alu(ptS, ptCtx, ptTok, 0x0400u);
    if (strcmp(sz, "CMPI") == 0)
        return as_encode_imm_alu(ptS, ptCtx, ptTok, 0x0C00u);
    if (strcmp(sz, "ANDI") == 0)
        return as_encode_imm_alu(ptS, ptCtx, ptTok, 0x0200u);
    if (strcmp(sz, "ORI")  == 0)
        return as_encode_imm_alu(ptS, ptCtx, ptTok, 0x0000u);
    if (strcmp(sz, "EORI") == 0)
        return as_encode_imm_alu(ptS, ptCtx, ptTok, 0x0A00u);

    if (strcmp(sz, "ADDQ") == 0)
        return as_encode_addq_subq(ptS, ptCtx, ptTok, 0);
    if (strcmp(sz, "SUBQ") == 0)
        return as_encode_addq_subq(ptS, ptCtx, ptTok, 1);

    if (strcmp(sz, "NEG")  == 0)
        return as_encode_unary(ptS, ptCtx, ptTok, 0x4400u);
    if (strcmp(sz, "NEGX") == 0)
        return as_encode_unary(ptS, ptCtx, ptTok, 0x4000u);
    if (strcmp(sz, "NOT")  == 0)
        return as_encode_unary(ptS, ptCtx, ptTok, 0x4600u);
    if (strcmp(sz, "TST")  == 0)
        return as_encode_unary(ptS, ptCtx, ptTok, 0x4A00u);
    if (strcmp(sz, "EXT")  == 0)
        return as_encode_ext(ptS, ptCtx, ptTok);

    if (as_bcc_code(sz) >= 0)
        return as_encode_branch(ptS, ptCtx, ptTok);

    if (strcmp(sz, "NOP")  == 0)
        return as_encode_implied(ptS, ptCtx, ptTok->iLine, 0x4E71u);
    if (strcmp(sz, "RTS")  == 0)
        return as_encode_implied(ptS, ptCtx, ptTok->iLine, 0x4E75u);
    if (strcmp(sz, "RTR")  == 0)
        return as_encode_implied(ptS, ptCtx, ptTok->iLine, 0x4E77u);
    if (strcmp(sz, "RTE")  == 0)
        return as_encode_implied(ptS, ptCtx, ptTok->iLine, 0x4E73u);

    if (strcmp(sz, "STOP") == 0)
        return as_encode_stop(ptS, ptCtx, ptTok);
    if (strcmp(sz, "TRAP") == 0)
        return as_encode_trap(ptS, ptCtx, ptTok);

    if (strcmp(sz, "JMP")  == 0)
        return as_encode_jmp_jsr(ptS, ptCtx, ptTok, 0x4EC0u);
    if (strcmp(sz, "JSR")  == 0)
        return as_encode_jmp_jsr(ptS, ptCtx, ptTok, 0x4E80u);

    if (strcmp(sz, "LINK") == 0)
        return as_encode_link(ptS, ptCtx, ptTok);
    if (strcmp(sz, "UNLK") == 0)
        return as_encode_unlk(ptS, ptCtx, ptTok);

    /* UC30D: shifts */
    if (strcmp(sz, "ASL") == 0) return as_encode_shift(ptS, ptCtx, ptTok, 0, 1);
    if (strcmp(sz, "ASR") == 0) return as_encode_shift(ptS, ptCtx, ptTok, 0, 0);
    if (strcmp(sz, "LSL") == 0) return as_encode_shift(ptS, ptCtx, ptTok, 1, 1);
    if (strcmp(sz, "LSR") == 0) return as_encode_shift(ptS, ptCtx, ptTok, 1, 0);
    if (strcmp(sz, "ROXL")== 0) return as_encode_shift(ptS, ptCtx, ptTok, 2, 1);
    if (strcmp(sz, "ROXR")== 0) return as_encode_shift(ptS, ptCtx, ptTok, 2, 0);
    if (strcmp(sz, "ROL") == 0) return as_encode_shift(ptS, ptCtx, ptTok, 3, 1);
    if (strcmp(sz, "ROR") == 0) return as_encode_shift(ptS, ptCtx, ptTok, 3, 0);

    /* UC30D: bit ops */
    if (strcmp(sz, "BTST") == 0) return as_encode_bitop(ptS, ptCtx, ptTok, 0);
    if (strcmp(sz, "BCHG") == 0) return as_encode_bitop(ptS, ptCtx, ptTok, 1);
    if (strcmp(sz, "BCLR") == 0) return as_encode_bitop(ptS, ptCtx, ptTok, 2);
    if (strcmp(sz, "BSET") == 0) return as_encode_bitop(ptS, ptCtx, ptTok, 3);

    /* UC30D: MOVEM */
    if (strcmp(sz, "MOVEM") == 0) return as_encode_movem(ptS, ptCtx, ptTok);

    /* UC30D: address register arithmetic */
    if (strcmp(sz, "ADDA") == 0) return as_encode_adda_suba(ptS, ptCtx, ptTok, 0xD000u);
    if (strcmp(sz, "SUBA") == 0) return as_encode_adda_suba(ptS, ptCtx, ptTok, 0x9000u);
    if (strcmp(sz, "CMPA") == 0) return as_encode_adda_suba(ptS, ptCtx, ptTok, 0xB000u);

    /* UC30D: multiply/divide */
    if (strcmp(sz, "MULU") == 0) return as_encode_mul_div(ptS, ptCtx, ptTok, 0xC000u, 0);
    if (strcmp(sz, "MULS") == 0) return as_encode_mul_div(ptS, ptCtx, ptTok, 0xC000u, 1);
    if (strcmp(sz, "DIVU") == 0) return as_encode_mul_div(ptS, ptCtx, ptTok, 0x8000u, 0);
    if (strcmp(sz, "DIVS") == 0) return as_encode_mul_div(ptS, ptCtx, ptTok, 0x8000u, 1);

    /* UC30D: extended arithmetic */
    if (strcmp(sz, "ADDX") == 0) return as_encode_addx_subx(ptS, ptCtx, ptTok, 0xD000u);
    if (strcmp(sz, "SUBX") == 0) return as_encode_addx_subx(ptS, ptCtx, ptTok, 0x9000u);

    /* UC30D: Scc */
    if (as_scc_code(sz) >= 0)
        return as_encode_scc(ptS, ptCtx, ptTok);

    /* UC30D: DBcc */
    if (as_dbcc_code(sz) >= 0)
        return as_encode_dbcc(ptS, ptCtx, ptTok);

    /* UC30D: EXG / PEA */
    if (strcmp(sz, "EXG") == 0) return as_encode_exg(ptS, ptCtx, ptTok);
    if (strcmp(sz, "PEA") == 0) return as_encode_pea(ptS, ptCtx, ptTok);

    /* UC30E: TAS / NBCD */
    if (strcmp(sz, "TAS")  == 0) return as_encode_tas_nbcd(ptS, ptCtx, ptTok, 0x4AC0u);
    if (strcmp(sz, "NBCD") == 0) return as_encode_tas_nbcd(ptS, ptCtx, ptTok, 0x4800u);

    /* UC30E: ABCD / SBCD */
    if (strcmp(sz, "ABCD") == 0) return as_encode_bcd(ptS, ptCtx, ptTok, 0xC100u);
    if (strcmp(sz, "SBCD") == 0) return as_encode_bcd(ptS, ptCtx, ptTok, 0x8100u);

    /* UC30E: CHK */
    if (strcmp(sz, "CHK")  == 0) return as_encode_chk(ptS, ptCtx, ptTok);

    /* UC30E: MOVEP */
    if (strcmp(sz, "MOVEP") == 0) return as_encode_movep(ptS, ptCtx, ptTok);

    /* UC30E: CMPM */
    if (strcmp(sz, "CMPM") == 0) return as_encode_cmpm(ptS, ptCtx, ptTok);

    /* UC30E: TRAPV / RESET / ILLEGAL */
    if (strcmp(sz, "TRAPV")   == 0) return as_encode_implied(ptS, ptCtx, ptTok->iLine, 0x4E76u);
    if (strcmp(sz, "RESET")   == 0) return as_encode_implied(ptS, ptCtx, ptTok->iLine, 0x4E70u);
    if (strcmp(sz, "ILLEGAL") == 0) return as_encode_implied(ptS, ptCtx, ptTok->iLine, 0x4AFCu);

    as_err(ptCtx, ptTok->iLine, "unknown mnemonic '%s'", sz);
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
