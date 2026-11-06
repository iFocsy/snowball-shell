/* What starts with the shell: the program "win" was given, WIN.INI's
   load= and run= lines, the StartUp folder. */
#include "internal.h"

BOOL StartupIsShell(void)
{
    char shell[MAX_PATH];
    GetPrivateProfileString("boot", "shell", "progman.exe", shell, sizeof shell, "SYSTEM.INI");
    return !lstrcmpi(PathFileName(shell), "SNOWBALL.EXE");
}

/* WIN.INI load= (minimized) and run= lines, as Program Manager does. */
static void RunProfileLine(LPCSTR key, int show)
{
    char line[256], *p, *start;

    GetProfileString("windows", key, "", line, sizeof line);
    for (p = line; *p; ) {
        while (*p == ' ' || *p == ',')
            p++;
        for (start = p; *p && *p != ' ' && *p != ','; p++)
            ;
        if (*p)
            *p++ = '\0';
        if (*start)
            LaunchCommand(NULL, start, NULL, show);
    }
}

/* "win program" hands the program to the shell; then load=, run= and StartUp. */
void StartupRun(LPCSTR cmdLine)
{
    char dir[MAX_PATH], path[MAX_PATH];
    DOSENTRY e;
    DOSFIND f;

    WallpaperStart();
    if (*cmdLine)
        LaunchCommand(NULL, cmdLine, NULL, SW_SHOWNORMAL);
    if (GetAsyncKeyState(VK_SHIFT) < 0)     /* Shift held: skip, as in Windows */
        return;
    RunProfileLine("load", SW_SHOWMINNOACTIVE);
    RunProfileLine("run", SW_SHOWNORMAL);
    StartMenuFolder(dir);
    PathAppend(dir, "Programs\\StartUp");
    lstrcpy(path, dir);
    PathAppend(path, "*.*");
    if (DosFindFirst(path, &f, &e)) {
        do {
            lstrcpy(path, dir);
            PathAppend(path, e.name);
            if (!(e.attr & (FA_HIDDEN | FA_SYSTEM | FA_DIR)))
                LaunchItem(NULL, path);
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
}
