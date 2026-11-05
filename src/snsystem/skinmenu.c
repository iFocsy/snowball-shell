/*
 * Windows 98 look for popup menus (part of the skin, see skin.c): the
 * raised edge, etched separators, a submenu placed at its menu's edge.
 * Menu windows (#32768) are Windows' own, kept for later menus of any
 * program.
 */
#include "snsystem.h"
#include "skinint.h"

LRESULT CALLBACK __export SkinMenuProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);

/* ---- popup menus ---- */

/* Windows 3.1 draws a menu's black border and its shadow inside the window,
   the items one pixel in. Given a frame (2 pixels left and above, 1 right
   and below) and 3 pixels more each way, it draws all that as before
   within; Windows 98's raised edge is then painted over the frame, the old
   border and the shadow. */
#define MENU_GROW   3

static void PaintMenu(HWND w)
{
    HDC dc = GetWindowDC(w);
    RECT r;

    GetWindowRect(w, &r);
    OffsetRect(&r, -r.left, -r.top);
    SkinRaisedEdge(dc, &r);
    DrawFrame(dc, &r, GetSysColor(COLOR_MENU), GetSysColor(COLOR_MENU));
    ReleaseDC(w, dc);
}

/* Windows 3.1's separators are a line in the text's colour across the
   items (client 1 to right - 3); they become Windows 98's etched line,
   as the shell's own menus draw it. */
static void EtchSeparators(HWND w)
{
    HDC dc = GetDC(w);
    COLORREF line = GetNearestColor(dc, GetSysColor(COLOR_MENUTEXT));
    RECT c, r;
    int y;

    GetClientRect(w, &c);
    for (y = 1; y < c.bottom - 3; y++) {
        if (GetPixel(dc, 1, y) != line || GetPixel(dc, c.right / 2, y) != line ||
            GetPixel(dc, c.right - 3, y) != line)
            continue;
        SetRect(&r, 1, y, c.right - 2, y + 1);
        DrawSolidRect(dc, &r, GetSysColor(COLOR_MENU));
        SetRect(&r, 3, y, c.right - 4, y + 1);
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNSHADOW));
        OffsetRect(&r, 0, 1);
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT));
        y++;
    }
    ReleaseDC(w, dc);
}

static RECT chosen;                     /* the owner-drawn item last drawn selected, on the screen */
#define MAX_PICTURES 8
static RECT picRect[MAX_PICTURES];      /* menus (window, screen) as they were, */
static HBITMAP picBmp[MAX_PICTURES];    /* their pictures (the caller's) */
static int nPictures;
#define MAX_OPEN    8
static HWND open[MAX_OPEN];             /* menu windows placed and not hidden since, in that order */
static int nOpen;

/* A menu's client area on the screen. */
static void ClientOnScreen(HWND m, RECT *c)
{
    POINT o = { 0, 0 };

    GetClientRect(m, c);
    ClientToScreen(m, &o);
    OffsetRect(c, o.x, o.y);
}

/* A menu window is placed or shown, or hidden. Windows may place a
   submenu before it shows the menu it opens from (keys typed fast). */
static void Placed(HWND w, const WINDOWPOS FAR *p)
{
    int i;

    for (i = 0; i < nOpen && open[i] != w; i++)
        ;
    if (p->flags & SWP_HIDEWINDOW) {
        if (i < nOpen)
            _fmemmove(open + i, open + i + 1, (--nOpen - i) * sizeof open[0]);
    } else if ((!(p->flags & SWP_NOMOVE) || (p->flags & SWP_SHOWWINDOW)) && i == nOpen && nOpen < MAX_OPEN) {
        open[nOpen++] = w;
    }
}

/* The menu a submenu opens from: the one with the owner-drawn item chosen
   last, else the one placed last. */
static HWND OpenedFrom(HWND w)
{
    HWND last = NULL;
    RECT c;
    int i;

    for (i = nOpen - 1; i >= 0; i--) {
        if (open[i] == w || !IsWindow(open[i]))
            continue;
        ClientOnScreen(open[i], &c);
        if (chosen.right > chosen.left && chosen.left >= c.left && chosen.right <= c.right &&
            chosen.top >= c.top && chosen.bottom <= c.bottom)
            return open[i];
        if (!last)
            last = open[i];
    }
    return last;
}

