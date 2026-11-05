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

/* The desktop and folder windows each remember the view last chosen. */
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

    if (!FileMenusAvailable(iv, CMD_DELETE))
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
        if (FileMenusAvailable(iv, CMD_RENAME))
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
    case CMD_DOSHERE:                   /* in the folder or drive selected, else the one shown */
        k = IconViewItemKind(iv, IconViewNextSelected(iv, -1));
        if (!((k == IVK_FOLDER || k == IVK_DRIVE) && IconViewItemPath(iv, IconViewNextSelected(iv, -1), dir)))
            lstrcpy(dir, IconViewFolder(iv));
        LaunchDosPrompt(owner, dir, NULL, "MS-DOS Prompt");
        break;
    case CMD_FORMAT:
    case CMD_COPYDISK:
        if (!IconViewItemPath(iv, IconViewNextSelected(iv, -1), dir))
            break;
        if (cmd == CMD_FORMAT)
            FormatDialogShow(owner, (dir[0] | 0x20) - 'a');
        else
            FormatDialogCopyDisk(owner, (dir[0] | 0x20) - 'a');
        break;
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
    case CMD_PIN:
        if (IconViewItemPath(iv, IconViewNextSelected(iv, -1), dir))
            FrequentPin(dir, !FrequentIsPinned(dir));
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
                    LaunchAction(owner, p, FileMenusAction(cmd));
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
        FolderOptionsShow(iv);
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
        FolderViewsSave(iv);
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
        programs = programs && !PathIsShortcut(p) && LaunchIsProgram(p);    /* (no shortcut read) */
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

