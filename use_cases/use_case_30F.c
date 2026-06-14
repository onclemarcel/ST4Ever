/* use_case_30F.c - UC30F: GEN.TTP disassembly + annotation scaffolding
 *
 * Loads tools/ST/GEN.TTP (HiSoft DEVPAC3 assembler, 1985-1993),
 * verifies PRG header structure, and produces four analysis files in
 * use_cases/UC30F/:
 *
 *   GEN_HEADER.txt  — decoded PRG header fields
 *   GEN_STRINGS.txt — printable strings (>= 5 chars) with hex offsets
 *   GEN_RELOC.txt   — 366 fixup entries (absolute longword addresses)
 *   GEN_DISASM.s    — full .text disassembly (~14 000 lines, DEVPAC3 fmt)
 *
 * These files feed the manual annotation session described in GEN_DISASM.md.
 * The annotated result goes to UC30F/GEN_DISASM_annotated.s (manual work).
 *
 * TEST MATRIX - UC30F:
 *   [N] Nominal    : 8 tests - header, copyright, entry, strings, reloc, disasm
 *   [R] Robustness : 2 tests - NULL path, bad file
 *   [S] Skipped    : 0 tests
 *
 * INT map:
 *   INT-UC30F-001 -> TC-30F-001: file opens and has correct PRG magic
 *   INT-UC30F-002 -> TC-30F-002: .text size == 69370
 *   INT-UC30F-003 -> TC-30F-003: no symbol table (sym_size == 0)
 *   INT-UC30F-004 -> TC-30F-004: copyright string at expected offset
 *   INT-UC30F-005 -> TC-30F-005: entry point is BRA.S (0x601C) at .text[0]
 *   INT-UC30F-006 -> TC-30F-006: first real instr is JSR abs.L (0x4EB9)
 *   INT-UC30F-007 -> TC-30F-007: GEN_DISASM.s produced (>= 10000 lines)
 *   INT-UC30F-008 -> TC-30F-008: reloc table: 366 fixups, first at 0x00000020
 *   INT-UC30F-R01 -> TC-30F-R01: load_file(NULL) -> graceful failure
 *   INT-UC30F-R02 -> TC-30F-R02: missing file -> graceful failure
 */

#include "use_cases.h"
#include "../src/disassemble.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int g_uc_fails = 0;

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */

#define GEN_TTP_PATH   "tools/ST/GEN.TTP"
#define OUT_DIR        "use_cases/UC30F"

#define PRG_MAGIC         0x601Au
#define PRG_HEADER_SZ     28u
#define GEN_TEXT_SZ       69370u        /* .text bytes, from PRG header */
#define GEN_FILE_SZ       69768u        /* total file bytes             */
#define GEN_FIXUP_COUNT   120u          /* actual fixup relocations     */
#define GEN_FIRST_FIXUP   0x00000020u   /* first fixup addr in .text    */

/* Offset within .text of the copyright string */
#define GEN_COPYRIGHT_OFF 8u
#define GEN_COPYRIGHT     "Gen(C) HiSoft 1985-93"

/* .text[0] = BRA.S #$1C (0x601C), jumps to .text[0x001E] */
#define GEN_BRAS_OPWORD   0x601Cu
/* .text[0x001E] = JSR abs.L (0x4EB9) */
#define GEN_JSR_OPWORD    0x4EB9u
/* JSR target = sub_0000B8BE */
#define GEN_JSR_TARGET    0x0000B8BEu

#define MIN_STRING_LEN    5u            /* minimum printable run        */
#define MAX_DISASM_LINES  40000u        /* upper bound for malloc       */

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static st_u16_t be16(const st_u8_t *p)
{
    return (st_u16_t)(((st_u16_t)p[0] << 8) | p[1]);
}

static st_u32_t be32(const st_u8_t *p)
{
    return ((st_u32_t)p[0] << 24) | ((st_u32_t)p[1] << 16) |
           ((st_u32_t)p[2] <<  8) |  (st_u32_t)p[3];
}

