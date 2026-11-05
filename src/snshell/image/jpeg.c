/*
 * JPEG files: baseline, extended sequential and progressive, Huffman coded,
 * 8 bits, grey or YCbCr or RGB, turned as their EXIF data says.
 *
 * A plain file goes through once: a row of MCUs decoded, turned to pixels and
 * given on, in the memory of that row alone. Progressive files (and plain
 * ones with a scan per colour) hold every block between scans, but only what
 * the size asked for needs: read at 1/4, four coefficients a block, plus a
 * bit each for the rest, which later scans refer to. Read at 1/8 the first
 * scans are enough and the others are skipped. Short of memory, a file is
 * read at less, down to that: it comes out softer, but it comes out.
 */
#include "internal.h"
#include "imageint.h"
#include "jpeg.h"

const BYTE JpegZigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

static int Word(IMAGE *im)
{
    int a = ImageByte(im), b = ImageByte(im);

    return (a & 255) << 8 | (b & 255);
}

/* ---- segments ---- */

static void ReadQuant(JPEG *j, long n)
{
    IMAGE *im = j->im;
    int   i;

    while (n >= 65) {
        int pt = ImageByte(im), wide = (pt >> 4) & 1;
        WORD *t = j->quant[pt & 3];
        for (i = 0; i < 64; i++)
            t[JpegZigzag[i]] = wide ? (WORD)Word(im) : (WORD)(ImageByte(im) & 255);
        n -= 65 + (wide ? 64 : 0);
    }
    if (n > 0)
        ImageSkip(im, n);
}

static void ReadHuff(JPEG *j, long n)
{
    IMAGE *im = j->im;
    BYTE  counts[16], values[256];
    int   i, total;

    while (n >= 17) {
        int tc = ImageByte(im);
        for (total = i = 0; i < 16; i++)
            total += counts[i] = (BYTE)ImageByte(im);
        if (total > 256 || total > n - 17)
            break;
        _fmemset(values, 0, sizeof values);
        for (i = 0; i < total; i++)
            values[i] = (BYTE)ImageByte(im);
        JpegHuffBuild(tc & 0x10 ? &j->ac[tc & 3] : &j->dc[tc & 3], counts, values);
        n -= 17 + total;
    }
}

/* EXIF's orientation, from the first directory. */
static void ReadExif(JPEG *j, long n)
{
    IMAGE *im = j->im;
    BYTE  *b = (BYTE *)ImageAlloc(n > 4096 ? 4096 : n);
    long  have = n > 4096 ? 4096 : n, i;

    if (!b) {
        ImageSkip(im, n);
        return;
    }
    for (i = 0; i < have; i++)
        b[(UINT)i] = (BYTE)ImageByte(im);
    ImageSkip(im, n - have);
    if (have > 14 && !_fmemcmp(b, "Exif\0\0", 6)) {
        const BYTE *t = b + 6;
        long  size = have - 6;
        BOOL  little = t[0] == 'I';
#define Get16(p)    (little ? (p)[0] | (p)[1] << 8 : (p)[0] << 8 | (p)[1])
        long  at = little ? (long)t[4] | (long)t[5] << 8 : (long)t[6] << 8 | t[7];
        if (at >= 8 && at + 2 <= size) {
            int count = Get16(t + at);
            for (i = 0; i < count && at + 2 + (i + 1) * 12 <= size; i++) {
                const BYTE *e = t + at + 2 + i * 12;
                if (Get16(e) == 0x0112) {
                    int v = Get16(e + 8);
                    if (v >= 1 && v <= 8)
                        im->orient = v;
                    break;
                }
            }
        }
    }
    ImageFree(b);
}

static void ReadApp14(JPEG *j, long n)
{
    IMAGE *im = j->im;
    BYTE  b[12];
    int   i, have = n < 12 ? (int)n : 12;

    for (i = 0; i < have; i++)
        b[i] = (BYTE)ImageByte(im);
    ImageSkip(im, n - have);
    if (have == 12 && !_fmemcmp(b, "Adobe", 5)) {
        j->adobe = TRUE;
        j->transform = b[11];
    }
}

