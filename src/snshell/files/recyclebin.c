/*
 * The Recycle Bin: a hidden X:\RECYCLED folder on each fixed disk. Deleted
 * items are moved there (a rename, so it is instant) as D<n>.<ext>, and
 * X:\RECYCLED\INFO.INI remembers where each one came from:
 *   [Bin]    Next=<n>
 *   [Files]  D12.TXT=<DOS date>,<DOS time>,C:\Original Folder\Name.txt
 * Its settings (SNOWBALL.INI [Recycle Bin], settings\binproperties.c):
 * NoMove (deleting is for good), MaxPercent (of each drive: past it, the
 * oldest items go for good), Confirm (asking before sending to the bin).
 */
#include "internal.h"
#include <stdlib.h>

#define LIST_MAX    16384               /* key list of one INFO.INI */
#define TRIM_MAX    400                 /* items of one bin weighed for trimming */

static void BinDir(char drive, LPSTR out)
{
    wsprintf(out, "%c:\\" BIN_FOLDER, drive);
}

static void InfoPath(char drive, LPSTR out)
{
    wsprintf(out, "%c:\\" BIN_FOLDER "\\INFO.INI", drive);
}

static BOOL FixedDrive(int d)
{
    return GetDriveType(d) == DRIVE_FIXED;
}

BOOL RecycleBinCovers(LPCSTR path)
{
    char dir[16];

    if (path[1] != ':' || !FixedDrive((path[0] | 0x20) - 'a'))
        return FALSE;
    BinDir(path[0], dir);
    return !PathIsInside(path, dir) && !SettingsGetInt("Recycle Bin", "NoMove", 0);
}

BOOL RecycleBinReserve(LPCSTR path, LPSTR stored)
{
    char info[32], name[24], next[8];
    int n;

    BinDir(path[0], stored);
    if (!PathIsFolder(stored)) {
        if (!DosMakeFolder(stored))
            return FALSE;
        DosSetAttributes(stored, FA_HIDDEN | FA_SYSTEM);
    }
    InfoPath(path[0], info);
    n = GetPrivateProfileInt("Bin", "Next", 1, info);
    wsprintf(next, "%d", n + 1);
    WritePrivateProfileString("Bin", "Next", next, info);
    wsprintf(name, "D%d%s", n, PathIsFolder(path) ? (LPSTR)"" : PathExtension((LPSTR)path));
    PathAppend(stored, name);              /* the extension keeps the icon */
    return TRUE;
}

void RecycleBinRecord(LPCSTR stored, LPCSTR original)
{
    char info[32], value[MAX_PATH + 16];
    DWORD now = DosNow();

    InfoPath(stored[0], info);
    wsprintf(value, "%u,%u,%s", HIWORD(now), LOWORD(now), original);
    WritePrivateProfileString("Files", PathFileName((LPSTR)stored), value, info);
}

/* Calls back for every item of every bin (stored path, original path)
   until it answers FALSE. */
typedef BOOL (*BINITEM)(LPCSTR stored, LPCSTR original, void *data);

static void ForEach(BINITEM fn, void *data)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, LIST_MAX);
    LPSTR keys = GlobalLock(h), k, comma;
    char info[32], stored[MAX_PATH], value[MAX_PATH + 16];
    BOOL more = TRUE;
    int d;

    if (!keys)
        return;
    for (d = 2; d < 26 && more; d++) {
        if (!FixedDrive(d))
            continue;
        InfoPath('A' + d, info);
        GetPrivateProfileString("Files", NULL, "", keys, LIST_MAX, info);
        for (k = keys; *k && more; k += lstrlen(k) + 1) {
            GetPrivateProfileString("Files", k, "", value, sizeof value, info);
            comma = _fstrchr(value, ',');
            comma = comma ? _fstrchr(comma + 1, ',') : NULL;
            BinDir('A' + d, stored);
            PathAppend(stored, k);
            if (comma && DosGetAttributes(stored) != 0xFFFF)
                more = fn(stored, comma + 1, data);
            else                        /* gone: forget it */
                WritePrivateProfileString("Files", k, NULL, info);
        }
    }
    GlobalUnlock(h);
    GlobalFree(h);
}