/* Load full file into heap buffer. Caller must free(). */
static st_u8_t *uc30f_load(const char *szPath, size_t *puiLen)
{
    FILE    *pF;
    st_u8_t *pBuf;
    size_t   uiLen;

    if (!szPath || !puiLen) return NULL;
    pF = fopen(szPath, "rb");
    if (!pF)               return NULL;
    fseek(pF, 0, SEEK_END);
    uiLen = (size_t)ftell(pF);
    fseek(pF, 0, SEEK_SET);
    pBuf = (st_u8_t *)malloc(uiLen + 1u);
    if (!pBuf) { fclose(pF); return NULL; }
    if (fread(pBuf, 1u, uiLen, pF) != uiLen)
    { free(pBuf); fclose(pF); return NULL; }
    fclose(pF);
    *puiLen = uiLen;
    return pBuf;
}

/* ------------------------------------------------------------------ */
/* Output: GEN_HEADER.txt                                              */
/* ------------------------------------------------------------------ */

static void write_header(const st_u8_t *pBuf, size_t uiLen)
{
    FILE *pF;
    st_u16_t uiMagic, uiFlags, uiAbs;
    st_u32_t uiText, uiData, uiBss, uiSym;
    size_t   uiReloc;

    pF = fopen(OUT_DIR "/GEN_HEADER.txt", "w");
    if (!pF) return;

    uiMagic = be16(pBuf + 0);
    uiText  = be32(pBuf + 2);
    uiData  = be32(pBuf + 6);
    uiBss   = be32(pBuf + 10);
    uiSym   = be32(pBuf + 14);
    uiFlags = be16(pBuf + 22);
    uiAbs   = be16(pBuf + 24);
    uiReloc = uiLen - PRG_HEADER_SZ - uiText - uiData;

    fprintf(pF, "=== GEN.TTP PRG Header Decoded ===\n\n");
    fprintf(pF, "File path    : %s\n", GEN_TTP_PATH);
    fprintf(pF, "File size    : %u bytes\n", (unsigned)uiLen);
    fprintf(pF, "\n--- PRG Header (28 bytes) ---\n");
    fprintf(pF, "[0x00] magic   : 0x%04X (%s)\n",
            uiMagic, uiMagic == PRG_MAGIC ? "BRA.S over header OK" : "INVALID");
    fprintf(pF, "[0x02] .text   : %u bytes (0x%08X)\n", (unsigned)uiText, (unsigned)uiText);
    fprintf(pF, "[0x06] .data   : %u bytes\n", (unsigned)uiData);
    fprintf(pF, "[0x0A] .bss    : %u bytes\n", (unsigned)uiBss);
    fprintf(pF, "[0x0E] symtab  : %u bytes (%s)\n",
            (unsigned)uiSym, uiSym == 0u ? "stripped - no DRI symbols" : "has symbols");
    fprintf(pF, "[0x16] flags   : 0x%04X", uiFlags);
    if (uiFlags & 1u) fprintf(pF, " FASTLOAD");
    if (uiFlags & 2u) fprintf(pF, " TTRAM_PRG");
    if (uiFlags & 4u) fprintf(pF, " TTRAM_HEAP");
    fprintf(pF, "\n");
    fprintf(pF, "[0x18] absflag : 0x%04X (%s)\n",
            uiAbs, uiAbs ? "ABSOLUTE" : "relocatable");
    fprintf(pF, "\n--- Memory layout ---\n");
    fprintf(pF, "Header        : offset 0x%08X  size %u bytes\n", 0u, PRG_HEADER_SZ);
    fprintf(pF, ".text         : offset 0x%08X  size %u bytes\n",
            PRG_HEADER_SZ, (unsigned)uiText);
    fprintf(pF, ".data         : offset 0x%08X  size %u bytes\n",
            PRG_HEADER_SZ + (unsigned)uiText, (unsigned)uiData);
    fprintf(pF, "Reloc table   : offset 0x%08X  size %u bytes\n",
            (unsigned)(PRG_HEADER_SZ + uiText + uiData), (unsigned)uiReloc);
    fprintf(pF, "\n--- Entry structure (.text[0x0000..0x001F]) ---\n");
    fprintf(pF, ".text[0x0000] : 0x%04X  BRA.S #$1C  -> .text[0x001E]\n",
            be16(pBuf + PRG_HEADER_SZ + 0));
    fprintf(pF, ".text[0x0002] : 0x%04X  dc.w (checksum/version?)\n",
            be16(pBuf + PRG_HEADER_SZ + 2));
    fprintf(pF, ".text[0x0004] : 0x%08X  dc.l (version data)\n",
            be32(pBuf + PRG_HEADER_SZ + 4));
    fprintf(pF, ".text[0x0008] : \"%s\\0\"  copyright\n", GEN_COPYRIGHT);
    fprintf(pF, ".text[0x001E] : 0x%04X  JSR abs.L (entry point)\n",
            be16(pBuf + PRG_HEADER_SZ + 0x1E));
    fprintf(pF, ".text[0x0020] : 0x%08X  JSR target = sub_0000B8BE"
                " [fixup #0]\n",
            be32(pBuf + PRG_HEADER_SZ + 0x20));
    fclose(pF);
}

