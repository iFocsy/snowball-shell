/*
 * A 256-colour copy of a true-colour BMP, for the wallpaper painted from its
 * file: a third of the bytes to read, and far less for the display driver to
 * convert. The palette is the picture's own, by median cut of a 15-bit
 * histogram, then Floyd-Steinberg diffusion hides what it lost. It works
 * through the file row by row: this runs where memory is short.
 */
#include "internal.h"
#undef MAX_PATH                         /* toolhelp.h has its own */
#include <toolhelp.h>

#define HIDDEN_PAGES 211                /* what the memory manager does not count, of 4 KB */
#define HIST_CELLS  32768L              /* 5 bits each of red, green and blue */
#define MAX_COLORS  256
#define CELL(r, g, b) (((long)(r) << 10) | ((g) << 5) | (b))

typedef struct {
    BYTE lo[3], hi[3];
    DWORD count;
} BOX;

static WORD FAR *hist;
static BYTE FAR *map;
static BOX boxes[MAX_COLORS];
static RGBQUAD colors[MAX_COLORS];
static int nBoxes;

/* Shrinks the box around what it holds, and counts it. */
static void Fit(BOX *b)
{
    int lo[3] = { 31, 31, 31 }, hi[3] = { 0, 0, 0 }, r, g, c;
    DWORD count = 0;

    for (r = b->lo[0]; r <= b->hi[0]; r++)
        for (g = b->lo[1]; g <= b->hi[1]; g++)
            for (c = b->lo[2]; c <= b->hi[2]; c++)
                if (hist[CELL(r, g, c)]) {
                    count += hist[CELL(r, g, c)];
                    if (r < lo[0]) lo[0] = r;
                    if (r > hi[0]) hi[0] = r;
                    if (g < lo[1]) lo[1] = g;
                    if (g > hi[1]) hi[1] = g;
                    if (c < lo[2]) lo[2] = c;
                    if (c > hi[2]) hi[2] = c;
                }
    b->count = count;
    if (count)
        for (c = 0; c < 3; c++) {
            b->lo[c] = (BYTE)lo[c];
            b->hi[c] = (BYTE)hi[c];
        }
}

/* The box to split next: the most pixels among those that can be. */
static int Largest(void)
{
    DWORD best = 0;
    int i, found = -1;

    for (i = 0; i < nBoxes; i++)
        if (boxes[i].count > best && (boxes[i].hi[0] > boxes[i].lo[0] || boxes[i].hi[1] > boxes[i].lo[1] ||
                                      boxes[i].hi[2] > boxes[i].lo[2])) {
            best = boxes[i].count;
            found = i;
        }
    return found;
}

/* In two at the median of its longest side. */
static void Split(BOX *b)
{
    static DWORD along[32];
    int axis = 0, a, r, g, c, at;
    DWORD sum = 0;
    BOX *n = &boxes[nBoxes++];

    for (a = 1; a < 3; a++)
        if (b->hi[a] - b->lo[a] > b->hi[axis] - b->lo[axis])
            axis = a;
    _fmemset(along, 0, sizeof along);
    for (r = b->lo[0]; r <= b->hi[0]; r++)
        for (g = b->lo[1]; g <= b->hi[1]; g++)
            for (c = b->lo[2]; c <= b->hi[2]; c++)
                along[axis == 0 ? r : axis == 1 ? g : c] += hist[CELL(r, g, c)];
    for (at = b->lo[axis]; at < b->hi[axis] - 1; at++) {
        sum += along[at];
        if (sum >= b->count / 2)
            break;
    }
    *n = *b;
    b->hi[axis] = (BYTE)at;
    n->lo[axis] = (BYTE)(at + 1);
    Fit(b);
    Fit(n);
}