static int ReadFrame(JPEG *j, long n)
{
    IMAGE *im = j->im;
    int   precision = ImageByte(im), i;

    im->h = Word(im);
    im->w = Word(im);
    j->comps = ImageByte(im);
    if (precision != 8 || im->w <= 0 || im->h <= 0 || (j->comps != 1 && j->comps != 3) || n < 6 + 3 * j->comps)
        return IMAGE_DAMAGED;
    for (i = 0; i < j->comps; i++) {
        JCOMP *c = &j->comp[i];
        int   hv;
        c->id = ImageByte(im);
        hv = ImageByte(im);
        c->tq = ImageByte(im) & 3;
        c->h = hv >> 4;
        c->v = hv & 15;
        if (c->h < 1 || c->h > 4 || c->v < 1 || c->v > 4)
            return IMAGE_DAMAGED;
        j->hMax = max(j->hMax, c->h);
        j->vMax = max(j->vMax, c->v);
    }
    if (j->comps == 1)
        j->comp[0].h = j->comp[0].v = j->hMax = j->vMax = 1;
    j->mcusX = (int)(((long)im->w + 8 * j->hMax - 1) / (8 * j->hMax));
    j->mcusY = (int)(((long)im->h + 8 * j->vMax - 1) / (8 * j->vMax));
    for (i = 0; i < j->comps; i++) {
        JCOMP *c = &j->comp[i];
        long  w = ((long)im->w * c->h + j->hMax - 1) / j->hMax, h = ((long)im->h * c->v + j->vMax - 1) / j->vMax;
        c->blocksW = j->mcusX * c->h;
        c->blocksH = j->mcusY * c->v;
        c->scanW = (int)((w + 7) / 8);
        c->scanH = (int)((h + 7) / 8);
    }
    ImageSkip(im, n - 6 - 3 * j->comps);
    return IMAGE_OK;
}

static BOOL ReadScanHeader(JPEG *j, long n)
{
    IMAGE *im = j->im;
    int   i, k, count = ImageByte(im);

    if (count < 1 || count > j->comps || n < 4 + 2 * count)
        return FALSE;
    j->scanComps = count;
    for (i = 0; i < count; i++) {
        int id = ImageByte(im), tables = ImageByte(im);
        for (k = 0; k < j->comps && j->comp[k].id != id; k++)
            ;
        if (k == j->comps)
            return FALSE;
        j->scanComp[i] = k;
        j->comp[k].dcTable = (tables >> 4) & 3;
        j->comp[k].acTable = tables & 3;
    }
    j->ss = ImageByte(im) & 63;
    j->se = ImageByte(im) & 63;
    k = ImageByte(im);
    j->ah = (k >> 4) & 15;
    j->al = k & 15;
    if (j->al > 13)
        j->al = 13;
    if (!j->progressive) {
        j->ss = 0;
        j->se = 63;
        j->ah = j->al = 0;
    }
    ImageSkip(im, n - 4 - 2 * count);
    return TRUE;
}

/* ---- memory ---- */

static void FreeStore(JPEG *j)
{
    int i;

    for (i = 0; i < j->comps; i++) {
        ImageFree((void *)j->comp[i].coef);
        ImageFree((void *)j->comp[i].nonzero);
        j->comp[i].coef = NULL;
        j->comp[i].nonzero = NULL;
    }
    j->stored = FALSE;
}

/* Every block held, as much of each as is needed. */
static BOOL AllocStore(JPEG *j)
{
    int i;

    for (i = 0; i < j->comps; i++) {
        JCOMP *c = &j->comp[i];
        DWORD blocks = (DWORD)c->blocksW * c->blocksH;
        c->coef = (short huge *)ImageAlloc(blocks * j->nn * 2);
        if (j->n > 1 && j->progressive)
            c->nonzero = (BYTE huge *)ImageAlloc(blocks * 8);
        if (!c->coef || (j->n > 1 && j->progressive && !c->nonzero)) {
            FreeStore(j);
            return FALSE;
        }
    }
    j->stored = TRUE;
    return TRUE;
}

static void FreeRows(JPEG *j)
{
    int i;

    for (i = 0; i < 3; i++) {
        ImageFree(j->comp[i].rows);
        ImageFree(j->up[i]);
        j->comp[i].rows = NULL;
        j->up[i] = NULL;
    }
    ImageFree(j->bgr);
    j->bgr = NULL;
}

