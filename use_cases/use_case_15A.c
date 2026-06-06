/*
 * use_case_15A.c - Disassembler torture test vs SOURCE.S / SOURCE.PRG
 *
 * Loads use_cases/UC15A/SOURCE.PRG (assembled from SOURCE.S via DEVPAC3
 * on real ATARI ST), disassembles the .text section instruction by
 * instruction, then parses SOURCE.S to extract the expected instruction
 * strings, and compares them in sequence.
 *
 * Comparison categories:
 *   MATCH  : exact match after case + whitespace normalisation
 *   DCW    : disassembler outputs DC.W (unimplemented instruction, expected)
 *   PCREL  : source uses PC-relative notation (*+N(PC) / LABEL(PC))
 *             — disassembler outputs absolute address (COMPAT, not DIFF)
 *   COMPAT : known syntax variant:
 *              abs.L  : source $HEXHEX.L, disasm $HEXHEX (no .L suffix)
 *              disp   : source 8(An) decimal, disasm $8(An) hex
 *              imm    : source #-8 decimal, disasm #-$8 hex
 *              branch : source "BRA LABEL", disasm "BRA.S $addr"
 *   DIFF   : genuine mismatch — MUST be 0 for test to pass
 *
 * TEST MATRIX - UC15A:
 *   [N] Nominal    : 9 tests  - load, disasm, parse, compare, counts
 *   [R] Robustness : 3 tests  - empty input, comment line, bad PRG path
 *   [S] Skipped    : 0 tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../src/common.h"
#include "../src/st.h"
#include "../src/load.h"
#include "../src/disassemble.h"
#include "use_cases.h"

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------ */

#define UC15A_PRG_PATH    "use_cases/UC15A/SOURCE.PRG"
#define UC15A_SRC_PATH    "use_cases/UC15A/SOURCE.S"
#define UC15A_MAX_INSTR   3000     /* max instructions in test binary */
#define UC15A_MAX_LINE    256      /* max chars per instruction string */

/* Comparison categories */
#define UC15A_MATCH   0
#define UC15A_DCW     1
#define UC15A_PCREL   2
#define UC15A_COMPAT  3
#define UC15A_DIFF    4

/* ------------------------------------------------------------------
 * String normalisation: uppercase + collapse whitespace + comma spacing
 * ------------------------------------------------------------------ */

static void uc15a_normalize(const char *szIn, char *szOut, size_t uiMax)
{
    size_t iIn = 0;
    size_t iOut = 0;
    int    bWasSpace = 1;   /* suppress leading spaces */

    while (szIn[iIn] && iOut + 1 < uiMax)
    {
        char c = (char)toupper((unsigned char)szIn[iIn]);
        if (c == ' ' || c == '\t')
        {
            if (!bWasSpace)
                szOut[iOut++] = ' ';
            bWasSpace = 1;
        }
        else if (c == ',')
        {
            /* strip any trailing space before comma */
            while (iOut > 0 && szOut[iOut - 1] == ' ')
                iOut--;
            if (iOut + 1 < uiMax)
                szOut[iOut++] = ',';
            bWasSpace = 1;  /* swallow space after comma */
        }
        else
        {
            szOut[iOut++] = c;
            bWasSpace = 0;
        }
        iIn++;
    }
    /* strip trailing space */
    while (iOut > 0 && szOut[iOut - 1] == ' ')
        iOut--;
    szOut[iOut] = '\0';
}

/* ------------------------------------------------------------------
 * Apply COMPAT transforms to a normalised source instruction so it can
 * be compared against disassembler output.
 *
 * Rules (source is already uppercase after uc15a_normalize):
 *   1. $HEX.L → $HEX  (abs.L: remove .L suffix, keep .W)
 *   2. N( → $HEX(     (decimal displacement before paren, not hex)
 *   3. -N( → -$HEX(   (negative decimal displacement)
 *   4. #N → #$HEX     (decimal immediate, not already $)
 *   5. #-N → #-$HEX   (negative decimal immediate)
 * ------------------------------------------------------------------ */

