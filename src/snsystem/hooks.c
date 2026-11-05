/*
 * System-wide hooks for the taskbar (part of SNSYSTEM.DLL).
 *   WH_SHELL      top-level windows created / destroyed
 *   WH_CBT        activation, minimize/maximize, skin for new windows
 *   WH_CALLWNDPROC  maximized windows stop above the taskbar; title changes;
 *                   scroll bars' arrows to be drawn over;
 *                   skin for document windows (MDI), as they become active;
 *                   the zoom of windows not skinned (SkinFrameProc zooms those)
 *   WH_KEYBOARD     Alt+Tab, taken from Windows for the shell's switcher;
 *                   shortcut keys; the Windows key (alone, or with another)
 *   WH_MSGFILTER    the mouse in menus with the Windows 98 edge, and menus
 *                   that follow it with no button down
 * Events are posted to the taskbar window; the hooks never block.
 */
#include "snsystem.h"
#include "hooks.h"                      /* the tests' hooks (src\hooks): nothing without SNOWBALL_HOOKS */

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
static BOOL skinAll, skinShell = TRUE, lost;
static HWND titlePending;               /* posted, not yet read by the taskbar */
static HTASK shellTask;                 /* its menus have the Windows 98 edge in any case */
static HWND newMenu;                    /* a menu window being made, skinned at its first message */
static BOOL pressed;                    /* the menu shown now got its press (HookFilterProc) */
static BOOL barMenu;                    /* the menu shown now is the menu bar's (not a context menu) */
static HWND rightTo;                    /* a right click outside the menu goes on to this window */
static HWND pressedIn;                  /* the menu window it went down in */
static POINT pointer;                   /* where the pointer was at the menu's last move */
#define VK_LWIN     0x5B                /* the Windows keys (Windows 95's keyboard) */
#define VK_RWIN     0x5C
static BOOL winDown, winUsed;           /* a Windows key is down; another key came with it */
#define MAX_HOTKEYS 32
static WORD hotkeys[MAX_HOTKEYS];       /* the shortcuts' keys (SK_* << 8 | virtual key) */
static int nHotkeys;
#define MAX_BUTTONS 40
static HWND buttonWnd[MAX_BUTTONS];     /* each window's taskbar button (screen), */
static RECT buttonRect[MAX_BUTTONS];    /* where it zooms to and from */
static int nButtons;

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
        barMenu = c->wParam && (HMENU)c->wParam == GetMenu(c->hwnd);
        rightTo = NULL;
        GetCursorPos(&pointer);
        SkinMenuDrawn(NULL);
    }
    if (code >= 0 && c->message == WM_DRAWITEM)
        SkinMenuDrawn((const DRAWITEMSTRUCT FAR *)c->lParam);
    if (code >= 0 && c->message == WM_SYSCOMMAND && !SkinHas(c->hwnd))
        HookZoom(c->hwnd, c->wParam & 0xFFF0);
    if (code >= 0 && (c->message == WM_SYSCOLORCHANGE || c->message == WM_WININICHANGE))
        SkinColorsChanged();            /* the Windows 98 colours are read again when used */
    HOOK_MENU_MESSAGE(code, c);
    if (code >= 0 && c->message == WM_INITMENUPOPUP)
        Post(SBM_SOUND, SOUND_MENUPOPUP, 0L);
    if (code >= 0)
        SkinScrollMessage(c->hwnd, c->message, notify);
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

/* The point is on the menu bar of the window whose bar's menu is open (the
   menu loop's messages are that window's): not on another window's, nor
   with a context menu open, where a press would end the menu. */
static BOOL OnMenuBar(MSG FAR *m)
{
    HWND w;

    if (!barMenu)
        return FALSE;
    w = WindowFromPoint(m->pt);
    return w && w == m->hwnd && SendMessage(w, WM_NCHITTEST, 0, MAKELPARAM(m->pt.x, m->pt.y)) == HTMENU;
}

/* A right click outside the menu: the menu ends, and the press (up: its
   release) goes on to the window under it, posted to come after the menu
   is gone, so that window's own menu opens (Windows 98's way). */
