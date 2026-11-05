/*
 * Windows 98's scroll bar arrows (part of the skin, see skin.c). Windows
 * 3.1 draws its own from the display driver's bitmaps, grey in any scheme.
 * When a window with scroll bars gets a message after which Windows may
 * have drawn them (its frame painted, a scroll, its size or contents
 * changed), the hooks post it to the taskbar, which has them drawn over:
 * buttons in the scheme's colours with Windows 98's small triangles,
 * pressed while held. Once per window until done, however many messages.
 */
#include "snsystem.h"
#include "skinint.h"

#define MAX_PENDING 8

static HWND pending[MAX_PENDING];

enum { UP, DOWN, LEFT, RIGHT };

/* A triangle of 1, 3, 5 and 7 pixels pointing towards way, centred in r. */
static void Triangle(HDC dc, const RECT *r, int way, COLORREF c)
{
    int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2, i;
    RECT t;

    for (i = 0; i < 4; i++) {
        switch (way) {
        case UP:    SetRect(&t, x - i, y - 2 + i, x + i + 1, y - 1 + i); break;
        case DOWN:  SetRect(&t, x - 3 + i, y - 1 + i, x + 4 - i, y + i); break;
        case LEFT:  SetRect(&t, x - 2 + i, y - i, x - 1 + i, y + i + 1); break;
        default:    SetRect(&t, x - 1 + i, y - 3 + i, x + i, y + 4 - i); break;
        }
        DrawSolidRect(dc, &t, c);
    }
}

/* One arrow's button, over Windows 3.1's bitmap; pressed if the mouse holds it. */
static void Arrow(HDC dc, HWND w, int x, int y, int cx, int cy, int way)
{
    POINT p;
    RECT r, s;
    BOOL down;

    SetRect(&r, x, y, x + cx, y + cy);
    GetCursorPos(&p);
    GetWindowRect(w, &s);
    p.x -= s.left;
    p.y -= s.top;
    down = GetCapture() == w && GetAsyncKeyState(VK_LBUTTON) < 0 && PtInRect(&r, p);
    DrawSolidRect(dc, &r, DrawSysColor(COLOR_BTNFACE));
    if (down) {                         /* flat, the triangle a pixel on */
        DrawFrame(dc, &r, DrawSysColor(COLOR_BTNSHADOW), DrawSysColor(COLOR_BTNSHADOW));
        OffsetRect(&r, 1, 1);
    } else {
        DrawBevel(dc, &r, FALSE);
    }
    Triangle(dc, &r, way, DrawSysColor(COLOR_BTNTEXT));
}

/* The arrows of w's scroll bars: where Windows 3.1 puts its bitmaps
   (measured), inside the scroll bars' lines, against the client area. */
void SNSYSTEM_API SkinScrollArrows(HWND w)
{
    int vx = GetSystemMetrics(SM_CXVSCROLL) - 2, vy = GetSystemMetrics(SM_CYVSCROLL) - 2;
    int hx = GetSystemMetrics(SM_CXHSCROLL) - 2, hy = GetSystemMetrics(SM_CYHSCROLL) - 2;
    POINT o = { 0, 0 };
    RECT win, c;
    LONG style;
    HDC dc;
    int i;

    for (i = 0; i < MAX_PENDING; i++)
        if (pending[i] == w)
            pending[i] = NULL;
    if (!IsWindow(w))                   /* gone before the taskbar got to it */
        return;
    style = GetWindowLong(w, GWL_STYLE);
    if (!IsWindowVisible(w) || IsIconic(w) || !(style & (WS_VSCROLL | WS_HSCROLL)) || !SkinWanted(w))
        return;
    GetWindowRect(w, &win);
    GetClientRect(w, &c);
    ClientToScreen(w, &o);
    OffsetRect(&c, o.x - win.left, o.y - win.top);      /* in the window's own coordinates */
    dc = GetWindowDC(w);
    if ((style & WS_VSCROLL) && c.bottom - c.top >= 2 * vy) {
        Arrow(dc, w, c.right + 1, c.top, vx, vy, UP);
        Arrow(dc, w, c.right + 1, c.bottom - vy, vx, vy, DOWN);
    }
    if ((style & WS_HSCROLL) && c.right - c.left >= 2 * hx) {
        Arrow(dc, w, c.left, c.bottom + 1, hx, hy, LEFT);
        Arrow(dc, w, c.right - hx, c.bottom + 1, hx, hy, RIGHT);
    }
    ReleaseDC(w, dc);
}

/* From the hooks, before w gets msg: after it, its arrows may be Windows 3.1's
   again; the taskbar is asked to draw them over (notify), once. */
void SkinScrollMessage(HWND w, UINT msg, HWND notify)
{
    int i, free = -1;

    switch (msg) {
    case WM_NCPAINT: case WM_VSCROLL: case WM_HSCROLL: case WM_SIZE: case WM_SHOWWINDOW:
    case WM_SETTEXT: case WM_SETREDRAW: case WM_ENABLE:
        break;
    default:
        if (msg < WM_USER)              /* a list's or an edit's own messages: their contents */
            return;
    }
    if (!(GetWindowLong(w, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL)) || !SkinWanted(w))
        return;
    for (i = 0; i < MAX_PENDING; i++)
        if (pending[i] == w)
            return;
        else if (!pending[i] && free < 0)
            free = i;
    if (free >= 0 && PostMessage(notify, SBM_ARROWS, (WPARAM)w, 0L))
        pending[free] = w;
}
