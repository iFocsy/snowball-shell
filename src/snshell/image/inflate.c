/*
 * Deflate, as PNG's data is compressed (RFC 1951): bytes in through a
 * function, bytes out to another, 32 KB at a time. Codes of up to 9 bits are
 * found in one look; the rest, which are rare, a bit at a time.
 */
#include "internal.h"
#include "imageint.h"

#define WINDOW      32768U
#define FAST_BITS   9
#define MAX_CODES   320

typedef struct {
    WORD fast[1 << FAST_BITS];          /* by the next bits: symbol << 4 | length; 0: longer */
    WORD count[16];                     /* codes of each length */
    WORD symbol[MAX_CODES];             /* by length, then code */
} TREE;

typedef struct {
    int   (*next)(void *user);
    BOOL  (*sink)(void *user, const BYTE *data, UINT n);
    void  *user;
    DWORD bits;
    int   nbits, past;
    BOOL  failed, stopped;
    BYTE  *window;
    UINT  pos;
    TREE  lit, dist;
    BYTE  lengths[MAX_CODES];
} INFLATE;

static const WORD LenBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const BYTE LenExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const WORD DistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
    6145, 8193, 12289, 16385, 24577
};
static const BYTE DistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};
static const BYTE LengthOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

/* A little past the end is normal (looking ahead); more is a file cut short. */
static void Need(INFLATE *z, int n)
{
    while (z->nbits < n) {
        int c = z->next(z->user);
        if (c < 0) {
            if (++z->past > 8)
                z->failed = TRUE;
            c = 0;
        }
        z->bits |= (DWORD)c << z->nbits;
        z->nbits += 8;
    }
}

static UINT Bits(INFLATE *z, int n)
{
    UINT v;

    if (!n)
        return 0;
    Need(z, n);
    v = (UINT)(z->bits & ((1L << n) - 1));
    z->bits >>= n;
    z->nbits -= n;
    return v;
}

static void Build(TREE *t, const BYTE *lengths, int n)
{
    WORD offset[17], code[17], c = 0;
    int  i, l;

    _fmemset(t, 0, sizeof *t);
    for (i = 0; i < n; i++)
        t->count[lengths[i]]++;
    t->count[0] = 0;
    offset[1] = 0;
    for (l = 1; l < 16; l++)
        offset[l + 1] = offset[l] + t->count[l];
    for (l = 1; l < 16; l++) {
        code[l] = c;
        c = (WORD)((c + t->count[l]) << 1);
    }
    for (i = 0; i < n; i++) {
        l = lengths[i];
        if (!l)
            continue;
        t->symbol[offset[l]++] = (WORD)i;
        if (l <= FAST_BITS) {
            UINT rev = 0, v = code[l], k;
            int  b;
            for (b = 0; b < l; b++, v >>= 1)
                rev = rev << 1 | (v & 1);
            for (k = rev; k < 1U << FAST_BITS; k += 1U << l)
                t->fast[k] = (WORD)(i << 4 | l);
        }
        code[l]++;
    }
}