/* The rows of memory at a reduction; FALSE: they do not fit. */
static BOOL AllocRows(JPEG *j, int reduce)
{
    IMAGE *im = j->im;
    int   i, k;

    j->n = 8 / reduce;
    j->nn = j->n * j->n;
    im->rowW = (int)(((long)im->w * j->n + 7) / 8);
    im->rowH = (int)(((long)im->h * j->n + 7) / 8);
    if ((DWORD)im->rowW * 3 > IMAGE_ROW_MAX)
        return FALSE;
    for (i = 0; i < j->comps; i++) {
        JCOMP *c = &j->comp[i];
        DWORD size;
        c->stride = (UINT)c->blocksW * j->n;
        size = (DWORD)c->stride * c->v * j->n;
        if (size > IMAGE_ROW_MAX || (c->rows = (BYTE *)ImageAlloc(size)) == NULL)
            return FALSE;
        if (c->h != j->hMax && (j->up[i] = (BYTE *)ImageAlloc((DWORD)im->rowW + 8)) == NULL)
            return FALSE;
    }
    if ((j->bgr = (BYTE *)ImageAlloc((DWORD)im->rowW * 3)) == NULL)
        return FALSE;
    for (k = 0; k < 64; k++) {
        int row = JpegZigzag[k] >> 3, col = JpegZigzag[k] & 7;
        j->keep[k] = (signed char)(row < j->n && col < j->n ? row * j->n + col : -1);
    }
    return TRUE;
}

static int Prepare(IMAGE *im, int *reduce)
{
    JPEG *j = (JPEG *)im->reader;

    for (;;) {
        if (AllocRows(j, *reduce) && (!j->progressive || AllocStore(j)))
            return IMAGE_OK;
        FreeRows(j);
        if (*reduce == 8)
            return IMAGE_MEMORY;
        *reduce *= 2;
    }
}

/* ---- pixels ---- */

static void ColorTables(JPEG *j)
{
    int i;

    for (i = 0; i < 256; i++) {
        long x = i - 128;
        j->crR[i] = (int)((91881L * x + 32768L) >> 16);
        j->cbB[i] = (int)((116130L * x + 32768L) >> 16);
        j->crG[i] = (int)((-46802L * x + 32768L) >> 16);
        j->cbG[i] = (int)((-22554L * x + 32768L) >> 16);
    }
    for (i = 0; i < 768; i++)
        j->range[i] = (BYTE)(i < 256 ? 0 : i > 511 ? 255 : i - 256);
}

/* Each kept coefficient's divisor, in the order blocks are kept. */
static void KeptQuant(JPEG *j)
{
    int t, y, x;

    for (t = 0; t < 4; t++)
        for (y = 0; y < j->n; y++)
            for (x = 0; x < j->n; x++)
                j->quantKept[t][y * j->n + x] = j->n == 8 ? JpegIdctScale(j->quant[t][y * 8 + x], y * 8 + x) :
                                                j->quant[t][y * 8 + x];
}

static void Widen(const BYTE *from, BYTE *to, int w, int h, int hMax)
{
    int x, acc = 0;

    for (x = 0; x < w; x++) {
        to[x] = *from;
        if ((acc += h) >= hMax) {
            acc -= hMax;
            from++;
        }
    }
}

/* Adobe's RGB, or named so: not YCbCr. */
static BOOL IsRgb(const JPEG *j)
{
    if (j->comps != 3 || j->jfif)
        return FALSE;
    if (j->adobe)
        return j->transform == 0;
    return j->comp[0].id == 'R' && j->comp[1].id == 'G' && j->comp[2].id == 'B';
}

