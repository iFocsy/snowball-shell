/*
 * Quick Launch, as on Windows 98's taskbar: Show Desktop, then the
 * shortcuts of the Quick Launch folder (next to SNOWBALL.EXE), as flat
 * buttons with small icons. Files dropped on it become shortcuts there.
 * The taskbar gives the band its place (a row, or rows down a taskbar at
 * a side of the screen), then hit-tests and paints through these.
 */
#include "internal.h"

#define MAX_QL      20
#define GRIP        7                   /* the buttons, that far right of their band's grip */
#define BTN_W       23
#define BTN_H       22

typedef struct {
    HICON icon;
    char  file[80];                     /* in the folder; "" for Show Desktop */
} QLITEM;

static HGLOBAL mem;
static QLITEM FAR *items;
static int count;
static BOOL shown;
static HWND hiddenBy[40];               /* what Show Desktop minimized, front first (MAX_TASKS) */
static int nHidden;
static int menuItem;                    /* the button whose menu is open */
static RECT band;                       /* where the taskbar put it, its grip included */
static BOOL vertical;                   /* down a taskbar at a side: rows of buttons */

static void Folder(LPSTR out)
{
    lstrcpy(out, g_home);
    lstrcat(out, "Quick Launch");
}

/* The folder made on the first run, with Windows Explorer in it. */
static void FirstRun(LPCSTR dir)
{
    if (SettingsGetInt("Taskbar", "QuickLaunchMade", 0))
        return;
    if (PathIsFolder(dir) || DosMakeFolder(dir))
        ProgramManagerAddExplorer(dir);
    SettingsSetInt("Taskbar", "QuickLaunchMade", 1);
}

void QuickLaunchLoad(void)
{
    char dir[MAX_PATH], spec[MAX_PATH], path[MAX_PATH];
    DOSENTRY e;
    DOSFIND f;

    shown = SettingsGetInt("Taskbar", "QuickLaunch", 1);
    if (!mem && (mem = GlobalAlloc(GMEM_MOVEABLE, MAX_QL * sizeof(QLITEM))) != NULL)
        items = (QLITEM FAR *)GlobalLock(mem);
    if (!items)
        return;
    items[0].icon = IconStock(IDI_SHOWDESKTOP);
    items[0].file[0] = '\0';
    count = 1;
    Folder(dir);
    FirstRun(dir);
    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (DosFindFirst(spec, &f, &e)) {
        do {
            if ((e.attr & (FA_DIR | FA_HIDDEN)) || lstrlen(e.name) >= (int)sizeof items[0].file)
                continue;
            lstrcpy(path, dir);
            PathAppend(path, e.name);
            lstrcpy(items[count].file, e.name);
            items[count++].icon = IconForFile(path, FALSE);
        } while (count < MAX_QL && DosFindNext(&f, &e));
        DosFindClose(&f);
    }
}

/* Its buttons per row down a side of the given width. */
static int PerRow(int width)
{
    return max(1, width / BTN_W);
}

int QuickLaunchLength(BOOL down, int width)
{
    if (!shown || !count)
        return 0;
    return down ? GRIP + (count + PerRow(width) - 1) / PerRow(width) * BTN_H : GRIP + count * BTN_W;
}

void QuickLaunchPlace(const RECT *r, BOOL down)
{
    band = *r;
    vertical = down;
}

static void ItemRect(int i, RECT *r)
{
    int n = PerRow(band.right - band.left);

    if (vertical)
        SetRect(r, band.left + i % n * BTN_W, band.top + GRIP + i / n * BTN_H, 0, 0);
    else
        SetRect(r, band.left + GRIP + i * BTN_W, band.top, 0, 0);
    r->right = r->left + BTN_W;
    r->bottom = r->top + BTN_H;
}

int QuickLaunchHitTest(int x, int y)
{
    POINT p;
    RECT r;
    int i;

    p.x = x;
    p.y = y;
    for (i = 0; shown && i < count; i++) {
        ItemRect(i, &r);
        if (PtInRect(&r, p))
            return i;
    }
    return -1;
}

BOOL QuickLaunchHas(int x, int y)
{
    POINT p;

    p.x = x;
    p.y = y;
    return QuickLaunchLength(vertical, band.right - band.left) && PtInRect(&band, p);
}

/* A band's grip, as Windows 98's toolbars have: a thin raised bar at its
   start, across the band (along its top down a side). */
