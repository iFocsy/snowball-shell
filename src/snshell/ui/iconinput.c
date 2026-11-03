/*
 * SbIconView, what the user does to it: the mouse (clicks, the rubber band,
 * drags, sorting and resizing by the Details header), the keyboard, the
 * infotip of the item the mouse rests on, and renaming in place in an edit
 * control over the label.
 */
#include "internal.h"
#include "iconviewint.h"
#include <stdlib.h>

static FARPROC editOld;

static void TipSuppress(HWND w);

/* ---- in-place rename: the editor posts its end to the view, so the view
   is never changed from inside the editor's own messages ---- */

/* Closes the label editor; the focus goes back to the view. */
void IconViewDropEdit(HWND w)
{
    VIEW *v = View(w);
    HWND e = v->edit;

    if (!e)
        return;
    v->edit = NULL;                     /* its WM_KILLFOCUS must not commit again */
    if (GetFocus() == e)
        SetFocus(w);
    DestroyWindow(e);
}

void IconViewEndEdit(HWND w, BOOL accept)
{
    VIEW *v = View(w);
    char text[MAX_PATH];
    static BOOL committing;

    if (!v->edit || committing)
        return;
    GetWindowText(v->edit, text, sizeof text);
    if (accept && v->focus >= 0 && lstrcmp(text, v->pool + v->it[v->focus].name)) {
        committing = TRUE;
        accept = (BOOL)SendMessage(GetParent(w), WM_COMMAND, GetWindowWord(w, GWW_ID),
                                   MAKELPARAM(w, IVN_RENAME));
        committing = FALSE;
        if (!accept && v->edit) {               /* not renamed: let the user fix it */
            SetFocus(v->edit);
            SendMessage(v->edit, EM_SETSEL, 0, MAKELPARAM(0, -1));
            return;
        }
    }
    IconViewDropEdit(w);
}

LRESULT CALLBACK __export IconViewLabelProc(HWND e, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_CHAR:
        if (wp == '\r' || wp == 27) {
            PostMessage(GetParent(e), MSG_ENDEDIT, wp == '\r', 0L);
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        PostMessage(GetParent(e), MSG_ENDEDIT, TRUE, 0L);
        break;
    }
    return CallWindowProc(editOld, e, msg, wp, lp);
}

/* The editor fits the name, as in Windows 98. Large Icons: one line when
   it fits under the icon, else two lines the width of the cell; the other
   modes: one line over the label. */
void IconViewEditLabel(HWND iv, int i)
{
    VIEW *v = View(iv);
    RECT icon, label;
    HDC dc;
    TEXTMETRIC tm;
    LPSTR name;
    int width, lines = 1, x;
    DWORD style = WS_CHILD | WS_BORDER | ES_AUTOHSCROLL;

    if (i < 0 || i >= v->count || v->edit)
        return;
    IconViewSelect(iv, i);
    IconViewPartRects(iv, i, &icon, &label);
    name = v->pool + v->it[i].name;
    dc = GetDC(iv);
    SelectObject(dc, DrawFont(FALSE));
    GetTextMetrics(dc, &tm);
    width = LOWORD(GetTextExtent(dc, name, lstrlen(name))) + 2 * tm.tmAveCharWidth;
    ReleaseDC(iv, dc);
    if (v->mode == IVM_LARGE || v->mode == IVM_THUMBS) {
        style = WS_CHILD | WS_BORDER | ES_MULTILINE | ES_CENTER | ES_AUTOVSCROLL;
        if (width > (v->mode == IVM_THUMBS ? THUMB_W : CELL_W)) {
            width = v->mode == IVM_THUMBS ? THUMB_W : CELL_W;
            lines = 2;
        }
        x = (label.left + label.right) / 2 - width / 2;
    } else {
        width = max(width, 60);
        x = label.left - 1;
    }
    v->edit = CreateWindow("EDIT", name, style, x, label.top, width, lines * tm.tmHeight + 4,
                           iv, NULL, g_hInst, NULL);
    if (!v->edit)
        return;
    SendMessage(v->edit, WM_SETFONT, (WPARAM)DrawFont(FALSE), 0L);
    SendMessage(v->edit, EM_LIMITTEXT, MAX_PATH - 8, 0L);
    SendMessage(v->edit, EM_SETSEL, 0, MAKELPARAM(0, -1));
    editOld = (FARPROC)SetWindowLong(v->edit, GWL_WNDPROC, (LONG)(FARPROC)IconViewLabelProc);
    ShowWindow(v->edit, SW_SHOW);
    SetFocus(v->edit);
}

