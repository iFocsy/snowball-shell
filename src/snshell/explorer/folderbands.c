/* Folder windows' bands, as in Windows 98: the Standard Buttons and the
   address bar (the folder's icon, the list of places, AutoComplete), a
   row each or both on one (Compact); and the window's menu bar. */
#include "internal.h"
#include "folderwindowint.h"

static BOOL showTools = -1, showAddress, showLabels;   /* -1: not read yet */
static BOOL compact;                    /* both bands on one row, the buttons without labels */
static WNDPROC comboProc, editProc, listProc;
#define ADDRESS_OF  "SbAddressOf"           /* an address list's combo box */
static BOOL keyClose;                       /* the address list closed by Enter or Esc */

HMENU FolderBandsMenuBar(BOOL bin)
{
    HMENU bar = CreateMenu(), go = CreatePopupMenu(), view = FileMenusView(), bars = CreatePopupMenu(), help;
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
    AppendMenu(bar, MF_POPUP, (UINT)FileMenusFile(bin), "&File");
    AppendMenu(bar, MF_POPUP, (UINT)FileMenusEdit(), "&Edit");
    AppendMenu(bar, MF_POPUP, (UINT)view, "&View");
    AppendMenu(bar, MF_POPUP, (UINT)go, "&Go");
    help = CreatePopupMenu();
    AppendMenu(help, MF_STRING, CMD_ABOUT, "&About Snowball");
    AppendMenu(bar, MF_POPUP, (UINT)help, "&Help");
    return bar;
}

/* ---- the bands: Standard Buttons, Address ---- */

void FolderBandsAddress(HWND w);

/* What the buttons can do now. */
void FolderBandsTools(HWND w)
{
    FOLDER *f = Folder(w);
    TRAIL *t = f->trail;

    ToolbarEnable(f->tools, CMD_BACK, t && t->at > 0);
    ToolbarEnable(f->tools, CMD_FORWARD, t && t->at < t->count - 1);
    ToolbarEnable(f->tools, CMD_UP, *f->path && !IsBin(f->path));
    ToolbarEnable(f->tools, CMD_CUT, FileMenusAvailable(f->view, CMD_CUT));
    ToolbarEnable(f->tools, CMD_COPY, FileMenusAvailable(f->view, CMD_COPY));
    ToolbarEnable(f->tools, CMD_PASTE, FileMenusAvailable(f->view, CMD_PASTE));
    ToolbarEnable(f->tools, CMD_DELETE, FileMenusAvailable(f->view, CMD_DELETE));
}

/* The folder shown, in the address bar. */
void FolderBandsAddress(HWND w)
{
    FOLDER *f = Folder(w);
    char name[MAX_PATH];

    PlacesFill(f->address, f->path);
    f->icon = PlacesIcon(f->path);      /* (looked up once: the field repaints it often) */
    if (IsBin(f->path) || !*f->path)
        PlacesName(f->path, name);
    else
        lstrcpy(name, f->path);
    SetWindowText(f->address, name);
    InvalidateRect(f->address, NULL, FALSE);   /* its icon */
}

/* Compact: both on one row, as Windows 98's bands dragged onto one */
#define OneRow()    (compact && showTools && showAddress)

int FolderBandsHeight(HWND w)
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

