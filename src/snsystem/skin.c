/*
 * Windows 98 look for every program's windows (part of SNSYSTEM.DLL), in
 * three parts:
 *   skin.c      captions and frames, document windows (MDI) in the menu
 *               bar and as strips, putting the skin on windows
 *   skinctl.c   dialogs and their controls
 *   skinmenu.c  popup menus
 *   skinint.h   what they share
 *
 * The skin only replaces how windows are drawn: they keep their own
 * behaviour. A subclassed window keeps its previous window procedure in
 * two properties.
 */
#include "snsystem.h"
#include "skinint.h"

#define MDIBARS "SbMdiBars"     /* a frame whose menu bar has our document items: */
                                /* the document window they are for */
#define MDIMENU "SbMdiMenu"     /* ... and its window menu, opened by our icon item */
#define MDI_ITEM 0x53420000L    /* item data of those owner-drawn items, + MI_* */
#define MOVING  "SbMoving"      /* a minimized document window being dragged, */
#define PLACED  "SbPlaced"      /* ... left where it was dropped */
#define PENDING "SbPending"     /* a strip to be put in its place */

enum { MI_ICON = 1, MI_MIN, MI_RESTORE, MI_CLOSE };

LRESULT CALLBACK __export SkinIconTitleProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);

LRESULT SkinCallOld(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    WNDPROC old = (WNDPROC)MAKELONG(GetProp(w, OLD_LO), GetProp(w, OLD_HI));
    if (msg == WM_NCDESTROY) {
        SetWindowLong(w, GWL_WNDPROC, (LONG)old);
        RemoveProp(w, OLD_LO);
        RemoveProp(w, OLD_HI);
        RemoveProp(w, MDIBARS);
        RemoveProp(w, MDIMENU);
        RemoveProp(w, FRAMED);
        RemoveProp(w, MOVING);
        RemoveProp(w, PLACED);
        RemoveProp(w, PENDING);
    }
    return CallWindowProc((FARPROC)old, w, msg, wp, lp);
}

void SkinSubclass(HWND w, WNDPROC proc)
{
    LONG old;
    if (GetProp(w, OLD_HI))
        return;
    old = SetWindowLong(w, GWL_WNDPROC, (LONG)proc);
    SetProp(w, OLD_LO, (HANDLE)LOWORD(old));
    SetProp(w, OLD_HI, (HANDLE)HIWORD(old));
}

/* ---- Windows 98 caption and frame ---- */

#define HT_SB_CLOSE  SKIN_CLOSE /* our caption buttons (Windows 3.1 has no close button) */
#define HT_SB_MAX    SKIN_MAX
#define HT_SB_MIN    SKIN_MIN

#define STRIP_W      160        /* a minimized document window, as in Windows 98 */
#define STRIP_FRAME  3

static int StripHeight(void)            /* its strip; the window may be higher */
{
    return 2 * STRIP_FRAME + GetSystemMetrics(SM_CYCAPTION) - 1;
}

typedef struct {
    RECT caption, close, max, min;
    int frame;                  /* border thickness */
    int top;                    /* a strip: where it starts in its window */
} FRAMEGEOM;

/* Everything in window coordinates (0,0 = the window's top left corner). */
static void Geometry(HWND w, FRAMEGEOM *g)
{
    LONG style = GetWindowLong(w, GWL_STYLE);
    RECT win;
    POINT origin = { 0, 0 };
    int bh, bw;

    GetWindowRect(w, &win);
    ClientToScreen(w, &origin);
    g->frame = IsIconic(w) ? STRIP_FRAME : origin.x - win.left;
    g->top = IsIconic(w) ? max(0, win.bottom - win.top - StripHeight()) : 0;
    SetRect(&g->caption, g->frame, g->top + g->frame, win.right - win.left - g->frame,
            g->top + g->frame + GetSystemMetrics(SM_CYCAPTION) - 1);
    bh = g->caption.bottom - g->caption.top - 4;
    bw = bh + 2;
    SetRectEmpty(&g->close);
    SetRectEmpty(&g->max);
    SetRectEmpty(&g->min);
    if (style & WS_SYSMENU)
        SetRect(&g->close, g->caption.right - 2 - bw, g->caption.top + 2, g->caption.right - 2, g->caption.top + 2 + bh);
    if (style & (WS_MAXIMIZEBOX | WS_MINIMIZEBOX)) {
        SetRect(&g->max, g->close.left - 2 - bw, g->close.top, g->close.left - 2, g->close.bottom);
        SetRect(&g->min, g->max.left - bw, g->close.top, g->max.left, g->close.bottom);
    }
}

