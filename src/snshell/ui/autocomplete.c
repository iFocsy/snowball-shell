/*
 * AutoComplete, as Internet Explorer 4 brought it to Windows 98's address
 * bar and Run box: as a path is typed into an edit, the folders and files
 * it can go on to drop down below it. Up and Down walk them (the edit
 * shows each), a click takes one, Esc puts back what was typed, and
 * leaving the edit closes them. The list never takes the focus.
 */
#include "internal.h"

#define AC_MAX      64
#define AC_ROWS     8
#define PROC_LO     "SbAcLo"            /* the edit's own procedure, in two props */
#define PROC_HI     "SbAcHi"

static HWND list, target;               /* the list shown, the edit it serves */
static WNDPROC listProc;
static char typed[MAX_PATH];            /* what was typed, for Esc */
static BOOL escaped;                    /* Esc closed the list: its WM_CHAR is not the dialog's */

static WNDPROC OldProc(HWND edit)
{
    return (WNDPROC)MAKELONG((WORD)GetProp(edit, PROC_LO), (WORD)GetProp(edit, PROC_HI));
}

static void Hide(void)
{
    if (list && IsWindowVisible(list))
        ShowWindow(list, SW_HIDE);
    target = NULL;
}

/* The edit takes an entry: its text, the caret at the end. */
static void Take(int i)
{
    char text[MAX_PATH];

    if (!target || i < 0 || SendMessage(list, LB_GETTEXT, i, (LPARAM)(LPSTR)text) < 0)
        return;
    SetWindowText(target, text);
    SendMessage(target, EM_SETSEL, 0, MAKELPARAM(lstrlen(text), lstrlen(text)));
}

static int ItemAt(int y)
{
    int h = (int)SendMessage(list, LB_GETITEMHEIGHT, 0, 0L);
    int i = (int)SendMessage(list, LB_GETTOPINDEX, 0, 0L) + y / max(h, 1);

    return i < (int)SendMessage(list, LB_GETCOUNT, 0, 0L) ? i : -1;
}

/* Under the mouse an entry is selected; a click gives it to the edit. The
   list is never activated nor focused: the edit keeps typing. */
LRESULT CALLBACK __export AutoCompleteListProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    int i;

    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETFOCUS:
        if (target)
            SetFocus(target);
        return 0;
    case WM_MOUSEMOVE:
        if ((i = ItemAt(HIWORD(lp))) >= 0 && i != (int)SendMessage(w, LB_GETCURSEL, 0, 0L))
            SendMessage(w, LB_SETCURSEL, i, 0L);
        return 0;
    case WM_LBUTTONDOWN:
        if ((i = ItemAt(HIWORD(lp))) >= 0) {
            Take(i);
            Hide();
        }
        return 0;
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        return 0;
    }
    return CallWindowProc((FARPROC)listProc, w, msg, wp, lp);
}

