/*
 * The Start menu: native popup menus with owner-drawn items. Root items are
 * large with the banner on their left; folders become submenus that are
 * filled when they first open. Everything for one opening lives in one
 * arena, reset the next time the menu opens. Items of the Start Menu
 * folder have their own menu on a right click, and are dragged to another
 * place or submenu. Windows 3.1's menus know neither: a message filter
 * takes the mouse, ends the menu, shows the item's menu or moves the item,
 * then opens the Start menu again where it was.
 */
#include "internal.h"

#define ROOT_H      36
#define SEP_H       8
#define BANNER_W    21
#define ARENA_SIZE  32000
#define MAX_ENTRIES 500
#define CMD_BASE    0x1000
#define MAX_DEPTH   8                   /* submenus to an item, the root's included */
#define DRAG_MIN    4                   /* pixels the mouse moves before a press is a drag */
#define SMM_MARK    (WM_USER + 130)     /* the menu followed the mouse: mark the drop */

enum { SPECIAL_NONE, SPECIAL_RUN, SPECIAL_SHUTDOWN, SPECIAL_FIND, SPECIAL_TASKBAR, SPECIAL_FOLDEROPTS };

typedef struct {
    LPSTR label;
    LPSTR path;                 /* NULL for specials and separators */
    HMENU parent, sub;
    HICON icon;
    int   top;                  /* root items: offset of their banner slice */
    int   place;                /* StartMenuOrderPlace: 0, or where it was dragged */
    RECT  rc;                   /* where it was painted, on the screen */
    WORD  painted;              /* when: the latest painted at a point is shown there */
    BYTE  pos, root, separator, filled, special, disabled;
    BYTE  mnemonic;             /* label has an &-prefixed access key */
    BYTE  empty;                /* "(Empty)": its folder has nothing */
    BYTE  column;               /* starts a new column of its menu */
} ENTRY;

enum { TRACK_CLOSED, TRACK_ITEM, TRACK_DROP };

typedef struct {                /* what an opening ended with */
    int      what;              /* TRACK_* */
    char     path[MAX_PATH];    /* the item right-clicked or dropped */
    char     dir[MAX_PATH];     /* dropped into this folder */
    PATHLIST order;             /* ... whose items are now in this order */
    BYTE     pos[MAX_DEPTH];    /* the menu opens again there */
    int      depth;
} OUTCOME;

static HGLOBAL arena;
static BYTE far *heap;
static WORD used;
static ENTRY far *entries[MAX_ENTRIES];
static int nEntries;
static HDC bannerDC;
static HBITMAP bannerBmp, bannerOld;
static HWND owner;
static BOOL smallRoot;                   /* Taskbar Properties: small icons in the Start menu */
static HHOOK filter;                    /* set while the menu is open */
static HTASK menuTask;                  /* the filter sees every program's menus: ours only */
static RECT startButton;                /* (screen) pressed while the menu is up: it closes */
static ENTRY far *clicked;              /* right-clicked: its menu comes next */
static ENTRY far *dragged;              /* moved with the button down */
static BOOL dragging, dropped;
static POINT pressedAt;
static ENTRY far *dropOn;               /* the mark is by this item, */
static BOOL dropAfter;                  /* below it, else above */
static HWND markWnd;                    /* in this menu window */
static WORD paints;                     /* items painted in this opening */
static OUTCOME out;

static void far *Alloc(WORD n)
{
    void far *p;
    n = (n + 1) & ~1;
    if (used + n > ARENA_SIZE)
        return NULL;
    p = heap + used;
    used += n;
    return p;
}

static LPSTR Str(LPCSTR s)
{
    LPSTR p = Alloc(lstrlen(s) + 1);
    if (p)
        lstrcpy(p, s);
    return p;
}

static ENTRY far *Add(HMENU m, LPCSTR label, LPCSTR path, BOOL root)
{
    ENTRY far *e;

    if (nEntries == MAX_ENTRIES || (e = Alloc(sizeof(ENTRY))) == NULL)
        return NULL;
    _fmemset(e, 0, sizeof *e);
    e->label = Str(label);
    e->path = path ? Str(path) : NULL;
    e->parent = m;
    e->root = (BYTE)root;
    entries[nEntries++] = e;
    return e;
}

static int ItemHeight(ENTRY far *e);

/* How far down its column a submenu has got before e. */
static int ColumnHeight(ENTRY far *e)
{
    int y = 0, i;

    for (i = 0; i < nEntries && entries[i] != e; i++)
        if (entries[i]->parent == e->parent)
            y = (entries[i]->column ? 0 : y) + ItemHeight(entries[i]);
    return y;
}

