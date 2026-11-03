/* Folder windows, as in Windows 98: the Standard Buttons and the address
   bar above a view and its status bar, browsing in place with a history
   for Back and Forward; Explorer windows add the folder tree on the left, and
   any folder window shows or hides it (View > Explorer Bar). Folder Options
   may open each folder in its own window instead, as Windows 95 did; Ctrl
   with a double click does the other.
   "" is My Computer (the drives), BIN_PATH the Recycle Bin. A folder that
   already has a plain window comes to the front instead of opening twice. */
#include "internal.h"

#define ID_TREE         2
#define ID_TOOLS        3
#define ID_ADDRESS      4
#define BAR_H           18              /* "All Folders" above the tree */
#define SPLIT_W         4
#define SIZE_W          110             /* the status bar's size panel */
#define DRIVE_W         110             /* ... and its drive's free space */
#define GRIP_W          10              /* a band's grip, before what it holds */
#define BAND_GAP        2               /* above and below what a band holds */
#define LIST_H          200             /* the address list, dropped down */
#define COMPACT_MIN     120             /* Compact: the address field's least width */
#define HISTORY         16              /* folders Back and Forward remember */
#define SECTION         "Folders"
#define CLOSE_W         14              /* the tree's close button, on its title */
#define FWM_PLACE       (WM_USER + 300) /* wParam: the place chosen in the address list */
#define FWM_GO          (WM_USER + 301) /* Enter in the address bar */

typedef struct {
    int  count, at;                     /* at: the folder shown */
    char path[HISTORY][MAX_PATH];
} TRAIL;

typedef struct {
    HWND    view, tree, tools, address; /* tree: Explorer windows only */
    int     split;                      /* width of the tree pane */
    BOOL    splitting;
    DWORD   free;                       /* bytes free on the folder's disk */
    char    filter[16];                 /* View > Filter: only these files ("*.txt"), "" all */
    HGLOBAL hTrail;
    TRAIL  *trail;
    char    path[MAX_PATH];
} FOLDER;

static int opened;
static BOOL showTools = -1, showAddress, showLabels;   /* -1: not read yet */
static BOOL compact;                    /* both bands on one row, the buttons without labels */
static WNDPROC comboProc, editProc, listProc;
#define ADDRESS_OF  "SbAddressOf"           /* an address list's combo box */
static BOOL keyClose;                       /* the address list closed by Enter or Esc */

#define Folder(w)   ((FOLDER *)LocalHeapPointer(GetWindowWord(w, 0)))

static BOOL IsBin(LPCSTR path)
{
    return !lstrcmp(path, BIN_PATH);
}

static HMENU MenuBar(BOOL bin)
{
    HMENU bar = CreateMenu(), go = CreatePopupMenu(), view = FileCommandsViewMenu(), bars = CreatePopupMenu(), help;
    HMENU explorer = CreatePopupMenu();

    AppendMenu(bars, MF_STRING, CMD_SHOWTOOLS, "&Standard Buttons");
    AppendMenu(bars, MF_STRING, CMD_SHOWADDRESS, "&Address Bar");
    AppendMenu(bars, MF_SEPARATOR, 0, NULL);
    AppendMenu(bars, MF_STRING, CMD_SHOWLABELS, "&Text Labels");
    AppendMenu(bars, MF_STRING, CMD_COMPACT, "&Compact");
    InsertMenu(view, 0, MF_BYPOSITION | MF_POPUP, (UINT)bars, "&Toolbars");
    AppendMenu(explorer, MF_STRING, CMD_FOLDERS, "Fol&ders");
    AppendMenu(explorer, MF_SEPARATOR, 0, NULL);
    AppendMenu(explorer, MF_STRING, CMD_NOBAR, "&None");
    InsertMenu(view, 1, MF_BYPOSITION | MF_POPUP, (UINT)explorer, "Explorer &Bar");
    InsertMenu(view, 2, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    AppendMenu(go, MF_STRING, CMD_BACK, "&Back\tAlt+Left Arrow");
    AppendMenu(go, MF_STRING, CMD_FORWARD, "&Forward\tAlt+Right Arrow");
    AppendMenu(go, MF_STRING, CMD_UP, "&Up One Level\tBackspace");
    AppendMenu(go, MF_SEPARATOR, 0, NULL);
    AppendMenu(go, MF_STRING, CMD_GOTO, "Go to &Folder...\tCtrl+G");
    AppendMenu(bar, MF_POPUP, (UINT)FileCommandsFileMenu(bin), "&File");
    AppendMenu(bar, MF_POPUP, (UINT)FileCommandsEditMenu(), "&Edit");
    AppendMenu(bar, MF_POPUP, (UINT)view, "&View");
    AppendMenu(bar, MF_POPUP, (UINT)go, "&Go");
    help = CreatePopupMenu();
    AppendMenu(help, MF_STRING, CMD_ABOUT, "&About Snowball");
    AppendMenu(bar, MF_POPUP, (UINT)help, "&Help");
    return bar;
}

/* ---- the bands: Standard Buttons, Address ---- */

static void AddressUpdate(HWND w);

/* What the buttons can do now. */
static void ToolsUpdate(HWND w)
{
    FOLDER *f = Folder(w);
    TRAIL *t = f->trail;

    ToolbarEnable(f->tools, CMD_BACK, t && t->at > 0);
    ToolbarEnable(f->tools, CMD_FORWARD, t && t->at < t->count - 1);
    ToolbarEnable(f->tools, CMD_UP, *f->path && !IsBin(f->path));
    ToolbarEnable(f->tools, CMD_CUT, FileCommandsAvailable(f->view, CMD_CUT));
    ToolbarEnable(f->tools, CMD_COPY, FileCommandsAvailable(f->view, CMD_COPY));
    ToolbarEnable(f->tools, CMD_PASTE, FileCommandsAvailable(f->view, CMD_PASTE));
    ToolbarEnable(f->tools, CMD_DELETE, FileCommandsAvailable(f->view, CMD_DELETE));
}

/* The folder shown, in the address bar. */
static void AddressUpdate(HWND w)
{
    FOLDER *f = Folder(w);
    char name[MAX_PATH];

    PlacesFill(f->address, f->path);
    if (IsBin(f->path) || !*f->path)
        PlacesName(f->path, name);
    else
        lstrcpy(name, f->path);
    SetWindowText(f->address, name);
    InvalidateRect(f->address, NULL, FALSE);   /* its icon */
}

/* Compact: both on one row, as Windows 98's bands dragged onto one */
#define OneRow()    (compact && showTools && showAddress)

static int BandsHeight(HWND w)
{
    FOLDER *f = Folder(w);
    RECT r;
    int h = 0;

    if (OneRow()) {
        GetWindowRect(f->address, &r);
        return max(ToolbarHeight(f->tools), r.bottom - r.top) + 2 * BAND_GAP + 4;
    }
    if (showTools)
        h += ToolbarHeight(f->tools) + 2 * BAND_GAP + 2;
    if (showAddress) {
        GetWindowRect(f->address, &r);  /* the field; the list is not counted while up */
        h += r.bottom - r.top + 2 * BAND_GAP + 2;
    }
    return h ? h + 2 : 0;               /* the line under the menu bar */
}

static int AddressLabelWidth(HDC dc)
{
    return (int)LOWORD(GetTextExtent(dc, "Address", 7)) + 6;
}

/* A band's grip: a raised bar, as Windows 98's toolbars have. */
static void Grip(HDC dc, int top, int bottom)
{
    RECT r;
    SetRect(&r, 2, top + 1, 5, bottom - 1);
    DrawFrame(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
}

static void Etched(HDC dc, int y, int right)
{
    RECT r;
    SetRect(&r, 0, y, right, y + 1);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNSHADOW));
    OffsetRect(&r, 0, 1);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT));
}

