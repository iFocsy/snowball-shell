/*
 * The system's mouse pointers from a theme (Windows 98's Mouse Pointers).
 * Windows 3.1 has no call to change them, so the shapes of its own shared
 * cursors (LoadCursor(NULL, IDC_*)) are written over with a cursor made
 * from a .CUR file, or from an .ANI's first frame: black and white at the
 * display's cursor size, as Windows 3.1 draws them. The old shape is kept
 * to put back, and the cursor's memory kept locked meanwhile (a discarded
 * shape would come back from the driver). A cursor is changed only when its
 * memory is laid out as CreateCursor's, so nothing else is ever written.
 */
#include "snsystem.h"

#define CURSOR_W    32                  /* Windows 3.1's (SM_CXCURSOR) */
#define ROW         (CURSOR_W / 8)
#define MAX_IMAGE   16384               /* a cursor's picture in its file, at most */

static LPCSTR const ids[CURSORS] = {
    IDC_ARROW, IDC_IBEAM, IDC_WAIT, IDC_CROSS, IDC_UPARROW, IDC_SIZE, IDC_SIZENWSE, IDC_SIZENESW, IDC_SIZEWE, IDC_SIZENS,
};
static HGLOBAL kept[CURSORS];           /* Windows' own shape, while another is in */

static DWORD Dword(const BYTE FAR *p)
{
    return *(const DWORD FAR *)p;
}

/* The offset of the first frame (an "icon" chunk) of an .ANI's RIFF chunks
   from at to end; 0: none. */
static DWORD AniFrame(HFILE f, DWORD at, DWORD end)
{
    BYTE head[12];
    DWORD size, found;

    while (at + 8 <= end && _llseek(f, at, 0) == (LONG)at && _lread(f, head, 12) >= 8) {
        size = Dword(head + 4);
        if (!_fmemcmp(head, "icon", 4))
            return at + 8;
        if (!_fmemcmp(head, "LIST", 4) && (found = AniFrame(f, at + 12, at + 8 + size)) != 0)
            return found;
        at += 8 + ((size + 1) & ~1L);
    }
    return 0;
}

/* Pixel (x, y) of a DIB's colours (rows bottom up), lightness and whether
   its alpha (32 bits) makes it transparent. */
static int Light(const BITMAPINFOHEADER FAR *bi, const BYTE FAR *bits, int x, int y, BOOL *clear)
{
    int bpp = bi->biBitCount, w = (int)bi->biWidth, h = (int)bi->biHeight / 2;
    DWORD stride = ((DWORD)w * bpp + 31) / 32 * 4;
    const BYTE FAR *row = bits + stride * (h - 1 - y), FAR *c;
    const RGBQUAD FAR *pal = (const RGBQUAD FAR *)((const BYTE FAR *)bi + bi->biSize);
    int i;

    *clear = FALSE;
    if (bpp <= 8) {
        i = (row[x * bpp / 8] >> (8 - bpp - x * bpp % 8)) & ((1 << bpp) - 1);
        c = (const BYTE FAR *)&pal[i];
    } else {
        c = row + x * (bpp / 8);
        *clear = bpp == 32 && c[3] == 0;
    }
    return (c[0] * 11 + c[1] * 59 + c[2] * 30) / 100;  /* blue, green, red */
}

/* A cursor from a .CUR (or .ICO) image at base of the file. */
static HCURSOR FromImage(HFILE f, DWORD base)
{
    BYTE dir[6 + 16 * 8], andBits[ROW * CURSOR_W], xorBits[ROW * CURSOR_W];
    const BYTE FAR *e, FAR *bits, FAR *mask;
    const BITMAPINFOHEADER FAR *bi;
    int n, i, best = 0, w, h, x, y, sx, sy, colors, light, hotX, hotY;
    HGLOBAL mem;
    DWORD size, maskRow;
    HCURSOR c = NULL;
    BOOL clear;

    if (_llseek(f, base, 0) != (LONG)base || _lread(f, dir, sizeof dir) < 22 || Dword(dir) & 0xFFFF)
        return NULL;
    n = min(*(WORD *)(dir + 4), 8);
    for (i = 0; i < n; i++)             /* a 32-pixel one if there is */
        if (dir[6 + 16 * i] == CURSOR_W)
            best = i;
    e = dir + 6 + 16 * best;
    size = Dword(e + 8);
    if (!n || size > MAX_IMAGE || size < sizeof(BITMAPINFOHEADER) ||
        (mem = GlobalAlloc(GMEM_MOVEABLE, size)) == NULL)
        return NULL;
    bi = (const BITMAPINFOHEADER FAR *)GlobalLock(mem);
    if (_llseek(f, base + Dword(e + 12), 0) != -1 && _lread(f, (LPVOID)bi, (UINT)size) == (UINT)size &&
        bi->biSize == sizeof(BITMAPINFOHEADER) && (w = (int)bi->biWidth) > 0 && w <= 64 &&
        (h = (int)bi->biHeight / 2) > 0 && h <= 64 &&
        (bi->biBitCount == 1 || bi->biBitCount == 4 || bi->biBitCount == 8 || bi->biBitCount == 24 || bi->biBitCount == 32)) {
        colors = bi->biBitCount <= 8 ? (bi->biClrUsed ? (int)bi->biClrUsed : 1 << bi->biBitCount) : 0;
        bits = (const BYTE FAR *)bi + bi->biSize + colors * sizeof(RGBQUAD);
        mask = bits + ((DWORD)w * bi->biBitCount + 31) / 32 * 4 * h;
        maskRow = ((DWORD)w + 31) / 32 * 4;
        if ((DWORD)(mask - (const BYTE FAR *)bi) + maskRow * h <= size) {
            _fmemset(andBits, 0, sizeof andBits);
            _fmemset(xorBits, 0, sizeof xorBits);
            for (y = 0; y < CURSOR_W; y++)
                for (x = 0; x < CURSOR_W; x++) {
                    sx = x * w / CURSOR_W;
                    sy = y * h / CURSOR_W;
                    light = Light(bi, bits, sx, sy, &clear);
                    if (clear || (mask[maskRow * (h - 1 - sy) + sx / 8] & (0x80 >> (sx % 8)))) {
                        andBits[y * ROW + x / 8] |= (BYTE)(0x80 >> (x % 8));  /* transparent, */
                        if (!clear && light > 0)                            /* or the screen inverted */
                            xorBits[y * ROW + x / 8] |= (BYTE)(0x80 >> (x % 8));
                    } else if (light >= 128) {
                        xorBits[y * ROW + x / 8] |= (BYTE)(0x80 >> (x % 8));  /* white, else black */
                    }
                }
            hotX = *(WORD *)(dir + 2) == 2 ? *(const WORD FAR *)(e + 4) * CURSOR_W / w : 0;
            hotY = *(WORD *)(dir + 2) == 2 ? *(const WORD FAR *)(e + 6) * CURSOR_W / h : 0;
            c = CreateCursor(g_hInst, min(hotX, CURSOR_W - 1), min(hotY, CURSOR_W - 1), CURSOR_W, CURSOR_W,
                             andBits, xorBits);
        }
    }
    GlobalUnlock(mem);
    GlobalFree(mem);
    return c;
}