static void MakeList(void)
{
    if (list)
        return;
    list = CreateWindow("LISTBOX", NULL, WS_POPUP | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                        0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
    if (list)
        listProc = (WNDPROC)SetWindowLong(list, GWL_WNDPROC, (LONG)(FARPROC)AutoCompleteListProc);
}

/* The entries for what the edit says now: "C:\WIN" -> the folders and
   files of C:\ that start so; none: the list goes. */
static void Update(HWND edit)
{
    static DOSENTRY e;
    char text[MAX_PATH], spec[MAX_PATH], path[MAX_PATH], cls[16];
    HWND field = edit;
    LPSTR slash;
    RECT r;
    DOSFIND f;
    int n = 0, h;

    GetWindowText(edit, text, sizeof text);
    lstrcpy(typed, text);
    if ((slash = _fstrrchr(text, '\\')) == NULL || text[1] != ':' || lstrlen(text) + 4 >= MAX_PATH) {
        Hide();
        return;
    }
    MakeList();
    if (!list)
        return;
    SendMessage(list, WM_SETREDRAW, FALSE, 0L);
    SendMessage(list, LB_RESETCONTENT, 0, 0L);
    lstrcpy(spec, text);
    lstrcat(spec, "*.*");
    if (DosFindFirst(spec, &f, &e)) {
        do {
            if (e.attr & FA_HIDDEN)
                continue;
            lstrcpyn(path, text, (int)(slash - text) + 2);
            if (lstrlen(path) + lstrlen(e.name) < MAX_PATH) {
                lstrcat(path, e.name);
                SendMessage(list, LB_ADDSTRING, 0, (LPARAM)(LPSTR)path);
                n++;
            }
        } while (n < AC_MAX && DosFindNext(&f, &e));
        DosFindClose(&f);
    }
    SendMessage(list, WM_SETREDRAW, TRUE, 0L);
    if (!n) {
        Hide();
        return;
    }
    SendMessage(list, WM_SETFONT, SendMessage(edit, WM_GETFONT, 0, 0L), 0L);
    h = (int)SendMessage(list, LB_GETITEMHEIGHT, 0, 0L);
    GetClassName(GetParent(edit), cls, sizeof cls);
    if (!lstrcmpi(cls, "ComboBox"))     /* under the whole field */
        field = GetParent(edit);
    GetWindowRect(field, &r);
    target = edit;
    /* placed, then shown: Windows 3.1 neither moves nor sizes what it shows */
    SetWindowPos(list, HWND_TOPMOST, r.left, r.bottom, max(r.right - r.left, 120), min(n, AC_ROWS) * h + 2,
                 SWP_NOACTIVATE);
    ShowWindow(list, SW_SHOWNA);
    InvalidateRect(list, NULL, TRUE);
}

LRESULT CALLBACK __export AutoCompleteEditProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    WNDPROC old = OldProc(w);
    BOOL shown = target == w && list && IsWindowVisible(list);
    LRESULT res;
    int i;

    switch (msg) {
    case WM_KEYDOWN:
        if (shown && (wp == VK_DOWN || wp == VK_UP)) {
            i = (int)SendMessage(list, LB_GETCURSEL, 0, 0L) + (wp == VK_DOWN ? 1 : -1);
            if (i < 0) {                /* above the first: what was typed */
                SendMessage(list, LB_SETCURSEL, (WPARAM)-1, 0L);
                SetWindowText(w, typed);
                SendMessage(w, EM_SETSEL, 0, MAKELPARAM(lstrlen(typed), lstrlen(typed)));
            } else if (i < (int)SendMessage(list, LB_GETCOUNT, 0, 0L)) {
                SendMessage(list, LB_SETCURSEL, i, 0L);
                Take(i);
            }
            return 0;
        }
        if (shown && wp == VK_ESCAPE) {
            SetWindowText(w, typed);
            SendMessage(w, EM_SETSEL, 0, MAKELPARAM(lstrlen(typed), lstrlen(typed)));
            Hide();
            escaped = TRUE;
            return 0;
        }
        if (shown && wp == VK_RETURN)
            Hide();
        break;
    case WM_CHAR:
        if (wp == 27 && (shown || escaped)) {
            escaped = FALSE;
            return 0;
        }
        break;
    case WM_GETDLGCODE:                 /* in a dialog, Esc closes the list, not the dialog */
        if ((shown || escaped) && GetKeyState(VK_ESCAPE) < 0)
            return CallWindowProc((FARPROC)old, w, msg, wp, lp) | DLGC_WANTALLKEYS;
        break;
    case WM_KILLFOCUS:
    case WM_DESTROY:
        Hide();
        break;
    }
    res = CallWindowProc((FARPROC)old, w, msg, wp, lp);
    if (msg == WM_CHAR && (wp >= ' ' || wp == '\b'))
        Update(w);
    if (msg == WM_DESTROY) {
        SetWindowLong(w, GWL_WNDPROC, (LONG)(FARPROC)old);
        RemoveProp(w, PROC_LO);
        RemoveProp(w, PROC_HI);
    }
    return res;
}

void AutoCompleteAttach(HWND edit)
{
    WNDPROC old;

    if (!edit || GetProp(edit, PROC_LO))
        return;
    old = (WNDPROC)SetWindowLong(edit, GWL_WNDPROC, (LONG)(FARPROC)AutoCompleteEditProc);
    SetProp(edit, PROC_LO, (HANDLE)LOWORD((DWORD)old));
    SetProp(edit, PROC_HI, (HANDLE)HIWORD((DWORD)old));
}