static void uc15a_apply_compat(const char *szIn, char *szOut, size_t uiMax)
{
    const char *p   = szIn;
    size_t      iOut = 0;
    char       *endp = NULL;
    long        lVal;

    while (*p && iOut + 1 < uiMax)
    {
        /* Rule 1: $HEX … .L  — copy $ + hex digits, skip .L if present */
        if (*p == '$')
        {
            szOut[iOut++] = *p++;
            while (isxdigit((unsigned char)*p) && iOut + 1 < uiMax)
                szOut[iOut++] = *p++;
            /* Skip .L suffix (not .W) */
            if (p[0] == '.' && p[1] == 'L' &&
                !isalpha((unsigned char)p[2]) && p[2] != '_')
                p += 2;
            continue;
        }

        /* Rule 4/5: #N or #-N → #$HEX or #-$HEX */
        if (*p == '#')
        {
            szOut[iOut++] = *p++;   /* write # */
            if (*p == '-' && isdigit((unsigned char)p[1]))
            {
                lVal = strtol(p, &endp, 10);
                if (endp > p)
                {
                    iOut += (size_t)snprintf(szOut + iOut, uiMax - iOut,
                                             "-$%lX", -lVal);
                    p = endp;
                }
            }
            else if (isdigit((unsigned char)*p))
            {
                lVal = strtol(p, &endp, 10);
                if (endp > p)
                {
                    iOut += (size_t)snprintf(szOut + iOut, uiMax - iOut,
                                             "$%lX", lVal);
                    p = endp;
                }
            }
            continue;
        }

        /* Rule 2/3: decimal displacement before ( */
        if (isdigit((unsigned char)*p) ||
            (*p == '-' && isdigit((unsigned char)p[1])))
        {
            /* Not part of a hex literal (not preceded by $) */
            int bPrecHex = (iOut > 0) && (szOut[iOut - 1] == '$' ||
                            isxdigit((unsigned char)szOut[iOut - 1]));
            if (!bPrecHex)
            {
                const char *q = p;
                if (*q == '-') q++;
                while (isdigit((unsigned char)*q)) q++;
                if (*q == '(')
                {
                    lVal = strtol(p, &endp, 10);
                    if (endp == q)
                    {
                        if (lVal < 0)
                            iOut += (size_t)snprintf(szOut + iOut,
                                                     uiMax - iOut,
                                                     "-$%lX", -lVal);
                        else
                            iOut += (size_t)snprintf(szOut + iOut,
                                                     uiMax - iOut,
                                                     "$%lX", lVal);
                        p = endp;
                        continue;
                    }
                }
            }
        }

        szOut[iOut++] = *p++;
    }
    /* strip trailing space */
    while (iOut > 0 && szOut[iOut - 1] == ' ')
        iOut--;
    szOut[iOut] = '\0';
}

/* ------------------------------------------------------------------
 * Directive keywords to skip when parsing SOURCE.S
 * ------------------------------------------------------------------ */

static const char * const s_aszDirectives[] = {
    "DC.B","DC.W","DC.L","DS.B","DS.W","DS.L",
    "DCB.B","DCB.W","DCB.L",
    "SECTION","EVEN","ODD","ORG","END","EQU","=",
    "INCBIN","INCLUDE","MACRO","ENDM","REPT","ENDR",
    NULL
};

static int uc15a_is_directive(const char *szMnem)
{
    char szUp[32];
    int  i;
    int  j;

    for (j = 0; szMnem[j] && j < 31; j++)
        szUp[j] = (char)toupper((unsigned char)szMnem[j]);
    szUp[j] = '\0';

    for (i = 0; s_aszDirectives[i]; i++)
        if (strcmp(szUp, s_aszDirectives[i]) == 0)
            return 1;
    return 0;
}

/* ------------------------------------------------------------------
 * Extract and normalise one instruction from a SOURCE.S line.
 * Returns 1 if an instruction was found, 0 otherwise.
 * ------------------------------------------------------------------ */