static HCURSOR FromFile(LPCSTR file)
{
    HFILE f = _lopen(file, OF_READ | OF_SHARE_DENY_NONE);
    BYTE head[12];
    DWORD at = 0;
    HCURSOR c = NULL;

    if (f == HFILE_ERROR)
        return NULL;
    if (_lread(f, head, 12) == 12 && !_fmemcmp(head, "RIFF", 4) && !_fmemcmp(head + 8, "ACON", 4))
        at = AniFrame(f, 12, Dword(head + 4) + 8);
    if (at || _fmemcmp(head, "RIFF", 4))
        c = FromImage(f, at);
    _lclose(f);
    return c;
}

/* A pointer from a file, as CursorsSet would make it (the caller destroys
   it); NULL: none there. */
HCURSOR SNSYSTEM_API CursorsLoad(LPCSTR file)
{
    return FromFile(file);
}

/* The bytes of a cursor's shape: Windows 3.1's CURSORSHAPE (hot spot,
   size, bytes a row, planes, bits) and its two masks. */
static WORD Shape(const BYTE FAR *p)
{
    const WORD FAR *w = (const WORD FAR *)p;

    return p[10] == 1 && p[11] == 1 && w[3] <= 64 && w[4] <= 16 ? 12 + 2 * w[4] * w[3] : 0;
}

/* Pointer which (CURSOR_*) from file (a short path; NULL or "": Windows'
   own again). FALSE: the file has none, or this Windows keeps its pointers
   in another way. */
BOOL SNSYSTEM_API CursorsSet(int which, LPCSTR file)
{
    static WORD keptSize[CURSORS];
    HCURSOR sys, mine = NULL;
    BYTE FAR *to, FAR *p;
    WORD need;
    BOOL ok = FALSE;

    if (which < 0 || which >= CURSORS || (sys = LoadCursor(NULL, ids[which])) == NULL)
        return FALSE;
    if (file && *file && (mine = FromFile(file)) == NULL)
        return FALSE;
    if (!mine && !kept[which])
        return TRUE;                    /* Windows' own already */
    if ((to = LockResource((HGLOBAL)sys)) == NULL) {    /* (loaded again if it was discarded) */
        if (mine)
            DestroyCursor(mine);
        return FALSE;
    }
    if (mine) {
        p = GlobalLock((HGLOBAL)mine);
        need = p ? Shape(p) : 0;
        ok = need && GlobalSize((HGLOBAL)mine) >= need && GlobalSize((HGLOBAL)sys) >= need &&
             Shape(to) == need && !_fmemcmp(to + 4, p + 4, 8);   /* the same layout */
        if (ok && !kept[which] && (kept[which] = GlobalAlloc(GMEM_MOVEABLE, need)) != NULL) {
            _fmemcpy(GlobalLock(kept[which]), to, need);        /* Windows' shape kept, */
            GlobalUnlock(kept[which]);
            keptSize[which] = need;
            GlobalLock((HGLOBAL)sys);                           /* its memory locked meanwhile */
        }
        if (ok && kept[which])
            _fmemcpy(to, p, need);
        else
            ok = FALSE;
        if (p)
            GlobalUnlock((HGLOBAL)mine);
        DestroyCursor(mine);
    } else {
        _fmemcpy(to, GlobalLock(kept[which]), keptSize[which]);
        GlobalUnlock(kept[which]);
        GlobalFree(kept[which]);
        kept[which] = NULL;
        GlobalUnlock((HGLOBAL)sys);     /* (the lock taken when changed) */
        ok = TRUE;
    }
    GlobalUnlock((HGLOBAL)sys);
    if (GetCursor() == sys) {           /* the display takes the new shape */
        SetCursor(NULL);
        SetCursor(sys);
    }
    return ok;
}

/* Windows' own pointers back (the shell ends). */
void CursorsRestore(void)
{
    int i;

    for (i = 0; i < CURSORS; i++)
        CursorsSet(i, NULL);
}