void IconViewEditText(HWND iv, LPSTR out, int size)
{
    VIEW *v = View(iv);
    if (v->edit)
        GetWindowText(v->edit, out, size);
    else
        *out = '\0';
}

/* ---- the Details header ---- */

/* Column under x; *edge: x is on its right edge (drag to resize). */
static int HeaderHit(VIEW *v, int x, BOOL *edge)
{
    int col, right = -v->hscroll;

    for (col = 0; col < v->ncols; col++) {
        right += v->colW[col];
        *edge = abs(x - right) <= GRIP;
        if (*edge || x < right)
            return col;
    }
    *edge = FALSE;
    return -1;
}

static void HeaderDown(HWND w, int x)
{
    VIEW *v = View(w);
    BOOL edge;
    int col = HeaderHit(v, x, &edge);

    if (col < 0)
        return;
    if (edge) {
        v->sizing = col;
        v->sizeX = x - v->colW[col];
    } else {
        v->headDown = col;
        InvalidateRect(w, NULL, FALSE);
    }
    SetCapture(w);
}

/* A click on a column sorts by it; a second click reverses the order. */
static void HeaderUp(HWND w, int x, int y)
{
    VIEW *v = View(w);
    BOOL edge;
    int col = v->headDown;

    ReleaseCapture();
    v->headDown = v->sizing = -1;
    if (col >= 0 && y < HEAD_H && HeaderHit(v, x, &edge) == col) {
        IconViewSort(w, v->colKey[col], v->sortKey == v->colKey[col] && !v->sortDesc);
        IconViewNotify(w, IVN_SORTED);
    }
    InvalidateRect(w, NULL, FALSE);
}

/* ---- mouse ---- */

static void DrawBand(HWND w)
{
    VIEW *v = View(w);
    HDC dc = GetDC(w);
    RECT r;
    SetRect(&r, min(v->bandRect.left, v->bandRect.right), min(v->bandRect.top, v->bandRect.bottom),
            max(v->bandRect.left, v->bandRect.right), max(v->bandRect.top, v->bandRect.bottom));
    DrawFocusRect(dc, &r);
    ReleaseDC(w, dc);
}

/* Rubber band: items it touches are selected (toggled with Ctrl). */
static void BandTo(HWND w, int x, int y, BOOL ctrl)
{
    VIEW *v = View(w);
    RECT band, icon, label, tmp;
    int i;

    if (v->mode == IVM_DETAILS)
        y = max(y, HEAD_H);
    DrawBand(w);
    v->bandRect.right = x;
    v->bandRect.bottom = y;
    SetRect(&band, min(v->bandRect.left, x), min(v->bandRect.top, y),
            max(v->bandRect.left, x) + 1, max(v->bandRect.top, y) + 1);
    for (i = 0; i < v->count; i++) {
        BOOL in;
        IconViewPartRects(w, i, &icon, &label);
        in = IntersectRect(&tmp, &band, &icon) || IntersectRect(&tmp, &band, &label);
        IconViewSetSel(w, i, ctrl ? ((v->it[i].flags & IF_WAS) != 0) != in : in);
    }
    UpdateWindow(w);
    DrawBand(w);
}

