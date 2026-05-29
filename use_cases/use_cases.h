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
#include "../src/line.h"
#include "../src/gui.h"
#include "../src/renderer.h"
#include "../src/ST.h"
#include "../src/CPU.h"
#include "../src/disassemble.h"
#include "../src/as.h"
#include "../src/dir.h"
#include "../src/mount.h"
#include "../src/edit_txt.h"
#include "../src/edit_hex.h"
#include "../src/edit.h"
#include "../src/exec.h"
#include "../src/exec_mon.h"
#include "../src/exec_mem.h"
#include "../src/exec_cpu.h"
#include "../src/exec_asm.h"
#include "../src/exec_screen.h"

/* ------------------------------------------------------------------
 * Test helper macros
 * ------------------------------------------------------------------ */

/* Print a test result line */
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

/* Global fail counter - define in each use_case_NN.c */
extern int g_uc_fails;

#endif /* USE_CASES_H */