static void PaintBands(HWND w, HDC dc, const RECT *c)
{
    FOLDER *f = Folder(w);
    RECT r;
    int y, h;

    if (!BandsHeight(w))
        return;
    SetRect(&r, 0, 0, c->right, BandsHeight(w));
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
    Etched(dc, 0, c->right);
    y = 2;
    if (OneRow()) {                     /* the address field's band, then the buttons' */
        h = BandsHeight(w) - 4;
        Grip(dc, y, y + h);
        GetWindowRect(f->address, &r);
        ScreenToClient(w, (POINT *)&r.right);  /* the second grip, where the field ends */
        SetRect(&r, r.right + 2, y + 1, r.right + 5, y + h - 1);
        DrawFrame(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
        Etched(dc, y + h, c->right);
        return;
    }
    if (showTools) {
        h = ToolbarHeight(f->tools) + 2 * BAND_GAP;
        Grip(dc, y, y + h);
        Etched(dc, y + h, c->right);
        y += h + 2;
    }
    if (showAddress) {
        GetWindowRect(f->address, &r);
        h = r.bottom - r.top + 2 * BAND_GAP;
        Grip(dc, y, y + h);
        SetRect(&r, GRIP_W, y, GRIP_W + AddressLabelWidth(dc), y + h);
        DrawText(dc, "Address", -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        Etched(dc, y + h, c->right);
    }
}

static void ArrangeBands(HWND w, const RECT *c)
{
    FOLDER *f = Folder(w);
    HDC dc;
    HFONT old;
    int y = 2, label;

    ShowWindow(f->tools, showTools ? SW_SHOWNA : SW_HIDE);
    ShowWindow(f->address, showAddress ? SW_SHOWNA : SW_HIDE);
    if (OneRow()) {                     /* the buttons keep their width; the field takes the rest */
        RECT a;
        int h = BandsHeight(w) - 4, fieldW, toolsW = ToolbarWidth(f->tools);
        GetWindowRect(f->address, &a);
        fieldW = max(COMPACT_MIN, c->right - GRIP_W - toolsW - 2 * GRIP_W);
        MoveWindow(f->address, GRIP_W, y + (h - (a.bottom - a.top)) / 2, fieldW, LIST_H, TRUE);
        MoveWindow(f->tools, GRIP_W + fieldW + GRIP_W, y + (h - ToolbarHeight(f->tools)) / 2,
                   max(0, c->right - GRIP_W - fieldW - GRIP_W), ToolbarHeight(f->tools), TRUE);
        return;
    }
    if (showTools) {
        MoveWindow(f->tools, GRIP_W, y + BAND_GAP, max(0, c->right - GRIP_W), ToolbarHeight(f->tools), TRUE);
        y += ToolbarHeight(f->tools) + 2 * BAND_GAP + 2;
    }
    if (showAddress) {
        dc = GetDC(w);
        old = SelectObject(dc, DrawFont(FALSE));
        label = AddressLabelWidth(dc);
        SelectObject(dc, old);
        ReleaseDC(w, dc);
        MoveWindow(f->address, GRIP_W + label, y + BAND_GAP, max(0, c->right - GRIP_W - label - 2), LIST_H, TRUE);
    }
}

#define ICON_X          4               /* the address field's icon, from its left */
#define ICON_GAP        3               /* and the text after it */

/* The folder's icon left of the edit, inside the field's sunken edge. */
static void PaintAddressIcon(HWND w)
{
    FOLDER *f = Folder(GetParent(w));
    RECT r, c;
    HDC dc;

    if (!f)
        return;
    GetClientRect(w, &c);
    SetRect(&r, 2, 2, ICON_X + 16 + ICON_GAP, c.bottom - 2);
    dc = GetDC(w);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_WINDOW));
    DrawIconImage(dc, ICON_X, (c.bottom - 16) / 2, PlacesIcon(f->path), TRUE);
    ReleaseDC(w, dc);
}

/* The edit after the icon, its text centred in the field (Windows 3.1
   makes it the field's height, its text at the top). Its bottom stays:
   the list opens below it. */
