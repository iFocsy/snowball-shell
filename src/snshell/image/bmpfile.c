/* BMP files, uncompressed, a row at a time (Windows' own wallpaper loader
   takes the whole file, however large). */
#include "internal.h"
#include "imageint.h"

typedef struct {
    DWORD bits;                         /* where the rows start */
    BOOL  topDown;
    int   depth;
    BOOL  plain;                        /* no compression */
    UINT  rowBytes;
    BYTE  colors[256][3];               /* B, G, R */
    BYTE  *raw, *bgr;
} BMP;

static DWORD Le(IMAGE *im, int bytes)
{
    DWORD v = 0;
    int   i;

    for (i = 0; i < bytes; i++)
        v |= (DWORD)(BYTE)ImageByte(im) << (i * 8);
    return v;
}

static int Prepare(IMAGE *im, int *reduce)
{
    BMP *b = (BMP *)im->reader;

    *reduce = 1;
    if (!b->plain)
        return IMAGE_DAMAGED;
    im->rowW = im->w;
    im->rowH = im->h;
    b->raw = (BYTE *)ImageAlloc(b->rowBytes);
    b->bgr = (BYTE *)ImageAlloc((DWORD)im->w * 3);
    return b->raw && b->bgr ? IMAGE_OK : IMAGE_MEMORY;
}

static void ToBgr(const BMP *b, int w)
{
    BYTE *to = b->bgr;
    int  x;

    for (x = 0; x < w; x++, to += 3) {
        const BYTE *c;
        int i;
        if (b->depth == 32) {
            c = b->raw + (UINT)x * 4;
        } else {
            i = b->depth == 8 ? b->raw[x] :
                b->depth == 4 ? (x & 1 ? b->raw[x / 2] & 15 : b->raw[x / 2] >> 4) :
                (b->raw[x / 8] >> (7 - (x & 7))) & 1;
            c = b->colors[i];
        }
        to[0] = c[0];
        to[1] = c[1];
        to[2] = c[2];
    }
}

static int Run(IMAGE *im)
{
    BMP *b = (BMP *)im->reader;
    int y;

    for (y = 0; y < im->h; y++) {
        DWORD row = b->topDown ? y : im->h - 1 - y;
        _llseek(im->file, (LONG)(b->bits + row * b->rowBytes), 0);
        if (_lread(im->file, b->raw, b->rowBytes) != b->rowBytes)
            break;                      /* cut short: what was read shows */
        if (b->depth != 24)
            ToBgr(b, im->w);
        if (!ResampleRow(im->rs, b->depth == 24 ? b->raw : b->bgr) ||
            !ImageTick(im, (int)((long)y * 100 / im->h)))
            break;
    }
    return IMAGE_OK;
}

static void CloseBmp(IMAGE *im)
{
    BMP *b = (BMP *)im->reader;

    ImageFree(b->raw);
    ImageFree(b->bgr);
    ImageFree(b);
}

/* After "BM". */
int BmpOpen(IMAGE *im)
{
    BMP   *b;
    DWORD header, used, compression, i;
    long  w, h;

    ImageSkip(im, 8);
    if ((b = (BMP *)ImageAlloc(sizeof *b)) == NULL)
        return IMAGE_MEMORY;
    im->reader = b;
    im->prepare = Prepare;
    im->run = Run;
    im->close = CloseBmp;
    b->bits = Le(im, 4);
    header = Le(im, 4);
    w = (long)Le(im, 4);
    h = (long)Le(im, 4);
    Le(im, 2);
    b->depth = (int)Le(im, 2);
    compression = Le(im, 4);
    Le(im, 4);
    Le(im, 4);
    Le(im, 4);
    used = Le(im, 4);
    Le(im, 4);
    if (im->ended || header < 40 || w <= 0 || w > 20000 || h == 0 || h > 32767 || h < -32767)
        return IMAGE_DAMAGED;
    b->topDown = h < 0;
    im->w = (int)w;
    im->h = (int)(h < 0 ? -h : h);
    b->plain = compression == BI_RGB && (b->depth == 1 || b->depth == 4 || b->depth == 8 || b->depth == 24 ||
                                         b->depth == 32);
    b->rowBytes = (UINT)(((DWORD)w * b->depth + 31) / 32 * 4);
    if (b->depth <= 8) {
        if (!used || used > 256)
            used = 1L << b->depth;
        ImageSkip(im, header - 40);
        for (i = 0; i < used; i++) {
            b->colors[(UINT)i][0] = (BYTE)ImageByte(im);
            b->colors[(UINT)i][1] = (BYTE)ImageByte(im);
            b->colors[(UINT)i][2] = (BYTE)ImageByte(im);
            ImageByte(im);
        }
    }
    return IMAGE_OK;
}
