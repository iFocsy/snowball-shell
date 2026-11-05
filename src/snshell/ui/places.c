/*
 * Places, as Look in and the address bar list them: the Desktop, My
 * Computer, the drives, and the folders down to the one shown, each under
 * its parent. In owner-drawn combo boxes (CBS_OWNERDRAWFIXED |
 * CBS_HASSTRINGS) whose items' text is the place's path.
 */
#include "internal.h"

#define INDENT      10                  /* per level */

static void Add(HWND cb, LPCSTR path, int level)
{
    int i = (int)SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)path);
    SendMessage(cb, CB_SETITEMDATA, i, level);
}

void PlacesFill(HWND cb, LPCSTR path)
{
    char part[MAX_PATH], root[4];
    LPCSTR p;
    int d, level, n;

    SendMessage(cb, CB_RESETCONTENT, 0, 0L);
    Add(cb, PLACE_DESKTOP, 0);
    Add(cb, "", 1);
    for (d = 0; d < 26; d++) {
        if (!GetDriveType(d))
            continue;
        wsprintf(root, "%c:\\", 'A' + d);
        Add(cb, root, 2);
        if (PathOnDisk(path) && (path[0] | 0x20) == 'a' + d)
            for (p = path + 3, level = 3; *p; level++) {
                while (*p && *p != '\\')
                    p++;
                lstrcpyn(part, path, (int)(p - path) + 1);
                Add(cb, part, level);
                if (*p)
                    p++;
            }
    }
    n = (int)SendMessage(cb, CB_GETCOUNT, 0, 0L);
    for (d = 0; d < n; d++) {
        SendMessage(cb, CB_GETLBTEXT, d, (LPARAM)(LPSTR)part);
        if (!lstrcmpi(part, path))
            SendMessage(cb, CB_SETCURSEL, d, 0L);
    }
}

void PlacesName(LPCSTR path, LPSTR out)
{
    if (!lstrcmp(path, PLACE_DESKTOP))
        lstrcpy(out, "Desktop");
    else if (!lstrcmp(path, BIN_PATH))
        lstrcpy(out, "Recycle Bin");
    else if (!*path)
        lstrcpy(out, "My Computer");
    else if (lstrlen(path) <= 3)
        DriveName((path[0] | 0x20) - 'a', out);
    else
        lstrcpy(out, PathFileName((LPSTR)path));
}

HICON PlacesIcon(LPCSTR path)
{
    if (!lstrcmp(path, PLACE_DESKTOP))
        return IconStock(IDI_SNOWBALL);
    if (!lstrcmp(path, BIN_PATH))
        return IconStock(RecycleBinIsEmpty() ? IDI_BIN : IDI_BINFULL);
    if (!*path)
        return IconStock(IDI_COMPUTER);
    if (lstrlen(path) <= 3)
        return IconForDrive((path[0] | 0x20) - 'a');
    return IconForFile(path, TRUE);
}

/* Windows 3.1 does not say whether the combo box's own field is drawn
   (ODS_COMBOBOXEDIT came with Windows 95): it is, unless the list is down
   and the item lies on a row of it. The field shows no indentation. */
void PlacesDraw(const DRAWITEMSTRUCT FAR *d)
{
    char path[MAX_PATH], name[80];
    BOOL field;

    if ((int)d->itemID < 0)
        return;
    field = !SendMessage(d->hwndItem, CB_GETDROPPEDSTATE, 0, 0L) || d->rcItem.top % PLACE_H;
    SendMessage(d->hwndItem, CB_GETLBTEXT, d->itemID, (LPARAM)(LPSTR)path);
    PlacesName(path, name);
    PaintListItem(d, d->rcItem.left + 2 + (field ? 0 : INDENT * LOWORD(d->itemData)), PlacesIcon(path), name, FALSE);
}
