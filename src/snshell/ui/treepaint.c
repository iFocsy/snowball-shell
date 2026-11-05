/* SbTree painted as Windows 98 paints it: dotted lines, +/- boxes, small
   icons, the selection and the drop target. */
#include "internal.h"
#include "treeviewint.h"

/* Dotted lines; dots sit on even x + y, so rows join up seamlessly. */
static void VDots(HDC dc, int x, int y0, int y1)
{
    COLORREF c = GetSysColor(COLOR_BTNSHADOW);
    for (y0 += (x + y0) & 1; y0 <= y1; y0 += 2)
        SetPixel(dc, x, y0, c);
}

static void HDots(HDC dc, int x0, int x1, int y)
{
    COLORREF c = GetSysColor(COLOR_BTNSHADOW);
    for (x0 += (x0 + y) & 1; x0 <= x1; x0 += 2)
        SetPixel(dc, x0, y, c);
}

/* Off the screen, then onto it at once (no white flash under the rows). */
void TreeViewPaint(HWND w)
{
    TREE *t = Tree(w);
    PAINTSTRUCT ps;
    OFFSCREEN o;
    HDC dc;
    RECT r, box, text;
    HFONT old;
    BOOL focused = GetFocus() == w, lit;
    int i, d, x, y, cx, cy;
    LPSTR name;

    BeginPaint(w, &ps);
    dc = DrawOffscreenBegin(&o, ps.hdc, &ps.rcPaint);
    DrawSolidRect(dc, &ps.rcPaint, GetSysColor(COLOR_WINDOW));
    old = SelectObject(dc, DrawFont(FALSE));
    SetBkMode(dc, TRANSPARENT);
    GetClientRect(w, &r);
    for (i = t->top; i < t->count && (y = (i - t->top) * ROW_H) < r.bottom; i++) {
        NODE far *n = t->row + i;
        x = 4 + n->depth * INDENT;
        cy = y + ROW_H / 2;
        for (d = 1; d < n->depth; d++)          /* lines of the ancestors that go on */
            if (n->lines & (1 << d))
                VDots(dc, 4 + d * INDENT - INDENT / 2, y, y + ROW_H - 1);
        if (n->depth) {                         /* the line into this row */
            cx = x - INDENT / 2;
            VDots(dc, cx, y, (n->flags & TF_LAST) ? cy : y + ROW_H - 1);
            HDots(dc, cx, x - 2, cy);
            if (n->flags & TF_KIDS) {           /* the +/- box */
                SetRect(&box, cx - 4, cy - 4, cx + 5, cy + 5);
                DrawSolidRect(dc, &box, GetSysColor(COLOR_WINDOW));
                DrawFrame(dc, &box, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNSHADOW));
                SetRect(&box, cx - 2, cy, cx + 3, cy + 1);
                DrawSolidRect(dc, &box, GetSysColor(COLOR_WINDOWTEXT));
                if (!(n->flags & TF_EXPANDED)) {
                    SetRect(&box, cx, cy - 2, cx + 1, cy + 3);
                    DrawSolidRect(dc, &box, GetSysColor(COLOR_WINDOWTEXT));
                }
            }
        }
        DrawIconImage(dc, x, y + 1, n->icon, TRUE);
        name = t->pool + n->text;
        SetRect(&text, x + 19, y + 1, x + 23 + LOWORD(GetTextExtent(dc, name, lstrlen(name))), y + ROW_H - 1);
        lit = (i == t->sel && focused) || i == t->drop;
        if (lit || i == t->sel)
            DrawSolidRect(dc, &text, GetSysColor(lit ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
        SetTextColor(dc, GetSysColor(lit ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        InflateRect(&text, -2, 0);
        DrawText(dc, name, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        /* (no focus rectangle: over the selection its XOR is a yellow line) */
    }
    SelectObject(dc, old);
    DrawOffscreenEnd(&o);
    EndPaint(w, &ps);
}