/* A document window inside a program (MDI), e.g. File Manager's. */
static BOOL IsMdiChild(HWND w)
{
    char cls[12];

    if (!(GetWindowLong(w, GWL_STYLE) & WS_CHILD))
        return FALSE;
    GetClassName(GetParent(w), cls, sizeof cls);
    return !lstrcmpi(cls, "MDIClient");
}

/* A document window looks active when it is the program's active one and
   the program is. */
static BOOL Active(HWND w)
{
    HWND top;

    if (!IsMdiChild(w))
        return GetActiveWindow() == w;
    for (top = w; GetWindowLong(top, GWL_STYLE) & WS_CHILD; top = GetParent(top))
        ;
    return (HWND)LOWORD(SendMessage(GetParent(w), WM_MDIGETACTIVE, 0, 0L)) == w && GetActiveWindow() == top;
}

/* Black glyphs drawn from rectangles: close X, maximize, restore, minimize. */
static void Glyph(HDC dc, const RECT *b, int which, BOOL down, BOOL enabled)
{
    COLORREF c = enabled ? RGB(0, 0, 0) : GetSysColor(COLOR_BTNSHADOW);
    int x = (b->left + b->right) / 2 + down, y = (b->top + b->bottom) / 2 + down, i;
    RECT r;

    switch (which) {
    case HT_SB_CLOSE:                                   /* 8x7 X, two pixels wide */
        for (i = 0; i < 7; i++) {
            SetRect(&r, x - 4 + i, y - 4 + i, x - 2 + i, y - 3 + i);         DrawSolidRect(dc, &r, c);
            SetRect(&r, x + 2 - i, y - 4 + i, x + 4 - i, y - 3 + i);         DrawSolidRect(dc, &r, c);
        }
        break;
    case HT_SB_MAX:                                     /* 9x9 window, thick top */
        SetRect(&r, x - 5, y - 5, x + 4, y + 4);        DrawFrame(dc, &r, c, c);
        SetRect(&r, x - 5, y - 4, x + 4, y - 3);        DrawSolidRect(dc, &r, c);
        break;
    case HT_SB_MAX + 100:                               /* restore: two windows */
        SetRect(&r, x - 3, y - 6, x + 4, y + 0);        DrawFrame(dc, &r, c, c);
        SetRect(&r, x - 3, y - 5, x + 4, y - 4);        DrawSolidRect(dc, &r, c);
        SetRect(&r, x - 5, y - 3, x + 2, y + 3);
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
        DrawFrame(dc, &r, c, c);
        SetRect(&r, x - 5, y - 2, x + 2, y - 1);        DrawSolidRect(dc, &r, c);
        break;
    case HT_SB_MIN:                                     /* 6x2 bar at the bottom */
        SetRect(&r, x - 4, y + 2, x + 2, y + 4);        DrawSolidRect(dc, &r, c);
        break;
    }
}

static void CaptionButton(HDC dc, const RECT *b, int which, BOOL down, BOOL enabled)
{
    RECT r = *b;

    if (IsRectEmpty(&r))
        return;
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
    DrawBevel(dc, &r, down);
    Glyph(dc, &r, which, down, enabled);
}

/* A caption button drawn anywhere (Display Properties' picture). */
void SNSYSTEM_API SkinCaptionButton(HDC dc, const RECT FAR *r, int which)
{
    RECT b = *r;
    CaptionButton(dc, &b, which, FALSE, TRUE);
}

