/* Drawing primitives shared by the shell (SNSHELL.DLL) and the skin. */
#include "snsystem.h"

#define MAX_IMAGES  112

static HFONT fonts[2];

/* Images of icons at 16 or 32 pixels: colour (black where transparent) and
   mask (1 where transparent). Fixed ones came from the shell's resources;
   the others are shrunk copies of programs' icons, which a program may
   free: its handle can come back as another icon's, so the copy remembers
   what the icon held. */
typedef struct {
    HICON   icon;
    BYTE    size, fixed;
    WORD    print;                      /* Fingerprint of a shrunk copy's icon */
    HBITMAP color, mask;
} IMAGE;
static IMAGE images[MAX_IMAGES];
static int nImages;

/* MS Sans Serif 8, the Windows 98 UI font; created once, never bold by accident. */
HFONT SNSYSTEM_API DrawFont(BOOL bold)
{
    if (!fonts[bold]) {
        HDC dc = GetDC(NULL);
        int h = -MulDiv(8, GetDeviceCaps(dc, LOGPIXELSY), 72);
        ReleaseDC(NULL, dc);
        fonts[bold] = CreateFont(h, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0,
                                 ANSI_CHARSET, 0, 0, 0, VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
    }
    return fonts[bold];
}

void SNSYSTEM_API DrawSolidRect(HDC dc, const RECT FAR *r, COLORREF c)
{
    COLORREF old = SetBkColor(dc, c);
    ExtTextOut(dc, 0, 0, ETO_OPAQUE, r, NULL, 0, NULL);
    SetBkColor(dc, old);
}

/* At most 32 solid bands. Channels are rounded to multiples of 8 so greys stay
   neutral in 15/16-bit colour (5 or 6 bits per channel). */
void SNSYSTEM_API DrawGradient(HDC dc, const RECT FAR *r, COLORREF from, COLORREF to, BOOL vertical)
{
    int len = vertical ? r->bottom - r->top : r->right - r->left;
    int bands = len < 32 ? len : 32, last = bands > 1 ? bands - 1 : 1, i;
    RECT b = *r;

    for (i = 0; i < bands; i++) {
        int a = MulDiv(len, i, bands), e = MulDiv(len, i + 1, bands);
        COLORREF c = RGB((GetRValue(from) + MulDiv(GetRValue(to) - GetRValue(from), i, last)) & 0xF8,
                         (GetGValue(from) + MulDiv(GetGValue(to) - GetGValue(from), i, last)) & 0xF8,
                         (GetBValue(from) + MulDiv(GetBValue(to) - GetBValue(from), i, last)) & 0xF8);
        if (vertical) { b.top = r->top + a; b.bottom = r->top + e; }
        else          { b.left = r->left + a; b.right = r->left + e; }
        DrawSolidRect(dc, &b, c);
    }
}

/* One-pixel frame: top/left in tl, bottom/right in br. */
void SNSYSTEM_API DrawFrame(HDC dc, const RECT FAR *r, COLORREF tl, COLORREF br)
{
    RECT e;
    SetRect(&e, r->left, r->top, r->right - 1, r->top + 1);          DrawSolidRect(dc, &e, tl);
    SetRect(&e, r->left, r->top, r->left + 1, r->bottom - 1);        DrawSolidRect(dc, &e, tl);
    SetRect(&e, r->left, r->bottom - 1, r->right, r->bottom);        DrawSolidRect(dc, &e, br);
    SetRect(&e, r->right - 1, r->top, r->right, r->bottom);          DrawSolidRect(dc, &e, br);
}

/* Windows 98 two-pixel edge: raised (buttons, frames) or sunken (fields). */
void SNSYSTEM_API DrawBevel(HDC dc, const RECT FAR *r, BOOL sunken)
{
    COLORREF light = GetSysColor(COLOR_BTNHIGHLIGHT), face = GetSysColor(COLOR_BTNFACE);
    COLORREF shadow = GetSysColor(COLOR_BTNSHADOW), dark = RGB(0, 0, 0);
    RECT in = *r;

    InflateRect(&in, -1, -1);
    if (sunken) {
        DrawFrame(dc, r, shadow, light);
        DrawFrame(dc, &in, dark, face);
    } else {
        DrawFrame(dc, r, light, dark);
        DrawFrame(dc, &in, face, shadow);
    }
}

/* Group box: an etched frame from half the font's height down; the caption's
   place (gap pixels wide, from the left + 8) is cleared and returned. */
void SNSYSTEM_API DrawGroupFrame(HDC dc, const RECT FAR *r, int gap, RECT FAR *caption)
{
    COLORREF shadow = GetSysColor(COLOR_BTNSHADOW), light = GetSysColor(COLOR_BTNHIGHLIGHT);
    TEXTMETRIC tm;
    RECT t;

    GetTextMetrics(dc, &tm);
    SetRect(&t, r->left, r->top + tm.tmHeight / 2, r->right - 1, r->bottom - 1);
    DrawFrame(dc, &t, shadow, shadow);
    OffsetRect(&t, 1, 1);
    DrawFrame(dc, &t, light, light);
    SetRect(caption, r->left + 8, r->top, r->left + 8 + gap, r->top + tm.tmHeight);
    if (gap)
        DrawSolidRect(dc, caption, GetSysColor(COLOR_BTNFACE));
}

/* Single line, vertically centred, cut with "..." to fit (binary search). */
void SNSYSTEM_API DrawTextEllipsis(HDC dc, LPCSTR text, RECT FAR *r, UINT align)
{
    char buf[MAX_PATH + 4];
    int width = r->right - r->left, lo = 0, hi = lstrlen(text), n;

    if (hi >= MAX_PATH)
        hi = MAX_PATH - 1;
    if (LOWORD(GetTextExtent(dc, text, hi)) <= width) {
        lstrcpyn(buf, text, hi + 1);
    } else {
        while (lo < hi) {
            n = (lo + hi + 1) / 2;
            lstrcpyn(buf, text, n + 1);
            lstrcat(buf, "...");
            if (LOWORD(GetTextExtent(dc, buf, n + 3)) <= width) lo = n; else hi = n - 1;
        }
        lstrcpyn(buf, text, lo + 1);
        lstrcat(buf, "...");
    }
    DrawText(dc, buf, -1, r, align | DT_SINGLELINE | DT_VCENTER);
}

HDC SNSYSTEM_API DrawOffscreenBegin(OFFSCREEN FAR *o, HDC dc, const RECT FAR *r)
{
    o->dc = dc;
    o->r = *r;
    o->mem = NULL;
    if (r->right > r->left && r->bottom > r->top && (o->mem = CreateCompatibleDC(dc)) != NULL) {
        if ((o->bmp = CreateCompatibleBitmap(dc, r->right - r->left, r->bottom - r->top)) != NULL) {
            o->old = SelectObject(o->mem, o->bmp);
            SetWindowOrg(o->mem, r->left, r->top);
            return o->mem;
        }
        DeleteDC(o->mem);
        o->mem = NULL;
    }
    return dc;
}

void SNSYSTEM_API DrawOffscreenEnd(OFFSCREEN FAR *o)
{
    if (!o->mem)
        return;
    BitBlt(o->dc, o->r.left, o->r.top, o->r.right - o->r.left, o->r.bottom - o->r.top, o->mem, o->r.left, o->r.top,
           SRCCOPY);
    SelectObject(o->mem, o->old);
    DeleteObject(o->bmp);
    DeleteDC(o->mem);
}

/* Windows 3.1 has no PaintDesktop: the desktop window paints its pattern
   and wallpaper into our DC (for windows placed at the screen origin). */
void SNSYSTEM_API DrawWallpaper(HDC dc)
{
    SendMessage(GetDesktopWindow(), WM_ERASEBKGND, (WPARAM)dc, 0L);
}

/* ---- icons ----
   The shell's own icons have the images Windows 98 drew for each size
   (DrawIconImages). Any other icon Windows draws at 32x32; at 16x16 it gets
   a shrunk copy, made once. */

/* Its size and the first bytes of its header, mask and colours. */
static WORD Fingerprint(HICON icon)
{
    DWORD n = GlobalSize((HGLOBAL)icon);
    BYTE FAR *p = GlobalLock((HGLOBAL)icon);
    WORD sum = (WORD)n;
    int i;

    if (!p)
        return 0;
    for (i = 0; i < 320 && i < n; i++)
        sum = (sum << 1 | sum >> 15) ^ p[i];
    GlobalUnlock((HGLOBAL)icon);
    return sum;
}

static IMAGE *Find(HICON icon, int size)
{
    int i;

    for (i = 0; i < nImages; i++)
        if (images[i].icon == icon && images[i].size == size) {
            if (!images[i].fixed && images[i].print != Fingerprint(icon)) {
                DeleteObject(images[i].color);      /* another icon now: shrunk again */
                DeleteObject(images[i].mask);
                images[i].icon = NULL;
                return NULL;
            }
            return &images[i];
        }
    return NULL;
}

static IMAGE *Keep(HICON icon, int size, HBITMAP color, HBITMAP mask, BOOL fixed)
{
    int i;

    for (i = 0; i < nImages && images[i].icon; i++)     /* a place left by a stale copy, */
        ;
    if (i == MAX_IMAGES) {              /* else, full: the newest shrunk copy makes room */
        for (i = MAX_IMAGES - 1; i >= 0 && images[i].fixed; i--)
            ;
        if (i < 0) {
            DeleteObject(color);
            DeleteObject(mask);
            return NULL;
        }
        DeleteObject(images[i].color);
        DeleteObject(images[i].mask);
    } else if (i == nImages) {          /* else, at the end */
        nImages++;
    }
    images[i].icon = icon;
    images[i].size = (BYTE)size;
    images[i].fixed = (BYTE)fixed;
    images[i].print = fixed ? 0 : Fingerprint(icon);
    images[i].color = color;
    images[i].mask = mask;
    return &images[i];
}

/* An icon resource's DIB (its height counts the colours and the mask) as
   the two bitmaps. */
static BOOL Bitmaps(const BITMAPINFOHEADER FAR *dib, int size, HBITMAP *color, HBITMAP *mask)
{
    BYTE info[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
    BYTE maskBits[32 * 4];              /* top down, rows of whole words */
    int colors, row = (size + 15) / 16 * 2, dibRow = (size + 31) / 32 * 4, y;
    const BYTE FAR *xorBits, *andBits;
    HDC screen;

    /* only a DIB of 1, 4 or 8 bits that its resource holds whole (an icon of
       Windows 2's form is not one: the caller draws it another way) */
    if (dib->biSize != sizeof(BITMAPINFOHEADER) || dib->biPlanes != 1 ||
        (dib->biBitCount != 1 && dib->biBitCount != 4 && dib->biBitCount != 8) || dib->biClrUsed > 256)
        return FALSE;
    colors = dib->biClrUsed ? (int)dib->biClrUsed : 1 << dib->biBitCount;
    xorBits = (const BYTE FAR *)dib + dib->biSize + colors * sizeof(RGBQUAD);
    andBits = xorBits + (size * dib->biBitCount + 31) / 32 * 4 * size;
    if ((DWORD)OFFSETOF(andBits) + (DWORD)dibRow * size > GetSelectorLimit(SELECTOROF(dib)) + 1)
        return FALSE;
    _fmemcpy(info, dib, (size_t)dib->biSize + colors * sizeof(RGBQUAD));
    ((BITMAPINFOHEADER *)info)->biHeight = size;
    for (y = 0; y < size; y++)
        _fmemcpy(maskBits + y * row, andBits + (size - 1 - y) * dibRow, row);
    screen = GetDC(NULL);
    *color = CreateDIBitmap(screen, (BITMAPINFOHEADER *)info, CBM_INIT, xorBits, (BITMAPINFO *)info, DIB_RGB_COLORS);
    ReleaseDC(NULL, screen);
    *mask = CreateBitmap(size, size, 1, 1, maskBits);
    if (*color && *mask)
        return TRUE;
    if (*color)
        DeleteObject(*color);
    if (*mask)
        DeleteObject(*mask);
    return FALSE;
}

/* The image of an icon group that is size pixels wide, with the most colours
   up to depth bits. */
static BOOL ResourceImage(HINSTANCE module, LPCSTR group, int size, int depth, HBITMAP *color, HBITMAP *mask)
{
    HRSRC r = FindResource(module, group, RT_GROUP_ICON);
    HGLOBAL h;
    const BYTE FAR *dir;
    WORD i, id = 0, bits = 0;
    BOOL ok;

    if (!r || (h = LoadResource(module, r)) == NULL)
        return FALSE;
    dir = (const BYTE FAR *)LockResource(h);
    for (i = 0; i < *(const WORD FAR *)(dir + 4); i++) {
        const BYTE FAR *e = dir + 6 + 14 * i;  /* width, height, colours, 0, planes, bits, bytes, id */
        WORD bpp = *(const WORD FAR *)(e + 6);
        if (e[0] == size && bpp <= depth && bpp > bits) {
            bits = bpp;
            id = *(const WORD FAR *)(e + 12);
        }
    }
    UnlockResource(h);
    FreeResource(h);
    if (!id || (r = FindResource(module, MAKEINTRESOURCE(id), RT_ICON)) == NULL ||
        (h = LoadResource(module, r)) == NULL)
        return FALSE;
    ok = Bitmaps((const BITMAPINFOHEADER FAR *)LockResource(h), size, color, mask);
    UnlockResource(h);
    FreeResource(h);
    return ok;
}

BOOL SNSYSTEM_API DrawIconImages(HICON icon, HINSTANCE module, LPCSTR group)
{
    HDC screen = GetDC(NULL);
    int depth = GetDeviceCaps(screen, BITSPIXEL) * GetDeviceCaps(screen, PLANES);
    HBITMAP color, mask;
    BOOL ok = FALSE;

    ReleaseDC(NULL, screen);
    if (!Find(icon, 16) && ResourceImage(module, group, 16, depth, &color, &mask))
        ok = Keep(icon, 16, color, mask, TRUE) != NULL;
    /* Windows 3.1 may take the 16 colour 32x32: ours has 256 if the screen does */
    if (depth >= 8 && !Find(icon, 32) && ResourceImage(module, group, 32, depth, &color, &mask))
        Keep(icon, 32, color, mask, TRUE);
    return ok;
}

/* A 16x16 copy of an icon that only has 32x32: each pixel stands for two by
   two, opaque if two of them are, in their most common colour (the darker
   on a tie), so that outlines and thin lines stay. The icon drawn on black
   and on white tells which pixels are transparent: they differ. */
static IMAGE *Shrink(HICON icon)
{
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP onBlack = CreateCompatibleBitmap(screen, 32, 32), onWhite = CreateCompatibleBitmap(screen, 32, 32);
    HBITMAP color = CreateCompatibleBitmap(screen, 16, 16), mask = NULL, old;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(BITMAPINFOHEADER) + 2 * 32 * 96 + 16 * 48);
    BITMAPINFOHEADER FAR *bi;
    BYTE *black, *white, *out;
    BYTE maskBits[16 * 2];
    int x, y, i, j;

    old = SelectObject(mem, onBlack);
    PatBlt(mem, 0, 0, 32, 32, BLACKNESS);
    DrawIcon(mem, 0, 0, icon);
    SelectObject(mem, onWhite);
    PatBlt(mem, 0, 0, 32, 32, WHITENESS);
    DrawIcon(mem, 0, 0, icon);
    SelectObject(mem, old);
    DeleteDC(mem);
    if (h && (bi = (BITMAPINFOHEADER FAR *)GlobalLock(h)) != NULL) {
        black = (BYTE *)(bi + 1);
        white = black + 32 * 96;
        out = white + 32 * 96;
        bi->biSize = sizeof(BITMAPINFOHEADER);
        bi->biWidth = bi->biHeight = 32;
        bi->biPlanes = 1;
        bi->biBitCount = 24;
        GetDIBits(screen, onBlack, 0, 32, black, (BITMAPINFO FAR *)bi, DIB_RGB_COLORS);
        GetDIBits(screen, onWhite, 0, 32, white, (BITMAPINFO FAR *)bi, DIB_RGB_COLORS);
        _fmemset(maskBits, 0, sizeof maskBits);
        for (y = 0; y < 16; y++)                /* DIB rows go bottom up */
            for (x = 0; x < 16; x++) {
                const BYTE FAR *px[4];
                int n = 0, best = -1, bestCount = 0;
                for (j = 0; j < 2; j++)
                    for (i = 0; i < 2; i++) {
                        int at = (2 * y + j) * 96 + (2 * x + i) * 3;
                        if (!_fmemcmp(black + at, white + at, 3))
                            px[n++] = black + at;
                    }
                for (i = 0; i < n; i++) {
                    int count = 0;
                    for (j = 0; j < n; j++)
                        count += !_fmemcmp(px[i], px[j], 3);
                    if (count > bestCount || (count == bestCount &&
                        px[i][0] + px[i][1] + px[i][2] < px[best][0] + px[best][1] + px[best][2])) {
                        best = i;
                        bestCount = count;
                    }
                }
                if (n >= 2)
                    _fmemcpy(out + y * 48 + x * 3, px[best], 3);
                else                            /* transparent: black, mask bit set */
                    maskBits[(15 - y) * 2 + x / 8] |= (BYTE)(0x80 >> (x % 8));
            }
        bi->biWidth = bi->biHeight = 16;
        SetDIBits(screen, color, 0, 16, out, (BITMAPINFO FAR *)bi, DIB_RGB_COLORS);
        mask = CreateBitmap(16, 16, 1, 1, maskBits);
        GlobalUnlock(h);
    }
    if (h)
        GlobalFree(h);
    DeleteObject(onBlack);
    DeleteObject(onWhite);
    ReleaseDC(NULL, screen);
    if (!mask) {
        DeleteObject(color);
        return NULL;
    }
    return Keep(icon, 16, color, mask, FALSE);
}

void SNSYSTEM_API DrawIconImage(HDC dc, int x, int y, HICON icon, BOOL small)
{
    IMAGE *im = Find(icon, small ? 16 : 32);
    static HDC mem;                     /* one for every icon drawn: making one each time is slow */
    COLORREF bk, fg;
    HBITMAP old;

    if (!im && small && (im = Shrink(icon)) == NULL)
        return;                         /* no memory: better nothing than a 32x32 */
    bk = SetBkColor(dc, RGB(255, 255, 255));   /* the mono masks turn into colours through these */
    fg = SetTextColor(dc, RGB(0, 0, 0));
    if (!im) {
        DrawIcon(dc, x, y, icon);
    } else {
        if (!mem && (mem = CreateCompatibleDC(NULL)) == NULL) {
            DrawIcon(dc, x, y, icon);
        } else {
            old = SelectObject(mem, im->mask);
            BitBlt(dc, x, y, im->size, im->size, mem, 0, 0, SRCAND);
            SelectObject(mem, im->color);
            BitBlt(dc, x, y, im->size, im->size, mem, 0, 0, SRCPAINT);
            SelectObject(mem, old);
        }
    }
    SetBkColor(dc, bk);
    SetTextColor(dc, fg);
}