/* The decoded rows of MCU row my given on; FALSE: no more are needed. */
static BOOL GiveRows(JPEG *j, int my)
{
    IMAGE *im = j->im;
    int   rows = j->vMax * j->n, y0 = my * rows, y, c, x, w = im->rowW;
    BOOL  rgb = IsRgb(j);

    for (y = 0; y < rows && y0 + y < im->rowH; y++) {
        BYTE *to = j->bgr;
        BOOL half = j->comps == 3 && !rgb && j->hMax == 2 && j->comp[0].h == 2 && j->comp[1].h == 1 && j->comp[2].h == 1;
        for (c = 0; c < j->comps; c++) {
            JCOMP *p = &j->comp[c];
            BYTE  *src = p->rows + (UINT)(y * p->v / j->vMax) * p->stride;
            if (p->h != j->hMax && !half) {
                Widen(src, j->up[c], w, p->h, j->hMax);
                src = j->up[c];
            }
            j->line[c] = src;
        }
        if (j->comps == 1) {
            const BYTE *a = j->line[0];
            for (x = 0; x < w; x++, to += 3)
                to[0] = to[1] = to[2] = a[x];
        } else if (rgb) {
            const BYTE *r = j->line[0], *g = j->line[1], *b = j->line[2];
            for (x = 0; x < w; x++, to += 3) {
                to[0] = b[x];
                to[1] = g[x];
                to[2] = r[x];
            }
        } else if (half) {              /* the usual: a colour for two pixels, worked out once */
            const BYTE *a = j->line[0], *b = j->line[1], *r = j->line[2], *range = j->range + 256;
            for (x = 0; x < w; x += 2, to += 6, a += 2) {
                int cb = *b++, cr = *r++, db = j->cbB[cb], dg = j->cbG[cb] + j->crG[cr], dr = j->crR[cr], l = a[0];
                to[0] = range[l + db];
                to[1] = range[l + dg];
                to[2] = range[l + dr];
                if (x + 1 < w) {
                    l = a[1];
                    to[3] = range[l + db];
                    to[4] = range[l + dg];
                    to[5] = range[l + dr];
                }
            }
        } else {
            const BYTE *a = j->line[0], *b = j->line[1], *r = j->line[2], *range = j->range + 256;
            for (x = 0; x < w; x++, to += 3) {
                int l = a[x], cb = b[x], cr = r[x];
                to[0] = range[l + j->cbB[cb]];
                to[1] = range[l + j->cbG[cb] + j->crG[cr]];
                to[2] = range[l + j->crR[cr]];
            }
        }
        if (!ResampleRow(im->rs, j->bgr))
            return FALSE;
    }
    return TRUE;
}

/* ---- scans ---- */

/* A plain scan of everything: straight through. */
static void ScanThrough(JPEG *j)
{
    IMAGE *im = j->im;
    int   my, mx, s, by, bx;

    KeptQuant(j);
    JpegScanStart(j);
    for (my = 0; my < j->mcusY; my++) {
        for (mx = 0; mx < j->mcusX; mx++) {
            if (j->restart && !j->left)
                JpegRestart(j);
            j->left--;
            for (s = 0; s < j->scanComps; s++) {
                JCOMP *c = &j->comp[j->scanComp[s]];
                for (by = 0; by < c->v; by++)
                    for (bx = 0; bx < c->h; bx++) {
                        _fmemset(j->block, 0, j->nn * 2);
                        JpegBlockPlain(j, c, j->block);
                        JpegIdct(j->block, j->quantKept[c->tq], j->n,
                                 c->rows + (UINT)by * j->n * c->stride + (UINT)(mx * c->h + bx) * j->n, c->stride);
                    }
            }
        }
        if (!GiveRows(j, my) || !ImageTick(im, ImagePercent(im)))
            return;
    }
}

static void HoldBlock(JPEG *j, JCOMP *c, DWORD at)
{
    short *b = (short *)(c->coef + at * j->nn);

    if (j->progressive)
        JpegBlockProgressive(j, c, b, c->nonzero ? (BYTE *)(c->nonzero + at * 8) : NULL);
    else
        JpegBlockPlain(j, c, b);
}

/* A scan into the store. */
static void ScanHeld(JPEG *j)
{
    IMAGE *im = j->im;
    int   my, mx, s, by, bx;

    JpegScanStart(j);
    if (j->scanComps == 1) {            /* its own blocks, not whole MCUs */
        JCOMP *c = &j->comp[j->scanComp[0]];
        for (by = 0; by < c->scanH; by++) {
            for (bx = 0; bx < c->scanW; bx++) {
                if (j->restart && !j->left)
                    JpegRestart(j);
                j->left--;
                HoldBlock(j, c, (DWORD)by * c->blocksW + bx);
            }
            if (!ImageTick(im, ImagePercent(im) * 9 / 10))
                return;
        }
        return;
    }
    for (my = 0; my < j->mcusY; my++) {
        for (mx = 0; mx < j->mcusX; mx++) {
            if (j->restart && !j->left)
                JpegRestart(j);
            j->left--;
            for (s = 0; s < j->scanComps; s++) {
                JCOMP *c = &j->comp[j->scanComp[s]];
                for (by = 0; by < c->v; by++)
                    for (bx = 0; bx < c->h; bx++)
                        HoldBlock(j, c, (DWORD)(my * c->v + by) * c->blocksW + mx * c->h + bx);
            }
        }
        if (!ImageTick(im, ImagePercent(im) * 9 / 10))
            return;
    }
}

