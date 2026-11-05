/*
 * SbTree: the folder tree of Explorer windows, drawn as Windows 98 draws
 * it (dotted lines, +/- boxes, small icons). Desktop holds My Computer,
 * the Recycle Bin and the Desktop's folders; My Computer holds the drives.
 * Or the tree of one folder, with its files too if asked (the Start menu's,
 * for Taskbar Properties).
 * The tree is kept as the list of visible rows: expanding inserts the
 * children after their parent, collapsing removes them. Folders are read
 * when expanded; whether one has subfolders is asked when it is added.
 * treepaint.c paints it.
 */
#include "internal.h"
#include "treeviewint.h"

#define GROW        64
#define TIMER_SEL   1                   /* keyboard selection: open after a pause */
#define SEL_DELAY   400

static void Notify(HWND w)
{
    SendMessage(GetParent(w), WM_COMMAND, GetWindowWord(w, GWW_ID), MAKELPARAM(w, TVN_SELECT));
}

/* Collapsed rows leave their strings behind; a full pool is rebuilt with
   only the strings of the rows that remain. */
static void Compact(TREE *t)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, t->poolCap);
    LPSTR p = h ? GlobalLock(h) : NULL;
    WORD used = 1;
    int i;

    if (!p)
        return;
    p[0] = '\0';
    for (i = 0; i < t->count; i++) {
        lstrcpy(p + used, t->pool + t->row[i].text);
        t->row[i].text = used;
        used += lstrlen(p + used) + 1;
        lstrcpy(p + used, t->pool + t->row[i].path);
        t->row[i].path = used;
        used += lstrlen(p + used) + 1;
    }
    GlobalUnlock(t->hPool);
    GlobalFree(t->hPool);
    t->hPool = h;
    t->pool = p;
    t->used = used;
}

static WORD AddString(TREE *t, LPCSTR s)
{
    WORD n = lstrlen(s) + 1, at;

    if (t->used + n > t->poolCap)
        Compact(t);
    if (t->used + n > t->poolCap) {
        WORD cap = t->poolCap + 2048 > 65000U ? 65000U : t->poolCap + 2048;
        if (t->used + n > cap || !GlobalGrow(&t->hPool, (void far **)&t->pool, cap))
            return 0;
        t->poolCap = cap;
    }
    at = t->used;
    lstrcpy(t->pool + at, s);
    t->used += n;
    return at;
}

/* Recomputes the dotted lines: one pass from the bottom up. */
static void Lines(TREE *t)
{
    WORD more = 0;                      /* bit d: a row at depth d follows, before any shallower */
    int i, d;

    for (i = t->count - 1; i >= 0; i--) {
        d = t->row[i].depth;
        t->row[i].flags = (t->row[i].flags & ~TF_LAST) | ((more & (1 << d)) ? 0 : TF_LAST);
        t->row[i].lines = more & ((1 << d) - 1);
        more = (more & ((2 << d) - 1)) | (1 << d);
    }
}

/* Inserts a row at index at; FALSE when out of memory. */
static BOOL Insert(TREE *t, int at, LPCSTR text, LPCSTR path, int kind, int depth, HICON icon, BOOL kids)
{
    NODE far *n;

    if (t->count == t->cap) {
        if (!GlobalGrow(&t->hRows, (void far **)&t->row, (DWORD)(t->cap + GROW) * sizeof(NODE)))
            return FALSE;
        t->cap += GROW;
    }
    _fmemmove(t->row + at + 1, t->row + at, (t->count - at) * sizeof(NODE));
    t->count++;
    n = t->row + at;
    n->text = n->path = 0;              /* a compaction for its strings copies this row too */
    n->text = AddString(t, text);
    n->path = AddString(t, path);
    n->icon = icon;
    n->kind = (BYTE)kind;
    n->depth = (BYTE)depth;
    n->flags = kids ? TF_KIDS : 0;
    return TRUE;
}

static LPSTR sortPool;

