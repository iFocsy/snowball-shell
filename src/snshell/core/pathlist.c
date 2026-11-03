/* Lists of paths in one global block, each ending in '\0'. */
#include "internal.h"

/* ---- path lists: one global block, up to 64 KB ---- */

void PathListInit(PATHLIST *l)
{
    _fmemset(l, 0, sizeof *l);
}

BOOL PathListAdd(PATHLIST *l, LPCSTR path)
{
    WORD n = lstrlen(path) + 1;
    DWORD cap = l->cap;
    LPSTR p;

    while ((DWORD)l->used + n + 1 > cap)
        cap = cap ? cap * 2 : 512;
    if (cap > 65000L)
        return FALSE;
    if (cap != l->cap) {
        HGLOBAL h = l->mem ? GlobalReAlloc(l->mem, cap, GMEM_MOVEABLE)
                           : GlobalAlloc(GMEM_MOVEABLE, cap);
        if (!h)
            return FALSE;
        l->mem = h;
        l->cap = (WORD)cap;
    }
    p = GlobalLock(l->mem);
    lstrcpy(p + l->used, path);
    l->used += n;
    p[l->used] = '\0';                  /* the list ends with an empty path */
    GlobalUnlock(l->mem);
    l->count++;
    return TRUE;
}

LPSTR PathListLock(PATHLIST *l)
{
    static char empty[1];
    return l->mem ? (LPSTR)GlobalLock(l->mem) : (LPSTR)empty;
}

LPSTR PathListNext(LPSTR p)
{
    return p + lstrlen(p) + 1;
}

void PathListUnlock(PATHLIST *l)
{
    if (l->mem)
        GlobalUnlock(l->mem);
}

void PathListFree(PATHLIST *l)
{
    if (l->mem)
        GlobalFree(l->mem);
    PathListInit(l);
}
