/*
 * File commands of the desktop and the folder windows: context menus, the
 * File and Edit menus, keys, the clipboard, renaming, new items and drops.
 * Both kinds of window hand their icon view here, so each command exists
 * once; Open, Up and Close stay with the windows, which differ there.
 */
#include "internal.h"

static BOOL InBin(HWND iv)
{
    return !lstrcmp(IconViewFolder(iv), BIN_PATH);
}

static BOOL InFolder(HWND iv)
{
    return PathOnDisk(IconViewFolder(iv));
}

/* Number of selected files and folders (not drives, My Computer, the bin). */
static int SelFiles(HWND iv)
{
    int n = 0, i = -1;
    while ((i = IconViewNextSelected(iv, i)) >= 0)
        if (IconViewItemKind(iv, i) <= IVK_FOLDER)
            n++;
    return n;
}

/* The desktop's My Documents among the selected (it stays where it is). */
static BOOL SelMyDocuments(HWND iv)
{
    int i = -1;
    while ((i = IconViewNextSelected(iv, i)) >= 0)
        if (DesktopIsMyDocuments(iv, i))
            return TRUE;
    return FALSE;
}

/* Their paths; drives too when asked. */
static int Selection(HWND iv, PATHLIST *l, BOOL drives)
{
    char path[MAX_PATH];
    int i = -1, k;

    PathListInit(l);
    while ((i = IconViewNextSelected(iv, i)) >= 0) {
        k = IconViewItemKind(iv, i);
        if ((k <= IVK_FOLDER || (drives && k == IVK_DRIVE)) && IconViewItemPath(iv, i, path))
            PathListAdd(l, path);
    }
    return l->count;
}

BOOL CALLBACK __export FileCommandsChangedProc(HWND w, LPARAM dir)
{
    SendMessage(w, SBM_CHANGED, 0, dir);
    return TRUE;
}

void FileCommandsFolderChanged(LPCSTR dir)
{
    EnumTaskWindows(GetCurrentTask(), (WNDENUMPROC)FileCommandsChangedProc, (LPARAM)dir);
    ShortcutKeysChanged(dir);
}

/* The folders the files were in, and dest, have changed. */
static void Changed(PATHLIST *files, LPCSTR dest)
{
    char dir[MAX_PATH], last[MAX_PATH];
    LPSTR p;

    last[0] = '\0';
    for (p = PathListLock(files); *p; p = PathListNext(p)) {
        lstrcpy(dir, p);
        PathToParent(dir);
        if (lstrcmpi(dir, last)) {
            FileCommandsFolderChanged(dir);
            lstrcpy(last, dir);
        }
    }
    PathListUnlock(files);
    if (dest)
        FileCommandsFolderChanged(dest);
}

/* ---- menus ---- */

static HMENU NewMenu(void)
{
    HMENU m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, CMD_NEWFOLDER, "&Folder");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_NEWTEXT, "Text Document");
    return m;
}

HMENU FileCommandsFileMenu(BOOL bin)
{
    HMENU m = CreatePopupMenu();

    if (bin) {
        AppendMenu(m, MF_STRING, CMD_RESTORE, "R&estore");
        AppendMenu(m, MF_STRING, CMD_EMPTYBIN, "Empty Recycle &Bin");
    } else {
        AppendMenu(m, MF_STRING, CMD_OPEN, "&Open");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_POPUP, (UINT)SendToMenu(), "Se&nd To");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_POPUP, (UINT)NewMenu(), "Ne&w");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_LINK, "Create &Shortcut");
    }
    AppendMenu(m, MF_STRING, CMD_DELETE, "&Delete");
    if (!bin) {
        AppendMenu(m, MF_STRING, CMD_RENAME, "Rena&me");
        AppendMenu(m, MF_STRING, CMD_PROPERTIES, "P&roperties");
    }
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_CLOSE, "&Close");
    return m;
}

static HMENU ArrangeMenu(void)
{
    HMENU m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, CMD_BYNAME + IVSORT_NAME, "by &Name");
    AppendMenu(m, MF_STRING, CMD_BYNAME + IVSORT_TYPE, "by &Type");
    AppendMenu(m, MF_STRING, CMD_BYNAME + IVSORT_SIZE, "by Si&ze");
    AppendMenu(m, MF_STRING, CMD_BYNAME + IVSORT_DATE, "by &Date");
    return m;
}

UINT FileCommandsModeCommand(int mode)
{
    return mode == IVM_THUMBS ? CMD_THUMBS : CMD_LARGE + mode;
}