static void PlaceAddressEdit(HWND w)
{
    HWND edit = GetWindow(w, GW_CHILD);
    TEXTMETRIC tm;
    RECT r, c, want;
    HFONT old;
    HDC dc;

    dc = GetDC(edit);
    old = SelectObject(dc, (HFONT)SendMessage(edit, WM_GETFONT, 0, 0L));
    GetTextMetrics(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(edit, dc);
    GetClientRect(w, &c);
    GetWindowRect(edit, &r);
    ScreenToClient(w, (POINT FAR *)&r.left);
    ScreenToClient(w, (POINT FAR *)&r.right);
    SetRect(&want, ICON_X + 16 + ICON_GAP - 3,      /* the edit's text starts 3 pixels in */
            max(0, (c.bottom - tm.tmHeight) / 2 - 3), r.right, r.bottom);
    if (!EqualRect(&r, &want))
        MoveWindow(edit, want.left, want.top, want.right - want.left, want.bottom - want.top, TRUE);
}

/* The address field shows the folder's icon before its text: the edit
   inside moves aside, the icon is painted in the space it leaves, again
   whenever the field is painted (the skin repaints it after these). */
LRESULT CALLBACK __export FolderWindowAddressProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res = CallWindowProc((FARPROC)comboProc, w, msg, wp, lp);

    switch (msg) {
    case WM_SIZE: case WM_SETFONT:      /* the combo box put the edit back */
        PlaceAddressEdit(w);
        /* and the icon */
    case WM_PAINT: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_SETFOCUS:
    case WM_KILLFOCUS: case WM_ENABLE: case CB_SHOWDROPDOWN: case WM_KEYDOWN:
    case WM_CHAR: case WM_SETTEXT: case CB_SETCURSEL: case CB_SELECTSTRING: case CB_RESETCONTENT:
        if (IsWindow(w))
            PaintAddressIcon(w);
    }
    return res;
}

/* The address list drops below the whole field and as wide, as Windows
   98's (Windows 3.1 places it from the edit, which the icon moved). */
LRESULT CALLBACK __export FolderWindowListProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    WINDOWPOS FAR *p = (WINDOWPOS FAR *)lp;
    HWND combo = (HWND)GetProp(w, ADDRESS_OF);
    RECT r;

    if (msg == WM_WINDOWPOSCHANGING && combo) {
        GetWindowRect(combo, &r);
        if (!(p->flags & SWP_NOMOVE))
            p->x = r.left;
        if (!(p->flags & SWP_NOSIZE))
            p->cx = r.right - r.left;
    } else if (msg == WM_DESTROY) {
        RemoveProp(w, ADDRESS_OF);
    }
    return CallWindowProc((FARPROC)listProc, w, msg, wp, lp);
}

/* Enter goes there; Esc puts the folder shown back. */
LRESULT CALLBACK __export FolderWindowAddressEditProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND frame = GetParent(GetParent(w));

    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        keyClose = TRUE;                /* an open list closes first, choosing nothing */
        SendMessage(GetParent(w), CB_SHOWDROPDOWN, FALSE, 0L);
        keyClose = FALSE;
        if (wp == VK_RETURN)
            PostMessage(frame, FWM_GO, 0, 0L);
        else
            AddressUpdate(frame);
        return 0;
    }
    if (msg == WM_CHAR && (wp == '\r' || wp == 27))
        return 0;                       /* no beep */
    return CallWindowProc((FARPROC)editProc, w, msg, wp, lp);
}

static void ReadBands(void)
{
    if (showTools >= 0)
        return;
    showTools = SettingsGetInt(SECTION, "StandardButtons", 1) != 0;
    showAddress = SettingsGetInt(SECTION, "AddressBar", 1) != 0;
    showLabels = SettingsGetInt(SECTION, "TextLabels", 1) != 0;
    compact = SettingsGetInt(SECTION, "Compact", 0) != 0;
}

static BOOL MakeBands(HWND w)
{
    FOLDER *f = Folder(w);
    HWND edit, list;
    char cls[12];

    ReadBands();
    f->tools = CreateWindow(TOOLBAR_CLASS, NULL, WS_CHILD, 0, 0, 0, 0, w, (HMENU)ID_TOOLS, g_hInst, NULL);
    f->address = CreateWindow("COMBOBOX", NULL, WS_CHILD | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL |
                              CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 0, LIST_H, w, (HMENU)ID_ADDRESS, g_hInst, NULL);
    if (!f->tools || !f->address)
        return FALSE;
    ToolbarSetImages(f->tools, IDB_FOLDERBAR, IDB_FOLDERBARCOLD, IDB_FOLDERBAROFF);
    ToolbarShowLabels(f->tools, showLabels && !compact);
    ToolbarAdd(f->tools, FBI_BACK, CMD_BACK, "Back", TBS_BUTTON);
    ToolbarAdd(f->tools, FBI_FORWARD, CMD_FORWARD, "Forward", TBS_BUTTON);
    ToolbarAdd(f->tools, FBI_UP, CMD_UP, "Up", TBS_BUTTON);
    ToolbarAdd(f->tools, 0, 0, NULL, TBS_SEPARATOR);
    ToolbarAdd(f->tools, FBI_CUT, CMD_CUT, "Cut", TBS_BUTTON);
    ToolbarAdd(f->tools, FBI_COPY, CMD_COPY, "Copy", TBS_BUTTON);
    ToolbarAdd(f->tools, FBI_PASTE, CMD_PASTE, "Paste", TBS_BUTTON);
    ToolbarAdd(f->tools, 0, 0, NULL, TBS_SEPARATOR);
    ToolbarAdd(f->tools, FBI_DELETE, CMD_DELETE, "Delete", TBS_BUTTON);
    ToolbarAdd(f->tools, FBI_PROPERTIES, CMD_PROPERTIES, "Properties", TBS_BUTTON);
    ToolbarAdd(f->tools, 0, 0, NULL, TBS_SEPARATOR);
    ToolbarAdd(f->tools, FBI_VIEWS, CMD_VIEWS, "Views", TBS_DROPDOWN);
    SkinControl(f->address);            /* the Windows 98 look first, the icon over it */
    comboProc = (WNDPROC)SetWindowLong(f->address, GWL_WNDPROC, (LONG)(FARPROC)FolderWindowAddressProc);
    edit = GetWindow(f->address, GW_CHILD);
    editProc = (WNDPROC)SetWindowLong(edit, GWL_WNDPROC, (LONG)(FARPROC)FolderWindowAddressEditProc);
    AutoCompleteAttach(edit);
    /* its list, a window of the desktop made with it: the newest, so the
       first list from the top (only topmost windows go above new ones) */
    for (list = GetWindow(GetDesktopWindow(), GW_CHILD); list; list = GetWindow(list, GW_HWNDNEXT)) {
        GetClassName(list, cls, sizeof cls);
        if (!lstrcmpi(cls, "ComboLBox")) {
            SetProp(list, ADDRESS_OF, f->address);
            listProc = (WNDPROC)SetWindowLong(list, GWL_WNDPROC, (LONG)(FARPROC)FolderWindowListProc);
            break;
        }
    }
    SendMessage(f->address, WM_SIZE, 0, 0L);    /* the edit moves aside now */
    return TRUE;
}