static int CompareRows(const void far *a, const void far *b)
{
    return lstrcmpi(sortPool + ((NODE far *)a)->text, sortPool + ((NODE far *)b)->text);
}

/* The folders in dir (or its files), sorted, inserted from first on. */
static int AddPart(TREE *t, int first, int depth, LPCSTR dir, BOOL folders)
{
    char spec[MAX_PATH], path[MAX_PATH], name[MAX_PATH];
    int at = first;
    DOSENTRY e;
    DOSFIND f;

    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (DosFindFirst(spec, &f, &e)) {
        do {
            if (!(e.attr & FA_DIR) != !folders || ViewOptionsHides(e.attr) ||
                (lstrlen(dir) <= 3 && !lstrcmpi(e.name, BIN_FOLDER)) || lstrlen(dir) + lstrlen(e.name) + 2 > MAX_PATH)
                continue;
            lstrcpy(path, dir);
            PathAppend(path, e.name);
            if (folders) {
                DisplayNameOfFolder(e.name, name);
                at += Insert(t, at, name, path, IVK_FOLDER, depth, IconStock(IDI_FOLDER),
                             t->files || DosHasSubfolders(path));
            } else {
                DisplayNameOfFile(e.name, name);
                at += Insert(t, at, name, path, IVK_FILE, depth, IconForFile(path, FALSE), FALSE);
            }
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
    sortPool = t->pool;
    SortRecords(t->row + first, at - first, sizeof(NODE), CompareRows);
    return at - first;
}

/* Folders first, then the files if the tree shows them. */
static int AddFolders(TREE *t, int first, int depth, LPCSTR dir)
{
    int n = AddPart(t, first, depth, dir, TRUE);
    return t->files ? n + AddPart(t, first + n, depth, dir, FALSE) : n;
}

static void Expand(HWND w, int i)
{
    TREE *t = Tree(w);
    NODE far *n = t->row + i;
    char path[MAX_PATH], name[24], root[4];
    int added = 0, d;

    if ((n->flags & TF_EXPANDED) || !(n->flags & TF_KIDS))
        return;
    lstrcpy(path, t->pool + n->path);
    switch (n->kind) {
    case IVK_COMPUTER:
        for (d = 0; d < 26; d++)
            if (GetDriveType(d)) {
                wsprintf(root, "%c:\\", 'A' + d);
                DriveName(d, name);
                /* floppies are not touched until opened */
                if (Insert(t, i + 1 + added, name, root, IVK_DRIVE, n->depth + 1, IconForDrive(d),
                           GetDriveType(d) == DRIVE_REMOVABLE || DosHasSubfolders(root)))
                    added++;
                n = t->row + i;
            }
        break;
    case IVK_DRIVE:
    case IVK_FOLDER:
        if (n->depth == 0 && !t->plain) {      /* Desktop: as Windows 98 orders it */
            added = Insert(t, i + 1, "My Computer", "", IVK_COMPUTER, 1, IconStock(IDI_COMPUTER), TRUE);
            added += Insert(t, i + 1 + added, "Recycle Bin", BIN_PATH, IVK_BIN, 1,
                            IconStock(RecycleBinIsEmpty() ? IDI_BIN : IDI_BINFULL), FALSE);
            added += AddFolders(t, i + 1 + added, 1, path);
        } else {
            added = AddFolders(t, i + 1, n->depth + 1, path);
        }
        break;
    }
    n = t->row + i;
    n->flags |= TF_EXPANDED;
    if (!added)
        n->flags &= ~TF_KIDS;           /* nothing inside after all: no box */
    Lines(t);
}

static void Collapse(HWND w, int i)
{
    TREE *t = Tree(w);
    int end = i + 1;

    while (end < t->count && t->row[end].depth > t->row[i].depth)
        end++;
    _fmemmove(t->row + i + 1, t->row + end, (t->count - end) * sizeof(NODE));
    t->count -= end - i - 1;
    t->row[i].flags &= ~TF_EXPANDED;
    if (t->sel > i && t->sel < end)
        t->sel = i;
    else if (t->sel >= end)
        t->sel -= end - i - 1;
    Lines(t);
}

static void Layout(HWND w)
{
    TREE *t = Tree(w);
    RECT r;
    int page;

    GetClientRect(w, &r);
    page = max(1, r.bottom / ROW_H);
    t->top = max(0, min(t->top, t->count - page));
    SetScrollRange(w, SB_VERT, 0, max(0, t->count - page), FALSE);
    SetScrollPos(w, SB_VERT, t->top, TRUE);
    InvalidateRect(w, NULL, TRUE);
}

static void Toggle(HWND w, int i)
{
    TREE *t = Tree(w);
    if (t->row[i].flags & TF_EXPANDED)
        Collapse(w, i);
    else
        Expand(w, i);
    Layout(w);
}

static void EnsureVisible(HWND w, int i)
{
    TREE *t = Tree(w);
    RECT r;
    int page;

    GetClientRect(w, &r);
    page = max(1, r.bottom / ROW_H);
    if (i < t->top)
        t->top = i;
    else if (i >= t->top + page)
        t->top = i - page + 1;
    Layout(w);
}

static void Select(HWND w, int i, BOOL notify)
{
    TREE *t = Tree(w);
    if (i < 0 || i >= t->count)
        return;
    t->sel = i;
    EnsureVisible(w, i);
    InvalidateRect(w, NULL, TRUE);
    if (notify)
        Notify(w);
}

/* ---- public ---- */

int TreeViewHitTest(HWND tv, int x, int y)
{
    TREE *t = Tree(tv);
    int i = t->top + y / ROW_H;
    return (y >= 0 && i < t->count && x >= 4 + t->row[i].depth * INDENT) ? i : -1;
}

BOOL TreeViewItemPath(HWND tv, int i, LPSTR out)
{
    TREE *t = Tree(tv);
    if (i < 0 || i >= t->count)
        return FALSE;
    lstrcpy(out, t->pool + t->row[i].path);
    return TRUE;
}

int TreeViewItemKind(HWND tv, int i)
{
    TREE *t = Tree(tv);
    return (i >= 0 && i < t->count) ? t->row[i].kind : -1;
}

void TreeViewDropHighlight(HWND tv, int i)
{
    TREE *t = Tree(tv);
    if (i != t->drop) {
        t->drop = i;
        InvalidateRect(tv, NULL, TRUE);
        UpdateWindow(tv);
    }
}

BOOL TreeViewSelectedPath(HWND tv, LPSTR out)
{
    return TreeViewItemPath(tv, Tree(tv)->sel, out);
}

/* Desktop (the folder desk), expanded, with My Computer expanded too. */
void TreeViewReset(HWND tv, LPCSTR desk)
{
    TREE *t = Tree(tv);

    t->count = t->top = 0;
    t->used = 1;
    t->sel = t->drop = -1;
    t->plain = t->files = FALSE;
    Insert(t, 0, "Desktop", desk, IVK_FOLDER, 0, IconStock(IDI_SNOWBALL), TRUE);
    Expand(tv, 0);
    Expand(tv, 1);
    Layout(tv);
}

void TreeViewResetFolder(HWND tv, LPCSTR dir, LPCSTR name, BOOL files)
{
    TREE *t = Tree(tv);

    t->count = t->top = 0;
    t->used = 1;
    t->sel = t->drop = -1;
    t->plain = TRUE;
    t->files = files;
    Insert(t, 0, name, dir, IVK_FOLDER, 0, IconStock(IDI_FOLDER), TRUE);
    Expand(tv, 0);
    Layout(tv);
}

/* Opens the branches down to path and selects it; the parent is not told. */
void TreeViewShowPath(HWND tv, LPCSTR path)
{
    TREE *t = Tree(tv);
    char want[MAX_PATH];
    int i, best = -1, n;

    for (i = 0; i < t->count; i++) {
        lstrcpy(want, t->pool + t->row[i].path);
        n = lstrlen(want);
        if (!lstrcmpi(want, path)) {
            best = i;
            break;
        }
        /* a drive or folder on the way: open it and look below */
        if (t->row[i].kind >= IVK_FOLDER && t->row[i].kind <= IVK_DRIVE && t->row[i].depth &&
            n && PathIsInside(path, want) && !(t->row[i].flags & TF_EXPANDED))
            Expand(tv, i);
    }
    if (best >= 0)
        Select(tv, best, FALSE);
    Layout(tv);
}

/* dir changed on disk: its open branch is read again. */
void TreeViewRefresh(HWND tv, LPCSTR dir)
{
    TREE *t = Tree(tv);
    char sel[MAX_PATH];
    int i;

    TreeViewSelectedPath(tv, sel);
    for (i = 0; i < t->count; i++)
        if ((t->row[i].depth || t->plain) && (t->row[i].kind == IVK_FOLDER || t->row[i].kind == IVK_DRIVE) &&
            !lstrcmpi(t->pool + t->row[i].path, dir)) {
            if (t->row[i].flags & TF_EXPANDED) {
                Collapse(tv, i);
                t->row[i].flags |= TF_KIDS;
                Expand(tv, i);
            } else if (t->files || DosHasSubfolders(dir)) {
                t->row[i].flags |= TF_KIDS;
            } else {
                t->row[i].flags &= ~TF_KIDS;
            }
            break;
        }
    TreeViewShowPath(tv, sel);
}

/* ---- input ---- */

static void Click(HWND w, int x, int y, BOOL twice)
{
    TREE *t = Tree(w);
    int i = t->top + y / ROW_H, box;

    if (i >= t->count)
        return;
    box = 4 + t->row[i].depth * INDENT - INDENT / 2;
    if (t->row[i].depth && x >= box - 6 && x <= box + 6) {
        Toggle(w, i);                           /* the +/- box */
    } else if (x >= box + 6) {
        Select(w, i, TRUE);
        if (twice)
            Toggle(w, i);
    }
}

static void Key(HWND w, WPARAM key)
{
    TREE *t = Tree(w);
    int i = t->sel, parent;

    if (i < 0)
        i = 0;
    switch (key) {
    case VK_UP:     i--; break;
    case VK_DOWN:   i++; break;
    case VK_HOME:   i = 0; break;
    case VK_END:    i = t->count - 1; break;
    case VK_ADD:
    case VK_RIGHT:
        if (!(t->row[i].flags & TF_EXPANDED) && (t->row[i].flags & TF_KIDS)) {
            Toggle(w, i);
            return;
        }
        if (key == VK_RIGHT && i + 1 < t->count && t->row[i + 1].depth > t->row[i].depth)
            i++;
        break;
    case VK_SUBTRACT:
    case VK_LEFT:
        if (t->row[i].flags & TF_EXPANDED) {
            Toggle(w, i);
            return;
        }
        for (parent = i - 1; key == VK_LEFT && parent >= 0; parent--)
            if (t->row[parent].depth < t->row[i].depth) {
                i = parent;
                break;
            }
        break;
    case VK_RETURN:
        Toggle(w, i);
        return;
    default:
        SendMessage(GetParent(w), WM_KEYDOWN, key, 0L);
        return;
    }
    if (i >= 0 && i < t->count && i != t->sel) {
        Select(w, i, FALSE);
        SetTimer(w, TIMER_SEL, SEL_DELAY, NULL);    /* the folder follows once keys pause */
    }
}

static LRESULT Create(HWND w)
{
    TREE *t = LocalHeapAlloc(sizeof(TREE));

    if (!t)
        return -1;
    t->hRows = GlobalAlloc(GMEM_MOVEABLE, GROW * sizeof(NODE));
    t->hPool = GlobalAlloc(GMEM_MOVEABLE, 2048);
    if (!t->hRows || !t->hPool)
        return -1;
    t->row = (NODE far *)GlobalLock(t->hRows);
    t->pool = GlobalLock(t->hPool);
    t->cap = GROW;
    t->poolCap = 2048;
    t->sel = t->drop = -1;
    SetWindowWord(w, 0, LocalHeapOffset(t));
    return 0;
}

static void Destroy(TREE *t)
{
    if (!t)                             /* WM_CREATE failed */
        return;
    GlobalUnlock(t->hRows);
    GlobalUnlock(t->hPool);
    GlobalFree(t->hRows);
    GlobalFree(t->hPool);
    LocalHeapFree(t);
}

/* The row lit while its menu is up, as in Windows 98. */
static void RightClick(HWND w, int y)
{
    TREE *t = Tree(w);
    int i = t->top + y / ROW_H;

    if (i >= t->count)
        return;
    TreeViewDropHighlight(w, i);
    UpdateWindow(w);
    SendMessage(GetParent(w), WM_COMMAND, GetWindowWord(w, GWW_ID), MAKELPARAM(w, TVN_CONTEXT));
    if (IsWindow(w))
        TreeViewDropHighlight(w, -1);
}

static void Scroll(HWND w, WPARAM code, int pos)
{
    TREE *t = Tree(w);
    RECT r;
    int old = t->top;

    GetClientRect(w, &r);
    switch (code) {
    case SB_LINEUP:        t->top--; break;
    case SB_LINEDOWN:      t->top++; break;
    case SB_PAGEUP:        t->top -= r.bottom / ROW_H; break;
    case SB_PAGEDOWN:      t->top += r.bottom / ROW_H; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: t->top = pos; break;
    }
    Layout(w);
    if (t->top == old)
        ValidateRect(w, NULL);
}

/* The wheel (ui.h): 3 rows a notch, a page for PageUp/PageDown. */
static void Wheel(HWND w, WPARAM key)
{
    int n;

    for (n = key == VK_UP || key == VK_DOWN ? 3 : 1; n; n--)
        Scroll(w, key == VK_UP ? SB_LINEUP : key == VK_DOWN ? SB_LINEDOWN : key == VK_PRIOR ? SB_PAGEUP : SB_PAGEDOWN, 0);
}

LRESULT CALLBACK __export TreeViewProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    HOOK_DESCRIBE(msg, TreeViewDescribe(w, (LPSTR)lp, wp));
    switch (msg) {
    case WM_CREATE:
        return Create(w);
    case WM_DESTROY:
        Destroy(Tree(w));
        return 0;
    case WM_SIZE:
        Layout(w);
        return 0;
    case WM_ERASEBKGND:                 /* Paint fills what it repaints */
        return 1;
    case WM_PAINT:
        TreeViewPaint(w);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(w, NULL, TRUE);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetFocus(w);
        Click(w, (int)LOWORD(lp), (int)HIWORD(lp), msg == WM_LBUTTONDBLCLK);
        return 0;
    case WM_RBUTTONDOWN:
        SetFocus(w);
        return 0;
    case WM_RBUTTONUP:
        RightClick(w, (int)HIWORD(lp));
        return 0;
    case WM_MOUSEWHEEL:
        Wheel(w, (short)HIWORD(wp) > 0 ? VK_UP : VK_DOWN);
        return 0;
    case WM_KEYDOWN:
        if (IS_WHEEL_KEY(wp, lp))
            Wheel(w, wp);
        else
            Key(w, wp);
        return 0;
    case WM_TIMER:
        KillTimer(w, TIMER_SEL);
        Notify(w);
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_VSCROLL:
        Scroll(w, wp, LOWORD(lp));
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL TreeViewRegister(void)
{

    return WindowRegister(TREEVIEW_CLASS, TreeViewProc, CS_DBLCLKS, sizeof(WORD), NULL, (HBRUSH)(COLOR_WINDOW + 1));
}
