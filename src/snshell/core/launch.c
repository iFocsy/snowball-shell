/*
 * Opening things: folders, shortcuts, programs and documents. Old Windows
 * APIs only understand 8.3 paths, so long paths are converted to their
 * aliases before launching.
 */
#include "internal.h"

static SHORTCUT s;                     /* large: one for the module, never in use twice */

BOOL LaunchIsProgram(LPCSTR path)
{
    if (PathIsShortcut(path))
        return ShortcutRead(path, &s) && PathIsProgram(s.target);
    return PathIsProgram(path);
}

static void LaunchError(HWND owner, LPCSTR what, UINT code)
{
    char msg[MAX_PATH + 120];
    LPCSTR why;

    switch (code) {
    case 0: case 8:   why = "There is not enough memory to start it."; break;
    case 2: case 3:   why = "The file or one of its folders cannot be found."; break;
    case 11: case 12: case 13: case 14: case 15: case 20:
                      why = "It is not a valid Windows or MS-DOS program."; break;
    case 31:          why = "No program is associated with this type of file."; break;
    default:          why = "Windows could not start it."; break;
    }
    wsprintf(msg, "Cannot open '%s'.\n\n%s", what, (LPSTR)why);
    ErrorBox(owner, msg);
}

/* Runs a program or opens a document (its type's default action; a type
   with none asks Open With). file may be long; args as typed, cut to what a
   command line holds. */
static BOOL Launch(HWND owner, LPCSTR file, LPCSTR longArgs, LPCSTR dir, int show)
{
    char shortFile[MAX_PATH], shortDir[MAX_PATH], args[128], cmd[MAX_PATH + 130], cls[40], action[16];
    UINT r;

    lstrcpyn(args, longArgs, sizeof args);
    DosShortPath(file, shortFile);
    shortDir[0] = '\0';
    if (dir && *dir) {
        DosShortPath(dir, shortDir);
        DosChangeFolder(shortDir);
    }
    if (PathIsProgram(shortFile)) {
        wsprintf(cmd, *args ? "%s %s" : "%s", (LPSTR)shortFile, (LPSTR)args);
        r = WinExec(cmd, show);
    } else {
        if (!*PathExtension(shortFile) || !FileTypeClass(PathExtension(shortFile) + 1, cls, sizeof cls))
            cls[0] = '\0';
        FileTypeDefaultAction(cls, action, sizeof action);
        r = (UINT)ShellExecute(owner, action, shortFile, *args ? args : NULL,
                               *shortDir ? shortDir : NULL, show);
        if (r == 31)                    /* no program for the type */
            return OpenWithDialogShow(owner, file);
    }
    if (r > 32)
        return TRUE;
    LaunchError(owner, file, r);
    return FALSE;
}

/* One of the type's actions on a document ("print"), from its menu. */
BOOL LaunchAction(HWND owner, LPCSTR file, LPCSTR action)
{
    char shortFile[MAX_PATH];
    UINT r;

    DosShortPath(file, shortFile);
    r = (UINT)ShellExecute(owner, action, shortFile, NULL, NULL, SW_SHOWNORMAL);
    if (r > 32)
        return TRUE;
    LaunchError(owner, file, r);
    return FALSE;
}

BOOL LaunchItem(HWND owner, LPCSTR path)
{
    char target[MAX_PATH];

    if (PathIsFolder(path)) {
        FolderWindowOpen(path);
        return TRUE;
    }
    if (!PathIsShortcut(path)) {
        if (!Launch(owner, path, "", NULL, SW_SHOWNORMAL))
            return FALSE;
        if (!PathIsProgram(path))       /* Start > Documents */
            RecentAdd(path);
        return TRUE;
    }
    if (!ShortcutRead(path, &s)) {
        LaunchError(owner, path, 2);
        return FALSE;
    }
    if (PathIsFolder(s.target)) {
        FolderWindowOpen(s.target);
        return TRUE;
    }
    lstrcpy(target, s.target);
    if (!Launch(owner, s.target, s.args, s.dir, s.show ? s.show : SW_SHOWNORMAL))
        return FALSE;
    if (!PathIsProgram(target))
        RecentAdd(target);
    return TRUE;
}

