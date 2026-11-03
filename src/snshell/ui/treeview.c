/*
 * SbTree: the folder tree of Explorer windows, drawn as Windows 98 draws
 * it (dotted lines, +/- boxes, small icons). Desktop holds My Computer,
 * the Recycle Bin and the Desktop's folders; My Computer holds the drives.
 * Or the tree of one folder, with its files too if asked (the Start menu's,
 * for Taskbar Properties).
 * The tree is kept as the list of visible rows: expanding inserts the
 * children after their parent, collapsing removes them. Folders are read
 * when expanded; whether one has subfolders is asked when it is added.
 */
#include "internal.h"

#define ROW_H       18
#define INDENT      19
#define GROW        64
#define TIMER_SEL   1                   /* keyboard selection: open after a pause */
#define SEL_DELAY   400

#define TF_EXPANDED 0x01
#define TF_KIDS     0x02                /* has (or may have) children */
#define TF_LAST     0x04                /* last of its siblings */

typedef struct {
    WORD  text, path;                   /* offsets in the string pool */
    HICON icon;
    BYTE  depth, flags, kind;           /* kind: IVK_* */
    WORD  lines;                        /* bit d: the ancestor at depth d has more siblings */
} NODE;

typedef struct {
    HGLOBAL  hRows, hPool;
    NODE far *row;
    LPSTR    pool;
    int      count, cap, top, sel, drop;
    WORD     used, poolCap;
    BOOL     plain, files;              /* one folder's tree; with its files */
} TREE;

#define Tree(w)     ((TREE *)LocalHeapPointer(GetWindowWord(w, 0)))

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
    static DOSENTRY e;
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

/* ---- painting ---- */

/* Dotted lines; dots sit on even x + y, so rows join up seamlessly. */
static void VDots(HDC dc, int x, int y0, int y1)
{
    COLORREF c = GetSysColor(COLOR_BTNSHADOW);
    for (y0 += (x + y0) & 1; y0 <= y1; y0 += 2)
        SetPixel(dc, x, y0, c);
}

static void HDots(HDC dc, int x0, int x1, int y)
{
    COLORREF c = GetSysColor(COLOR_BTNSHADOW);
    for (x0 += (x0 + y) & 1; x0 <= x1; x0 += 2)
        SetPixel(dc, x0, y, c);
}

