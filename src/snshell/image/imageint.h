/* What the readers of the formats share with picture.c and resample.c. */
#ifndef IMAGEINT_H
#define IMAGEINT_H

#define IMAGE_BUFFER    8192            /* file bytes read at a time */
#define IMAGE_ROW_MAX   60000U          /* the longest row a far pointer walks */

/* 16-bit products in one instruction: in this compiler's 16-bit code a long
   product is a library call, and these are where the time goes. */
int      Mul8(int a, int b);                    /* a * b / 256 */
#pragma aux Mul8 = "imul dx" "mov al,ah" "mov ah,dl" parm [ax] [dx] value [ax] modify exact [ax dx];
unsigned MulHigh(unsigned a, unsigned b);       /* a * b / 65536 */
#pragma aux MulHigh = "mul dx" "mov ax,dx" parm [ax] [dx] value [ax] modify exact [ax dx];
unsigned long MulWide(unsigned a, unsigned b);  /* a * b */
#pragma aux MulWide = "mul dx" parm [ax] [dx] value [dx ax] modify exact [ax dx];

typedef struct RESAMPLE RESAMPLE;

typedef struct IMAGE {
    HFILE    file;
    DWORD    size, done;                /* the file's bytes, those read */
    BYTE     *in;
    UINT     pos, len;
    BOOL     ended;                     /* no more bytes */
    int      w, h;                      /* as stored */
    int      orient;                    /* EXIF's: 1 upright, 2-8 mirrored and turned */
    int      rowW, rowH;                /* the rows given to the resampler */
    BOOL     rgbRows;                   /* R, G, B, not B, G, R */
    COLORREF back;
    RESAMPLE *rs;
    void     *reader;
    int      (*prepare)(struct IMAGE *im, int *reduce);  /* reduce: 1, 2, 4, 8; may raise it */
    int      (*run)(struct IMAGE *im);                   /* rows to ResampleRow */
    void     (*close)(struct IMAGE *im);
    IMAGEPROGRESS progress;
    DWORD    tick;
    BOOL     stopped;
} IMAGE;

/* The next byte, or -1 at the end. */
#define ImageByte(im)   ((im)->pos < (im)->len ? (int)(im)->in[(im)->pos++] : ImageFill(im))
int   ImageFill(IMAGE *im);
void  ImageSkip(IMAGE *im, DWORD n);
int   ImagePercent(const IMAGE *im);            /* of the file read */
BOOL  ImageTick(IMAGE *im, int percent);        /* FALSE: stop */
void  *ImageAlloc(DWORD size);                  /* zeroed, locked; NULL: no memory */
void  ImageFree(void *p);

/* resample.c: rows of B, G, R bytes in, the part of them asked for out at
   the size asked for, into the DIB's bits. */
typedef struct {
    int  srcW, srcH;                    /* the rows */
    int  cropX, cropY, cropW, cropH;    /* the part of them used */
    int  outW, outH;                    /* it becomes this size, then */
    BOOL rgb;                           /* rows of R, G, B */
    int  orient;                        /* is turned as EXIF says, */
    BYTE huge *bits;                    /* into these rows of a DIB */
    DWORD pitch;
    int  dibH;
} RESAMPLEJOB;
RESAMPLE *ResampleOpen(const RESAMPLEJOB *job);
BOOL  ResampleRow(RESAMPLE *r, const BYTE *row);        /* FALSE: no more rows needed */
void  ResampleClose(RESAMPLE *r);                       /* rows missing stay as they are */

int   JpegOpen(IMAGE *im);              /* IMAGE_: reads up to the frame's size */
int   PngOpen(IMAGE *im);
int   BmpOpen(IMAGE *im);

#endif
