/* The view and order of folder windows and the desktop: [Folder] and
   [Desktop] the last chosen; or (Folder Options' Remember each folder's
   view settings) each folder's own, and [Folder] the one of folders that
   have none: Large Icons, or Like Current Folder's. */
#include "internal.h"

static LPCSTR ViewSection(HWND iv)
{
    return (GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP) ? "Desktop" : "Folder";
}

/* Folder Options' Remember each folder's view settings: [Folder Views]
   path="view sort descending stamp", the 50 used last kept. */
#define VIEWS       "Folder Views"
#define MAX_VIEWS   50

static LPCSTR OwnView(HWND iv)
{
    LPCSTR dir = IconViewFolder(iv);

    if (!g_viewOptions.rememberViews || (GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP))
        return NULL;
    return !*dir ? "My Computer" : PathOnDisk(dir) ? dir : NULL;
}

static int Number(LPCSTR *p)
{
    int n = 0;

    while (**p == ' ')
        (*p)++;
    while (**p >= '0' && **p <= '9')
        n = n * 10 + *(*p)++ - '0';
    return n;
}

/* The new folder's place: the one used longest ago goes when full. */
static void MakeRoom(void)
{
    char oldest[MAX_PATH], v[24];
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 8192);
    LPSTR keys, k;
    LPCSTR p;
    int n = 0, low = 0x7FFF, s;

    if (!h)
        return;
    keys = GlobalLock(h);
    GetPrivateProfileString(VIEWS, NULL, "", keys, 8192, g_ini);
    for (k = keys; *k; k += lstrlen(k) + 1, n++) {
        SettingsGetString(VIEWS, k, "", v, sizeof v);
        p = v;
        Number(&p);
        Number(&p);
        Number(&p);
        if ((s = Number(&p)) < low) {
            low = s;
            lstrcpyn(oldest, k, sizeof oldest);
        }
    }
    if (n >= MAX_VIEWS)
        SettingsSetString(VIEWS, oldest, NULL);
    GlobalUnlock(h);
    GlobalFree(h);
}

/* own: the folder's own if it has one; else (and for a window not showing
   one yet) [Folder]'s. */
void FolderViewsLoad(HWND iv, BOOL own)
{
    LPCSTR section = ViewSection(iv), dir = own ? OwnView(iv) : NULL, p;
    int mode = SettingsGetInt(section, "View", IVM_LARGE), sort = SettingsGetInt(section, "Sort", IVSORT_NAME);
    int desc = SettingsGetInt(section, "Descending", 0);
    char v[24];

    if (dir) {
        SettingsGetString(VIEWS, dir, "", v, sizeof v);
        if (*v) {
            p = v;
            mode = Number(&p);
            sort = Number(&p);
            desc = Number(&p);
        }
    }
    if (!(GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP))
        IconViewSetMode(iv, mode);
    IconViewSort(iv, sort, desc);
}

void FolderViewsSave(HWND iv)
{
    LPCSTR section = ViewSection(iv), dir = OwnView(iv);
    char v[24];
    int stamp;

    if (dir) {                          /* the folder's own */
        SettingsGetString(VIEWS, dir, "", v, sizeof v);
        if (!*v)
            MakeRoom();
        stamp = SettingsGetInt("Folders", "ViewStamp", 0) + 1;
        SettingsSetInt("Folders", "ViewStamp", stamp);
        wsprintf(v, "%d %d %d %d", IconViewMode(iv), IconViewSortKey(iv), IconViewSortDescending(iv), stamp);
        SettingsSetString(VIEWS, dir, v);
        return;
    }
    if (!(GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP))
        SettingsSetInt(section, "View", IconViewMode(iv));
    SettingsSetInt(section, "Sort", IconViewSortKey(iv));
    SettingsSetInt(section, "Descending", IconViewSortDescending(iv));
}

/* Folder Options' Like Current Folder (iv's view for every folder) and
   Reset All Folders (Large Icons by name): no folder keeps its own. */
void FolderViewsDefault(HWND iv)
{
    SettingsSetString(VIEWS, NULL, NULL);
    SettingsSetInt("Folder", "View", iv ? IconViewMode(iv) : IVM_LARGE);
    SettingsSetInt("Folder", "Sort", iv ? IconViewSortKey(iv) : IVSORT_NAME);
    SettingsSetInt("Folder", "Descending", iv ? IconViewSortDescending(iv) : 0);
}