static void Append(ENTRY far *e)
{
    UINT flags = MF_OWNERDRAW;
    UINT id = CMD_BASE;
    int i, y;
    RECT work;

    for (i = 0; i < nEntries && entries[i] != e; i++)
        ;
    id += i;
    /* a submenu taller than the screen (less the taskbar) goes on in
       another column, as Windows 95's do (Windows 3.1's run off the screen) */
    y = ColumnHeight(e);
    TaskbarWorkArea(&work);
    if (!e->root && y && y + ItemHeight(e) > work.bottom - work.top - 8) {
        e->column = 1;
        flags |= MF_MENUBREAK;
    }
    if (e->sub) {
        flags |= MF_POPUP;
        id = (UINT)e->sub;
    }
    if (e->separator || e->disabled)
        flags |= MF_DISABLED;
    e->pos = (BYTE)GetMenuItemCount(e->parent);
    AppendMenu(e->parent, flags, id, (LPCSTR)e);
}

static void Separator(HMENU m, BOOL root)
{
    ENTRY far *e = Add(m, "", NULL, root);
    if (e) {
        e->separator = 1;
        Append(e);
    }
}

static int CompareEntries(const void far *pa, const void far *pb)
{
    ENTRY far *a = *(ENTRY far * far *)pa, far *b = *(ENTRY far * far *)pb;
    WORD placeA = a->place ? a->place : 0xFFFF, placeB = b->place ? b->place : 0xFFFF;

    if (placeA != placeB)               /* dragged into place first */
        return placeA < placeB ? -1 : 1;
    if ((a->sub != NULL) != (b->sub != NULL))
        return a->sub ? -1 : 1;
    return lstrcmpi(a->label, b->label);
}

/* Adds the contents of dir to m: as dragged, then folders as submenus,
   then everything else. */
static int AddFolder(HMENU m, LPCSTR dir, BOOL root, LPCSTR skip)
{
    char spec[MAX_PATH], path[MAX_PATH], name[MAX_PATH];
    static DOSENTRY f;
    DOSFIND find;
    int first = nEntries, i;

    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (DosFindFirst(spec, &find, &f)) {
        do {
            ENTRY far *e;
            if ((f.attr & 0x06) || (skip && !lstrcmpi(f.name, skip)))
                continue;
            lstrcpy(path, dir);
            PathAppend(path, f.name);
            DisplayNameOfFile(f.name, name);
            if ((e = Add(m, name, path, root)) == NULL)
                break;
            e->place = StartMenuOrderPlace(path);
            if (f.attr & 0x10)
                e->sub = CreatePopupMenu();
        } while (DosFindNext(&find, &f));
        DosFindClose(&find);
    }
    SortRecords(entries + first, nEntries - first, sizeof(ENTRY far *), CompareEntries);
    for (i = first; i < nEntries; i++)
        Append(entries[i]);
    return nEntries - first;
}

static void FillSubmenu(ENTRY far *e)
{
    char recent[MAX_PATH];
    ENTRY far *docs;

    e->filled = 1;
    if (e->root && !lstrcmpi(e->path, RecentFolder(recent)) &&     /* Documents: My Documents first */
        (docs = Add(e->sub, "My Documents", DesktopMyDocuments(), FALSE)) != NULL) {
        docs->icon = IconStock(IDI_MYDOCS);
        Append(docs);
        Separator(e->sub, FALSE);
    }
    if (AddFolder(e->sub, e->path, FALSE, NULL) == 0) {
        ENTRY far *empty = Add(e->sub, "(Empty)", NULL, FALSE);
        if (empty) {
            empty->disabled = empty->empty = 1;
            Append(empty);
        }
    }
}

/* A file of that name in the Windows folder. */
static BOOL WindowsHas(LPCSTR file)
{
    char path[MAX_PATH];

    GetWindowsDirectory(path, sizeof path);
    PathAppend(path, file);
    return DosGetAttributes(path) != 0xFFFF;
}

static ENTRY far *Program(HMENU m, LPCSTR label, LPCSTR exe)
{
    char path[MAX_PATH];
    ENTRY far *e;

    GetWindowsDirectory(path, sizeof path);
    PathAppend(path, exe);
    if ((e = Add(m, label, path, FALSE)) != NULL)
        e->mnemonic = 1;
    return e;
}