static void RightClickThrough(MSG FAR *m, BOOL up)
{
    HWND w = up ? rightTo : WindowFromPoint(m->pt);
    POINT p = m->pt;
    UINT hit;

    if (!up)
        PostMessage(m->hwnd, WM_CANCELMODE, 0, 0L);
    rightTo = up ? NULL : w;
    if (!w || !IsWindow(w))
        return;
    hit = (UINT)SendMessage(w, WM_NCHITTEST, 0, MAKELPARAM(p.x, p.y));
    if (hit == HTCLIENT) {
        ScreenToClient(w, &p);
        PostMessage(w, up ? WM_RBUTTONUP : WM_RBUTTONDOWN, up ? 0 : MK_RBUTTON, MAKELPARAM(p.x, p.y));
    } else {
        PostMessage(w, up ? WM_NCRBUTTONUP : WM_NCRBUTTONDOWN, hit, MAKELPARAM(p.x, p.y));
    }
}

/* After w was moved (SkinFrameProc's SC_MOVE): let go of at the screen's
   edge, Windows 7's snap (left or right half, top maximized), done by
   the taskbar. */
void HookMoved(HWND w)
{
    POINT p;

    GetCursorPos(&p);
    if (p.x <= 0)
        Post(SBM_SNAP, (WPARAM)w, VK_LEFT);
    else if (p.x >= GetSystemMetrics(SM_CXSCREEN) - 1)
        Post(SBM_SNAP, (WPARAM)w, VK_RIGHT);
    else if (p.y <= 0)
        Post(SBM_SNAP, (WPARAM)w, VK_UP);
}