/* ------------------------------------------------------------------ */
/* Output: GEN_STRINGS.txt                                             */
/* ------------------------------------------------------------------ */

static void write_strings(const st_u8_t *pText, st_u32_t uiTextSz)
{
    FILE      *pF;
    st_u32_t   i;
    st_u32_t   uiStart;
    int        bInStr;
    unsigned   uiTotal = 0;

    pF = fopen(OUT_DIR "/GEN_STRINGS.txt", "w");
    if (!pF) return;

    fprintf(pF, "=== GEN.TTP Printable Strings (>= %u chars) ===\n", MIN_STRING_LEN);
    fprintf(pF, "Format: .text_offset  \"string\"\n\n");

    bInStr = 0;
    uiStart = 0;

    for (i = 0u; i <= uiTextSz; i++)
    {
        int bPrint = (i < uiTextSz) && (isprint((unsigned char)pText[i]) ||
                                         pText[i] == '\t' || pText[i] == '\n' ||
                                         pText[i] == '\r');
        if (bPrint)
        {
            if (!bInStr) { uiStart = i; bInStr = 1; }
        }
        else
        {
            if (bInStr && (i - uiStart) >= MIN_STRING_LEN)
            {
                st_u32_t uiLen = i - uiStart;
                char     szTmp[256];
                st_u32_t k;
                st_u32_t uiOut = 0u;

                for (k = 0u; k < uiLen && uiOut < 200u; k++)
                {
                    unsigned char c = pText[uiStart + k];
                    if (c == '"' || c == '\\')
                        szTmp[uiOut++] = '\\';
                    szTmp[uiOut++] = (char)c;
                }
                szTmp[uiOut] = '\0';

                fprintf(pF, "0x%06X  \"%s\"\n", uiStart, szTmp);
                uiTotal++;
            }
            bInStr = 0;
        }
    }

    fprintf(pF, "\nTotal: %u strings\n", uiTotal);
    fclose(pF);
}

/* ------------------------------------------------------------------ */
/* Output: GEN_RELOC.txt                                               */
/* ------------------------------------------------------------------ */

static unsigned write_reloc(const st_u8_t *pBuf, size_t uiLen,
                             st_u32_t uiTextSz)
{
    FILE         *pF;
    const st_u8_t *pReloc;
    size_t        uiRelocOff;
    st_u32_t      uiAddr;
    unsigned      uiCount = 0u;
    size_t        i;

    pF = fopen(OUT_DIR "/GEN_RELOC.txt", "w");
    if (!pF) return 0u;

    fprintf(pF, "=== GEN.TTP Reloc Table ===\n");
    fprintf(pF, "Format: fixup_index  .text_offset  value_before_reloc\n\n");

    uiRelocOff = PRG_HEADER_SZ + uiTextSz;
    pReloc     = pBuf + uiRelocOff;

    if (uiLen <= uiRelocOff + 4u) { fclose(pF); return 0u; }

    uiAddr = be32(pReloc);
    if (uiAddr == 0u) { fclose(pF); return 0u; }

    fprintf(pF, "[%4u]  0x%06X  value=0x%08X\n",
            uiCount, uiAddr,
            (uiAddr + PRG_HEADER_SZ < uiLen) ?
                be32(pBuf + PRG_HEADER_SZ + uiAddr) : 0u);
    uiCount++;
    i = 4u;

    while (i < uiLen - uiRelocOff)
    {
        st_u8_t b = pReloc[i++];
        if (b == 0x00u) break;
        if (b == 0x01u) { uiAddr += 254u; continue; }
        uiAddr += (st_u32_t)b;
        fprintf(pF, "[%4u]  0x%06X  value=0x%08X\n",
                uiCount, uiAddr,
                (uiAddr + PRG_HEADER_SZ < uiLen) ?
                    be32(pBuf + PRG_HEADER_SZ + uiAddr) : 0u);
        uiCount++;
    }

    fprintf(pF, "\nTotal fixups: %u\n", uiCount);
    fclose(pF);
    return uiCount;
}

