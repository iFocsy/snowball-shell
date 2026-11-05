/*
 * Windows 98's slider: a sunken groove and a raised thumb over positions
 * from the left (SLS_VERTICAL: from the bottom); dragged, clicked beside the thumb (a step that way) or
 * moved with the arrows, Home and End. The parent hears of each change by
 * WM_COMMAND, SLN_CHANGE.
 */
#include "internal.h"

#define THUMB_W     11
#define Pos(w)      ((int)GetWindowWord(w, 0))
#define Count(w)    ((int)GetWindowWord(w, 2))

static BOOL Vertical(HWND w)
{
    return (GetWindowLong(w, GWL_STYLE) & SLS_VERTICAL) != 0;
}

/* Its length, and a point along it from where positions start. */
static int Length(HWND w)
{
    RECT c;

    GetClientRect(w, &c);
    return Vertical(w) ? c.bottom : c.right;
}

static int Along(HWND w, LPARAM lp)
{
    return Vertical(w) ? Length(w) - 1 - (short)HIWORD(lp) : (short)LOWORD(lp);
}

/* Where the thumb starts, along it. */
static int ThumbAt(HWND w, int pos)
{
    return Count(w) > 1 ? pos * (Length(w) - THUMB_W) / (Count(w) - 1) : 0;
}

static int PosAt(HWND w, int x)
{
    int span = Length(w) - THUMB_W;

    if (Count(w) < 2 || span <= 0)
        return 0;
    return (int)(((long)(x - THUMB_W / 2) * (Count(w) - 1) + span / 2) / span);
}

static void Set(HWND w, int pos, BOOL notify)
{
    pos = max(0, min(pos, Count(w) - 1));
    if (pos == Pos(w))
        return;
    SetWindowWord(w, 0, pos);
    InvalidateRect(w, NULL, FALSE);
    if (notify)
        SendMessage(GetParent(w), WM_COMMAND, GetDlgCtrlID(w), MAKELPARAM(w, SLN_CHANGE));
}

static void Paint(HWND w)
{
    PAINTSTRUCT ps;
    RECT c, r;
    int x;

    BeginPaint(w, &ps);
    GetClientRect(w, &c);
    DrawSolidRect(ps.hdc, &c, GetSysColor(COLOR_BTNFACE));
    x = ThumbAt(w, Pos(w));
    if (Vertical(w)) {
        SetRect(&r, c.right / 2 - 2, THUMB_W / 2, c.right / 2 + 2, c.bottom - THUMB_W / 2);
        DrawBevel(ps.hdc, &r, TRUE);
        SetRect(&r, 1, c.bottom - x - THUMB_W, c.right - 1, c.bottom - x);
    } else {
        SetRect(&r, THUMB_W / 2, c.bottom / 2 - 2, c.right - THUMB_W / 2, c.bottom / 2 + 2);
        DrawBevel(ps.hdc, &r, TRUE);
        SetRect(&r, x, 1, x + THUMB_W, c.bottom - 1);
    }
    DrawSolidRect(ps.hdc, &r, GetSysColor(COLOR_BTNFACE));
    DrawBevel(ps.hdc, &r, FALSE);
    if (GetFocus() == w)
        DrawFocusRect(ps.hdc, &c);
    EndPaint(w, &ps);
}

LRESULT CALLBACK __export SliderProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    int x = Along(w, lp), up = Vertical(w) ? 1 : -1;

    HOOK_DESCRIBE(msg, SliderDescribe(w, (LPSTR)lp, wp));

    switch (msg) {
    case WM_PAINT:
        Paint(w);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(w, NULL, FALSE);
        return 0;
    case WM_KEYDOWN:
        switch (wp) {
        case VK_LEFT:  Set(w, Pos(w) - 1, TRUE); break;
        case VK_RIGHT: Set(w, Pos(w) + 1, TRUE); break;
        case VK_UP:    Set(w, Pos(w) + up, TRUE); break;
        case VK_DOWN:  Set(w, Pos(w) - up, TRUE); break;
        case VK_HOME:                Set(w, 0, TRUE); break;
        case VK_END:                 Set(w, Count(w) - 1, TRUE); break;
        }
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(w);
        if (x < ThumbAt(w, Pos(w)))
            Set(w, Pos(w) - 1, TRUE);
        else if (x >= ThumbAt(w, Pos(w)) + THUMB_W)
            Set(w, Pos(w) + 1, TRUE);
        else
            SetCapture(w);
        return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == w)
            Set(w, PosAt(w, x), TRUE);
        return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == w)
            ReleaseCapture();
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

void SliderSetCount(HWND s, int n)
{
    SetWindowWord(s, 2, n);
    Set(s, Pos(s), FALSE);
    InvalidateRect(s, NULL, FALSE);
}

void SliderSetPos(HWND s, int pos)
{
    Set(s, pos, FALSE);
}

int SliderPos(HWND s)
{
    return Pos(s);
}

BOOL SliderRegister(void)
{

    return WindowRegister(SLIDER_CLASS, SliderProc, 0, 4, NULL, NULL);
}

#ifdef SNOWBALL_HOOKS
int SliderCount(HWND s)                 /* the tests' hooks (src\hooks) */
{
    return Count(s);
}
#endif
