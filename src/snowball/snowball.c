/*
 * SNOWBALL.EXE, the shell's task. The shell itself is SNSHELL.DLL, whose
 * data has its own 64 KB; this program starts it and owns the stack. Later
 * starts pass their command line on and end: "SNOWBALL C:\DOCS" opens a
 * folder, Windows' Task List call (Ctrl+Esc) the Start menu.
 *
 * The libraries are loaded from this program's folder (Windows looks there
 * for no library at startup), else from the search path (WINDOWS\SYSTEM,
 * where older versions put them). Setup cannot replace them while the shell
 * runs: it leaves the new ones in NEW\, and they take the old ones' place
 * here, before any is loaded. This program's own code and icon stay in
 * memory, so Setup may replace its file while it runs.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include "snowball.h"
#include "snshell.h"

typedef int (FAR PASCAL *SHELLMAIN)(HINSTANCE exe, LPSTR cmdLine);

static const char *Libraries[] = { "SNSYSTEM.DLL", "SNSHELL.DLL", "W98ART.DLL" };
#define NLIBS   (sizeof Libraries / sizeof Libraries[0])

/* Windows' Task List call (Ctrl+Esc runs taskman.exe=SNOWBALL.EXE): where
   to show it, "0 0", or nothing. */
static BOOL TaskListCall(LPCSTR cmd)
{
    for (; *cmd; cmd++)
        if (*cmd != ' ' && (*cmd < '0' || *cmd > '9'))
            return FALSE;
    return TRUE;
}

static BOOL Exists(LPCSTR path)
{
    OFSTRUCT of;

    return OpenFile(path, &of, OF_EXIST) != HFILE_ERROR;
}

/* dir ends with '\': the new libraries Setup left in NEW\ take the old ones'
   place, and copies an older version put in WINDOWS\SYSTEM go. */
static void Update(LPCSTR dir)
{
    char from[MAX_PATH], to[MAX_PATH], old[MAX_PATH];
    int i, n;

    for (i = 0; i < NLIBS; i++) {
        wsprintf(from, "%sNEW\\%s", dir, (LPSTR)Libraries[i]);
        wsprintf(to, "%s%s", dir, (LPSTR)Libraries[i]);
        if (Exists(from)) {
            remove(to);
            rename(from, to);
        }
        n = GetSystemDirectory(old, sizeof old);
        wsprintf(old + n, "%s%s", (LPSTR)(n && old[n - 1] == '\\' ? "" : "\\"), (LPSTR)Libraries[i]);
        if (Exists(to) && Exists(old))
            remove(old);
    }
    wsprintf(from, "%sNEW", dir);
    rmdir(from);
}

static HINSTANCE Load(LPCSTR dir, LPCSTR name)
{
    char path[MAX_PATH];
    HINSTANCE h;

    wsprintf(path, "%s%s", dir, name);
    if ((UINT)(h = LoadLibrary(path)) < 32)
        h = LoadLibrary(name);
    return h;
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int show)
{
    char dir[MAX_PATH], text[80];
    HINSTANCE sys, shell;
    SHELLMAIN shellMain;
    int result;

    /* A second start opens what its command line names ("SNOWBALL C:\DOCS");
       from Ctrl+Esc, the Start menu, or Task Manager while Shift is held
       too (the keys are still down: Ctrl+Shift+Esc). */
    if (hPrev) {
        HWND tb = FindWindow(TASKBAR_CLASS, NULL), desk = FindWindow(DESKTOP_CLASS, NULL);
        if (!TaskListCall(cmdLine) && desk)
            PostMessage(desk, SBM_OPEN, 0, MAKELPARAM(GlobalAddAtom(cmdLine), 0));
        else if (tb)
            PostMessage(tb, *cmdLine && GetAsyncKeyState(VK_SHIFT) < 0 ? SBM_TASKMAN : SBM_STARTMENU, 0, 0L);
        return 0;
    }
    GetModuleFileName(hInst, dir, sizeof dir);
    *(strrchr(dir, '\\') + 1) = '\0';
    Update(dir);
    /* SNSYSTEM.DLL first: SNSHELL.DLL's link to it then finds it loaded */
    if ((UINT)(sys = Load(dir, Libraries[0])) < 32 || (UINT)(shell = Load(dir, Libraries[1])) < 32 ||
        (shellMain = (SHELLMAIN)GetProcAddress(shell, "SHELLMAIN")) == NULL) {
        wsprintf(text, "Snowball cannot start: %s is missing or damaged.",
                 (LPSTR)Libraries[(UINT)sys < 32 ? 0 : 1]);
        MessageBox(NULL, text, "Snowball", MB_OK | MB_ICONSTOP);
        return 1;
    }
    result = shellMain(hInst, cmdLine);
    FreeLibrary(shell);
    FreeLibrary(sys);
    return result;
}
