/*
 * load.h - ST4Ever file load into emulated ST RAM
 *
 * Implements the `load` command: detects file type (binary / ATARI ST
 * PRG), reads the content into the emulated st_machine_t RAM, and
 * tracks the loaded state for inspection via `info`.
 *
 * File type detection (by extension, case-insensitive, via file_stat):
 *   .prg / .ttp / .tos  → LOAD_TYPE_PRG  (ATARI ST executable)
 *   .st / .msa / .stx   → rejected (ST_ERROR) — caller prints "use mount"
 *   directory           → rejected (ST_ERROR) — caller prints "use mount"
 *   all other           → LOAD_TYPE_BINARY (verbatim raw copy)
 *
 * PRG loading (UC15 — full implementation):
 *   Validates the 0x601A magic, reads the 28-byte header (text_size,
 *   data_size, bss_size, sym_size, abs_flag), loads .text + .data at
 *   ST_LOAD_BASE (0x0800, just above the 68000 vector table), zeroes
 *   the BSS area, skips the symbol table, then applies the fixup
 *   relocation table when abs_flag == 0.
 *
 * Fixup relocation table format (after symtab):
 *   - First 4 bytes (big-endian): offset from .text start to first fixup.
 *     A value of 0 means "no fixups".
 *   - Then successive bytes until 0x00:
 *       0x00  end of fixup list
 *       0x01  advance current fixup offset by 254 bytes
 *       other N  advance by N bytes, then relocate the longword at
 *                that offset (add ST_LOAD_BASE to it).
 *
 * Binary loading:
 *   Copies file content verbatim to ST RAM at ST_LOAD_BASE.
 *   Files larger than ST_LOAD_MAX_SIZE are rejected.
 *
 * State:
 *   A single global load state (g_load_tState) is managed inside
 *   load.c.  A new successful load_file() call replaces the previous
 *   state.  load_get_state() returns a read-only pointer for `info`.
 *
 * Lifecycle:
 *   main.c calls load_init(ptMachine) after st_init(), then
 *   load_shutdown() before st_shutdown().
 *
 * TODO(UC24): Memory map-aware load address allocation (first free
 *             block above TOS variable area).
 */

#ifndef LOAD_H
#define LOAD_H

#include "common.h"
#include "ST.h"

/* ------------------------------------------------------------------
 * Load address constants
 * ------------------------------------------------------------------ */

#define ST_LOAD_BASE        0x0800u             /* After 68000 vectors  */
#define ST_LOAD_MAX_SIZE    (ST_RAM_SIZE - ST_LOAD_BASE)

/* PRG header */
#define ST_PRG_MAGIC        0x601Au
#define ST_PRG_HEADER_SIZE  28u

/* ------------------------------------------------------------------
 * File type discriminant
 * ------------------------------------------------------------------ */

typedef enum load_type_e
{
    LOAD_TYPE_NONE   = 0,   /* Nothing currently loaded                */
    LOAD_TYPE_BINARY,       /* Raw binary / text (verbatim copy)       */
    LOAD_TYPE_PRG,          /* ATARI ST executable (.prg/.ttp/.tos)    */
} load_type_t;

/* ------------------------------------------------------------------
 * Load state (read-only via load_get_state)
 * ------------------------------------------------------------------ */

typedef struct load_state_s
{
    st_bool_t    bLoaded;                /* ST_TRUE if a file is loaded */
    load_type_t  eType;                  /* File type discriminant      */
    char         szPath[ST_MAX_PATH];    /* Full path of loaded file    */
    st_u32_t     uiLoadAddr;             /* ST RAM address of content   */
    st_u32_t     uiSize;                 /* Total bytes in RAM          */
                                         /*  (text+data+bss for PRG)    */
    /* PRG-specific fields (0 for LOAD_TYPE_BINARY) */
    st_u32_t     uiTextSize;             /* .text section size (bytes)  */
    st_u32_t     uiDataSize;             /* .data section size (bytes)  */
    st_u32_t     uiBssSize;              /* .bss  section size (bytes)  */
    st_u32_t     uiFixupCount;           /* Number of fixups applied    */
} load_state_t;

/* ------------------------------------------------------------------
 * API — lifecycle
 * ------------------------------------------------------------------ */

/*
 * load_init() - Attach the load module to an ST machine instance.
 *
 * Must be called before load_file().  Clears any previous state and
 * stores the machine pointer for subsequent load operations.
 *
 * Parameters:
 *   ptMachine [in] : Initialised ST machine (must not be NULL).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if ptMachine is NULL.
 */
st_error_t load_init(st_machine_t *ptMachine);

/*
 * load_shutdown() - Detach the load module and reset state.
 *
 * After return the internal machine pointer is NULL and bLoaded is
 * ST_FALSE.
 *
 * Returns:
 *   ST_NO_ERROR always.
 */
st_error_t load_shutdown(void);

/* ------------------------------------------------------------------
 * API — load operation
 * ------------------------------------------------------------------ */

/*
 * load_file() - Load a file into emulated ST RAM.
 *
 * Detects the file type via file_stat(), then:
 *   - directory / disk image → ST_ERROR (caller should show user msg)
 *   - .prg/.ttp/.tos        → full PRG load with fixup relocation
 *   - all other             → copies file content verbatim
 *
 * On success the internal load state is updated and can be read via
 * load_get_state().
 *
 * Parameters:
 *   szPath [in] : Path to the file (must not be NULL).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if szPath is NULL, load_init() has not been called,
 *               the file does not exist, is too large, or has an
 *               invalid PRG magic.
 */
st_error_t load_file(const char *szPath);

/* ------------------------------------------------------------------
 * API — state query
 * ------------------------------------------------------------------ */

/*
 * load_get_state() - Return a pointer to the current load state.
 *
 * The pointer is valid for the lifetime of the module (until
 * load_shutdown() is called).  Never returns NULL.
 *
 * Returns:
 *   Read-only pointer to the internal load_state_t.
 */
const load_state_t *load_get_state(void);

#endif /* LOAD_H */
