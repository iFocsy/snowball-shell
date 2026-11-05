/*
 * Send To, in the menus of files and folders: the items of the SendTo
 * folder by SNOWBALL.EXE - shortcuts to drives, folders and programs,
 * folders and programs themselves, and Desktop as Shortcut. Sending is a
 * drop: the files are copied there (moved with Shift), a program opens
 * them, the desktop gets shortcuts to them. The menu is filled as it opens;
 * the first time, the folder is made with Windows 98's own items.
 */
#include "internal.h"

static PATHLIST items;                  /* the menu filled last: CMD_SENDTO + i */

static void Folder(LPSTR out)
{
    lstrcpy(out, g_home);
    lstrcat(out, "SendTo");
}

/* Windows 98's items: the floppy drive and Desktop as Shortcut. */
static void Setup(LPCSTR dir)
{
    char path[MAX_PATH];
    SHORTCUT s;

    if (!DosMakeFolder(dir))
        return;
    if (GetDriveType(0) == DRIVE_REMOVABLE) {
        _fmemset(&s, 0, sizeof s);
        lstrcpy(s.target, "A:\\");
        s.show = SW_SHOWNORMAL;
        lstrcpy(path, dir);
        PathAppend(path, DosHasLongNames() ? "3\xBD Floppy (A)" SHC_EXT : "FLOPPY" SHC_EXT);
        ShortcutWrite(path, &s);
    }
    if (DosHasLongNames()) {
        lstrcpy(path, dir);
        PathAppend(path, "Desktop as Shortcut" DESKLINK_EXT);
        DosCreateTextFile(path, "");
    }
}

static void Empty(HMENU m)
{
    AppendMenu(m, MF_STRING | MF_GRAYED, CMD_SENDTO + MAX_SENDTO, "(Empty)");
}

static int CompareNames(const void far *a, const void far *b)
{
    return lstrcmpi(PathFileName(*(LPSTR far *)a), PathFileName(*(LPSTR far *)b));
}

/* The folder's items as they are now, by name. */
static void Fill(HMENU m)
{
    char dir[MAX_PATH], path[MAX_PATH];
    DOSENTRY f;
    LPSTR order[MAX_SENDTO], p;
    PATHLIST found;
    DOSFIND find;
    int i, n = 0;

    while (GetMenuItemCount(m) > 0)
        DeleteMenu(m, 0, MF_BYPOSITION);
    PathListFree(&items);
    Folder(dir);
    if (!PathIsFolder(dir))
        Setup(dir);
    PathListInit(&found);
    lstrcpy(path, dir);
    PathAppend(path, "*.*");
    if (DosFindFirst(path, &find, &f)) {
        do {
            if (found.count == MAX_SENDTO)
                break;
            if (ViewOptionsHides(f.attr))
                continue;
            lstrcpy(path, dir);
            PathAppend(path, f.name);
            PathListAdd(&found, path);
        } while (DosFindNext(&find, &f));
        DosFindClose(&find);
    }
    for (p = PathListLock(&found); *p; p = PathListNext(p))
        order[n++] = p;
    SortRecords(order, n, sizeof order[0], CompareNames);
    for (i = 0; i < n; i++)
        PathListAdd(&items, order[i]);
    PathListUnlock(&found);
    PathListFree(&found);
    for (i = 0; i < n; i++)
        AppendMenu(m, MF_OWNERDRAW, CMD_SENDTO + i, NULL);
    if (!n)
        Empty(m);
}

/* The path and label of the item of a command. */
static BOOL Item(UINT cmd, LPSTR path, LPSTR label)
{
    PathListAt(&items, cmd - CMD_SENDTO, path);
    if (label)
        DisplayNameOfFile(PathFileName(path), label);
    return *path != '\0';
}

static BOOL IsItem(UINT ctlType, UINT id)
{
    return ctlType == ODT_MENU && id >= CMD_SENDTO && id < CMD_SENDTO + MAX_SENDTO;
}

HMENU SendToMenu(void)
{
    HMENU m = CreatePopupMenu();
    Empty(m);
    return m;
}

#ifdef SNOWBALL_HOOKS
BOOL SendToItemLabel(UINT cmd, LPSTR label)     /* the tests' hooks (src\hooks) */
{
    char path[MAX_PATH];

    return Item(cmd, path, label);
}
#endif

BOOL SendToIsMenu(HMENU m)
{
    UINT id = m ? GetMenuItemID(m, 0) : 0;
    return id >= CMD_SENDTO && id <= CMD_SENDTO + MAX_SENDTO;
}

BOOL SendToMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT *res)
{
    MEASUREITEMSTRUCT FAR *mi = (MEASUREITEMSTRUCT FAR *)lp;
    DRAWITEMSTRUCT FAR *di = (DRAWITEMSTRUCT FAR *)lp;
    char path[MAX_PATH], label[MAX_PATH];
    HMENU m = (HMENU)HIWORD(lp);
    int i;

    HOOK_DESCRIBE_MENU_RES(msg, res, SendToDescribeMenu((HMENU)wp, (LPSTR)lp));
    switch (msg) {
    case WM_INITMENUPOPUP:
        if (!SendToIsMenu((HMENU)wp))
            return FALSE;
        Fill((HMENU)wp);
        *res = 0;
        return TRUE;
    case WM_MEASUREITEM:
        if (!IsItem(mi->CtlType, mi->itemID))
            return FALSE;
        Item(mi->itemID, path, label);
        mi->itemHeight = MENUITEM_H;
        mi->itemWidth = MenuItemWidth(label, 0);
        *res = TRUE;
        return TRUE;
    case WM_DRAWITEM:
        if (!IsItem(di->CtlType, di->itemID))
            return FALSE;
        Item(di->itemID, path, label);
        MenuItemPaint(di->hDC, &di->rcItem, IconForFile(path, PathIsFolder(path)), label,
                      di->itemState & ODS_SELECTED ? MIP_SELECTED : 0);
        *res = TRUE;
        return TRUE;
    case WM_MENUCHAR:
        if (!SendToIsMenu(m))
            return FALSE;
        for (i = 0; Item(CMD_SENDTO + i, path, label); i++)
            if (MenuItemMatchesKey(label, FALSE, wp)) {
                *res = MAKELONG(i, 2);          /* execute */
                return TRUE;
            }
        return FALSE;
    }
    return FALSE;
}

void SendToRun(HWND owner, PATHLIST *files, UINT cmd)
{
    char item[MAX_PATH], target[MAX_PATH];
    int effect = GetKeyState(VK_SHIFT) < 0 ? DE_MOVE : DE_COPY;

    if (!files->count || !Item(cmd, item, NULL))
        return;
    lstrcpy(target, item);
    if (PathIsDeskLink(item)) {
        DesktopFolder(target);
        effect = DE_LINK;
    } else if (LaunchIsProgram(item)) {
        effect = DE_RUN;                        /* the program, or its shortcut, opens them */
    } else if (PathIsShortcut(item)) {
        ShortcutTarget(item, target);
    }
    if (effect == DE_RUN || effect == DE_LINK || PathIsFolder(target))
        FileCommandsDrop(owner, target, IVK_FOLDER, files, effect);
}
