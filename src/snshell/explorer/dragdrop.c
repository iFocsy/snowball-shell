/*
 * Dragging icons out of a view. The drag runs its own message loop until
 * the button comes up (Esc cancels). Targets are Snowball's views - a
 * folder, drive, program or the Recycle Bin under the cursor, else the
 * view's own folder - the folder tree of Explorer windows, and any window
 * that accepts files, which gets WM_DROPFILES the way File Manager drops.
 * The right button asks first. A taskbar button stands for its window: a
 * folder window's folder, or the program if it takes files; resting on it
 * brings the window forward.
 */
#include "internal.h"

#define CMD_CANCEL  9                   /* menu ids are DE_* */

enum { T_NONE, T_VIEW, T_TREE, T_OTHER, T_TASK };

static HCURSOR cursors[5];              /* by DE_*; DE_NONE = no drop */

static HWND TopLevel(HWND w)
{
    while (GetWindowLong(w, GWL_STYLE) & WS_CHILD)
        w = GetParent(w);
    return w;
}

/* What is under the screen point: a Snowball view or tree, a window
   accepting files, or a taskbar button (its window); *w gets it. */
static int TargetAt(POINT p, HWND *w)
{
    HWND top, task;
    char cls[16];

    if ((*w = WindowFromPoint(p)) == NULL)
        return T_NONE;
    GetClassName(*w, cls, sizeof cls);
    if (!lstrcmp(cls, ICONVIEW_CLASS))
        return T_VIEW;
    if (!lstrcmp(cls, TREEVIEW_CLASS))
        return T_TREE;
    top = TopLevel(*w);
    if (top == g_taskbar && (task = TaskbarTaskAt(p)) != NULL) {
        *w = task;
        return T_TASK;
    }
    GetClassName(top, cls, sizeof cls);
    if (!lstrcmp(cls, FOLDERWINDOW_CLASS) || !lstrcmp(cls, DESKTOP_CLASS)) {
        *w = GetDlgItem(top, ID_VIEW);
        return T_VIEW;
    }
    for (;; *w = GetParent(*w)) {
        if (GetWindowLong(*w, GWL_EXSTYLE) & WS_EX_ACCEPTFILES)
            return T_OTHER;
        if (*w == top)
            return T_NONE;
    }
}

static void Highlight(int type, HWND w, int item)
{
    if (type == T_VIEW)
        IconViewDropHighlight(w, item);
    else if (type == T_TREE)
        TreeViewDropHighlight(w, item);
}

/* WM_DROPFILES for other programs: a header (size, point, non-client)
   followed by 8.3 paths, as SHELL.DLL's DragQueryFile reads it. */
static void DropForeign(HWND w, POINT screen, PATHLIST *files)
{
    char path[MAX_PATH];
    LPSTR list = PathListLock(files), p, q;
    WORD size = 9;
    POINT client = screen;
    HGLOBAL h;

    for (p = list; *p; p = PathListNext(p)) {
        DosShortPath(p, path);
        size += lstrlen(path) + 1;
    }
    h = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE | GMEM_ZEROINIT, size);
    if (h && (q = GlobalLock(h)) != NULL) {
        ScreenToClient(w, &client);
        ((WORD FAR *)q)[0] = 8;
        ((int FAR *)q)[1] = client.x;
        ((int FAR *)q)[2] = client.y;
        ((WORD FAR *)q)[3] = SendMessage(w, WM_NCHITTEST, 0, MAKELPARAM(screen.x, screen.y)) != HTCLIENT;
        for (q += 8, p = list; *p; p = PathListNext(p)) {
            DosShortPath(p, q);
            AnsiUpper(q);
            q += lstrlen(q) + 1;
        }
        GlobalUnlock(h);
        PostMessage(w, WM_DROPFILES, (WPARAM)h, 0L);
    }
    PathListUnlock(files);
}

/* Right-button drop: Move Here / Copy Here / Create Shortcut(s) Here. */
static int AskEffect(HWND owner)
{
    HMENU m = CreatePopupMenu();
    UINT effect;

    AppendMenu(m, MF_STRING, DE_MOVE, "&Move Here");
    AppendMenu(m, MF_STRING, DE_COPY, "&Copy Here");
    AppendMenu(m, MF_STRING, DE_LINK, "Create &Shortcut(s) Here");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_CANCEL, "Cancel");
    effect = PopupMenuTrack(m, owner);
    DestroyMenu(m);
    return effect <= DE_LINK ? effect : DE_NONE;
}

/* A drag under way: what is dragged, and what it is over now. */
typedef struct {
    HWND     src, w, lit;               /* w: the target window; lit: the view or tree lit up */
    int      type, litType;             /* T_* */
    int      item, kind, effect;        /* the item under the cursor or -1, its IVK_*, DE_* */
    POINT    p;                         /* the cursor, on the screen */
    char     target[MAX_PATH];          /* the path dropped on */
    PATHLIST files;
} DRAG;

/* What is dragged: the selected files, folders and drives, nothing out of
   the Recycle Bin. */
static void Gather(HWND src, PATHLIST *files)
{
    char path[MAX_PATH];
    int i = -1;

    PathListInit(files);
    if (lstrcmp(IconViewFolder(src), BIN_PATH))
        while ((i = IconViewNextSelected(src, i)) >= 0)
            if (IconViewItemKind(src, i) <= IVK_DRIVE && IconViewItemPath(src, i, path))
                PathListAdd(files, path);
}

/* The drag cursors, loaded the first time. */
static void LoadCursors(void)
{
    static const int ids[] = { IDR_NODROP, IDR_DRAGMOVE, IDR_DRAGCOPY, IDR_DRAGLINK, IDR_DRAGMOVE };
    int i;

    for (i = 0; i < 5; i++)
        if (!cursors[i])
            cursors[i] = LoadCursor(g_hArt, MAKEINTRESOURCE(ids[i]));
}

