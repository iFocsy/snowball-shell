/*
 * Rows of a picture made another size as they come. Made smaller, each new
 * pixel is the average of the area it covers, parts of pixels at its edges
 * counted by how much of them it covers (no pixels dropped: thin lines and
 * fine patterns stay as they are, smaller). Made larger, it is between its
 * two nearest pixels. Across and down are done separately, so a picture may
 * do one of each.
 *
 * Each new pixel's weights add up to 256: a weighted sum of bytes then fits
 * 16 bits, and needs no division. This is the longest part of making a
 * wallpaper, and 32-bit sums here took twice as long as reading the JPEG.
 */
#include "internal.h"
#include "imageint.h"

typedef struct {
    UINT  first;                        /* the offset of the first source pixel it takes */
    WORD  between;                      /* whole pixels after that one, */
    WORD  mid;                          /* their weight: 256 * 256 / area, with 8 bits after the point */
    WORD  wFirst, wLast;                /* the weights of the pixels at the ends; wLast 0: just one */
} SPAN;

struct RESAMPLE {
    RESAMPLEJOB j;
    SPAN   *spans;
    BOOL   shrinkX, shrinkY, same;
    BYTE   *now, *before;               /* a row made the width asked for */
    WORD   *sum;                        /* shrinking: the row being added up, 8 bits after the point */
    BYTE   *out;
    int    y;                           /* the next row made */
    int    srcY;                        /* the next row given */
    int    seen;                        /* the rows of the part seen */
    DWORD  from, to, step;              /* shrinking: this row's edges, in 1/256 of a row */
    DWORD  rest, restStep;
    UINT   given;                       /* of its 256 */
};

/* sum[i] += row[i] * weight. */
void AddRow(WORD *sum, const BYTE *row, UINT n, UINT weight);
#pragma aux AddRow =    \
    "push ds"           \
    "mov ds,dx"         \
    "next:"             \
    "mov al,[si]"       \
    "xor ah,ah"         \
    "mul bx"            \
    "add es:[di],ax"    \
    "inc si"            \
    "add di,2"          \
    "loop next"         \
    "pop ds"            \
    parm [es di] [dx si] [cx] [bx] modify exact [ax cx dx si di];

/* out[i] = sum[i] rounded to a byte, and sum[i] cleared. */
void TakeRow(BYTE *out, WORD *sum, UINT n);
#pragma aux TakeRow =   \
    "push ds"           \
    "mov ds,dx"         \
    "next:"             \
    "mov ax,[si]"       \
    "mov word ptr [si],0" \
    "add ax,128"        \
    "mov es:[di],ah"    \
    "add si,2"          \
    "inc di"            \
    "loop next"         \
    "pop ds"            \
    parm [es di] [dx si] [cx] modify exact [ax cx si di];

/* The output's x covers [a, b) of the source, in 1/256 of a pixel: Bresenham's
   way, so no product overflows, and the weights made to add up to 256. */
static void MakeSpans(RESAMPLE *r)
{
    DWORD whole = (DWORD)r->j.cropW << 8, step = whole / r->j.outW, more = whole % r->j.outW;
    DWORD a = 0, b, rest = 0;
    int   x;

    for (x = 0; x < r->j.outW; x++, a = b) {
        SPAN *s = &r->spans[x];
        DWORD area, first, last;
        b = a + step;
        if ((rest += more) >= (DWORD)r->j.outW) {
            rest -= r->j.outW;
            b++;
        }
        area = b - a;
        first = a >> 8;
        last = (b - 1) >> 8;
        s->first = (UINT)first * 3;
        if (first == last) {
            s->wFirst = 256;
            continue;
        }
        s->wFirst = (WORD)((256 - (a & 255)) * 256 / area);
        s->wLast = (WORD)max(((b & 255) ? (b & 255) : 256) * 256 / area, 1);    /* 0 means one pixel only */
        if (last - first > 1) {
            s->between = (WORD)min(last - first - 1, 256);
            s->mid = (WORD)min(16777216L / area, 65535L);
        }
        /* rounded up: over 256 in all, a white pixel would pass 16 bits and come out black */
        s->wFirst += (WORD)(256 - s->wFirst - s->wLast - (WORD)(((DWORD)s->mid * s->between + 255) >> 8));
    }
}