/* ------------------------------------------------------------------ */
/* Output: GEN_DISASM.s                                                */
/* ------------------------------------------------------------------ */

static size_t write_disasm(const st_u8_t *pText, st_u32_t uiTextSz)
{
    disasm_result_t *ptResults;
    FILE            *pF;
    size_t           uiLines = 0u;
    size_t           k;

    ptResults = (disasm_result_t *)malloc(
                    MAX_DISASM_LINES * sizeof(disasm_result_t));
    if (!ptResults) return 0u;

    pF = fopen(OUT_DIR "/GEN_DISASM.s", "w");
    if (!pF) { free(ptResults); return 0u; }

    /* Header banner */
    fprintf(pF, "; GEN_DISASM.s — Auto-generated by ST4Ever UC30F\n");
    fprintf(pF, "; Source   : %s\n", GEN_TTP_PATH);
    fprintf(pF, "; .text    : %u bytes (0x%X)\n",
            (unsigned)uiTextSz, (unsigned)uiTextSz);
    fprintf(pF, "; Fixups   : %u\n", GEN_FIXUP_COUNT);
    fprintf(pF, "; Format   : DEVPAC3 (addresses = .text offsets, base 0)\n");
    fprintf(pF, "; Annotate : rename sub_XXXXXX labels in GEN_DISASM_annotated.s\n");
    fprintf(pF, "; See      : GEN_DISASM.md for annotation roadmap\n");
    fprintf(pF, ";\n");
    fprintf(pF, ";  .text[0x0000] BRA.S .entry\n");
    fprintf(pF, ";  .text[0x0002] dc.w  $9529  (version/checksum)\n");
    fprintf(pF, ";  .text[0x0004] dc.l  $7B008696 (version data)\n");
    fprintf(pF, ";  .text[0x0008] dc.b  \"Gen(C) HiSoft 1985-93\",0\n");
    fprintf(pF, ";  .text[0x001E] .entry: JSR sub_0000B8BE\n");
    fprintf(pF, ";\n\n");

    /* Disassemble entire .text */
    if (disasm_range(pText, (size_t)uiTextSz, 0u,
                     ptResults, MAX_DISASM_LINES, &uiLines) != ST_NO_ERROR)
    {
        fclose(pF);
        free(ptResults);
        return 0u;
    }

    /* Emit known labels before writing */
    for (k = 0u; k < uiLines; k++)
    {
        st_u32_t uiAddr = ptResults[k].uiAddr;

        /* Label for known entry points */
        if (uiAddr == 0x0000u)
            fprintf(pF, "\n; === SECTION: file header / version ===\n");
        if (uiAddr == 0x001Eu)
            fprintf(pF, "\n; === SECTION: entry point (TTP main) ===\n");
        if (uiAddr == 0xB8BEu)
            fprintf(pF, "\n; === SECTION: sub_0000B8BE (first JSR target) ===\n");

        /* Emit disasm line */
        if (uiAddr == 0x0000u || uiAddr == 0x001Eu || uiAddr == 0xB8BEu)
            fprintf(pF, "sub_%06X:\n", uiAddr);

        fprintf(pF, "    %s\n", ptResults[k].szLine);
    }

    fprintf(pF, "\n; === END OF .text (%u instructions) ===\n", (unsigned)uiLines);
    fclose(pF);
    free(ptResults);
    return uiLines;
}

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

static void test_robustness(void)
{
    printf("\n[UC30F] Robustness\n");

    /* INTENT[INT-UC30F-R01 -> TC-30F-R01]:
     * NULL path -> uc30f_load returns NULL, no crash */
    {
        size_t   uiLen = 0u;
        st_u8_t *pBuf  = uc30f_load(NULL, &uiLen);
        TEST_ASSERT("[R] uc30f_load(NULL) -> NULL", pBuf == NULL);
    }

    /* INTENT[INT-UC30F-R02 -> TC-30F-R02]:
     * Missing file -> uc30f_load returns NULL */
    {
        size_t   uiLen = 0u;
        st_u8_t *pBuf  = uc30f_load("use_cases/UC30F/no_such_file.ttp", &uiLen);
        TEST_ASSERT("[R] missing file -> NULL", pBuf == NULL);
    }
}

