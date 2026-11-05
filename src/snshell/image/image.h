/*
 * Pictures (BMP, JPEG, PNG) made the size of what shows them, read a row at
 * a time: a photo many times the screen's size never is in memory whole.
 * Whole numbers only (no floating point: a 486SX has no coprocessor).
 */
#ifndef IMAGE_H
#define IMAGE_H

#define WP_FILL     0                   /* the screen covered, what is over cut off */
#define WP_FIT      1                   /* all of it, as large as the screen allows */
#define WP_STRETCH  2                   /* the screen's shape, whatever its own */
#define WP_TILE     3                   /* its own size, repeated */
#define WP_CENTER   4                   /* its own size, in the middle */
#define WP_STYLES   5
/* Tile and Center: a picture larger than the screen is first made to fit. */

#define IMAGE_OK        0
#define IMAGE_STOPPED   1               /* the progress function said so */
#define IMAGE_NOFILE    2
#define IMAGE_DAMAGED   3               /* not a picture, or a form not read */
#define IMAGE_MEMORY    4
#define IMAGE_NOWRITE   5

typedef BOOL (*IMAGEPROGRESS)(int percent);     /* FALSE: stop; may run Windows */

typedef struct {
    int      style;                     /* WP_ */
    int      w, h;                      /* the screen */
    COLORREF back;                      /* behind what a picture leaves transparent */
    IMAGEPROGRESS progress;             /* NULL: none */
} IMAGEJOB;

/* Its size as seen (turned as its EXIF data says). */
int  ImageSize(LPCSTR path, int *w, int *h);
/* The picture as it would show on the screen, as a 24-bit DIB; shownW and
   shownH its size there. scaleTo: the DIB made for a screen that many pixels
   wide (a small view of it); 0: full size. */
int  ImageMakeDib(LPCSTR path, const IMAGEJOB *job, int scaleTo, DIB *d);
/* A BMP file of it at a screen's colour depth: 24 for 15 bits and up, else
   8, 4 or 1, dithered to the colours Windows has at that depth. */
int  ImageWriteBitmap(const DIB *d, LPCSTR path, int depth);

#endif
