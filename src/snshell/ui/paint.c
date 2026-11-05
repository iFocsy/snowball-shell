/* Painting helpers of the shell: images of bitmap strips, column headers,
   status bars. The primitives shared with the skin are in SNSYSTEM.DLL
   (draw.c). */
#include "internal.h"

#define MAX_BITMAPS 8

/* Bitmaps drawn with magenta transparent: each and its mask (magenta = 1)
   are made once. */
static struct { int id, w, h; HBITMAP bmp, mask; } bitmaps[MAX_BITMAPS];

static int Load(int id)
{
    HDC src, mask;
    HBITMAP old1, old2;
    BITMAP bm;
    int s;

    for (s = 0; s < MAX_BITMAPS && bitmaps[s].id && bitmaps[s].id != id; s++)
        ;
    if (s == MAX_BITMAPS || bitmaps[s].id)
        return s < MAX_BITMAPS ? s : -1;
    if ((bitmaps[s].bmp = LoadBitmap(g_hArt, MAKEINTRESOURCE(id))) == NULL)
        return -1;
    bitmaps[s].id = id;
    GetObject(bitmaps[s].bmp, sizeof bm, &bm);
    bitmaps[s].w = bm.bmWidth;
    bitmaps[s].h = bm.bmHeight;
    bitmaps[s].mask = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, NULL);
    src = CreateCompatibleDC(NULL);
    mask = CreateCompatibleDC(NULL);
    old1 = SelectObject(src, bitmaps[s].bmp);
    old2 = SelectObject(mask, bitmaps[s].mask);
    SetBkColor(src, RGB(255, 0, 255));
    BitBlt(mask, 0, 0, bm.bmWidth, bm.bmHeight, src, 0, 0, SRCCOPY);
    SelectObject(src, old1);
    SelectObject(mask, old2);
    DeleteDC(src);
    DeleteDC(mask);
    return s;
}

/* w x h pixels of bitmap s from sx, at (x, y). */
static void Blit(HDC dc, int x, int y, int s, int sx, int w, int h)
{
    HDC mem = DrawMemoryDC();           /* (the skin's: a toolbar draws a dozen images a paint) */
    HBITMAP old;
    COLORREF bk, fg;

    if (!mem)
        return;
    old = SelectObject(mem, bitmaps[s].bmp);
    bk = SetBkColor(dc, RGB(255, 255, 255));
    fg = SetTextColor(dc, RGB(0, 0, 0));
    BitBlt(dc, x, y, w, h, mem, sx, 0, SRCINVERT);
    SelectObject(mem, bitmaps[s].mask);
    BitBlt(dc, x, y, w, h, mem, sx, 0, SRCAND);
    SelectObject(mem, bitmaps[s].bmp);
    BitBlt(dc, x, y, w, h, mem, sx, 0, SRCINVERT);
    SetBkColor(dc, bk);
    SetTextColor(dc, fg);
    SelectObject(mem, old);
}

/* One image of a strip of square images as tall as the strip. */
void PaintStripImage(HDC dc, int x, int y, int strip, int index)
{
    int s = Load(strip);

    if (s >= 0)
        Blit(dc, x, y, s, index * bitmaps[s].h, bitmaps[s].h, bitmaps[s].h);
}

void PaintBitmap(HDC dc, int x, int y, int id)
{
    int s = Load(id);

    if (s >= 0)
        Blit(dc, x, y, s, 0, bitmaps[s].w, bitmaps[s].h);
}

/* Windows 98's progress bar in r: a sunken line, then blocks of c along
   permille of it. */