static ENTRY far *RootItem(HMENU m, LPCSTR label, LPCSTR path, int icon)
{
    ENTRY far *e = Add(m, label, path, TRUE);
    if (e) {
        e->icon = IconStock(icon);
        e->mnemonic = 1;
    }
    return e;
}

static HMENU BuildRoot(void)
{
    HMENU root = CreatePopupMenu(), settings, find;
    char dir[MAX_PATH], programs[MAX_PATH];
    ENTRY far *e;

    StartMenuFolder(dir);
    if (AddFolder(root, dir, TRUE, "Programs"))
        Separator(root, TRUE);

    lstrcpy(programs, dir);
    PathAppend(programs, "Programs");
    if ((e = RootItem(root, "&Programs", programs, IDI_SM_PROGRAMS)) != NULL) {
        e->sub = CreatePopupMenu();
        Append(e);
    }
    if ((e = RootItem(root, "&Documents", RecentFolder(dir), IDI_SM_DOCS)) != NULL) {
        e->sub = CreatePopupMenu();     /* filled when it opens: "(Empty)" if nothing */
        Append(e);
    }
    StartMenuFolder(dir);
    if ((e = RootItem(root, "&Settings", NULL, IDI_SM_SETTINGS)) != NULL) {
        e->sub = settings = CreatePopupMenu();
        e->filled = 1;
        Append(e);
        if ((e = Program(settings, "&Control Panel", "CONTROL.EXE")) != NULL) Append(e);
        if ((e = Program(settings, "&Printers", "PRINTMAN.EXE")) != NULL) Append(e);
        /* WINSETUP.EXE: Setup inside Windows (SETUP.EXE is the DOS one: no use with Windows running) */
        if (WindowsHas("WINSETUP.EXE") && (e = Program(settings, "Windows &Setup", "WINSETUP.EXE")) != NULL) Append(e);
        if ((e = Add(settings, "&Taskbar && Start Menu...", NULL, FALSE)) != NULL) {
            e->icon = IconStock(IDI_SNOWBALL);
            e->mnemonic = 1;
            e->special = SPECIAL_TASKBAR;
            Append(e);
        }
        if ((e = Add(settings, "&Folder Options...", NULL, FALSE)) != NULL) {
            e->icon = IconStock(IDI_FOLDER);
            e->mnemonic = 1;
            e->special = SPECIAL_FOLDEROPTS;
            Append(e);
        }
    }
    if ((e = RootItem(root, "&Find", NULL, IDI_SM_FIND)) != NULL) {
        e->sub = find = CreatePopupMenu();
        e->filled = 1;
        Append(e);
        if ((e = Add(find, "&Files or Folders...", NULL, FALSE)) != NULL) {
            e->icon = IconStock(IDI_SM_FIND);
            e->mnemonic = 1;
            e->special = SPECIAL_FIND;
            Append(e);
        }
    }
    if ((e = RootItem(root, "&Run...", NULL, IDI_SM_RUN)) != NULL) {
        e->special = SPECIAL_RUN;
        Append(e);
    }
    Separator(root, TRUE);
    if ((e = RootItem(root, "Sh&ut Down...", NULL, IDI_SM_SHUTDOWN)) != NULL) {
        e->special = SPECIAL_SHUTDOWN;
        Append(e);
    }
    return root;
}

/* Root items are large, with the banner, unless Taskbar Properties says
   small icons. */
static BOOL Big(ENTRY far *e)
{
    return e->root && !smallRoot;
}

static int ItemHeight(ENTRY far *e)
{
    return e->separator ? SEP_H : Big(e) ? ROOT_H : MENUITEM_H;
}

static COLORREF IniColor(LPCSTR key, LPCSTR def)
{
    char hex[8];
    DWORD v = 0;
    LPSTR p;

    SettingsGetString("Start", key, def, hex, sizeof hex);
    for (p = hex; *p; p++)
        v = v * 16 + (*p <= '9' ? *p - '0' : ((*p | 0x20) - 'a' + 10));
    return RGB((BYTE)(v >> 16), (BYTE)(v >> 8), (BYTE)v);
}

/* The banner: a vertical gradient with the text running upwards,
   first word bold ("Windows 3.11"). */
