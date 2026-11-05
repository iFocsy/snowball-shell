/* JPEG's compressed data: bits, Huffman codes, the blocks of plain and of
   progressive scans. */
#include "internal.h"
#include "imageint.h"
#include "jpeg.h"

/* Bits until 32 are held. A marker stops them: zeros follow until it is
   dealt with, so a damaged file ends in grey and never runs past its data. */
static void Fill(JPEG *j)
{
    IMAGE *im = j->im;

    while (j->nbits <= 24) {
        int c = 0;
        if (!j->marker) {
            c = ImageByte(im);
            if (c == 0xFF) {
                int m;
                do {
                    m = ImageByte(im);
                } while (m == 0xFF);
                if (m != 0) {
                    j->marker = m < 0 ? M_EOI : m;
                    c = 0;
                }
            } else if (c < 0) {
                j->marker = M_EOI;
                c = 0;
            }
        }
        j->bits |= (DWORD)c << (24 - j->nbits);
        j->nbits += 8;
    }
}

#define Drop(j, n)  ((j)->bits <<= (n), (j)->nbits -= (n))

static int GetBits(JPEG *j, int n)
{
    int v;

    if (j->nbits < n)
        Fill(j);
    v = (int)(j->bits >> (32 - n));
    Drop(j, n);
    return v;
}

/* The bits of a value of s bits as the signed number they mean. */
static int Extend(int v, int s)
{
    return v < (1 << (s - 1)) ? v - (1 << s) + 1 : v;
}

static int Decode(JPEG *j, const HUFF *t)
{
    UINT code;
    int  l;

    if (j->nbits < 16)
        Fill(j);
    code = (UINT)(j->bits >> 24);
    if ((l = t->length[code]) != 0) {
        Drop(j, l);
        return t->symbol[code];
    }
    code = (UINT)(j->bits >> 16);
    for (l = 9; l <= 16; l++) {
        long c = code >> (16 - l);
        if (c <= t->maxCode[l]) {
            Drop(j, l);
            return t->values[(UINT)(t->valPtr[l] + c - t->minCode[l]) & 255];
        }
    }
    Drop(j, 16);
    return 0;
}

void JpegHuffBuild(HUFF *t, const BYTE *counts, const BYTE *values)
{
    long code = 0;
    int  l, i, k = 0;

    _fmemset(t, 0, sizeof *t);
    _fmemcpy(t->values, values, 256);
    for (l = 1; l <= 16; l++) {
        t->valPtr[l] = k;
        t->minCode[l] = code;
        for (i = 0; i < counts[l - 1]; i++, k++, code++) {
            if (l <= 8) {
                UINT at = (UINT)(code << (8 - l)), n = 1 << (8 - l);
                while (n-- && at < 256) {
                    t->symbol[at] = values[k & 255];
                    t->length[at++] = (BYTE)l;
                }
            }
        }
        t->maxCode[l] = counts[l - 1] ? code - 1 : -1;
        code <<= 1;
    }
    t->maxCode[17] = 0xFFFFFL;
}

void JpegScanStart(JPEG *j)
{
    int i;

    j->bits = 0;
    j->nbits = 0;
    j->marker = 0;
    j->eobRun = 0;
    j->left = j->restart;
    for (i = 0; i < j->comps; i++)
        j->comp[i].pred = 0;
}

/* Past the restart marker; data after a lost one comes out wrong, not fatal. */
void JpegRestart(JPEG *j)
{
    int i, m = JpegNextMarker(j);

    j->bits = 0;
    j->nbits = 0;
    j->eobRun = 0;
    j->left = j->restart;
    j->marker = m >= M_RST0 && m <= M_RST7 ? 0 : m;
    for (i = 0; i < j->comps; i++)
        j->comp[i].pred = 0;
}

int JpegNextMarker(JPEG *j)
{
    IMAGE *im = j->im;
    int   c, m;

    if (j->marker) {
        m = j->marker;
        j->marker = 0;
        return m;
    }
    for (;;) {
        if ((c = ImageByte(im)) < 0)
            return M_EOI;
        if (c != 0xFF)
            continue;
        do {
            m = ImageByte(im);
        } while (m == 0xFF);
        if (m < 0)
            return M_EOI;
        if (m)
            return m;
    }
}