HMENU FileCommandsModesMenu(HMENU m, int checked)
{
    int k;

    AppendMenu(m, MF_STRING, CMD_LARGE + IVM_LARGE, "Lar&ge Icons");
    AppendMenu(m, MF_STRING, CMD_LARGE + IVM_SMALL, "S&mall Icons");
    AppendMenu(m, MF_STRING, CMD_LARGE + IVM_LIST, "&List");
    AppendMenu(m, MF_STRING, CMD_LARGE + IVM_DETAILS, "&Details");
    AppendMenu(m, MF_STRING, CMD_THUMBS, "T&humbnails");
    for (k = IVM_LARGE; k <= IVM_THUMBS; k++)
        CheckMenuItem(m, FileCommandsModeCommand(k), MF_BYCOMMAND | (k == checked ? MF_CHECKED : MF_UNCHECKED));
    return m;
}

static HMENU ModesMenu(HMENU m)
{
    return FileCommandsModesMenu(m, -1);
}

HMENU FileCommandsViewMenu(void)
{
    HMENU m = ModesMenu(CreatePopupMenu());
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_POPUP, (UINT)ArrangeMenu(), "Arrange &Icons");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_FILTER, "&Filter...");
    AppendMenu(m, MF_STRING, CMD_REFRESH, "&Refresh\tF5");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_FOLDEROPTS, "Folder &Options...");
    return m;
}

/* The desktop and folder windows each remember the view last chosen. */
static LPCSTR ViewSection(HWND iv)
{
    return (GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP) ? "Desktop" : "Folder";
}

void FileCommandsLoadView(HWND iv)
{
    LPCSTR section = ViewSection(iv);
    if (!(GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP))
        IconViewSetMode(iv, SettingsGetInt(section, "View", IVM_LARGE));
    IconViewSort(iv, SettingsGetInt(section, "Sort", IVSORT_NAME), SettingsGetInt(section, "Descending", 0));
}

void FileCommandsSaveView(HWND iv)
{
    LPCSTR section = ViewSection(iv);
    if (!(GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP))
        SettingsSetInt(section, "View", IconViewMode(iv));
    SettingsSetInt(section, "Sort", IconViewSortKey(iv));
    SettingsSetInt(section, "Descending", IconViewSortDescending(iv));
}

HMENU FileCommandsEditMenu(void)
{
    HMENU m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, CMD_UNDO, "&Undo\tCtrl+Z");     /* named as it opens */
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_CUT, "Cu&t\tCtrl+X");
    AppendMenu(m, MF_STRING, CMD_COPY, "&Copy\tCtrl+C");
    AppendMenu(m, MF_STRING, CMD_PASTE, "&Paste\tCtrl+V");
    AppendMenu(m, MF_STRING, CMD_PASTELINK, "Paste &Shortcut");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_SELECTALL, "Select &All\tCtrl+A");
    AppendMenu(m, MF_STRING, CMD_INVERT, "&Invert Selection");
    AppendMenu(m, MF_STRING, CMD_SELECTFILES, "Select &Files...");
    return m;
}

BOOL FileCommandsAvailable(HWND iv, UINT cmd)
{
    int files = SelFiles(iv), n = IconViewSelectedCount(iv), k = IconViewItemKind(iv, IconViewNextSelected(iv, -1));
    BOOL bin = InBin(iv);

    if ((cmd == CMD_CUT || cmd == CMD_DELETE || cmd == CMD_RENAME) && SelMyDocuments(iv))
        return FALSE;
    switch (cmd) {
    case CMD_OPEN:      return n > 0 && !bin;
    case CMD_CUT:
    case CMD_COPY:
    case CMD_SENDTO:    return files && !bin;
    case CMD_PASTE:
    case CMD_PASTELINK: return InFolder(iv) && ClipboardHasFiles();
    case CMD_LINK:      return (files || k == IVK_DRIVE) && !bin;
    case CMD_DELETE:    return files != 0;
    case CMD_RENAME:    return files == 1 && n == 1 && !bin;
    case CMD_NEWFOLDER:
    case CMD_NEWTEXT:   return InFolder(iv);
    case CMD_SELECTALL:
    case CMD_INVERT:
    case CMD_SELECTFILES: return IconViewCount(iv) > 0;
    case CMD_FILTER:    return InFolder(iv);
    case CMD_RESTORE:   return files && bin;
    case CMD_EMPTYBIN:  return !RecycleBinIsEmpty();
    }
    return TRUE;
}

