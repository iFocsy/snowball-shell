/*
 * Windows 98's flat toolbars: buttons raised under the mouse and sunk when
 * pressed or checked; images from strips (a cold strip, grey in Explorer,
 * until the mouse comes; a greyed one for buttons that do not apply);
 * labels under the images if wanted; etched separators; buttons with a
 * drop-down arrow of their own. The parent gets WM_COMMAND with the
 * button's command, HIWORD(lParam) TBN_DROPDOWN for the arrow: on the
 * button's release, so that its menu stays (Windows 3.1 closes one opened
 * while the button is down at the release).
 */
#include "internal.h"

#define MAX_BUTTONS 16
#define SEP_W       8
#define ARROW_W     12
#define TIMER_HOT   1                   /* notices the mouse leaving */

typedef struct {
    UINT cmd;
    BYTE image, style, off, checked;
    char label[16];
} BUTTON;

typedef struct {
    int    strip, cold, grey;           /* bitmap ids; cold, grey 0: the strip itself */
    int    size;                        /* of an image */
    BOOL   labels;
    int    count, over, down;           /* the button under the mouse, pressed; -1 none */
    BOOL   overArrow, downArrow;
    int    w, h;                        /* a button's size: measured when buttons, labels or images change */
    BUTTON b[MAX_BUTTONS];
} TOOLBAR;

#define Bar(w)  ((TOOLBAR *)LocalHeapPointer(GetWindowWord(w, 0)))

/* Every button is as wide as the widest label needs. */
static void Measure(TOOLBAR *t)
{
    HDC dc = GetDC(NULL);
    HFONT old = SelectObject(dc, DrawFont(FALSE));
    TEXTMETRIC tm;
    int i;

    GetTextMetrics(dc, &tm);
    t->w = t->size + 7;
    for (i = 0; t->labels && i < t->count; i++)
        t->w = max(t->w, (int)LOWORD(GetTextExtent(dc, t->b[i].label, lstrlen(t->b[i].label))) + 10);
    t->h = t->size + 6 + (t->labels ? tm.tmHeight + 2 : 0);
    SelectObject(dc, old);
    ReleaseDC(NULL, dc);
}

/* Button i's rectangle; arrow: the part with the drop-down arrow. */
static void Place(TOOLBAR *t, int i, RECT *r, RECT *arrow)
{
    int k, x = 0, w = t->w;

    for (k = 0; k < i; k++)
        x += t->b[k].style & TBS_SEPARATOR ? SEP_W : w + (t->b[k].style & TBS_DROPDOWN ? ARROW_W : 0);
    SetRect(r, x, 0, x + (t->b[i].style & TBS_SEPARATOR ? SEP_W : w), t->h);
    SetRect(arrow, r->right, r->top, t->b[i].style & TBS_DROPDOWN ? r->right + ARROW_W : r->right, r->bottom);
}

static int Hit(TOOLBAR *t, int x, int y, BOOL *onArrow)
{
    RECT r, a;
    POINT p;
    int i;

    p.x = x;
    p.y = y;
    for (i = 0; i < t->count; i++) {
        Place(t, i, &r, &a);
        if (t->b[i].style & TBS_SEPARATOR)
            continue;
        if (PtInRect(&r, p) || PtInRect(&a, p)) {
            *onArrow = PtInRect(&a, p);
            return i;
        }
    }
    return -1;
}

static int Find(TOOLBAR *t, UINT cmd)
{
    int i;
    for (i = 0; i < t->count; i++)
        if (t->b[i].cmd == cmd && !(t->b[i].style & TBS_SEPARATOR))
            return i;
    return -1;
}

static void Redraw(HWND w, int i)
{
    TOOLBAR *t = Bar(w);
    RECT r, a;

    if (i < 0 || i >= t->count)
        return;
    Place(t, i, &r, &a);
    UnionRect(&r, &r, &a);
    InvalidateRect(w, &r, FALSE);
}

static void Raised(HDC dc, RECT *r, BOOL sunken)
{
    DrawFrame(dc, r, GetSysColor(sunken ? COLOR_BTNSHADOW : COLOR_BTNHIGHLIGHT),
              GetSysColor(sunken ? COLOR_BTNHIGHLIGHT : COLOR_BTNSHADOW));
}