/* View > Toolbars: this window now, new ones later. */
static void ToggleBand(HWND w, UINT cmd)
{
    FOLDER *f = Folder(w);

    if (cmd == CMD_SHOWTOOLS)
        SettingsSetInt(SECTION, "StandardButtons", showTools = !showTools);
    else if (cmd == CMD_SHOWADDRESS)
        SettingsSetInt(SECTION, "AddressBar", showAddress = !showAddress);
    else if (cmd == CMD_SHOWLABELS)
        SettingsSetInt(SECTION, "TextLabels", showLabels = !showLabels);
    else
        SettingsSetInt(SECTION, "Compact", compact = !compact);
    ToolbarShowLabels(f->tools, showLabels && !compact);
    SendMessage(w, WM_SIZE, 0, 0L);
}

/* Views: the next one, or the list of them from its arrow. */
static void Views(HWND w, BOOL list)
{
    FOLDER *f = Folder(w);
    HMENU m;
    RECT r;

    if (!list) {
        FileCommandsRun(w, f->view, FileCommandsModeCommand((IconViewMode(f->view) + 1) % (IVM_THUMBS + 1)));
        return;
    }
    m = FileCommandsModesMenu(CreatePopupMenu(), IconViewMode(f->view));
    ToolbarButtonRect(f->tools, CMD_VIEWS, &r);
    ClientToScreen(f->tools, (POINT FAR *)&r.left);
    ClientToScreen(f->tools, (POINT FAR *)&r.right);
    TrackPopupMenu(m, TPM_LEFTALIGN, r.left, r.bottom, 0, w, NULL);
    DestroyMenu(m);
}

/* ---- the folder shown, and the way here ---- */

/* "Exploring - " with the tree, then the folder's name (or path). */
static void SetTitle(HWND w)
{
    FOLDER *f = Folder(w);
    char title[MAX_PATH + 12];

    lstrcpy(title, f->tree ? "Exploring - " : "");
    if (!*f->path)
        lstrcat(title, "My Computer");
    else if (IsBin(f->path))
        lstrcat(title, "Recycle Bin");
    else
        lstrcat(title, lstrlen(f->path) > 3 && !g_viewOptions.fullPath ? PathFileName(f->path) : f->path);
    if (f->filter[0])                   /* "TEST (*.txt)": not all is shown */
        wsprintf(title + lstrlen(title), " (%s)", (LPSTR)f->filter);
    SetWindowText(w, title);
}

/* Shows path; keep: the same folder again (refresh). */
static void Load(HWND w, LPCSTR path, BOOL keep)
{
    FOLDER *f = Folder(w);
    DWORD total;
    HMENU old;

    if (!keep && IsBin(path) != IsBin(f->path)) {       /* the File menu differs */
        old = GetMenu(w);
        SetMenu(w, MenuBar(IsBin(path)));
        DestroyMenu(old);
    }
    if (lstrcmpi(path, f->path))        /* the filter was for the folder left */
        f->filter[0] = '\0';
    lstrcpy(f->path, path);
    IconViewClear(f->view, keep);
    if (!*path)
        IconViewLoadDrives(f->view);
    else if (IsBin(path))
        RecycleBinLoad(f->view);
    else
        IconViewLoadFolder(f->view, path, f->filter[0] ? f->filter : NULL);
    f->free = 0;
    if (PathOnDisk(path))
        DosDiskSpace((path[0] | 0x20) - 'a', &total, &f->free);
    IconViewFinish(f->view);
    if (f->tree)
        TreeViewShowPath(f->tree, path);
    SetTitle(w);
    ToolsUpdate(w);
    AddressUpdate(w);
    InvalidateRect(w, NULL, FALSE);
}

/* A new folder in the history: what was ahead of the one shown goes. */
static void Navigate(HWND w, LPCSTR path)
{
    FOLDER *f = Folder(w);
    TRAIL *t = f->trail;

    if (t && (t->count == 0 || lstrcmpi(t->path[t->at], path))) {
        if (t->count)
            t->count = t->at + 1;
        if (t->count == HISTORY) {      /* full: the oldest goes */
            _fmemmove(t->path[0], t->path[1], (HISTORY - 1) * MAX_PATH);
            t->count--;
        }
        lstrcpy(t->path[t->count], path);
        t->at = t->count++;
    }
    Load(w, path, FALSE);
}

/* Folders open in windows of their own: Folder Options says so (Ctrl the
   other way); never in Explorer, whose tree goes along. */
static BOOL OwnWindows(HWND w)
{
    return !Folder(w)->tree && g_viewOptions.ownWindows != (GetKeyState(VK_CONTROL) < 0);
}

static void Step(HWND w, int by)
{
    TRAIL *t = Folder(w)->trail;

    if (t && t->at + by >= 0 && t->at + by < t->count) {
        t->at += by;
        Load(w, t->path[t->at], FALSE);
    }
}

