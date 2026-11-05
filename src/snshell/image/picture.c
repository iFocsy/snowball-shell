/*
 * A picture file made into what will show on the screen: which format it
 * is, what size it comes out and which part of it is used, and the BMP
 * written at last, the one form Windows 3.1's wallpaper takes.
 */
#include "internal.h"
#include "imageint.h"

typedef struct {
    int cropX, cropY, cropW, cropH;     /* of the picture as stored */
    int shownW, shownH;                 /* on the screen */
    int dibW, dibH;                     /* the DIB: shown, or less for a small view */
} PLAN;

int ImageFill(IMAGE *im)
{
    UINT n = im->ended ? 0 : _lread(im->file, im->in, IMAGE_BUFFER);

    if (n == 0 || n == (UINT)HFILE_ERROR) {
        im->ended = TRUE;
        im->pos = im->len = 0;
        return -1;
    }
    im->done += n;
    im->len = n;
    im->pos = 1;
    return im->in[0];
}

void ImageSkip(IMAGE *im, DWORD n)
{
    DWORD have = im->len - im->pos;

    if (n <= have) {
        im->pos += (UINT)n;
        return;
    }
    n -= have;
    im->pos = im->len = 0;
    _llseek(im->file, (LONG)n, 1);
    im->done += n;
}

int ImagePercent(const IMAGE *im)
{
    return (int)((im->done >> 8) * 100 / ((im->size >> 8) + 1));
}

BOOL ImageTick(IMAGE *im, int percent)
{
    DWORD now = GetTickCount();

    if (im->progress && now - im->tick >= 400) {
        im->tick = now;
        if (!im->progress(percent))
            im->stopped = TRUE;
    }
    return !im->stopped;
}

void *ImageAlloc(DWORD size)
{
    HGLOBAL h = GlobalAlloc(GHND, size);

    return h ? GlobalLock(h) : NULL;
}

void ImageFree(void *p)
{
    if (p) {
        HGLOBAL h = (HGLOBAL)LOWORD(GlobalHandle(SELECTOROF(p)));
        GlobalUnlock(h);
        GlobalFree(h);
    }
}

static void Close(IMAGE *im)
{
    if (im->close)
        im->close(im);
    ImageFree(im->in);
    _lclose(im->file);
}

/* The format from its first bytes, whatever the file is called. */
static int Open(LPCSTR path, IMAGE *im)
{
    int a, b, r = IMAGE_MEMORY;

    _fmemset(im, 0, sizeof *im);
    im->orient = 1;
    if ((im->file = DosOpenRead(path)) == HFILE_ERROR)
        return IMAGE_NOFILE;
    im->size = _llseek(im->file, 0, 2);
    _llseek(im->file, 0, 0);
    if ((im->in = (BYTE *)ImageAlloc(IMAGE_BUFFER)) != NULL) {
        a = ImageByte(im);
        b = ImageByte(im);
        r = a == 0xFF && b == 0xD8 ? JpegOpen(im) :
            a == 0x89 && b == 'P' ? PngOpen(im) :
            a == 'B' && b == 'M' ? BmpOpen(im) : IMAGE_DAMAGED;
    }
    if (r != IMAGE_OK)
        Close(im);
    return r;
}

int ImageSize(LPCSTR path, int *w, int *h)
{
    IMAGE im;
    int   r = Open(path, &im);

    if (r == IMAGE_OK) {
        *w = im.orient >= 5 ? im.h : im.w;
        *h = im.orient >= 5 ? im.w : im.h;
        Close(&im);
    }
    return r;
}

/* Sizes and parts by style, as seen, then the part as stored. */
static void Plan(const IMAGE *im, const IMAGEJOB *job, int scaleTo, PLAN *p)
{
    BOOL turned = im->orient >= 5;
    int  pw = turned ? im->h : im->w, ph = turned ? im->w : im->h, x = 0, y = 0, cw = pw, ch = ph, t;
    int  sw = job->w, sh = job->h, style = job->style;

    if ((style == WP_CENTER || style == WP_TILE) && (pw > sw || ph > sh))
        style = WP_FIT;
    switch (style) {
    case WP_FILL:
        if ((long)pw * sh > (long)ph * sw) {                    /* wider than the screen */
            cw = (int)((long)ph * sw / sh);
            x = (pw - cw) / 2;
        } else {
            ch = (int)((long)pw * sh / sw);
            y = (ph - ch) / 2;
        }
        /* fall through */
    case WP_STRETCH:
        p->shownW = sw;
        p->shownH = sh;
        break;
    case WP_FIT:
        if ((long)pw * sh > (long)ph * sw) {
            p->shownW = sw;
            p->shownH = (int)max((long)ph * sw / pw, 1);
        } else {
            p->shownH = sh;
            p->shownW = (int)max((long)pw * sh / ph, 1);
        }
        break;
    default:
        p->shownW = pw;
        p->shownH = ph;
    }
    if (turned) {
        t = x; x = y; y = t;
        t = cw; cw = ch; ch = t;
    }
    if (im->orient == 2 || im->orient == 3 || im->orient == 7 || im->orient == 8)
        x = im->w - x - cw;             /* stored the other way across */
    if (im->orient == 3 || im->orient == 4 || im->orient == 6 || im->orient == 7)
        y = im->h - y - ch;             /* and down */
    p->cropX = x;
    p->cropY = y;
    p->cropW = max(cw, 1);
    p->cropH = max(ch, 1);
    p->dibW = scaleTo ? (int)max((long)p->shownW * scaleTo / sw, 1) : p->shownW;
    p->dibH = scaleTo ? (int)max((long)p->shownH * scaleTo / sw, 1) : p->shownH;
}

