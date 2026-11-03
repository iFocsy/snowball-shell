/*
 * System-wide hooks for the taskbar (part of SNSYSTEM.DLL).
 *   WH_SHELL      top-level windows created / destroyed
 *   WH_CBT        activation, minimize/maximize, skin for new windows
 *   WH_CALLWNDPROC  maximized windows stop above the taskbar; title changes;
 *                   skin for document windows (MDI), as they become active
 *   WH_KEYBOARD     Alt+Tab, taken from Windows for the shell's switcher;
 *                   shortcut keys; the Windows key (alone, or with another)
 *   WH_MSGFILTER    the mouse in menus with the Windows 98 edge, and menus
 *                   that follow it with no button down
 * Events are posted to the taskbar window; the hooks never block.
 */
#include "snsystem.h"

typedef struct {                /* Windows 3.1 CWPSTRUCT */
    LPARAM lParam;
    WPARAM wParam;
    UINT   message;
    HWND   hwnd;
} CALLWND;

static HHOOK hShell, hCbt, hCall, hKey, hFilter;
static BOOL switching;                  /* Alt+Tab pressed, Alt not yet let go */
static HWND notify;
static RECT work;                       /* what maximized windows fill: the screen less the taskbar */
static BOOL skinAll, lost;
static HWND titlePending;               /* posted, not yet read by the taskbar */
static HTASK shellTask;                 /* its menus have the Windows 98 edge in any case */
static HWND newMenu;                    /* a menu window being made, skinned at its first message */
static BOOL pressed;                    /* the menu shown now got its press (HookFilterProc) */
static HWND pressedIn;                  /* the menu window it went down in */
static POINT pointer;                   /* where the pointer was at the menu's last move */
#define VK_LWIN     0x5B                /* the Windows keys (Windows 95's keyboard) */
#define VK_RWIN     0x5C
static BOOL winDown, winUsed;           /* a Windows key is down; another key came with it */
#define MAX_HOTKEYS 32
static WORD hotkeys[MAX_HOTKEYS];       /* the shortcuts' keys (SK_* << 8 | virtual key) */
static int nHotkeys;

/* Posts to the taskbar; a full queue loses the event, which the taskbar
   learns from HookLost at its next scan. */
static void Post(UINT msg, WPARAM wp, LPARAM lp)
{
    if (!PostMessage(notify, msg, wp, lp))
        lost = TRUE;
}

BOOL SNSYSTEM_API HookLost(void)
{
    BOOL was = lost;
    lost = FALSE;
    return was;
}

void SNSYSTEM_API HookTitleSeen(void)
{
    titlePending = NULL;
}

LRESULT CALLBACK __export HookShellProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HSHELL_WINDOWCREATED || code == HSHELL_WINDOWDESTROYED)
        Post(SBM_SHELL, code, (LPARAM)(WORD)wp);
    return CallNextHookEx(hShell, code, wp, lp);
}

LRESULT CALLBACK __export HookCbtProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HCBT_ACTIVATE) {
        if (skinAll)                    /* Windows 98 look for every program, once built */
            SkinWindow((HWND)wp);
        Post(SBM_ACTIVATE, wp, 0L);
    } else if (code == HCBT_MINMAX) {     /* with the state it leaves: 1 minimized, 2 maximized */
        Post(SBM_MINMAX, wp, MAKELONG(LOWORD(lp), IsIconic((HWND)wp) ? 1 : IsZoomed((HWND)wp) ? 2 : 0));
    } else if (code == HCBT_CREATEWND && SkinIsMenu((HWND)wp)) {
        newMenu = (HWND)wp;
    } else if (code == HCBT_DESTROYWND) {
        SkinForget((HWND)wp);
    }
    return CallNextHookEx(hCbt, code, wp, lp);
}

