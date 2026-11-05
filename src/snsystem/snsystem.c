/* SNSYSTEM.DLL's entry and exit. */
#include "snsystem.h"

HINSTANCE g_hInst;

int FAR PASCAL LibMain(HINSTANCE hInst, WORD dataSeg, WORD heapSize, LPSTR cmdLine)
{
    g_hInst = hInst;
    return 1;
}

int FAR PASCAL WEP(int exitType)
{
    HookRemove();
    CpuEnd(FALSE);
    FileDialogsServe(NULL);
    DrawFree();
    return 1;
}
