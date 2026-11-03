/*
 * Memory. Blocks of SNSHELL.DLL's local heap: windows keep their own data
 * there and find it again through a 16-bit window word or property, so these
 * blocks travel as offsets. NULL stays NULL both ways: a plain conversion of a near
 * 0 would point at the start of the data segment.
 */
#include "internal.h"
#include <i86.h>

void *LocalHeapAlloc(UINT size)
{
    HLOCAL h = LocalAlloc(LPTR, size);
    return h ? (void NEAR *)h : NULL;
}

void LocalHeapFree(void *block)
{
    if (block)
        LocalFree((HLOCAL)FP_OFF(block));
}

WORD LocalHeapOffset(void *block)
{
    return block ? FP_OFF(block) : 0;
}

void *LocalHeapPointer(WORD offset)
{
    return offset ? (void NEAR *)offset : NULL;
}

/* A locked global block (*h, locked at *p) made size bytes, locked again;
   FALSE if there is no room (the block stays as it was). */
BOOL GlobalGrow(HGLOBAL *h, void far **p, DWORD size)
{
    HGLOBAL n;

    GlobalUnlock(*h);
    n = GlobalReAlloc(*h, size, GMEM_MOVEABLE);
    if (n)
        *h = n;
    *p = GlobalLock(*h);
    return n != NULL;
}
