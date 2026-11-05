/* What jpeg.c, jpegscan.c and jpegidct.c share. */
#ifndef JPEG_H
#define JPEG_H

#define M_SOF0  0xC0
#define M_SOF1  0xC1
#define M_SOF2  0xC2
#define M_DHT   0xC4
#define M_RST0  0xD0
#define M_RST7  0xD7
#define M_SOI   0xD8
#define M_EOI   0xD9
#define M_SOS   0xDA
#define M_DQT   0xDB
#define M_DRI   0xDD
#define M_APP0  0xE0
#define M_APP1  0xE1
#define M_APP14 0xEE

typedef struct {
    BYTE  symbol[256];                  /* of a code of up to 8 bits, by the next 8 bits, */
    BYTE  length[256];                  /* its length; 0: a longer code */
    long  maxCode[18];                  /* longer codes: the largest of each length, -1 none */
    long  minCode[17];
    int   valPtr[17];
    BYTE  values[256];
} HUFF;

typedef struct {
    int   id, h, v, tq;
    int   dcTable, acTable;
    int   pred;
    int   blocksW, blocksH;             /* padded to whole MCUs */
    int   scanW, scanH;                 /* the blocks of a scan of it alone */
    BYTE  *rows;                        /* a row of MCUs, decoded */
    UINT  stride;
    short huge *coef;                   /* all its blocks' coefficients kept, n * n each */
    BYTE  huge *nonzero;                /* 8 bytes a block: those not zero, kept or not */
} JCOMP;

typedef struct {
    IMAGE *im;
    DWORD bits;                         /* the next bits, at the top */
    int   nbits;
    int   marker;                       /* met in the data: no more bits until dealt with */
    HUFF  dc[4], ac[4];
    WORD  quant[4][64];                 /* in natural order */
    WORD  quantKept[4][64];             /* as blocks are kept: n * n */
    BOOL  progressive, stored;
    BOOL  jfif, adobe;
    int   transform;                    /* Adobe's: 0 RGB, 1 YCbCr */
    int   comps, hMax, vMax, mcusX, mcusY;
    JCOMP comp[3];
    int   n, nn;                        /* pixels across a block: 8, 4, 2, 1; n * n */
    signed char keep[64];               /* by zigzag place: where a block holds it, -1 not */
    int   scanComps, scanComp[3];
    int   ss, se, ah, al;
    UINT  restart, left;
    UINT  eobRun;
    short block[64];
    BYTE  *line[3], *up[3], *bgr;
    int   crR[256], cbB[256], crG[256], cbG[256];
    BYTE  range[768];
} JPEG;

extern const BYTE JpegZigzag[64];

void JpegHuffBuild(HUFF *t, const BYTE *counts, const BYTE *values);
void JpegScanStart(JPEG *j);
void JpegRestart(JPEG *j);
int  JpegNextMarker(JPEG *j);
void JpegBlockPlain(JPEG *j, JCOMP *c, short *b);
void JpegBlockProgressive(JPEG *j, JCOMP *c, short *b, BYTE *nonzero);
void JpegIdct(const short *in, const WORD *q, int n, BYTE *out, UINT stride);
WORD JpegIdctScale(WORD quant, int i);

#endif
