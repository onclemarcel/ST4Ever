/*
 * use_cases.h - ST4Ever common include for all use case test programs
 *
 * Each use_case_NN.c file includes this header so it gets all the
 * module headers and standard I/O without repeating includes.
 *
 * Test programs are stand-alone executables compiled by the Makefile
 * against the full library objects (excluding main.o).
 */

#ifndef USE_CASES_H
#define USE_CASES_H

/* Standard library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ST4Ever portable layer */
#include "../src/common.h"
#include "../src/trace.h"
#include "../src/console.h"
#include "../src/line.h"
#include "../src/gui.h"
#include "../src/renderer.h"
#include "../src/ST.h"
#include "../src/CPU.h"
#include "../src/disassemble.h"
#include "../src/as.h"
#include "../src/dir.h"
#include "../src/mount.h"
#include "../src/image_st.h"
#include "../src/edit_txt.h"
#include "../src/edit_hex.h"
#include "../src/edit.h"
#include "../src/exec.h"
#include "../src/exec_mon.h"
#include "../src/exec_mem.h"
#include "../src/exec_cpu.h"
#include "../src/exec_asm.h"
#include "../src/exec_screen.h"
#include "../src/sector_analyze.h"
#include "../src/image_annot.h"

/* ------------------------------------------------------------------
 * Test helper macros
 * ------------------------------------------------------------------ */

/* Print a test result line */
#define TEST_ASSERT(desc, cond) \
    do { \
        if (cond) { \
            printf("  [PASS] %s\n", (desc)); \
        } else { \
            printf("  [FAIL] %s  (line %d)\n", (desc), __LINE__); \
            g_uc_fails++; \
        } \
    } while (0)

#define UC_TEST(desc, cond) \
    do { \
        if (cond) { \
            printf("  [PASS] %s\n", (desc)); \
        } else { \
            printf("  [FAIL] %s  (line %d)\n", (desc), __LINE__); \
            g_uc_fails++; \
        } \
    } while (0)

/* Call an ST4Ever function and check ST_NO_ERROR */
#define UC_CHECK(desc, call) \
    do { \
        st_error_t _e = (call); \
        UC_TEST(desc, _e == ST_NO_ERROR); \
    } while (0)

/* Skip a test with a reason - does not count as failure */
#define TEST_SKIP(desc) \
    printf("  [SKIP] %s\n", (desc))

/*
 * TEST_MANUAL(desc, question) - Visual/interactive test (R16).
 *
 * In manual mode (-DST_MANUAL_TEST via 'make manual'): displays the
 * description and asks the user y/n.  In automated mode: prints [SKIP].
 *
 * Usage:
 *   TEST_MANUAL("[S] Window title updated", "Is the title correct?");
 */
#ifdef ST_MANUAL_TEST
#define TEST_MANUAL(desc, question) \
    do { \
        int _c; \
        printf("  [MANUAL] %s\n  > %s (y/n): ", (desc), (question)); \
        fflush(stdout); \
        _c = getchar(); \
        while (getchar() != '\n'); \
        if (_c == 'y' || _c == 'Y') \
        { \
            printf("  [PASS]  %s\n", (desc)); \
        } \
        else \
        { \
            printf("  [FAIL]  %s\n", (desc)); \
            g_uc_fails++; \
        } \
    } while (0)
#else
#define TEST_MANUAL(desc, question) \
    printf("  [SKIP] %s (run make manual)\n", (desc))
#endif

/*
 * ST_TEST_LEVEL_UCNN is defined by the Makefile (-DST_TEST_LEVEL_UC01,
 * -DST_TEST_LEVEL_UC03, ...) when compiling each use_case_NN binary.
 * Use it to guard assertions that depend on stub vs real behaviour:
 *
 *   #ifdef ST_TEST_LEVEL_UC01
 *       UC_CHECK("[N] gui_init stub", gui_init());
 *   #else
 *       TEST_SKIP("[N] gui_init - requires display, tested in UC3");
 *   #endif
 */

/* Global fail counter - define in each use_case_NN.c */
extern int g_uc_fails;

#endif /* USE_CASES_H */