void IconViewButtonDown(HWND w, UINT msg, WPARAM keys, int x, int y)
{
    VIEW *v = View(w);
    int i = IconViewHitTest(w, x, y);
    BOOL ctrl = (keys & MK_CONTROL) != 0, shift = (keys & MK_SHIFT) != 0;

    TipSuppress(w);
    if (msg == WM_RBUTTONDOWN) {        /* its window comes forward, as for a left click */
        HWND top = w;
        while (GetParent(top))
            top = GetParent(top);
        if (GetActiveWindow() != top)
            SetActiveWindow(top);
    }
    SetFocus(w);
    KillTimer(w, TIMER_EDIT);
    v->slowClick = FALSE;
    if (v->mode == IVM_DETAILS && y < HEAD_H) {
        if (msg == WM_LBUTTONDOWN)
            HeaderDown(w, x);
        return;
    }
    if (i >= 0) {
        if (msg == WM_RBUTTONDOWN) {
            if (!Sel(v, i))
                IconViewSelect(w, i);
        } else if (ctrl) {
            IconViewSetSel(w, i, !Sel(v, i));
            v->anchor = i;
            IconViewSetFocusItem(w, i);
            IconViewNotify(w, IVN_SELECT);
        } else if (shift) {
            IconViewSelectRange(w, v->anchor < 0 ? i : v->anchor, i, TRUE);
            IconViewSetFocusItem(w, i);
        } else if (Sel(v, i)) {                 /* keep the selection: it may be dragged */
            v->slowClick = IconViewSelectedCount(w) == 1 && v->focus == i;
            IconViewSetFocusItem(w, i);
        } else {
            IconViewSelect(w, i);
        }
    } else if (!ctrl && !shift) {
        IconViewSelect(w, -1);
    }
    v->pressed = i;
    v->button = msg;
    v->down.x = x;
    v->down.y = y;
    if (i < 0 && msg == WM_LBUTTONDOWN) {       /* rubber band from here */
        for (i = 0; i < v->count; i++)
            v->it[i].flags = (v->it[i].flags & ~IF_WAS) | (Sel(v, i) ? IF_WAS : 0);
        v->band = TRUE;
        SetRect(&v->bandRect, x, y, x, y);
        DrawBand(w);
    }
    SetCapture(w);
}

void IconViewButtonUp(HWND w, UINT msg, WPARAM keys, int x, int y)
{
    VIEW *v = View(w);

    if (v->headDown >= 0 || v->sizing >= 0) {
        HeaderUp(w, x, y);
        return;
    }
    if (!v->button)
        return;
    ReleaseCapture();
    if (v->band) {
        DrawBand(w);
        v->band = FALSE;
        IconViewNotify(w, IVN_SELECT);
    } else if (msg == WM_RBUTTONUP) {
        UpdateWindow(w);                /* what it is for shows before its menu */
        IconViewNotify(w, IVN_CONTEXT);
    } else if (v->pressed >= 0 && !(keys & (MK_CONTROL | MK_SHIFT))) {
        if (v->slowClick)
            SetTimer(w, TIMER_EDIT, GetDoubleClickTime() + 100, NULL);
        else
            IconViewSelect(w, v->pressed);
    }
    v->button = 0;
}

/* The infotip goes with the mouse, a click or a key. */
void IconViewTipOff(HWND w)
{
    VIEW *v = View(w);

    KillTimer(w, TIMER_TIP);
    if (v->tipUp)
        TooltipHide();
    v->tipUp = FALSE;
    v->hover = -1;
}

/* A click or a key: no tip for the item under the mouse until the mouse
   goes to another, as in Windows. */
static void TipSuppress(HWND w)
{
    POINT p;

    IconViewTipOff(w);
    GetCursorPos(&p);
    ScreenToClient(w, &p);
    View(w)->hover = IconViewHitTest(w, p.x, p.y);
}

/* The mouse over item i: its description after a rest, as Windows shows it. */
static void Hover(HWND w, int i)
{
    VIEW *v = View(w);

    if (i == v->hover)
        return;
    IconViewTipOff(w);
    if (i >= 0 && g_viewOptions.infoTips && (v->it[i].kind == IVK_FOLDER || v->it[i].kind == IVK_FILE)) {
        v->hover = i;
        SetTimer(w, TIMER_TIP, TIP_MS, NULL);
    }
}

/* An item's description: a folder's infotip (desktop.ini), a file's
   description (a program's own), type, size and last change. */