void FileCommandsInitMenu(HMENU m, HWND iv)
{
    static const UINT cmds[] = { CMD_OPEN, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_PASTELINK, CMD_LINK, CMD_DELETE,
                                 CMD_RENAME, CMD_NEWFOLDER, CMD_NEWTEXT, CMD_SELECTALL, CMD_INVERT, CMD_RESTORE,
                                 CMD_EMPTYBIN };
    int i, k;

    for (i = 0; i < sizeof cmds / sizeof cmds[0]; i++)
        EnableMenuItem(m, cmds[i], MF_BYCOMMAND | (FileCommandsAvailable(iv, cmds[i]) ? MF_ENABLED : MF_GRAYED));
    for (k = IVM_LARGE; k <= IVM_THUMBS; k++)
        CheckMenuItem(m, FileCommandsModeCommand(k), MF_BYCOMMAND | (k == IconViewMode(iv) ? MF_CHECKED : MF_UNCHECKED));
    if (GetMenuState(m, CMD_UNDO, MF_BYCOMMAND) != (UINT)-1) {   /* "Undo Delete", or grey */
        char text[40];
        BOOL can = UndoMenuText(text);
        ModifyMenu(m, CMD_UNDO, MF_BYCOMMAND | MF_STRING | (can ? MF_ENABLED : MF_GRAYED), CMD_UNDO, text);
    }
    for (k = GetMenuItemCount(m) - 1; k >= 0; k--)
        if (SendToIsMenu(GetSubMenu(m, k)))
            EnableMenuItem(m, k, MF_BYPOSITION | (FileCommandsAvailable(iv, CMD_SENDTO) ? MF_ENABLED : MF_GRAYED));
}

/* The actions of the type of the document a menu was made for. */
static char actions[FT_ACTIONS][16];

/* "&Print" for "print", as the menu shows an action. */
static void ActionLabel(LPCSTR action, LPSTR out)
{
    wsprintf(out, "&%s", action);
    out[1] = (char)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)out[1]);
}

/* Open, named as the type's default action, then its other actions and
   Open With for a document (path: the item the menu is for); Open alone
   for anything else. */
static void OpenItems(HMENU m, int k, LPCSTR path)
{
    char label[20], ext[8];
    int n = 0, i;

    if (k == IVK_FILE && path && !PathIsProgram(path) && !PathIsShortcut(path) && !PathIsDeskLink(path)) {
        lstrcpyn(ext, *PathExtension((LPSTR)path) ? PathExtension((LPSTR)path) + 1 : "", sizeof ext);
        n = FileTypeActions(ext, actions, FT_ACTIONS);
        ActionLabel(n ? actions[0] : "open", label);
        AppendMenu(m, MF_STRING, CMD_OPEN, label);
        for (i = 1; i < n; i++) {
            ActionLabel(actions[i], label);
            AppendMenu(m, MF_STRING, CMD_ACTION + i, label);
        }
        if (QuickViewCan(path))
            AppendMenu(m, MF_STRING, CMD_QUICKVIEW, "Quick &View");
        AppendMenu(m, MF_STRING, CMD_OPENWITH, "Open Wit&h...");
    } else {
        AppendMenu(m, MF_STRING, CMD_OPEN, "&Open");
    }
}

/* The right-click menu of items of one kind (IVK_*), path the first. */
static void ItemsMenu(HMENU m, int k, LPCSTR path)
{
    OpenItems(m, k, path);
    if (k >= IVK_FOLDER && k <= IVK_COMPUTER)
        AppendMenu(m, MF_STRING, CMD_EXPLORE, "&Explore");
    if (k == IVK_FOLDER || k == IVK_DRIVE)
        AppendMenu(m, MF_STRING, CMD_FIND, "Fi&nd...");
    if (k == IVK_BIN) {
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_EMPTYBIN, "Empty Recycle &Bin");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_PROPERTIES, "P&roperties");
    } else if (k == IVK_DRIVE) {
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_LINK, "Create &Shortcut");
    } else if (k != IVK_COMPUTER) {
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_POPUP, (UINT)SendToMenu(), "Se&nd To");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_CUT, "Cu&t");
        AppendMenu(m, MF_STRING, CMD_COPY, "&Copy");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_LINK, "Create &Shortcut");
        AppendMenu(m, MF_STRING, CMD_DELETE, "&Delete");
        AppendMenu(m, MF_STRING, CMD_RENAME, "Rena&me");
    }
    if (k != IVK_BIN) {
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_PROPERTIES, "P&roperties");
    }
}

void FileCommandsContextMenu(HWND owner, HWND iv)
{
    HMENU m = CreatePopupMenu();
    int n = IconViewSelectedCount(iv), k = IconViewItemKind(iv, IconViewNextSelected(iv, -1));
    char path[MAX_PATH];
    POINT p;

    if (n && InBin(iv)) {
        AppendMenu(m, MF_STRING, CMD_RESTORE, "R&estore");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_DELETE, "&Delete");
    } else if (n) {
        ItemsMenu(m, k, IconViewItemPath(iv, IconViewNextSelected(iv, -1), path) ? path : NULL);
    } else {
        if (!(GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP))
            AppendMenu(m, MF_POPUP, (UINT)ModesMenu(CreatePopupMenu()), "&View");
        AppendMenu(m, MF_POPUP, (UINT)ArrangeMenu(), "Arrange &Icons");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_REFRESH, "R&efresh");
        if (InBin(iv)) {
            AppendMenu(m, MF_SEPARATOR, 0, NULL);
            AppendMenu(m, MF_STRING, CMD_EMPTYBIN, "Empty Recycle &Bin");
        }
        if (InFolder(iv)) {
            char undo[40];
            AppendMenu(m, MF_SEPARATOR, 0, NULL);
            AppendMenu(m, MF_STRING, CMD_PASTE, "&Paste");
            AppendMenu(m, MF_STRING, CMD_PASTELINK, "Paste &Shortcut");
            if (UndoMenuText(undo)) {           /* "Undo Delete", as Windows 98's, when there is one */
                *_fstrchr(undo, '\t') = '\0';
                AppendMenu(m, MF_STRING, CMD_UNDO, undo);
            }
            AppendMenu(m, MF_SEPARATOR, 0, NULL);
            AppendMenu(m, MF_POPUP, (UINT)NewMenu(), "Ne&w");
        }
        if (!InBin(iv)) {
            AppendMenu(m, MF_SEPARATOR, 0, NULL);
            AppendMenu(m, MF_STRING, CMD_PROPERTIES, "P&roperties");
        }
    }
    FileCommandsInitMenu(m, iv);
    if (!IconViewMenuPoint(iv, &p))      /* a key asked: at the item; else at the pointer */
        GetCursorPos(&p);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, 0, owner, NULL);
    DestroyMenu(m);
}

