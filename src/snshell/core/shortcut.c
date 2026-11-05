/* Shortcuts: .SHC files, small INI-style text files. */
#include "internal.h"

static const char Section[] = "[Shortcut]";

BOOL ShortcutRead(LPCSTR path, SHORTCUT *s)
{
    char file[MAX_PATH];

    DosShortPath(path, file);
    _fmemset(s, 0, sizeof *s);
    GetPrivateProfileString("Shortcut", "Target", "", s->target, sizeof s->target, file);
    GetPrivateProfileString("Shortcut", "Arguments", "", s->args, sizeof s->args, file);
    GetPrivateProfileString("Shortcut", "Directory", "", s->dir, sizeof s->dir, file);
    GetPrivateProfileString("Shortcut", "Icon", "", s->icon, sizeof s->icon, file);
    s->iconIndex = GetPrivateProfileInt("Shortcut", "IconIndex", 0, file);
    if (s->icon[0] && !lstrcmpi(PathFileName(s->icon), "SNOWBALL.EXE"))
        lstrcpy(PathFileName(s->icon), "W98ART.DLL");   /* an older Snowball's: its icons are there now */
    s->show = GetPrivateProfileInt("Shortcut", "Show", SW_SHOWNORMAL, file);
    s->hotkey = GetPrivateProfileInt("Shortcut", "HotKey", 0, file);
    return s->target[0] != '\0';
}

/* One line alone, for callers that need no more (hovering, scanning). */
WORD ShortcutHotKey(LPCSTR path)
{
    char file[MAX_PATH];

    DosShortPath(path, file);
    return GetPrivateProfileInt("Shortcut", "HotKey", 0, file);
}

BOOL ShortcutTarget(LPCSTR path, LPSTR target)
{
    char file[MAX_PATH];

    DosShortPath(path, file);
    return GetPrivateProfileString("Shortcut", "Target", "", target, MAX_PATH, file) != 0;
}

/* Written in one go so that long file names work (the profile APIs only
   open 8.3 paths). */
BOOL ShortcutWrite(LPCSTR path, const SHORTCUT *s)
{
    static char text[1100];
    wsprintf(text, "%s\r\nTarget=%s\r\nArguments=%s\r\nDirectory=%s\r\nIcon=%s\r\nIconIndex=%d\r\nShow=%d\r\n",
             (LPSTR)Section, (LPSTR)s->target, (LPSTR)s->args, (LPSTR)s->dir,
             (LPSTR)s->icon, s->iconIndex, s->show);
    if (s->hotkey)                      /* only then: other shortcuts stay as they were */
        wsprintf(text + lstrlen(text), "HotKey=%u\r\n", s->hotkey);
    return DosCreateTextFile(path, text);
}

/* Name of a new shortcut to target in dir: "Shortcut to Notepad" (programs
   lose their extension, as in Windows 98), or just "Notepad" when named. */
BOOL ShortcutCreate(LPCSTR target, LPCSTR dir, BOOL named)
{
    char base[MAX_PATH], path[MAX_PATH];
    SHORTCUT s;

    if (PathIsShortcut(target)) {                   /* a shortcut to a shortcut is a copy */
        if (!ShortcutRead(target, &s))
            return FALSE;
    } else {
        _fmemset(&s, 0, sizeof s);
        lstrcpy(s.target, target);
        if (!PathIsFolder(target)) {
            lstrcpy(s.dir, target);
            PathToParent(s.dir);
        }
        s.show = SW_SHOWNORMAL;
    }
    lstrcpy(base, named ? "" : "Shortcut to ");
    lstrcat(base, lstrlen(target) <= 3 ? target : PathFileName((LPSTR)target));
    if (lstrlen(target) <= 3)
        base[lstrlen(base) - 2] = '\0';        /* "C:\" -> "C" */
    else if (PathIsShortcut(target) || PathIsProgram(target))
        *PathExtension(base) = '\0';
    return PathMakeUnique(path, dir, base, SHC_EXT) && ShortcutWrite(path, &s);
}