static void Up(HWND w)
{
    FOLDER *f = Folder(w);
    char parent[MAX_PATH];

    if (!*f->path || IsBin(f->path))
        return;
    lstrcpy(parent, f->path);
    if (lstrlen(parent) <= 3)                   /* a drive root: up to My Computer */
        parent[0] = '\0';
    else
        PathToParent(parent);
    if (OwnWindows(w))
        FolderWindowOpen(parent);
    else
        Navigate(w, parent);
}

/* One folder or drive browses here, unless each has its own window;
   anything else opens. */
static void Open(HWND w)
{
    FOLDER *f = Folder(w);
    char path[MAX_PATH];
    int i = -1, k;

    if (IsBin(f->path))
        return;
    while ((i = IconViewNextSelected(f->view, i)) >= 0) {
        if (!IconViewItemPath(f->view, i, path))
            continue;
        k = IconViewItemKind(f->view, i);
        if (k == IVK_FILE)
            LaunchItem(w, path);
        else if (IconViewSelectedCount(f->view) == 1 && !OwnWindows(w)) {
            Navigate(w, path);          /* the view now shows other items */
            return;
        } else
            FolderWindowOpen(path);
    }
}

/* What the address bar was given: a place by name, a folder, or else a
   command or document, as Run takes them. */
static void Go(HWND w)
{
    FOLDER *f = Folder(w);
    char typed[MAX_PATH], path[MAX_PATH];
    int n;

    GetWindowText(f->address, typed, sizeof typed);
    lstrcpy(path, typed);
    if (lstrlen(path) == 2 && path[1] == ':')
        lstrcat(path, "\\");
    else if ((n = lstrlen(path)) > 3 && path[n - 1] == '\\')
        path[n - 1] = '\0';
    if (!lstrcmpi(path, "My Computer"))
        Navigate(w, "");
    else if (!lstrcmpi(path, "Recycle Bin"))
        Navigate(w, BIN_PATH);
    else if (!lstrcmpi(path, "Desktop")) {
        DesktopFolder(path);
        Navigate(w, path);
    } else if (*path && PathIsFolder(path)) {
        Navigate(w, path);
    } else {
        if (*path)
            LaunchCommand(w, typed, NULL, SW_SHOWNORMAL);
        AddressUpdate(w);
    }
}

/* ---- painting: bands, status bar, tree title, splitter ---- */

/* "3 object(s) selected | 1.23 KB | (drive) 1.87 GB free": where Windows 98
   shows Internet Explorer's zone ("My Computer"), the free space of the
   folder's drive, always there; My Computer shows the count alone (a
   drive selected: "Free Space: 1.87 GB, Capacity: 1.99 GB"), the Recycle
   Bin no drive. */
static void PaintStatus(HWND w, HDC dc, RECT *c)
{
    FOLDER *f = Folder(w);
    static const int widths[] = { 0, SIZE_W - 2, DRIVE_W };
    char text[80], size[24], room[32];
    LPCSTR texts[3];
    HICON icons[3];
    DWORD bytes = 0;
    int n = IconViewSelectedCount(f->view), i;

    for (i = 0; i < IconViewCount(f->view); i++)
        if (!n || IconViewNextSelected(f->view, i - 1) == i)
            bytes += IconViewItemSize(f->view, i);
    if (n)
        wsprintf(text, "%d object(s) selected", n);
    else
        wsprintf(text, "%d object(s)", IconViewCount(f->view));
    if (!*f->path && n == 1 && IconViewItemSize(f->view, i = IconViewNextSelected(f->view, -1))) {
        FormatSize(IconViewItemFree(f->view, i), size);
        FormatSize(IconViewItemSize(f->view, i), room);
        wsprintf(text, "Free Space: %s, Capacity: %s", (LPSTR)size, (LPSTR)room);
    }
    FormatSize(bytes, size);
    FormatSize(f->free, room);
    lstrcat(room, " free");
    texts[0] = text;
    texts[1] = size;
    texts[2] = room;
    icons[0] = icons[1] = NULL;
    icons[2] = f->path[1] == ':' ? IconForDrive((f->path[0] | 0x20) - 'a') : NULL;
    PaintStatusBar(dc, c, !*f->path ? 1 : f->path[1] == ':' ? 3 : 2, widths, texts, icons, !IsZoomed(w));
}

/* The tree's close button, at the right of its title. */
static void TreeClose(HWND w, RECT *r)
{
    FOLDER *f = Folder(w);
    int top = BandsHeight(w);

    SetRect(r, f->split - CLOSE_W - 2, top + (BAR_H - 12) / 2, f->split - 2, top + (BAR_H - 12) / 2 + 12);
}

/* A small flat X, as Windows 98's Explorer bar has. */
static void PaintTreeClose(HDC dc, const RECT *r)
{
    int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2, i;
    RECT t;

    for (i = 0; i < 6; i++) {
        SetRect(&t, x - 3 + i, y - 3 + i, x - 1 + i, y - 2 + i);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNTEXT));
        SetRect(&t, x + 1 - i, y - 3 + i, x + 3 - i, y - 2 + i);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNTEXT));
    }
}