static void MakeBanner(int height)
{
    HDC screen = GetDC(NULL);
    char text[40];
    LPSTR rest;
    LOGFONT lf;
    HFONT bold, light, old;
    RECT r;
    int y;

    bannerDC = CreateCompatibleDC(screen);
    bannerBmp = CreateCompatibleBitmap(screen, BANNER_W, height);
    ReleaseDC(NULL, screen);
    bannerOld = SelectObject(bannerDC, bannerBmp);
    SetRect(&r, 0, 0, BANNER_W, height);
    DrawGradient(bannerDC, &r, IniColor("BannerTop", "000040"), IniColor("BannerBottom", "1084D0"), TRUE);

    SettingsGetString("Start", "Banner", "Windows 3.11", text, sizeof text);
    for (rest = text; *rest && *rest != ' '; rest++)
        ;
    if (*rest)
        *rest++ = '\0';
    _fmemset(&lf, 0, sizeof lf);
    lf.lfHeight = -17;
    lf.lfEscapement = lf.lfOrientation = 900;
    lf.lfWeight = FW_BOLD;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lstrcpy(lf.lfFaceName, "Arial");
    bold = CreateFontIndirect(&lf);
    lf.lfWeight = FW_NORMAL;
    light = CreateFontIndirect(&lf);

    SetBkMode(bannerDC, TRANSPARENT);
    SetTextColor(bannerDC, RGB(255, 255, 255));
    old = SelectObject(bannerDC, bold);
    y = height - 6;
    TextOut(bannerDC, 1, y, text, lstrlen(text));
    y -= LOWORD(GetTextExtent(bannerDC, text, lstrlen(text))) + 5;
    SelectObject(bannerDC, light);
    SetTextColor(bannerDC, RGB(224, 224, 224));
    TextOut(bannerDC, 1, y, rest, lstrlen(rest));
    SelectObject(bannerDC, old);
    DeleteObject(bold);
    DeleteObject(light);
}

/* A drop mark: a black line along the top or the bottom of e, on the screen. */
static void MarkRect(ENTRY far *e, BOOL after, RECT *r)
{
    *r = e->rc;
    if (Big(e))
        r->left += BANNER_W;
    if (after)
        r->top = r->bottom - 2;
    else
        r->bottom = r->top + 2;
}

/* Each item off the screen, then onto it at once. */
static void DrawEntry(const DRAWITEMSTRUCT FAR *d)
{
    ENTRY far *e = (ENTRY far *)d->itemData;
    DWORD org = GetDCOrg(d->hDC);
    RECT r = d->rcItem, t;
    OFFSCREEN o;
    HDC dc = DrawOffscreenBegin(&o, d->hDC, &d->rcItem);

    e->rc = r;
    OffsetRect(&e->rc, LOWORD(org), HIWORD(org));
    e->painted = ++paints;
    if (Big(e)) {
        BitBlt(dc, r.left, r.top, BANNER_W, r.bottom - r.top, bannerDC, 0, e->top, SRCCOPY);
        r.left += BANNER_W;
    }
    if (e->separator) {
        DrawSolidRect(dc, &r, GetSysColor(COLOR_MENU));
        SetRect(&t, r.left + 2, r.top + SEP_H / 2 - 1, r.right - 2, r.top + SEP_H / 2);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNSHADOW));
        OffsetRect(&t, 0, 1);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNHIGHLIGHT));
        DrawOffscreenEnd(&o);
        return;
    }
    if (!e->icon && e->path)
        e->icon = e->sub ? IconStock(IDI_FOLDER) : IconForFile(e->path, FALSE);
    MenuItemPaint(dc, &r, e->icon, Big(e), e->label, (d->itemState & ODS_SELECTED ? MIP_SELECTED : 0) |
                  (e->disabled ? MIP_DISABLED : 0) | (e->mnemonic ? MIP_MNEMONIC : 0));
    DrawOffscreenEnd(&o);
    if (e == dropOn) {                  /* (the mark goes over the item's edge: on the menu itself) */
        MarkRect(e, dropAfter, &t);
        OffsetRect(&t, -(int)LOWORD(org), -(int)HIWORD(org));
        DrawSolidRect(d->hDC, &t, RGB(0, 0, 0));
    }
}

static void MeasureEntry(MEASUREITEMSTRUCT FAR *m)
{
    ENTRY far *e = (ENTRY far *)m->itemData;
    int w = MenuItemWidth(e->label, e->mnemonic, Big(e));

    m->itemHeight = ItemHeight(e);
    m->itemWidth = Big(e) ? max(BANNER_W + w, 150) : w;
}

static BOOL InStartMenu(LPCSTR path)
{
    char dir[MAX_PATH];

    StartMenuFolder(dir);
    return path && PathIsInside(path, dir);
}

/* An item of the Start Menu folder: it has a menu of its own, it moves. */
static BOOL Editable(ENTRY far *e)
{
    return e && !e->mnemonic && InStartMenu(e->path);
}