static BOOL ItemTip(HWND w, int i, LPSTR tip, int size)
{
    ITEM far *it = View(w)->it + i;
    char path[MAX_PATH], text[80];

    IconViewItemPath(w, i, path);
    if (it->kind == IVK_FOLDER)
        return FolderIniRead(path, "InfoTip", tip, size) > 0;
    tip[0] = '\0';
    if (FormatDescription(path, text, sizeof text))
        wsprintf(tip, "%s\r\n", (LPSTR)text);
    FormatFileType(path, text);
    wsprintf(tip + lstrlen(tip), "Type: %s\r\nSize: ", (LPSTR)text);
    FormatSize(it->size, tip + lstrlen(tip));
    FormatDateTime(it->date, it->time, text);
    wsprintf(tip + lstrlen(tip), "\r\nModified: %s", (LPSTR)text);
    return TRUE;
}

/* The tip shown once the mouse rested, then gone when it leaves (Windows
   3.1 tells no window that the mouse left it). */
static void TipTimer(HWND w)
{
    VIEW *v = View(w);
    char tip[260];
    POINT p, at;

    GetCursorPos(&p);
    at = p;
    ScreenToClient(w, &at);
    if (WindowFromPoint(p) != w || IconViewHitTest(w, at.x, at.y) != v->hover) {
        IconViewTipOff(w);
        return;
    }
    if (v->tipUp)
        return;
    KillTimer(w, TIMER_TIP);
    if (!ItemTip(w, v->hover, tip, sizeof tip))
        return;
    TooltipShow(tip, p.x, p.y + 20);
    v->tipUp = TRUE;
    SetTimer(w, TIMER_TIP, 200, NULL);  /* watches the mouse */
}

void IconViewMouseMove(HWND w, WPARAM keys, int x, int y)
{
    VIEW *v = View(w);

    if (!(keys & (MK_LBUTTON | MK_RBUTTON)) && !v->band && v->sizing < 0) {
        POINT p;                        /* Windows also says so when what is under it changes */
        GetCursorPos(&p);
        if (p.x != v->moved.x || p.y != v->moved.y) {
            v->moved = p;
            Hover(w, IconViewHitTest(w, x, y));
        }
    }
    if (v->sizing >= 0) {
        v->colW[v->sizing] = max(24, x - v->sizeX);
        IconViewLayout(w);
        InvalidateRect(w, NULL, TRUE);
    } else if (v->band) {
        BandTo(w, x, y, (keys & MK_CONTROL) != 0);
    } else if (v->button && v->pressed >= 0 &&
               (abs(x - v->down.x) > DRAG_MIN || abs(y - v->down.y) > DRAG_MIN)) {
        BOOL right = v->button == WM_RBUTTONDOWN;
        ReleaseCapture();
        v->button = 0;
        v->slowClick = FALSE;
        IconViewNotify(w, right ? IVN_RDRAG : IVN_DRAG);
    }
}

/* ---- keyboard ---- */

/* The context menu from the keyboard (Shift+F10, the Menu key): the owner
   asks IconViewMenuPoint where it goes. */
static void KeyMenu(HWND w)
{
    VIEW *v = View(w);

    v->keyMenu = TRUE;
    IconViewNotify(w, IVN_CONTEXT);
    v->keyMenu = FALSE;
}

BOOL IconViewMenuPoint(HWND iv, POINT *p)
{
    VIEW *v = View(iv);
    RECT icon, label;
    int i = v->focus >= 0 && Sel(v, v->focus) ? v->focus : IconViewNextSelected(iv, -1);

    if (!v->keyMenu)
        return FALSE;
    if (i >= 0) {
        IconViewPartRects(iv, i, &icon, &label);
        p->x = (label.left + label.right) / 2;
        p->y = (label.top + label.bottom) / 2;
    } else {
        p->x = p->y = 8;
    }
    ClientToScreen(iv, p);
    return TRUE;
}

