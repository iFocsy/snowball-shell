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

/* The menu a submenu opens from: the one shown last. */
static HWND OpenedFrom(HWND w)
{
    char cls[8];
    HWND m;

    for (m = GetWindow(GetDesktopWindow(), GW_CHILD); m; m = GetWindow(m, GW_HWNDNEXT)) {
        GetClassName(m, cls, sizeof cls);
        if (m != w && IsWindowVisible(m) && !lstrcmp(cls, "#32768"))
            return m;
    }
    return NULL;
}

/* A submenu covers only its menu's edge (Windows 3.1's covered 9 pixels,
   arrows too): on the right, else on the left. Any menu stays on the
   screen at its new size. */
static void PlaceMenu(HWND w, int FAR *x, int FAR *y, int cx, int cy)
{
    HWND from = OpenedFrom(w);
    RECT r, area;

    HookArea(&area);                    /* the screen less the taskbar */
    if (from) {
        GetWindowRect(from, &r);
        *x = r.right - MENU_GROW;
        if (*x + cx > area.right)
            *x = r.left - cx + MENU_GROW;
    }
    *x = max(area.left, min(*x, area.right - cx));
    *y = max(area.top, min(*y, area.bottom - cy));
}

LRESULT CALLBACK __export SkinMenuProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    WINDOWPOS FAR *p = (WINDOWPOS FAR *)lp;
    RECT FAR *r = (RECT FAR *)lp;       /* WM_NCCALCSIZE: the window, made the client */
    RECT now;
    LRESULT res;

    if (!HookSkinning())                /* a menu of a program left as it was */
        return SkinCallOld(w, msg, wp, lp);
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
                      p->flags & SWP_NOSIZE ? now.bottom - now.top : p->cy);
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
        if (on) {
            SkinSubclass(w, SkinMenuProc);
        } else if (GetProp(w, OLD_HI)) {
            SetWindowLong(w, GWL_WNDPROC, MAKELONG(GetProp(w, OLD_LO), GetProp(w, OLD_HI)));
            RemoveProp(w, OLD_LO);
            RemoveProp(w, OLD_HI);
        }
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