/* The folder an item dropped by e lands in: e's, or the one "(Empty)" is in. */
static BOOL DropFolder(ENTRY far *e, LPSTR dir)
{
    int i;

    if (e && e->empty) {
        for (i = 0; i < nEntries && entries[i]->sub != e->parent; i++)
            ;
        if (i == nEntries || !InStartMenu(entries[i]->path))
            return FALSE;
        lstrcpy(dir, entries[i]->path);
        return TRUE;
    }
    if (!Editable(e))
        return FALSE;
    lstrcpy(dir, e->path);
    PathToParent(dir);
    return TRUE;
}

static BOOL CanDrop(ENTRY far *e)
{
    char dir[MAX_PATH];

    return dragged && e != dragged && DropFolder(e, dir) && !PathIsInside(dir, dragged->path);
}

/* The item under a point of the screen: in a menu shown there, the one
   painted last (closed menus leave their items behind). */
static ENTRY far *ItemAt(POINT p)
{
    ENTRY far *found = NULL;
    HWND under = WindowFromPoint(p);
    char cls[8];
    int i;

    if (!under)                         /* (no window there) */
        return NULL;
    GetClassName(under, cls, sizeof cls);
    if (lstrcmp(cls, "#32768"))
        return NULL;
    for (i = 0; i < nEntries; i++)
        if (entries[i]->painted > (found ? found->painted : 0) && PtInRect(&entries[i]->rc, p))
            found = entries[i];
    return found;
}

static void RepaintMark(HWND w, ENTRY far *e, BOOL after)
{
    RECT r;

    MarkRect(e, after, &r);
    ScreenToClient(w, (POINT FAR *)&r.left);
    ScreenToClient(w, (POINT FAR *)&r.right);
    InvalidateRect(w, &r, FALSE);
    UpdateWindow(w);
}

/* Moves the mark to e (NULL: none) by repainting the item it leaves and
   the one it goes to. */
static void MarkAt(HWND w, ENTRY far *e, BOOL after)
{
    ENTRY far *was = dropOn;
    BOOL wasAfter = dropAfter;
    HWND wasIn = markWnd;

    dropOn = e;
    dropAfter = after;
    markWnd = w;
    if (was)
        RepaintMark(wasIn, was, wasAfter);
    if (e)
        RepaintMark(w, e, after);
}

/* The mark where the dragged item would go, above or below the item under
   the cursor, and the cursor saying whether it can go there. */
static void Mark(void)
{
    static HCURSOR move, none;
    ENTRY far *e;
    POINT p;
    BOOL after;

    GetCursorPos(&p);
    e = ItemAt(p);
    if (!CanDrop(e))
        e = NULL;
    after = e && !e->empty && p.y >= (e->rc.top + e->rc.bottom) / 2;
    if (e != dropOn || after != dropAfter)
        MarkAt(WindowFromPoint(p), e, after);
    if (!move) {
        move = LoadCursor(g_hArt, MAKEINTRESOURCE(IDR_DRAGMOVE));
        none = LoadCursor(g_hArt, MAKEINTRESOURCE(IDR_NODROP));
    }
    SetCursor(e ? move : none);
}

static int Distance(int a, int b)
{
    return a > b ? a - b : b - a;
}

/* ---- ghosts: Windows 3.1 shows one menu at a time, so the Start menu ends
   for an item's own menu and opens again after it. Meanwhile pictures of
   its open menus stay where they were, under the item's menu, until the
   menu has opened again as far. ---- */
#define MAX_GHOSTS  (MAX_DEPTH + 1)
static HWND ghosts[MAX_GHOSTS];
static int nGhosts;
static BOOL ghostsGo;                   /* the menu has opened again: they may go */

