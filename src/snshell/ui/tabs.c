/*
 * SbTabs: the tabs and the raised panel of a Windows 98 property sheet.
 * Only the tabs and the panel's edge are drawn: the inside belongs to the
 * dialog (its separators, a pie) and the page controls, siblings laid over
 * the panel (WS_CLIPSIBLINGS keeps this control off them). Arrow keys or a
 * click choose a tab; the parent gets WM_COMMAND with TCN_SELECT.
 */
#include "internal.h"

#define MAX_TABS    6

typedef struct {
    char labels[MAX_TABS][24];
    int  count, sel;
    BYTE off[25];               /* page controls (id - IDC_PAGE1) disabled for their hidden page */
} TABS;

#define Tabs(w)     ((TABS *)LocalHeapPointer(GetWindowWord(w, 0)))

/* Rectangle of tab i (the chosen one stands 2 pixels out on three sides). */
static void TabRect(HWND w, int i, RECT *r)
{
    TABS *t = Tabs(w);
    HDC dc = GetDC(w);
    HFONT old = SelectObject(dc, DrawFont(FALSE));
    int k, x = 2, width = 0;

    for (k = 0; k <= i; k++) {
        x += width;
        width = max(40, LOWORD(GetTextExtent(dc, t->labels[k], lstrlen(t->labels[k]))) + 16);
    }
    SelectObject(dc, old);
    ReleaseDC(w, dc);
    SetRect(r, x, 2, x + width, TABS_H);
    if (i == t->sel)
        InflateRect(r, 2, 2);
}