/* From the hooks: an owner-drawn item drawn (NULL: a menu starts). */
void SkinMenuDrawn(const DRAWITEMSTRUCT FAR *d)
{
    DWORD org;

    if (!d) {
        SetRectEmpty(&chosen);
        nOpen = 0;
    } else if (d->CtlType == ODT_MENU && (d->itemState & ODS_SELECTED)) {
        org = GetDCOrg(d->hDC);
        chosen = d->rcItem;
        OffsetRect(&chosen, LOWORD(org), HIWORD(org));
    }
}

/* The screen top of the item highlighted in menu m, or -1: searched for
   down a column near its right (past a banner and the submenu arrow). */
static int HighlightTop(HWND m)
{
    HDC dc = GetDC(m);
    COLORREF hi = GetNearestColor(dc, GetSysColor(COLOR_HIGHLIGHT));
    POINT o = { 0, 0 };
    RECT c;
    int x, y;

    GetClientRect(m, &c);
    x = c.right - 4;
    for (y = 0; y < c.bottom && GetPixel(dc, x, y) != hi; y += 4)    /* items are higher */
        ;
    while (y > 0 && y < c.bottom && GetPixel(dc, x, y - 1) == hi)
        y--;
    ReleaseDC(m, dc);
    ClientToScreen(m, &o);
    return y < c.bottom ? o.y + y : -1;
}

/* A submenu covers only its menu's edge (Windows 3.1's covered 9 pixels,
   arrows too): on the right, else on the left; beside its item's column
   (Windows 3.1 put it past the last). One too long to go down from its item
   goes up only as far as it must, beside it (Windows 3.1 put it all above
   the item). Any menu stays on the screen at its new size. Not skinned:
   Windows 3.1's place, only beside an owner-drawn item's column. */
static void PlaceMenu(HWND w, int FAR *x, int FAR *y, int cx, int cy, BOOL skinned)
{
    HWND from = OpenedFrom(w);
    RECT r, c, area;
    int top, before = 0, after = 0;

    HookArea(&area);                    /* the screen less the taskbar */
    if (from) {
        GetWindowRect(from, &r);
        ClientOnScreen(from, &c);
        if (chosen.left >= c.left && chosen.right <= c.right && chosen.top >= c.top && chosen.bottom <= c.bottom &&
            chosen.right > chosen.left) {       /* owner-drawn: its item is known */
            before = chosen.left - c.left;      /* (less: Windows' margin, no other column) */
            after = c.right - chosen.right;
            before = before < 8 ? 0 : before;
            after = after < 8 ? 0 : after;
            top = chosen.top;
        } else if (skinned) {
            top = HighlightTop(from);
        }
        if (!skinned) {
            *x += *x < r.left ? before : -after;
            return;
        }
        *x = r.right - after - MENU_GROW;
        if (*x + cx > area.right)
            *x = r.left + before - cx + MENU_GROW;
        /* (its items start 3 pixels in: level with the item's) */
        if (*y + cy <= r.bottom && top >= 0 && *y + cy <= top + 3)
            *y = top - 3;
    }
    *x = max(area.left, min(*x, area.right - cx));
    *y = max(area.top, min(*y, area.bottom - cy));
}

/* A program that ends its menu to open it again (the shell's Start menu, for
   an item's own menu or a move) gives pictures of its menus as they were: a
   menu window erased in the same place is given its picture instead, which
   the items then paint over (else they would be blank until then: Windows
   paints after the keys that open the menu again). n 0: none. */
void SNSYSTEM_API SkinMenuPictures(const RECT FAR *r, const HBITMAP FAR *b, int n)
{
    nPictures = min(n, MAX_PICTURES);
    _fmemcpy(picRect, r, nPictures * sizeof *r);
    _fmemcpy(picBmp, b, nPictures * sizeof *b);
}

