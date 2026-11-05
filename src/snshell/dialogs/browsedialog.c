/* Open, to pick a file (the shell's Browse buttons), and Save As. */
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

/* A file to write, its name in file (shown first); ext: the extension
   added when none is typed. */
BOOL BrowseDialogSave(HWND owner, LPCSTR title, LPCSTR filter, LPCSTR ext, LPSTR file)
{
    OPENFILENAME ofn;

    _fmemset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = ext;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_LONGNAMES;
    return GetSaveFileName(&ofn);
}