/* ---- a maximized document window's place in its program's menu bar ---- */

static HWND MdiClient(HWND frame)
{
    char cls[12];
    HWND c;

    for (c = GetWindow(frame, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        GetClassName(c, cls, sizeof cls);
        if (!lstrcmpi(cls, "MDIClient"))
            return c;
    }
    return NULL;
}

static void CaptionButton(HDC dc, const RECT *b, int which, BOOL down, BOOL enabled);

/* With a document window maximized, MDI puts Windows 3.1's two bitmaps in
   the menu bar: the window menu box first, the restore button last. Here
   they become, as in Windows 98, the document's icon and three buttons:
   Minimize and Restore are put in before the last item, which becomes
   Close (DefFrameProc hands the document these commands, as it did
   Restore). MDI no longer knows them as its own, so what it does when it
   takes its items out is done here: they go once nothing is maximized, or
   when MDI puts in fresh ones for another document, and the document they
   were for gets its window menu style back (MDI takes it while maximized).
   Nothing changes when nothing has to (this runs on repaints). */
static void MdiDone(HWND frame)
{
    HWND child = (HWND)GetProp(frame, MDIBARS);

    if (IsWindow(child) && !IsZoomed(child) && !(GetWindowLong(child, GWL_STYLE) & WS_SYSMENU)) {
        SetWindowLong(child, GWL_STYLE, GetWindowLong(child, GWL_STYLE) | WS_SYSMENU);
        SetWindowPos(child, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

static void MdiMenu(HWND frame)
{
    HMENU m = GetMenu(frame);
    HWND client = MdiClient(frame);
    BOOL ours = GetProp(frame, MDIBARS) != NULL, changed = FALSE;
    DWORD act;
    int last;

    if (!m || !client)
        return;
    act = SendMessage(client, WM_MDIGETACTIVE, 0, 0L);
    if (LOWORD(act) && HIWORD(act)) {
        /* A menu's item that opens a menu has its flags in the low byte only
           (MF_BITMAP is there, MF_OWNERDRAW is not): ours is known by its menu. */
        if ((GetMenuState(m, 0, MF_BYPOSITION) & MF_BITMAP) && GetSubMenu(m, 0)) {
            if (GetSubMenu(m, 1) && GetSubMenu(m, 1) == (HMENU)GetProp(frame, MDIMENU))
                RemoveMenu(m, 1, MF_BYPOSITION);                    /* the icon of the one before */
            SetProp(frame, MDIMENU, (HANDLE)GetSubMenu(m, 0));
            if (ours && (HWND)GetProp(frame, MDIBARS) != (HWND)LOWORD(act)) {
                MdiDone(frame);
                SetProp(frame, MDIBARS, (HANDLE)LOWORD(act));
            }
            ModifyMenu(m, 0, MF_BYPOSITION | MF_POPUP | MF_OWNERDRAW, (UINT)GetSubMenu(m, 0),
                       (LPCSTR)(MDI_ITEM | MI_ICON));
            changed = TRUE;
        }
        last = GetMenuItemCount(m) - 1;
        if (last > 0 && GetMenuItemID(m, last) == SC_RESTORE && (GetMenuState(m, last, MF_BYPOSITION) & MF_BITMAP)) {
            if (ours)
                DeleteMenu(m, SC_CLOSE, MF_BYCOMMAND);      /* the Close of the one before */
            else {
                InsertMenu(m, last, MF_BYPOSITION | MF_OWNERDRAW | MF_HELP, SC_MINIMIZE, (LPCSTR)(MDI_ITEM | MI_MIN));
                InsertMenu(m, last + 1, MF_BYPOSITION | MF_OWNERDRAW, SC_RESTORE, (LPCSTR)(MDI_ITEM | MI_RESTORE));
                SetProp(frame, MDIBARS, (HANDLE)LOWORD(act));
            }
            last = GetMenuItemCount(m) - 1;
            ModifyMenu(m, last, MF_BYPOSITION | MF_OWNERDRAW, SC_CLOSE, (LPCSTR)(MDI_ITEM | MI_CLOSE));
            changed = TRUE;
        }
    } else if (ours) {
        if (GetSubMenu(m, 0) && GetSubMenu(m, 0) == (HMENU)GetProp(frame, MDIMENU))
            RemoveMenu(m, 0, MF_BYPOSITION);            /* the document's window menu: not ours to destroy */
        RemoveProp(frame, MDIMENU);
        DeleteMenu(m, SC_MINIMIZE, MF_BYCOMMAND);
        DeleteMenu(m, SC_RESTORE, MF_BYCOMMAND);
        DeleteMenu(m, SC_CLOSE, MF_BYCOMMAND);
        MdiDone(frame);
        RemoveProp(frame, MDIBARS);
        changed = TRUE;
    }
    if (changed)
        DrawMenuBar(frame);
}

static void CaptionButton(HDC dc, const RECT *b, int which, BOOL down, BOOL enabled);

/* The four items: the document's icon, or a button of caption buttons' look. */
static BOOL MdiItem(HWND frame, UINT msg, LPARAM lp)
{
    MEASUREITEMSTRUCT FAR *mi = (MEASUREITEMSTRUCT FAR *)lp;
    const DRAWITEMSTRUCT FAR *d = (const DRAWITEMSTRUCT FAR *)lp;
    HWND child;
    HICON icon;
    RECT r;
    int which;

    if (msg == WM_MEASUREITEM) {
        if (mi->CtlType != ODT_MENU || HIWORD(mi->itemData) != HIWORD(MDI_ITEM))
            return FALSE;
        mi->itemWidth = LOWORD(mi->itemData) == MI_ICON ? 16 : 14;
        mi->itemHeight = GetSystemMetrics(SM_CYMENU) - 2;
        return TRUE;
    }
    if (d->CtlType != ODT_MENU || HIWORD(d->itemData) != HIWORD(MDI_ITEM))
        return FALSE;
    r = d->rcItem;
    DrawSolidRect(d->hDC, &r, GetSysColor(COLOR_MENU));
    which = LOWORD(d->itemData);
    if (which == MI_ICON) {
        child = (HWND)LOWORD(SendMessage(MdiClient(frame), WM_MDIGETACTIVE, 0, 0L));
        icon = child ? (HICON)GetClassWord(child, GCW_HICON) : NULL;
        if (!icon)                      /* as Windows 98 shows such documents */
            icon = LoadIcon(NULL, IDI_APPLICATION);
        DrawIconImage(d->hDC, r.left + (r.right - r.left - 16) / 2, r.top + (r.bottom - r.top - 16) / 2, icon, TRUE);
    } else {
        SetRect(&r, r.left, r.top + (r.bottom - r.top - 14) / 2, r.left + 16, r.top + (r.bottom - r.top - 14) / 2 + 14);
        CaptionButton(d->hDC, &r, which == MI_MIN ? HT_SB_MIN : which == MI_CLOSE ? HT_SB_CLOSE : HT_SB_MAX + 100,
                      (d->itemState & ODS_SELECTED) != 0, TRUE);
    }
    return TRUE;
}

static void PaintFrame(HWND w, BOOL active, int pressed)
{
    LONG style = GetWindowLong(w, GWL_STYLE);
    HDC dc = GetWindowDC(w);
    FRAMEGEOM g;
    RECT win, r, t;
    char title[128];
    HICON icon;
    HFONT old;
    int i;

    Geometry(w, &g);
    GetWindowRect(w, &win);
    OffsetRect(&win, -win.left, -win.top);
    if (g.top > 0) {                                    /* a strip, at the bottom of a higher window */
        SetRect(&r, 0, 0, win.right, g.top);
        DrawSolidRect(dc, &r, GetSysColor(COLOR_APPWORKSPACE));
        win.top = g.top;
    }
    if (g.frame > 0) {                                  /* raised edge, then face */
        r = win;
        DrawFrame(dc, &r, GetSysColor(COLOR_BTNFACE), RGB(0, 0, 0));
        InflateRect(&r, -1, -1);
        DrawFrame(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
        for (i = 2; i < g.frame; i++) {                 /* the border's colour */
            InflateRect(&r, -1, -1);
            DrawFrame(dc, &r, GetSysColor(active ? COLOR_ACTIVEBORDER : COLOR_INACTIVEBORDER),
                      GetSysColor(active ? COLOR_ACTIVEBORDER : COLOR_INACTIVEBORDER));
        }
    }
    r = g.caption;                                      /* the row below the caption too */
    SetRect(&t, r.left, r.bottom, r.right, r.bottom + 1);
    DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNFACE));
    DrawGradient(dc, &r, GetSysColor(active ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION),
             active ? CAPTION_END_ACTIVE : CAPTION_END_INACTIVE, FALSE);

    t = r;
    t.left += 2;
    if ((style & WS_SYSMENU) && (style & (WS_THICKFRAME | WS_MINIMIZEBOX)) &&      /* not dialogs */
        (icon = (HICON)GetClassWord(w, GCW_HICON)) != NULL) {
        DrawIconImage(dc, t.left, t.top + (t.bottom - t.top - 16) / 2, icon, TRUE);
        t.left += 18;
    }
    t.left += 2;
    t.right = (IsRectEmpty(&g.min) ? g.close.left : g.min.left) - 2;
    GetWindowText(w, title, sizeof title);
    old = SelectObject(dc, DrawFont(TRUE));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT));
    DrawTextEllipsis(dc, title, &t, DT_LEFT | DT_NOPREFIX);
    SelectObject(dc, old);

    CaptionButton(dc, &g.min, IsIconic(w) ? HT_SB_MAX + 100 : HT_SB_MIN, pressed == HT_SB_MIN,
                  (style & WS_MINIMIZEBOX) != 0);
    CaptionButton(dc, &g.max, IsZoomed(w) ? HT_SB_MAX + 100 : HT_SB_MAX, pressed == HT_SB_MAX,
                  (style & WS_MAXIMIZEBOX) != 0);
    CaptionButton(dc, &g.close, HT_SB_CLOSE, pressed == HT_SB_CLOSE, TRUE);
    if (GetMenu(w) && !(style & WS_CHILD)) {    /* Windows 3.1's black line under the menu bar */
        POINT o;
        RECT c, wr;
        o.x = o.y = 0;
        ClientToScreen(w, &o);
        GetWindowRect(w, &wr);
        GetClientRect(w, &c);
        SetRect(&t, o.x - wr.left, o.y - wr.top - 1, o.x - wr.left + c.right, o.y - wr.top);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_MENU));
    }
    ReleaseDC(w, dc);
}

