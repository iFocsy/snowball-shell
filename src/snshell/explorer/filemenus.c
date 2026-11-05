/* Menus of files and folders: the File, Edit and View menus, each item's
   right-click menu (its type's actions, Open With, Send To...) and which
   commands apply to the selection now. filecommands.c does them. */
#include "internal.h"

static BOOL InBin(HWND iv)
{
    return !lstrcmp(IconViewFolder(iv), BIN_PATH);
}

static BOOL InFolder(HWND iv)
{
    return PathOnDisk(IconViewFolder(iv));
}

static HMENU NewMenu(void)
{
    HMENU m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, CMD_NEWFOLDER, "&Folder");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_NEWTEXT, "Text Document");
    return m;
}

HMENU FileMenusFile(BOOL bin)
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

UINT FileMenusModeCommand(int mode)
{
    return mode == IVM_THUMBS ? CMD_THUMBS : CMD_LARGE + mode;
}

HMENU FileMenusModes(HMENU m, int checked)
{
    int k;

    AppendMenu(m, MF_STRING, CMD_LARGE + IVM_LARGE, "Lar&ge Icons");
    AppendMenu(m, MF_STRING, CMD_LARGE + IVM_SMALL, "S&mall Icons");
    AppendMenu(m, MF_STRING, CMD_LARGE + IVM_LIST, "&List");
    AppendMenu(m, MF_STRING, CMD_LARGE + IVM_DETAILS, "&Details");
    AppendMenu(m, MF_STRING, CMD_THUMBS, "T&humbnails");
    for (k = IVM_LARGE; k <= IVM_THUMBS; k++)
        CheckMenuItem(m, FileMenusModeCommand(k), MF_BYCOMMAND | (k == checked ? MF_CHECKED : MF_UNCHECKED));
    return m;
}

static HMENU ModesMenu(HMENU m)
{
    return FileMenusModes(m, -1);
}

HMENU FileMenusView(void)
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

HMENU FileMenusEdit(void)
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

/* The selection and the view, looked at once for all the commands of a menu. */
typedef struct {
    int  files, kind;                   /* selected files and folders; the first one's IVK_* */
    BOOL bin;                           /* the view is the Recycle Bin */
    BYTE have;                          /* A_* */
} SELECTION;

#define A_SEL       0x01                /* something is selected */
#define A_FILES     0x02                /* files or folders among it */
#define A_ONE       0x04                /* one item, a file or folder */
#define A_NOBIN     0x08                /* the view is not the Recycle Bin */
#define A_BIN       0x10                /* it is */
#define A_FOLDER    0x20                /* the view is a folder on a disk */
#define A_ITEMS     0x40                /* the view has items */
#define A_NOMYDOCS  0x80                /* the desktop's My Documents is not among them (it stays) */

/* What each command needs; one not listed always applies. */
static const struct { UINT cmd; BYTE needs; } Needs[] = {
    { CMD_OPEN, A_SEL | A_NOBIN },              { CMD_CUT, A_FILES | A_NOBIN | A_NOMYDOCS },
    { CMD_COPY, A_FILES | A_NOBIN },            { CMD_SENDTO, A_FILES | A_NOBIN },
    { CMD_DELETE, A_FILES | A_NOMYDOCS },       { CMD_RENAME, A_ONE | A_NOBIN | A_NOMYDOCS },
    { CMD_NEWFOLDER, A_FOLDER },                { CMD_NEWTEXT, A_FOLDER },
    { CMD_SELECTALL, A_ITEMS },                 { CMD_INVERT, A_ITEMS },
    { CMD_SELECTFILES, A_ITEMS },               { CMD_FILTER, A_FOLDER },
    { CMD_RESTORE, A_FILES | A_BIN },
};

static void LookAt(HWND iv, SELECTION *s)
{
    BOOL myDocs = FALSE;
    int i = -1, n = 0, k;

    s->files = 0;
    s->kind = IconViewItemKind(iv, IconViewNextSelected(iv, -1));
    s->bin = InBin(iv);
    while ((i = IconViewNextSelected(iv, i)) >= 0) {
        n++;
        if ((k = IconViewItemKind(iv, i)) <= IVK_FOLDER)
            s->files++;
        if (k == IVK_FOLDER && DesktopIsMyDocuments(iv, i))
            myDocs = TRUE;
    }
    s->have = (n ? A_SEL : 0) | (s->files ? A_FILES : 0) | (n == 1 && s->files == 1 ? A_ONE : 0) |
              (s->bin ? A_BIN : A_NOBIN) | (InFolder(iv) ? A_FOLDER : 0) | (IconViewCount(iv) ? A_ITEMS : 0) |
              (myDocs ? 0 : A_NOMYDOCS);
}

static BOOL Available(const SELECTION *s, UINT cmd)
{
    int i;

    switch (cmd) {
    case CMD_PASTE:
    case CMD_PASTELINK: return (s->have & A_FOLDER) && ClipboardHasFiles();
    case CMD_LINK:      return (s->files || s->kind == IVK_DRIVE) && !s->bin;
    case CMD_EMPTYBIN:  return !RecycleBinIsEmpty();
    }
    for (i = 0; i < sizeof Needs / sizeof Needs[0]; i++)
        if (Needs[i].cmd == cmd)
            return (s->have & Needs[i].needs) == Needs[i].needs;
    return TRUE;
}

