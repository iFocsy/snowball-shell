/*
 * File system access. Uses the Windows 95 long file name API (INT 21h
 * AX=71xxh, provided under Windows 3.11 by LFN.DRV) when the C: drive
 * supports it, plain 8.3 DOS calls otherwise. Names are ANSI on the
 * outside and OEM towards DOS.
 */
#include "internal.h"
#include "dpmi.h"
#include <i86.h>

typedef struct { WORD ax, bx, cx, dx, si, di, ds, es; } DOSREGS;   /* ds, es 0: our data */

/* Where DOS finds a buffer: its segment in DS or ES, its offset in a register. */
#define DSDX(r, p)  ((r).ds = FP_SEG(p), (r).dx = FP_OFF(p))
#define DSSI(r, p)  ((r).ds = FP_SEG(p), (r).si = FP_OFF(p))
#define ESDI(r, p)  ((r).es = FP_SEG(p), (r).di = FP_OFF(p))

static BOOL lfn;
static WORD error;              /* DOS error code of the last failed call */
static BYTE findData[318];      /* WIN32_FIND_DATA; copied out at once */

/* INT 21h with DS and ES set from the registers given (our data segment if
   0); the buffers may be anywhere: the stack, far data. FALSE on carry. */
static BOOL Int21(DOSREGS *r)
{
    WORD a = r->ax, b = r->bx, c = r->cx, d = r->dx, s = r->si, i = r->di, cf = 0;
    WORD sds = r->ds, ses = r->es;
    _asm {
        push si
        push di
        push es
        push ds
        mov ax, ses
        or ax, ax
        jnz has_es
        mov ax, ds
    has_es:
        mov es, ax
        mov ax, sds
        or ax, ax
        jnz has_ds
        mov ax, ds
    has_ds:
        push ax
        mov ax, a
        mov bx, b
        mov cx, c
        mov dx, d
        mov si, s
        mov di, i
        pop ds
        stc
        int 21h
        pop ds
        mov a, ax
        mov b, bx
        mov c, cx
        mov d, dx
        sbb ax, ax
        mov cf, ax
        pop es
        pop di
        pop si
    }
    r->ax = a; r->bx = b; r->cx = c; r->dx = d;
    if (cf)
        error = a;
    return !cf;
}

WORD DosLastError(void)
{
    return error;
}

static void Regs(DOSREGS *r, WORD ax)
{
    _fmemset(r, 0, sizeof *r);
    r->ax = ax;
}

BOOL DosInit(void)
{
    static char root[] = "C:\\";
    char fsName[16];
    DOSREGS r;

    Regs(&r, 0x71A0);
    DSDX(r, root);
    ESDI(r, fsName);
    r.cx = sizeof fsName;
    lfn = Int21(&r) && (r.bx & 0x4000);
    return lfn;
}

BOOL DosHasLongNames(void)
{
    return lfn;
}

static BOOL FromLfn(DOSENTRY *e)
{
    e->attr = findData[0];
    e->ctime = *(WORD *)(findData + 0x04);    /* DOS format (SI = 1): time, then date */
    e->cdate = *(WORD *)(findData + 0x06);
    e->adate = *(WORD *)(findData + 0x0E);
    e->time = *(WORD *)(findData + 0x14);
    e->date = *(WORD *)(findData + 0x16);
    if (!e->cdate) {                    /* not kept (DOSBox, for folders): as written */
        e->cdate = e->date;
        e->ctime = e->time;
    }
    if (!e->adate)
        e->adate = e->date;
    /* folders have no size; DOSBox-X passes the host's on (hundreds of KB) */
    e->size = (e->attr & FA_DIR) ? 0 : *(DWORD *)(findData + 0x20);
    OemToAnsi((LPCSTR)(findData + 0x2C), e->name);
    return TRUE;
}

static void FromDta(DOSFIND *f, DOSENTRY *e)
{
    e->attr = f->dta[21];
    e->time = e->ctime = *(WORD *)(f->dta + 22);
    e->date = e->cdate = e->adate = *(WORD *)(f->dta + 24);
    e->size = (e->attr & FA_DIR) ? 0 : *(DWORD *)(f->dta + 26);
    OemToAnsi((LPCSTR)(f->dta + 30), e->name);
}