/* Off the screen, then onto it at once (no white flash under the rows). */
static void Paint(HWND w)
{
    TREE *t = Tree(w);
    PAINTSTRUCT ps;
    OFFSCREEN o;
    HDC dc;
    RECT r, box, text;
    HFONT old;
    BOOL focused = GetFocus() == w, lit;
    int i, d, x, y, cx, cy;
    LPSTR name;

    BeginPaint(w, &ps);
    dc = DrawOffscreenBegin(&o, ps.hdc, &ps.rcPaint);
    DrawSolidRect(dc, &ps.rcPaint, GetSysColor(COLOR_WINDOW));
    old = SelectObject(dc, DrawFont(FALSE));
    SetBkMode(dc, TRANSPARENT);
    GetClientRect(w, &r);
    for (i = t->top; i < t->count && (y = (i - t->top) * ROW_H) < r.bottom; i++) {
        NODE far *n = t->row + i;
        x = 4 + n->depth * INDENT;
        cy = y + ROW_H / 2;
        for (d = 1; d < n->depth; d++)          /* lines of the ancestors that go on */
            if (n->lines & (1 << d))
                VDots(dc, 4 + d * INDENT - INDENT / 2, y, y + ROW_H - 1);
        if (n->depth) {                         /* the line into this row */
            cx = x - INDENT / 2;
            VDots(dc, cx, y, (n->flags & TF_LAST) ? cy : y + ROW_H - 1);
            HDots(dc, cx, x - 2, cy);
            if (n->flags & TF_KIDS) {           /* the +/- box */
                SetRect(&box, cx - 4, cy - 4, cx + 5, cy + 5);
                DrawSolidRect(dc, &box, GetSysColor(COLOR_WINDOW));
                DrawFrame(dc, &box, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNSHADOW));
                SetRect(&box, cx - 2, cy, cx + 3, cy + 1);
                DrawSolidRect(dc, &box, GetSysColor(COLOR_WINDOWTEXT));
                if (!(n->flags & TF_EXPANDED)) {
                    SetRect(&box, cx, cy - 2, cx + 1, cy + 3);
                    DrawSolidRect(dc, &box, GetSysColor(COLOR_WINDOWTEXT));
                }
            }
        }
        DrawIconImage(dc, x, y + 1, n->icon, TRUE);
        name = t->pool + n->text;
        SetRect(&text, x + 19, y + 1, x + 23 + LOWORD(GetTextExtent(dc, name, lstrlen(name))), y + ROW_H - 1);
        lit = (i == t->sel && focused) || i == t->drop;
        if (lit || i == t->sel)
            DrawSolidRect(dc, &text, GetSysColor(lit ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
        SetTextColor(dc, GetSysColor(lit ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        InflateRect(&text, -2, 0);
        DrawText(dc, name, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        /* (no focus rectangle: over the selection its XOR is a yellow line) */
    }
    SelectObject(dc, old);
    DrawOffscreenEnd(&o);
    EndPaint(w, &ps);
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

LRESULT CALLBACK __export TreeViewProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    TREE *t = Tree(w);
    RECT r;
    int old;

    switch (msg) {
    case WM_CREATE:
        t = LocalHeapAlloc(sizeof(TREE));
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
    case WM_DESTROY:
        GlobalUnlock(t->hRows);
        GlobalUnlock(t->hPool);
        GlobalFree(t->hRows);
        GlobalFree(t->hPool);
        LocalHeapFree(t);
        return 0;
    case WM_SIZE:
        Layout(w);
        return 0;
    case WM_ERASEBKGND:                 /* Paint fills what it repaints */
        return 1;
    case WM_PAINT:
        Paint(w);
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
    case WM_RBUTTONUP:                  /* the row lit while its menu is up, as in Windows 98 */
        old = t->top + (int)HIWORD(lp) / ROW_H;
        if (old < t->count) {
            TreeViewDropHighlight(w, old);
            UpdateWindow(w);
            SendMessage(GetParent(w), WM_COMMAND, GetWindowWord(w, GWW_ID), MAKELPARAM(w, TVN_CONTEXT));
            if (IsWindow(w))
                TreeViewDropHighlight(w, -1);
        }
        return 0;
    case WM_MOUSEWHEEL:
        wp = (short)HIWORD(wp) > 0 ? VK_UP : VK_DOWN;
        lp = 0;
        /* fall through */
    case WM_KEYDOWN:
        if (IS_WHEEL_KEY(wp, lp)) {     /* the wheel (ui.h): 3 rows a notch */
            for (old = wp == VK_UP || wp == VK_DOWN ? 3 : 1; old; old--)
                SendMessage(w, WM_VSCROLL, wp == VK_UP ? SB_LINEUP : wp == VK_DOWN ? SB_LINEDOWN :
                            wp == VK_PRIOR ? SB_PAGEUP : SB_PAGEDOWN, 0L);
        } else {
            Key(w, wp);
        }
        return 0;
    case WM_TIMER:
        KillTimer(w, TIMER_SEL);
        Notify(w);
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_VSCROLL:
        GetClientRect(w, &r);
        old = t->top;
        switch (wp) {
        case SB_LINEUP:        t->top--; break;
        case SB_LINEDOWN:      t->top++; break;
        case SB_PAGEUP:        t->top -= r.bottom / ROW_H; break;
        case SB_PAGEDOWN:      t->top += r.bottom / ROW_H; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: t->top = LOWORD(lp); break;
        }
        Layout(w);
        if (t->top == old)
            ValidateRect(w, NULL);
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL TreeViewRegister(void)
{
    WNDCLASS wc;

    _fmemset(&wc, 0, sizeof wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = TreeViewProc;
    wc.cbWndExtra = sizeof(WORD);
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = TREEVIEW_CLASS;
    return RegisterClass(&wc);
}