LRESULT CALLBACK __export HookCallWndProc(int code, WPARAM wp, LPARAM lp)
{
    CALLWND FAR *c = (CALLWND FAR *)lp;

    if (code >= 0 && c->hwnd == newMenu) {
        newMenu = NULL;
        SkinMenu(c->hwnd);
    }
    if (code >= 0 && c->message == WM_INITMENU) {
        pressed = FALSE;
        GetCursorPos(&pointer);
    }
    if (code >= 0 && c->message == WM_INITMENUPOPUP)
        Post(SBM_SOUND, SOUND_MENUPOPUP, 0L);
    if (code >= 0 && c->message == WM_NCACTIVATE && !(GetWindowLong(c->hwnd, GWL_STYLE) & WS_CHILD))
        Post(SBM_FLASH, (WPARAM)c->hwnd, c->wParam);    /* the taskbar's button flashes along */
    if (code >= 0 && c->message == WM_MDIACTIVATE && skinAll)
        SkinFrame(c->hwnd);             /* only document windows get it */
    if (code >= 0 && (c->message == WM_GETMINMAXINFO || c->message == WM_SETTEXT) &&
        !(GetWindowLong(c->hwnd, GWL_STYLE) & WS_CHILD)) {
        if (c->message == WM_SETTEXT) {
            if (c->hwnd != titlePending) {      /* a title that changes fast posts once */
                titlePending = c->hwnd;
                Post(SBM_TITLE, (WPARAM)c->hwnd, 0L);
            }
        } else if (!IsRectEmpty(&work)) {       /* the frame stays beyond the area, as beyond the screen */
            MINMAXINFO FAR *m = (MINMAXINFO FAR *)c->lParam;
            m->ptMaxPosition.x += work.left;
            m->ptMaxPosition.y += work.top;
            m->ptMaxSize.x -= work.left + GetSystemMetrics(SM_CXSCREEN) - work.right;
            m->ptMaxSize.y -= work.top + GetSystemMetrics(SM_CYSCREEN) - work.bottom;
        }
    }
    return CallNextHookEx(hCall, code, wp, lp);
}

/* A visible menu (a popup's window), or NULL. */
static HWND OpenPopup(void)
{
    char cls[8];
    HWND w;

    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT)) {
        GetClassName(w, cls, sizeof cls);
        if (IsWindowVisible(w) && !lstrcmp(cls, "#32768"))
            return w;
    }
    return NULL;
}

/* The point is on the menu bar of the window whose menu is open (the menu
   loop's messages are that window's): not on another window's, where a
   press would end the menu. */
static BOOL OnMenuBar(MSG FAR *m)
{
    HWND w = WindowFromPoint(m->pt);

    return w && w == m->hwnd && SendMessage(w, WM_NCHITTEST, 0, MAKELPARAM(m->pt.x, m->pt.y)) == HTMENU;
}

LRESULT CALLBACK __export HookFilterProc(int code, WPARAM wp, LPARAM lp)
{
    MSG FAR *m = (MSG FAR *)lp;
    BOOL inMenu, moved;
    RECT r;

    /* Windows 3.1 follows the mouse in a menu only while a button is down
       (a menu opened by a click, or by a key, stays put once it is let go),
       and takes no keys then; Windows 98 does both. The first move over a
       menu with no button becomes a press there, and the moves after it are
       made with the button down. A key first lets go of the button on the
       menu's corner, on no item, and comes again after it. Only a move of
       the pointer presses: Windows also sends one, the pointer still, when
       a menu opens or closes under it (the press would take the item under
       the pointer back from the keys). The menu bar counts as a menu: from
       File's menu straight over to Edit, Edit's opens. */
    if (code != MSGF_MENU)
        ;
    else if (m->message >= WM_MOUSEFIRST && m->message <= WM_MOUSELAST) {
        inMenu = SkinMenuPoint(m);
        if (m->message == WM_LBUTTONUP || m->message == WM_RBUTTONUP)
            pressed = FALSE;
        else if (m->message == WM_MOUSEMOVE && HookSkinning() && !(m->wParam & (MK_LBUTTON | MK_RBUTTON))) {
            moved = m->pt.x != pointer.x || m->pt.y != pointer.y;
            pointer = m->pt;
            if (!pressed && moved && (inMenu || OnMenuBar(m))) {
                pressed = TRUE;     /* (let go of on a menu's corner: on the bar it would close it) */
                pressedIn = inMenu ? WindowFromPoint(m->pt) : OpenPopup();
                m->message = WM_LBUTTONDOWN;
            }
            if (pressed)
                m->wParam |= MK_LBUTTON;
        }
    } else if ((m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN) && pressed) {
        pressed = FALSE;
        if (IsWindowVisible(pressedIn)) {
            PostMessage(m->hwnd, m->message, m->wParam, m->lParam);
            GetWindowRect(pressedIn, &r);
            m->message = WM_LBUTTONUP;
            m->wParam = 0;
            m->lParam = MAKELPARAM(r.left, r.top);
        }
    }
    return CallNextHookEx(hFilter, code, wp, lp);
}

/* Alt+Tab (Shift: backwards) goes to the taskbar instead of to Windows 3.1's
   own switcher; letting go of Alt ends it, Esc cancels. Alt's release is
   eaten too (else the program would open its menu) and taken out of the
   key state. Keys only peeked at (HC_NOREM) pass: they come again. */