static int HitCaption(HWND w, LPARAM screenPoint)
{
    FRAMEGEOM g;
    RECT win;
    POINT p;

    Geometry(w, &g);
    GetWindowRect(w, &win);
    p.x = LOWORD(screenPoint) - win.left;
    p.y = HIWORD(screenPoint) - win.top;
    if (!PtInRect(&g.caption, p))
        return 0;
    if (PtInRect(&g.close, p)) return HT_SB_CLOSE;
    if (PtInRect(&g.max, p) && (GetWindowLong(w, GWL_STYLE) & WS_MAXIMIZEBOX)) return HT_SB_MAX;
    if (PtInRect(&g.min, p) && (GetWindowLong(w, GWL_STYLE) & WS_MINIMIZEBOX)) return HT_SB_MIN;
    if (p.x < g.caption.left + 20 && (GetWindowLong(w, GWL_STYLE) & WS_SYSMENU)) return HTSYSMENU;
    return HTCAPTION;
}

/* ---- a minimized document window: Windows 98's strip, not an icon ---- */

/* Posted to a strip to put it in its place: a message no program uses. */
static UINT StripPlaceMessage(void)
{
    static UINT msg;
    if (!msg)
        msg = RegisterWindowMessage("SbStripPlace");
    return msg;
}

/* Another minimized document window of the program is at (x, y). */
static BOOL StripTaken(HWND self, int x, int y)
{
    HWND s;
    RECT r;

    for (s = GetWindow(GetParent(self), GW_CHILD); s; s = GetWindow(s, GW_HWNDNEXT))
        if (s != self && IsIconic(s) && IsWindowVisible(s)) {
            GetWindowRect(s, &r);
            ScreenToClient(GetParent(self), (POINT FAR *)&r.left);
            if (r.left == x && r.top == y)
                return TRUE;
        }
    return FALSE;
}

