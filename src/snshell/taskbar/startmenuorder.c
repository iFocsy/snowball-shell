/*
 * The order the Start menu's items were dragged into, kept in SNOWBALL.INI:
 * a section for each folder of the Start Menu ("Start Menu: Programs\Games"),
 * each item's file name with its place, from 1. Items without a place come
 * after, in the usual order; Sort by Name forgets a folder's places.
 */
#include "internal.h"

/* "Start Menu", "Start Menu: Programs\Games"; FALSE outside the Start Menu. */
static BOOL Section(LPCSTR dir, LPSTR out)
{
    char start[MAX_PATH];
    int n;

    StartMenuFolder(start);
    if (!PathIsInside(dir, start))
        return FALSE;
    n = lstrlen(start);
    lstrcpy(out, "Start Menu");
    if (dir[n] == '\\' && dir[n + 1]) {
        lstrcat(out, ": ");
        lstrcat(out, dir + n + 1);
    }
    return TRUE;
}

static BOOL SectionOf(LPCSTR path, LPSTR out)
{
    char dir[MAX_PATH];

    lstrcpy(dir, path);
    PathToParent(dir);
    return Section(dir, out);
}

int StartMenuOrderPlace(LPCSTR path)
{
    char section[MAX_PATH + 16];

    return SectionOf(path, section) ? SettingsGetInt(section, PathFileName((LPSTR)path), 0) : 0;
}

void StartMenuOrderSet(LPCSTR dir, PATHLIST *names)
{
    char section[MAX_PATH + 16];
    LPSTR p;
    int i = 1;

    if (!Section(dir, section))
        return;
    WritePrivateProfileString(section, NULL, NULL, g_ini);
    for (p = PathListLock(names); *p; p = PathListNext(p))
        SettingsSetInt(section, p, i++);
    PathListUnlock(names);
}

/* Renamed, it keeps its place; moved to another folder, it leaves it. */
void StartMenuOrderRename(LPCSTR from, LPCSTR to)
{
    char section[MAX_PATH + 16], other[MAX_PATH + 16];
    int place = StartMenuOrderPlace(from);

    if (!place || !SectionOf(from, section))
        return;
    WritePrivateProfileString(section, PathFileName((LPSTR)from), NULL, g_ini);
    if (SectionOf(to, other) && !lstrcmp(section, other))
        SettingsSetInt(section, PathFileName((LPSTR)to), place);
}

void StartMenuOrderForget(LPCSTR dir)
{
    char section[MAX_PATH + 16];

    if (Section(dir, section))
        WritePrivateProfileString(section, NULL, NULL, g_ini);
}
