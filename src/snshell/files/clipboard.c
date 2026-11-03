/* Cut, Copy and Paste of files through the clipboard, in a private format:
   a WORD (1 = cut) followed by the path list. */
#include "internal.h"

static UINT Format(void)
{
    static UINT format;
    if (!format)
        format = RegisterClipboardFormat("Snowball Files");
    return format;
}

void ClipboardPut(HWND owner, PATHLIST *files, BOOL cut)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (DWORD)files->used + 3);
    LPSTR p;

    if (!h)
        return;
    p = GlobalLock(h);
    *(WORD FAR *)p = (WORD)cut;
    _fmemcpy(p + 2, PathListLock(files), files->used + 1);
    PathListUnlock(files);
    GlobalUnlock(h);
    if (OpenClipboard(owner)) {
        EmptyClipboard();
        SetClipboardData(Format(), h);
        CloseClipboard();
    } else {
        GlobalFree(h);
    }
}

BOOL ClipboardGet(HWND owner, PATHLIST *files, BOOL *cut)
{
    HGLOBAL h;
    LPSTR p;

    PathListInit(files);
    if (!OpenClipboard(owner))
        return FALSE;
    if ((h = GetClipboardData(Format())) != NULL && (p = GlobalLock(h)) != NULL) {
        *cut = *(WORD FAR *)p;
        for (p += 2; *p; p = PathListNext(p))
            PathListAdd(files, p);
        GlobalUnlock(h);
    }
    CloseClipboard();
    return files->count > 0;
}

BOOL ClipboardHasFiles(void)
{
    return IsClipboardFormatAvailable(Format());
}

void ClipboardClear(HWND owner)
{
    if (OpenClipboard(owner)) {
        EmptyClipboard();
        CloseClipboard();
    }
}