void FolderBandsPaint(HWND w, HDC dc, const RECT *c)
{
    FOLDER *f = Folder(w);
    RECT r;
    int y, h;

    if (!FolderBandsHeight(w))
        return;
    SetRect(&r, 0, 0, c->right, FolderBandsHeight(w));
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
    Etched(dc, 0, c->right);
    y = 2;
    if (OneRow()) {                     /* the address field's band, then the buttons' */
        h = FolderBandsHeight(w) - 4;
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

void FolderBandsArrange(HWND w, const RECT *c)
{
    FOLDER *f = Folder(w);
    HDC dc;
    HFONT old;
    int y = 2, label;

    ShowWindow(f->tools, showTools ? SW_SHOWNA : SW_HIDE);
    ShowWindow(f->address, showAddress ? SW_SHOWNA : SW_HIDE);
    if (OneRow()) {                     /* the buttons keep their width; the field takes the rest */
        RECT a;
        int h = FolderBandsHeight(w) - 4, fieldW, toolsW = ToolbarWidth(f->tools);
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

    if (!f || !f->icon)
        return;
    GetClientRect(w, &c);
    SetRect(&r, 2, 2, ICON_X + 16 + ICON_GAP, c.bottom - 2);
    dc = GetDC(w);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_WINDOW));
    DrawIconImage(dc, ICON_X, (c.bottom - 16) / 2, f->icon, TRUE);
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
    WindowRectIn(edit, w, &r);
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
            FolderBandsAddress(frame);
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

#define MAX_LISTS   48

/* The combo boxes' lists (windows of the desktop), at most max into list. */
static int Lists(HWND *list, int max)
{
    char cls[12];
    HWND w;
    int n = 0;

    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w && n < max; w = GetWindow(w, GW_HWNDNEXT)) {
        GetClassName(w, cls, sizeof cls);
        if (!lstrcmpi(cls, "ComboLBox"))
            list[n++] = w;
    }
    return n;
}

BOOL FolderBandsMake(HWND w)
{
    FOLDER *f = Folder(w);
    HWND edit, before[MAX_LISTS], after[MAX_LISTS];
    int nBefore, nAfter, i, j;

    ReadBands();
    nBefore = Lists(before, MAX_LISTS);
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
    /* its list, a window of the desktop made with it: the one that was not
       there before (the top one may be another program's, shown last) */
    nAfter = Lists(after, MAX_LISTS);
    for (i = 0; i < nAfter && nBefore < MAX_LISTS; i++) {
        for (j = 0; j < nBefore && before[j] != after[i]; j++)
            ;
        if (j == nBefore && !GetProp(after[i], ADDRESS_OF)) {      /* (subclassed twice: endless) */
            SetProp(after[i], ADDRESS_OF, f->address);
            listProc = (WNDPROC)SetWindowLong(after[i], GWL_WNDPROC, (LONG)(FARPROC)FolderWindowListProc);
            break;
        }
    }
    SendMessage(f->address, WM_SIZE, 0, 0L);    /* the edit moves aside now */
    return TRUE;
}

/* View > Toolbars: this window now, new ones later. */
void FolderBandsToggle(HWND w, UINT cmd)
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
void FolderBandsViews(HWND w, BOOL list)
{
    FOLDER *f = Folder(w);
    HMENU m;
    RECT r;

    if (!list) {
        FileCommandsRun(w, f->view, FileMenusModeCommand((IconViewMode(f->view) + 1) % (IVM_THUMBS + 1)));
        return;
    }
    m = FileMenusModes(CreatePopupMenu(), IconViewMode(f->view));
    ToolbarButtonRect(f->tools, CMD_VIEWS, &r);
    ClientToScreen(f->tools, (POINT FAR *)&r.left);
    ClientToScreen(f->tools, (POINT FAR *)&r.right);
    TrackPopupMenu(m, TPM_LEFTALIGN, r.left, r.bottom, 0, w, NULL);
    DestroyMenu(m);
}

/* View > Toolbars as the bands are. */
void FolderBandsInitMenu(HMENU m)
{
    CheckMenuItem(m, CMD_SHOWTOOLS, MF_BYCOMMAND | (showTools ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m, CMD_SHOWADDRESS, MF_BYCOMMAND | (showAddress ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m, CMD_SHOWLABELS, MF_BYCOMMAND | (showLabels && !compact ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(m, CMD_SHOWLABELS, MF_BYCOMMAND | (compact ? MF_GRAYED : MF_ENABLED));
    CheckMenuItem(m, CMD_COMPACT, MF_BYCOMMAND | (compact ? MF_CHECKED : MF_UNCHECKED));
}

BOOL FolderBandsKeyClosed(void)
{
    return keyClose;
}