/* The palette and, for every cell, its colour. */
static void BuildPalette(void)
{
    int i, r, g, c;

    nBoxes = 1;
    boxes[0].lo[0] = boxes[0].lo[1] = boxes[0].lo[2] = 0;
    boxes[0].hi[0] = boxes[0].hi[1] = boxes[0].hi[2] = 31;
    Fit(&boxes[0]);
    while (nBoxes < MAX_COLORS && (i = Largest()) >= 0)
        Split(&boxes[i]);
    for (i = 0; i < nBoxes; i++) {
        BOX *b = &boxes[i];
        DWORD sum[3] = { 0, 0, 0 }, count = 0;

        for (r = b->lo[0]; r <= b->hi[0]; r++)
            for (g = b->lo[1]; g <= b->hi[1]; g++)
                for (c = b->lo[2]; c <= b->hi[2]; c++) {
                    WORD n = hist[CELL(r, g, c)];
                    sum[0] += (DWORD)n * r;
                    sum[1] += (DWORD)n * g;
                    sum[2] += (DWORD)n * c;
                    count += n;
                    map[CELL(r, g, c)] = (BYTE)i;
                }
        if (!count)
            count = 1;
        colors[i].rgbRed = (BYTE)(sum[0] * 255 / 31 / count);
        colors[i].rgbGreen = (BYTE)(sum[1] * 255 / 31 / count);
        colors[i].rgbBlue = (BYTE)(sum[2] * 255 / 31 / count);
        colors[i].rgbReserved = 0;
    }
}

/* Dithering reaches colours the picture does not have: the nearest is found the first
   time, and kept. */
static BYTE Nearest(int red, int green, int blue)
{
    long best = 0x7FFFFFFFL;
    int i, found = 0;

    for (i = 0; i < nBoxes; i++) {
        long r = red - colors[i].rgbRed, g = green - colors[i].rgbGreen, b = blue - colors[i].rgbBlue;
        long d = r * r + g * g + b * b;
        if (d < best) {
            best = d;
            found = i;
        }
    }
    return (BYTE)found;
}

static int Clamp(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

/* A row, BGR, to colour numbers. err: this row's errors, next: the next one's. */
static void Diffuse(const BYTE FAR *row, BYTE FAR *out, int w, int FAR *err, int FAR *next)
{
    int x, c;

    _fmemset(next, 0, (w + 2) * 3 * sizeof(int));
    for (x = 0; x < w; x++) {
        int v[3], e;
        long cell;
        BYTE i;
        const RGBQUAD FAR *p;

        for (c = 0; c < 3; c++)         /* red first */
            v[c] = Clamp(row[x * 3 + 2 - c] + err[(x + 1) * 3 + c] / 16);
        cell = CELL(v[0] >> 3, v[1] >> 3, v[2] >> 3);
        if (!hist[cell]) {
            map[cell] = Nearest(v[0], v[1], v[2]);
            hist[cell] = 1;
        }
        i = map[cell];
        out[x] = i;
        p = &colors[i];
        for (c = 0; c < 3; c++) {
            e = v[c] - (c == 0 ? p->rgbRed : c == 1 ? p->rgbGreen : p->rgbBlue);
            err[(x + 2) * 3 + c] += e * 7;
            next[x * 3 + c] += e * 3;
            next[(x + 1) * 3 + c] += e * 5;
            next[(x + 2) * 3 + c] += e;
        }
    }
}

static BOOL Histogram(HFILE f, DWORD bitsAt, BYTE FAR *row, int w, int h, UINT stride)
{
    int x, y;

    _llseek(f, bitsAt, 0);
    for (y = 0; y < h; y++) {
        if (_lread(f, row, stride) != stride)
            return FALSE;
        for (x = 0; x < w; x++) {
            WORD FAR *cell = &hist[CELL(row[x * 3 + 2] >> 3, row[x * 3 + 1] >> 3, row[x * 3] >> 3)];
            if (*cell != 0xFFFF)
                ++*cell;
        }
        if (!(y & 31))
            WallpaperYield(0);
    }
    return TRUE;
}

static BOOL Convert(HFILE f, HFILE to, DWORD bitsAt, BYTE FAR *row, int w, int h, UINT stride, UINT outStride)
{
    UINT size = (w + 2) * 3 * sizeof(int);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size * 2 + outStride);
    int FAR *err, FAR *next, FAR *swap;
    BYTE FAR *out;
    BOOL ok = TRUE;
    int y;

    if (!mem)
        return FALSE;
    err = (int FAR *)GlobalLock(mem);
    next = err + (w + 2) * 3;
    out = (BYTE FAR *)(next + (w + 2) * 3);
    _fmemset(err, 0, size);
    _fmemset(out, 0, outStride);
    _llseek(f, bitsAt, 0);
    for (y = 0; y < h && ok; y++) {
        ok = _lread(f, row, stride) == stride;
        if (ok) {
            Diffuse(row, out, w, err, next);
            ok = _lwrite(to, (LPCSTR)out, outStride) == outStride;
        }
        swap = err;
        err = next;
        next = swap;
        if (!(y & 31))
            WallpaperYield(0);
    }
    GlobalUnlock(mem);
    GlobalFree(mem);
    return ok;
}

