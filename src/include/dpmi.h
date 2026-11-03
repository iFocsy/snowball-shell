/*
 * DPMI's real-mode call (INT 31h AX=0300h): an interrupt run in DOS (V86
 * mode) with the registers given, for the DOS calls Windows does not carry
 * from programs to DOS with their pointers (FCBs, long file names). Buffers
 * go in DOS memory (GlobalDosAlloc). SNSHELL.DLL and SBSETUP.EXE use it.
 */
#ifndef DPMI_H
#define DPMI_H

typedef struct {
    DWORD edi, esi, ebp, reserved, ebx, edx, ecx, eax;
    WORD  flags, es, ds, fs, gs, ip, cs, sp, ss;
} RMCALL;

/* rm in: the registers (ss:sp 0: DPMI's stack); out: as DOS left them.
   FALSE if DPMI refused the call. */
static BOOL DpmiRealInt(BYTE number, RMCALL FAR *rm)
{
    WORD sel = SELECTOROF(rm), off = OFFSETOF(rm), n = number, failed = 0;

    _asm {
        push es
        push di
        mov es, sel
        mov di, off
        mov ax, 0300h
        mov bx, n
        xor cx, cx
        int 31h
        sbb ax, ax
        mov failed, ax
        pop di
        pop es
    }
    return !failed;
}

#endif