/* A block of a plain scan, its kept coefficients in b (zeroed). */
void JpegBlockPlain(JPEG *j, JCOMP *c, short *b)
{
    const HUFF *ac = &j->ac[c->acTable];
    int t = Decode(j, &j->dc[c->dcTable]) & 15, k, rs, s;

    if (t)
        c->pred += Extend(GetBits(j, t), t);
    b[0] = (short)c->pred;
    for (k = 1; k < 64; k++) {
        rs = Decode(j, ac);
        s = rs & 15;
        if (!s) {
            if (rs >> 4 < 15)
                break;
            k += 15;
            continue;
        }
        k += rs >> 4;
        t = GetBits(j, s);
        if (k < 64 && j->keep[k] >= 0)
            b[j->keep[k]] = (short)Extend(t, s);
    }
}

#define Mark(nz, k)     ((nz)[(k) >> 3] |= (BYTE)(1 << ((k) & 7)))
#define Marked(nz, k)   ((nz)[(k) >> 3] & (1 << ((k) & 7)))

/* The first scan of a band of coefficients. */
static void AcFirst(JPEG *j, const HUFF *ac, short *b, BYTE *nz)
{
    int k, rs, r, s, v;

    if (j->eobRun) {
        j->eobRun--;
        return;
    }
    for (k = j->ss; k <= j->se; k++) {
        rs = Decode(j, ac);
        r = rs >> 4;
        s = rs & 15;
        if (!s) {
            if (r < 15) {
                j->eobRun = (1 << r) - 1;
                if (r)
                    j->eobRun += GetBits(j, r);
                break;
            }
            k += 15;
            continue;
        }
        k += r;
        v = Extend(GetBits(j, s), s);
        if (k < 64) {
            Mark(nz, k);
            if (j->keep[k] >= 0)
                b[j->keep[k]] = (short)(v * (1 << j->al));
        }
    }
}

/* One more bit of a coefficient known not to be zero. */
static void Sharpen(JPEG *j, short *b, int k, int bit)
{
    if (GetBits(j, 1) && j->keep[k] >= 0) {
        short *p = &b[j->keep[k]];
        if (!(*p & bit))
            *p += (short)(*p >= 0 ? bit : -bit);
    }
}

/* A later scan: a bit more of each coefficient, and new ones that were zero. */
static void AcRefine(JPEG *j, const HUFF *ac, short *b, BYTE *nz)
{
    int k = j->ss, se = j->se, bit = 1 << j->al;

    if (!j->eobRun) {
        for (; k <= se; k++) {
            int rs = Decode(j, ac), r = rs >> 4, s = rs & 15, v = 0;
            if (s) {
                v = GetBits(j, 1) ? bit : -bit;
            } else if (r < 15) {
                j->eobRun = 1 << r;
                if (r)
                    j->eobRun += GetBits(j, r);
                break;
            }
            for (; k <= se; k++) {              /* past r zeros, sharpening the others */
                if (Marked(nz, k))
                    Sharpen(j, b, k, bit);
                else if (--r < 0)
                    break;
            }
            if (v && k <= se) {
                Mark(nz, k);
                if (j->keep[k] >= 0)
                    b[j->keep[k]] = (short)v;
            }
        }
    }
    if (j->eobRun) {
        for (; k <= se; k++)
            if (Marked(nz, k))
                Sharpen(j, b, k, bit);
        j->eobRun--;
    }
}

void JpegBlockProgressive(JPEG *j, JCOMP *c, short *b, BYTE *nz)
{
    if (j->ss) {
        const HUFF *ac = &j->ac[c->acTable];
        if (j->ah)
            AcRefine(j, ac, b, nz);
        else
            AcFirst(j, ac, b, nz);
    } else if (j->ah) {
        if (GetBits(j, 1))
            b[0] |= (short)(1 << j->al);
    } else {
        int t = Decode(j, &j->dc[c->dcTable]) & 15;
        if (t)
            c->pred += Extend(GetBits(j, t), t);
        b[0] = (short)(c->pred * (1 << j->al));
    }
}
