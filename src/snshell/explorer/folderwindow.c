/* Folder windows, as in Windows 98: the Standard Buttons and the address
   bar above a view and its status bar, browsing in place with a history
   for Back and Forward; Explorer windows add the folder tree on the left, and
   any folder window shows or hides it (View > Explorer Bar). Folder Options
   may open each folder in its own window instead, as Windows 95 did; Ctrl
   with a double click does the other.
   "" is My Computer (the drives), BIN_PATH the Recycle Bin. A folder that
   already has a plain window comes to the front instead of opening twice. */
#include "internal.h"

#include "folderwindowint.h"


/* ---- the folder shown, and the way here ---- */

/* "Exploring - " with the tree, then the folder's name (or path). */
void FolderWindowSetTitle(HWND w)
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
        SetMenu(w, FolderBandsMenuBar(IsBin(path)));
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
    if (g_viewOptions.rememberViews)    /* the folder's own view and order */
        FolderViewsLoad(f->view, TRUE);
    if (f->tree)
        TreeViewShowPath(f->tree, path);
    FolderWindowSetTitle(w);
    FolderBandsTools(w);
    FolderBandsAddress(w);
    InvalidateRect(w, NULL, FALSE);
}

/* A new folder in the history: what was ahead of the one shown goes. */
void FolderWindowNavigate(HWND w, LPCSTR path)
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
        FolderWindowNavigate(w, parent);
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
            FolderWindowNavigate(w, path);          /* the view now shows other items */
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
        FolderWindowNavigate(w, "");
    else if (!lstrcmpi(path, "Recycle Bin"))
        FolderWindowNavigate(w, BIN_PATH);
    else if (!lstrcmpi(path, "Desktop")) {
        DesktopFolder(path);
        FolderWindowNavigate(w, path);
    } else if (*path && PathIsFolder(path)) {
        FolderWindowNavigate(w, path);
    } else {
        if (*path)
            LaunchCommand(w, typed, NULL, SW_SHOWNORMAL);
        FolderBandsAddress(w);
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
    int n = IconViewSelectedCount(f->view), count = IconViewCount(f->view), i;

    for (i = n ? IconViewNextSelected(f->view, -1) : 0; i >= 0 && i < count;     /* the selected, else all */
         i = n ? IconViewNextSelected(f->view, i) : i + 1)
        bytes += IconViewItemSize(f->view, i);
    if (n)
        wsprintf(text, "%d object(s) selected", n);
    else
        wsprintf(text, "%d object(s)", count);
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

static void Paint(HWND w)
{
    FOLDER *f = Folder(w);
    PAINTSTRUCT ps;
    RECT c;
    HFONT old;
    int top = FolderBandsHeight(w);

    BeginPaint(w, &ps);
    GetClientRect(w, &c);
    old = SelectObject(ps.hdc, DrawFont(FALSE));
    SetBkMode(ps.hdc, TRANSPARENT);
    SetTextColor(ps.hdc, GetSysColor(COLOR_BTNTEXT));
    FolderBandsPaint(w, ps.hdc, &c);
    PaintStatus(w, ps.hdc, &c);
    if (f->tree)
        ExplorerBarPaint(w, ps.hdc, &c, top);
    SelectObject(ps.hdc, old);
    EndPaint(w, &ps);
}

void FolderWindowArrange(HWND w)
{
    FOLDER *f = Folder(w);
    RECT c;
    int h, x = 0, top;

    GetClientRect(w, &c);
    FolderBandsArrange(w, &c);
    top = FolderBandsHeight(w);
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
    FolderBandsTools(w);
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
            FolderWindowNavigate(w, "");                    /* Explorer stays, at My Computer */
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
    case CMD_VIEWS:     FolderBandsViews(w, code == TBN_DROPDOWN); break;
    case CMD_SHOWTOOLS:
    case CMD_SHOWADDRESS:
    case CMD_SHOWLABELS:
    case CMD_COMPACT:    FolderBandsToggle(w, cmd); break;
    case CMD_ABOUT:      AboutDialogShow(w); break;
    case CMD_FOLDERS:   ExplorerBarShow(w, TRUE); break;
    case CMD_NOBAR:     ExplorerBarShow(w, FALSE); break;
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
            FolderWindowNavigate(w, path);
        break;
    case CMD_CLOSE:     DestroyWindow(w); break;
    default:
        FileCommandsRun(w, f->view, cmd);
        if (IsWindow(w))
            FolderBandsTools(w);
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
    if (!f->view || !FolderBandsMake(w))
        return -1;
    DragAcceptFiles(f->view, TRUE);
    FolderViewsLoad(f->view, FALSE);
    if (cs->lpCreateParams && !ExplorerBarShow(w, TRUE))     /* Explorer */
        return -1;
    return 0;
}

/* WM_DESTROY: the history and the window's data go. */
static void Destroy(HWND w)
{
    FOLDER *f = Folder(w);

    if (!f)                             /* WM_CREATE failed */
        return;
    if (f->hTrail) {
        GlobalUnlock(f->hTrail);
        GlobalFree(f->hTrail);
    }
    LocalHeapFree(f);
}

/* A menu opening: the file commands that apply, Back and Forward while there
   is somewhere to go, the bars and the tree checked as they are. */
static void InitMenu(HWND w, HMENU m)
{
    FOLDER *f = Folder(w);

    FileMenusInit(m, f->view);
    EnableMenuItem(m, CMD_BACK, MF_BYCOMMAND | (f->trail && f->trail->at > 0 ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(m, CMD_FORWARD, MF_BYCOMMAND |
                   (f->trail && f->trail->at < f->trail->count - 1 ? MF_ENABLED : MF_GRAYED));
    FolderBandsInitMenu(m);
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
        FolderWindowNavigate(w, path);
    }
    SetFocus(f->view);
}

/* The address list: a place chosen goes to FWM_PLACE. */
static void AddressNotify(HWND w, UINT code)
{
    FOLDER *f = Folder(w);

    if ((code == CBN_CLOSEUP && !FolderBandsKeyClosed()) ||   /* chosen: the list closed, or a key while closed */
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
    case IVN_CONTEXT: FileMenusContext(w, f->view); break;
    case IVN_DRAG:
    case IVN_RDRAG:   DragDropRun(f->view, code == IVN_RDRAG); break;
    case IVN_RENAME:  return FileCommandsRenamed(w, f->view);
    case IVN_SORTED:  FolderViewsSave(f->view); break;
    }
    return 0;
}

/* WM_COMMAND: the tree's, the address list's and the view's notifications, else a command. */
static LRESULT CommandMessage(HWND w, WPARAM wp, LPARAM lp)
{
    if (wp == ID_TREE)
        ExplorerBarNotify(w, HIWORD(lp));
    else if (wp == ID_ADDRESS)
        AddressNotify(w, HIWORD(lp));
    else if (wp == ID_VIEW)
        return ViewNotify(w, HIWORD(lp));
    else
        Command(w, wp, HIWORD(lp));
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
        FolderWindowArrange(w);
        return 0;
    case WM_SETFOCUS:
        SetFocus(f->view);
        return 0;
    case WM_ACTIVATE:                   /* the clipboard may hold other things now */
        if (wp != WA_INACTIVE && f)
            FolderBandsTools(w);
        break;
    case WM_PAINT:
        Paint(w);
        return 0;
    case WM_SETCURSOR:
        if (ExplorerBarCursor(w, (HWND)wp, LOWORD(lp)))
            return TRUE;
        break;
    case WM_LBUTTONDOWN:                /* the tree's close button; the splitter */
        ExplorerBarButtonDown(w, (int)LOWORD(lp), (int)HIWORD(lp));
        return 0;
    case WM_MOUSEMOVE:
        ExplorerBarMouseMove(w, (int)LOWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        ExplorerBarButtonUp(w);
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
    case WM_COMMAND:
        return CommandMessage(w, wp, lp);
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL FolderWindowRegister(void)
{

    return WindowRegister(FOLDERWINDOW_CLASS, FolderWindowProc, 0, sizeof(WORD), IconStock(IDI_FOLDER), NULL);
}

static int opened;                      /* windows made: each new one a step down */
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
                     x, x, explore ? 640 : 600, explore ? 440 : 400, NULL, FolderBandsMenuBar(IsBin(path)),
                     g_hInst, (LPVOID)(DWORD)explore);
    opened++;
    if (!w)
        return NULL;
    SkinFrame(w);
    FolderWindowNavigate(w, path);                  /* its first folder */
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