static void Paint(HWND w)
{
    FOLDER *f = Folder(w);
    PAINTSTRUCT ps;
    RECT c, r;
    HFONT old;
    int top = BandsHeight(w);

    BeginPaint(w, &ps);
    GetClientRect(w, &c);
    old = SelectObject(ps.hdc, DrawFont(FALSE));
    SetBkMode(ps.hdc, TRANSPARENT);
    SetTextColor(ps.hdc, GetSysColor(COLOR_BTNTEXT));
    PaintBands(w, ps.hdc, &c);
    PaintStatus(w, ps.hdc, &c);
    if (f->tree) {
        SetRect(&r, 0, top, f->split, top + BAR_H);     /* the tree's title */
        DrawSolidRect(ps.hdc, &r, GetSysColor(COLOR_BTNFACE));
        DrawFrame(ps.hdc, &r, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
        InflateRect(&r, -6, 0);
        r.right -= CLOSE_W;
        DrawTextEllipsis(ps.hdc, "All Folders", &r, DT_LEFT | DT_NOPREFIX);
        TreeClose(w, &r);
        PaintTreeClose(ps.hdc, &r);
        SetRect(&r, f->split, top, f->split + SPLIT_W, c.bottom - STATUS_H);
        DrawSolidRect(ps.hdc, &r, GetSysColor(COLOR_BTNFACE));
    }
    SelectObject(ps.hdc, old);
    EndPaint(w, &ps);
}

static void Arrange(HWND w)
{
    FOLDER *f = Folder(w);
    RECT c;
    int h, x = 0, top;

    GetClientRect(w, &c);
    ArrangeBands(w, &c);
    top = BandsHeight(w);
    h = max(0, c.bottom - STATUS_H - top);
    if (f->tree) {
        f->split = max(60, min(f->split, c.right - 100));
        MoveWindow(f->tree, 0, top + BAR_H, f->split, max(0, h - BAR_H), TRUE);
        x = f->split + SPLIT_W;
    }
    MoveWindow(f->view, x, top, max(0, c.right - x), h, TRUE);
    InvalidateRect(w, NULL, FALSE);
}

static void StatusChanged(HWND w)
{
    RECT r;
    GetClientRect(w, &r);
    r.top = r.bottom - STATUS_H;
    InvalidateRect(w, &r, FALSE);
    ToolsUpdate(w);
}

/* The folder tree on the left (Explorer Bar > Folders), or none. */
static BOOL ShowTree(HWND w, BOOL on)
{
    FOLDER *f = Folder(w);
    char desk[MAX_PATH];

    if (on == (f->tree != NULL))
        return TRUE;
    if (on) {
        f->split = 180;
        f->tree = CreateWindow(TREEVIEW_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER,
                               0, 0, 0, 0, w, (HMENU)ID_TREE, g_hInst, NULL);
        if (!f->tree)
            return FALSE;
        SkinControl(f->tree);           /* Windows 98's sunken edge for the 1-pixel border */
        DesktopFolder(desk);
        TreeViewReset(f->tree, desk);
        if (*f->path || IsWindowVisible(w))
            TreeViewShowPath(f->tree, f->path);
    } else {
        DestroyWindow(f->tree);
        f->tree = NULL;
        SetFocus(f->view);
    }
    Arrange(w);
    SetTitle(w);
    return TRUE;
}

static BOOL OnSplit(HWND w, int x, int y)
{
    FOLDER *f = Folder(w);
    RECT c;
    GetClientRect(w, &c);
    return f->tree && x >= f->split && x < f->split + SPLIT_W && y >= BandsHeight(w) && y < c.bottom - STATUS_H;
}

/* SBM_CHANGED: refresh when it is this folder; close when the folder is gone.
   No folder: Folder Options changed, everything again. */
static void Changed(HWND w, LPCSTR dir)
{
    FOLDER *f = Folder(w);
    char desk[MAX_PATH];

    if (!dir) {
        if (f->tree) {
            DesktopFolder(desk);
            TreeViewReset(f->tree, desk);
            TreeViewShowPath(f->tree, f->path);
        }
        Load(w, f->path, TRUE);
        return;
    }
    if (f->tree)
        TreeViewRefresh(f->tree, dir);
    if (PathOnDisk(f->path) && PathIsInside(f->path, dir) && !PathIsFolder(f->path)) {
        if (f->tree)
            Navigate(w, "");                    /* Explorer stays, at My Computer */
        else
            DestroyWindow(w);
    } else if (!lstrcmpi(f->path, dir)) {
        Load(w, f->path, TRUE);
    }
}

static void Command(HWND w, UINT cmd, UINT code)
{
    FOLDER *f = Folder(w);
    char path[MAX_PATH];

    switch (cmd) {
    case CMD_OPEN:      Open(w); break;
    case CMD_UP:        Up(w); break;
    case CMD_BACK:      Step(w, -1); break;
    case CMD_FORWARD:   Step(w, 1); break;
    case CMD_VIEWS:     Views(w, code == TBN_DROPDOWN); break;
    case CMD_SHOWTOOLS:
    case CMD_SHOWADDRESS:
    case CMD_SHOWLABELS:
    case CMD_COMPACT:    ToggleBand(w, cmd); break;
    case CMD_ABOUT:      AboutDialogShow(w); break;
    case CMD_FOLDERS:   ShowTree(w, TRUE); break;
    case CMD_NOBAR:     ShowTree(w, FALSE); break;
    case CMD_FILTER:                    /* "*.*" or nothing: all again */
        lstrcpy(path, f->filter[0] ? f->filter : "*.*");
        if (*f->path && !IsBin(f->path) && PromptDialogShow(w, "Filter", "&Show only files named:", path, sizeof f->filter)) {
            lstrcpy(f->filter, lstrcmp(path, "*.*") && lstrcmp(path, "*") ? path : "");
            Load(w, f->path, TRUE);
        }
        break;
    case CMD_GOTO:
        lstrcpy(path, f->path);
        if (GoToDialogShow(w, path))
            Navigate(w, path);
        break;
    case CMD_CLOSE:     DestroyWindow(w); break;
    default:
        FileCommandsRun(w, f->view, cmd);
        if (IsWindow(w))
            ToolsUpdate(w);
    }
}

/* WM_CREATE: the window's data, its view and bands, and the tree when it is
   an Explorer window; -1 when any of them fails. */
static LRESULT Init(HWND w, CREATESTRUCT FAR *cs)
{
    FOLDER *f;

    f = LocalHeapAlloc(sizeof(FOLDER));
    if (!f)
        return -1;
    SetWindowWord(w, 0, LocalHeapOffset(f));
    f->hTrail = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(TRAIL));
    f->trail = f->hTrail ? (TRAIL *)GlobalLock(f->hTrail) : NULL;
    f->view = CreateWindow(ICONVIEW_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                           0, 0, 0, 0, w, (HMENU)ID_VIEW, g_hInst, NULL);
    if (!f->view || !MakeBands(w))
        return -1;
    DragAcceptFiles(f->view, TRUE);
    FileCommandsLoadView(f->view);
    if (cs->lpCreateParams && !ShowTree(w, TRUE))     /* Explorer */
        return -1;
    return 0;
}

