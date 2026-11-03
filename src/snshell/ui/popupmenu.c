/*
 * A popup menu at the cursor that returns the command chosen: Windows 3.1
 * has no TPM_RETURNCMD, and sends a command chosen with the keyboard before
 * TrackPopupMenu returns but posts one chosen with the mouse. A hidden
 * window owns the menu, keeps the command and passes the menu's other
 * messages (owner-drawn items, submenus filled as they open) to the owner.
 */
#include "internal.h"

#define POPUPMENU_CLASS "SbPopupMenu"

static HWND keeper, owner;
static UINT chosen;

LRESULT CALLBACK __export PopupMenuProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COMMAND:
        chosen = wp;
        return 0;
    case WM_INITMENUPOPUP:
    case WM_MENUSELECT:
    case WM_MENUCHAR:
    case WM_MEASUREITEM:
    case WM_DRAWITEM:
        return SendMessage(owner, msg, wp, lp);
    }
    return DefWindowProc(w, msg, wp, lp);
}

UINT PopupMenuTrack(HMENU m, HWND w)
{
    WNDCLASS wc;
    POINT p;
    MSG msg;

    if (!keeper) {
        _fmemset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = PopupMenuProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = POPUPMENU_CLASS;
        RegisterClass(&wc);
        if ((keeper = CreateWindow(POPUPMENU_CLASS, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL)) == NULL)
            return 0;
    }
    owner = w;
    chosen = 0;
    GetCursorPos(&p);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, 0, keeper, NULL);
    if (PeekMessage(&msg, keeper, WM_COMMAND, WM_COMMAND, PM_REMOVE))
        chosen = msg.wParam;
    return chosen;
}