void PaintProgressBar(HDC dc, const RECT *r, int permille, COLORREF c)
{
    RECT in = *r, b;
    int step, x, end;

    DrawFrame(dc, r, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
    InflateRect(&in, -1, -1);
    DrawSolidRect(dc, &in, GetSysColor(COLOR_BTNFACE));
    InflateRect(&in, -1, -1);
    end = in.left + (int)((long)(in.right - in.left) * max(0, min(permille, 1000)) / 1000);
    step = (in.bottom - in.top) * 2 / 3 + 2;
    for (x = in.left; x < end; x += step) {
        SetRect(&b, x, in.top, min(x + step - 2, end), in.bottom);
        DrawSolidRect(dc, &b, c);
    }
}

/* A 50% checker in the DC's text and background colours (pressed task
   buttons, the dimmed screen), made once. */
HBRUSH PaintChecker(void)
{
    static WORD bits[8] = { 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA };
    static HBITMAP pattern;
    static HBRUSH brush;

    if (!brush && (pattern = CreateBitmap(8, 8, 1, 1, bits)) != NULL)
        brush = CreatePatternBrush(pattern);    /* (the bitmap stays: Windows 3.1's brush uses it) */
    return brush;
}

/* A column header button of Details lists; pressed while clicked. */
void PaintHeaderCell(HDC dc, const RECT *r, LPCSTR label, UINT align, BOOL pressed)
{
    RECT t = *r;

    DrawSolidRect(dc, r, GetSysColor(COLOR_BTNFACE));
    DrawBevel(dc, r, pressed);
    InflateRect(&t, -6, 0);
    if (pressed)
        OffsetRect(&t, 1, 1);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawTextEllipsis(dc, label, &t, align | DT_NOPREFIX);
}

#define GRIP        13                  /* the size grip's square */

/* Windows 98's size grip in the corner of c: three white and grey
   stripes over a square that covers the last panel's corner. */
static void PaintGrip(HDC dc, const RECT *c)
{
    COLORREF light = GetSysColor(COLOR_BTNHIGHLIGHT), shadow = GetSysColor(COLOR_BTNSHADOW);
    int right = c->right - 1, bottom = c->bottom - 1, s, i, x, y;
    RECT r;

    SetRect(&r, c->right - GRIP, c->bottom - GRIP, c->right, c->bottom);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
    for (s = 0; s < 3; s++)
        for (i = 0, y = bottom - 12 + 4 * s; y < bottom; i++, y++) {
            x = right - 1 - i;
            SetPixel(dc, x, y, light);
            if (x + 1 < right)
                SetPixel(dc, x + 1, y, shadow);
            if (x + 2 < right)
                SetPixel(dc, x + 2, y, shadow);
        }
}

/* The status bar along the bottom of c, as Windows 98's: n panels with a
   1-pixel sunken edge, widths[i] pixels wide (0: what the others leave),
   2 apart from edge to edge; a bitmap (images[i], its 13x13 icon) before a
   panel's text; with grip, the size grip in the corner. The font is the
   caller's. */
void PaintStatusBar(HDC dc, const RECT *c, int n, const int *widths, LPCSTR FAR *texts,
                    const HICON *icons, BOOL grip)
{
    RECT r;
    int i, rest = c->right - c->left - 2 * (n - 1), x = c->left;

    for (i = 0; i < n; i++)
        rest -= widths[i];
    SetRect(&r, c->left, c->bottom - STATUS_H, c->right, c->bottom);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    for (i = 0; i < n; i++) {
        SetRect(&r, x, c->bottom - STATUS_H + 2, x + (widths[i] ? widths[i] : max(rest, 0)), c->bottom);
        x = r.right + 2;
        DrawFrame(dc, &r, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
        InflateRect(&r, -1, -1);
        if (icons && icons[i]) {        /* a small icon, its top row left out (the panel is 15 high) */
            SaveDC(dc);
            IntersectClipRect(dc, r.left, r.top, r.right, r.bottom);
            DrawIconImage(dc, r.left + 1, r.top - 1, icons[i], TRUE);
            RestoreDC(dc, -1);
            r.left += 17;
        }
        r.left += 2;
        if (grip && i == n - 1)         /* the text stops 16 pixels before the corner */
            r.right = min(r.right, c->right - 16);
        DrawText(dc, texts[i], -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    if (grip)
        PaintGrip(dc, c);
}

/* A point of the screen in the size grip of w's status bar (not when w is
   maximized): dragging it sizes the window. */
BOOL StatusBarGrip(HWND w, LPARAM screenPoint)
{
    POINT p;
    RECT c;

    p.x = (int)LOWORD(screenPoint);
    p.y = (int)HIWORD(screenPoint);
    ScreenToClient(w, &p);
    GetClientRect(w, &c);
    return !IsZoomed(w) && p.x >= c.right - GRIP && p.x < c.right && p.y >= c.bottom - GRIP && p.y < c.bottom;
}

/* A list's row as Windows 98 draws one: the 16x16 icon at x, then the
   name; selected, the name highlighted, the focus rectangle around it. */
void PaintListItem(const DRAWITEMSTRUCT FAR *d, int x, HICON icon, LPCSTR name, BOOL bold)
{
    RECT r = d->rcItem, t;
    BOOL sel = (d->itemState & ODS_SELECTED) != 0;
    HFONT old;

    DrawSolidRect(d->hDC, &r, GetSysColor(COLOR_WINDOW));
    if (icon) {
        DrawIconImage(d->hDC, x, r.top + (r.bottom - r.top - 16) / 2, icon, TRUE);
        x += 19;
    }
    old = SelectObject(d->hDC, DrawFont(bold));
    SetRect(&t, x, r.top + 1, min(r.right, x + 4 + (int)LOWORD(GetTextExtent(d->hDC, name, lstrlen(name)))),
            r.bottom - 1);
    if (sel)
        DrawSolidRect(d->hDC, &t, GetSysColor(COLOR_HIGHLIGHT));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    InflateRect(&t, -2, 0);
    DrawTextEllipsis(d->hDC, name, &t, DT_LEFT | DT_NOPREFIX);
    InflateRect(&t, 2, 0);
    if (d->itemState & ODS_FOCUS)
        DrawFocusRect(d->hDC, &t);
    SelectObject(d->hDC, old);
}
