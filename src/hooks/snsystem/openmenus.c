/*
 * The tests' hooks (hooks.h), in SNSYSTEM.DLL: the chain of popup menus open
 * now, of any program, seen by Windows' WH_CALLWNDPROC hook (hooks.c), and
 * HookOpenMenus, which UIDRIVE's facts call through GetProcAddress.
 */
#include "snsystem.h"
#include "hooks.h"

#define OPEN_MENUS  8

static HMENU openMenu[OPEN_MENUS];      /* outermost first */
static HWND openOwner[OPEN_MENUS];      /* the windows they report to */
static int openMenus;

/* A popup menu opening: after the open one it is a submenu of (those after
   that have closed), else a menu of its own. */
static void Opening(HWND owner, HMENU m, int index)
{
    int i = openMenus - 1;

    while (i >= 0 && GetSubMenu(openMenu[i], index) != m)
        i--;
    openMenus = i + 1;
    if (openMenus < OPEN_MENUS) {
        openMenu[openMenus] = m;
        openOwner[openMenus++] = owner;
    }
}

void HookMenuMessage(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITMENU || (msg == WM_MENUSELECT && LOWORD(lp) == 0xFFFF && !HIWORD(lp)))
        openMenus = 0;                  /* a menu starting, or the menu closed */
    else if (msg == WM_INITMENUPOPUP)
        Opening(w, (HMENU)wp, LOWORD(lp));
}

/* As many as menu windows are shown (a submenu left by going back up is not). */
int SNSYSTEM_API HookOpenMenus(HMENU FAR *menus, HWND FAR *owners, int max)
{
    char cls[16];
    HWND w;
    int shown = 0, i;

    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT))
        if (IsWindowVisible(w) && GetClassName(w, cls, sizeof cls) && !lstrcmp(cls, "#32768"))
            shown++;
    for (i = 0; i < openMenus && i < shown && i < max; i++) {
        menus[i] = openMenu[i];
        owners[i] = openOwner[i];
    }
    return i;
}
