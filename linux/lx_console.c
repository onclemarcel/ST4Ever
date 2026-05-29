/*
 * lx_console.c - Linux console initialisation stub
 *
 * On Linux the terminal (xterm, GNOME Terminal, etc.) supports ANSI
 * escape sequences natively.  No initialisation is needed for UC1.
 *
 * TODO(UC4): Implement raw-mode line editor using termios:
 *   - tcgetattr / tcsetattr(TCSANOW) to switch to raw mode
 *   - read() byte-by-byte, parse VT100 escape sequences
 *     (\033[A = up arrow, \033[B = down, \033[H = home, \033[F = end)
 *   - Restore canonical mode on shutdown
 */

#include "../src/common.h"
#include "../src/trace.h"

#ifdef ST_PLATFORM_LINUX

st_error_t lx_console_init(void)
{
    LOG_TRACE("(void)");
    LOG_INFO("lx_console_init: ANSI natively supported on Linux");
    LOG_TODO("lx_console_init: raw-mode line editor (UC4)");
    return ST_NO_ERROR;
}

st_error_t lx_console_shutdown(void)
{
    LOG_TRACE("(void)");
    LOG_TODO("lx_console_shutdown: restore termios canonical mode (UC4)");
    return ST_NO_ERROR;
}

#endif /* ST_PLATFORM_LINUX */
