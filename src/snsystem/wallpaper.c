/*
 * The wallpaper when Snowball shows it instead of Windows, which keeps any
 * wallpaper whole in memory. settings\wallpaper.c decides how: a picture within
 * its share of memory stays as a bitmap in the screen's format, as Windows'
 * would; any other is read from its file strip by strip as it is painted, and
 * nothing of it stays in memory. Only plain BMP files are shown this way.
 */
#include "snsystem.h"

#define STRIP_BYTES   32768L
#define OFFSCREEN_MAX 32768L            /* pixels; see DrawOffscreenLimit */

typedef struct {
    BITMAPINFOHEADER head;
    RGBQUAD colors[256];
} DIBHEAD;

static char file[MAX_PATH];             /* "": none, Windows paints the desktop */
static DIBHEAD dib;
static DWORD bitsAt;
static int wide, high;
static BOOL tiled;
static HBITMAP picture;                 /* in memory; else the file is read */

/* Reads the header into dib; FALSE if this is no plain BMP. */
static BOOL ReadHeader(LPCSTR path)
{
    BITMAPFILEHEADER fh;
    HFILE f = _lopen(path, OF_READ | OF_SHARE_DENY_NONE);
    int colors;
    BOOL ok;

    if (f == HFILE_ERROR)
        return FALSE;
    ok = _lread(f, &fh, sizeof fh) == sizeof fh && fh.bfType == 0x4D42 &&
         _lread(f, &dib.head, sizeof dib.head) == sizeof dib.head && dib.head.biSize == sizeof dib.head &&
         dib.head.biCompression == BI_RGB && dib.head.biHeight > 0 && dib.head.biPlanes == 1 &&
         (dib.head.biBitCount == 1 || dib.head.biBitCount == 4 || dib.head.biBitCount == 8 || dib.head.biBitCount == 24);
    if (ok) {
        colors = dib.head.biBitCount == 24 ? 0 : dib.head.biClrUsed ? (int)dib.head.biClrUsed : 1 << dib.head.biBitCount;
        if (colors)
            ok = _lread(f, dib.colors, colors * sizeof(RGBQUAD)) == (UINT)(colors * sizeof(RGBQUAD));
        bitsAt = fh.bfOffBits;
    }
    _lclose(f);
    return ok;
}

BOOL SNSYSTEM_API DrawWallpaperInfo(LPCSTR path, int FAR *w, int FAR *h, int FAR *depth)
{
    if (!ReadHeader(path))
        return FALSE;
    *w = (int)dib.head.biWidth;
    *h = (int)dib.head.biHeight;
    *depth = dib.head.biBitCount;
    return TRUE;
}

/* The part of the picture at (px, py) inside clip, from the file. */
static void PaintFile(HDC dc, int px, int py, const RECT FAR *clip)
{
    long stride = ((long)wide * dib.head.biBitCount + 31) / 32 * 4;
    int top = max(clip->top, py), bottom = min(clip->bottom, py + high);
    int left = max(clip->left, px), right = min(clip->right, px + wide);
    int band = (int)(STRIP_BYTES / stride), y, n;
    HGLOBAL h;
    HFILE f;
    BYTE FAR *bits;

    if (top >= bottom || left >= right || band < 1)
        return;
    band = min(band, bottom - top);
    if ((h = GlobalAlloc(GMEM_MOVEABLE, band * stride)) == NULL)
        return;
    bits = GlobalLock(h);
    if ((f = _lopen(file, OF_READ | OF_SHARE_DENY_NONE)) != HFILE_ERROR) {
        for (y = top; y < bottom; y += n) {
            n = min(band, bottom - y);
            /* the file goes from the picture's last row up */
            _llseek(f, bitsAt + (long)(high - (y - py) - n) * stride, 0);
            _lread(f, bits, (UINT)(n * stride));
            dib.head.biHeight = n;
            SetDIBitsToDevice(dc, left, y, right - left, n, left - px, 0, 0, n, bits,
                              (BITMAPINFO FAR *)&dib, DIB_RGB_COLORS);
        }
        dib.head.biHeight = high;
        _lclose(f);
    }
    GlobalUnlock(h);
    GlobalFree(h);
}