/* Minimized, a document window gets a strip's size instead of an icon's
   (Windows' other moves of it are left alone: it may be on its way back). */
static void StripSize(WINDOWPOS FAR *p)
{
    if (!(p->flags & SWP_NOSIZE) && p->cx <= 64 && p->cy <= 64) {
        p->cx = STRIP_W;
        p->cy = StripHeight();
    }
}

/* Where a strip goes: where it is, if that is a free place along the bottom
   of the program's work area, else the first free one, rows going up. */
static void StripPlace(HWND w)
{
    int h = StripHeight(), row, perRow, k, x, y, free = -1;
    RECT c, r;

    GetClientRect(GetParent(w), &c);
    GetWindowRect(w, &r);
    ScreenToClient(GetParent(w), (POINT FAR *)&r.left);
    ScreenToClient(GetParent(w), (POINT FAR *)&r.right);
    row = max(h, r.bottom - r.top);     /* Windows keeps a minimized window 36 pixels high */
    perRow = max(1, c.right / STRIP_W);
    for (k = 0; k < 200; k++) {
        x = k % perRow * STRIP_W;
        y = c.bottom - (k / perRow + 1) * row;
        if (StripTaken(w, x, y))
            continue;
        if (x == r.left && y == r.top && r.right - r.left == STRIP_W)
            return;                     /* in a free place already */
        if (free < 0)
            free = k;
    }
    SetWindowPos(w, NULL, free % perRow * STRIP_W, c.bottom - (free / perRow + 1) * row, STRIP_W, row,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

/* After Windows moved a strip (minimized it, arranged the icons), it is put
   in its place once Windows is done; dragged, it stays where it is dropped. */
static void StripMoved(HWND w, const WINDOWPOS FAR *p)
{
    if (GetProp(w, MOVING)) {
        SetProp(w, PLACED, (HANDLE)1);
        return;
    }
    if (!(p->flags & SWP_NOMOVE))
        RemoveProp(w, PLACED);
    if (!GetProp(w, PLACED) && !GetProp(w, PENDING)) {
        SetProp(w, PENDING, (HANDLE)1);
        PostMessage(w, StripPlaceMessage(), 0, 0L);
    }
}

/* Windows 3.1's title under an icon, a window of its own, while its icon
   is a strip: never shown, no size (hiding it again and again would make
   Windows show the icon again and again). */
LRESULT CALLBACK __export SkinIconTitleProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    WINDOWPOS FAR *p = (WINDOWPOS FAR *)lp;
    HWND icon = GetWindow(w, GW_OWNER);

    if (msg == WM_WINDOWPOSCHANGING && icon && IsIconic(icon) && IsMdiChild(icon)) {
        p->flags = (p->flags & ~(SWP_SHOWWINDOW | SWP_NOSIZE));
        p->cx = p->cy = 0;
    }
    return SkinCallOld(w, msg, wp, lp);
}

static void QuietIconTitle(HWND w)
{
    HWND s;
    char cls[8];
    int pass;

    for (pass = 0; pass < 2; pass++)            /* among the documents, else top level */
        for (s = GetWindow(pass ? GetDesktopWindow() : GetParent(w), GW_CHILD); s; s = GetWindow(s, GW_HWNDNEXT)) {
            GetClassName(s, cls, sizeof cls);
            if (!lstrcmp(cls, "#32772") && GetWindow(s, GW_OWNER) == w && !GetProp(s, OLD_HI)) {
                SkinSubclass(s, SkinIconTitleProc);
                SetWindowPos(s, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW);
            }
        }
}

static BOOL TrackButton(HWND w, int which);

/* Messages of a minimized document window the strip takes; FALSE: as usual. */
static BOOL StripMessage(HWND w, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res)
{
    PAINTSTRUCT ps;
    int hit;

    switch (msg) {
    case WM_NCLBUTTONDOWN:              /* Windows sees all of an icon as its caption */
    case WM_NCLBUTTONDBLCLK:
        if ((hit = HitCaption(w, lp)) < HT_SB_CLOSE)
            return FALSE;
        if (msg == WM_NCLBUTTONDOWN && TrackButton(w, hit))
            PostMessage(w, WM_SYSCOMMAND, hit == HT_SB_CLOSE ? SC_CLOSE : hit == HT_SB_MAX ? SC_MAXIMIZE : SC_RESTORE, 0L);
        *res = 0;
        return TRUE;
    case WM_PAINT:
    case WM_PAINTICON:
        BeginPaint(w, &ps);
        EndPaint(w, &ps);
        PaintFrame(w, Active(w), 0);
        *res = 0;
        return TRUE;
    case WM_ERASEBKGND:
    case WM_ICONERASEBKGND:
        *res = 1;
        return TRUE;
    default:
        if (msg != StripPlaceMessage())
            return FALSE;
        RemoveProp(w, PENDING);
        StripPlace(w);
        *res = 0;
        return TRUE;
    case WM_SYSCOMMAND:                 /* dragged: stays where it is dropped */
        if ((wp & 0xFFF0) != SC_MOVE)
            return FALSE;
        SetProp(w, MOVING, (HANDLE)1);
        *res = SkinCallOld(w, msg, wp, lp);
        RemoveProp(w, MOVING);
        return TRUE;
    }
    return FALSE;
}

/* Pressed look while the mouse is held on a caption button; TRUE if released on it. */
static BOOL TrackButton(HWND w, int which)
{
    BOOL active = Active(w), inside = TRUE;
    MSG msg;

    SetCapture(w);
    PaintFrame(w, active, which);
    while (GetMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST)) {
        DWORD pos = GetMessagePos();
        BOOL now = HitCaption(w, pos) == which;
        if (msg.message == WM_LBUTTONUP)
            break;
        if (now != inside) {
            inside = now;
            PaintFrame(w, active, inside ? which : 0);
        }
    }
    ReleaseCapture();
    PaintFrame(w, active, 0);
    return inside;
}

