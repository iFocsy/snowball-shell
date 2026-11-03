/* Window placement. */
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