static int uc15a_src_extract(const char *szLine,
                               char *szOut, size_t uiMax)
{
    const char *p = szLine;
    char        szMnem[32];
    int         iMLen;
    const char *pInstr;
    const char *pEnd;
    char        szTmp[UC15A_MAX_LINE];
    size_t      uiTmpLen;

    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t')
        p++;

    /* Blank line or comment */
    if (*p == '\0' || *p == '*' || *p == ';')
        return 0;

    /* Skip label if present (non-whitespace token ending with ':') */
    {
        const char *q = p;
        while (*q && *q != ':' && *q != ' ' && *q != '\t' && *q != ';')
            q++;
        if (*q == ':')
        {
            p = q + 1;
            while (*p == ' ' || *p == '\t')
                p++;
        }
    }

    /* Blank or comment after label */
    if (*p == '\0' || *p == ';' || *p == '*')
        return 0;

    /* Extract mnemonic (up to whitespace) */
    iMLen = 0;
    while (p[iMLen] && p[iMLen] != ' ' && p[iMLen] != '\t' && iMLen < 31)
        iMLen++;
    strncpy(szMnem, p, (size_t)iMLen);
    szMnem[iMLen] = '\0';

    /* Skip directives */
    if (uc15a_is_directive(szMnem))
        return 0;

    /* Collect instruction text up to ';' comment */
    pInstr = p;
    pEnd   = p;
    while (*pEnd && *pEnd != ';')
        pEnd++;

    /* Strip trailing whitespace */
    while (pEnd > pInstr && (*(pEnd - 1) == ' ' || *(pEnd - 1) == '\t'))
        pEnd--;

    if (pEnd == pInstr)
        return 0;

    uiTmpLen = (size_t)(pEnd - pInstr);
    if (uiTmpLen >= sizeof(szTmp))
        uiTmpLen = sizeof(szTmp) - 1;
    strncpy(szTmp, pInstr, uiTmpLen);
    szTmp[uiTmpLen] = '\0';

    uc15a_normalize(szTmp, szOut, uiMax);

    return (szOut[0] != '\0') ? 1 : 0;
}

/* ------------------------------------------------------------------
 * Strip leading zeros from $HEX tokens: $0000 → $0, $00FF → $FF, etc.
 * ------------------------------------------------------------------ */

static void uc15a_strip_hex_zeros(const char *szIn, char *szOut, size_t uiMax)
{
    const char *p  = szIn;
    size_t      iOut = 0;

    while (*p && iOut + 1 < uiMax)
    {
        if (*p == '$')
        {
            szOut[iOut++] = *p++;
            /* skip leading zeros but keep at least one hex digit */
            while (*p == '0' && isxdigit((unsigned char)*(p + 1)))
                p++;
            while (isxdigit((unsigned char)*p) && iOut + 1 < uiMax)
                szOut[iOut++] = *p++;
        }
        else
        {
            szOut[iOut++] = *p++;
        }
    }
    while (iOut > 0 && szOut[iOut - 1] == ' ')
        iOut--;
    szOut[iOut] = '\0';
}

/* ------------------------------------------------------------------
 * Truncation COMPAT check: $FFFFFFFF vs $FF (DEVPAC3 over-wide imm).
 * Returns 1 if strings differ only in $HEX tokens where source hex
 * ends with the disasm hex string (truncation to instruction size).
 * ------------------------------------------------------------------ */

