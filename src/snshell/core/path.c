/* Path strings: file names, extensions, parents, patterns, kinds. */
#include "internal.h"

LPSTR PathFileName(LPSTR path)
{
    LPSTR p = path, f = path;
    for (; *p; p = AnsiNext(p))
        if (*p == '\\' || *p == ':')
            f = p + 1;
    return f;
}

LPSTR PathExtension(LPSTR path)
{
    LPSTR f = PathFileName(path), dot = NULL;
    for (; *f; f = AnsiNext(f))
        if (*f == '.')
            dot = f;
    return dot ? dot : f;
}

void PathAppend(LPSTR dir, LPCSTR name)
{
    int n = lstrlen(dir);
    if (n && dir[n - 1] != '\\')
        lstrcat(dir, "\\");
    lstrcat(dir, name);
}

void PathToParent(LPSTR path)
{
    LPSTR f = PathFileName(path);
    if (f > path && f[-1] == '\\')
        f--;
    if (f - path < 3)                   /* keep "C:\" */
        f = path + 3;
    if (lstrlen(path) > 3)
        *f = '\0';
}

BOOL PathIsInside(LPCSTR path, LPCSTR dir)
{
    int n = lstrlen(dir);
    return _fstrnicmp(path, dir, n) == 0 &&
           (path[n] == '\0' || path[n] == '\\' || (n && dir[n - 1] == '\\'));
}

BOOL PathOnDisk(LPCSTR path)
{
    return path[0] && path[0] != ':' && path[1] == ':';
}

BOOL PathIsFolder(LPCSTR path)
{
    WORD a;

    if (lstrlen(path) == 3 && path[1] == ':')   /* DOS has no attributes for a root */
        return GetDriveType((path[0] | 0x20) - 'a') != 0;
    a = DosGetAttributes(path);
    return a != 0xFFFF && (a & FA_DIR);
}

static char Upper(char c)
{
    return (char)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)c);
}

/* name against the first len characters of a pattern of * and ?, case
   ignored. A name without a dot still matches "x.*" (as DOS has it). */
static BOOL Match(LPCSTR n, LPCSTR p, int len)
{
    for (; len; p++, len--) {
        if (*p == '*') {
            for (;; n++) {
                if (Match(n, p + 1, len - 1))
                    return TRUE;
                if (!*n)
                    return FALSE;
            }
        }
        if (!*n)
            return *p == '.' && Match(n, p + 1, len - 1);
        if (*p != '?' && Upper(*p) != Upper(*n))
            return FALSE;
        n++;
    }
    return !*n;
}

BOOL PathIsShortName(LPCSTR name)
{
    LPCSTR dot = NULL, p;

    for (p = name; *p; p++) {
        if (*p == ' ' || _fstrchr("+,;=[]\"", *p))
            return FALSE;
        if (*p == '.') {
            if (dot)
                return FALSE;
            dot = p;
        }
    }
    return dot ? dot > name && dot - name <= 8 && p - dot - 1 <= 3 : p > name && p - name <= 8;
}

BOOL PathMatchesPattern(LPCSTR name, LPCSTR patterns)
{
    LPCSTR e;

    while (*patterns) {
        while (*patterns == ';' || *patterns == ' ')
            patterns++;
        for (e = patterns; *e && *e != ';'; e++)
            ;
        while (e > patterns && e[-1] == ' ')
            e--;
        if (e > patterns && Match(name, patterns, (int)(e - patterns)))
            return TRUE;
        while (*e == ' ')
            e++;
        patterns = e;
    }
    return FALSE;
}

BOOL PathMakeUnique(LPSTR out, LPCSTR dir, LPCSTR base, LPCSTR ext)
{
    char name[MAX_PATH];
    int n;

    for (n = 1; n < 100; n++) {
        if (n == 1)
            wsprintf(name, "%s%s", base, ext);
        else
            wsprintf(name, "%s (%d)%s", base, n, ext);
        if (lstrlen(dir) + lstrlen(name) + 2 > MAX_PATH)
            return FALSE;
        lstrcpy(out, dir);
        PathAppend(out, name);
        if (DosGetAttributes(out) == 0xFFFF)
            return TRUE;
    }
    return FALSE;
}

BOOL PathIsShortcut(LPCSTR path)
{
    return !lstrcmpi(PathExtension((LPSTR)path), SHC_EXT);
}

BOOL PathIsDeskLink(LPCSTR path)
{
    return !lstrcmpi(PathExtension((LPSTR)path), DESKLINK_EXT);
}

BOOL PathIsProgram(LPCSTR path)
{
    LPSTR e = PathExtension((LPSTR)path);
    return !lstrcmpi(e, ".EXE") || !lstrcmpi(e, ".COM") || !lstrcmpi(e, ".BAT") || !lstrcmpi(e, ".PIF");
}