void IconViewKey(HWND w, WPARAM key)
{
    VIEW *v = View(w);
    BOOL shift = GetKeyState(VK_SHIFT) < 0, ctrl = GetKeyState(VK_CONTROL) < 0;
    int across, down, i = v->focus < 0 ? 0 : v->focus;

    TipSuppress(w);
    switch (v->mode) {                          /* steps for the arrow keys */
    case IVM_LARGE:   across = IconViewIsDesktop(w) ? v->rows : 1; down = IconViewIsDesktop(w) ? 1 : v->cols; break;
    case IVM_THUMBS:  across = 1; down = v->cols; break;
    case IVM_SMALL:   across = 1; down = v->cols; break;
    case IVM_LIST:    across = v->rows; down = 1; break;
    default:          across = 0; down = 1;
    }
    if (key == VK_APPS || (key == VK_F10 && shift)) {
        KeyMenu(w);
        return;
    }
    switch (key) {
    case VK_LEFT:   i -= across; break;
    case VK_RIGHT:  i += across; break;
    case VK_UP:     i -= down; break;
    case VK_DOWN:   i += down; break;
    case VK_HOME:   i = 0; break;
    case VK_END:    i = v->count - 1; break;
    case VK_SPACE:
        if (ctrl && v->focus >= 0) {
            IconViewSetSel(w, v->focus, !Sel(v, v->focus));
            IconViewNotify(w, IVN_SELECT);
        }
        return;
    case VK_RETURN:
        if (IconViewSelectedCount(w))
            IconViewNotify(w, IVN_OPEN);
        return;
    default:
        SendMessage(GetParent(w), WM_KEYDOWN, key, 0L);
        return;
    }
    if (!v->count)
        return;
    i = max(0, min(v->count - 1, i));
    if (shift)
        IconViewSelectRange(w, v->anchor < 0 ? i : v->anchor, i, !ctrl);
    else if (!ctrl)
        IconViewSelect(w, i);
    IconViewSetFocusItem(w, i);
}

/* Jumps to the next name starting with the typed character. */
void IconViewTypeSelect(HWND w, WPARAM ch)
{
    VIEW *v = View(w);
    BYTE want = (BYTE)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)ch);
    int i, j;

    if (ch < ' ')
        return;
    for (i = 1; i <= v->count; i++) {
        j = (v->focus + i) % v->count;
        if ((BYTE)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)v->pool[v->it[j].name]) == want) {
            IconViewSelect(w, j);
            return;
        }
    }
}

/* ---- the cursor, Windows taking the mouse away, the timers, Alt keys ---- */

/* The sizing cursor on a Details column edge, or while one is dragged; TRUE if set. */
BOOL IconViewCursor(HWND w, UINT hit)
{
    VIEW *v = View(w);
    BOOL edge;
    POINT p;

    if (v->mode == IVM_DETAILS && hit == HTCLIENT) {
        GetCursorPos(&p);
        ScreenToClient(w, &p);
        if ((p.y < HEAD_H && HeaderHit(v, p.x, &edge) >= 0 && edge) || v->sizing >= 0) {
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            return TRUE;
        }
    }
    return FALSE;
}

/* Windows takes the mouse away: the tip, the rubber band and a held button go. */
void IconViewCancel(HWND w)
{
    VIEW *v = View(w);

    IconViewTipOff(w);
    if (v->band)
        DrawBand(w);
    v->band = FALSE;
    v->button = 0;
    v->headDown = v->sizing = -1;
    ReleaseCapture();
}

/* The infotip's timer, or the slow second click's: the focused label is renamed. */
void IconViewTimer(HWND w, WPARAM id)
{
    VIEW *v = View(w);

    if (id == TIMER_TIP) {
        TipTimer(w);
        return;
    }
    KillTimer(w, TIMER_EDIT);
    if (v->focus >= 0 && IconViewSelectedCount(w) == 1 && GetFocus() == w)
        IconViewEditLabel(w, v->focus);
}

/* Shift+F10 and the Alt keys the parent acts on; FALSE: as usual. */
BOOL IconViewSysKey(HWND w, WPARAM key, LPARAM lp)
{
    if (key == VK_F10 && GetKeyState(VK_SHIFT) < 0) {      /* Shift+F10: the context menu */
        KeyMenu(w);
        return TRUE;
    }
    if (key == VK_RETURN) {                     /* Alt+Enter: the parent shows Properties */
        SendMessage(GetParent(w), WM_KEYDOWN, key, lp);
        return TRUE;
    }
    if (key == VK_LEFT || key == VK_RIGHT) {    /* and Alt+arrows go back and forth */
        SendMessage(GetParent(w), WM_SYSKEYDOWN, key, lp);
        return TRUE;
    }
    return FALSE;
}