/* The same menu for one file or folder outside the views (an item of the
   Start menu), its command done here; Rename asks in a dialog. Sort by
   Name, if offered, is the caller's to do. */
static int itemKind = -1;               /* FileCommandsKindMenu's; else the path says */

UINT FileCommandsItemMenu(HWND owner, LPCSTR path, BOOL sortable, LPSTR renamed)
{
    char dir[MAX_PATH], name[MAX_PATH];
    HMENU m = CreatePopupMenu();
    int k = itemKind >= 0 ? itemKind : PathIsFolder(path) ? IVK_FOLDER : IVK_FILE, done = 0;
    UINT cmd;
    PATHLIST l;

    ItemsMenu(m, k, path);
    if (sortable)                       /* after Rename */
        InsertMenu(m, GetMenuItemCount(m) - 2, MF_BYPOSITION | MF_STRING, CMD_BYNAME + IVSORT_NAME, "Sort &by Name");
    cmd = PopupMenuTrack(m, owner);
    DestroyMenu(m);
    *renamed = '\0';
    lstrcpy(dir, path);
    PathToParent(dir);
    PathListInit(&l);
    PathListAdd(&l, path);
    switch (cmd) {
    case CMD_OPEN:
        if (k == IVK_COMPUTER || k == IVK_BIN)
            FolderWindowOpen(path);
        else
            LaunchItem(owner, path);
        break;
    case CMD_EMPTYBIN:   RecycleBinEmptyAll(owner); break;
    case CMD_OPENWITH:   OpenWithDialogShow(owner, path); break;
    case CMD_QUICKVIEW:  QuickViewShow(path); break;
    case CMD_EXPLORE:    FolderWindowExplore(path); break;
    case CMD_FIND:       FindShow(path); break;
    case CMD_CUT:
    case CMD_COPY:       ClipboardPut(owner, &l, cmd == CMD_CUT); break;
    case CMD_DELETE:     FileCommandsDelete(owner, &l, GetKeyState(VK_SHIFT) < 0); break;
    case CMD_PROPERTIES:
        if (k == IVK_BIN)
            RecycleBinPropertiesShow(owner);
        else
            PropertiesShow(owner, &l, k);
        break;
    case CMD_LINK:
        ShortcutCreate(path, dir, FALSE);
        FileCommandsFolderChanged(dir);
        break;
    case CMD_RENAME:
        if (k == IVK_FOLDER)
            DisplayNameOfFolder(PathFileName((LPSTR)path), name);
        else
            DisplayNameOfFile(PathFileName((LPSTR)path), name);
        while (PromptDialogShow(owner, "Rename", "&New name:", name, sizeof name) &&
               (done = FileCommandsRename(owner, path, name, renamed)) == 0)
            ;                                   /* an error: the name can be corrected */
        if (done <= 0)
            *renamed = '\0';
        break;
    default:
        if (cmd >= CMD_SENDTO && cmd < CMD_SENDTO + MAX_SENDTO)
            SendToRun(owner, &l, cmd);
        else if (cmd > CMD_ACTION && cmd < CMD_ACTION + FT_ACTIONS)
            LaunchAction(owner, path, actions[cmd - CMD_ACTION]);
    }
    PathListFree(&l);
    return cmd;
}

/* ---- commands ---- */