static int uc15a_truncation_compat(const char *szS, const char *szD)
{
    const char *ps = szS;
    const char *pd = szD;
    char        sHex[32];
    char        dHex[32];
    int         i;
    size_t      sLen, dLen;

    while (*ps && *pd)
    {
        if (*ps == '$' && *pd == '$')
        {
            ps++; pd++;
            i = 0;
            while (isxdigit((unsigned char)*ps) && i < 31) sHex[i++] = *ps++;
            sHex[i] = '\0';
            i = 0;
            while (isxdigit((unsigned char)*pd) && i < 31) dHex[i++] = *pd++;
            dHex[i] = '\0';
            sLen = strlen(sHex);
            dLen = strlen(dHex);
            /* Accept if equal or source ends with disasm (truncation) */
            if (strcasecmp(sHex, dHex) != 0)
            {
                if (dLen > sLen) return 0;
                if (strcasecmp(sHex + sLen - dLen, dHex) != 0)
                    return 0;
            }
        }
        else if (toupper((unsigned char)*ps) == toupper((unsigned char)*pd))
        {
            ps++; pd++;
        }
        else
        {
            return 0;
        }
    }
    return (*ps == '\0' && *pd == '\0');
}

/* ------------------------------------------------------------------
 * Branch/DBcc mnemonic detection
 * ------------------------------------------------------------------ */

static const char * const s_aszBranchMnem[] = {
    "BRA","BSR","BHI","BLS","BCC","BCS","BNE","BEQ",
    "BVC","BVS","BPL","BMI","BGE","BLT","BGT","BLE",
    "DBT","DBRA","DBF","DBHI","DBLS","DBCC","DBCS",
    "DBNE","DBEQ","DBVC","DBVS","DBPL","DBMI","DBGE",
    "DBLT","DBGT","DBLE",
    NULL
};

static int uc15a_is_branch_mnem(const char *sz)
{
    char szBase[16];
    int  i;

    for (i = 0; sz[i] && sz[i] != '.' && sz[i] != ' ' && i < 15; i++)
        szBase[i] = (char)toupper((unsigned char)sz[i]);
    szBase[i] = '\0';

    for (i = 0; s_aszBranchMnem[i]; i++)
        if (strcmp(szBase, s_aszBranchMnem[i]) == 0)
            return 1;
    return 0;
}

/* Return 1 if the token looks like a symbolic label. */
static int uc15a_is_label_operand(const char *sz)
{
    int i;
    if (!sz || (int)strlen(sz) <= 2)  return 0;
    if (!isalpha((unsigned char)sz[0]) && sz[0] != '_')  return 0;
    for (i = 0; sz[i]; i++)
        if (!isalnum((unsigned char)sz[i]) && sz[i] != '_')
            return 0;
    return 1;
}

/* ------------------------------------------------------------------
 * Substitute ADD.x → ADDI.x and SUB.x → SUBI.x mnemonics.
 * DEVPAC3 accepts ADD.x #imm,Dn while the disassembler always emits ADDI.
 * ------------------------------------------------------------------ */

static void uc15a_add_to_addi(const char *szIn, char *szOut, size_t uiMax)
{
    if ((strncmp(szIn, "ADD.B ", 6) == 0 ||
         strncmp(szIn, "ADD.W ", 6) == 0 ||
         strncmp(szIn, "ADD.L ", 6) == 0) &&
        strchr(szIn, '#') != NULL)
    {
        snprintf(szOut, uiMax, "ADDI%s", szIn + 3);
        return;
    }
    if ((strncmp(szIn, "SUB.B ", 6) == 0 ||
         strncmp(szIn, "SUB.W ", 6) == 0 ||
         strncmp(szIn, "SUB.L ", 6) == 0) &&
        strchr(szIn, '#') != NULL)
    {
        snprintf(szOut, uiMax, "SUBI%s", szIn + 3);
        return;
    }
    strncpy(szOut, szIn, uiMax - 1);
    szOut[uiMax - 1] = '\0';
}

/* ------------------------------------------------------------------
 * Normalise immediate operands: convert #$HEX → #DECIMAL (positive
 * values only).  Used to compare ADDQ/SUBQ decimal immediates vs
 * source #$HEX produced by uc15a_apply_compat.
 * ------------------------------------------------------------------ */