static void Paint(HWND w)
{
    TABS *t = Tabs(w);
    PAINTSTRUCT ps;
    RECT c, r, e;
    COLORREF light = GetSysColor(COLOR_BTNHIGHLIGHT), shadow = GetSysColor(COLOR_BTNSHADOW);
    HFONT old;
    int i;

    BeginPaint(w, &ps);
    GetClientRect(w, &c);
    SetRect(&r, 0, 0, c.right, TABS_H);
    DrawSolidRect(ps.hdc, &r, GetSysColor(COLOR_BTNFACE));
    SetRect(&r, 0, TABS_H, c.right, c.bottom);   /* the panel's edge only */
    DrawBevel(ps.hdc, &r, FALSE);
    old = SelectObject(ps.hdc, DrawFont(FALSE));
    SetBkMode(ps.hdc, TRANSPARENT);
    SetTextColor(ps.hdc, GetSysColor(COLOR_BTNTEXT));
    for (i = 0; i < t->count; i++) {
        TabRect(w, i, &r);
        SetRect(&e, r.left, r.top + 2, r.left + 1, r.bottom);           /* left edge */
        DrawSolidRect(ps.hdc, &e, light);
        SetRect(&e, r.left + 1, r.top + 1, r.left + 2, r.top + 2);      /* rounded corner */
        DrawSolidRect(ps.hdc, &e, light);
        SetRect(&e, r.left + 2, r.top, r.right - 2, r.top + 1);         /* top */
        DrawSolidRect(ps.hdc, &e, light);
        SetRect(&e, r.right - 2, r.top + 1, r.right - 1, r.bottom);     /* right: shadow, then black */
        DrawSolidRect(ps.hdc, &e, shadow);
        SetRect(&e, r.right - 1, r.top + 2, r.right, r.bottom);
        DrawSolidRect(ps.hdc, &e, RGB(0, 0, 0));
        if (i == t->sel) {                      /* open into the panel */
            SetRect(&e, r.left + 1, TABS_H, r.right - 2, TABS_H + 1);
            DrawSolidRect(ps.hdc, &e, GetSysColor(COLOR_BTNFACE));
        }
        e = r;
        DrawText(ps.hdc, t->labels[i], -1, &e, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        if (i == t->sel && GetFocus() == w) {
            DrawText(ps.hdc, t->labels[i], -1, &e, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX);
            OffsetRect(&e, (r.right - r.left - (e.right - e.left)) / 2, (r.bottom - r.top - (e.bottom - e.top)) / 2);
            InflateRect(&e, 2, 0);
            DrawFocusRect(ps.hdc, &e);
        }
    }
    SelectObject(ps.hdc, old);
    EndPaint(w, &ps);
    SkinEdges(GetParent(w));            /* the edges of lists on the page, painted over */
}

static void Choose(HWND w, int i)
{
    TABS *t = Tabs(w);
    if (i < 0 || i >= t->count || i == t->sel)
        return;
    t->sel = i;
    InvalidateRect(w, NULL, FALSE);
    SendMessage(GetParent(w), WM_COMMAND, GetWindowWord(w, GWW_ID), MAKELPARAM(w, TCN_SELECT));
}

void TabsAdd(HWND tabs, LPCSTR label)
{
    TABS *t = Tabs(tabs);
    if (t->count < MAX_TABS)
        lstrcpyn(t->labels[t->count++], label, sizeof t->labels[0]);
    InvalidateRect(tabs, NULL, FALSE);
}

/* A page's controls have ids from IDC_PAGE1 + 50 * page, 50 of them: those
   of the page shown are shown (unless wanted says no), the others hidden. */
/* Hidden pages' controls are disabled too: Windows 3.1 gives a mnemonic
   (Alt+C) to a hidden control with it before the page's own. Shown again,
   only those it disabled come back. */
void TabsShowPage(HWND dlg, int page, BOOL (*wanted)(HWND dlg, int id))
{
    HWND c, tabs = GetDlgItem(dlg, IDC_TABS);
    TABS *t = tabs ? Tabs(tabs) : NULL;
    int id, bit, first = IDC_PAGE1 + 50 * page;
    BOOL show;

    for (c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        id = GetDlgCtrlID(c);
        if (id < IDC_PAGE1 || id >= IDC_PAGE1 + 50 * 4)
            continue;
        show = id >= first && id < first + 50 && (!wanted || wanted(dlg, id));
        ShowWindow(c, show ? SW_SHOW : SW_HIDE);
        if (!t)
            continue;
        bit = 1 << ((id - IDC_PAGE1) & 7);
        if (!show && IsWindowEnabled(c)) {
            EnableWindow(c, FALSE);
            t->off[(id - IDC_PAGE1) >> 3] |= bit;
        } else if (show && (t->off[(id - IDC_PAGE1) >> 3] & bit)) {
            EnableWindow(c, TRUE);
            t->off[(id - IDC_PAGE1) >> 3] &= ~bit;
        }
    }
    InvalidateRect(dlg, NULL, TRUE);            /* what the dialog paints on the page */
}

/* The tab shown first (no TCN_SELECT). */
void TabsSelect(HWND tabs, int i)
{
    Tabs(tabs)->sel = i;
    InvalidateRect(tabs, NULL, FALSE);
}

int TabsSelected(HWND tabs)
{
    return Tabs(tabs)->sel;
}

LRESULT CALLBACK __export TabsProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    TABS *t = Tabs(w);
    RECT r;
    POINT p;
    int i;

    switch (msg) {
    case WM_CREATE:
        t = LocalHeapAlloc(sizeof(TABS));
        if (!t)
            return -1;
        SetWindowWord(w, 0, LocalHeapOffset(t));
        return 0;
    case WM_DESTROY:
        LocalHeapFree(t);
        return 0;
    case WM_PAINT:
        Paint(w);
        return 0;
    case WM_ERASEBKGND:                 /* the dialog paints the inside */
        return 1;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(w, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        p.x = (int)LOWORD(lp);
        p.y = (int)HIWORD(lp);
        for (i = 0; i < t->count; i++) {
            TabRect(w, i, &r);
            if (PtInRect(&r, p)) {
                SetFocus(w);
                Choose(w, i);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_LEFT || wp == VK_RIGHT)
            Choose(w, t->sel + (wp == VK_RIGHT ? 1 : -1));
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    case WM_NCHITTEST:                  /* the page's controls lie under the panel */
        p.x = (int)LOWORD(lp);
        p.y = (int)HIWORD(lp);
        ScreenToClient(w, &p);
        if (p.y >= TABS_H)
            return HTTRANSPARENT;
        break;
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL TabsRegister(void)
{
    WNDCLASS wc;

    _fmemset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = TabsProc;
    wc.cbWndExtra = sizeof(WORD);
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = TABS_CLASS;
    return RegisterClass(&wc);
}

/* A modeless sheet's OK, Apply and Cancel; FALSE for any other control. */
BOOL SheetButton(HWND dlg, UINT id, void (*apply)(HWND dlg))
{
    if (id == IDOK || id == IDC_APPLY)
        apply(dlg);
    if (id == IDOK || id == IDCANCEL)
        DestroyWindow(dlg);
    return id == IDOK || id == IDC_APPLY || id == IDCANCEL;
}
