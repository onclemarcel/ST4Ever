/*
 * win_console.c - ST4Ever Windows console initialisation
 *
 * Enables Virtual Terminal Processing (ANSI / VT100 escape sequences)
 * on stdout and stderr so that ANSI colour codes produced by trace.c
 * and line.c render correctly in Windows 10+ cmd.exe and MSYS2/mintty.
 *
 * Also sets the console output code page to UTF-8 (65001).
 *
 * Note: mintty (the MSYS2 terminal) handles ANSI natively without
 * SetConsoleMode, so these calls are non-fatal if they fail (mintty
 * does not expose a real Win32 console handle).
 */

#include "../src/common.h"
#include "../src/trace.h"

#ifdef ST_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ------------------------------------------------------------------
 * win_console_init()
 * ------------------------------------------------------------------ */

st_error_t win_console_init(void)
{
    HANDLE hOut;
    HANDLE hErr;
    DWORD  dwMode;

    LOG_TRACE("(void)");

    /* ---- Enable VT processing on stdout ---- */
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR("GetStdHandle(STD_OUTPUT_HANDLE) failed: %lu",
                  GetLastError());
        /* Non-fatal: mintty uses pipe handles */
    }
    else
    {
        if (GetConsoleMode(hOut, &dwMode))
        {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (!SetConsoleMode(hOut, dwMode))
            {
                LOG_INFO("SetConsoleMode(stdout) failed (code %lu) "
                         "- likely mintty, ANSI already supported",
                         GetLastError());
            }
        }
    }

    /* ---- Enable VT processing on stderr ---- */
    hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE)
    {
        if (GetConsoleMode(hErr, &dwMode))
        {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (!SetConsoleMode(hErr, dwMode))
            {
                LOG_INFO("SetConsoleMode(stderr) failed (code %lu)",
                         GetLastError());
            }
        }
    }

    /* ---- UTF-8 output ---- */
    if (!SetConsoleOutputCP(CP_UTF8))
    {
        LOG_INFO("SetConsoleOutputCP(UTF-8) failed: %lu",
                 GetLastError());
    }
    if (!SetConsoleCP(CP_UTF8))
    {
        LOG_INFO("SetConsoleCP(UTF-8) failed: %lu",
                 GetLastError());
    }

    LOG_INFO("win_console_init: ANSI + UTF-8 console ready");
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * win_console_shutdown()
 * ------------------------------------------------------------------ */

st_error_t win_console_shutdown(void)
{
    LOG_TRACE("(void)");
    /* Nothing to release - console mode resets with the process */
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_WINDOWS */
