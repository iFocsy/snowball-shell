/*
 * MS-DOS Prompt in a window, as Windows 98 opens it (Windows 3.1's own
 * PIF opens it full screen): Windows' DOSPRMPT.PIF copied to Snowball's
 * PROMPT.PIF with its 386 enhanced section windowed (and running in the
 * background), starting in a folder. With a command, the PIF runs a batch
 * file (SBDOS.BAT) that does it, then waits for a key so its output can be
 * read. Standard mode has no DOS windows: there it is full screen anyway.
 */
#include "internal.h"

#define PIF_MAX     1024
#define PIF_TITLE   0x02                /* 30 bytes */
#define PIF_PROGRAM 0x24                /* 63 */
#define PIF_DIR     0x65                /* 64 */
#define PIF_HEADS   0x171               /* the sections' headings: "MICROSOFT PIFEX" first */

/* The 386 enhanced section's flags: full screen (0x08) off, background (0x02)
   on, and Allow Close When Active (0x01): its window's Close ends it, as
   Windows 98's MS-DOS Prompt (Windows 3.1 refuses while it runs). */
static BOOL Windowed(BYTE far *pif, int size)
{
    WORD at = PIF_HEADS, data;
    int n;

    for (n = 0; n < 8 && at + 22 <= (WORD)size; n++) {
        data = *(WORD far *)(pif + at + 18);
        if (!_fstrncmp((LPSTR)pif + at, "WINDOWS 386 3.0", 16) && data + 0x14 <= (WORD)size) {
            pif[data + 0x10] = (BYTE)((pif[data + 0x10] & ~0x08) | 0x03);
            return TRUE;
        }
        if ((at = *(WORD far *)(pif + at + 16)) == 0xFFFF || !at)
            break;
    }
    return FALSE;
}

static void Field(BYTE far *pif, int at, int size, LPCSTR text)
{
    _fmemset(pif + at, 0, size);
    lstrcpyn((LPSTR)pif + at, text, size);
}

static BOOL WriteFile(LPCSTR path, const void far *data, UINT n)
{
    HFILE f = _lcreat(path, 0);

    if (f == HFILE_ERROR)
        return FALSE;
    n = n == _lwrite(f, data, n);
    _lclose(f);
    return n;
}

/* In dir (NULL: Windows'), titled; command (or NULL) run in it. */
BOOL LaunchDosPrompt(HWND owner, LPCSTR dir, LPCSTR command, LPCSTR title)
{
    char path[MAX_PATH], shortPath[MAX_PATH], bat[MAX_PATH + 32];
    HGLOBAL h;
    BYTE far *pif;
    HFILE f;
    int n = 0;
    UINT r;

    GetWindowsDirectory(path, sizeof path);
    if (!dir || !*dir)
        dir = path;
    DosShortPath(dir, shortPath);
    GetWindowsDirectory(path, sizeof path);
    PathAppend(path, "DOSPRMPT.PIF");
    if ((h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, PIF_MAX)) == NULL)
        return FALSE;
    pif = GlobalLock(h);
    if ((f = _lopen(path, OF_READ)) != HFILE_ERROR) {
        n = _lread(f, pif, PIF_MAX);
        _lclose(f);
    }
    if (n <= PIF_HEADS || n == PIF_MAX || !Windowed(pif, n)) {
        GlobalUnlock(h);
        GlobalFree(h);
        return LaunchCommand(owner, "COMMAND.COM", shortPath, SW_SHOWNORMAL);
    }
    Field(pif, PIF_TITLE, 30, title);
    Field(pif, PIF_DIR, 64, shortPath);
    if (command) {
        lstrcpy(path, g_home);
        PathAppend(path, "SBDOS.BAT");
        wsprintf(bat, "@ECHO OFF\r\n%s\r\nPAUSE\r\n", command);
        if (!WriteFile(path, bat, lstrlen(bat)))
            n = 0;
        DosShortPath(path, shortPath);
        Field(pif, PIF_PROGRAM, 63, shortPath);
    }
    lstrcpy(path, g_home);
    PathAppend(path, "PROMPT.PIF");
    if (n && !WriteFile(path, pif, n))
        n = 0;
    GlobalUnlock(h);
    GlobalFree(h);
    DosShortPath(path, shortPath);
    if (!n || (r = WinExec(shortPath, SW_SHOWNORMAL)) <= 32) {
        ErrorBox(owner, "MS-DOS Prompt cannot be opened.");
        return FALSE;
    }
    return TRUE;
}