/* Made larger: where each pixel lies, once. */
static void MakeGrowth(RESAMPLE *r)
{
    int x;

    for (x = 0; x < r->j.outW; x++) {
        long at = (2L * x + 1) * ((long)r->j.cropW << 7) / r->j.outW - 128;
        SPAN *s = &r->spans[x];
        UINT i = at < 0 ? 0 : (UINT)(at >> 8);
        s->first = i * 3;
        s->wLast = at < 0 ? 0 : (WORD)(at & 255);
        s->between = i + 1 < (UINT)r->j.cropW ? 3 : 0;          /* to the pixel after */
    }
}

/* Made smaller across: one row of source pixels. */
static void ShrinkRow(RESAMPLE *r, const BYTE *row)
{
    const SPAN *s = r->spans;
    BYTE *to = r->now;
    int  x, w = r->j.outW, one = r->j.rgb ? 2 : 0, other = 2 - one;

    for (x = 0; x < w; x++, s++, to += 3) {
        const BYTE *p = row + s->first;
        UINT wf = s->wFirst, wl = s->wLast, c0 = p[0] * wf + 128, c1 = p[1] * wf + 128, c2 = p[2] * wf + 128;

        if (wl) {
            if (s->between) {
                UINT a0 = 0, a1 = 0, a2 = 0, n = s->between, mid = s->mid;
                do {
                    p += 3;
                    a0 += p[0];
                    a1 += p[1];
                    a2 += p[2];
                } while (--n);
                c0 += (UINT)(MulWide(a0, mid) >> 8);
                c1 += (UINT)(MulWide(a1, mid) >> 8);
                c2 += (UINT)(MulWide(a2, mid) >> 8);
            }
            p += 3;
            c0 += p[0] * wl;
            c1 += p[1] * wl;
            c2 += p[2] * wl;
        }
        to[one] = (BYTE)(c0 >> 8);
        to[1] = (BYTE)(c1 >> 8);
        to[other] = (BYTE)(c2 >> 8);
    }
}

/* Made larger across: each pixel between its two nearest. */
static void GrowRow(RESAMPLE *r, const BYTE *row)
{
    const SPAN *s = r->spans;
    BYTE *to = r->now;
    int  x, one = r->j.rgb ? 2 : 0, other = 2 - one;

    for (x = 0; x < r->j.outW; x++, s++, to += 3) {
        const BYTE *p = row + s->first;
        UINT f = s->wLast, g = 256 - f, n = s->between;

        to[one] = (BYTE)((p[0] * g + p[n] * f + 128) >> 8);
        to[1] = (BYTE)((p[1] * g + p[n + 1] * f + 128) >> 8);
        to[other] = (BYTE)((p[2] * g + p[n + 2] * f + 128) >> 8);
    }
}

/* Where EXIF's turn puts the row made as y. */
static void Put(RESAMPLE *r, int y, const BYTE *row)
{
    const RESAMPLEJOB *j = &r->j;
    int  x, w = j->outW, h = j->outH, o = j->orient;

    if (o < 5) {
        int  at = o == 3 || o == 4 ? h - 1 - y : y;             /* from the top */
        BYTE huge *to = j->bits + (DWORD)(j->dibH - 1 - at) * j->pitch;
        if (o == 2 || o == 3) {
            for (x = 0; x < w; x++) {
                const BYTE *p = row + (UINT)(w - 1 - x) * 3;
                to[0] = p[0];
                to[1] = p[1];
                to[2] = p[2];
                to += 3;
            }
        } else {
            hmemcpy(to, row, (long)w * 3);
        }
        return;
    }
    for (x = 0; x < w; x++, row += 3) {                         /* a row becomes a column */
        int  col = o == 5 || o == 8 ? y : h - 1 - y;
        int  at = o == 5 || o == 6 ? x : w - 1 - x;
        BYTE huge *to = j->bits + (DWORD)(j->dibH - 1 - at) * j->pitch + (DWORD)col * 3;
        to[0] = row[0];
        to[1] = row[1];
        to[2] = row[2];
    }
}

/* The edges of the next row made. */
static void NextEdges(RESAMPLE *r)
{
    r->from = r->to;
    r->to += r->step;
    if ((r->rest += r->restStep) >= (DWORD)r->j.outH) {
        r->rest -= r->j.outH;
        r->to++;
    }
    r->given = 0;
}