static int Symbol(INFLATE *z, const TREE *t)
{
    UINT e, v;
    int  len, code = 0, first = 0, index = 0;

    Need(z, 15);
    e = t->fast[(UINT)z->bits & ((1 << FAST_BITS) - 1)];
    if (e) {
        len = e & 15;
        z->bits >>= len;
        z->nbits -= len;
        return e >> 4;
    }
    v = (UINT)z->bits;
    for (len = 1; len <= 15; len++, v >>= 1) {
        int count = t->count[len];
        code |= v & 1;
        if (code - count < first) {
            z->bits >>= len;
            z->nbits -= len;
            return t->symbol[(index + code - first) % MAX_CODES];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    z->failed = TRUE;
    return 256;
}

static void Flush(INFLATE *z)
{
    if (z->pos && !z->sink(z->user, z->window, z->pos))
        z->stopped = TRUE;
    z->pos = 0;
}

static void Put(INFLATE *z, BYTE b)
{
    z->window[z->pos++] = b;
    if (z->pos == WINDOW)
        Flush(z);
}

/* len bytes from dist back, in pieces that neither wrap nor overlap where it can. */
static void Copy(INFLATE *z, UINT len, UINT dist)
{
    BYTE *w = z->window;
    UINT from = (z->pos - dist) & (WINDOW - 1);

    while (len && !z->stopped) {
        UINT n = min(len, WINDOW - z->pos), i;
        n = min(n, WINDOW - from);
        if (dist >= n) {
            _fmemcpy(w + z->pos, w + from, n);
        } else {
            BYTE *to = w + z->pos;
            const BYTE *p = w + from;
            for (i = 0; i < n; i++)
                to[i] = p[i];
        }
        z->pos += n;
        from = (from + n) & (WINDOW - 1);
        len -= n;
        if (z->pos == WINDOW)
            Flush(z);
    }
}

static void Stored(INFLATE *z)
{
    UINT n, not;

    z->bits >>= z->nbits & 7;
    z->nbits -= z->nbits & 7;
    n = Bits(z, 16);
    not = Bits(z, 16);
    if ((n ^ not) != 0xFFFF) {
        z->failed = TRUE;
        return;
    }
    while (n-- && !z->failed && !z->stopped)
        Put(z, (BYTE)Bits(z, 8));
}

static void Codes(INFLATE *z)
{
    while (!z->failed && !z->stopped) {
        int s = Symbol(z, &z->lit);
        if (s < 256) {
            Put(z, (BYTE)s);
        } else if (s == 256) {
            return;
        } else {
            UINT len;
            int  d;
            if ((s -= 257) >= 29) {
                z->failed = TRUE;
                return;
            }
            len = LenBase[s] + Bits(z, LenExtra[s]);
            if ((d = Symbol(z, &z->dist)) >= 30) {
                z->failed = TRUE;
                return;
            }
            Copy(z, len, DistBase[d] + Bits(z, DistExtra[d]));
        }
    }
}

static void FixedTrees(INFLATE *z)
{
    int i;

    for (i = 0; i < 288; i++)
        z->lengths[i] = (BYTE)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
    Build(&z->lit, z->lengths, 288);
    for (i = 0; i < 30; i++)
        z->lengths[i] = 5;
    Build(&z->dist, z->lengths, 30);
}

static void DynamicTrees(INFLATE *z)
{
    int hlit = (int)Bits(z, 5) + 257, hdist = (int)Bits(z, 5) + 1, hclen = (int)Bits(z, 4) + 4, i, total;

    if (hlit > 286 || hdist > 30) {
        z->failed = TRUE;
        return;
    }
    _fmemset(z->lengths, 0, sizeof z->lengths);
    for (i = 0; i < hclen; i++)
        z->lengths[LengthOrder[i]] = (BYTE)Bits(z, 3);
    Build(&z->lit, z->lengths, 19);
    total = hlit + hdist;
    _fmemset(z->lengths, 0, sizeof z->lengths);
    for (i = 0; i < total && !z->failed;) {
        int s = Symbol(z, &z->lit), repeat;
        BYTE value = 0;
        if (s < 16) {
            z->lengths[i++] = (BYTE)s;
            continue;
        }
        if (s == 16) {
            if (!i) {
                z->failed = TRUE;
                return;
            }
            value = z->lengths[i - 1];
            repeat = 3 + (int)Bits(z, 2);
        } else {
            repeat = s == 17 ? 3 + (int)Bits(z, 3) : 11 + (int)Bits(z, 7);
        }
        if (i + repeat > total) {
            z->failed = TRUE;
            return;
        }
        while (repeat--)
            z->lengths[i++] = value;
    }
    Build(&z->lit, z->lengths, hlit);
    Build(&z->dist, z->lengths + hlit, hdist);
}

/* A zlib stream to its end, or until sink says FALSE; TRUE unless memory ran out. */
BOOL InflateRun(int (*next)(void *user), BOOL (*sink)(void *user, const BYTE *data, UINT n), void *user)
{
    INFLATE *z = (INFLATE *)ImageAlloc(sizeof *z);
    BYTE    *window = (BYTE *)ImageAlloc(WINDOW);
    UINT    last = 0, type;

    if (!z || !window) {
        ImageFree(z);
        ImageFree(window);
        return FALSE;
    }
    z->next = next;
    z->sink = sink;
    z->user = user;
    z->window = window;
    next(user);                         /* zlib's two bytes */
    next(user);
    while (!last && !z->failed && !z->stopped) {
        last = Bits(z, 1);
        type = Bits(z, 2);
        if (type == 0) {
            Stored(z);
        } else if (type == 3) {
            z->failed = TRUE;
        } else {
            if (type == 1)
                FixedTrees(z);
            else
                DynamicTrees(z);
            if (!z->failed)
                Codes(z);
        }
    }
    if (!z->stopped)
        Flush(z);
    ImageFree(window);
    ImageFree(z);
    return TRUE;
}