/* Asks as Windows 98 does, then sends to the Recycle Bin or deletes. */
void FileCommandsDelete(HWND owner, PATHLIST *l, BOOL purge)
{
    char text[MAX_PATH + 100], name[MAX_PATH];
    LPSTR first = PathListLock(l);
    BOOL toBin = !purge && RecycleBinCovers(first), folder = PathIsFolder(first);
    LPCSTR title = l->count > 1 ? "Confirm Multiple File Delete"
                 : folder ? "Confirm Folder Delete" : "Confirm File Delete";

    DisplayNameOfFile(PathFileName(first), name);
    PathListUnlock(l);
    if (l->count > 1)
        wsprintf(text, toBin ? "Are you sure you want to send these %d items to the Recycle Bin?"
                             : "Are you sure you want to delete these %d items?", l->count);
    else if (folder)
        wsprintf(text, toBin ? "Are you sure you want to remove the folder '%s' and move all its contents to the Recycle Bin?"
                             : "Are you sure you want to remove the folder '%s' and all its contents?", (LPSTR)name);
    else
        wsprintf(text, toBin ? "Are you sure you want to send '%s' to the Recycle Bin?"
                             : "Are you sure you want to delete '%s'?", (LPSTR)name);
    if ((!toBin || SettingsGetInt("Recycle Bin", "Confirm", 1)) &&
        MessageBox(owner, text, title, MB_ICONQUESTION | MB_YESNO) != IDYES)
        return;
    FileOperationRun(owner, toBin ? FOP_DELETE : FOP_PURGE, l, NULL);
    Changed(l, toBin ? BIN_PATH : NULL);
    if (toBin)
        RecycleBinTrim(owner);
}

static void Delete(HWND owner, HWND iv, BOOL purge)
{
    PATHLIST l;

    if (!FileCommandsAvailable(iv, CMD_DELETE))
        return;
    if (Selection(iv, &l, FALSE)) {
        if (InBin(iv))
            RecycleBinPurge(owner, &l);
        else
            FileCommandsDelete(owner, &l, purge);
    }
    PathListFree(&l);
}

static void Paste(HWND owner, HWND iv, BOOL link)
{
    char dir[MAX_PATH];
    PATHLIST l;
    BOOL cut;
    LPSTR p;

    lstrcpy(dir, IconViewFolder(iv));
    if (!InFolder(iv) || !ClipboardGet(owner, &l, &cut))
        return;
    if (link) {
        for (p = PathListLock(&l); *p; p = PathListNext(p))
            ShortcutCreate(p, dir, FALSE);
        PathListUnlock(&l);
        FileCommandsFolderChanged(dir);
    } else {
        FileOperationRun(owner, cut ? FOP_MOVE : FOP_COPY, &l, dir);
        if (cut)
            ClipboardClear(owner);
        Changed(&l, dir);
    }
    PathListFree(&l);
}

/* Create Shortcut: next to the items, or on the desktop for drives. */
static void Shortcuts(HWND owner, HWND iv)
{
    char dir[MAX_PATH];
    PATHLIST l;
    LPSTR p;

    if (!Selection(iv, &l, TRUE))
        return;
    if (InFolder(iv)) {
        lstrcpy(dir, IconViewFolder(iv));
    } else if (MessageBox(owner, "Windows cannot create a shortcut here.\n\nDo you want the shortcut to be placed on the desktop instead?",
                          "Shortcut", MB_ICONQUESTION | MB_YESNO) == IDYES) {
        DesktopFolder(dir);
    } else {
        PathListFree(&l);
        return;
    }
    for (p = PathListLock(&l); *p; p = PathListNext(p))
        ShortcutCreate(p, dir, FALSE);
    PathListUnlock(&l);
    PathListFree(&l);
    FileCommandsFolderChanged(dir);
}

/* New Folder / New Text Document, then rename it in place. */
static void NewItem(HWND owner, HWND iv, BOOL folder)
{
    char dir[MAX_PATH], path[MAX_PATH];

    lstrcpy(dir, IconViewFolder(iv));
    if (!PathMakeUnique(path, dir, folder ? "New Folder" : "New Text Document", folder ? "" : ".txt"))
        return;
    if (!(folder ? DosMakeFolder(path) : DosCreateTextFile(path, ""))) {
        FileOperationError(owner, "create", path, DosLastError());
        return;
    }
    FileCommandsFolderChanged(dir);
    IconViewEditLabel(iv, IconViewFind(iv, PathFileName(path)));
}

/* Renames from to name (typed: spaces around it are dropped) with the
   checks and questions of Windows 98; to gets the new path. Returns 1 when
   renamed, 0 on an error (the name can be corrected), -1 when the user
   kept the old name. */
