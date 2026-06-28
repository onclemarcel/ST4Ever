/*
 * win.h - Windows-specific functions
 *
 * This include file declares the context structure of the win_xxx.c
 * files
 * 
 */

#ifndef WIN_H
#define WIN_H

#include "../src/common.h"

#ifdef ST_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#endif /* ST_PLATFORM_WINDOWS */

/* ------------------------------------------------------------------
 * Win Console Context - types & enums are 'translated' for ST4Ever
 * ------------------------------------------------------------------ */
typedef enum win_stdin_type_e
{
    WIN_STDIN_UNKNOWN = 0,
    WIN_STDIN_PIPE,     /* mintty / MSYS2 — pipe, VT100 byte stream   */
    WIN_STDIN_CONSOLE   /* cmd.exe — Win32 console, VK event stream   */
} win_stdin_type_t;


typedef struct win_console_context_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */
    
    win_stdin_type_t eStdinType;      /* Type of stdin: mintty or cmd */
    void*            hStdin;          /* Pointer to stdin   */
    st_u32_t         dwOrigConMode;   /* cmd.exe path only  */

} win_console_context_t;

/* ------------------------------------------------------------------
 * Win Console API
 * ------------------------------------------------------------------ */
st_u64_t   win_console_init(void);
st_error_t win_console_shutdown(void);

#endif /* WIN_H */