static BOOL LoadPicture(void)
{
    long stride = ((long)wide * dib.head.biBitCount + 31) / 32 * 4;
    int band = (int)(STRIP_BYTES / stride), start, n;
    HDC screen = GetDC(NULL);
    HGLOBAL h = band > 0 ? GlobalAlloc(GMEM_MOVEABLE, band * stride) : NULL;
    HFILE f = _lopen(file, OF_READ | OF_SHARE_DENY_NONE);
    BYTE FAR *bits;

    picture = h && f != HFILE_ERROR ? CreateCompatibleBitmap(screen, wide, high) : NULL;
    if (picture) {
        bits = GlobalLock(h);
        _llseek(f, bitsAt, 0);          /* from the picture's last row up, as the file goes */
        for (start = 0; start < high; start += n) {
            n = min(band, high - start);
            _lread(f, bits, (UINT)(n * stride));
            SetDIBits(screen, picture, start, n, bits, (BITMAPINFO FAR *)&dib, DIB_RGB_COLORS);
        }
        GlobalUnlock(h);
    }
    if (f != HFILE_ERROR)
        _lclose(f);
    if (h)
        GlobalFree(h);
    ReleaseDC(NULL, screen);
    return picture != NULL;
}

/*
 * What the desktop shows from now on: path in memory or from its file; NULL:
 * nothing of Snowball's. FALSE: it cannot be held in memory, or read at all.
 */
BOOL SNSYSTEM_API DrawWallpaperUse(LPCSTR path, BOOL inMemory, BOOL tile)
{
    if (picture)
        DeleteObject(picture);
    picture = NULL;
    file[0] = '\0';
    DrawOffscreenLimit(0);
    if (!path || !*path || !ReadHeader(path))
        return !path || !*path;
    lstrcpy(file, path);
    wide = (int)dib.head.biWidth;
    high = (int)dib.head.biHeight;
    tiled = tile;
    if (inMemory && !LoadPicture()) {
        file[0] = '\0';
        return FALSE;
    }
    if (!inMemory)
        DrawOffscreenLimit(OFFSCREEN_MAX);  /* or a repaint would hold what this saved */
    return TRUE;
}

static void PaintOne(HDC dc, HDC mem, int x, int y, const RECT FAR *clip)
{
    if (mem) {
        int left = max(clip->left, x), top = max(clip->top, y);
        int right = min(clip->right, x + wide), bottom = min(clip->bottom, y + high);
        if (left < right && top < bottom)
            BitBlt(dc, left, top, right - left, bottom - top, mem, left - x, top - y, SRCCOPY);
    } else {
        PaintFile(dc, x, y, clip);
    }
}

/* Windows 3.1 has no PaintDesktop: the desktop window paints its pattern
   and wallpaper into our DC (for windows placed at the screen origin).
   Its procedure is called, not sent to: the desktop window belongs to the
   active task, and what is sent to a task that is ending is lost, which
   left an off-screen bitmap as it came, whatever was in that memory.
   Snowball's picture goes over that, when it does not cover it all. */
void SNSYSTEM_API DrawWallpaper(HDC dc)
{
    HWND desktop = GetDesktopWindow();
    int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN), x, y;
    HDC mem = NULL;
    HBITMAP old = NULL;
    RECT clip;

    if (!file[0] || (!tiled && (wide < cx || high < cy)))
        CallWindowProc((FARPROC)GetWindowLong(desktop, GWL_WNDPROC), desktop, WM_ERASEBKGND, (WPARAM)dc, 0L);
    if (!file[0])
        return;
    GetClipBox(dc, &clip);
    if (picture && (mem = CreateCompatibleDC(dc)) != NULL)
        old = SelectObject(mem, picture);
    if (tiled) {
        for (y = clip.top / high * high; y < clip.bottom; y += high)
            for (x = clip.left / wide * wide; x < clip.right; x += wide)
                PaintOne(dc, mem, x, y, &clip);
    } else {
        PaintOne(dc, mem, (cx - wide) / 2, (cy - high) / 2, &clip);
    }
    if (mem) {
        SelectObject(mem, old);
        DeleteDC(mem);
    }
}