LRESULT CALLBACK __export HookFilterProc(int code, WPARAM wp, LPARAM lp)
{
    MSG FAR *m = (MSG FAR *)lp;
    BOOL inMenu, moved;
    HWND under;
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
       File's menu straight over to Edit, Edit's opens (only the bar's own
       menus: a context menu would end). A right click anywhere else ends
       the menu, as a left one does (Windows 3.1 takes no notice of it), and
       goes on to what is under it. */
    if (code != MSGF_MENU)
        ;
    else if (m->message >= WM_MOUSEFIRST && m->message <= WM_MOUSELAST) {
        inMenu = SkinMenuPoint(m);
        if (m->message == WM_RBUTTONDOWN && HookSkinning() && !OnMenuBar(m) &&
            ((under = WindowFromPoint(m->pt)) == NULL || !SkinIsMenu(under))) {
            RightClickThrough(m, FALSE);
            return 1;
        }
        if (m->message == WM_RBUTTONUP && rightTo) {     /* (let go of before the menu ended) */
            RightClickThrough(m, TRUE);
            return 1;
        }
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

/* The Windows key: let go alone, the Start menu; with a key, its command
   (Win+E, Win+R...); neither reaches the program. TRUE: taken. */
static BOOL WinKey(WPARAM wp, BOOL up)
{
    if (wp == VK_LWIN || wp == VK_RWIN) {
        if (!up && !winDown) {
            winDown = TRUE;
            winUsed = FALSE;
        } else if (up) {
            if (winDown && !winUsed)
                Post(SBM_WINKEY, 0, 0L);
            winDown = FALSE;
        }
        return TRUE;
    }
    if (!winDown || wp == VK_SHIFT)
        return FALSE;
    if (!up && !winUsed) {
        winUsed = TRUE;
        Post(SBM_WINKEY, wp, GetAsyncKeyState(VK_SHIFT) < 0);
    }
    return TRUE;
}

/* A shortcut's key, in any program but a shortcut key field (it takes any key). */
static BOOL HotKey(WPARAM wp, BOOL up, BOOL alt)
{
    WORD key;
    int i;

    if (!nHotkeys || up || wp == VK_SHIFT || wp == VK_CONTROL || wp == VK_MENU || GetProp(GetFocus(), "SbKeyField"))
        return FALSE;
    key = (WORD)wp | ((GetAsyncKeyState(VK_SHIFT) < 0 ? SK_SHIFT : 0) |
                      (GetAsyncKeyState(VK_CONTROL) < 0 ? SK_CONTROL : 0) | (alt ? SK_ALT : 0)) << 8;
    for (i = 0; i < nHotkeys; i++)
        if (hotkeys[i] == key) {
            Post(SBM_HOTKEY, i, 0L);
            return TRUE;
        }
    return FALSE;
}

/* Alt+Tab (Shift: backwards) goes to the taskbar instead of to Windows 3.1's
   own switcher; letting go of Alt ends it, Esc cancels. Alt's release is
   eaten too (else the program would open its menu) and taken out of the
   key state. */
static BOOL Switch(WPARAM wp, BOOL up, BOOL alt)
{
    BYTE keys[256];

    if (wp == VK_TAB && alt) {
        if (!up) {
            Post(SBM_SWITCH, !switching ? SWITCH_START : GetKeyState(VK_SHIFT) < 0 ? SWITCH_PREV : SWITCH_NEXT, 0L);
            switching = TRUE;
        }
        return TRUE;
    }
    if (switching && wp == VK_ESCAPE) {
        if (!up) {
            switching = FALSE;
            Post(SBM_SWITCH, SWITCH_CANCEL, 0L);
        }
        return TRUE;
    }
    if (!switching || wp != VK_MENU || !up)
        return FALSE;
    switching = FALSE;
    Post(SBM_SWITCH, SWITCH_DONE, 0L);
    GetKeyboardState(keys);
    keys[VK_MENU] &= ~0x80;
    SetKeyboardState(keys);
    return TRUE;
}

/* Keys only peeked at (HC_NOREM) pass: they come again. */
LRESULT CALLBACK __export HookKeyboardProc(int code, WPARAM wp, LPARAM lp)
{
    BOOL up = (lp & 0x80000000L) != 0, alt = (lp & 0x20000000L) != 0;

    if (code == HC_ACTION && (WinKey(wp, up) || HotKey(wp, up, alt) || Switch(wp, up, alt)))
        return 1;
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
    return GetCurrentTask() == shellTask ? skinShell : skinAll;
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
    CursorsRestore();
}

/* The taskbar's buttons, after each repaint: windows zoom to and from them. */
void SNSYSTEM_API HookTaskButtons(const HWND FAR *w, const RECT FAR *r, int n)
{
    nButtons = min(n, MAX_BUTTONS);
    _fmemcpy(buttonWnd, w, nButtons * sizeof *w);
    _fmemcpy(buttonRect, r, nButtons * sizeof *r);
}

static BOOL Button(HWND w, RECT *r)
{
    int i;

    for (i = 0; i < nButtons && buttonWnd[i] != w; i++)
        ;
    if (i == nButtons)
        return FALSE;
    *r = buttonRect[i];
    return TRUE;
}

/* The zoom of a top-level window about to be minimized (to its button),
   restored (from it, or from the work area) or maximized, by cmd (SC_*). */
void SNSYSTEM_API HookZoom(HWND w, UINT cmd)
{
    WINDOWPLACEMENT p;
    RECT now, b, area;

    if (!DrawAnimations() || !IsWindowVisible(w) || (GetWindowLong(w, GWL_STYLE) & WS_CHILD))
        return;
    p.length = sizeof p;
    GetWindowPlacement(w, &p);
    GetWindowRect(w, &now);
    HookArea(&area);
    if (cmd == SC_MINIMIZE && !IsIconic(w) && Button(w, &b))
        DrawZoom(&now, &b);
    else if (cmd == SC_RESTORE && IsIconic(w) && Button(w, &b))
        DrawZoom(&b, (p.flags & WPF_RESTORETOMAXIMIZED) ? &area : &p.rcNormalPosition);
    else if (cmd == SC_RESTORE && IsZoomed(w))
        DrawZoom(&now, &p.rcNormalPosition);
    else if (cmd == SC_MAXIMIZE && !IsZoomed(w))
        DrawZoom(IsIconic(w) && Button(w, &b) ? &b : &now, &area);
}

/* Whether w (of the shell, or another program) is to have the Windows 98 style. */
BOOL SNSYSTEM_API SkinWanted(HWND w)
{
    return !shellTask || GetWindowTask(w) == shellTask ? skinShell : skinAll;
}

/* The shell's windows (shell) or the other programs' get the style or
   lose it at once: those open now (their own look back). */
static void Reskin(BOOL shell, BOOL on)
{
    HWND w;

    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT))
        if ((GetWindowTask(w) == shellTask) == shell && IsWindowVisible(w)) {
            if (on)
                SkinWindow(w);
            else
                SkinWindowOff(w);
        }
}

/* The Windows 98 style for other programs' windows on or off. */
void SNSYSTEM_API HookSkinPrograms(BOOL on)
{
    if (on != skinAll)
        Reskin(FALSE, skinAll = on);
}

/* ... and for the shell's own (its menus too): off, Windows 3.1's look everywhere. */
void SNSYSTEM_API HookSkinShell(BOOL on)
{
    if (on != skinShell)
        Reskin(TRUE, skinShell = on);
}