static BOOL ErasePicture(HWND w, HDC dc)
{
    POINT o = { 0, 0 };
    HBITMAP old;
    HDC mem;
    RECT r, c;
    int i;

    GetWindowRect(w, &r);
    for (i = 0; i < nPictures && !EqualRect(&r, &picRect[i]); i++)
        ;
    if (i == nPictures || (mem = CreateCompatibleDC(dc)) == NULL)
        return FALSE;
    ClientToScreen(w, &o);
    GetClientRect(w, &c);
    old = SelectObject(mem, picBmp[i]);
    BitBlt(dc, 0, 0, c.right, c.bottom, mem, o.x - r.left, o.y - r.top, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
    return TRUE;
}

LRESULT CALLBACK __export SkinMenuProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    WINDOWPOS FAR *p = (WINDOWPOS FAR *)lp;
    RECT FAR *r = (RECT FAR *)lp;       /* WM_NCCALCSIZE: the window, made the client */
    RECT now;
    LRESULT res;

    if (msg == WM_ERASEBKGND && nPictures && ErasePicture(w, (HDC)wp))
        return 1;
    if (!HookSkinning()) {              /* a menu of a program left as it was */
        if (msg == WM_WINDOWPOSCHANGING && !(p->flags & SWP_NOMOVE) && !(p->flags & SWP_NOSIZE))
            PlaceMenu(w, &p->x, &p->y, p->cx, p->cy, FALSE);
        if (msg == WM_WINDOWPOSCHANGING)
            Placed(w, p);
        return SkinCallOld(w, msg, wp, lp);
    }
    switch (msg) {
    case WM_NCCALCSIZE:
        r->left += 2;
        r->top += 2;
        r->right -= 1;
        r->bottom -= 1;
        return 0;
    case WM_NCPAINT:
        PaintMenu(w);
        return 0;
    case WM_WINDOWPOSCHANGING:
        GetWindowRect(w, &now);
        if (!(p->flags & SWP_NOSIZE)) {
            p->cx += MENU_GROW;
            p->cy += MENU_GROW;
        }
        if (!(p->flags & SWP_NOMOVE))
            PlaceMenu(w, &p->x, &p->y, p->flags & SWP_NOSIZE ? now.right - now.left : p->cx,
                      p->flags & SWP_NOSIZE ? now.bottom - now.top : p->cy, TRUE);
        Placed(w, p);
        break;
    }
    res = SkinCallOld(w, msg, wp, lp);
    if (msg == WM_PAINT) {
        PaintMenu(w);
        EtchSeparators(w);
    }
    return res;
}

BOOL SkinIsMenu(HWND w)
{
    char cls[8];

    GetClassName(w, cls, sizeof cls);
    return !lstrcmp(cls, "#32768");
}

/* A popup menu window, kept by Windows for later menus of any program. The
   hooks give it the edge from its first message (Windows sets a new
   window's procedure after the CBT hook). */
void SkinMenu(HWND w)
{
    SkinSubclass(w, SkinMenuProc);
}

/* All menu windows made so far get the edge, or give it back (the shell
   ends: they outlive the library). */
void SkinMenus(BOOL on)
{
    HWND w;

    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT)) {
        if (!SkinIsMenu(w))
            continue;
        if (on)
            SkinSubclass(w, SkinMenuProc);
        else
            SkinUnsubclass(w);
    }
}

/* Windows 3.1 finds a menu's items from its window's corner, less 1 pixel,
   but draws them from the client's, now 2 pixels in: a mouse message to the
   menu loop (screen point) is moved back the difference (measured: 1),
   staying in the menu. From the cursor's point, which stays: the loop may
   filter a message twice. Whether the point is in a menu with the edge. */
BOOL SkinMenuPoint(MSG FAR *m)
{
    POINT p = m->pt, o = { 0, 0 };
    RECT r;
    HWND w;

    w = WindowFromPoint(p);
    if (!w || !SkinIsMenu(w) || !GetProp(w, OLD_HI))
        return FALSE;
    GetWindowRect(w, &r);
    ClientToScreen(w, &o);
    p.x = max(r.left, p.x - (o.x - r.left - 1));
    p.y = max(r.top, p.y - (o.y - r.top - 1));
    m->lParam = MAKELPARAM(p.x, p.y);
    return TRUE;
}
