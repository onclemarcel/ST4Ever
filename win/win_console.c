/*
 * win_console.c - ST4Ever Windows console initialisation and raw input
 *
 * win_console_init() / win_console_shutdown():
 *   Enables ANSI / VT100 processing on stdout/stderr and sets the
 *   output code page to UTF-8.  Non-fatal on mintty (pipe handles).
 *
 * console_set_raw() / console_restore() / console_read_key():
 *   Raw keyboard input for the rich line editor (UC4.2 - R5).
 *
 *   Runtime detection via GetFileType(GetStdHandle(STD_INPUT_HANDLE)):
 *
 *   FILE_TYPE_PIPE (mintty / MSYS2):
 *     stdin is a Windows anonymous pipe.  Mintty delivers keystrokes
 *     byte-by-byte through the pipe as they are typed - no termios
 *     raw-mode setup is required.  Single-byte reads use ReadFile().
 *     VT100 escape sequences are decoded with a 50 ms poll timeout
 *     via PeekNamedPipe() to distinguish bare ESC from \033[A etc.
 *
 *   FILE_TYPE_CHAR (cmd.exe / Win32 console):
 *     stdin is a real Win32 console handle.  Raw mode is set by
 *     removing ENABLE_PROCESSED_INPUT / LINE_INPUT / ECHO_INPUT.
 *     Keys are read via ReadConsoleInputA() which returns pre-decoded
 *     virtual key codes - no VT100 parsing is needed.
 *
 * Note: termios.h is NOT included here because MINGW64 (the project
 * target) does not provide it.  The pipe detection path replaces the
 * termios strategy documented in R5.
 */

#include "../src/common.h"
#include "../src/trace.h"
#include "../src/console.h"

#ifdef ST_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Module-level raw-mode state
 * ------------------------------------------------------------------ */

typedef enum win_stdin_type_e
{
    WIN_STDIN_UNKNOWN = 0,
    WIN_STDIN_PIPE,     /* mintty / MSYS2 — pipe, VT100 byte stream   */
    WIN_STDIN_CONSOLE   /* cmd.exe — Win32 console, VK event stream   */
} win_stdin_type_t;

static win_stdin_type_t g_eStdinType    = WIN_STDIN_UNKNOWN;
static HANDLE           g_hStdin        = INVALID_HANDLE_VALUE;
static DWORD            g_dwOrigConMode = 0;   /* cmd.exe path only   */

/* ------------------------------------------------------------------
 * win_console_init() / win_console_shutdown()
 * ------------------------------------------------------------------ */

st_error_t win_console_init(void)
{
    HANDLE hOut;
    HANDLE hErr;
    DWORD  dwMode;

    LOG_TRACE("(void)");

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
                LOG_INFO("SetConsoleMode(stdout) failed (%lu) "
                         "- likely mintty, ANSI already supported",
                         GetLastError());
            }
        }
    }

    hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE)
    {
        if (GetConsoleMode(hErr, &dwMode))
        {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (!SetConsoleMode(hErr, dwMode))
            {
                LOG_INFO("SetConsoleMode(stderr) failed (%lu)",
                         GetLastError());
            }
        }
    }

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