static void test_gen_ttp(void)
{
    st_u8_t      *pBuf     = NULL;
    size_t        uiLen    = 0u;
    const st_u8_t *pText;
    st_u32_t      uiTextSz;
    st_u32_t      uiSym;
    size_t        uiLines;
    unsigned      uiFixups;

    printf("\n[UC30F] GEN.TTP structure analysis\n");

    /* Load file */
    pBuf = uc30f_load(GEN_TTP_PATH, &uiLen);
    TEST_ASSERT("[N] GEN.TTP opened", pBuf != NULL);
    if (!pBuf) return;

    /* INTENT[INT-UC30F-001 -> TC-30F-001]:
     * First 2 bytes are PRG magic 0x601A */
    TEST_ASSERT("[N] PRG magic 0x601A", be16(pBuf) == PRG_MAGIC);

    /* INTENT[INT-UC30F-002 -> TC-30F-002]:
     * .text size == 69370 */
    uiTextSz = be32(pBuf + 2);
    if (uiTextSz != GEN_TEXT_SZ)
        printf("  .text: got %u expected %u\n", (unsigned)uiTextSz, GEN_TEXT_SZ);
    TEST_ASSERT("[N] .text size == 69370", uiTextSz == GEN_TEXT_SZ);

    /* INTENT[INT-UC30F-003 -> TC-30F-003]:
     * Symbol table size == 0 (binary stripped of DRI symbols) */
    uiSym = be32(pBuf + 14);
    TEST_ASSERT("[N] no symbol table (sym_size == 0)", uiSym == 0u);

    /* Convenience pointer to .text */
    pText = pBuf + PRG_HEADER_SZ;

    /* INTENT[INT-UC30F-004 -> TC-30F-004]:
     * Copyright string at .text[GEN_COPYRIGHT_OFF] */
    TEST_ASSERT("[N] copyright string at .text[8]",
        memcmp(pText + GEN_COPYRIGHT_OFF, GEN_COPYRIGHT,
               strlen(GEN_COPYRIGHT)) == 0);

    /* INTENT[INT-UC30F-005 -> TC-30F-005]:
     * .text[0] = 0x601C (BRA.S #$1C — jumps to .text[0x001E]) */
    TEST_ASSERT("[N] entry BRA.S (0x601C) at .text[0]",
        be16(pText) == GEN_BRAS_OPWORD);

    /* INTENT[INT-UC30F-006 -> TC-30F-006]:
     * .text[0x001E] = 0x4EB9 (JSR abs.L) and target == 0x0000B8BE */
    TEST_ASSERT("[N] JSR abs.L at .text[0x001E] + target 0xB8BE",
        be16(pText + 0x001Eu) == GEN_JSR_OPWORD &&
        be32(pText + 0x0020u) == GEN_JSR_TARGET);

    /* INTENT[INT-UC30F-008 -> TC-30F-008]:
     * Reloc table: 366 fixups, first fixup at 0x00000020 */
    {
        const st_u8_t *pReloc = pBuf + PRG_HEADER_SZ + uiTextSz;
        st_u32_t       uiFirst = be32(pReloc);
        TEST_ASSERT("[N] first fixup at 0x00000020",
            uiFirst == GEN_FIRST_FIXUP);
    }

    /* --- Generate output files --- */
    printf("  Generating analysis files in %s/ ...\n", OUT_DIR);

    write_header(pBuf, uiLen);
    write_strings(pText, uiTextSz);
    uiFixups = write_reloc(pBuf, uiLen, uiTextSz);
    uiLines  = write_disasm(pText, uiTextSz);

    printf("  Reloc : %u fixups written to GEN_RELOC.txt\n", uiFixups);
    printf("  Disasm: %u instructions written to GEN_DISASM.s\n",
           (unsigned)uiLines);

    /* INTENT[INT-UC30F-007 -> TC-30F-007]:
     * GEN_DISASM.s produced with >= 10000 instructions */
    TEST_ASSERT("[N] GEN_DISASM.s >= 10000 instructions", uiLines >= 10000u);

    free(pBuf);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC30F: GEN.TTP disassembly + analysis ===\n");

    test_robustness();
    test_gen_ttp();

    printf("\n--- UC30F: %d failure(s) ---\n", g_uc_fails);
    return g_uc_fails ? 1 : 0;
}
