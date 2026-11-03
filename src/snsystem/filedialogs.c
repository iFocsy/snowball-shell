/*
 * Open and Save As for every program (part of SNSYSTEM.DLL). COMMDLG's
 * GetOpenFileName and GetSaveFileName are patched in memory to jump here;
 * the shell shows its Windows 98 dialog (in its own task, where the file
 * views are) while the program waits here, its windows still painting.
 * Programs that change the dialog (a hook or their own template) get
 * COMMDLG's, as Windows 95 gives 16-bit programs.
 */
#include "snsystem.h"

#define OPEN    0
#define SAVE    1

typedef BOOL (FAR PASCAL *FILEFN)(OPENFILENAME FAR *);

/* COMMDLG's first five bytes run here, then a jump back past them. */
typedef struct {
    BYTE code[5];
    BYTE jmp;                           /* 0xEA: jmp far */
    WORD off, sel;
} TRAMP;

static struct {
    FARPROC entry;
    BYTE    saved[5];
} patch[2];
static HGLOBAL trampMem;
static UINT trampCode;                  /* code alias of the trampolines */
static HWND server;

/* An exported function's first instructions, as the loader leaves them:
   "mov ax,DGROUP" (or "push ds, pop ax"), then "nop, inc bp" or "inc bp,
   push bp". Five bytes of whole instructions that can run elsewhere. */
static BOOL Standard(const BYTE FAR *p)
{
    return (p[0] == 0xB8 || (p[0] == 0x1E && p[1] == 0x58)) &&
           ((p[3] == 0x90 && p[4] == 0x45) || (p[3] == 0x45 && p[4] == 0x55));
}

/* Copies n bytes into a code segment through a data alias of it. */
static void Poke(FARPROC at, const BYTE FAR *bytes, int n)
{
    UINT alias = AllocSelector(SELECTOROF(at));

    PrestoChangoSelector(SELECTOROF(at), alias);
    _fmemcpy(MAKELP(alias, OFFSETOF(at)), bytes, n);
    FreeSelector(alias);
}

static BOOL Patch(int i, FARPROC entry, FARPROC ours, TRAMP FAR *t)
{
    BYTE jump[5];

    if (!entry || !Standard((const BYTE FAR *)entry))
        return FALSE;
    LockSegment(SELECTOROF(entry));     /* never discarded: the patch would go with it */
    patch[i].entry = entry;
    _fmemcpy(patch[i].saved, (const BYTE FAR *)entry, 5);
    _fmemcpy(t->code, patch[i].saved, 5);
    t->jmp = 0xEA;
    t->off = OFFSETOF(entry) + 5;
    t->sel = SELECTOROF(entry);
    jump[0] = 0xEA;
    *(WORD *)&jump[1] = OFFSETOF(ours);
    *(WORD *)&jump[3] = SELECTOROF(ours);
    Poke(entry, jump, 5);
    return TRUE;
}

static void Unpatch(int i)
{
    if (patch[i].entry) {
        Poke(patch[i].entry, patch[i].saved, 5);
        UnlockSegment(SELECTOROF(patch[i].entry));
        patch[i].entry = NULL;
    }
}

static BOOL Original(int i, OPENFILENAME FAR *ofn)
{
    return ((FILEFN)MAKELP(trampCode, i * sizeof(TRAMP)))(ofn);
}

/* The calling program's current folder, with long names ("C:\" and what
   DOS gives, through LFN 7147h, else 47h). */
static void CurrentDir(LPSTR out)
{
    BYTE drive = 0;
    LPSTR rest = out + 3;
    BOOL failed = FALSE;

    _asm {
        mov ah, 19h
        int 21h
        mov drive, al
    }
    wsprintf(out, "%c:\\", 'A' + drive);
    _asm {
        push ds
        push si
        mov ax, 7147h
        mov dl, drive
        inc dl
        lds si, rest
        stc
        int 21h
        jnc done
        mov ah, 47h
        mov dl, drive
        inc dl
        int 21h
        jnc done
        mov failed, 1
    done:
        pop si
        pop ds
    }
    if (failed)
        *rest = '\0';
    OemToAnsi(out, out);
}