/* The mouse pressed on our caption buttons or on the icon; FALSE: as usual. */
static BOOL CaptionDown(HWND w, WPARAM hit)
{
    switch (hit) {
    case HT_SB_CLOSE:
        if (TrackButton(w, HT_SB_CLOSE)) PostMessage(w, WM_SYSCOMMAND, SC_CLOSE, 0L);
        return TRUE;
    case HT_SB_MAX:
        if (TrackButton(w, HT_SB_MAX)) PostMessage(w, WM_SYSCOMMAND, IsZoomed(w) ? SC_RESTORE : SC_MAXIMIZE, 0L);
        return TRUE;
    case HT_SB_MIN:                     /* a strip's restores */
        if (TrackButton(w, HT_SB_MIN)) PostMessage(w, WM_SYSCOMMAND, IsIconic(w) ? SC_RESTORE : SC_MINIMIZE, 0L);
        return TRUE;
    case HTSYSMENU:                     /* the icon opens the window menu, as Alt+Space does */
        if (IsMdiChild(w))              /* a document's: MDI opens it in its place */
            return FALSE;
        SetActiveWindow(w);
        PostMessage(w, WM_SYSCOMMAND, SC_KEYMENU, ' ');
        return TRUE;
    }
    return FALSE;
}

/* A double click on the icon closes the window, on our buttons it is taken; FALSE: as usual. */
static BOOL CaptionDoubleClick(HWND w, WPARAM hit)
{
    if (hit == HTSYSMENU) {
        PostMessage(w, WM_SYSCOMMAND, SC_CLOSE, 0L);
        return TRUE;
    }
    if (hit >= HT_SB_CLOSE)
        return TRUE;
    return FALSE;
}