LRESULT CALLBACK __export HookKeyboardProc(int code, WPARAM wp, LPARAM lp)
{
    BOOL up = (lp & 0x80000000L) != 0, alt = (lp & 0x20000000L) != 0;
    BYTE keys[256];

    /* the Windows key: let go alone, the Start menu; with a key, its command
       (Win+E, Win+R...); neither reaches the program */
    if (code == HC_ACTION && (wp == VK_LWIN || wp == VK_RWIN)) {
        if (!up && !winDown) {
            winDown = TRUE;
            winUsed = FALSE;
        } else if (up) {
            if (winDown && !winUsed)
                Post(SBM_WINKEY, 0, 0L);
            winDown = FALSE;
        }
        return 1;
    }
    if (code == HC_ACTION && winDown && wp != VK_SHIFT) {
        if (!up && !winUsed) {
            winUsed = TRUE;
            Post(SBM_WINKEY, wp, GetAsyncKeyState(VK_SHIFT) < 0);
        }
        return 1;
    }
    if (code == HC_ACTION && !up && wp != VK_SHIFT && wp != VK_CONTROL && wp != VK_MENU &&
        !GetProp(GetFocus(), "SbKeyField")) {          /* a shortcut key field takes any key */
        WORD key = (WORD)wp | ((GetAsyncKeyState(VK_SHIFT) < 0 ? SK_SHIFT : 0) |
                               (GetAsyncKeyState(VK_CONTROL) < 0 ? SK_CONTROL : 0) | (alt ? SK_ALT : 0)) << 8;
        int i;
        for (i = 0; i < nHotkeys; i++)
            if (hotkeys[i] == key) {
                Post(SBM_HOTKEY, i, 0L);
                return 1;
            }
    }
    if (code == HC_ACTION) {
        if (wp == VK_TAB && alt) {
            if (!up) {
                Post(SBM_SWITCH, !switching ? SWITCH_START :
                                 GetKeyState(VK_SHIFT) < 0 ? SWITCH_PREV : SWITCH_NEXT, 0L);
                switching = TRUE;
            }
            return 1;
        }
        if (switching && wp == VK_ESCAPE) {
            if (!up) {
                switching = FALSE;
                Post(SBM_SWITCH, SWITCH_CANCEL, 0L);
            }
            return 1;
        }
        if (switching && wp == VK_MENU && up) {
            switching = FALSE;
            Post(SBM_SWITCH, SWITCH_DONE, 0L);
            GetKeyboardState(keys);
            keys[VK_MENU] &= ~0x80;
            SetKeyboardState(keys);
            return 1;
        }
    }
    return CallNextHookEx(hKey, code, wp, lp);
}

/* The shortcuts' keys: pressed in any program, the taskbar is told which. */
void SNSYSTEM_API HookHotKeys(const WORD FAR *keys, int n)
{
    nHotkeys = min(n, MAX_HOTKEYS);
    _fmemcpy(hotkeys, keys, nHotkeys * sizeof(WORD));
}

/* Where menus go: the work area, the whole screen until there is one. */
void HookArea(RECT FAR *r)
{
    if (IsRectEmpty(&work))
        SetRect(r, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    else
        *r = work;
}

/* What maximized windows fill (the screen less the taskbar). */
void SNSYSTEM_API HookWorkArea(const RECT FAR *r)
{
    work = *r;
}

BOOL SNSYSTEM_API HookInstall(HWND w, BOOL skinPrograms)
{
    notify = w;
    skinAll = skinPrograms;
    shellTask = GetCurrentTask();
    SkinMenus(TRUE);
    hShell = SetWindowsHookEx(WH_SHELL, (HOOKPROC)HookShellProc, g_hInst, NULL);
    hCbt = SetWindowsHookEx(WH_CBT, (HOOKPROC)HookCbtProc, g_hInst, NULL);
    hCall = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)HookCallWndProc, g_hInst, NULL);
    hKey = SetWindowsHookEx(WH_KEYBOARD, (HOOKPROC)HookKeyboardProc, g_hInst, NULL);
    hFilter = SetWindowsHookEx(WH_MSGFILTER, (HOOKPROC)HookFilterProc, g_hInst, NULL);
    return hShell && hCbt && hCall && hKey && hFilter;
}

/* Whether the menus shown now get the Windows 98 look. */
BOOL HookSkinning(void)
{
    return skinAll || GetCurrentTask() == shellTask;
}

void SNSYSTEM_API HookRemove(void)
{
    if (hShell) UnhookWindowsHookEx(hShell);
    if (hCbt)   UnhookWindowsHookEx(hCbt);
    if (hCall)  UnhookWindowsHookEx(hCall);
    if (hKey)   UnhookWindowsHookEx(hKey);
    if (hFilter) UnhookWindowsHookEx(hFilter);
    hShell = hCbt = hCall = hKey = hFilter = NULL;
    SkinMenus(FALSE);
}