static BOOL Found(LPCSTR stored, LPCSTR original, void *data)
{
    *(BOOL *)data = TRUE;
    return FALSE;                       /* one is enough */
}

static BOOL AddItem(LPCSTR stored, LPCSTR original, void *data)
{
    DOSENTRY e;
    char name[MAX_PATH];

    DisplayNameOfFile(PathFileName((LPSTR)original), name);
    if (DosGetEntry(stored, &e))
        IconViewAdd(*(HWND *)data, name, stored, (e.attr & FA_DIR) ? IVK_FOLDER : IVK_FILE, &e);
    return TRUE;
}

static BOOL ListItem(LPCSTR stored, LPCSTR original, void *data)
{
    PathListAdd((PATHLIST *)data, stored);
    return TRUE;
}

BOOL RecycleBinIsEmpty(void)
{
    BOOL any = FALSE;

    ForEach(Found, &any);
    return !any;
}

void RecycleBinLoad(HWND iv)
{
    IconViewSetFolder(iv, BIN_PATH);
    ForEach(AddItem, &iv);
}

/* Original path of a stored item, "" if unknown. */
static void Original(LPCSTR stored, LPSTR out)
{
    char info[32], value[MAX_PATH + 16];
    LPSTR comma;

    InfoPath(stored[0], info);
    GetPrivateProfileString("Files", PathFileName((LPSTR)stored), "", value, sizeof value, info);
    comma = _fstrchr(value, ',');
    comma = comma ? _fstrchr(comma + 1, ',') : NULL;
    lstrcpy(out, comma ? comma + 1 : "");
}

static void Forget(LPCSTR stored)
{
    char info[32];
    InfoPath(stored[0], info);
    WritePrivateProfileString("Files", PathFileName((LPSTR)stored), NULL, info);
}

/* Creates the missing folders of path (not path itself). */
static void MakeParents(LPCSTR path)
{
    char dir[MAX_PATH];
    LPSTR p;

    lstrcpy(dir, path);
    for (p = dir + 3; *p; p = AnsiNext(p))
        if (*p == '\\') {
            *p = '\0';
            if (!PathIsFolder(dir))
                DosMakeFolder(dir);
            *p = '\\';
        }
}

void RecycleBinRestore(HWND owner, PATHLIST *stored)
{
    char original[MAX_PATH];
    LPSTR p;

    for (p = PathListLock(stored); *p; p = PathListNext(p)) {
        Original(p, original);
        if (!*original)
            continue;
        if (DosGetAttributes(original) != 0xFFFF) {
            FileOperationError(owner, "restore", original, 80);
            continue;
        }
        MakeParents(original);
        if (DosRename(p, original))
            Forget(p);
        else
            FileOperationError(owner, "restore", original, DosLastError());
        PathToParent(original);
        FileCommandsFolderChanged(original);
    }
    PathListUnlock(stored);
    FileCommandsFolderChanged(BIN_PATH);
}

static void Purge(HWND owner, PATHLIST *stored)
{
    LPSTR p;

    FileOperationRun(owner, FOP_PURGE, stored, NULL);
    for (p = PathListLock(stored); *p; p = PathListNext(p))
        if (DosGetAttributes(p) == 0xFFFF)
            Forget(p);
    PathListUnlock(stored);
    FileCommandsFolderChanged(BIN_PATH);
}

void RecycleBinPurge(HWND owner, PATHLIST *stored)
{
    char text[MAX_PATH + 60], name[MAX_PATH];
    LPSTR p = PathListLock(stored);

    if (stored->count == 1) {
        Original(p, text);
        DisplayNameOfFile(PathFileName(text), name);
        wsprintf(text, "Are you sure you want to delete '%s'?", (LPSTR)name);
    } else {
        wsprintf(text, "Are you sure you want to delete these %d items?", stored->count);
    }
    PathListUnlock(stored);
    if (MessageBox(owner, text, stored->count == 1 ? "Confirm File Delete" : "Confirm Multiple File Delete",
                   MB_ICONQUESTION | MB_YESNO) == IDYES)
        Purge(owner, stored);
}

