/* Open, to pick a file (the shell's Browse buttons). */
#include "internal.h"

/* The name chosen in file, "" at first. */
BOOL BrowseDialogShow(HWND owner, LPCSTR title, LPCSTR filter, LPSTR file, BOOL longNames)
{
    OPENFILENAME ofn;

    _fmemset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | (longNames ? OFN_LONGNAMES : 0);
    return GetOpenFileName(&ofn);
}