void QuickLaunchPaintGrip(HDC dc, const RECT *b, BOOL down)
{
    RECT r;

    if (down)
        SetRect(&r, b->left + 2, b->top, b->right - 2, b->top + 3);
    else
        SetRect(&r, b->left, b->top + 2, b->left + 3, b->bottom - 2);
    DrawFrame(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
}

/* Flat buttons: raised under the mouse (hot), sunken while pressed. */
void QuickLaunchPaint(HDC dc, int hot, int pressed)
{
    RECT r;
    int i, down;

    if (!QuickLaunchLength(vertical, band.right - band.left))
        return;
    QuickLaunchPaintGrip(dc, &band, vertical);
    for (i = 0; i < count; i++) {
        ItemRect(i, &r);
        down = i == pressed && i == hot;
        if (down || i == hot)
            DrawFrame(dc, &r, GetSysColor(down ? COLOR_BTNSHADOW : COLOR_BTNHIGHLIGHT),
                  GetSysColor(down ? COLOR_BTNHIGHLIGHT : COLOR_BTNSHADOW));
        DrawIconImage(dc, r.left + (BTN_W - 16) / 2 + down, r.top + (r.bottom - r.top - 16) / 2 + down, items[i].icon, TRUE);
    }
}

#ifdef SNOWBALL_HOOKS
int QuickLaunchCount(void)              /* the tests' hooks (src\hooks) */
{
    return count;
}
#endif

void QuickLaunchName(int i, LPSTR out)
{
    if (i == 0)
        lstrcpy(out, "Show Desktop");
    else
        DisplayNameOfFile(items[i].file, out);
}

static void ItemPath(int i, LPSTR out)
{
    Folder(out);
    PathAppend(out, items[i].file);
}

/* Show Desktop: every window minimized and remembered, or (when they still are) those
   back as they were; SD_MINIMIZE and SD_RESTORE do only that (Win+M,
   Win+Shift+M). */
void QuickLaunchShowDesktop(int how)
{
    HWND w;
    int i;
    BOOL back = nHidden > 0;

    for (i = 0; i < nHidden; i++)
        if (!IsWindow(hiddenBy[i]) || !IsIconic(hiddenBy[i]))
            back = FALSE;
    if (how == SD_RESTORE && !back)
        return;
    if (back && how != SD_MINIMIZE) {
        while (nHidden)                 /* the front one last, so it is in front again */
            ShowWindow(hiddenBy[--nHidden], SW_RESTORE);
        return;
    }
    nHidden = 0;
    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w && nHidden < sizeof hiddenBy / sizeof hiddenBy[0];
         w = GetWindow(w, GW_HWNDNEXT))
        if (TaskbarHasButton(w) && !IsIconic(w))
            hiddenBy[nHidden++] = w;
    TaskbarArrange(ARR_MINALL);
}

void QuickLaunchOpen(HWND owner, int i)
{
    char path[MAX_PATH];

    if (i == 0)
        QuickLaunchShowDesktop(SD_TOGGLE);
    else if (i > 0 && i < count) {
        ItemPath(i, path);
        LaunchItem(owner, path);
    }
}

/* A shortcut's menu; its command comes to the owner (WM_COMMAND, QLC_*),
   which hands it to QuickLaunchCommand. */
void QuickLaunchMenu(HWND owner, int i)
{
    HMENU m;
    POINT p;

    if (i <= 0 || i >= count)
        return;
    menuItem = i;
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, QLC_OPEN, "&Open");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, QLC_DELETE, "&Delete");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, QLC_PROPS, "P&roperties");
    GetCursorPos(&p);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, 0, owner, NULL);
    DestroyMenu(m);
}

void QuickLaunchCommand(HWND owner, UINT cmd)
{
    char path[MAX_PATH];
    PATHLIST l;

    if (menuItem <= 0 || menuItem >= count)
        return;
    ItemPath(menuItem, path);
    PathListInit(&l);
    PathListAdd(&l, path);
    if (cmd == QLC_OPEN)
        LaunchItem(owner, path);
    else if (cmd == QLC_DELETE) {
        FileCommandsDelete(owner, &l, FALSE);
        QuickLaunchLoad();
        InvalidateRect(owner, NULL, FALSE);
    } else if (cmd == QLC_PROPS)
        PropertiesShow(owner, &l, IVK_FILE);
    PathListFree(&l);
}

/* Files dropped on the strip: a shortcut to each (shortcuts are copied). */
void QuickLaunchAddFiles(HDROP drop)
{
    char dir[MAX_PATH], file[MAX_PATH], to[MAX_PATH];
    SHORTCUT s;
    int i, n = DragQueryFile(drop, (UINT)-1, NULL, 0);

    Folder(dir);
    for (i = 0; i < n; i++) {
        DragQueryFile(drop, i, file, sizeof file);
        if (PathIsShortcut(file) && ShortcutRead(file, &s)) {
            lstrcpy(to, dir);
            PathAppend(to, PathFileName(file));
            ShortcutWrite(to, &s);
        } else {
            ShortcutCreate(file, dir, TRUE);
        }
    }
    QuickLaunchLoad();
}