/* After the old procedure: our frame over what it drew, a strip put in its place. */
static void AfterOld(HWND w, UINT msg, LPARAM lp)
{
    switch (msg) {
    case WM_NCPAINT: case WM_SETTEXT: case WM_INITMENU: case WM_SIZE:
        if (IsWindow(w) && !IsIconic(w)) {
            PaintFrame(w, Active(w), 0);
            if (!(GetWindowLong(w, GWL_STYLE) & WS_CHILD))
                MdiMenu(w);
        } else if (IsWindow(w) && IsMdiChild(w)) {
            PaintFrame(w, Active(w), 0);
        }
        break;
    case WM_WINDOWPOSCHANGED:
        if (IsWindow(w) && IsIconic(w) && IsMdiChild(w)) {
            QuietIconTitle(w);
            StripMoved(w, (const WINDOWPOS FAR *)lp);
        }
        break;
    case WM_MENUSELECT:                 /* window menu closed: its box was drawn over us */
        if (LOWORD(lp) == 0xFFFF && IsWindow(w))
            PaintFrame(w, Active(w), 0);
        break;
    }
}

LRESULT CALLBACK __export SkinFrameProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res;
    int hit;

    if ((GetWindowLong(w, GWL_STYLE) & WS_CAPTION) != WS_CAPTION)
        return SkinCallOld(w, msg, wp, lp);
    if (msg == WM_WINDOWPOSCHANGING && IsIconic(w) && IsMdiChild(w))
        StripSize((WINDOWPOS FAR *)lp);
    else if (IsIconic(w) && IsMdiChild(w) && StripMessage(w, msg, wp, lp, &res))
        return res;
    switch (msg) {
    case WM_NCHITTEST:
        if ((hit = HitCaption(w, lp)) != 0)
            return hit;
        break;
    case WM_MEASUREITEM:
    case WM_DRAWITEM:
        if (MdiItem(w, msg, lp))
            return TRUE;
        break;
    case WM_NCLBUTTONDOWN:
        if (CaptionDown(w, wp))
            return 0;
        break;
    case WM_NCLBUTTONDBLCLK:
        if (CaptionDoubleClick(w, wp))
            return 0;
        break;
    case WM_NCACTIVATE:                 /* Windows draws the old caption first */
        res = SkinCallOld(w, msg, wp, lp);
        PaintFrame(w, (BOOL)wp, 0);
        return res;
    }
    res = SkinCallOld(w, msg, wp, lp);
    AfterOld(w, msg, lp);
    return res;
}

/* Gives a top-level or document window the Windows 98 caption and frame, once. */
void SNSYSTEM_API SkinFrame(HWND w)
{
    if (!(GetWindowLong(w, GWL_STYLE) & WS_CAPTION) || GetProp(w, OLD_HI) ||
        ((GetWindowLong(w, GWL_STYLE) & WS_CHILD) && !IsMdiChild(w)))
        return;
    SkinSubclass(w, SkinFrameProc);
    SetWindowPos(w, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

/* From the CBT hook when a window of any program is about to be activated
   (fully built by then): dialogs and captioned top-level windows, once. */
void SkinWindow(HWND w)
{
    char cls[8];

    if (GetProp(w, OLD_HI) || (GetWindowLong(w, GWL_STYLE) & WS_CHILD))
        return;
    GetClassName(w, cls, sizeof cls);
    if (!lstrcmp(cls, "#32770"))
        SkinDialog(w);
    else
        SkinFrame(w);
}