static BOOL IsDotEntry(DOSENTRY *e)
{
    return e->name[0] == '.' && (e->name[1] == 0 || (e->name[1] == '.' && e->name[2] == 0));
}

static BOOL SetDta(DOSFIND *f)
{
    DOSREGS r;
    Regs(&r, 0x1A00);
    DSDX(r, f->dta);
    return Int21(&r);
}

static BOOL NextRaw(DOSFIND *f, DOSENTRY *e)
{
    DOSREGS r;

    if (f->lfn) {
        Regs(&r, 0x714F);
        r.bx = f->handle;
        r.si = 1;
        ESDI(r, findData);
        return Int21(&r) && FromLfn(e);
    }
    SetDta(f);
    Regs(&r, 0x4F00);
    if (!Int21(&r))
        return FALSE;
    FromDta(f, e);
    return TRUE;
}

BOOL DosFindFirst(LPCSTR spec, DOSFIND *f, DOSENTRY *e)
{
    char oem[MAX_PATH];
    DOSREGS r;

    AnsiToOem(spec, oem);
    f->lfn = lfn;
    if (lfn) {
        Regs(&r, 0x714E);
        r.cx = 0x0016;                  /* hidden, system and directories allowed */
        r.si = 1;                       /* DOS date/time format */
        DSDX(r, oem);
        ESDI(r, findData);
        if (!Int21(&r))
            return FALSE;
        f->handle = r.ax;
        FromLfn(e);
    } else {
        SetDta(f);
        Regs(&r, 0x4E00);
        r.cx = 0x0016;
        DSDX(r, oem);
        if (!Int21(&r))
            return FALSE;
        FromDta(f, e);
    }
    while (IsDotEntry(e))
        if (!NextRaw(f, e)) {
            DosFindClose(f);
            return FALSE;
        }
    return TRUE;
}

/* Whether dir holds any visible folder. With long names DOS is asked for
   folders only (CH = required attributes), so big folders cost nothing. */
BOOL DosHasSubfolders(LPCSTR dir)
{
    char spec[MAX_PATH], oem[MAX_PATH];
    static DOSENTRY e;
    DOSFIND f;
    DOSREGS r;
    BOOL found = FALSE;

    if (lstrlen(dir) + 5 > MAX_PATH)
        return FALSE;
    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (lfn) {
        AnsiToOem(spec, oem);
        Regs(&r, 0x714E);
        r.cx = 0x1016;                  /* CH folders required, CL hidden/system allowed */
        r.si = 1;
        DSDX(r, oem);
        ESDI(r, findData);
        if (!Int21(&r))
            return FALSE;
        f.lfn = TRUE;
        f.handle = r.ax;
        FromLfn(&e);
    } else if (!DosFindFirst(spec, &f, &e)) {
        return FALSE;
    }
    do
        found = (e.attr & FA_DIR) && !ViewOptionsHides(e.attr) && !IsDotEntry(&e);
    while (!found && NextRaw(&f, &e));
    DosFindClose(&f);
    return found;
}

BOOL DosFindNext(DOSFIND *f, DOSENTRY *e)
{
    do {
        if (!NextRaw(f, e))
            return FALSE;
    } while (IsDotEntry(e));
    return TRUE;
}

void DosFindClose(DOSFIND *f)
{
    DOSREGS r;

    if (f->lfn && f->handle) {
        Regs(&r, 0x71A1);
        r.bx = f->handle;
        Int21(&r);
        f->handle = 0;
    }
}

/* Runs an LFN call taking a path in DS:DX (or the 8.3 equivalent). */
static BOOL PathCall(LPCSTR path, WORD lfnAx, WORD dosAx, WORD bx, WORD cx, DOSREGS *r)
{
    char oem[MAX_PATH];

    AnsiToOem(path, oem);
    Regs(r, lfn ? lfnAx : dosAx);
    r->bx = bx;
    r->cx = cx;
    DSDX(*r, oem);
    return Int21(r);
}

/* The path in its 8.3 (how 1) or long (how 2) form, of something that exists. */
static BOOL Canonical(LPCSTR path, LPSTR out, WORD how)
{
    char oem[MAX_PATH], result[MAX_PATH];
    DOSREGS r;

    if (!lfn) {
        lstrcpy(out, path);
        return TRUE;
    }
    AnsiToOem(path, oem);
    Regs(&r, 0x7160);
    r.cx = how;
    DSSI(r, oem);
    ESDI(r, result);
    if (!Int21(&r)) {
        lstrcpy(out, path);
        return FALSE;
    }
    OemToAnsi(result, out);
    return TRUE;
}