/* As COMMDLG does: the folder of the chosen file becomes the program's. */
static void SetDir(LPCSTR file, int nameAt)
{
    char dir[128];
    LPSTR d = dir;
    BYTE drive = (BYTE)((file[0] | 0x20) - 'a');

    if (file[1] != ':' || nameAt < 3 || nameAt > (int)sizeof dir)
        return;
    lstrcpyn(dir, file, nameAt > 3 ? nameAt : 4);   /* "C:\DIR" or "C:\" */
    AnsiToOem(dir, dir);
    _asm {
        push ds
        mov ah, 0Eh
        mov dl, drive
        int 21h
        mov ax, 713Bh                   /* long names, else DOS's own */
        lds dx, d
        stc
        int 21h
        jnc done
        mov ah, 3Bh
        int 21h
    done:
        pop ds
    }
}

/* The shell shows the dialog; meanwhile this program handles its own
   messages (and keeps a WM_QUIT for later). */
static BOOL Ask(int i, OPENFILENAME FAR *ofn)
{
    FILEREQUEST req;
    MSG msg;
    BOOL quit = FALSE;
    int code = 0;

    if (!IsWindow(server) || (ofn->Flags & (OFN_ENABLEHOOK | OFN_ENABLETEMPLATE | OFN_ENABLETEMPLATEHANDLE)))
        return Original(i, ofn);
    req.ofn = ofn;
    req.task = GetCurrentTask();
    req.save = i == SAVE;
    req.done = req.ok = FALSE;
    CurrentDir(req.dir);
    if (!PostMessage(server, SBM_FILEDLG, 0, (LPARAM)(FILEREQUEST FAR *)&req))
        return Original(i, ofn);
    while (!req.done) {
        if (!GetMessage(&msg, NULL, 0, 0)) {
            quit = TRUE;
            code = msg.wParam;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (quit)
        PostQuitMessage(code);
    if (req.ok && !(ofn->Flags & OFN_NOCHANGEDIR))
        SetDir(ofn->lpstrFile, ofn->nFileOffset);
    return req.ok;
}

BOOL FAR PASCAL __export FileDialogsGetOpen(OPENFILENAME FAR *ofn)
{
    return Ask(OPEN, ofn);
}

BOOL FAR PASCAL __export FileDialogsGetSave(OPENFILENAME FAR *ofn)
{
    return Ask(SAVE, ofn);
}

/* The shell's window that shows the dialogs; NULL undoes the patches. */
BOOL SNSYSTEM_API FileDialogsServe(HWND w)
{
    HMODULE commdlg = GetModuleHandle("COMMDLG");
    TRAMP FAR *t;

    server = w;
    if (!w) {
        Unpatch(OPEN);
        Unpatch(SAVE);
        if (trampMem) {
            FreeSelector(trampCode);
            GlobalUnlock(trampMem);
            GlobalFree(trampMem);
            trampMem = NULL;
        }
        return TRUE;
    }
    if (trampMem || !commdlg)
        return trampMem != NULL;
    trampMem = GlobalAlloc(GMEM_FIXED | GMEM_SHARE, 2 * sizeof(TRAMP));
    if (!trampMem)
        return FALSE;
    t = (TRAMP FAR *)GlobalLock(trampMem);
    trampCode = AllocDStoCSAlias(SELECTOROF(t));
    Patch(OPEN, GetProcAddress(commdlg, "GetOpenFileName"), (FARPROC)FileDialogsGetOpen, &t[OPEN]);
    Patch(SAVE, GetProcAddress(commdlg, "GetSaveFileName"), (FARPROC)FileDialogsGetSave, &t[SAVE]);
    return patch[OPEN].entry && patch[SAVE].entry;
}