LRESULT CALLBACK __export StartMenuGhostProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HDC mem;
    HBITMAP old;
    RECT r;

    switch (msg) {
    case WM_PAINT:
        BeginPaint(w, &ps);
        GetClientRect(w, &r);
        mem = CreateCompatibleDC(ps.hdc);
        old = SelectObject(mem, (HBITMAP)GetWindowWord(w, 0));
        BitBlt(ps.hdc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
        EndPaint(w, &ps);
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_DESTROY:
        DeleteObject((HBITMAP)GetWindowWord(w, 0));
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

/* The visible menus (their windows), at most max of them into list. */
static int OpenMenus(HWND *list, int max)
{
    char cls[8];
    HWND m;
    int n = 0;

    for (m = GetWindow(GetDesktopWindow(), GW_CHILD); m && n < max; m = GetWindow(m, GW_HWNDNEXT)) {
        GetClassName(m, cls, sizeof cls);
        if (IsWindowVisible(m) && !lstrcmp(cls, "#32768")) {
            if (list)
                list[n] = m;
            n++;
        }
    }
    return n;
}

static void GhostsMake(void)
{
    static BOOL registered;
    HWND open[MAX_GHOSTS], g;
    HDC screen, mem;
    HBITMAP bmp, old;
    RECT r;
    int i, n;

    if (!registered) {
        WNDCLASS wc;
        _fmemset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = StartMenuGhostProc;
        wc.cbWndExtra = sizeof(WORD);   /* its picture */
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = "SbMenuGhost";
        registered = RegisterClass(&wc);
    }
    n = OpenMenus(open, MAX_GHOSTS);
    screen = GetDC(NULL);
    mem = CreateCompatibleDC(screen);
    for (i = 0; i < n && nGhosts < MAX_GHOSTS; i++) {
        GetWindowRect(open[i], &r);
        if ((bmp = CreateCompatibleBitmap(screen, r.right - r.left, r.bottom - r.top)) == NULL)
            continue;
        old = SelectObject(mem, bmp);
        BitBlt(mem, 0, 0, r.right - r.left, r.bottom - r.top, screen, r.left, r.top, SRCCOPY);
        SelectObject(mem, old);
        g = CreateWindowEx(0, "SbMenuGhost", "", WS_POPUP, r.left, r.top, r.right - r.left,
                           r.bottom - r.top, NULL, NULL, g_hInst, NULL);
        if (!g) {
            DeleteObject(bmp);
            continue;
        }
        SetWindowWord(g, 0, (WORD)bmp);
        ShowWindow(g, SW_SHOWNOACTIVATE);
        UpdateWindow(g);
        ghosts[nGhosts++] = g;
    }
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
}

static void GhostsGone(void)
{
    while (nGhosts)
        DestroyWindow(ghosts[--nGhosts]);
}

/* The menu loop's messages. The right button is taken from the menu: let go
   over an item of the Start Menu folder, it ends the menu for the item's
   own. The left button pressed on one and moved drags it; let go by the
   mark, it ends the menu for the move. */
LRESULT CALLBACK __export StartMenuFilterProc(int code, WPARAM wp, LPARAM lp)
{
    MSG FAR *m = (MSG FAR *)lp;
    ENTRY far *e;
    POINT p;

    if (code != MSGF_MENU || GetCurrentTask() != menuTask)
        return CallNextHookEx(filter, code, wp, lp);
    /* opened again as far as before (or the user goes on): no more pictures */
    if (nGhosts && ghostsGo && (OpenMenus(NULL, MAX_GHOSTS) >= nGhosts || m->message == WM_LBUTTONDOWN ||
                    m->message == WM_RBUTTONDOWN || m->message == WM_CHAR))
        GhostsGone();
    GetCursorPos(&p);
    switch (m->message) {
    case WM_RBUTTONDOWN:
        return TRUE;
    case WM_RBUTTONUP:
        if (!dragging && Editable(e = ItemAt(p))) {
            clicked = e;
            GhostsMake();
            ghostsGo = FALSE;           /* (this menu is still up) */
            PostMessage(owner, WM_CANCELMODE, 0, 0L);
        }
        return TRUE;
    case WM_LBUTTONDOWN:
        if (PtInRect(&startButton, p)) {    /* the Start button again: the menu closes */
            PostMessage(owner, WM_CANCELMODE, 0, 0L);
            return TRUE;
        }
        dragging = FALSE;
        pressedAt = p;
        break;
    case WM_MOUSEMOVE:
        if (!dragging && (m->wParam & MK_LBUTTON) && Editable(e = ItemAt(pressedAt)) &&
            (Distance(p.x, pressedAt.x) > DRAG_MIN || Distance(p.y, pressedAt.y) > DRAG_MIN)) {
            dragged = e;
            dragging = TRUE;
        }
        if (dragging)
            PostMessage(owner, SMM_MARK, 0, 0L);
        break;
    case WM_LBUTTONUP:
        if (!dragging)
            break;
        dragging = FALSE;
        if (dropOn) {
            dropped = TRUE;
            PostMessage(owner, WM_CANCELMODE, 0, 0L);
        } else {
            dragged = NULL;
            SetCursor(LoadCursor(NULL, IDC_ARROW));
        }
        return TRUE;                    /* not a click on the item */
    }
    return CallNextHookEx(filter, code, wp, lp);
}

/* Positions of the items from the root to e, each in its menu. */
static int Trail(ENTRY far *e, BYTE *pos)
{
    ENTRY far *up[MAX_DEPTH];
    int n = 0, i;

    while (e && n < MAX_DEPTH) {
        up[n++] = e;
        for (i = 0; i < nEntries && entries[i]->sub != e->parent; i++)
            ;
        e = i < nEntries ? entries[i] : NULL;
    }
    for (i = 0; i < n; i++)
        pos[i] = up[n - 1 - i]->pos;
    return n;
}

/* Keys for the menu to open the submenus to pos and select the last. A
   menu opened with no button down starts on its first item, as does each
   submenu opened with the right arrow. */
static void PostTrail(const BYTE *pos, int n)
{
    int i, k;

    for (i = 0; i < n; i++) {
        if (i)
            PostMessage(owner, WM_KEYDOWN, VK_RIGHT, 1L);
        for (k = 0; k < pos[i]; k++)
            PostMessage(owner, WM_KEYDOWN, VK_DOWN, 1L);
    }
}

/* The dragged item's folder now, and the order of the folder's items with
   it; the menu opens again on it. */
static void Dropped(void)
{
    LPSTR name = PathFileName(dragged->path);
    ENTRY far *e;
    int i, at = 0;

    DropFolder(dropOn, out.dir);
    lstrcpy(out.path, dragged->path);
    PathListInit(&out.order);
    if (dropOn->empty)
        PathListAdd(&out.order, name);
    for (i = 0; i < nEntries && !dropOn->empty; i++) {
        e = entries[i];
        if (e->parent != dropOn->parent || e == dragged || !Editable(e))
            continue;
        if (e == dropOn && !dropAfter) {
            at = out.order.count;
            PathListAdd(&out.order, name);
        }
        PathListAdd(&out.order, PathFileName(e->path));
        if (e == dropOn && dropAfter) {
            at = out.order.count;
            PathListAdd(&out.order, name);
        }
    }
    out.depth = Trail(dropOn, out.pos);
    out.pos[out.depth - 1] = (BYTE)at;  /* the folder's own items come first in its menu */
    out.what = TRACK_DROP;
}

/* The dropped item into its folder, in its place. */
static void Move(HWND w)
{
    char from[MAX_PATH], to[MAX_PATH];

    lstrcpy(from, out.path);
    PathToParent(from);
    if (lstrcmpi(from, out.dir)) {
        lstrcpy(to, out.dir);
        PathAppend(to, PathFileName(out.path));
        if (DosGetAttributes(to) != 0xFFFF) {
            FileOperationError(w, "move", out.path, 80);
            return;
        }
        if (!DosRename(out.path, to)) {
            FileOperationError(w, "move", out.path, DosLastError());
            return;
        }
        StartMenuOrderRename(out.path, to);
        FileCommandsFolderChanged(from);
        FileCommandsFolderChanged(out.dir);
    }
    StartMenuOrderSet(out.dir, &out.order);
}

/* One opening; out says how it ended. Letting go of the mouse over the
   Start button that opened it keeps it open. */
static void Track(const RECT *button)
{
    RECT bar;
    HMENU root;
    int i, y = 0;

    startButton = *button;
    heap = GlobalLock(arena);
    used = 0;
    nEntries = 0;
    smallRoot = SettingsGetInt("Taskbar", "SmallIcons", 0);
    root = BuildRoot();
    for (i = 0; i < nEntries; i++)
        if (entries[i]->parent == root) {
            entries[i]->top = y;
            y += ItemHeight(entries[i]);
        }
    MakeBanner(y);
    clicked = dragged = dropOn = NULL;
    dragging = dropped = FALSE;
    ghostsGo = TRUE;
    paints = 0;
    pressedAt.x = pressedAt.y = -32000;  /* no press in it yet: the Start button's drags select */
    out.what = TRACK_CLOSED;
    menuTask = GetCurrentTask();         /* Windows 3.1 refuses a task's own filter from a DLL */
    filter = SetWindowsHookEx(WH_MSGFILTER, (HOOKPROC)StartMenuFilterProc, g_hInst, NULL);
    GetWindowRect(owner, &bar);
    /* the items and the menu's edges (3 pixels each side) against the
       taskbar: just above it, below it at the top of the screen, beside it
       from the Start button's top at a side (its edge grows 3 pixels right) */
    switch (TaskbarEdge()) {
    case TBE_TOP:   TrackPopupMenu(root, TPM_LEFTALIGN | TPM_LEFTBUTTON, bar.left, bar.bottom, 0, owner, button); break;
    case TBE_LEFT:  TrackPopupMenu(root, TPM_LEFTALIGN | TPM_LEFTBUTTON, bar.right, button->top, 0, owner, button); break;
    case TBE_RIGHT: TrackPopupMenu(root, TPM_RIGHTALIGN | TPM_LEFTBUTTON, bar.left - 3, button->top, 0, owner, button); break;
    default:        TrackPopupMenu(root, TPM_LEFTALIGN | TPM_LEFTBUTTON, bar.left, bar.top - y - 6, 0, owner, button);
    }
    UnhookWindowsHookEx(filter);
    filter = NULL;
    if (clicked) {
        out.what = TRACK_ITEM;
        lstrcpy(out.path, clicked->path);
        out.depth = Trail(clicked, out.pos);
    } else if (dropped) {
        Dropped();
    }
    DestroyMenu(root);
    SelectObject(bannerDC, bannerOld);
    DeleteObject(bannerBmp);
    DeleteDC(bannerDC);
    GlobalUnlock(arena);        /* entries stay valid until the next opening */
}

void StartMenuShow(HWND w, const RECT *button)
{
    char renamed[MAX_PATH];

    owner = w;
    if (!arena && (arena = GlobalAlloc(GMEM_MOVEABLE, ARENA_SIZE)) == NULL)
        return;
    for (;;) {
        Track(button);
        if (out.what == TRACK_ITEM) {
            if (FileCommandsItemMenu(w, out.path, TRUE, renamed) == CMD_BYNAME + IVSORT_NAME) {
                PathToParent(out.path);
                StartMenuOrderForget(out.path);
            } else if (*renamed) {
                StartMenuOrderRename(out.path, renamed);
            }
        } else if (out.what == TRACK_DROP) {
            Move(w);
            PathListFree(&out.order);
        } else {
            break;
        }
        PostTrail(out.pos, out.depth);
    }
    GhostsGone();
}

static void Command(int i)
{
    ENTRY far *e;

    if (i < 0 || i >= nEntries)
        return;
    heap = GlobalLock(arena);
    e = entries[i];
    switch (e->special) {
    case SPECIAL_RUN:      RunDialogShow(owner); break;
    case SPECIAL_SHUTDOWN: ShutDownDialogShow(owner); break;
    case SPECIAL_FIND:     FindShow(""); break;
    case SPECIAL_TASKBAR:  TaskbarPropertiesShow(); break;
    case SPECIAL_FOLDEROPTS: FolderOptionsShow(); break;
    default:               if (e->path) LaunchItem(owner, e->path);
    }
    GlobalUnlock(arena);
}

BOOL StartMenuMessage(HWND w, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res)
{
    int i;

    switch (msg) {
    case WM_MEASUREITEM:
        if (((MEASUREITEMSTRUCT FAR *)lp)->CtlType != ODT_MENU)
            return FALSE;
        MeasureEntry((MEASUREITEMSTRUCT FAR *)lp);
        *res = TRUE;
        return TRUE;
    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT FAR *)lp)->CtlType != ODT_MENU)
            return FALSE;
        DrawEntry((DRAWITEMSTRUCT FAR *)lp);
        *res = TRUE;
        return TRUE;
    case SMM_MARK:
        if (dragging)
            Mark();
        *res = 0;
        return TRUE;
    case WM_INITMENUPOPUP:
        for (i = 0; i < nEntries; i++)
            if (entries[i]->sub == (HMENU)wp && !entries[i]->filled)
                FillSubmenu(entries[i]);
        *res = 0;
        return TRUE;
    case WM_MENUCHAR:
        for (i = 0; i < nEntries; i++) {
            ENTRY far *e = entries[i];
            if (e->parent == (HMENU)HIWORD(lp) && !e->separator && !e->disabled &&
                MenuItemMatchesKey(e->label, e->mnemonic, wp)) {
                *res = MAKELONG(e->pos, 2);         /* execute */
                return TRUE;
            }
        }
        return FALSE;
    case WM_COMMAND:
        if (wp < CMD_BASE || wp >= CMD_BASE + MAX_ENTRIES || LOWORD(lp))
            return FALSE;
        Command(wp - CMD_BASE);
        *res = 0;
        return TRUE;
    }
    return FALSE;
}

void StartMenuFolder(LPSTR out)
{
    lstrcpy(out, g_home);
    lstrcat(out, DosHasLongNames() ? "Start Menu" : "STARTMNU");
}