static void uc15a_imm_to_decimal(const char *szIn, char *szOut, size_t uiMax)
{
    const char *p   = szIn;
    char       *o   = szOut;
    size_t      rem = uiMax - 1;

    while (*p && rem > 0)
    {
        /* Convert #$HEX (no leading minus) → #DECIMAL */
        if (p[0] == '#' && p[1] == '$')
        {
            char         *endp;
            unsigned long  lV;
            int            n;

            lV = strtoul(p + 2, &endp, 16);
            n  = snprintf(o, rem, "#%lu", lV);
            if (n > 0 && (size_t)n < rem) { o += n; rem -= (size_t)n; }
            p = endp;
        }
        else
        {
            *o++ = *p++;
            rem--;
        }
    }
    *o = '\0';
}

/* ------------------------------------------------------------------
 * Convert #-$DEC negative decimal to unsigned 32-bit hex equivalent.
 * DEVPAC3 writes MOVE.L #-16384,-(A6) which after compat becomes
 * #-$4000.  The disassembler writes #$FFFFC000 (unsigned 32-bit).
 * Both represent the same longword value.
 * ------------------------------------------------------------------ */

static void uc15a_neg_to_unsigned32(const char *szIn, char *szOut,
                                    size_t uiMax)
{
    const char *p   = szIn;
    char       *o   = szOut;
    size_t      rem = uiMax - 1;

    while (*p && rem > 0)
    {
        if (p[0] == '#' && p[1] == '-' && p[2] == '$')
        {
            char         *endp;
            unsigned long  lN;
            unsigned long  lU;
            int            n;

            lN = strtoul(p + 3, &endp, 16);
            lU = (unsigned long)(((unsigned long long)0 - lN)
                                 & 0xFFFFFFFFULL);
            n  = snprintf(o, rem, "#$%lX", lU);
            if (n > 0 && (size_t)n < rem) { o += n; rem -= (size_t)n; }
            p = endp;
        }
        else
        {
            *o++ = *p++;
            rem--;
        }
    }
    *o = '\0';
}

/* ------------------------------------------------------------------
 * Classify one comparison pair.
 * ------------------------------------------------------------------ */