/* Runs a program, or the program of a shortcut, with extra arguments
   (files dropped on it). */
BOOL LaunchWith(HWND owner, LPCSTR program, LPCSTR args)
{
    char all[256];

    if (!PathIsShortcut(program))
        return Launch(owner, program, args, NULL, SW_SHOWNORMAL);
    if (!ShortcutRead(program, &s)) {
        LaunchError(owner, program, 2);
        return FALSE;
    }
    lstrcpyn(all, s.args, sizeof all);
    if (*all && lstrlen(all) + lstrlen(args) + 2 < sizeof all)
        lstrcat(all, " ");
    if (lstrlen(all) + lstrlen(args) + 1 < sizeof all)
        lstrcat(all, args);
    return Launch(owner, s.target, all, s.dir, s.show ? s.show : SW_SHOWNORMAL);
}

/* A typed command line: "program args", the program optionally in quotes
   (needed when its long name has spaces). Folders open a window. */
void LaunchSplitCommand(LPCSTR cmdline, LPSTR prog, LPSTR args, int argsSize)
{
    LPCSTR p = cmdline;
    int n = 0;

    while (*p == ' ')
        p++;
    if (*p == '"') {
        for (p++; *p && *p != '"' && n < MAX_PATH - 1; p++)
            prog[n++] = *p;
        if (*p == '"')
            p++;
    } else {
        for (; *p && *p != ' ' && n < MAX_PATH - 1; p++)
            prog[n++] = *p;
    }
    prog[n] = '\0';
    while (*p == ' ')
        p++;
    lstrcpyn(args, p, argsSize);
}

BOOL LaunchCommand(HWND owner, LPCSTR cmdline, LPCSTR dir, int show)
{
    char prog[MAX_PATH], args[128];

    LaunchSplitCommand(cmdline, prog, args, sizeof args);
    if (!*prog)
        return FALSE;
    if (PathIsFolder(prog)) {
        FolderWindowOpen(prog);
        return TRUE;
    }
    return Launch(owner, prog, args, dir, show);
}

/* OPEN.COM's request from a DOS box (%windir%\SBOPEN.REQ: its folder, then
   what was typed), if one is there: done, then deleted. A relative name
   ("." too) is the DOS box's. */
void LaunchOpenRequest(HWND owner)
{
    char file[MAX_PATH], text[440], dir[MAX_PATH], prog[MAX_PATH], args[128], cmd[MAX_PATH + 132];
    LPSTR typed, end;
    HFILE h;
    int n;

    GetWindowsDirectory(file, sizeof file);
    PathAppend(file, "SBOPEN.REQ");
    if (DosGetAttributes(file) == 0xFFFF || (h = DosOpenRead(file)) == HFILE_ERROR)
        return;
    n = _lread(h, text, sizeof text - 1);
    _lclose(h);
    DosDelete(file);
    text[max(n, 0)] = '\0';
    if ((typed = _fstrchr(text, '\r')) == NULL)
        return;
    *typed = '\0';
    typed += typed[1] == '\n' ? 2 : 1;
    if ((end = _fstrchr(typed, '\r')) != NULL)
        *end = '\0';
    OemToAnsi(text, dir);
    OemToAnsi(typed, typed);
    LaunchSplitCommand(typed, prog, args, sizeof args);
    if (!*prog)
        return;
    if (!lstrcmp(prog, ".")) {
        lstrcpy(prog, dir);
    } else if (prog[1] != ':' && prog[0] != '\\') {
        lstrcpy(cmd, dir);
        PathAppend(cmd, prog);
        if (DosGetAttributes(cmd) != 0xFFFF)
            lstrcpy(prog, cmd);
    }
    if (PathIsFolder(prog)) {
        FolderWindowOpen(prog);
        return;
    }
    wsprintf(cmd, _fstrchr(prog, ' ') ? "\"%s\" %s" : "%s %s", (LPSTR)prog, (LPSTR)args);
    LaunchCommand(owner, cmd, dir, SW_SHOWNORMAL);
}