BOOL DosShortPath(LPCSTR path, LPSTR out)
{
    return Canonical(path, out, 1);
}

BOOL DosLongPath(LPCSTR path, LPSTR out)
{
    return Canonical(path, out, 2);
}

/* Makes dir (an 8.3 or long path) the current drive and directory. */
BOOL DosChangeFolder(LPCSTR dir)
{
    DOSREGS r;

    if (dir[0] && dir[1] == ':') {
        Regs(&r, 0x0E00);
        r.dx = (BYTE)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)dir[0]) - 'A';
        Int21(&r);
    }
    return PathCall(dir, 0x713B, 0x3B00, 0, 0, &r);
}

BOOL DosGetEntry(LPCSTR path, DOSENTRY *e)
{
    DOSFIND f;
    if (!DosFindFirst(path, &f, e))
        return FALSE;
    DosFindClose(&f);
    return TRUE;
}

BOOL DosMakeFolder(LPCSTR path)
{
    DOSREGS r;
    return PathCall(path, 0x7139, 0x3900, 0, 0, &r);
}

BOOL DosRemoveFolder(LPCSTR path)
{
    DOSREGS r;
    return PathCall(path, 0x713A, 0x3A00, 0, 0, &r);
}

BOOL DosRename(LPCSTR from, LPCSTR to)
{
    char a[MAX_PATH], b[MAX_PATH];
    DOSREGS r;

    AnsiToOem(from, a);
    AnsiToOem(to, b);
    Regs(&r, lfn ? 0x7156 : 0x5600);
    DSDX(r, a);                     /* DS:DX old name, ES:DI new name */
    ESDI(r, b);
    return Int21(&r);
}

BOOL DosDelete(LPCSTR path)
{
    DOSREGS r;
    return PathCall(path, 0x7141, 0x4100, 0, 0, &r);
}

WORD DosGetAttributes(LPCSTR path)
{
    DOSREGS r;
    if (!PathCall(path, 0x7143, 0x4300, 0, 0, &r))
        return 0xFFFF;
    return r.cx;
}

BOOL DosSetAttributes(LPCSTR path, WORD attr)
{
    DOSREGS r;
    return PathCall(path, 0x7143, 0x4301, 1, attr, &r);
}

/* Opens with 716Ch (long names) or the Windows calls (8.3); the handle is
   a DOS handle of this task either way, used with _lread and _lwrite. */
static HFILE Open(LPCSTR path, WORD mode, WORD action)
{
    char oem[MAX_PATH];
    DOSREGS r;
    HFILE h;

    AnsiToOem(path, oem);
    if (lfn) {
        Regs(&r, 0x716C);
        r.bx = mode;
        r.dx = action;
        DSSI(r, oem);
        return Int21(&r) ? (HFILE)r.ax : HFILE_ERROR;
    }
    h = action == 0x0001 ? _lopen(oem, mode) : _lcreat(oem, 0);
    if (h == HFILE_ERROR)
        error = 5;
    return h;
}

/* A character device (CON, CLIP$...), not a file. */
BOOL DosIsDevice(HFILE h)
{
    DOSREGS r;

    Regs(&r, 0x4400);
    r.bx = (WORD)h;
    return Int21(&r) && (r.dx & 0x80);
}

HFILE DosOpenRead(LPCSTR path)
{
    return Open(path, OF_READ | OF_SHARE_DENY_NONE, 0x0001);   /* open existing */
}

HFILE DosCreate(LPCSTR path)
{
    return Open(path, OF_WRITE, 0x0012);                        /* create or truncate */
}

BOOL DosGetFileTime(HFILE h, WORD *date, WORD *time)
{
    DOSREGS r;
    Regs(&r, 0x5700);
    r.bx = h;
    if (!Int21(&r))
        return FALSE;
    *time = r.cx;
    *date = r.dx;
    return TRUE;
}

BOOL DosSetFileTime(HFILE h, WORD date, WORD time)
{
    DOSREGS r;
    Regs(&r, 0x5701);
    r.bx = h;
    r.cx = time;
    r.dx = date;
    return Int21(&r);
}

