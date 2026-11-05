/*
 * PNG files: every colour type and depth, transparency over the colour
 * asked for, interlaced ones too. Rows are unpacked and given on as they
 * leave inflate.c. An interlaced file gives its rows in seven passes over
 * the whole picture, so that one alone is held whole.
 */
#include "internal.h"
#include "imageint.h"

BOOL InflateRun(int (*next)(void *user), BOOL (*sink)(void *user, const BYTE *data, UINT n), void *user);

typedef struct {
    IMAGE *im;
    int   type, depth;
    BOOL  interlaced;
    UINT  pixelBytes;                   /* at least 1 */
    DWORD left;                         /* of this IDAT */
    BOOL  noMore;
    BYTE  colors[256][4];               /* B, G, R, alpha */
    BOOL  keyed;                        /* tRNS for grey or RGB: this colour is transparent */
    WORD  keyR, keyG, keyB;
    BYTE  *cur, *prev, *bgr;
    UINT  need, have;                   /* the row: its filter byte and bytes */
    int   pass, passW, passH, passY;
    BYTE  huge *full;
    BOOL  finished;
} PNG;

static const BYTE StartX[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const BYTE StartY[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const BYTE StepX[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const BYTE StepY[7] = { 8, 8, 8, 4, 4, 2, 2 };

static DWORD Be32(IMAGE *im)
{
    DWORD v = 0;
    int   i;

    for (i = 0; i < 4; i++)
        v = v << 8 | (BYTE)ImageByte(im);
    return v;
}

static int Channels(int type)
{
    return type == 2 ? 3 : type == 4 ? 2 : type == 6 ? 4 : 1;
}

static UINT RowBytes(const PNG *p, int w)
{
    return (UINT)(((DWORD)w * Channels(p->type) * p->depth + 7) / 8);
}

/* The next pass with something in it; FALSE: all done. */
static BOOL NextPass(PNG *p)
{
    IMAGE *im = p->im;

    if (!p->interlaced) {
        if (p->pass++)
            return FALSE;
        p->passW = im->w;
        p->passH = im->h;
    } else {
        do {
            if (++p->pass > 7)
                return FALSE;
            p->passW = (im->w - StartX[p->pass - 1] + StepX[p->pass - 1] - 1) / StepX[p->pass - 1];
            p->passH = (im->h - StartY[p->pass - 1] + StepY[p->pass - 1] - 1) / StepY[p->pass - 1];
        } while (p->passW <= 0 || p->passH <= 0);
    }
    p->need = RowBytes(p, p->passW) + 1;
    p->have = 0;
    p->passY = 0;
    _fmemset(p->prev, 0, p->need);
    return TRUE;
}

/* The filters over n bytes, every byte of the picture through one of them:
   c the row, u the row above, back -(bytes of a pixel). */
void FilterUp(BYTE *c, const BYTE *u, UINT n);
#pragma aux FilterUp =  \
    "push ds"           \
    "mov ds,dx"         \
    "next:"             \
    "mov al,es:[di]"    \
    "add [si],al"       \
    "inc si"            \
    "inc di"            \
    "loop next"         \
    "pop ds"            \
    parm [dx si] [es di] [cx] modify exact [ax cx si di];

void FilterSub(BYTE *c, UINT n, int back);
#pragma aux FilterSub = \
    "push ds"           \
    "mov ds,dx"         \
    "next:"             \
    "mov al,[si+bx]"    \
    "add [si],al"       \
    "inc si"            \
    "loop next"         \
    "pop ds"            \
    parm [dx si] [cx] [bx] modify exact [ax cx si];

void FilterAverage(BYTE *c, const BYTE *u, UINT n, int back);
#pragma aux FilterAverage = \
    "push ds"           \
    "mov ds,dx"         \
    "next:"             \
    "mov al,[si+bx]"    \
    "xor ah,ah"         \
    "mov dl,es:[di]"    \
    "xor dh,dh"         \
    "add ax,dx"         \
    "shr ax,1"          \
    "add [si],al"       \
    "inc si"            \
    "inc di"            \
    "loop next"         \
    "pop ds"            \
    parm [dx si] [es di] [cx] [bx] modify exact [ax cx dx si di];

void FilterPaeth(BYTE *c, const BYTE *u, UINT n, int back);
#pragma aux FilterPaeth = \
    "push ds"           \
    "push bp"           \
    "mov ds,dx"         \
    "next:"             \
    "mov al,es:[di]"    \
    "xor ah,ah"         \
    "mov dl,es:[di+bx]" \
    "xor dh,dh"         \
    "sub ax,dx"         \
    "mov bp,ax"         \
    "mov al,[si+bx]"    \
    "xor ah,ah"         \
    "sub ax,dx"         \
    "mov dx,ax"         \
    "add ax,bp"         \
    "jns a1"            \
    "neg ax"            \
    "a1:"               \
    "or dx,dx"          \
    "jns a2"            \
    "neg dx"            \
    "a2:"               \
    "or bp,bp"          \
    "jns a3"            \
    "neg bp"            \
    "a3:"               \
    "cmp bp,dx"         \
    "ja notleft"        \
    "cmp bp,ax"         \
    "ja notleft"        \
    "mov al,[si+bx]"    \
    "jmp done"          \
    "notleft:"          \
    "cmp dx,ax"         \
    "ja corner"         \
    "mov al,es:[di]"    \
    "jmp done"          \
    "corner:"           \
    "mov al,es:[di+bx]" \
    "done:"             \
    "add [si],al"       \
    "inc si"            \
    "inc di"            \
    "dec cx"            \
    "jnz next"          \
    "pop bp"            \
    "pop ds"            \
    parm [dx si] [es di] [cx] [bx] modify exact [ax cx dx si di];

static void Unfilter(PNG *p)
{
    BYTE *c = p->cur + 1, *u = p->prev + 1;
    UINT n = p->need - 1, b = p->pixelBytes, i;

    if (!n)
        return;
    switch (p->cur[0]) {
    case 1:
        if (n > b)
            FilterSub(c + b, n - b, -(int)b);
        break;
    case 2:
        FilterUp(c, u, n);
        break;
    case 3:
        for (i = 0; i < b && i < n; i++)
            c[i] += (BYTE)(u[i] >> 1);
        if (n > b)
            FilterAverage(c + b, u + b, n - b, -(int)b);
        break;
    case 4:
        for (i = 0; i < b && i < n; i++)
            c[i] += u[i];
        if (n > b)
            FilterPaeth(c + b, u + b, n - b, -(int)b);
        break;
    }
}

/* Sample x of a row of depth 1, 2 or 4. */
static UINT Small(const BYTE *row, int x, int depth)
{
    UINT bit = (UINT)x * depth;

    return (row[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
}

static void Over(const PNG *p, BYTE *to, int b, int g, int r, int alpha)
{
    if (alpha == 255) {
        to[0] = (BYTE)b;
        to[1] = (BYTE)g;
        to[2] = (BYTE)r;
    } else {
        COLORREF back = p->im->back;
        int      a = 255 - alpha;
        to[0] = (BYTE)((b * alpha + GetBValue(back) * a + 127) / 255);
        to[1] = (BYTE)((g * alpha + GetGValue(back) * a + 127) / 255);
        to[2] = (BYTE)((r * alpha + GetRValue(back) * a + 127) / 255);
    }
}

/* A row of the file's samples as B, G, R. */
static void ToBgr(PNG *p, int w)
{
    const BYTE *s = p->cur + 1;
    BYTE *to = p->bgr;
    int  x, d = p->depth;

    for (x = 0; x < w; x++, to += 3) {
        int v, r, g, b, alpha = 255;
        switch (p->type) {
        case 0:
            if (d == 16) {
                v = s[x * 2];
                if (p->keyed && (WORD)(s[x * 2] << 8 | s[x * 2 + 1]) == p->keyG)
                    alpha = 0;
            } else {
                UINT raw = d == 8 ? s[x] : Small(s, x, d);
                v = d == 8 ? (int)raw : (int)(raw * 255 / ((1 << d) - 1));
                if (p->keyed && raw == p->keyG)
                    alpha = 0;
            }
            Over(p, to, v, v, v, alpha);
            break;
        case 2:
            if (d == 8) {
                const BYTE *c = s + (UINT)x * 3;
                r = c[0];
                g = c[1];
                b = c[2];
                if (p->keyed && r == (int)p->keyR && g == (int)p->keyG && b == (int)p->keyB)
                    alpha = 0;
            } else {
                const BYTE *c = s + (UINT)x * 6;
                r = c[0];
                g = c[2];
                b = c[4];
                if (p->keyed && (WORD)(c[0] << 8 | c[1]) == p->keyR && (WORD)(c[2] << 8 | c[3]) == p->keyG &&
                    (WORD)(c[4] << 8 | c[5]) == p->keyB)
                    alpha = 0;
            }
            Over(p, to, b, g, r, alpha);
            if (p->im->rgbRows) {       /* the other rows of this picture are R, G, B */
                BYTE first = to[0];
                to[0] = to[2];
                to[2] = first;
            }
            break;
        case 3: {
            const BYTE *c = p->colors[d == 8 ? s[x] : Small(s, x, d)];
            Over(p, to, c[0], c[1], c[2], c[3]);
            break;
        }
        case 4:
            v = d == 8 ? s[x * 2] : s[x * 4];
            Over(p, to, v, v, v, d == 8 ? s[x * 2 + 1] : s[x * 4 + 2]);
            break;
        default:
            if (d == 8) {
                const BYTE *c = s + (UINT)x * 4;
                if (c[3] == 255) {
                    to[0] = c[2];
                    to[1] = c[1];
                    to[2] = c[0];
                } else {
                    Over(p, to, c[2], c[1], c[0], c[3]);
                }
            } else {
                const BYTE *c = s + (UINT)x * 8;
                Over(p, to, c[4], c[2], c[0], c[6]);
            }
        }
    }
}

/* A row whole: given on, or put in its place among the others. */
static BOOL RowDone(PNG *p)
{
    IMAGE *im = p->im;
    BYTE  *swap;

    Unfilter(p);
    if (!im->rgbRows || p->keyed)
        ToBgr(p, p->passW);
    if (p->interlaced) {
        int  i = p->pass - 1, x;
        DWORD y = (DWORD)StartY[i] + (DWORD)p->passY * StepY[i];
        BYTE huge *to = p->full + (y * im->w + StartX[i]) * 3;
        const BYTE *from = p->bgr;
        for (x = 0; x < p->passW; x++, from += 3, to += StepX[i] * 3) {
            to[0] = from[0];
            to[1] = from[1];
            to[2] = from[2];
        }
    } else if (!ResampleRow(im->rs, im->rgbRows && !p->keyed ? p->cur + 1 : p->bgr)) {
        return FALSE;
    }
    swap = p->cur;
    p->cur = p->prev;
    p->prev = swap;
    p->have = 0;
    if (++p->passY >= p->passH && !NextPass(p)) {
        p->finished = TRUE;
        return FALSE;
    }
    return ImageTick(im, ImagePercent(im));
}

static BOOL Sink(void *user, const BYTE *data, UINT n)
{
    PNG *p = (PNG *)user;

    while (n) {
        UINT take = min(n, p->need - p->have);
        _fmemcpy(p->cur + p->have, data, take);
        p->have += take;
        data += take;
        n -= take;
        if (p->have == p->need && !RowDone(p))
            return FALSE;
    }
    return TRUE;
}

/* The next byte of the IDAT chunks, one after another. */
static int Next(void *user)
{
    PNG   *p = (PNG *)user;
    IMAGE *im = p->im;

    while (!p->left) {
        DWORD type;
        if (p->noMore)
            return -1;
        ImageSkip(im, 4);               /* the chunk's CRC */
        p->left = Be32(im);
        type = Be32(im);
        if (im->ended || type != 0x49444154L) {
            p->left = 0;
            p->noMore = TRUE;
            return -1;
        }
    }
    p->left--;
    return ImageByte(im);
}

static void ReadPalette(PNG *p, DWORD n)
{
    IMAGE *im = p->im;
    UINT  i;

    for (i = 0; i < n / 3 && i < 256; i++) {
        p->colors[i][2] = (BYTE)ImageByte(im);
        p->colors[i][1] = (BYTE)ImageByte(im);
        p->colors[i][0] = (BYTE)ImageByte(im);
    }
    ImageSkip(im, n - (DWORD)i * 3);
}

static void ReadTransparent(PNG *p, DWORD n)
{
    IMAGE *im = p->im;
    UINT  i;

    if (p->type == 3) {
        for (i = 0; i < n && i < 256; i++)
            p->colors[i][3] = (BYTE)ImageByte(im);
        ImageSkip(im, n - i);
    } else if ((p->type == 0 && n >= 2) || (p->type == 2 && n >= 6)) {
        WORD v[3];
        for (i = 0; i < (p->type ? 3U : 1U); i++) {
            int hi = ImageByte(im), lo = ImageByte(im);
            v[i] = (WORD)((hi & 255) << 8 | (lo & 255));
        }
        p->keyed = TRUE;
        p->keyR = v[0];
        p->keyG = p->type ? v[1] : v[0];
        p->keyB = v[2];
        ImageSkip(im, n - (p->type ? 6 : 2));
    } else {
        ImageSkip(im, n);
    }
}

static int Prepare(IMAGE *im, int *reduce)
{
    PNG  *p = (PNG *)im->reader;
    UINT need = RowBytes(p, im->w) + 1;

    *reduce = 1;
    im->rowW = im->w;
    im->rowH = im->h;
    im->rgbRows = p->type == 2 && p->depth == 8 && !p->interlaced;
    p->cur = (BYTE *)ImageAlloc(need);
    p->prev = (BYTE *)ImageAlloc(need);
    p->bgr = (BYTE *)ImageAlloc((DWORD)im->w * 3);
    if (!p->cur || !p->prev || !p->bgr)
        return IMAGE_MEMORY;
    if (p->interlaced && (p->full = (BYTE huge *)ImageAlloc((DWORD)im->w * im->h * 3)) == NULL)
        return IMAGE_MEMORY;
    return IMAGE_OK;
}

static int Run(IMAGE *im)
{
    PNG   *p = (PNG *)im->reader;
    DWORD n, type;
    int   y;

    for (;;) {                          /* up to the first IDAT */
        n = Be32(im);
        type = Be32(im);
        if (im->ended || type == 0x49454E44L)           /* IEND */
            return IMAGE_DAMAGED;
        if (type == 0x49444154L)        /* IDAT */
            break;
        if (type == 0x504C5445L)        /* PLTE */
            ReadPalette(p, n);
        else if (type == 0x74524E53L)   /* tRNS */
            ReadTransparent(p, n);
        else
            ImageSkip(im, n);
        ImageSkip(im, 4);
    }
    p->left = n;
    if (!NextPass(p))
        return IMAGE_DAMAGED;
    if (!InflateRun(Next, Sink, p))
        return IMAGE_MEMORY;
    if (p->interlaced && !im->stopped) {
        DWORD pitch = (DWORD)im->w * 3;
        for (y = 0; y < im->h; y++) {
            hmemcpy(p->bgr, p->full + y * pitch, (long)pitch);
            if (!ResampleRow(im->rs, p->bgr))
                break;
        }
    }
    return IMAGE_OK;
}

static void ClosePng(IMAGE *im)
{
    PNG *p = (PNG *)im->reader;

    ImageFree(p->cur);
    ImageFree(p->prev);
    ImageFree(p->bgr);
    ImageFree((void *)p->full);
    ImageFree(p);
}

/* After the signature's first two bytes: the header chunk. */
int PngOpen(IMAGE *im)
{
    PNG   *p = (PNG *)ImageAlloc(sizeof *p);
    DWORD w, h;
    int   i;

    if (!p)
        return IMAGE_MEMORY;
    p->im = im;
    im->reader = p;
    im->prepare = Prepare;
    im->run = Run;
    im->close = ClosePng;
    for (i = 0; i < 256; i++)
        p->colors[i][3] = 255;
    ImageSkip(im, 6);
    if (Be32(im) != 13 || Be32(im) != 0x49484452L)      /* IHDR */
        return IMAGE_DAMAGED;
    w = Be32(im);
    h = Be32(im);
    p->depth = (BYTE)ImageByte(im);
    p->type = (BYTE)ImageByte(im);
    ImageSkip(im, 2);
    p->interlaced = ImageByte(im) == 1;
    ImageSkip(im, 4);
    if (im->ended || !w || !h || w > 20000 || h > 32767 || (p->type != 0 && p->type != 2 && p->type != 3 &&
        p->type != 4 && p->type != 6) || (p->depth != 1 && p->depth != 2 && p->depth != 4 && p->depth != 8 &&
        p->depth != 16) || (p->type == 3 && p->depth == 16) || (p->type != 0 && p->type != 3 && p->depth < 8))
        return IMAGE_DAMAGED;
    im->w = (int)w;
    im->h = (int)h;
    if ((DWORD)w * Channels(p->type) * p->depth / 8 + 2 > IMAGE_ROW_MAX)
        return IMAGE_DAMAGED;
    p->pixelBytes = max((UINT)(Channels(p->type) * p->depth / 8), 1);
    return IMAGE_OK;
}