static BOOL WriteHeader(HFILE to, int w, int h, UINT outStride)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER head;
    DWORD palette = (DWORD)nBoxes * sizeof(RGBQUAD);

    _fmemset(&fh, 0, sizeof fh);
    _fmemset(&head, 0, sizeof head);
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof fh + sizeof head + palette;
    fh.bfSize = fh.bfOffBits + (DWORD)outStride * h;
    head.biSize = sizeof head;
    head.biWidth = w;
    head.biHeight = h;
    head.biPlanes = 1;
    head.biBitCount = 8;
    head.biClrUsed = nBoxes;
    return _lwrite(to, (LPCSTR)&fh, sizeof fh) == sizeof fh &&
           _lwrite(to, (LPCSTR)&head, sizeof head) == sizeof head &&
           _lwrite(to, (LPCSTR)colors, (UINT)palette) == palette;
}

/* The memory in the machine, in KB; much, if it cannot be told. */
DWORD WallpaperMemoryKB(void)
{
    MEMMANINFO m;

    m.dwSize = sizeof m;
    if (!MemManInfo(&m))
        return 0x7FFFFFFFL;
    return (m.dwTotalPages + HIDDEN_PAGES) * 4;
}

/* source: a plain 24-bit BMP. FALSE: target was not made. */
BOOL WallpaperMakeLow(LPCSTR source, LPCSTR target)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER head;
    char temp[MAX_PATH];
    HGLOBAL histMem, mapMem, rowMem;
    HFILE f, to = HFILE_ERROR;
    BYTE FAR *row;
    UINT stride, outStride;
    BOOL ok = FALSE;
    int w, h;

    if ((f = _lopen(source, OF_READ | OF_SHARE_DENY_NONE)) == HFILE_ERROR)
        return FALSE;
    _lread(f, &fh, sizeof fh);
    _lread(f, &head, sizeof head);
    w = (int)head.biWidth;
    h = (int)head.biHeight;
    stride = (UINT)(((DWORD)w * 3 + 3) / 4 * 4);
    outStride = (UINT)(((DWORD)w + 3) / 4 * 4);
    /* each within one segment */
    histMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, HIST_CELLS * 2);
    mapMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, HIST_CELLS);
    rowMem = GlobalAlloc(GMEM_MOVEABLE, stride);
    if (!histMem || !mapMem || !rowMem)
        goto done;
    hist = (WORD FAR *)GlobalLock(histMem);
    map = (BYTE FAR *)GlobalLock(mapMem);
    row = (BYTE FAR *)GlobalLock(rowMem);
    if (Histogram(f, fh.bfOffBits, row, w, h, stride)) {
        BuildPalette();
        lstrcpy(temp, target);
        lstrcpy(PathExtension(temp), ".TMP");
        if ((to = _lcreat(temp, 0)) != HFILE_ERROR) {
            ok = WriteHeader(to, w, h, outStride) && Convert(f, to, fh.bfOffBits, row, w, h, stride, outStride);
            _lclose(to);
            if (ok) {
                DosDelete(target);
                ok = DosRename(temp, target);
            } else {
                DosDelete(temp);
            }
        }
    }
done:
    if (histMem) GlobalFree(histMem);
    if (mapMem) GlobalFree(mapMem);
    if (rowMem) GlobalFree(rowMem);
    _lclose(f);
    return ok;
}
