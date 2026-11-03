/*
 * The lists of the Applications and Processes pages: an owner-drawn list
 * box under a column header, both inside one sunken frame, as Windows list
 * views in Details look. The header and frame are painted by the dialog.
 */
#include "internal.h"

#define HEAD_H      18

/* A column's width; the one of width 0 takes what the list box has left. */
static int Width(HWND list, const COLUMN *cols, int n, int i)
{
    RECT c;
    int k, rest;

    if (cols[i].width)
        return cols[i].width;
    GetClientRect(list, &c);
    for (rest = c.right, k = 0; k < n; k++)
        rest -= cols[k].width;
    return max(rest, 40);
}

/* The header, in the dialog's coordinates: above the list box, as wide. */
static void HeadRect(HWND list, RECT *r)
{
    GetWindowRect(list, r);
    ScreenToClient(GetParent(list), (POINT FAR *)&r->left);
    ScreenToClient(GetParent(list), (POINT FAR *)&r->right);
    r->bottom = r->top;
    r->top -= HEAD_H;
}

void ColumnListPlace(HWND list, const RECT *f)
{
    MoveWindow(list, f->left + 2, f->top + 2 + HEAD_H,
               max(0, f->right - f->left - 4), max(0, f->bottom - f->top - 4 - HEAD_H), TRUE);
}

void ColumnListPaint(HDC dc, HWND list, const COLUMN *cols, int n)
{
    RECT h, r;
    int i, x;

    GetWindowRect(list, &r);
    HeadRect(list, &h);
    SetRect(&r, h.left - 2, h.top - 2, h.right + 2, h.bottom + (r.bottom - r.top) + 2);
    DrawBevel(dc, &r, TRUE);
    for (i = 0, x = h.left; i < n && x < h.right; i++) {
        SetRect(&r, x, h.top, min(x + Width(list, cols, n, i), h.right), h.bottom);
        PaintHeaderCell(dc, &r, cols[i].label, cols[i].align, FALSE);
        x = r.right;
    }
    if (x < h.right) {                  /* over the scroll bar */
        SetRect(&r, x, h.top, h.right, h.bottom);
        PaintHeaderCell(dc, &r, "", DT_LEFT, FALSE);
    }
}

int ColumnListHeaderHit(HWND list, const COLUMN *cols, int n, int x, int y)
{
    RECT h;
    int i, left;

    HeadRect(list, &h);
    if (!IsWindowVisible(list) || y < h.top || y >= h.bottom)
        return -1;
    for (i = 0, left = h.left; i < n; left += Width(list, cols, n, i++))
        if (x >= left && x < left + Width(list, cols, n, i))
            return i;
    return -1;
}

void ColumnListDrawRow(const DRAWITEMSTRUCT FAR *d, const COLUMN *cols, int n, LPCSTR *cells,
             HICON icon, BOOL fullRow)
{
    HDC dc = d->hDC;
    RECT r = d->rcItem, c, label;
    BOOL sel = (d->itemState & ODS_SELECTED) != 0;
    HFONT old;
    int i, x, w;

    if ((int)d->itemID < 0)
        return;
    DrawSolidRect(dc, &r, GetSysColor(sel && fullRow ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    old = SelectObject(dc, DrawFont(FALSE));
    SetBkMode(dc, TRANSPARENT);
    label = r;
    for (i = 0, x = r.left; i < n; i++, x += w) {
        w = Width(d->hwndItem, cols, n, i);
        SetRect(&c, x, r.top, x + w, r.bottom);
        if (i == 0) {
            if (icon) {
                DrawIconImage(dc, c.left + 2, r.top + (r.bottom - r.top - 16) / 2, icon, TRUE);
                c.left += 19;
            }
            if (!fullRow) {             /* only the name is selected, as in Windows 98 */
                label = c;
                label.right = min(c.right - 1, c.left + (int)LOWORD(GetTextExtent(dc, cells[0], lstrlen(cells[0]))) + 4);
                if (sel)
                    DrawSolidRect(dc, &label, GetSysColor(COLOR_HIGHLIGHT));
            }
        }
        SetTextColor(dc, GetSysColor(sel && (fullRow || i == 0) ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        InflateRect(&c, i == 0 && icon ? -2 : -6, 0);
        DrawTextEllipsis(dc, cells[i], &c, cols[i].align | DT_NOPREFIX);
    }
    if (d->itemState & ODS_FOCUS)
        DrawFocusRect(dc, &label);
    SelectObject(dc, old);
}

void ColumnListFill(HWND list, int n, DWORD (*id)(int row))
{
    int sel = (int)SendMessage(list, LB_GETCURSEL, 0, 0L), top, i, keep = -1;
    DWORD selId = sel >= 0 ? SendMessage(list, LB_GETITEMDATA, sel, 0L) : 0;
    BOOL same = (int)SendMessage(list, LB_GETCOUNT, 0, 0L) == n;

    for (i = 0; same && i < n; i++)
        same = (DWORD)SendMessage(list, LB_GETITEMDATA, i, 0L) == id(i);
    if (!same) {
        top = (int)SendMessage(list, LB_GETTOPINDEX, 0, 0L);
        SendMessage(list, WM_SETREDRAW, FALSE, 0L);
        SendMessage(list, LB_RESETCONTENT, 0, 0L);
        for (i = 0; i < n; i++) {
            SendMessage(list, LB_ADDSTRING, 0, (LPARAM)id(i));    /* no strings: the data */
            if (id(i) == selId)
                keep = i;
        }
        SendMessage(list, LB_SETTOPINDEX, top, 0L);
        if (n)
            SendMessage(list, LB_SETCURSEL, keep >= 0 ? keep : min(max(sel, 0), n - 1), 0L);
        SendMessage(list, WM_SETREDRAW, TRUE, 0L);
    }
    InvalidateRect(list, NULL, FALSE);
}

DWORD ColumnListSelected(HWND list)
{
    int sel = (int)SendMessage(list, LB_GETCURSEL, 0, 0L);
    return sel < 0 ? 0 : SendMessage(list, LB_GETITEMDATA, sel, 0L);
}
