/* SNSHELL.DLL's entry and exit, and ShellMain: the shell's start, its
   message loop until Windows ends, and its end. */
#include "internal.h"

HINSTANCE g_hInst;
HINSTANCE g_hArt;
BOOL g_isShell;

int FAR PASCAL LibMain(HINSTANCE hInst, WORD dataSeg, WORD heapSize, LPSTR cmdLine)
{
    g_hInst = hInst;
    return 1;
}

int FAR PASCAL WEP(int exitType)
{
    return 1;
}

int SNSHELL_API ShellMain(HINSTANCE exe, LPSTR cmdLine)
{
    char art[MAX_PATH];
    int queue, result;

    /* Events of every program arrive here: room for 64 instead of 8. A
       refused size leaves no queue at all, so smaller ones are tried. */
    for (queue = 64; queue > 8 && !SetMessageQueue(queue); queue -= 8)
        ;
    GetModuleFileName(exe, g_home, sizeof g_home);
    *PathFileName(g_home) = '\0';
    lstrcpy(g_ini, g_home);
    lstrcat(g_ini, "SNOWBALL.INI");
    IconsArtFile(art);                  /* beside SNOWBALL.EXE, else on the search path */
    if ((UINT)(g_hArt = LoadLibrary(art)) < 32 && (UINT)(g_hArt = LoadLibrary("W98ART.DLL")) < 32) {
        ErrorBox(NULL, "W98ART.DLL, which holds Snowball's icons and pictures, is missing.");
        g_hArt = g_hInst;
    }
    ViewOptionsLoad();

    DosInit();
    g_isShell = StartupIsShell();
    ProgramManagerConvertGroups();
    if (!IconViewRegister() || !TreeViewRegister() || !TabsRegister() || !ToolbarRegister() || !SliderRegister()
        || !FolderWindowRegister() || !DesktopCreate() || !TaskbarCreate()) {
        ErrorBox(NULL, "Snowball could not create its windows.");
        return 1;
    }
    if (g_isShell)
        ProgramManagerServe();
    CpuStart();                         /* Task Manager's processor times, from the start */
    HostClipboardStart();
    PostMessage(g_taskbar, SBM_SERVE, 0, 0L);   /* every program's Open and Save As, once running */
    if (g_isShell)
        StartupRun(cmdLine);

    result = MessageLoopRun();
    FileDialogsServe(NULL);
    CpuStop();
    if (g_hArt != g_hInst)
        FreeLibrary(g_hArt);
    return result;
}