int FileCommandsRename(HWND owner, LPCSTR from, LPCSTR typed, LPSTR to)
{
    static const char bad[] = "\\/:*?\"<>|";
    char text[MAX_PATH], dir[MAX_PATH];
    LPSTR name = text, end, c;

    lstrcpyn(text, typed, sizeof text);
    while (*name == ' ')
        name++;
    for (end = name + lstrlen(name); end > name && end[-1] == ' '; )
        *--end = '\0';
    if (!*name) {
        MessageBox(owner, "You must type a filename.", "Rename", MB_ICONSTOP | MB_OK);
        return 0;
    }
    for (c = name; *c; c++)
        if (_fstrchr(bad, *c)) {
            MessageBox(owner, "A filename cannot contain any of the following characters:\n\\ / : * ? \" < > |",
                       "Rename", MB_ICONSTOP | MB_OK);
            return 0;
        }
    lstrcpy(dir, from);
    PathToParent(dir);
    if (lstrlen(dir) + lstrlen(name) + 6 > MAX_PATH) {
        FileOperationError(owner, "rename", from, 206);
        return 0;
    }
    lstrcpy(to, dir);
    PathAppend(to, name);
    if (!PathIsFolder(from) && DisplayNameHidesExtension(PathFileName((LPSTR)from)))
        lstrcat(to, PathExtension((LPSTR)from));      /* it was not shown, so it was not typed */
    else if (!PathIsFolder(from) && lstrcmpi(PathExtension((LPSTR)from), PathExtension(to)) &&
             MessageBox(owner, "If you change a filename extension, the file may become unusable.\n\nAre you sure you want to change it?",
                        "Rename", MB_ICONQUESTION | MB_YESNO) != IDYES)
        return -1;
    if (!lstrcmp(from, to))
        return -1;
    if (lstrcmpi(from, to) && DosGetAttributes(to) != 0xFFFF) {
        FileOperationError(owner, "rename", from, 80);
        return 0;
    }
    if (!DosRename(from, to)) {
        FileOperationError(owner, "rename", from, DosLastError());
        return 0;
    }
    UndoBegin(UNDO_RENAME);
    UndoPair(from, to);
    UndoEnd();
    FileCommandsFolderChanged(dir);
    return 1;
}

BOOL FileCommandsRenamed(HWND owner, HWND iv)
{
    char text[MAX_PATH], from[MAX_PATH], to[MAX_PATH];
    int i = IconViewFocused(iv), k = IconViewItemKind(iv, i), done;

    if (k > IVK_FOLDER || InBin(iv))
        return TRUE;                            /* nothing to rename: just close */
    IconViewEditText(iv, text, sizeof text);
    IconViewItemPath(iv, i, from);
    done = FileCommandsRename(owner, from, text, to);
    if (done > 0)
        IconViewSelect(iv, IconViewFind(iv, PathFileName(to)));
    return done != 0;
}

/* Of the selection, else of the folder shown (My Computer: the system). */
static void Properties(HWND owner, HWND iv)
{
    char dir[MAX_PATH];
    PATHLIST l;
    int k = IconViewItemKind(iv, IconViewNextSelected(iv, -1));

    if (k == IVK_BIN || (k < 0 && InBin(iv))) {
        RecycleBinPropertiesShow(owner);
        return;
    }
    PathListInit(&l);
    if (k == IVK_COMPUTER || (k < 0 && !*IconViewFolder(iv))) {
        PathListAdd(&l, "");
        k = IVK_COMPUTER;
    } else if (k >= 0) {
        Selection(iv, &l, TRUE);
    } else if (InFolder(iv)) {
        lstrcpy(dir, IconViewFolder(iv));
        PathListAdd(&l, dir);
        k = lstrlen(dir) <= 3 ? IVK_DRIVE : IVK_FOLDER;
    }
    if (l.count)
        PropertiesShow(owner, &l, k);
    PathListFree(&l);
}

/* Edit menu: the clipboard, undo and selecting; FALSE for another command. */
static BOOL EditCommand(HWND owner, HWND iv, UINT cmd)
{
    PATHLIST l;

    switch (cmd) {
    case CMD_CUT:
    case CMD_COPY:
        if (Selection(iv, &l, FALSE))
            ClipboardPut(owner, &l, cmd == CMD_CUT);
        PathListFree(&l);
        break;
    case CMD_PASTE:
    case CMD_PASTELINK:
        Paste(owner, iv, cmd == CMD_PASTELINK);
        break;
    case CMD_SELECTALL:
    case CMD_INVERT:
        IconViewSelectAll(iv, cmd == CMD_INVERT);
        break;
    case CMD_UNDO:
        UndoLast(owner);
        break;
    case CMD_SELECTFILES: {             /* the pattern asked for is kept for next time */
        static char pattern[64] = "*.*";
        if (PromptDialogShow(owner, "Select Files", "&Files:", pattern, sizeof pattern) && pattern[0])
            IconViewSelectMatching(iv, pattern);
        break;
    }
    default:
        return FALSE;
    }
    return TRUE;
}

