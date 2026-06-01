/*
 * lx_console.c - Linux console initialisation and raw input
 *
 * lx_console_init() / lx_console_shutdown():
 *   On Linux the terminal supports ANSI escape sequences natively.
 *   No VT100 init is needed; the ANSI output code page is UTF-8 by
 *   default.
 *
 * console_set_raw() / console_restore() / console_read_key():
 *   Raw keyboard input for the rich line editor (UC4.2 - R5).
 *   Uses POSIX termios exclusively (no Win32 path needed on Linux).
 */

#include "../src/common.h"
#include "../src/trace.h"
#include "../src/console.h"

#ifdef ST_PLATFORM_LINUX

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>

/* ------------------------------------------------------------------
 * Module-level raw-mode state
 * ------------------------------------------------------------------ */

static st_bool_t    g_bTermiosActive = ST_FALSE;
static struct termios g_tTermOrig;

/* ------------------------------------------------------------------
 * lx_console_init() / lx_console_shutdown()
 * ------------------------------------------------------------------ */

st_error_t lx_console_init(void)
{
    LOG_TRACE("(void)");
    LOG_INFO("lx_console_init: ANSI + UTF-8 natively supported");
    return ST_NO_ERROR;
}

st_error_t lx_console_shutdown(void)
{
    LOG_TRACE("(void)");
    return ST_NO_ERROR;
}

/* ------------------------------------------------------------------
 * Static helpers
 * ------------------------------------------------------------------ */

/*
 * console_read_byte_timeout() - Read one byte with a millisecond
 * deadline.  Returns 0 on success, -1 on timeout or error.
 * Used only while parsing VT100 multi-byte escape sequences.
 */
static int console_read_byte_timeout(unsigned char *pByte, int iTimeoutMs)
{
    struct timeval tv;
    fd_set         rfds;
    int            iRet;

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    tv.tv_sec  = iTimeoutMs / 1000;
    tv.tv_usec = (iTimeoutMs % 1000) * 1000;

    iRet = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    if (iRet <= 0)
    {
        return -1;
    }

    iRet = (int)read(STDIN_FILENO, pByte, 1);
    return (iRet == 1) ? 0 : -1;
}

/*
 * console_decode_csi() - Decode a CSI (ESC '[') escape sequence.
 */
static int console_decode_csi(void)
{
    char          szSeq[16];
    int           iSeqLen;
    unsigned char ch;

    iSeqLen = 0;
    while (iSeqLen < (int)(sizeof(szSeq) - 1))
    {
        if (console_read_byte_timeout(&ch, 50) != 0)
        {
            break;
        }
        szSeq[iSeqLen++] = (char)ch;
        if (ch >= 0x40)
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

/* ------------------------------------------------------------------
 * Public raw-mode API (console.h)
 * ------------------------------------------------------------------ */

st_error_t console_set_raw(void)
{
    struct termios tRaw;

    LOG_TRACE("(void)");

    if (g_bTermiosActive == ST_TRUE)
    {
        LOG_INFO("console_set_raw: already active");
        return ST_NO_ERROR;
    }

    if (tcgetattr(STDIN_FILENO, &g_tTermOrig) != 0)
    {
        LOG_ERROR("tcgetattr failed: %s", strerror(errno));
        return ST_ERROR;
    }

    tRaw = g_tTermOrig;
    tRaw.c_lflag &= (tcflag_t)~(ECHO | ICANON | ISIG | IEXTEN);
    tRaw.c_iflag &= (tcflag_t)~(IXON | ICRNL | BRKINT | INPCK
                                 | ISTRIP);
    tRaw.c_cflag |= (tcflag_t)CS8;
    tRaw.c_cc[VMIN]  = 1;
    tRaw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &tRaw) != 0)
    {
        LOG_ERROR("tcsetattr(TCSANOW) failed: %s", strerror(errno));
        return ST_ERROR;
    }

    g_bTermiosActive = ST_TRUE;
    LOG_INFO("console_set_raw: termios raw mode active");
    return ST_NO_ERROR;
}

st_error_t console_restore(void)
{
    LOG_TRACE("(void)");

    if (g_bTermiosActive == ST_FALSE)
    {
        return ST_NO_ERROR;
    }

    if (tcsetattr(STDIN_FILENO, TCSANOW, &g_tTermOrig) != 0)
    {
        LOG_ERROR("tcsetattr(restore) failed: %s", strerror(errno));
        return ST_ERROR;
    }

    g_bTermiosActive = ST_FALSE;
    LOG_INFO("console_restore: termios canonical mode restored");
    return ST_NO_ERROR;
}

st_error_t console_read_key(int *piKey)
{
    unsigned char byte1;
    unsigned char byte2;
    int           iRet;

    if (piKey == NULL)
    {
        LOG_ERROR("piKey is NULL");
        return ST_ERROR;
    }

    if (g_bTermiosActive == ST_FALSE)
    {
        LOG_ERROR("console_read_key: no raw mode active");
        *piKey = CON_KEY_EOF;
        return ST_ERROR;
    }

    /* Timed read: allows prompt refresh on CON_KEY_TIMEOUT */
    if (console_read_byte_timeout(&byte1, 200) != 0)
    {
        *piKey = CON_KEY_TIMEOUT;
        return ST_NO_ERROR;
    }

    if (byte1 == (unsigned char)g_tTermOrig.c_cc[VERASE]
    ||  byte1 == 0x7F)
    {
        *piKey = CON_KEY_BACKSPACE;
        return ST_NO_ERROR;
    }

    if (byte1 == 0x1B)
    {
        if (console_read_byte_timeout(&byte2, 50) != 0)
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
            if (console_read_byte_timeout(&byte3, 50) != 0)
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

#endif /* ST_PLATFORM_LINUX */
