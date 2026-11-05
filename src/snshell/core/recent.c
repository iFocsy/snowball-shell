/*
 * The documents used last, for Start > Documents: shortcuts in the Recent
 * folder next to SNOWBALL.EXE, one per document (named after it, so using
 * it again only refreshes it), the 15 newest kept, as Windows 98 does.
 */
#include "internal.h"

#define KEEP    15

LPSTR RecentFolder(LPSTR out)
{
    lstrcpy(out, g_home);
    lstrcat(out, "Recent");
    return out;
}

/* The oldest shortcuts go while there are more than KEEP. */
static void Trim(LPCSTR dir)
{
    char spec[MAX_PATH], oldest[MAX_PATH];
    DOSENTRY e;
    DOSFIND f;
    DWORD when, first;
    int n;

    do {
        n = 0;
        first = 0xFFFFFFFFL;
        lstrcpy(spec, dir);
        PathAppend(spec, "*" SHC_EXT);
        if (DosFindFirst(spec, &f, &e)) {
            do {
                when = MAKELONG(e.time, e.date);
                if (when < first) {
                    first = when;
                    lstrcpy(oldest, dir);
                    PathAppend(oldest, e.name);
                }
                n++;
            } while (DosFindNext(&f, &e));
            DosFindClose(&f);
        }
    } while (n > KEEP && DosDelete(oldest));
}

void RecentAdd(LPCSTR document)
{
    char dir[MAX_PATH], path[MAX_PATH];
    SHORTCUT s;

    RecentFolder(dir);
    lstrcpy(path, dir);
    PathAppend(path, PathFileName((LPSTR)document));
    if (lstrlen(path) + lstrlen(SHC_EXT) >= MAX_PATH || (!PathIsFolder(dir) && !DosMakeFolder(dir)))
        return;
    lstrcat(path, SHC_EXT);
    _fmemset(&s, 0, sizeof s);
    lstrcpyn(s.target, document, sizeof s.target);
    s.show = SW_SHOWNORMAL;
    if (ShortcutWrite(path, &s))
        Trim(dir);
}

void RecentClear(void)
{
    char dir[MAX_PATH], spec[MAX_PATH], path[MAX_PATH];
    DOSENTRY e;
    DOSFIND f;

    RecentFolder(dir);
    lstrcpy(spec, dir);
    PathAppend(spec, "*" SHC_EXT);
    if (DosFindFirst(spec, &f, &e)) {
        do {
            lstrcpy(path, dir);
            PathAppend(path, e.name);
            DosDelete(path);
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
}

BOOL RecentIsEmpty(void)
{
    char spec[MAX_PATH];
    DOSENTRY e;
    DOSFIND f;

    RecentFolder(spec);
    PathAppend(spec, "*" SHC_EXT);
    if (!DosFindFirst(spec, &f, &e))
        return TRUE;
    DosFindClose(&f);
    return FALSE;
}
