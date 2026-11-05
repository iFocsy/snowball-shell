/* The Start menu's items of the Start Menu folder have a menu of their own
   (a right click) and can be dragged to another place or submenu. Windows
   3.1's menus know neither: a message filter takes the mouse, ends the
   menu, leaves pictures of it while the item's menu is up, and opens it
   again where it was. */
#include "internal.h"
#include "startmenuint.h"
#include <stdlib.h>

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

/* A drop mark: a black line along the top or the bottom of e, on the screen. */
static void MarkRect(ENTRY far *e, BOOL after, RECT *r)
{
    *r = e->rc;
    if (StartMenuBannered(e))
        r->left += BANNER_W;
    if (after)
        r->top = r->bottom - 2;
    else
        r->bottom = r->top + 2;
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
    return e && !e->mnemonic && !e->fixed && InStartMenu(e->path);
}

/* The folder an item dropped by e lands in: e's, or the one "(Empty)" is in. */
static BOOL DropFolder(ENTRY far *e, LPSTR dir)
{
    int i;

    if (e && e->empty) {
        for (i = 0; i < StartMenuCount && StartMenuEntries[i]->sub != e->parent; i++)
            ;
        if (i == StartMenuCount || !InStartMenu(StartMenuEntries[i]->path))
            return FALSE;
        lstrcpy(dir, StartMenuEntries[i]->path);
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

/* Whether a menu is shown at a point of the screen. */
static BOOL OpenMenuAt(POINT p)
{
    HWND under = WindowFromPoint(p);
    char cls[8];

    if (!under)                         /* (no window there) */
        return FALSE;
    GetClassName(under, cls, sizeof cls);
    return !lstrcmp(cls, "#32768");
}

/* The item under a point of the screen: in a menu shown there, the one
   painted last (closed menus leave their items behind). */
static ENTRY far *ItemAt(POINT p)
{
    ENTRY far *found = NULL;
    int i;

    if (!OpenMenuAt(p))
        return NULL;
    for (i = 0; i < StartMenuCount; i++)
        if (StartMenuEntries[i]->painted > (found ? found->painted : 0) && PtInRect(&StartMenuEntries[i]->rc, p))
            found = StartMenuEntries[i];
    return found;
}

static void RepaintMark(HWND w, ENTRY far *e, BOOL after)
{
    RECT r;

    MarkRect(e, after, &r);
    MapWindowPoints(NULL, w, (POINT FAR *)&r, 2);
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

/* ---- ghosts: Windows 3.1 shows one menu at a time, so the Start menu ends
   for an item's own menu or a move and opens again after it. Meanwhile
   pictures of its open menus stay where they were, under the item's menu,
   until the menu has opened again as far; the menus shown again are given
   them to erase with (SkinMenuPictures) until the Start menu closes. ---- */
#define MAX_GHOSTS  (MAX_DEPTH + 1)
static HWND ghosts[MAX_GHOSTS];
static int nGhosts;
static BOOL ghostsGo;                   /* the menu has opened again: they may go */
static HBITMAP pictures[MAX_GHOSTS];
static RECT pictureRects[MAX_GHOSTS];
static int nPictures;

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

void StartMenuGhostsGone(void)
{
    while (nGhosts)
        DestroyWindow(ghosts[--nGhosts]);
}

/* The Start menu closed for good: no ghosts, no pictures. */
void StartMenuPicturesGone(void)
{
    StartMenuGhostsGone();
    SkinMenuPictures(NULL, NULL, 0);
    while (nPictures)
        DeleteObject(pictures[--nPictures]);
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
        registered = WindowRegister("SbMenuGhost", StartMenuGhostProc, 0, sizeof(WORD), NULL, NULL);
    }
    n = OpenMenus(open, MAX_GHOSTS);
    StartMenuPicturesGone();            /* (those of the opening before) */
    screen = GetDC(NULL);
    mem = CreateCompatibleDC(screen);
    for (i = 0; i < n && nPictures < MAX_GHOSTS; i++) {
        GetWindowRect(open[i], &r);
        if ((bmp = CreateCompatibleBitmap(screen, r.right - r.left, r.bottom - r.top)) == NULL)
            continue;
        old = SelectObject(mem, bmp);
        BitBlt(mem, 0, 0, r.right - r.left, r.bottom - r.top, screen, r.left, r.top, SRCCOPY);
        SelectObject(mem, old);
        pictureRects[nPictures] = r;
        pictures[nPictures++] = bmp;
        g = CreateWindowEx(0, "SbMenuGhost", "", WS_POPUP, r.left, r.top, r.right - r.left,
                           r.bottom - r.top, NULL, NULL, g_hInst, NULL);
        if (!g)
            continue;
        SetWindowWord(g, 0, (WORD)bmp);
        ShowWindow(g, SW_SHOWNOACTIVATE);
        UpdateWindow(g);
        ghosts[nGhosts++] = g;
    }
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    SkinMenuPictures(pictureRects, pictures, nPictures);
}

/* The menu loop's messages. The right button is taken from the menu: let go
   over an item of the Start Menu folder, it ends the menu for the item's
   own; outside the menus, it is the skin's (it ends the menu and goes on
   to the window there). The left button pressed
   on one and moved drags it; let go by the mark, it ends the menu for the
   move. */
LRESULT CALLBACK __export StartMenuFilterProc(int code, WPARAM wp, LPARAM lp)
{
    MSG FAR *m = (MSG FAR *)lp;
    ENTRY far *e;
    BOOL after;
    POINT p;

    if (code != MSGF_MENU || GetCurrentTask() != menuTask)
        return CallNextHookEx(filter, code, wp, lp);
    /* opened again as far as before (or the user goes on): no more pictures */
    if (nGhosts && ghostsGo && (OpenMenus(NULL, MAX_GHOSTS) >= nGhosts || m->message == WM_LBUTTONDOWN ||
                    m->message == WM_RBUTTONDOWN || m->message == WM_CHAR))
        StartMenuGhostsGone();
    GetCursorPos(&p);
    switch (m->message) {
    case WM_RBUTTONDOWN:
        if (!OpenMenuAt(p))
            break;
        return TRUE;
    case WM_RBUTTONUP:
        if (!OpenMenuAt(p))
            break;
        if (!dragging && ((e = ItemAt(p)) != NULL && e->fixed && e->path || Editable(e))) {
            clicked = e;
            GhostsMake();
            ghostsGo = FALSE;           /* (this menu is still up) */
            PostMessage(StartMenuOwner, WM_CANCELMODE, 0, 0L);
        }
        return TRUE;
    case WM_LBUTTONDOWN:
        if (PtInRect(&startButton, p)) {    /* the Start button again: the menu closes */
            PostMessage(StartMenuOwner, WM_CANCELMODE, 0, 0L);
            return TRUE;
        }
        dragging = FALSE;
        pressedAt = p;
        break;
    case WM_MOUSEMOVE:
        if (!dragging && (m->wParam & MK_LBUTTON) && Editable(e = ItemAt(pressedAt)) &&
            (abs(p.x - pressedAt.x) > DRAG_MIN || abs(p.y - pressedAt.y) > DRAG_MIN)) {
            dragged = e;
            dragging = TRUE;
        }
        if (dragging)
            PostMessage(StartMenuOwner, SMM_MARK, 0, 0L);
        break;
    case WM_LBUTTONUP:
        if (!dragging)
            break;
        dragging = FALSE;
        if (dropOn) {
            dropped = TRUE;
            e = dropOn;                 /* the pictures without the mark */
            after = dropAfter;
            MarkAt(NULL, NULL, FALSE);
            GhostsMake();
            dropOn = e;
            dropAfter = after;
            ghostsGo = FALSE;
            PostMessage(StartMenuOwner, WM_CANCELMODE, 0, 0L);
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
        for (i = 0; i < StartMenuCount && StartMenuEntries[i]->sub != e->parent; i++)
            ;
        e = i < StartMenuCount ? StartMenuEntries[i] : NULL;
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
            PostMessage(StartMenuOwner, WM_KEYDOWN, VK_RIGHT, 1L);
        for (k = 0; k < pos[i]; k++)
            PostMessage(StartMenuOwner, WM_KEYDOWN, VK_DOWN, 1L);
    }
}

/* The dragged item's folder now, and the order of the folder's items with
   it; the menu opens again on it. */
static void Dropped(void)
{
    LPSTR name = PathFileName(dragged->path);
    ENTRY far *e;
    int i, at = 0;

    DropFolder(dropOn, StartMenuOut.dir);
    lstrcpy(StartMenuOut.path, dragged->path);
    PathListInit(&StartMenuOut.order);
    if (dropOn->empty)
        PathListAdd(&StartMenuOut.order, name);
    for (i = 0; i < StartMenuCount && !dropOn->empty; i++) {
        e = StartMenuEntries[i];
        if (e->parent != dropOn->parent || e == dragged || !Editable(e))
            continue;
        if (e == dropOn && !dropAfter) {
            at = StartMenuOut.order.count;
            PathListAdd(&StartMenuOut.order, name);
        }
        PathListAdd(&StartMenuOut.order, PathFileName(e->path));
        if (e == dropOn && dropAfter) {
            at = StartMenuOut.order.count;
            PathListAdd(&StartMenuOut.order, name);
        }
    }
    StartMenuOut.depth = Trail(dropOn, StartMenuOut.pos);
    StartMenuOut.pos[StartMenuOut.depth - 1] = (BYTE)at;  /* the folder's own items come first in its menu */
    StartMenuOut.what = TRACK_DROP;
}

/* The dropped item into its folder, in its place. */
void StartMenuDragMove(HWND w)
{
    char from[MAX_PATH], to[MAX_PATH];

    lstrcpy(from, StartMenuOut.path);
    PathToParent(from);
    if (lstrcmpi(from, StartMenuOut.dir)) {
        lstrcpy(to, StartMenuOut.dir);
        PathAppend(to, PathFileName(StartMenuOut.path));
        if (DosGetAttributes(to) != 0xFFFF) {
            FileOperationError(w, "move", StartMenuOut.path, 80);
            return;
        }
        if (!DosRename(StartMenuOut.path, to)) {
            FileOperationError(w, "move", StartMenuOut.path, DosLastError());
            return;
        }
        StartMenuOrderRename(StartMenuOut.path, to);
        FileCommandsFolderChanged(from);
        FileCommandsFolderChanged(StartMenuOut.dir);
    }
    StartMenuOrderSet(StartMenuOut.dir, &StartMenuOut.order);
}

/* An opening starts: nothing pressed or dragged yet, the message filter on. */
void StartMenuDragBegin(const RECT *button)
{
    startButton = *button;
    clicked = dragged = dropOn = NULL;
    dragging = dropped = FALSE;
    ghostsGo = TRUE;
    pressedAt.x = pressedAt.y = -32000;  /* no press in it yet: the Start button's drags select */
    StartMenuOut.what = TRACK_CLOSED;
    menuTask = GetCurrentTask();         /* Windows 3.1 refuses a task's own filter from a DLL */
    filter = SetWindowsHookEx(WH_MSGFILTER, (HOOKPROC)StartMenuFilterProc, g_hInst, NULL);
}

/* It ended: the filter off; out says how (an item's menu next, a move). */
void StartMenuDragEnd(void)
{
    UnhookWindowsHookEx(filter);
    filter = NULL;
    if (clicked) {
        StartMenuOut.what = TRACK_ITEM;
        lstrcpy(StartMenuOut.path, clicked->path);
        StartMenuOut.fixed = clicked->fixed;
        StartMenuOut.depth = Trail(clicked, StartMenuOut.pos);
    } else if (dropped) {
        Dropped();
    }
}

/* The drop mark, if e has it (over the item's edge: on the menu itself). */
void StartMenuDragPaintMark(HDC dc, ENTRY far *e, DWORD org)
{
    RECT t;

    if (e != dropOn)
        return;
    MarkRect(e, dropAfter, &t);
    OffsetRect(&t, -(int)LOWORD(org), -(int)HIWORD(org));
    DrawSolidRect(dc, &t, RGB(0, 0, 0));
}

void StartMenuDragMark(void)
{
    if (dragging)
        Mark();
}

void StartMenuDragReopen(void)
{
    PostTrail(StartMenuOut.pos, StartMenuOut.depth);
}