st_error_t win_console_shutdown(void)
{
    LOG_TRACE("(void)");
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Static helpers — mintty pipe path
 * ------------------------------------------------------------------ */

/*
 * console_read_byte_timeout_pipe() - Poll the pipe for one byte with
 * a millisecond deadline.  Returns 0 on success, -1 on timeout/error.
 *
 * Polls every 5 ms using PeekNamedPipe so that arrow-key sequences
 * (which arrive instantly) are decoded with near-zero latency.
 */
static int console_read_byte_timeout_pipe(unsigned char *pByte,
                                           int            iTimeoutMs)
{
    DWORD dwAvail;
    DWORD dwRead;
    int   iElapsed;

    iElapsed = 0;
    for (;;)
    {
        if (PeekNamedPipe(g_hStdin, NULL, 0, NULL, &dwAvail, NULL)
        &&  dwAvail > 0)
        {
            if (!ReadFile(g_hStdin, pByte, 1, &dwRead, NULL)
            ||  dwRead != 1)
            {
                return -1;
            }
            return 0;
        }
        if (iElapsed >= iTimeoutMs)
        {
            break;
        }
        Sleep(5);
        iElapsed += 5;
    }
    return -1;
}

/*
 * console_decode_csi() - Accumulate bytes after ESC '[' and map the
 * resulting sequence to a CON_KEY_* constant.
 */
static int console_decode_csi(void)
{
    char          szSeq[16];
    int           iSeqLen;
    unsigned char ch;

    iSeqLen = 0;
    while (iSeqLen < (int)(sizeof(szSeq) - 1))
    {
        if (console_read_byte_timeout_pipe(&ch, 50) != 0)
        {
            break;
        }
        szSeq[iSeqLen++] = (char)ch;
        /* Final byte: uppercase or lowercase letter, or '~' */
        if ((ch >= 0x40 && ch <= 0x7E))
        {
            break;
        }
    }
    szSeq[iSeqLen] = '\0';

    if (strcmp(szSeq, "A")   == 0) { return CON_KEY_UP;        }
    if (strcmp(szSeq, "B")   == 0) { return CON_KEY_DOWN;      }
    if (strcmp(szSeq, "C")   == 0) { return CON_KEY_RIGHT;     }
    if (strcmp(szSeq, "D")   == 0) { return CON_KEY_LEFT;      }
    if (strcmp(szSeq, "H")   == 0) { return CON_KEY_HOME;      }
    if (strcmp(szSeq, "F")   == 0) { return CON_KEY_END;       }
    if (strcmp(szSeq, "1~")  == 0) { return CON_KEY_HOME;      }
    if (strcmp(szSeq, "3~")  == 0) { return CON_KEY_DELETE;    }
    if (strcmp(szSeq, "4~")  == 0) { return CON_KEY_END;       }
    if (strcmp(szSeq, "5~")  == 0) { return CON_KEY_PAGE_UP;   }
    if (strcmp(szSeq, "6~")  == 0) { return CON_KEY_PAGE_DOWN; }
    if (strcmp(szSeq, "11~") == 0) { return CON_KEY_F1;        }
    if (strcmp(szSeq, "15~") == 0) { return CON_KEY_F5;        }

    return CON_KEY_ESC;
}

/*
 * console_read_key_pipe() - Read one key via the mintty pipe.
 * Decodes VT100 sequences from raw byte stream.
 * Times out after 200 ms → CON_KEY_TIMEOUT (prompt refresh).
 */
static st_error_t console_read_key_pipe(int *piKey)
{
    unsigned char byte1;
    unsigned char byte2;

    /* Timed read: allows prompt refresh on CON_KEY_TIMEOUT */
    if (console_read_byte_timeout_pipe(&byte1, 200) != 0)
    {
        *piKey = CON_KEY_TIMEOUT;
        return ST_NO_ERROR;
    }

    /* Mintty sends 0x7F for BACKSPACE, 0x08 for CTRL+H */
    if (byte1 == 0x7F)
    {
        *piKey = CON_KEY_BACKSPACE;
        return ST_NO_ERROR;
    }

    if (byte1 == 0x1B)
    {
        if (console_read_byte_timeout_pipe(&byte2, 50) != 0)
        {
            *piKey = CON_KEY_ESC;
            return ST_NO_ERROR;
        }

        if (byte2 == '[')
        {
            *piKey = console_decode_csi();
            return ST_NO_ERROR;
        }

        if (byte2 == 'O')
        {
            unsigned char byte3;
            if (console_read_byte_timeout_pipe(&byte3, 50) != 0)
            {
                *piKey = CON_KEY_ESC;
                return ST_NO_ERROR;
            }
            switch (byte3)
            {
                case 'A': *piKey = CON_KEY_UP;    return ST_NO_ERROR;
                case 'B': *piKey = CON_KEY_DOWN;  return ST_NO_ERROR;
                case 'C': *piKey = CON_KEY_RIGHT; return ST_NO_ERROR;
                case 'D': *piKey = CON_KEY_LEFT;  return ST_NO_ERROR;
                case 'H': *piKey = CON_KEY_HOME;  return ST_NO_ERROR;
                case 'F': *piKey = CON_KEY_END;   return ST_NO_ERROR;
                case 'P': *piKey = CON_KEY_F1;    return ST_NO_ERROR;
                default:  *piKey = CON_KEY_ESC;   return ST_NO_ERROR;
            }
        }

        *piKey = CON_KEY_ESC;
        return ST_NO_ERROR;
    }

    *piKey = (int)byte1;
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Static helpers — cmd.exe Win32 console path
 * ------------------------------------------------------------------ */

/*
 * console_read_key_win32() - Read one key via ReadConsoleInputA().
 * Win32 delivers pre-decoded virtual keys so no VT100 parsing needed.
 */
static st_error_t console_read_key_win32(int *piKey)
{
    INPUT_RECORD tRecord;
    DWORD        dwRead;
    DWORD        dwWait;
    BOOL         bResult;
    WORD         wVK;
    char         cAscii;

    for (;;)
    {
        /* Wait up to 200 ms — returns CON_KEY_TIMEOUT so that
         * line_read_rich() can refresh the contextual prompt. */
        dwWait = WaitForSingleObject(g_hStdin, 200);
        if (dwWait == WAIT_TIMEOUT)
        {
            *piKey = CON_KEY_TIMEOUT;
            return ST_NO_ERROR;
        }
        if (dwWait != WAIT_OBJECT_0)
        {
            LOG_ERROR("WaitForSingleObject failed: result=%lu err=%lu",
                      dwWait, GetLastError());
            *piKey = CON_KEY_EOF;
            return ST_ERROR;
        }

        bResult = ReadConsoleInputA(g_hStdin, &tRecord, 1, &dwRead);
        if (!bResult)
        {
            LOG_ERROR("ReadConsoleInputA failed: %lu", GetLastError());
            *piKey = CON_KEY_EOF;
            return ST_ERROR;
        }
        if (dwRead == 0)
        {
            continue;
        }
        if (tRecord.EventType != KEY_EVENT)
        {
            continue;
        }
        if (!tRecord.Event.KeyEvent.bKeyDown)
        {
            continue;
        }

        wVK    = tRecord.Event.KeyEvent.wVirtualKeyCode;
        cAscii = tRecord.Event.KeyEvent.uChar.AsciiChar;

        switch (wVK)
        {
            case VK_UP:     *piKey = CON_KEY_UP;        return ST_NO_ERROR;
            case VK_DOWN:   *piKey = CON_KEY_DOWN;      return ST_NO_ERROR;
            case VK_LEFT:   *piKey = CON_KEY_LEFT;      return ST_NO_ERROR;
            case VK_RIGHT:  *piKey = CON_KEY_RIGHT;     return ST_NO_ERROR;
            case VK_HOME:   *piKey = CON_KEY_HOME;      return ST_NO_ERROR;
            case VK_END:    *piKey = CON_KEY_END;       return ST_NO_ERROR;
            case VK_DELETE: *piKey = CON_KEY_DELETE;    return ST_NO_ERROR;
            case VK_BACK:   *piKey = CON_KEY_BACKSPACE; return ST_NO_ERROR;
            case VK_RETURN: *piKey = CON_KEY_ENTER;     return ST_NO_ERROR;
            case VK_ESCAPE: *piKey = CON_KEY_ESC;       return ST_NO_ERROR;
            case VK_TAB:    *piKey = '\t';              return ST_NO_ERROR;
            case VK_PRIOR:  *piKey = CON_KEY_PAGE_UP;   return ST_NO_ERROR;
            case VK_NEXT:   *piKey = CON_KEY_PAGE_DOWN; return ST_NO_ERROR;
            case VK_F1:     *piKey = CON_KEY_F1;        return ST_NO_ERROR;
            case VK_F5:     *piKey = CON_KEY_F5;        return ST_NO_ERROR;
            default:
                if (cAscii >= 0x20 && cAscii <= 0x7E)
                {
                    *piKey = (int)(unsigned char)cAscii;
                    return ST_NO_ERROR;
                }
                if (cAscii > 0)
                {
                    /* Control characters: CTRL+C = 0x03, CTRL+Q = 0x11 */
                    *piKey = (int)(unsigned char)cAscii;
                    return ST_NO_ERROR;
                }
                /* Extended key with no ASCII mapping: keep looping */
                break;
        }
    }
}

/* ------------------------------------------------------------------
 * Public raw-mode API (console.h)
 * ------------------------------------------------------------------ */

st_error_t console_set_raw(void)
{
    HANDLE hIn;
    DWORD  dwType;
    DWORD  dwMode;

    LOG_TRACE("(void)");

    if (g_eStdinType != WIN_STDIN_UNKNOWN)
    {
        LOG_INFO("console_set_raw: already active");
        return ST_NO_ERROR;
    }

    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR("GetStdHandle(STD_INPUT_HANDLE) failed: %lu",
                  GetLastError());
        return ST_ERROR;
    }

    dwType = GetFileType(hIn);

    if (dwType == FILE_TYPE_PIPE)
    {
        /* mintty / MSYS2: pipe delivers VT100 bytes as typed */
        g_hStdin       = hIn;
        g_eStdinType   = WIN_STDIN_PIPE;
        LOG_INFO("console_set_raw: pipe (mintty) — VT100 byte mode");
        return ST_NO_ERROR;
    }

    if (dwType == FILE_TYPE_CHAR)
    {
        /* cmd.exe: real Win32 console — disable processed/line/echo */
        if (!GetConsoleMode(hIn, &dwMode))
        {
            LOG_ERROR("GetConsoleMode failed: %lu", GetLastError());
            return ST_ERROR;
        }
        g_dwOrigConMode = dwMode;
        dwMode &= ~(ENABLE_PROCESSED_INPUT
                    | ENABLE_LINE_INPUT
                    | ENABLE_ECHO_INPUT);
        dwMode |= ENABLE_WINDOW_INPUT;
        if (!SetConsoleMode(hIn, dwMode))
        {
            LOG_ERROR("SetConsoleMode(raw) failed: %lu",
                      GetLastError());
            return ST_ERROR;
        }
        g_hStdin     = hIn;
        g_eStdinType = WIN_STDIN_CONSOLE;
        LOG_INFO("console_set_raw: Win32 console raw mode (cmd.exe)");
        return ST_NO_ERROR;
    }

    /* FILE_TYPE_UNKNOWN / FILE_TYPE_DISK — not a TTY */
    LOG_ERROR("console_set_raw: stdin type %lu is not a TTY", dwType);
    return ST_ERROR;
}

st_error_t console_restore(void)
{
    LOG_TRACE("(void)");

    if (g_eStdinType == WIN_STDIN_CONSOLE)
    {
        if (!SetConsoleMode(g_hStdin, g_dwOrigConMode))
        {
            LOG_ERROR("SetConsoleMode(restore) failed: %lu",
                      GetLastError());
            return ST_ERROR;
        }
        LOG_INFO("console_restore: Win32 console mode restored");
    }
    /* Pipe path: no mode was changed, nothing to restore */

    g_eStdinType = WIN_STDIN_UNKNOWN;
    g_hStdin     = INVALID_HANDLE_VALUE;
    return ST_NO_ERROR;
}

st_error_t console_read_key(int *piKey)
{
    if (piKey == NULL)
    {
        LOG_ERROR("piKey is NULL");
        return ST_ERROR;
    }

    if (g_eStdinType == WIN_STDIN_PIPE)
    {
        return console_read_key_pipe(piKey);
    }

    if (g_eStdinType == WIN_STDIN_CONSOLE)
    {
        return console_read_key_win32(piKey);
    }

    LOG_ERROR("console_read_key: no raw mode active");
    *piKey = CON_KEY_EOF;
    return ST_ERROR;
}

#endif /* ST_PLATFORM_WINDOWS */
