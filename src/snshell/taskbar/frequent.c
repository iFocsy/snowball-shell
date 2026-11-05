/*
 * The two-column Start menu's lists, as Windows XP's: programs pinned to it
 * (SNOWBALL.INI [Pinned] 1..12, in order) and those started most ([Most
 * Used] path=times; the 30 counted last are kept). Their items have a menu
 * of their own: Open, Pin or Unpin, Remove from This List, Properties;
 * nothing that would delete or move the program.
 */
#include "internal.h"

#define MAX_PINNED  12
#define MAX_USED    FREQUENT_MAX
#define PINNED      "Pinned"
#define USED        "Most Used"
#define FQC_OPEN    1
#define FQC_PIN     2
#define FQC_FORGET  3
#define FQC_PROPS   4

void FrequentPinned(PATHLIST *l)
{
    char key[4], path[MAX_PATH];
    int i;

    for (i = 1; i <= MAX_PINNED; i++) {
        wsprintf(key, "%d", i);
        SettingsGetString(PINNED, key, "", path, sizeof path);
        if (*path && DosGetAttributes(path) != 0xFFFF)
            PathListAdd(l, path);
    }
}

BOOL FrequentIsPinned(LPCSTR path)
{
    char p[MAX_PATH];
    PATHLIST l;
    int i;
    BOOL found = FALSE;

    PathListInit(&l);
    FrequentPinned(&l);
    for (i = 0; !found && PathListAt(&l, i, p); i++)
        found = !lstrcmpi(p, path);
    PathListFree(&l);
    return found;
}

/* Pinned (at the end) or not; the others keep their order. */
void FrequentPin(LPCSTR path, BOOL on)
{
    char key[4], p[MAX_PATH];
    PATHLIST l;
    int i, n = 0;

    PathListInit(&l);
    FrequentPinned(&l);
    for (i = 1; i <= MAX_PINNED; i++) {
        wsprintf(key, "%d", i);
        SettingsSetString(PINNED, key, NULL);
    }
    for (i = 0; PathListAt(&l, i, p); i++)
        if (lstrcmpi(p, path)) {
            wsprintf(key, "%d", ++n);
            SettingsSetString(PINNED, key, p);
        }
    if (on && n < MAX_PINNED) {
        wsprintf(key, "%d", ++n);
        SettingsSetString(PINNED, key, path);
    }
    PathListFree(&l);
}

/* Each key of [Most Used] in turn ("" ends), in a block of the caller's. */
static LPSTR UsedKeys(HGLOBAL *h)
{
    LPSTR keys;

    if ((*h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 4096)) == NULL)
        return NULL;
    keys = GlobalLock(*h);
    GetPrivateProfileString(USED, NULL, "", keys, 4096, g_ini);
    return keys;
}

static void FreeKeys(HGLOBAL h)
{
    GlobalUnlock(h);
    GlobalFree(h);
}

/* A program started (or a shortcut to one): counted, unless a setup's. */
void FrequentCount(LPCSTR path)
{
    static const char *skip[] = { "UNINST", "README", "SETUP", "INSTALL" };
    char name[16], least[MAX_PATH];
    HGLOBAL h;
    LPSTR k;
    int i, n = 0, low = 0x7FFF, c;

    lstrcpyn(name, PathFileName((LPSTR)path), sizeof name);
    AnsiUpper(name);
    for (i = 0; i < sizeof skip / sizeof skip[0]; i++)
        if (_fstrstr(name, skip[i]))
            return;
    if ((c = SettingsGetInt(USED, path, 0)) == 0 && (k = UsedKeys(&h)) != NULL) {
        for (; *k; k += lstrlen(k) + 1, n++)    /* full: the least started goes */
            if ((i = SettingsGetInt(USED, k, 0)) < low) {
                low = i;
                lstrcpyn(least, k, sizeof least);
            }
        if (n >= MAX_USED)
            SettingsSetString(USED, least, NULL);
        FreeKeys(h);
    }
    SettingsSetInt(USED, path, c + 1);
}

/* The n started most, most first; files gone left out. Each count is read
   once (a menu opening). */
void FrequentMostUsed(PATHLIST *l, int n)
{
    LPSTR key[MAX_USED], keys, k;
    int times[MAX_USED], m = 0, i, best;
    HGLOBAL h;

    if ((keys = UsedKeys(&h)) == NULL)
        return;
    for (k = keys; *k && m < MAX_USED; k += lstrlen(k) + 1, m++) {
        key[m] = k;
        times[m] = SettingsGetInt(USED, k, 0);
    }
    while (n > 0) {
        for (best = -1, i = 0; i < m; i++)
            if (times[i] > 0 && (best < 0 || times[i] > times[best]))
                best = i;
        if (best < 0)
            break;
        times[best] = 0;                /* taken */
        if (DosGetAttributes(key[best]) != 0xFFFF) {
            PathListAdd(l, key[best]);
            n--;
        }
    }
    FreeKeys(h);
}

BOOL FrequentAnyUsed(void)
{
    char k[4];

    return GetPrivateProfileString(USED, NULL, "", k, sizeof k, g_ini) != 0;
}

void FrequentClear(void)
{
    SettingsSetString(USED, NULL, NULL);
}

/* A pinned or most used item's own menu (the Start menu's right click). */
void FrequentMenu(HWND owner, LPCSTR path)
{
    HMENU m = CreatePopupMenu();
    BOOL pinned = FrequentIsPinned(path);
    PATHLIST l;

    AppendMenu(m, MF_STRING, FQC_OPEN, "&Open");
    AppendMenu(m, MF_STRING, FQC_PIN, pinned ? "Unp&in from Start menu" : "P&in to Start menu");
    if (SettingsGetInt(USED, path, 0))
        AppendMenu(m, MF_STRING, FQC_FORGET, "Remove from This &List");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, FQC_PROPS, "P&roperties");
    switch (PopupMenuTrack(m, owner)) {
    case FQC_OPEN:   LaunchItem(owner, path); break;
    case FQC_PIN:    FrequentPin(path, !pinned); break;
    case FQC_FORGET: SettingsSetString(USED, path, NULL); break;
    case FQC_PROPS:
        PathListInit(&l);
        PathListAdd(&l, path);
        PropertiesShow(owner, &l, IVK_FILE);
        PathListFree(&l);
        break;
    }
    DestroyMenu(m);
}