/* WM_DESTROY: the history and the window's data go. */
static void Destroy(HWND w)
{
    FOLDER *f = Folder(w);

    if (f->hTrail) {
        GlobalUnlock(f->hTrail);
        GlobalFree(f->hTrail);
    }
    LocalHeapFree(f);
}

/* The sizing cursor over the splitter and while it is dragged; FALSE leaves
   the cursor to DefWindowProc. */
static BOOL SplitCursor(HWND w, HWND over, UINT hit)
{
    FOLDER *f = Folder(w);

    if (over == w && hit == HTCLIENT) {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(w, &p);
        if (OnSplit(w, p.x, p.y) || f->splitting) {
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            return TRUE;
        }
    }
    return FALSE;
}

/* A click: the tree's close button closes it; on the splitter a drag starts,
   the mouse captured. */
static void ButtonDown(HWND w, int x, int y)
{
    FOLDER *f = Folder(w);

    if (f->tree) {
        POINT p;
        RECT r;
        p.x = x;
        p.y = y;
        TreeClose(w, &r);
        if (PtInRect(&r, p)) {
            ShowTree(w, FALSE);
            return;
        }
    }
    if (OnSplit(w, x, y)) {
        f->splitting = TRUE;
        SetCapture(w);
    }
}

/* Dragging the splitter: the tree pane follows the mouse. */
static void MouseMove(HWND w, int x)
{
    FOLDER *f = Folder(w);

    if (f->splitting) {
        f->split = x;
        Arrange(w);
    }
}

/* The splitter let go. */
static void ButtonUp(HWND w)
{
    FOLDER *f = Folder(w);

    if (f->splitting) {
        f->splitting = FALSE;
        ReleaseCapture();
    }
}

/* A menu opening: the file commands that apply, Back and Forward while there
   is somewhere to go, the bars and the tree checked as they are. */