/* File menu: shortcuts, delete, rename, new items, the Recycle Bin, properties. */
static BOOL FileCommand(HWND owner, HWND iv, UINT cmd)
{
    PATHLIST l;

    switch (cmd) {
    case CMD_LINK:
        Shortcuts(owner, iv);
        break;
    case CMD_DELETE:
        Delete(owner, iv, GetKeyState(VK_SHIFT) < 0);
        break;
    case CMD_RENAME:
        if (IconViewSelectedCount(iv) == 1 && SelFiles(iv) == 1 && !InBin(iv))
            IconViewEditLabel(iv, IconViewNextSelected(iv, -1));
        break;
    case CMD_NEWFOLDER:
    case CMD_NEWTEXT:
        if (InFolder(iv))
            NewItem(owner, iv, cmd == CMD_NEWFOLDER);
        break;
    case CMD_RESTORE:
        if (Selection(iv, &l, FALSE))
            RecycleBinRestore(owner, &l);
        PathListFree(&l);
        break;
    case CMD_EMPTYBIN:
        RecycleBinEmptyAll(owner);
        break;
    case CMD_PROPERTIES:
        Properties(owner, iv);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

/* The selected items handed on: Find, Open With, Quick View, Explore, Send To, actions. */
static BOOL ItemCommand(HWND owner, HWND iv, UINT cmd)
{
    char dir[MAX_PATH];
    PATHLIST l;
    LPSTR p;
    int k;

    switch (cmd) {
    case CMD_FIND:                      /* in the folder selected, else the one shown */
        k = IconViewItemKind(iv, IconViewNextSelected(iv, -1));
        if ((k == IVK_FOLDER || k == IVK_DRIVE) && IconViewItemPath(iv, IconViewNextSelected(iv, -1), dir))
            FindShow(dir);
        else
            FindShow(InFolder(iv) ? IconViewFolder(iv) : "");
        break;
    case CMD_OPENWITH:
        if (IconViewItemPath(iv, IconViewNextSelected(iv, -1), dir))
            OpenWithDialogShow(owner, dir);
        break;
    case CMD_QUICKVIEW:
        if (IconViewItemPath(iv, IconViewNextSelected(iv, -1), dir))
            QuickViewShow(dir);
        break;
    case CMD_EXPLORE:
        if (IconViewItemKind(iv, IconViewNextSelected(iv, -1)) == IVK_COMPUTER)
            FolderWindowExplore("");
        else if (IconViewItemPath(iv, IconViewNextSelected(iv, -1), dir))
            FolderWindowExplore(dir);
        break;
    default:
        if (cmd >= CMD_SENDTO && cmd < CMD_SENDTO + MAX_SENDTO) {
            if (Selection(iv, &l, FALSE))
                SendToRun(owner, &l, cmd);
            PathListFree(&l);
        } else if (cmd > CMD_ACTION && cmd < CMD_ACTION + FT_ACTIONS) {     /* each file selected */
            if (Selection(iv, &l, FALSE)) {
                for (p = PathListLock(&l); *p; p = PathListNext(p))
                    LaunchAction(owner, p, actions[cmd - CMD_ACTION]);
                PathListUnlock(&l);
            }
            PathListFree(&l);
        } else {
            return FALSE;
        }
    }
    return TRUE;
}

/* View menu: refresh, options, and the view's mode and order (remembered). */
static BOOL ViewCommand(HWND owner, HWND iv, UINT cmd)
{
    char dir[MAX_PATH];

    switch (cmd) {
    case CMD_REFRESH:
        lstrcpy(dir, IconViewFolder(iv));
        SendMessage(owner, SBM_CHANGED, 0, (LPARAM)(LPSTR)dir);
        break;
    case CMD_FOLDEROPTS:
        FolderOptionsShow();
        break;
    default:
        if (cmd >= CMD_LARGE && cmd <= CMD_DETAILS)
            IconViewSetMode(iv, cmd - CMD_LARGE);
        else if (cmd == CMD_THUMBS)
            IconViewSetMode(iv, IVM_THUMBS);
        else if (cmd >= CMD_BYNAME && cmd <= CMD_BYDATE)
            IconViewSort(iv, cmd - CMD_BYNAME, FALSE);
        else
            return FALSE;
        FileCommandsSaveView(iv);
    }
    return TRUE;
}

/* The command done on the view's selection; FALSE when it is not one of these. */
BOOL FileCommandsRun(HWND owner, HWND iv, UINT cmd)
{
    return EditCommand(owner, iv, cmd) || FileCommand(owner, iv, cmd) ||
           ItemCommand(owner, iv, cmd) || ViewCommand(owner, iv, cmd);
}

BOOL FileCommandsKey(HWND owner, HWND iv, WPARAM key)
{
    UINT cmd = 0;

    if (GetKeyState(VK_CONTROL) < 0) {
        switch (key) {
        case 'X': cmd = CMD_CUT; break;
        case 'C': cmd = CMD_COPY; break;
        case 'V': cmd = CMD_PASTE; break;
        case 'A': cmd = CMD_SELECTALL; break;
        case 'Z': cmd = CMD_UNDO; break;
        }
    } else if (key == VK_RETURN && GetKeyState(VK_MENU) < 0) {
        cmd = CMD_PROPERTIES;
    } else {
        switch (key) {
        case VK_DELETE: cmd = CMD_DELETE; break;
        case VK_F2:     cmd = CMD_RENAME; break;
        case VK_F5:     cmd = CMD_REFRESH; break;
        case VK_F3:     cmd = CMD_FIND; break;
        }
    }
    return cmd && FileCommandsRun(owner, iv, cmd);
}

/* ---- drops ---- */

int FileCommandsDropItem(HWND iv, int x, int y)
{
    char path[MAX_PATH];
    int i = IconViewHitTest(iv, x, y);

    if (IconViewItemKind(iv, i) == IVK_FILE && (!IconViewItemPath(iv, i, path) || !LaunchIsProgram(path)))
        return -1;
    return i;
}

/* The item under a drop, or the view's own folder (-1): path and kind. */
int FileCommandsDropTarget(HWND iv, int item, LPSTR target)
{
    if (item >= 0) {
        IconViewItemPath(iv, item, target);
        return IconViewItemKind(iv, item);
    }
    lstrcpy(target, IconViewFolder(iv));
    return InBin(iv) ? IVK_BIN : InFolder(iv) ? IVK_FOLDER : IVK_COMPUTER;
}

/* Windows 98 rules: Ctrl copies, Shift moves, both make shortcuts; else
   moves on the same drive, copies to another, programs get shortcuts.
   A program as target runs with the files, the Recycle Bin deletes them. */
int FileCommandsDropEffect(LPCSTR target, int kind, PATHLIST *files, BOOL ctrl, BOOL shift)
{
    char parent[MAX_PATH];
    BOOL programs = TRUE, here = TRUE, drives = FALSE, same, inside;
    LPSTR first, p;

    switch (kind) {
    case IVK_BIN:      return DE_MOVE;
    case IVK_COMPUTER: return DE_NONE;
    case IVK_FILE:     return DE_RUN;
    }
    first = PathListLock(files);
    same = (*first | 0x20) == (*target | 0x20);
    for (p = first; *p; p = PathListNext(p)) {
        if (PathIsInside(target, p))
            break;                              /* onto itself or into itself */
        lstrcpy(parent, p);
        PathToParent(parent);
        here = here && !lstrcmpi(parent, target);
        drives = drives || lstrlen(p) <= 3;
        programs = programs && LaunchIsProgram(p) && !PathIsShortcut(p);
    }
    inside = *p != '\0';
    PathListUnlock(files);
    if (inside)
        return DE_NONE;
    if (drives || (ctrl && shift) || (programs && !ctrl && !shift))
        return DE_LINK;
    if (ctrl)
        return DE_COPY;
    if (shift || same)
        return here ? DE_NONE : DE_MOVE;
    return DE_COPY;
}

void FileCommandsDrop(HWND owner, LPCSTR target, int kind, PATHLIST *files, int effect)
{
    char path[MAX_PATH], args[128];
    LPSTR p;

    if (effect == DE_NONE)
        return;
    if (kind == IVK_BIN) {
        FileCommandsDelete(owner, files, FALSE);
        return;
    }
    if (effect == DE_RUN) {                     /* program "file1 file2..." */
        args[0] = '\0';
        for (p = PathListLock(files); *p; p = PathListNext(p)) {
            DosShortPath(p, path);
            if (lstrlen(args) + lstrlen(path) + 2 < sizeof args) {
                if (*args)
                    lstrcat(args, " ");
                lstrcat(args, path);
            }
        }
        PathListUnlock(files);
        LaunchWith(owner, target, args);
    } else if (effect == DE_LINK) {
        for (p = PathListLock(files); *p; p = PathListNext(p))
            ShortcutCreate(p, target, FALSE);
        PathListUnlock(files);
        FileCommandsFolderChanged(target);
    } else {
        FileOperationRun(owner, effect == DE_MOVE ? FOP_MOVE : FOP_COPY, files, target);
        Changed(files, target);
    }
}

void FileCommandsDropFiles(HWND iv, HDROP drop)
{
    char path[MAX_PATH], target[MAX_PATH];
    PATHLIST l;
    POINT pt;
    int i, n = DragQueryFile(drop, (UINT)-1, NULL, 0), kind;

    PathListInit(&l);
    for (i = 0; i < n; i++) {
        DragQueryFile(drop, i, path, sizeof path);
        PathListAdd(&l, path);
    }
    DragQueryPoint(drop, &pt);
    DragFinish(drop);
    kind = FileCommandsDropTarget(iv, FileCommandsDropItem(iv, pt.x, pt.y), target);
    FileCommandsDrop(GetParent(iv), target, kind, &l,
           FileCommandsDropEffect(target, kind, &l, GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0));
    PathListFree(&l);
}

/* The same for an item of a kind its path does not tell (the tree's My
   Computer, Recycle Bin, a drive): IVK_*. */
UINT FileCommandsKindMenu(HWND owner, LPCSTR path, int kind, LPSTR renamed)
{
    UINT cmd;

    itemKind = kind;
    cmd = FileCommandsItemMenu(owner, path, FALSE, renamed);
    itemKind = -1;
    return cmd;
}
