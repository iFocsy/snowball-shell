/* Window classes and placement. */
#include "internal.h"

void WindowCenter(HWND w)
{
    RECT r;
    GetWindowRect(w, &r);
    SetWindowPos(w, NULL,
                 (GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) / 3,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

/* A class of SNSHELL's windows, with the arrow cursor; icon, background may be NULL. */
BOOL WindowRegister(LPCSTR name, WNDPROC proc, UINT style, int extra, HICON icon, HBRUSH background)
{
    WNDCLASS wc;

    _fmemset(&wc, 0, sizeof wc);
    wc.style = style;
    wc.lpfnWndProc = proc;
    wc.cbWndExtra = extra;
    wc.hInstance = g_hInst;
    wc.hIcon = icon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = background;
    wc.lpszClassName = name;
    return RegisterClass(&wc);
}

/* A window's rectangle in its parent's (or another window's) client coordinates. */
void WindowRectIn(HWND w, HWND in, RECT *r)
{
    GetWindowRect(w, r);
    MapWindowPoints(NULL, in, (POINT FAR *)r, 2);
}