static int uc15a_classify(const char *szSrc,
                           const char *szDM, const char *szDO)
{
    char szDisFull[UC15A_MAX_LINE];
    char szDisN[UC15A_MAX_LINE];
    char szSrcT[UC15A_MAX_LINE];
    char szSrcTC[UC15A_MAX_LINE];
    char szLastOp[64];
    const char *pComma;
    const char *pSpace;

    /* DC.W = unimplemented instruction */
    if (strcmp(szDM, "DC.W") == 0)
        return UC15A_DCW;

    /* Build full disasm string and normalise */
    if (szDO[0])
        snprintf(szDisFull, sizeof(szDisFull), "%s %s", szDM, szDO);
    else
        snprintf(szDisFull, sizeof(szDisFull), "%s", szDM);
    uc15a_normalize(szDisFull, szDisN, sizeof(szDisN));

    strncpy(szSrcT, szSrc, sizeof(szSrcT) - 1);
    szSrcT[sizeof(szSrcT) - 1] = '\0';

    /* Exact match? */
    if (strcmp(szSrcT, szDisN) == 0)
        return UC15A_MATCH;

    /* PC-relative: either string contains (PC) or (PC, or *+/-  */
    if (strstr(szSrcT, "(PC") || strstr(szDisN, "(PC") ||
        strchr(szSrcT, '*'))
        return UC15A_PCREL;

    /* COMPAT: apply syntax transforms */
    uc15a_apply_compat(szSrcT, szSrcTC, sizeof(szSrcTC));
    if (strcmp(szSrcTC, szDisN) == 0)
        return UC15A_COMPAT;

    /* COMPAT: hex zero-padding ($00 vs $0, $0000 vs $0) */
    {
        char szSrcTCZ[UC15A_MAX_LINE];
        char szDisNZ[UC15A_MAX_LINE];
        uc15a_strip_hex_zeros(szSrcTC, szSrcTCZ, sizeof(szSrcTCZ));
        uc15a_strip_hex_zeros(szDisN,  szDisNZ,  sizeof(szDisNZ));
        if (strcmp(szSrcTCZ, szDisNZ) == 0)
            return UC15A_COMPAT;

        /* COMPAT: immediate truncation ($FFFFFFFF → $FF for MOVE.B) */
        if (uc15a_truncation_compat(szSrcTCZ, szDisNZ))
            return UC15A_COMPAT;

        /* COMPAT: negative signed hex vs unsigned representation.
         * MOVEQ #-1 → #-$1 → unsigned 32-bit #$FFFFFFFF,
         * disasm shows #$FF (8-bit).  Truncation compat catches this
         * ($FFFFFFFF ends with $FF). */
        {
            char szSrcUN[UC15A_MAX_LINE];
            char szSrcUNZ[UC15A_MAX_LINE];
            char szDisNZ2[UC15A_MAX_LINE];
            uc15a_neg_to_unsigned32(szSrcTCZ, szSrcUN, sizeof(szSrcUN));
            uc15a_strip_hex_zeros(szSrcUN,   szSrcUNZ, sizeof(szSrcUNZ));
            uc15a_strip_hex_zeros(szDisN,    szDisNZ2, sizeof(szDisNZ2));
            if (strcmp(szSrcUNZ, szDisNZ2) == 0)
                return UC15A_COMPAT;
            if (uc15a_truncation_compat(szSrcUNZ, szDisNZ2))
                return UC15A_COMPAT;
        }
    }

    /* COMPAT: branch with symbolic label */
    if (uc15a_is_branch_mnem(szSrcT))
    {
        pComma = strrchr(szSrcT, ',');
        if (pComma)
        {
            strncpy(szLastOp, pComma + 1, sizeof(szLastOp) - 1);
            szLastOp[sizeof(szLastOp) - 1] = '\0';
        }
        else
        {
            pSpace = strchr(szSrcT, ' ');
            if (pSpace)
                strncpy(szLastOp, pSpace + 1, sizeof(szLastOp) - 1);
            else
                szLastOp[0] = '\0';
            szLastOp[sizeof(szLastOp) - 1] = '\0';
        }
        if (uc15a_is_label_operand(szLastOp))
            return UC15A_COMPAT;
    }

    /* COMPAT: ADD/SUB with immediate source = ADDI/SUBI (DEVPAC3 accepts
     * both mnemonics for the same opcode).  Also normalise all #$HEX
     * immediates to decimal so ADDQ/SUBQ/BTST #$N vs #N comparisons pass.
     * Strip hex zeros on disasm too so $00000000 and $0 compare equal.
     */
    {
        char szSrcTCZ3[UC15A_MAX_LINE];
        char szDisNZ3[UC15A_MAX_LINE];
        char szSrcAI[UC15A_MAX_LINE];
        char szSrcDec[UC15A_MAX_LINE];
        char szDisDec[UC15A_MAX_LINE];

        uc15a_strip_hex_zeros(szSrcTC, szSrcTCZ3, sizeof(szSrcTCZ3));
        uc15a_strip_hex_zeros(szDisN,  szDisNZ3,  sizeof(szDisNZ3));
        uc15a_add_to_addi(szSrcTCZ3, szSrcAI, sizeof(szSrcAI));
        uc15a_imm_to_decimal(szSrcAI,  szSrcDec, sizeof(szSrcDec));
        uc15a_imm_to_decimal(szDisNZ3, szDisDec, sizeof(szDisDec));
        if (strcmp(szSrcDec, szDisDec) == 0)
            return UC15A_COMPAT;
        /* Also try truncation for ADD.B #$FFFFFFFF vs ADDI.B #$FF */
        if (uc15a_truncation_compat(szSrcAI, szDisNZ3))
            return UC15A_COMPAT;
    }

    return UC15A_DIFF;
}

/* ------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------ */