/* A taskbar button: a folder window's folder, or a program taking files. */
static void AimTask(DRAG *d)
{
    GetClassName(d->w, d->target, sizeof d->target);
    if (!lstrcmp(d->target, FOLDERWINDOW_CLASS) && *IconViewFolder(GetDlgItem(d->w, ID_VIEW))) {
        lstrcpy(d->target, IconViewFolder(GetDlgItem(d->w, ID_VIEW)));
        d->kind = IVK_FOLDER;
        d->item = 0;
    } else if (GetWindowLong(d->w, GWL_EXSTYLE) & WS_EX_ACCEPTFILES) {
        d->effect = DE_COPY;
    }
}

/* What is under the cursor: the target window, and in Snowball's the item
   there (or -1) with its path and kind; other programs' windows take a copy. */
static void Aim(DRAG *d)
{
    POINT c;

    GetCursorPos(&d->p);
    d->type = TargetAt(d->p, &d->w);
    c = d->p;
    if (d->w)
        ScreenToClient(d->w, &c);
    d->item = -1;
    d->effect = d->type == T_OTHER ? DE_COPY : DE_NONE;
    if (d->type == T_VIEW) {
        d->item = FileCommandsDropItem(d->w, c.x, c.y);
        if (d->w == d->src && d->item >= 0 && IconViewNextSelected(d->w, d->item - 1) == d->item)
            d->item = -1;                       /* over the dragged items themselves */
        d->kind = FileCommandsDropTarget(d->w, d->item, d->target);
    } else if (d->type == T_TREE && (d->item = TreeViewHitTest(d->w, c.x, c.y)) >= 0) {
        TreeViewItemPath(d->w, d->item, d->target);
        d->kind = TreeViewItemKind(d->w, d->item);
    } else if (d->type == T_TASK) {     /* a folder window's: its folder */
        AimTask(d);
    }
}

/* Dropping on a Snowball target: move, copy, link or run, by the keys held. */
static void Effect(DRAG *d)
{
    if (d->type == T_VIEW || (d->type == T_TREE && d->item >= 0) || (d->type == T_TASK && d->item >= 0))
        d->effect = FileCommandsDropEffect(d->target, d->kind, &d->files, GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0);
}

/* The target item lit when dropping there does something, the one before
   unlit, and the cursor showing the effect. */
static void Show(DRAG *d)
{
    if (d->lit && (d->lit != d->w || d->litType != d->type))
        Highlight(d->litType, d->lit, -1);
    d->lit = d->type == T_TASK ? NULL : d->w;
    d->litType = d->type;
    Highlight(d->type, d->lit, d->effect != DE_NONE ? d->item : -1);
    SetCursor(cursors[d->effect]);
}

/* The next message. FALSE ends the drag: the button up (*dropped), Esc,
   quitting, or the mouse lost. */
static BOOL Pump(HWND src, UINT up, BOOL *dropped)
{
    MSG msg;

    if (!GetMessage(&msg, NULL, 0, 0)) {
        PostQuitMessage(msg.wParam);
        return FALSE;
    }
    if (msg.message == up) {
        *dropped = TRUE;
        return FALSE;
    }
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        return FALSE;
    if ((msg.message < WM_MOUSEFIRST || msg.message > WM_MOUSELAST) &&
        msg.message != WM_KEYDOWN && msg.message != WM_KEYUP)
        DispatchMessage(&msg);              /* paint, timers; keys only re-evaluate */
    if (GetCapture() != src)                /* another program took the mouse */
        return FALSE;
    return TRUE;
}

/* The button came up there: a taskbar button that takes nothing says so;
   other programs get WM_DROPFILES, Snowball's targets the file commands (the
   right button asking first). */
static void Drop(DRAG *d, BOOL right)
{
    if (d->type == T_TASK && d->effect == DE_NONE)
        TaskbarDropOnTask(d->src, d->w, NULL);
    if (d->effect != DE_NONE) {
        if (d->type == T_OTHER || (d->type == T_TASK && d->item < 0)) {
            DropForeign(d->w, d->p, &d->files);
        } else {
            if (right && d->effect != DE_RUN && d->kind != IVK_BIN)
                d->effect = AskEffect(d->src);
            FileCommandsDrop(TopLevel(d->w), d->target, d->kind, &d->files, d->effect);
        }
    }
}

void DragDropRun(HWND src, BOOL right)
{
    UINT up = right ? WM_RBUTTONUP : WM_LBUTTONUP;
    BOOL dropped = FALSE;
    int keys, held = -1;
    POINT at;
    DRAG d;

    d.src = src;
    d.w = d.lit = NULL;
    d.item = d.kind = -1;
    d.type = d.litType = T_NONE;
    d.effect = DE_NONE;
    Gather(src, &d.files);
    if (!d.files.count)
        return;
    LoadCursors();
    SetCapture(src);
    do {                                /* again when the pointer or Ctrl and Shift changed */
        GetCursorPos(&at);
        keys = (GetKeyState(VK_CONTROL) < 0) | (GetKeyState(VK_SHIFT) < 0) << 1;
        if (keys == held && at.x == d.p.x && at.y == d.p.y)
            continue;
        held = keys;
        Aim(&d);
        TaskbarDragOver(&d.p);
        Effect(&d);
        Show(&d);
    } while (Pump(src, up, &dropped));
    ReleaseCapture();
    TaskbarDragOver(NULL);
    if (d.lit)
        Highlight(d.litType, d.lit, -1);
    if (dropped)
        Drop(&d, right);
    PathListFree(&d.files);
}
