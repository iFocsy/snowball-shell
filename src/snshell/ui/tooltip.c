/*
 * SbTip: one tooltip at a time, Windows 98's pale yellow box with a black
 * edge. It never takes activation or the mouse; whoever shows it hides it.
 */
#include "internal.h"

#define TIP_CLASS   "SbTip"
#define TIP_BACK    RGB(255, 255, 225)
#define TIP_WRAP    280                 /* longer lines wrap (a folder's infotip) */

static HWND tip;
static char text[260];
static BOOL wrapped;

LRESULT CALLBACK __export TooltipProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    RECT r;
    HFONT old;

    switch (msg) {
    case WM_PAINT:
        BeginPaint(w, &ps);
        GetClientRect(w, &r);
        DrawSolidRect(ps.hdc, &r, TIP_BACK);
        DrawFrame(ps.hdc, &r, RGB(0, 0, 0), RGB(0, 0, 0));
        old = SelectObject(ps.hdc, DrawFont(FALSE));
        SetBkMode(ps.hdc, TRANSPARENT);
        SetTextColor(ps.hdc, RGB(0, 0, 0));
        if (wrapped) {
            InflateRect(&r, -4, -3);
            DrawText(ps.hdc, text, -1, &r, DT_WORDBREAK | DT_NOPREFIX);
        } else {
            DrawText(ps.hdc, text, -1, &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        }
        SelectObject(ps.hdc, old);
        EndPaint(w, &ps);
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    }
    return DefWindowProc(w, msg, wp, lp);
}

/* Shows s with its top left at (x, y) on the screen, moved to fit; above
   y when it would not fit below (the taskbar's tips). */
void TooltipShow(LPCSTR s, int x, int y)
{
    WNDCLASS wc;
    HDC dc;
    HFONT old;
    RECT r;
    int cx, cy = 18, sx = GetSystemMetrics(SM_CXSCREEN), sy = GetSystemMetrics(SM_CYSCREEN);

    if (!tip) {
        _fmemset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = TooltipProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = TIP_CLASS;
        RegisterClass(&wc);
        tip = CreateWindowEx(WS_EX_TOPMOST, TIP_CLASS, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
        if (!tip)
            return;
    }
    lstrcpyn(text, s, sizeof text);
    dc = GetDC(tip);
    old = SelectObject(dc, DrawFont(FALSE));
    cx = LOWORD(GetTextExtent(dc, text, lstrlen(text))) + 8;
    if ((wrapped = cx > TIP_WRAP + 8 || _fstrchr(text, '\n') != NULL) != FALSE) {
        LPSTR line, end;
        for (cx = 0, line = text; *line; line = *end ? end + 1 : end) {     /* the widest line */
            for (end = line; *end && *end != '\n'; end++)
                ;
            cx = max(cx, (int)LOWORD(GetTextExtent(dc, line, (int)(end - line))));
        }
        SetRect(&r, 0, 0, min(cx, TIP_WRAP), 0);
        DrawText(dc, text, -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        cx = r.right + 8;
        cy = r.bottom + 6;
    }
    SelectObject(dc, old);
    ReleaseDC(tip, dc);
    if (y + cy > sy)
        y -= cy + 24;
    x = max(0, min(x, sx - cx));
    SetWindowPos(tip, HWND_TOPMOST, x, y, cx, cy, SWP_NOACTIVATE);   /* then shown: not both at once */
    ShowWindow(tip, SW_SHOWNA);
    InvalidateRect(tip, NULL, TRUE);
}

void TooltipHide(void)
{
    if (tip && IsWindowVisible(tip))
        ShowWindow(tip, SW_HIDE);
}
