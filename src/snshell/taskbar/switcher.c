/*
 * Alt+Tab as in Windows 98: a box in the middle of the screen with the
 * programs' icons, the most recently used first, the chosen one framed
 * and its title below; letting go of Alt switches to it. SNSYSTEM's
 * keyboard hook sends the keys (SBM_SWITCH); the box never takes the
 * focus, so the keys keep going to the hook.
 */
#include "internal.h"

#define SWITCH_CLASS "SbSwitch"
#define MAX_WINS    40
#define CELL        43
#define PER_ROW     7
#define MARGIN      12
#define TITLE_H     22
#define TIMER_ALT   1                   /* Alt let go without the hook seeing it */

static HWND box;
static HWND wins[MAX_WINS];
static int n, sel;

static void Collect(void)
{
    HWND w;

    n = 0;
    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w && n < MAX_WINS; w = GetWindow(w, GW_HWNDNEXT))
        if (TaskbarHasButton(w))
            wins[n++] = w;              /* front first: the most recently used */
}

static void CellRect(int i, RECT *r)
{
    int x = MARGIN + (i % PER_ROW) * CELL, y = MARGIN + (i / PER_ROW) * CELL;
    SetRect(r, x, y, x + CELL, y + CELL);
}

static void Paint(HWND w)
{
    PAINTSTRUCT ps;
    RECT c, r;
    HFONT old;
    int i;

    BeginPaint(w, &ps);
    GetClientRect(w, &c);
    DrawSolidRect(ps.hdc, &c, GetSysColor(COLOR_BTNFACE));
    DrawBevel(ps.hdc, &c, FALSE);
    for (i = 0; i < n; i++) {
        CellRect(i, &r);
        if (i == sel) {
            InflateRect(&r, -2, -2);
            DrawFrame(ps.hdc, &r, GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT));
            InflateRect(&r, -1, -1);
            DrawFrame(ps.hdc, &r, GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT));
            InflateRect(&r, 3, 3);
        }
        DrawIcon(ps.hdc, r.left + (CELL - 32) / 2, r.top + (CELL - 32) / 2, TaskbarIcon(wins[i]));
    }
    SetRect(&r, MARGIN, c.bottom - MARGIN - TITLE_H, c.right - MARGIN, c.bottom - MARGIN);
    DrawBevel(ps.hdc, &r, TRUE);
    InflateRect(&r, -4, 0);
    old = SelectObject(ps.hdc, DrawFont(FALSE));
    SetBkMode(ps.hdc, TRANSPARENT);
    SetTextColor(ps.hdc, GetSysColor(COLOR_BTNTEXT));
    if (sel < n)
        DrawTextEllipsis(ps.hdc, TaskbarTitle(wins[sel]), &r, DT_CENTER | DT_NOPREFIX);
    SelectObject(ps.hdc, old);
    EndPaint(w, &ps);
}

static void Done(BOOL go)
{
    KillTimer(box, TIMER_ALT);
    ShowWindow(box, SW_HIDE);
    if (go && sel < n && IsWindow(wins[sel]))
        TaskbarSwitchTo(wins[sel]);
}

LRESULT CALLBACK __export SwitcherProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:
        Paint(w);
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_TIMER:
        if (GetAsyncKeyState(VK_MENU) >= 0)
            Done(TRUE);
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

static void Show(void)
{
    WNDCLASS wc;
    int rows = (n + PER_ROW - 1) / PER_ROW, cols = min(n, PER_ROW);
    int cx = 2 * MARGIN + max(cols * CELL, 200), cy = 2 * MARGIN + rows * CELL + 8 + TITLE_H;

    if (!box) {
        _fmemset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = SwitcherProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = SWITCH_CLASS;
        RegisterClass(&wc);
        box = CreateWindowEx(WS_EX_TOPMOST, SWITCH_CLASS, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
        if (!box)
            return;
    }
    /* Windows 3.1 neither moves nor sizes a window SetWindowPos also shows */
    SetWindowPos(box, HWND_TOPMOST, (GetSystemMetrics(SM_CXSCREEN) - cx) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - cy) / 2, cx, cy, SWP_NOACTIVATE);
    ShowWindow(box, SW_SHOWNA);
    InvalidateRect(box, NULL, FALSE);
    SetTimer(box, TIMER_ALT, 100, NULL);
}

/* SBM_SWITCH. A step with the box not up starts it (the hook may have
   missed a release of Alt). */
void SwitcherCommand(UINT what)
{
    BOOL up = box && IsWindowVisible(box);

    if ((what == SWITCH_NEXT || what == SWITCH_PREV) && !up)
        what = SWITCH_START;
    switch (what) {
    case SWITCH_START:
        Collect();
        if (!n)
            return;
        sel = n > 1;
        Show();
        break;
    case SWITCH_NEXT:
    case SWITCH_PREV:
        sel = (sel + (what == SWITCH_NEXT ? 1 : n - 1)) % n;
        InvalidateRect(box, NULL, FALSE);
        break;
    case SWITCH_DONE:
    case SWITCH_CANCEL:
        if (up)
            Done(what == SWITCH_DONE);
        break;
    }
}