static void Arrow(HDC dc, int x, int y, BOOL off)
{
    RECT r;
    int i;
    for (i = 0; i < 3; i++) {
        SetRect(&r, x + i, y + i, x + 5 - i, y + i + 1);
        DrawSolidRect(dc, &r, GetSysColor(off ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
    }
}

static void PaintButton(HDC dc, TOOLBAR *t, int i)
{
    BUTTON *b = &t->b[i];
    RECT r, a, l;
    BOOL hot = t->over == i && !b->off, pressed = t->down == i && hot && !t->downArrow;
    BOOL sunk = pressed || (b->checked && !b->off);
    int dx, strip;

    Place(t, i, &r, &a);
    if (b->style & TBS_SEPARATOR) {
        SetRect(&l, r.left + SEP_W / 2 - 1, r.top + 2, r.left + SEP_W / 2, r.bottom - 2);
        DrawSolidRect(dc, &l, GetSysColor(COLOR_BTNSHADOW));
        OffsetRect(&l, 1, 0);
        DrawSolidRect(dc, &l, GetSysColor(COLOR_BTNHIGHLIGHT));
        return;
    }
    if (sunk)
        Raised(dc, &r, TRUE);
    else if (hot)
        Raised(dc, &r, FALSE);
    if (b->style & TBS_DROPDOWN) {
        if (t->down == i && t->downArrow)
            Raised(dc, &a, TRUE);
        else if (hot)
            Raised(dc, &a, FALSE);
        Arrow(dc, a.left + (ARROW_W - 5) / 2 + (t->down == i && t->downArrow), r.top + 3 + t->size / 2 - 1, b->off);
    }
    dx = sunk ? 1 : 0;
    strip = b->off ? (t->grey ? t->grey : t->strip) : hot || sunk || !t->cold ? t->strip : t->cold;
    PaintStripImage(dc, r.left + (r.right - r.left - t->size) / 2 + dx, r.top + 3 + dx, strip, b->image);
    if (t->labels) {
        SetRect(&l, r.left + 1 + dx, r.top + 4 + t->size + dx, r.right - 1 + dx, r.bottom - 1 + dx);
        if (b->off) {                   /* etched, as Windows greys text */
            OffsetRect(&l, 1, 1);
            SetTextColor(dc, GetSysColor(COLOR_BTNHIGHLIGHT));
            DrawText(dc, b->label, -1, &l, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
            OffsetRect(&l, -1, -1);
            SetTextColor(dc, GetSysColor(COLOR_BTNSHADOW));
        } else {
            SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        }
        DrawText(dc, b->label, -1, &l, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

/* Off the screen, then onto it at once: a button the mouse comes to or
   leaves never shows as bare face first. Only the buttons in the part to
   repaint are drawn. */
static void Paint(HWND w)
{
    TOOLBAR *t = Bar(w);
    PAINTSTRUCT ps;
    OFFSCREEN o;
    RECT r, a, x;
    HFONT old;
    HDC dc;
    int i;

    BeginPaint(w, &ps);
    dc = DrawOffscreenBegin(&o, ps.hdc, &ps.rcPaint);
    DrawSolidRect(dc, &ps.rcPaint, GetSysColor(COLOR_BTNFACE));
    old = SelectObject(dc, DrawFont(FALSE));
    SetBkMode(dc, TRANSPARENT);
    for (i = 0; i < t->count; i++) {
        Place(t, i, &r, &a);
        UnionRect(&r, &r, &a);
        if (IntersectRect(&x, &r, &ps.rcPaint))
            PaintButton(dc, t, i);
    }
    SelectObject(dc, old);
    DrawOffscreenEnd(&o);
    EndPaint(w, &ps);
}

static void Track(HWND w, int over, BOOL onArrow)
{
    TOOLBAR *t = Bar(w);

    if (over == t->over && onArrow == t->overArrow)
        return;
    Redraw(w, t->over);
    t->over = over;
    t->overArrow = onArrow;
    Redraw(w, over);
    if (over >= 0)
        SetTimer(w, TIMER_HOT, 100, NULL);
    else
        KillTimer(w, TIMER_HOT);
}

LRESULT CALLBACK __export ToolbarProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    TOOLBAR *t = Bar(w);
    BOOL onArrow = FALSE;
    POINT p;
    int i;

    HOOK_DESCRIBE(msg, ToolbarDescribe(w, (LPSTR)lp, wp));

    switch (msg) {
    case WM_CREATE:
        if ((t = LocalHeapAlloc(sizeof(TOOLBAR))) == NULL)
            return -1;
        t->over = t->down = -1;
        t->size = 16;
        Measure(t);
        SetWindowWord(w, 0, LocalHeapOffset(t));
        return 0;
    case WM_DESTROY:
        KillTimer(w, TIMER_HOT);
        LocalHeapFree(t);
        return 0;
    case WM_PAINT:
        Paint(w);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        i = Hit(t, (int)LOWORD(lp), (int)HIWORD(lp), &onArrow);
        Track(w, i, onArrow);
        return 0;
    case WM_TIMER:                      /* gone without a message: Windows 3.1 has no WM_MOUSELEAVE */
        GetCursorPos(&p);
        if (WindowFromPoint(p) != w && t->down < 0)
            Track(w, -1, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        i = Hit(t, (int)LOWORD(lp), (int)HIWORD(lp), &onArrow);
        if (i < 0 || t->b[i].off)
            return 0;
        t->down = i;
        t->downArrow = onArrow;
        Redraw(w, i);
        SetCapture(w);
        return 0;
    case WM_LBUTTONUP:
        if (t->down < 0)
            return 0;
        ReleaseCapture();
        i = t->down;
        if (Hit(t, (int)LOWORD(lp), (int)HIWORD(lp), &onArrow) == i && onArrow == t->downArrow) {
            UpdateWindow(w);            /* the arrow stays sunk while its menu shows */
            SendMessage(GetParent(w), WM_COMMAND, t->b[i].cmd, MAKELPARAM(w, onArrow ? TBN_DROPDOWN : 0));
        }
        t->down = -1;
        t->downArrow = FALSE;
        Redraw(w, i);
        if (IsWindow(w)) {
            GetCursorPos(&p);
            ScreenToClient(w, &p);
            i = Hit(t, p.x, p.y, &onArrow);
            Track(w, i, onArrow);
        }
        return 0;
    case WM_CANCELMODE:
        if (t->down >= 0) {
            ReleaseCapture();
            Redraw(w, t->down);
            t->down = -1;
        }
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL ToolbarRegister(void)
{

    return WindowRegister(TOOLBAR_CLASS, ToolbarProc, 0, sizeof(WORD), NULL, NULL);
}

void ToolbarSetImages(HWND tb, int strip, int cold, int grey)
{
    TOOLBAR *t = Bar(tb);
    HBITMAP b = LoadBitmap(g_hArt, MAKEINTRESOURCE(strip));
    BITMAP bm;

    t->strip = strip;
    t->cold = cold;
    t->grey = grey;
    if (b) {
        GetObject(b, sizeof bm, &bm);
        t->size = bm.bmHeight;
        DeleteObject(b);
    }
    Measure(t);
    InvalidateRect(tb, NULL, FALSE);
}

void ToolbarAdd(HWND tb, int image, UINT cmd, LPCSTR label, UINT style)
{
    TOOLBAR *t = Bar(tb);
    BUTTON *b;

    if (t->count == MAX_BUTTONS)
        return;
    b = &t->b[t->count++];
    b->image = (BYTE)image;
    b->cmd = cmd;
    b->style = (BYTE)style;
    lstrcpyn(b->label, label ? label : "", sizeof b->label);
    Measure(t);
    InvalidateRect(tb, NULL, FALSE);
}

void ToolbarEnable(HWND tb, UINT cmd, BOOL on)
{
    TOOLBAR *t = Bar(tb);
    int i = Find(t, cmd);

    if (i >= 0 && t->b[i].off != (BYTE)!on) {
        t->b[i].off = (BYTE)!on;
        Redraw(tb, i);
    }
}

void ToolbarCheck(HWND tb, UINT cmd, BOOL on)
{
    TOOLBAR *t = Bar(tb);
    int i = Find(t, cmd);

    if (i >= 0 && t->b[i].checked != (BYTE)on) {
        t->b[i].checked = (BYTE)on;
        Redraw(tb, i);
    }
}

void ToolbarShowLabels(HWND tb, BOOL on)
{
    Bar(tb)->labels = on;
    Measure(Bar(tb));
    InvalidateRect(tb, NULL, FALSE);
}

int ToolbarHeight(HWND tb)
{
    return Bar(tb)->h;
}

int ToolbarWidth(HWND tb)
{
    TOOLBAR *t = Bar(tb);
    RECT r, a;

    if (!t->count)
        return 0;
    Place(t, t->count - 1, &r, &a);
    return a.right;
}

BOOL ToolbarButtonRect(HWND tb, UINT cmd, RECT *r)
{
    TOOLBAR *t = Bar(tb);
    RECT a;
    int i = Find(t, cmd);

    if (i < 0)
        return FALSE;
    Place(t, i, r, &a);
    UnionRect(r, r, &a);
    return TRUE;
}

#ifdef SNOWBALL_HOOKS
/* The tests' hooks (src\hooks): button i; FALSE past the last. */
BOOL ToolbarButton(HWND w, int i, UINT *cmd, LPSTR label, BOOL *off, BOOL *checked)
{
    TOOLBAR *t = Bar(w);

    if (!t || i >= t->count)
        return FALSE;
    *cmd = t->b[i].cmd;
    lstrcpy(label, t->b[i].label);
    *off = t->b[i].off;
    *checked = t->b[i].checked;
    return TRUE;
}
#endif