BOOL FileMenusAvailable(HWND iv, UINT cmd)
{
    SELECTION s;

    LookAt(iv, &s);
    return Available(&s, cmd);
}

void FileMenusInit(HMENU m, HWND iv)
{
    static const UINT cmds[] = { CMD_OPEN, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_PASTELINK, CMD_LINK, CMD_DELETE,
                                 CMD_RENAME, CMD_NEWFOLDER, CMD_NEWTEXT, CMD_SELECTALL, CMD_INVERT, CMD_RESTORE,
                                 CMD_EMPTYBIN };
    SELECTION s;
    int i, k;

    LookAt(iv, &s);
    for (i = 0; i < sizeof cmds / sizeof cmds[0]; i++)
        if (GetMenuState(m, cmds[i], MF_BYCOMMAND) != (UINT)-1)     /* only those it has: the bin is not read for nothing */
            EnableMenuItem(m, cmds[i], MF_BYCOMMAND | (Available(&s, cmds[i]) ? MF_ENABLED : MF_GRAYED));
    for (k = IVM_LARGE; k <= IVM_THUMBS; k++)
        CheckMenuItem(m, FileMenusModeCommand(k), MF_BYCOMMAND | (k == IconViewMode(iv) ? MF_CHECKED : MF_UNCHECKED));
    if (GetMenuState(m, CMD_UNDO, MF_BYCOMMAND) != (UINT)-1) {   /* "Undo Delete", or grey */
        char text[40];
        BOOL can = UndoMenuText(text);
        ModifyMenu(m, CMD_UNDO, MF_BYCOMMAND | MF_STRING | (can ? MF_ENABLED : MF_GRAYED), CMD_UNDO, text);
    }
    for (k = GetMenuItemCount(m) - 1; k >= 0; k--)
        if (SendToIsMenu(GetSubMenu(m, k)))
            EnableMenuItem(m, k, MF_BYPOSITION | (Available(&s, CMD_SENDTO) ? MF_ENABLED : MF_GRAYED));
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
        if (k == IVK_FILE && path && (PathIsProgram(path) || PathIsShortcut(path)))
            AppendMenu(m, MF_STRING, CMD_PIN, FrequentIsPinned(path) ? "Unp&in from Start menu" : "P&in to Start menu");
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
    if ((k == IVK_FOLDER || k == IVK_DRIVE) && path && PathOnDisk(path))
        AppendMenu(m, MF_STRING, CMD_DOSHERE, "MS-DOS Prompt &Here");
    if (k == IVK_DRIVE && path && GetDriveType((path[0] | 0x20) - 'a') == DRIVE_REMOVABLE) {
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_FORMAT, "F&ormat...");
        AppendMenu(m, MF_STRING, CMD_COPYDISK, "Cop&y Disk...");
    }
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

void FileMenusContext(HWND owner, HWND iv)
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
        if (InFolder(iv) && !(GetWindowLong(iv, GWL_STYLE) & IVS_DESKTOP))    /* (not on the desktop's, as Windows 98's) */
            AppendMenu(m, MF_STRING, CMD_DOSHERE, "MS-DOS Prompt &Here");
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
    FileMenusInit(m, iv);
    if (!IconViewMenuPoint(iv, &p))      /* a key asked: at the item; else at the pointer */
        GetCursorPos(&p);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, 0, owner, NULL);
    DestroyMenu(m);
}

/* The same menu for one file or folder outside the views (an item of the
   Start menu), its command done here; Rename asks in a dialog. Sort by
   Name, if offered, is the caller's to do. k: its IVK_* kind. */
static UINT ItemMenu(HWND owner, LPCSTR path, int k, BOOL sortable, LPSTR renamed)
{
    char dir[MAX_PATH], name[MAX_PATH];
    HMENU m = CreatePopupMenu();
    int done = 0;
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
    case CMD_PIN:        FrequentPin(path, !FrequentIsPinned(path)); break;
    case CMD_DOSHERE:    LaunchDosPrompt(owner, path, NULL, "MS-DOS Prompt"); break;
    case CMD_FORMAT:     FormatDialogShow(owner, (path[0] | 0x20) - 'a'); break;
    case CMD_COPYDISK:   FormatDialogCopyDisk(owner, (path[0] | 0x20) - 'a'); break;
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

UINT FileMenusItem(HWND owner, LPCSTR path, BOOL sortable, LPSTR renamed)
{
    return ItemMenu(owner, path, PathIsFolder(path) ? IVK_FOLDER : IVK_FILE, sortable, renamed);
}

/* The same for an item of a kind its path does not tell (the tree's My
   Computer, Recycle Bin, a drive): IVK_*. */
UINT FileMenusKind(HWND owner, LPCSTR path, int kind, LPSTR renamed)
{
    return ItemMenu(owner, path, kind, FALSE, renamed);
}

LPCSTR FileMenusAction(UINT cmd)
{
    return actions[cmd - CMD_ACTION];
}