/* A JPEG read at 1/2, 1/4 or 1/8 of its size where that is still enough. */
static int Reduction(const PLAN *p, int outW, int outH)
{
    int r = 1;

    while (r < 8 && (long)outW * r * 2 <= p->cropW && (long)outH * r * 2 <= p->cropH)
        r *= 2;
    return r;
}

int ImageMakeDib(LPCSTR path, const IMAGEJOB *job, int scaleTo, DIB *d)
{
    IMAGE im;
    PLAN  p;
    RESAMPLEJOB rj;
    BITMAPINFOHEADER FAR *bi;
    DWORD pitch;
    int   r, reduce;

    _fmemset(d, 0, sizeof *d);
    if ((r = Open(path, &im)) != IMAGE_OK)
        return r;
    im.back = job->back;
    im.progress = job->progress;
    im.tick = GetTickCount();
    Plan(&im, job, scaleTo, &p);
    rj.orient = im.orient;
    rj.outW = im.orient >= 5 ? p.dibH : p.dibW;
    rj.outH = im.orient >= 5 ? p.dibW : p.dibH;
    reduce = Reduction(&p, rj.outW, rj.outH);
    r = im.prepare(&im, &reduce);
    pitch = ((DWORD)p.dibW * 3 + 3) & ~3L;
    if (r == IMAGE_OK && (d->mem = GlobalAlloc(GHND, sizeof *bi + pitch * p.dibH)) == NULL)
        r = IMAGE_MEMORY;
    if (r == IMAGE_OK) {
        bi = (BITMAPINFOHEADER FAR *)GlobalLock(d->mem);
        bi->biSize = sizeof *bi;
        bi->biWidth = p.dibW;
        bi->biHeight = p.dibH;
        bi->biPlanes = 1;
        bi->biBitCount = 24;
        d->w = p.dibW;
        d->h = p.dibH;
        d->bits = sizeof *bi;
        d->shownW = p.shownW;
        d->shownH = p.shownH;
        rj.rgb = im.rgbRows;
        rj.srcW = im.rowW;
        rj.srcH = im.rowH;
        rj.cropX = (int)((long)p.cropX * im.rowW / im.w);
        rj.cropY = (int)((long)p.cropY * im.rowH / im.h);
        rj.cropW = (int)max((long)p.cropW * im.rowW / im.w, 1);
        rj.cropH = (int)max((long)p.cropH * im.rowH / im.h, 1);
        rj.bits = (BYTE huge *)bi + sizeof *bi;
        rj.pitch = pitch;
        rj.dibH = p.dibH;
        if ((im.rs = ResampleOpen(&rj)) == NULL) {
            r = IMAGE_MEMORY;
        } else {
            r = im.run(&im);
            ResampleClose(im.rs);
        }
        GlobalUnlock(d->mem);
    }
    if (r == IMAGE_OK && im.stopped)
        r = IMAGE_STOPPED;
    Close(&im);
    if (r != IMAGE_OK)
        DibFree(d);
    return r;
}

/* ---- the file written ---- */

static const BYTE Bayer[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42 }, { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 }, { 60, 28, 52, 20, 62, 30, 54, 22 },
    {  3, 35, 11, 43,  1, 33,  9, 41 }, { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 }, { 63, 31, 55, 23, 61, 29, 53, 21 },
};

static const BYTE Vga[16][3] = {        /* B, G, R: Windows' 16 colours */
    { 0, 0, 0 }, { 0, 0, 128 }, { 0, 128, 0 }, { 0, 128, 128 }, { 128, 0, 0 }, { 128, 0, 128 },
    { 128, 128, 0 }, { 192, 192, 192 }, { 128, 128, 128 }, { 0, 0, 255 }, { 0, 255, 0 },
    { 0, 255, 255 }, { 255, 0, 0 }, { 255, 0, 255 }, { 255, 255, 0 }, { 255, 255, 255 },
};

/* 256 colours: 6 levels of each, the steps between them dithered. */
static void Row8(const BYTE *in, BYTE *out, int w, int y)
{
    int x, c;

    for (x = 0; x < w; x++, in += 3) {
        int t = Bayer[y & 7][x & 7], index = 0, mul[3];
        mul[0] = 1;
        mul[1] = 6;
        mul[2] = 36;
        for (c = 0; c < 3; c++) {
            UINT v = (UINT)in[c] * 5, level = v / 255;
            if ((v % 255) * 64 / 255 > (UINT)t)
                level++;
            index += (int)level * mul[c];
        }
        out[x] = (BYTE)index;
    }
}

