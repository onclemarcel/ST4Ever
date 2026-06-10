/*
 * console.h - ST4Ever portable raw console input API
 *
 * Provides single-keystroke input for the rich line editor (UC4.2).
 * Decouples key reading from line editor logic.
 *
 * Strategy (R5):
 *   console_set_raw() attempts termios first (MSYS2/mintty + Linux).
 *   On cmd.exe it falls back to Win32 ReadConsoleInput.
 *
 * Platform implementations:
 *   win/win_console.c  - termios (MSYS2/mintty) + Win32 (cmd.exe)
 *   linux/lx_console.c - termios
 *
 * Key value encoding:
 *   Printable ASCII 0x20..0x7E  : returned as raw byte value.
 *   Control codes   0x01..0x1F  : returned as raw byte value.
 *   0x7F (BACKSPACE)            : normalised delete-before-cursor.
 *   Extended keys   >= 0x200    : CON_KEY_* constants below.
 *   CON_KEY_EOF (-1)            : stdin closed or fatal read error.
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include "common.h"

/* ------------------------------------------------------------------
 * Control character aliases (raw byte values sent by CTRL + letter)
 * ------------------------------------------------------------------ */

#define CON_KEY_CTRL_C     0x03   /* Cancel / quit shortcut            */
#define CON_KEY_CTRL_D     0x04   /* Dir shortcut                      */
#define CON_KEY_CTRL_E     0x05   /* Edit shortcut                     */
#define CON_KEY_CTRL_H     0x08   /* Help (mintty: distinct from BS)   */
#define CON_KEY_CTRL_L     0x0C   /* Load shortcut                     */
#define CON_KEY_ENTER      0x0D   /* ENTER / CR  (also CTRL+M)         */
#define CON_KEY_CTRL_Q     0x11   /* Quit shortcut                     */
#define CON_KEY_CTRL_T     0x14   /* Trace shortcut                    */
#define CON_KEY_CTRL_O     0x0F   /* Mount shortcut (UC24E P52)        */
#define CON_KEY_CTRL_W     0x17   /* Where shortcut                    */
#define CON_KEY_CTRL_X     0x18   /* Execute shortcut                  */
#define CON_KEY_TAB        0x09   /* Tab / CTRL+I  (completion UC4.3)  */
#define CON_KEY_ESC        0x1B   /* ESC - clear line                  */
#define CON_KEY_BACKSPACE  0x7F   /* Backspace (delete before cursor)  */

/* ------------------------------------------------------------------
 * Extended key codes (>= 0x200 — no overlap with ASCII / control)
 * ------------------------------------------------------------------ */

#define CON_KEY_UP         0x200
#define CON_KEY_DOWN       0x201
#define CON_KEY_LEFT       0x202
#define CON_KEY_RIGHT      0x203
#define CON_KEY_HOME       0x204
#define CON_KEY_END        0x205
#define CON_KEY_DELETE     0x206
#define CON_KEY_PAGE_UP    0x207
#define CON_KEY_PAGE_DOWN  0x208
#define CON_KEY_F1         0x210
#define CON_KEY_F5         0x214   /* Dir refresh (P22, UC5)           */
#define CON_KEY_F12        0x21B

/* EOF / fatal read error sentinel */
#define CON_KEY_EOF        (-1)

/* Timeout sentinel: no key within the platform poll window (200 ms).
 * console_read_key() returns ST_NO_ERROR + CON_KEY_TIMEOUT when no
 * keystroke arrived.  The caller (line_read_rich) uses this to
 * refresh the contextual prompt without changing the editor state. */
#define CON_KEY_TIMEOUT    (-2)

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * console_set_raw() - Switch stdin to raw (non-canonical) mode.
 *
 * Disables line buffering, echo and signal generation so that key
 * events arrive one byte at a time.  Must be matched by a call to
 * console_restore() before the process exits.
 *
 * Returns:
 *   ST_NO_ERROR  stdin is now in raw mode.
 *   ST_ERROR     both termios and Win32 raw mode failed (not a TTY).
 */
st_error_t console_set_raw(void);

/*
 * console_restore() - Restore the original console mode.
 *
 * Idempotent: safe to call even if console_set_raw() was not called
 * or returned ST_ERROR.
 *
 * Returns:
 *   ST_NO_ERROR  mode restored (or nothing was active).
 *   ST_ERROR     system call failure during restore.
 */
st_error_t console_restore(void);

/*
 * console_read_key() - Read one key event from stdin, blocking.
 *
 * Decodes raw bytes / VT100 escape sequences / Win32 virtual keys
 * into a portable CON_KEY_* value or printable ASCII.
 *
 * Parameters:
 *   piKey [out] : Receives CON_KEY_* or printable char (0x20..0x7E).
 *                 Set to CON_KEY_EOF on stdin close or fatal error.
 *
 * Returns:
 *   ST_NO_ERROR  key decoded; *piKey is a printable char, CON_KEY_*,
 *                or CON_KEY_TIMEOUT (no keystroke within 200 ms).
 *   ST_ERROR     read failure; *piKey set to CON_KEY_EOF.
 */
st_error_t console_read_key(int *piKey);

#endif /* CONSOLE_H */