/* All scans read: the blocks held to pixels. */
static void FromHeld(JPEG *j)
{
    IMAGE *im = j->im;
    int   my, i, by, bx;

    KeptQuant(j);
    for (my = 0; my < j->mcusY; my++) {
        for (i = 0; i < j->comps; i++) {
            JCOMP *c = &j->comp[i];
            for (by = 0; by < c->v; by++)
                for (bx = 0; bx < c->blocksW; bx++) {
                    DWORD at = (DWORD)(my * c->v + by) * c->blocksW + bx;
                    JpegIdct((short *)(c->coef + at * j->nn), j->quantKept[c->tq], j->n,
                             c->rows + (UINT)by * j->n * c->stride + (UINT)bx * j->n, c->stride);
                }
        }
        if (!GiveRows(j, my) || !ImageTick(im, 90 + (int)((long)my * 10 / j->mcusY)))
            return;
    }
}

static int Run(IMAGE *im)
{
    JPEG *j = (JPEG *)im->reader;

    ColorTables(j);
    for (;;) {
        int  m = JpegNextMarker(j);
        long n;

        if (im->stopped)
            return IMAGE_OK;
        if (m == M_EOI) {
            if (j->stored)
                FromHeld(j);
            return IMAGE_OK;
        }
        if (m == M_SOI || (m >= M_RST0 && m <= M_RST7))
            continue;
        n = (long)Word(im) - 2;
        if (n < 0 || im->ended) {
            if (j->stored)
                FromHeld(j);
            return IMAGE_OK;
        }
        switch (m) {
        case M_DQT:
            ReadQuant(j, n);
            break;
        case M_DHT:
            ReadHuff(j, n);
            break;
        case M_DRI:
            j->restart = (UINT)Word(im);
            ImageSkip(im, n - 2);
            break;
        case M_SOS:
            if (!ReadScanHeader(j, n))
                return IMAGE_DAMAGED;
            if (!j->progressive && !j->stored && j->scanComps == j->comps) {
                ScanThrough(j);
                return IMAGE_OK;
            }
            if (!j->stored && !AllocStore(j))
                return IMAGE_MEMORY;
            if (!j->progressive || j->n > 1 || j->ss == 0)      /* at 1/8 only the first coefficient */
                ScanHeld(j);
            break;
        default:
            ImageSkip(im, n);
        }
    }
}

static void CloseJpeg(IMAGE *im)
{
    JPEG *j = (JPEG *)im->reader;

    FreeStore(j);
    FreeRows(j);
    ImageFree(j);
}

/* After SOI: up to the frame's header, which gives the size. */
int JpegOpen(IMAGE *im)
{
    JPEG *j = (JPEG *)ImageAlloc(sizeof *j);

    if (!j)
        return IMAGE_MEMORY;
    j->im = im;
    im->reader = j;
    im->prepare = Prepare;
    im->run = Run;
    im->close = CloseJpeg;
    for (;;) {
        int  m = JpegNextMarker(j);
        long n;

        if (m == M_EOI || m == M_SOS || im->ended)
            return IMAGE_DAMAGED;
        if (m == M_SOI || (m >= M_RST0 && m <= M_RST7))
            continue;
        n = (long)Word(im) - 2;
        if (n < 0)
            return IMAGE_DAMAGED;
        switch (m) {
        case M_SOF2:
            j->progressive = TRUE;
            /* fall through */
        case M_SOF0:
        case M_SOF1:
            return ReadFrame(j, n);
        case M_DQT:
            ReadQuant(j, n);
            break;
        case M_DHT:
            ReadHuff(j, n);
            break;
        case M_DRI:
            j->restart = (UINT)Word(im);
            ImageSkip(im, n - 2);
            break;
        case M_APP0:
            j->jfif = TRUE;
            ImageSkip(im, n);
            break;
        case M_APP1:
            ReadExif(j, n);
            break;
        case M_APP14:
            ReadApp14(j, n);
            break;
        default:
            if (m >= 0xC3 && m <= 0xCF && m != 0xC8 && m != 0xCC)
                return IMAGE_DAMAGED;   /* lossless, arithmetic: not read */
            ImageSkip(im, n);
        }
    }
}