/* Bytes an item takes: a file's size, a folder's files'. */
static DWORD Weight(LPCSTR path)
{
    DOSENTRY e;
    DWORD bytes = 0;
    WORD files = 0, folders = 0;

    if (!PathIsFolder(path))
        return DosGetEntry(path, &e) ? e.size : 0;
    FileOperationTotals(path, &bytes, &files, &folders);
    return bytes;
}

typedef struct { char name[13]; DWORD when, weight; } TRIMITEM;

/* After items went to the bins: while one holds more than its share of
   its drive, its oldest items are deleted for good, as in Windows 98. */
void RecycleBinTrim(HWND owner)
{
    HGLOBAL hk = GlobalAlloc(GMEM_MOVEABLE, LIST_MAX), hi = GlobalAlloc(GMEM_MOVEABLE, TRIM_MAX * sizeof(TRIMITEM));
    LPSTR keys = hk ? GlobalLock(hk) : NULL, k;
    TRIMITEM FAR *items = hi ? (TRIMITEM FAR *)GlobalLock(hi) : NULL;
    char info[32], value[MAX_PATH + 16], stored[MAX_PATH];
    DWORD total, free, limit, held;
    int percent = SettingsGetInt("Recycle Bin", "MaxPercent", 10), d, n, i, old;
    PATHLIST purge;

    for (d = 2; keys && items && d < 26; d++) {
        if (!FixedDrive(d) || !DosDiskSpace(d, &total, &free))
            continue;
        InfoPath('A' + d, info);
        GetPrivateProfileString("Files", NULL, "", keys, LIST_MAX, info);
        for (n = 0, held = 0, k = keys; *k && n < TRIM_MAX; k += lstrlen(k) + 1) {
            GetPrivateProfileString("Files", k, "", value, sizeof value, info);
            BinDir('A' + d, stored);
            PathAppend(stored, k);
            lstrcpyn(items[n].name, k, sizeof items[0].name);
            items[n].when = MAKELONG(atoi(_fstrchr(value, ',') ? _fstrchr(value, ',') + 1 : value), atoi(value));
            held += items[n++].weight = Weight(stored);
        }
        limit = total / 100 * percent;
        PathListInit(&purge);
        while (held > limit && n) {
            for (old = 0, i = 1; i < n; i++)
                if (items[i].when < items[old].when)
                    old = i;
            BinDir('A' + d, stored);
            PathAppend(stored, items[old].name);
            PathListAdd(&purge, stored);
            held -= items[old].weight;
            items[old] = items[--n];
        }
        if (purge.count)
            Purge(owner, &purge);
        PathListFree(&purge);
    }
    if (keys)
        GlobalUnlock(hk);
    if (items)
        GlobalUnlock(hi);
    if (hk)
        GlobalFree(hk);
    if (hi)
        GlobalFree(hi);
}

void RecycleBinEmptyAll(HWND owner)
{
    PATHLIST all;
    char text[80];

    PathListInit(&all);
    ForEach(ListItem, &all);
    if (all.count) {
        wsprintf(text, "Are you sure you want to delete all of the %d items in the Recycle Bin?", all.count);
        if (MessageBox(owner, text, "Confirm Multiple File Delete", MB_ICONQUESTION | MB_YESNO) == IDYES) {
            Purge(owner, &all);
            SoundPlay(SOUND_EMPTYBIN);
        }
    }
    PathListFree(&all);
}

DWORD RecycleBinDrive(HWND owner, int drive, BOOL empty)
{
    PATHLIST all, mine;
    LPSTR p;
    DWORD bytes = 0;

    PathListInit(&all);
    PathListInit(&mine);
    ForEach(ListItem, &all);
    for (p = PathListLock(&all); *p; p = PathListNext(p))
        if ((p[0] | 0x20) - 'a' == drive) {
            bytes += Weight(p);
            PathListAdd(&mine, p);
        }
    PathListUnlock(&all);
    if (empty && mine.count)
        Purge(owner, &mine);
    PathListFree(&all);
    PathListFree(&mine);
    return bytes;
}