/* Made smaller down: the row seen, of the part, adds to the row or two it is in. */
static void ShrinkDown(RESAMPLE *r)
{
    UINT  n = (UINT)r->j.outW * 3;
    DWORD top = (DWORD)r->seen << 8, bottom = top + 256;

    while (r->y < r->j.outH) {
        DWORD end = min(bottom, r->to);
        if (end >= r->to) {             /* its last part: what is left of the 256 */
            AddRow(r->sum, r->now, n, 256 - r->given);
            TakeRow(r->out, r->sum, n);
            Put(r, r->y++, r->out);
            top = end;
            if (r->y < r->j.outH)
                NextEdges(r);
            if (top >= bottom)
                return;
        } else {
            UINT w = (UINT)((end - top) * 256 / (r->to - r->from));
            AddRow(r->sum, r->now, n, w);
            r->given += w;
            return;
        }
    }
}

/* Between two rows made. */
static void Blend(RESAMPLE *r, const BYTE *a, const BYTE *b, UINT f)
{
    UINT i, n = (UINT)r->j.outW * 3, g = 256 - f;

    for (i = 0; i < n; i++)
        r->out[i] = (BYTE)((a[i] * g + b[i] * f + 128) >> 8);
    Put(r, r->y++, r->out);
}

/* Made larger down: the rows that lie before the one just seen. */
static void GrowDown(RESAMPLE *r, BOOL last)
{
    while (r->y < r->j.outH) {
        long at = (2L * r->y + 1) * ((long)r->j.cropH << 7) / r->j.outH - 128;
        int  i = at < 0 ? -1 : (int)(at >> 8);

        if (i >= r->seen && !last)
            return;
        if (i < 0 || i >= r->seen)                              /* past the first or last row */
            Blend(r, r->now, r->now, 0);
        else
            Blend(r, r->before, r->now, (UINT)(at & 255));
    }
}

RESAMPLE *ResampleOpen(const RESAMPLEJOB *job)
{
    RESAMPLE *r = (RESAMPLE *)ImageAlloc(sizeof *r);
    UINT n = (UINT)job->outW * 3;

    if (!r)
        return NULL;
    r->j = *job;
    r->shrinkX = job->cropW >= job->outW;
    r->shrinkY = job->cropH >= job->outH;
    r->same = job->cropW == job->outW && job->cropH == job->outH && !job->rgb;
    r->spans = (SPAN *)ImageAlloc((DWORD)job->outW * sizeof(SPAN));
    r->now = (BYTE *)ImageAlloc(n);
    r->before = (BYTE *)ImageAlloc(n);
    r->sum = (WORD *)ImageAlloc((DWORD)n * 2);
    r->out = (BYTE *)ImageAlloc(n);
    if (!r->spans || !r->now || !r->before || !r->sum || !r->out) {
        ResampleClose(r);
        return NULL;
    }
    if (r->shrinkX)
        MakeSpans(r);
    else
        MakeGrowth(r);
    if (r->shrinkY) {
        DWORD whole = (DWORD)job->cropH << 8;
        r->step = whole / job->outH;
        r->restStep = whole % job->outH;
        NextEdges(r);
    }
    return r;
}

BOOL ResampleRow(RESAMPLE *r, const BYTE *row)
{
    const RESAMPLEJOB *j = &r->j;
    BYTE *swap;

    if (r->srcY++ < j->cropY)
        return TRUE;
    if (r->y >= j->outH)
        return FALSE;
    row += (UINT)j->cropX * 3;
    if (r->same) {
        Put(r, r->y++, row);
        return r->y < j->outH;
    }
    swap = r->now;
    r->now = r->before;
    r->before = swap;
    if (r->shrinkX)
        ShrinkRow(r, row);
    else
        GrowRow(r, row);
    if (r->shrinkY)
        ShrinkDown(r);
    else
        GrowDown(r, r->seen == j->cropH - 1);
    r->seen++;
    return r->y < j->outH;
}

void ResampleClose(RESAMPLE *r)
{
    if (!r)
        return;
    ImageFree(r->spans);
    ImageFree(r->now);
    ImageFree(r->before);
    ImageFree(r->sum);
    ImageFree(r->out);
    ImageFree(r);
}
