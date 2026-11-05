/*
 * A JPEG block's coefficients back to pixels, at 8, 4, 2 or 1 pixels across:
 * the lowest n x n coefficients through an n-point inverse DCT give the block
 * made smaller, without working out the 64 pixels first.
 *
 * All in 16 bits: a long product is a library call in this compiler's 16-bit
 * code, and this is where the time goes. The 8-point one is Arai, Agui and
 * Nakajima's, with 5 products a line, the arrangement of the Independent JPEG
 * Group's jidctfst; its scaling is folded into the divisors (JpegIdctScale).
 * Rows and columns that are all zero, most of them in a photo, are not worked.
 */
#include "internal.h"
#include "imageint.h"
#include "jpeg.h"

#define K_1082      277                 /* 1.082392200 * 256 */
#define K_1414      362                 /* 1.414213562 * 256 */
#define K_1847      473                 /* 1.847759065 * 256 */
#define K_2613      669                 /* 2.613125930 * 256 */

/* AAN's scale of each coefficient, 14 bits after the point. */
static const WORD Scale[64] = {
    16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
    22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
    21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
    19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
    16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
    12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
     8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
     4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247,
};

/* The divisor of coefficient i, for 8 points: 2 bits after the point. */
WORD JpegIdctScale(WORD quant, int i)
{
    DWORD v = (DWORD)quant * Scale[i] >> 12;

    return (WORD)(v > 0x7FFF ? 0x7FFF : v);
}

#define Clamp(v)    ((UINT)(v) > 255 ? ((v) < 0 ? 0 : 255) : (BYTE)(v))

/* One line of eight in; t0-t7 and t10-t13 the results' parts. */
#define LINE8(i0, i1, i2, i3, i4, i5, i6, i7) {                 \
    t0 = (i0); t1 = (i2); t2 = (i4); t3 = (i6);                 \
    t10 = t0 + t2;                                              \
    t11 = t0 - t2;                                              \
    t13 = t1 + t3;                                              \
    t12 = Mul8(t1 - t3, K_1414) - t13;                          \
    t0 = t10 + t13;                                             \
    t3 = t10 - t13;                                             \
    t1 = t11 + t12;                                             \
    t2 = t11 - t12;                                             \
    t4 = (i1); t5 = (i3); t6 = (i5); t7 = (i7);                 \
    z13 = t6 + t5;                                              \
    z10 = t6 - t5;                                              \
    z11 = t4 + t7;                                              \
    z12 = t4 - t7;                                              \
    t7 = z11 + z13;                                             \
    t11 = Mul8(z11 - z13, K_1414);                              \
    z5 = Mul8(z10 + z12, K_1847);                               \
    t10 = Mul8(z12, K_1082) - z5;                               \
    t12 = z5 - Mul8(z10, K_2613);                               \
    t6 = t12 - t7;                                              \
    t5 = t11 - t6;                                              \
    t4 = t10 + t5;                                              \
}