BOOL DosDiskSpace(int drive, DWORD *total, DWORD *free)
{
    DOSREGS r;
    Regs(&r, 0x3600);
    r.dx = drive + 1;
    Int21(&r);                            /* 36h leaves the carry alone: AX = FFFFh is failure */
    if (r.ax == 0xFFFF)
        return FALSE;
    *free = (DWORD)r.ax * r.cx * r.bx;  /* sectors per cluster * bytes * clusters */
    *total = (DWORD)r.ax * r.cx * r.dx;
    return TRUE;
}

/* The label is a directory entry of the root with the volume attribute. */
BOOL DosVolumeLabel(int drive, LPSTR out)
{
    static char spec[] = "?:\\*.*";
    DOSFIND f;
    DOSREGS r;
    LPSTR dot;

    *out = '\0';
    spec[0] = (char)('A' + drive);
    SetDta(&f);
    Regs(&r, 0x4E00);
    r.cx = 0x0008;
    DSDX(r, spec);
    if (!Int21(&r) || !(f.dta[21] & 0x08))
        return FALSE;
    OemToAnsi((LPCSTR)(f.dta + 30), out);
    if ((dot = _fstrchr(out, '.')) != NULL)     /* 11 characters, stored as 8.3 */
        _fmemmove(dot, dot + 1, lstrlen(dot));
    return TRUE;
}

/* INT 21h run in DOS (V86 mode), DS:DX the start of a DOS memory block:
   for the FCB calls, which Windows does not carry from programs to DOS.
   AL back, -1 if DPMI failed. */
static int RealInt21(WORD ax, WORD dosSegment)
{
    static RMCALL rm;

    _fmemset(&rm, 0, sizeof rm);
    rm.eax = ax;
    rm.ds = dosSegment;
    return DpmiRealInt(0x21, &rm) ? (int)(BYTE)rm.eax : -1;
}

/* A new label (11 characters, "" none), the way LABEL sets one: the old
   label deleted, a volume entry made, through an extended FCB in DOS
   memory. */
BOOL DosSetVolumeLabel(int drive, LPCSTR label)
{
    DWORD block = GlobalDosAlloc(64);
    BYTE FAR *fcb = MK_FP(LOWORD(block), 0);
    char oem[12];
    BOOL done = FALSE;
    int i;

    if (!block)
        return FALSE;
    _fmemset(fcb, 0, 44);
    fcb[0] = 0xFF;                      /* extended: the attribute counts */
    fcb[6] = FA_LABEL;
    fcb[7] = (BYTE)(drive + 1);
    _fmemset(fcb + 8, '?', 11);
    RealInt21(0x1300, HIWORD(block));
    if (!*label) {
        done = TRUE;
    } else {
        lstrcpyn(oem, label, sizeof oem);
        AnsiUpper(oem);
        AnsiToOem(oem, oem);
        _fmemset(fcb + 8, ' ', 11);
        for (i = 0; i < 11 && oem[i]; i++)
            fcb[8 + i] = oem[i];
        if (RealInt21(0x1600, HIWORD(block)) == 0)
            RealInt21(0x1000, HIWORD(block));
        DosVolumeLabel(drive, oem);         /* kept? (DOSBox-X's folders keep their own) */
        AnsiUpper(oem);
        lstrcpyn(fcb, label, 12);
        AnsiUpper(fcb);
        done = !lstrcmp(oem, fcb);
    }
    GlobalDosFree(LOWORD(block));
    return done;
}

DWORD DosNow(void)
{
    DOSREGS d, t;
    Regs(&d, 0x2A00);
    Int21(&d);
    Regs(&t, 0x2C00);
    Int21(&t);
    return MAKELONG(((t.cx >> 8) << 11) | ((t.cx & 0xFF) << 5) | ((t.dx >> 8) / 2),
                    ((d.cx - 1980) << 9) | ((d.dx >> 8) << 5) | (d.dx & 0xFF));
}

BOOL DosCreateTextFile(LPCSTR path, LPCSTR text)
{
    HFILE h = DosCreate(path);
    UINT n = lstrlen(text);
    BOOL ok;

    if (h == HFILE_ERROR)
        return FALSE;
    ok = _lwrite(h, text, n) == n;
    _lclose(h);
    return ok;
}
