/* BMP files, read whole and checked before GDI sees them: a damaged or
   misnamed file must not make GDI read past the block. Thumbnails, Quick
   View and Display Properties' monitor draw them. */
#include "internal.h"

#define DIB_MAX     0x400000L           /* larger files are not read */

/* A Windows 3 DIB that GDI draws, its bits inside the size read. */
static BOOL Valid(const BITMAPINFOHEADER FAR *bi, DWORD size, DWORD bits)
{
    DWORD colors = bi->biClrUsed ? bi->biClrUsed : bi->biBitCount <= 8 ? 1L << bi->biBitCount : 0;

    if (bi->biSize != sizeof(BITMAPINFOHEADER) || bi->biPlanes != 1 || bi->biWidth <= 0 || bi->biWidth > 32767 ||
        bi->biHeight <= 0 || bi->biHeight > 32767 || colors > 256 || bits < bi->biSize + colors * 4 || bits >= size)
        return FALSE;
    switch ((int)bi->biCompression) {
    case BI_RGB:
        return (bi->biBitCount == 1 || bi->biBitCount == 4 || bi->biBitCount == 8 || bi->biBitCount == 24) &&
               ((DWORD)bi->biWidth * bi->biBitCount + 31) / 32 * 4 * bi->biHeight <= size - bits;
    case BI_RLE8:
    case BI_RLE4:
        return bi->biBitCount == (bi->biCompression == BI_RLE8 ? 8 : 4) && bi->biSizeImage &&
               bi->biSizeImage <= size - bits;
    }
    return FALSE;
}

BOOL DibLoad(LPCSTR path, DIB *d)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER FAR *bi;
    HFILE f;
    LONG size;
    BOOL ok = FALSE;

    _fmemset(d, 0, sizeof *d);
    if ((f = DosOpenRead(path)) == HFILE_ERROR)
        return FALSE;
    size = _llseek(f, 0, 2) - (LONG)sizeof fh;
    _llseek(f, 0, 0);
    if (size > (LONG)sizeof *bi && size <= DIB_MAX && _lread(f, &fh, sizeof fh) == sizeof fh &&
        fh.bfType == 0x4D42 && fh.bfOffBits >= sizeof fh && (d->mem = GlobalAlloc(GMEM_MOVEABLE, size)) != NULL) {
        bi = (BITMAPINFOHEADER FAR *)GlobalLock(d->mem);
        d->bits = fh.bfOffBits - sizeof fh;
        ok = _hread(f, (void huge *)bi, size) == size && Valid(bi, size, d->bits);
        d->w = (int)bi->biWidth;
        d->h = (int)bi->biHeight;
        GlobalUnlock(d->mem);
    }
    _lclose(f);
    if (!ok)
        DibFree(d);
    return ok;
}

void DibDraw(HDC dc, const DIB *d, int x, int y, int cx, int cy)
{
    BYTE huge *p = (BYTE huge *)GlobalLock(d->mem);

    StretchDIBits(dc, x, y, cx, cy, 0, 0, d->w, d->h, (LPVOID)(p + d->bits), (BITMAPINFO FAR *)p,
                  DIB_RGB_COLORS, SRCCOPY);
    GlobalUnlock(d->mem);
}

void DibFree(DIB *d)
{
    if (d->mem)
        GlobalFree(d->mem);
    _fmemset(d, 0, sizeof *d);
}