static void Idct8(const short *in, const WORD *q, BYTE *out, UINT stride)
{
    int ws[64], i, v;
    int t0, t1, t2, t3, t4, t5, t6, t7, t10, t11, t12, t13, z5, z10, z11, z12, z13;

    for (i = 0; i < 8; i++) {           /* down each column */
        const short *c = in + i;
        const WORD  *m = q + i;
        int *w = ws + i;
        if (!(c[8] | c[16] | c[24] | c[32] | c[40] | c[48] | c[56])) {
            w[0] = w[8] = w[16] = w[24] = w[32] = w[40] = w[48] = w[56] = c[0] * (int)m[0];
            continue;
        }
        LINE8(c[0] * (int)m[0], c[8] * (int)m[8], c[16] * (int)m[16], c[24] * (int)m[24],
              c[32] * (int)m[32], c[40] * (int)m[40], c[48] * (int)m[48], c[56] * (int)m[56]);
        w[0] = t0 + t7;
        w[56] = t0 - t7;
        w[8] = t1 + t6;
        w[48] = t1 - t6;
        w[16] = t2 + t5;
        w[40] = t2 - t5;
        w[32] = t3 + t4;
        w[24] = t3 - t4;
    }
    for (i = 0; i < 8; i++, out += stride) {    /* then across each row */
        const int *w = ws + i * 8;
        int first = w[0] + (128 << 5) + 16;     /* to 0-255, rounded: 5 bits after the point */
        if (!(w[1] | w[2] | w[3] | w[4] | w[5] | w[6] | w[7])) {
            v = first >> 5;
            _fmemset(out, Clamp(v), 8);
            continue;
        }
        LINE8(first, w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
        v = (t0 + t7) >> 5;
        out[0] = (BYTE)Clamp(v);
        v = (t0 - t7) >> 5;
        out[7] = (BYTE)Clamp(v);
        v = (t1 + t6) >> 5;
        out[1] = (BYTE)Clamp(v);
        v = (t1 - t6) >> 5;
        out[6] = (BYTE)Clamp(v);
        v = (t2 + t5) >> 5;
        out[2] = (BYTE)Clamp(v);
        v = (t2 - t5) >> 5;
        out[5] = (BYTE)Clamp(v);
        v = (t3 + t4) >> 5;
        out[4] = (BYTE)Clamp(v);
        v = (t3 - t4) >> 5;
        out[3] = (BYTE)Clamp(v);
    }
}

/* Four points, 0.5, 0.65328 and 0.27060: first with 2 more bits after the
   point, for the second to take off with its rounding. */
static void Idct4(const short *in, const WORD *q, BYTE *out, UINT stride)
{
    int ws[16], i, v;

    for (i = 0; i < 4; i++) {
        int a = in[i] * (int)q[i], b = in[4 + i] * (int)q[4 + i], c = in[8 + i] * (int)q[8 + i],
            d = in[12 + i] * (int)q[12 + i];
        int t0 = (a + c) * 2, t1 = (a - c) * 2, o0 = Mul8(b, 669) + Mul8(d, 277), o1 = Mul8(b, 277) - Mul8(d, 669);
        ws[i] = t0 + o0;
        ws[12 + i] = t0 - o0;
        ws[4 + i] = t1 + o1;
        ws[8 + i] = t1 - o1;
    }
    for (i = 0; i < 4; i++, out += stride) {
        const int *w = ws + i * 4;
        int a = w[0] + (128 << 4) + 4, b = w[1], c = w[2], d = w[3];
        int t0 = (a + c) >> 1, t1 = (a - c) >> 1, o0 = Mul8(b, 167) + Mul8(d, 69), o1 = Mul8(b, 69) - Mul8(d, 167);
        v = (t0 + o0) >> 3;
        out[0] = (BYTE)Clamp(v);
        v = (t0 - o0) >> 3;
        out[3] = (BYTE)Clamp(v);
        v = (t1 + o1) >> 3;
        out[1] = (BYTE)Clamp(v);
        v = (t1 - o1) >> 3;
        out[2] = (BYTE)Clamp(v);
    }
}

static void Idct2(const short *in, const WORD *q, BYTE *out, UINT stride)
{
    int a = in[0] * (int)q[0] + (128 << 3) + 4, b = in[1] * (int)q[1], c = in[2] * (int)q[2], d = in[3] * (int)q[3], v;

    v = (a + b + c + d) >> 3;
    out[0] = (BYTE)Clamp(v);
    v = (a - b + c - d) >> 3;
    out[1] = (BYTE)Clamp(v);
    v = (a + b - c - d) >> 3;
    out[stride] = (BYTE)Clamp(v);
    v = (a - b - c + d) >> 3;
    out[stride + 1] = (BYTE)Clamp(v);
}

/* in: n * n coefficients, q the same; out: n rows of n pixels. */
void JpegIdct(const short *in, const WORD *q, int n, BYTE *out, UINT stride)
{
    if (n == 8) {
        Idct8(in, q, out, stride);
    } else if (n == 4) {
        Idct4(in, q, out, stride);
    } else if (n == 2) {
        Idct2(in, q, out, stride);
    } else {
        int v = (in[0] * (int)q[0] + (128 << 3) + 4) >> 3;
        *out = (BYTE)Clamp(v);
    }
}