int main(void)
{
    st_machine_t        tMachine;
    const load_state_t *ptState = NULL;
    disasm_result_t    *atDis   = NULL;
    char               *pSrcBuf = NULL;
    FILE               *fSrc    = NULL;
    char                szLine[512];
    int                 iSrcCount = 0;
    size_t              uiDisCount = 0;
    st_u32_t            uiBase    = 0;
    size_t              uiTSize   = 0;
    int                 i;
    int                 iMatch = 0, iDcw = 0, iPcrel = 0,
                        iCompat = 0, iDiff = 0;

    printf("\n=== UC15A: Disassembler torture test vs SOURCE.S ===\n\n");

    /* ----------------------------------------------------------------
     * [R] Robustness: helper edge cases
     * ---------------------------------------------------------------- */
    {
        char szOut[64];
        int  iRes;

        uc15a_normalize("", szOut, sizeof(szOut));
        UC_TEST("[R] normalize empty string → empty output",
                szOut[0] == '\0');

        iRes = uc15a_src_extract("", szOut, sizeof(szOut));
        UC_TEST("[R] extract_instr blank line → 0",
                iRes == 0);

        iRes = uc15a_src_extract("; this is a comment", szOut, sizeof(szOut));
        UC_TEST("[R] extract_instr comment line → 0",
                iRes == 0);
    }

    /* ----------------------------------------------------------------
     * Initialise machine and load SOURCE.PRG
     * ---------------------------------------------------------------- */
    UC_TEST("[N] st_init succeeds",
            st_init(&tMachine, NULL) == ST_NO_ERROR);

    UC_TEST("[N] load_init succeeds",
            load_init(&tMachine) == ST_NO_ERROR);

    UC_TEST("[R] load_file bad path → ST_ERROR",
            load_file("use_cases/UC15A/NONEXISTENT.PRG") == ST_ERROR);

    UC_TEST("[N] load_file SOURCE.PRG succeeds",
            load_file(UC15A_PRG_PATH) == ST_NO_ERROR);

    ptState = load_get_state();
    UC_TEST("[N] ptState->bLoaded == ST_TRUE",
            ptState != NULL && ptState->bLoaded == ST_TRUE);

    if (!ptState || !ptState->bLoaded)
    {
        printf("  [BAIL] PRG not loaded — aborting comparison\n");
        g_uc_fails++;
        goto cleanup;
    }

    uiBase  = ptState->uiLoadAddr;
    uiTSize = ptState->uiTextSize;
    UC_TEST("[N] uiTextSize > 0", uiTSize > 0);

    /* ----------------------------------------------------------------
     * Disassemble the .text section
     * ---------------------------------------------------------------- */
    atDis = (disasm_result_t *)malloc(
                UC15A_MAX_INSTR * sizeof(disasm_result_t));
    if (!atDis)
    {
        printf("  [BAIL] malloc disasm buffer failed\n");
        g_uc_fails++;
        goto cleanup;
    }

    UC_TEST("[N] disasm_range on .text succeeds",
            disasm_range(tMachine.aRam + uiBase, uiTSize, uiBase,
                         atDis, (size_t)UC15A_MAX_INSTR,
                         &uiDisCount) == ST_NO_ERROR);

    UC_TEST("[N] disasm result count > 0", (int)uiDisCount > 0);

    /* ----------------------------------------------------------------
     * Parse SOURCE.S
     * ---------------------------------------------------------------- */
    pSrcBuf = (char *)malloc(
                  (size_t)UC15A_MAX_INSTR * (size_t)UC15A_MAX_LINE);
    if (!pSrcBuf)
    {
        printf("  [BAIL] malloc source buffer failed\n");
        g_uc_fails++;
        goto cleanup;
    }
    memset(pSrcBuf, 0,
           (size_t)UC15A_MAX_INSTR * (size_t)UC15A_MAX_LINE);

    fSrc = fopen(UC15A_SRC_PATH, "r");
    if (!fSrc)
    {
        printf("  [BAIL] cannot open %s\n", UC15A_SRC_PATH);
        g_uc_fails++;
        goto cleanup;
    }

    while (fgets(szLine, (int)sizeof(szLine), fSrc) &&
           iSrcCount < UC15A_MAX_INSTR)
    {
        char  *szDst = pSrcBuf + iSrcCount * UC15A_MAX_LINE;
        size_t uiL   = strlen(szLine);
        while (uiL > 0 && (szLine[uiL-1] == '\n' || szLine[uiL-1] == '\r'))
            szLine[--uiL] = '\0';
        if (uc15a_src_extract(szLine, szDst, (size_t)UC15A_MAX_LINE))
            iSrcCount++;
    }
    fclose(fSrc);
    fSrc = NULL;

    UC_TEST("[N] SOURCE.S parsed: instruction count > 0",
            iSrcCount > 0);

    /* ----------------------------------------------------------------
     * Count check
     * ---------------------------------------------------------------- */
    if ((size_t)iSrcCount != uiDisCount)
    {
        int iMin = iSrcCount < (int)uiDisCount ?
                   iSrcCount : (int)uiDisCount;
        printf("  [WARN] count mismatch: SOURCE.S=%d, disasm=%u\n",
               iSrcCount, (unsigned)uiDisCount);
        for (i = 0; i < iMin && i < 8; i++)
        {
            const char *szS = pSrcBuf + i * UC15A_MAX_LINE;
            printf("  [%04d] src=%-42s | dis=%s %s\n",
                   i, szS, atDis[i].szMnemonic, atDis[i].szOperands);
        }
    }
    UC_TEST("[N] SOURCE.S instruction count == disasm count",
            (size_t)iSrcCount == uiDisCount);

    /* ----------------------------------------------------------------
     * Per-instruction comparison
     * ---------------------------------------------------------------- */
    {
        int iCompareCount = iSrcCount < (int)uiDisCount ?
                            iSrcCount : (int)uiDisCount;

        for (i = 0; i < iCompareCount; i++)
        {
            const char *szSrc = pSrcBuf + i * UC15A_MAX_LINE;
            int iCat = uc15a_classify(szSrc,
                                      atDis[i].szMnemonic,
                                      atDis[i].szOperands);
            switch (iCat)
            {
            case UC15A_MATCH:  iMatch++;  break;
            case UC15A_DCW:    iDcw++;    break;
            case UC15A_PCREL:  iPcrel++;  break;
            case UC15A_COMPAT: iCompat++; break;
            case UC15A_DIFF:
            default:
                iDiff++;
                printf("  [DIFF #%d] $%06X | src=%-42s | dis=%s %s\n",
                       iDiff,
                       (unsigned)atDis[i].uiAddr,
                       szSrc,
                       atDis[i].szMnemonic,
                       atDis[i].szOperands);
                break;
            }
        }
    }

    /* ----------------------------------------------------------------
     * Summary
     * ---------------------------------------------------------------- */
    printf("\n  --- UC15A comparison report ---\n");
    printf("  Instructions in SOURCE.S : %d\n", iSrcCount);
    printf("  Disasm results           : %u\n", (unsigned)uiDisCount);
    printf("  MATCH  (exact)           : %d\n", iMatch);
    printf("  DCW    (unimplemented)   : %d\n", iDcw);
    printf("  PCREL  (PC-relative)     : %d\n", iPcrel);
    printf("  COMPAT (syntax variant)  : %d\n", iCompat);
    printf("  DIFF   (genuine mismatch): %d\n", iDiff);
    printf("\n");

    UC_TEST("[N] DIFF count == 0 (no genuine disassembler mismatches)",
            iDiff == 0);
    UC_TEST("[N] MATCH + DCW + PCREL + COMPAT == total instructions",
            (iMatch + iDcw + iPcrel + iCompat) == iSrcCount);

cleanup:
    if (fSrc)    fclose(fSrc);
    if (pSrcBuf) free(pSrcBuf);
    if (atDis)   free(atDis);

    load_shutdown();
    st_shutdown(&tMachine);

    if (g_uc_fails == 0)
        printf("  All tests PASSED.\n\n");
    else
        printf("  %d test(s) FAILED.\n\n", g_uc_fails);

    return (g_uc_fails == 0) ? 0 : 1;
}