static void InitMenu(HWND w, HMENU m)
{
    FOLDER *f = Folder(w);

    FileCommandsInitMenu(m, f->view);
    EnableMenuItem(m, CMD_BACK, MF_BYCOMMAND | (f->trail && f->trail->at > 0 ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(m, CMD_FORWARD, MF_BYCOMMAND |
                   (f->trail && f->trail->at < f->trail->count - 1 ? MF_ENABLED : MF_GRAYED));
    CheckMenuItem(m, CMD_SHOWTOOLS, MF_BYCOMMAND | (showTools ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m, CMD_SHOWADDRESS, MF_BYCOMMAND | (showAddress ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m, CMD_SHOWLABELS, MF_BYCOMMAND | (showLabels && !compact ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(m, CMD_SHOWLABELS, MF_BYCOMMAND | (compact ? MF_GRAYED : MF_ENABLED));
    CheckMenuItem(m, CMD_COMPACT, MF_BYCOMMAND | (compact ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m, CMD_FOLDERS, MF_BYCOMMAND | (f->tree ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m, CMD_NOBAR, MF_BYCOMMAND | (f->tree ? MF_UNCHECKED : MF_CHECKED));
}

/* Keys: Backspace goes up a level, Ctrl+G is Go To, others are file commands. */
static void Key(HWND w, WPARAM key)
{
    FOLDER *f = Folder(w);

    if (key == VK_BACK)
        Up(w);
    else if (key == VK_TAB && f->tree)      /* between the tree and the view */
        SetFocus(GetFocus() == f->view ? f->tree : f->view);
    else if (key == 'G' && GetKeyState(VK_CONTROL) < 0)
        SendMessage(w, WM_COMMAND, CMD_GOTO, 0L);
    else
        FileCommandsKey(w, f->view, key);
}

/* FWM_PLACE: the place chosen in the address list, unless it is shown already. */
static void Place(HWND w, WPARAM i)
{
    FOLDER *f = Folder(w);
    char path[MAX_PATH];

    if (SendMessage(f->address, CB_GETLBTEXT, i, (LPARAM)(LPSTR)path) != CB_ERR && lstrcmpi(path, f->path)) {
        if (!lstrcmp(path, PLACE_DESKTOP))
            DesktopFolder(path);
        Navigate(w, path);
    }
    SetFocus(f->view);
}

/* The tree's selection: its folder shown here. */
static void TreeNotify(HWND w, UINT code)
{
    FOLDER *f = Folder(w);
    char path[MAX_PATH];

    char renamed[MAX_PATH];
    POINT p;
    int i;

    if (code == TVN_SELECT && TreeViewSelectedPath(f->tree, path) && lstrcmpi(path, f->path))
        Navigate(w, path);
    if (code != TVN_CONTEXT)
        return;
    GetCursorPos(&p);
    ScreenToClient(f->tree, &p);
    if ((i = TreeViewHitTest(f->tree, p.x, p.y)) >= 0 && TreeViewItemPath(f->tree, i, path) &&
        (PathOnDisk(path) || TreeViewItemKind(f->tree, i) != IVK_FOLDER)) {
        /* its kind's menu: a drive's, My Computer's, the Recycle Bin's */
        FileCommandsKindMenu(w, path, TreeViewItemKind(f->tree, i), renamed);
        if (*renamed && f->tree) {
            PathToParent(path);
            TreeViewRefresh(f->tree, path);
        }
    }
}

/* The address list: a place chosen goes to FWM_PLACE. */
static void AddressNotify(HWND w, UINT code)
{
    FOLDER *f = Folder(w);

    if ((code == CBN_CLOSEUP && !keyClose) ||   /* chosen: the list closed, or a key while closed */
        (code == CBN_SELCHANGE && !SendMessage(f->address, CB_GETDROPPEDSTATE, 0, 0L))) {
        int i = (int)SendMessage(f->address, CB_GETCURSEL, 0, 0L);
        if (i >= 0)
            PostMessage(w, FWM_PLACE, i, 0L);   /* once the list has closed */
    }
}

/* The view's notifications: opening, selection, the context menu, drags,
   renaming (its answer returned) and sorting. */
static LRESULT ViewNotify(HWND w, UINT code)
{
    FOLDER *f = Folder(w);

    switch (code) {
    case IVN_OPEN:    Open(w); break;
    case IVN_SELECT:  StatusChanged(w); break;
    case IVN_CONTEXT: FileCommandsContextMenu(w, f->view); break;
    case IVN_DRAG:
    case IVN_RDRAG:   DragDropRun(f->view, code == IVN_RDRAG); break;
    case IVN_RENAME:  return FileCommandsRenamed(w, f->view);
    case IVN_SORTED:  FileCommandsSaveView(f->view); break;
    }
    return 0;
}

LRESULT CALLBACK __export FolderWindowProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    FOLDER *f = Folder(w);
    LRESULT res;

    if (SendToMessage(msg, wp, lp, &res))
        return res;
    switch (msg) {
    case WM_NCHITTEST:
        if (StatusBarGrip(w, lp))
            return HTBOTTOMRIGHT;
        break;
    case WM_CREATE:
        return Init(w, (CREATESTRUCT FAR *)lp);
    case WM_DESTROY:
        Destroy(w);
        return 0;
    case WM_SIZE:
        Arrange(w);
        return 0;
    case WM_SETFOCUS:
        SetFocus(f->view);
        return 0;
    case WM_ACTIVATE:                   /* the clipboard may hold other things now */
        if (wp != WA_INACTIVE && f)
            ToolsUpdate(w);
        break;
    case WM_PAINT:
        Paint(w);
        return 0;
    case WM_SETCURSOR:
        if (SplitCursor(w, (HWND)wp, LOWORD(lp)))
            return TRUE;
        break;
    case WM_LBUTTONDOWN:                /* the tree's close button; the splitter */
        ButtonDown(w, (int)LOWORD(lp), (int)HIWORD(lp));
        return 0;
    case WM_MOUSEMOVE:
        MouseMove(w, (int)LOWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        ButtonUp(w);
        return 0;
    case WM_INITMENUPOPUP:
        InitMenu(w, (HMENU)wp);
        return 0;
    case WM_MEASUREITEM:                /* the address list */
        ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = PLACE_H;
        return TRUE;
    case WM_DRAWITEM:
        PlacesDraw((const DRAWITEMSTRUCT FAR *)lp);
        return TRUE;
    case WM_KEYDOWN:
        Key(w, wp);
        return 0;
    case WM_SYSKEYDOWN:                 /* Alt+Left, Alt+Right: back and forth */
        if (wp == VK_LEFT || wp == VK_RIGHT) {
            Step(w, wp == VK_LEFT ? -1 : 1);
            return 0;
        }
        break;
    case WM_DROPFILES:
        FileCommandsDropFiles(f->view, (HDROP)wp);
        return 0;
    case SBM_CHANGED:
        Changed(w, (LPCSTR)lp);
        return 0;
    case FWM_PLACE:
        Place(w, wp);
        return 0;
    case FWM_GO:
        Go(w);
        return 0;
    case WM_COMMAND:                    /* the tree, the address list, the view; else a command */
        if (wp == ID_TREE)
            TreeNotify(w, HIWORD(lp));
        else if (wp == ID_ADDRESS)
            AddressNotify(w, HIWORD(lp));
        else if (wp == ID_VIEW)
            return ViewNotify(w, HIWORD(lp));
        else
            Command(w, wp, HIWORD(lp));
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL FolderWindowRegister(void)
{
    WNDCLASS wc;

    _fmemset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = FolderWindowProc;
    wc.cbWndExtra = sizeof(WORD);
    wc.hInstance = g_hInst;
    wc.hIcon = IconStock(IDI_FOLDER);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = FOLDERWINDOW_CLASS;
    return RegisterClass(&wc);
}

static LPCSTR wanted;
static HWND found;

BOOL CALLBACK __export FolderWindowFindProc(HWND w, LPARAM lp)
{
    char cls[16];
    GetClassName(w, cls, sizeof cls);
    if (!lstrcmp(cls, FOLDERWINDOW_CLASS) && !Folder(w)->tree && !lstrcmpi(Folder(w)->path, wanted)) {
        found = w;
        return FALSE;
    }
    return TRUE;
}

static HWND Create(LPCSTR path, BOOL explore)
{
    int x = 40 + (opened % 8) * 24;
    HWND w;

    w = CreateWindow(FOLDERWINDOW_CLASS, "", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                     x, x, explore ? 640 : 600, explore ? 440 : 400, NULL, MenuBar(IsBin(path)),
                     g_hInst, (LPVOID)(DWORD)explore);
    opened++;
    if (!w)
        return NULL;
    SkinFrame(w);
    Navigate(w, path);                  /* its first folder */
    ShowWindow(w, SW_SHOWNORMAL);
    return w;
}

static HWND Present(LPCSTR path)
{
    wanted = path;
    found = NULL;
    EnumTaskWindows(GetCurrentTask(), (WNDENUMPROC)FolderWindowFindProc, 0L);
    if (!found)
        return Create(path, FALSE);
    if (IsIconic(found))
        ShowWindow(found, SW_RESTORE);
    BringWindowToTop(found);
    return found;
}

void FolderWindowOpen(LPCSTR path)
{
    Present(path);
}

void FolderWindowSelect(LPCSTR path)
{
    char parent[MAX_PATH];
    HWND w;

    lstrcpy(parent, path);
    if (lstrlen(parent) <= 3)
        parent[0] = '\0';                  /* a drive: in My Computer */
    else
        PathToParent(parent);
    if ((w = Present(parent)) != NULL) {
        HWND view = Folder(w)->view;
        IconViewSelect(view, IconViewFind(view, lstrlen(path) <= 3 ? path : PathFileName((LPSTR)path)));
    }
}

void FolderWindowExplore(LPCSTR path)
{
    Create(path, TRUE);
}