/* 16 colours: the nearest, after a little dither. */
static void Row4(const BYTE *in, BYTE *out, int w, int y)
{
    int x, i, c;

    for (x = 0; x < w; x++, in += 3) {
        int  t = (Bayer[y & 7][x & 7] - 32) * 2, best = 0;
        long bestOff = 0x7FFFFFFFL;
        for (i = 0; i < 16; i++) {
            long off = 0;
            for (c = 0; c < 3; c++) {
                long d = (long)in[c] + t - Vga[i][c];
                off += d * d;
            }
            if (off < bestOff) {
                bestOff = off;
                best = i;
            }
        }
        if (x & 1)
            out[x / 2] |= (BYTE)best;
        else
            out[x / 2] = (BYTE)(best << 4);
    }
}

static void Row1(const BYTE *in, BYTE *out, int w, int y)
{
    int x;

    for (x = 0; x < w; x++, in += 3) {
        UINT lum = ((UINT)in[0] * 29 + (UINT)in[1] * 150 + (UINT)in[2] * 77) >> 8;
        if (!(x & 7))
            out[x / 8] = 0;
        if (lum > (UINT)Bayer[y & 7][x & 7] * 4 + 2)
            out[x / 8] |= (BYTE)(0x80 >> (x & 7));
    }
}

static void WritePalette(HFILE f, int depth)
{
    RGBQUAD q;
    int     i;

    q.rgbReserved = 0;
    if (depth == 8) {
        for (i = 0; i < 216; i++) {
            q.rgbBlue = (BYTE)(i % 6 * 51);
            q.rgbGreen = (BYTE)(i / 6 % 6 * 51);
            q.rgbRed = (BYTE)(i / 36 * 51);
            _lwrite(f, (LPCSTR)&q, sizeof q);
        }
    } else if (depth == 4) {
        for (i = 0; i < 16; i++) {
            q.rgbBlue = Vga[i][0];
            q.rgbGreen = Vga[i][1];
            q.rgbRed = Vga[i][2];
            _lwrite(f, (LPCSTR)&q, sizeof q);
        }
    } else {
        for (i = 0; i < 2; i++) {
            q.rgbBlue = q.rgbGreen = q.rgbRed = (BYTE)(i * 255);
            _lwrite(f, (LPCSTR)&q, sizeof q);
        }
    }
}

int ImageWriteBitmap(const DIB *d, LPCSTR path, int depth)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER bi;
    BYTE huge *bits = (BYTE huge *)GlobalLock(d->mem) + d->bits;
    DWORD pitch = ((DWORD)d->w * 3 + 3) & ~3L, size, outPitch;
    int   colors, y, ok = TRUE;
    HFILE f = DosCreate(path);

    if (f == HFILE_ERROR) {
        GlobalUnlock(d->mem);
        return IMAGE_NOWRITE;
    }
    depth = depth >= 15 ? 24 : depth >= 8 ? 8 : depth >= 4 ? 4 : 1;
    colors = depth == 24 ? 0 : depth == 8 ? 216 : depth == 4 ? 16 : 2;
    outPitch = ((DWORD)d->w * depth + 31) / 32 * 4;
    size = outPitch * d->h;
    _fmemset(&bi, 0, sizeof bi);
    bi.biSize = sizeof bi;
    bi.biWidth = d->w;
    bi.biHeight = d->h;
    bi.biPlanes = 1;
    bi.biBitCount = (WORD)depth;
    bi.biSizeImage = size;
    bi.biClrUsed = colors;
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof fh + sizeof bi + colors * 4L;
    fh.bfSize = fh.bfOffBits + size;
    fh.bfReserved1 = fh.bfReserved2 = 0;
    _lwrite(f, (LPCSTR)&fh, sizeof fh);
    _lwrite(f, (LPCSTR)&bi, sizeof bi);
    if (depth == 24) {
        ok = _hwrite(f, (const void huge *)bits, (long)size) == (long)size;
    } else {
        BYTE *in = (BYTE *)ImageAlloc(pitch), *out = (BYTE *)ImageAlloc(outPitch);
        if (!in || !out) {
            ok = FALSE;
        } else {
            WritePalette(f, depth);
            for (y = 0; y < d->h && ok; y++) {
                hmemcpy(in, bits + pitch * y, (long)pitch);
                if (depth == 8)
                    Row8(in, out, d->w, y);
                else if (depth == 4)
                    Row4(in, out, d->w, y);
                else
                    Row1(in, out, d->w, y);
                ok = _lwrite(f, (LPCSTR)out, (UINT)outPitch) == (UINT)outPitch;
            }
        }
        ImageFree(in);
        ImageFree(out);
    }
    _lclose(f);
    GlobalUnlock(d->mem);
    return ok ? IMAGE_OK : IMAGE_NOWRITE;
}
