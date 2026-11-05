/*
 * SbIconView: the view of the desktop and of folder windows, in the four
 * Windows 98 modes: Large Icons (a grid; on the desktop, columns over the
 * wallpaper), Small Icons, List (columns, scrolling sideways) and Details
 * (rows under a header of sortable, resizable columns).
 * Items and their names live in two global blocks per view, locked while
 * the view exists; icons are looked up the first time an item is painted.
 * Selection works as in Windows 98: click, Ctrl/Shift+click, rubber band,
 * keyboard; a second, slow click on a selected label renames it in place.
 * This part: the items and their names, sorting, selection and the window
 * procedure. iconpaint.c lays the items out and paints them; iconinput.c
 * has the mouse, the keyboard, the infotips and the in-place rename;
 * iconviewint.h is what the three share.
 */
#include "internal.h"
#include "iconviewint.h"

void IconViewNotify(HWND w, int code)
{
    SendMessage(GetParent(w), WM_COMMAND, GetWindowWord(w, GWW_ID), MAKELPARAM(w, code));
}

BOOL IconViewIsDesktop(HWND w)
{
    return (GetWindowLong(w, GWL_STYLE) & IVS_DESKTOP) != 0;
}

static WORD AddString(VIEW *v, LPCSTR s)
{
    WORD n = lstrlen(s) + 1, at;

    if (v->used + n > v->poolCap) {
        WORD cap = v->poolCap + 2048 > 65000U ? 65000U : v->poolCap + 2048;
        if (v->used + n > cap || !GlobalGrow(&v->hPool, (void far **)&v->pool, cap))
            return 0;
        v->poolCap = cap;
    }
    at = v->used;
    lstrcpy(v->pool + at, s);
    v->used += n;
    return at;
}

/* ---- items ---- */

/* keep: a refresh of the same folder - the scroll position and the
   selection (by name) survive until IconViewFinish. */
void IconViewClear(HWND iv, BOOL keep)
{
    VIEW *v = View(iv);
    int i;

    IconViewTipOff(iv);
    PathListFree(&v->keep);
    if (keep)
        for (i = 0; i < v->count; i++)
            if (Sel(v, i))
                PathListAdd(&v->keep, v->pool + v->it[i].file);
    if (!keep)
        v->scroll = v->hscroll = 0;
    IconViewDropEdit(iv);
    for (i = 0; i < v->count; i++)
        if (v->it[i].thumb)
            DeleteObject(v->it[i].thumb);
    v->count = 0;
    v->used = 1;                /* offset 0 = empty string */
    v->focus = v->anchor = v->drop = v->pressed = -1;
}

void IconViewSetFolder(HWND iv, LPCSTR dir) { lstrcpy(View(iv)->dir, dir); }
LPCSTR IconViewFolder(HWND iv)              { return View(iv)->dir; }

void IconViewAdd(HWND iv, LPCSTR name, LPCSTR file, int kind, const DOSENTRY *info)
{
    VIEW *v = View(iv);
    ITEM far *it;

    if (v->count == v->cap) {
        if (v->cap + GROW > MAX_ITEMS ||
            !GlobalGrow(&v->hItems, (void far **)&v->it, (DWORD)(v->cap + GROW) * sizeof(ITEM)))
            return;
        v->cap += GROW;
    }
    it = v->it + v->count++;
    it->name = AddString(v, name);
    it->file = AddString(v, file);
    it->icon = NULL;
    it->thumb = NULL;
    it->free = 0;
    it->textW = 0;                      /* measured by IconViewMeasure */
    it->kind = (BYTE)kind;
    it->flags = PathIsShortcut(file) ? IF_SHORTCUT : 0;
    it->size = info ? info->size : 0;
    it->date = info ? info->date : 0;
    it->time = info ? info->time : 0;
}

void IconViewLoadFolder(HWND iv, LPCSTR dir, LPCSTR only)
{
    char spec[MAX_PATH], name[MAX_PATH];
    DOSFIND f;
    DOSENTRY e;

    IconViewSetFolder(iv, dir);
    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (DosFindFirst(spec, &f, &e)) {
        do {
            /* the bin's folder too: DOSBox does not keep attributes of folders */
            if (ViewOptionsHides(e.attr) || (lstrlen(dir) <= 3 && !lstrcmpi(e.name, BIN_FOLDER)) ||
                (only && !(e.attr & FA_DIR) && !PathMatchesPattern(e.name, only)))
                continue;
            if (e.attr & FA_DIR)
                DisplayNameOfFolder(e.name, name);
            else
                DisplayNameOfFile(e.name, name);
            IconViewAdd(iv, name, e.name, (e.attr & FA_DIR) ? IVK_FOLDER : IVK_FILE, &e);
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
}

/* The drives, each with its capacity and free space (fixed disks: the
   others are not asked, a floppy would want a disk). */
void IconViewLoadDrives(HWND iv)
{
    VIEW *v = View(iv);
    char name[32], root[4];
    DWORD total, room;
    int d;

    IconViewSetFolder(iv, "");
    for (d = 0; d < 26; d++) {
        if (!GetDriveType(d))
            continue;
        wsprintf(root, "%c:\\", 'A' + d);
        DriveName(d, name);
        IconViewAdd(iv, name, root, IVK_DRIVE, NULL);
        if (GetDriveType(d) == DRIVE_FIXED && DosDiskSpace(d, &total, &room) && v->count) {
            v->it[v->count - 1].size = total;
            v->it[v->count - 1].free = room;
        }
    }
}

int IconViewCount(HWND iv)    { return View(iv)->count; }
int IconViewFocused(HWND iv)  { return View(iv)->focus; }
int IconViewMode(HWND iv)     { return View(iv)->mode; }

int IconViewItemKind(HWND iv, int i)
{
    VIEW *v = View(iv);
    return (i >= 0 && i < v->count) ? v->it[i].kind : -1;
}

DWORD IconViewItemSize(HWND iv, int i)
{
    return View(iv)->it[i].size;
}

DWORD IconViewItemFree(HWND iv, int i)
{
    return View(iv)->it[i].free;
}

void IconViewPin(HWND iv, int i)
{
    View(iv)->it[i].flags |= IF_PINNED;
}

BOOL IconViewItemPath(HWND iv, int i, LPSTR out)
{
    VIEW *v = View(iv);
    LPSTR file;

    if (i < 0 || i >= v->count)
        return FALSE;
    file = v->pool + v->it[i].file;
    if (file[0] && file[1] == ':') {            /* absolute, or ::Bin */
        lstrcpy(out, file);
    } else {
        lstrcpy(out, v->dir);
        PathAppend(out, file);
    }
    return TRUE;
}

int IconViewFind(HWND iv, LPCSTR fileName)
{
    VIEW *v = View(iv);
    int i;
    for (i = 0; i < v->count; i++)
        if (!lstrcmpi(v->pool + v->it[i].file, fileName))
            return i;
    return -1;
}

int IconViewNextSelected(HWND iv, int after)
{
    VIEW *v = View(iv);
    int i;
    for (i = after + 1; i < v->count; i++)
        if (Sel(v, i))
            return i;
    return -1;
}

int IconViewSelectedCount(HWND iv)
{
    int n = 0, i = -1;
    while ((i = IconViewNextSelected(iv, i)) >= 0)
        n++;
    return n;
}

/* ---- sorting ---- */

static VIEW *sorting;

/* My Computer, what is pinned after it, Recycle Bin, drives, folders,
   then files - each group by the sort key (the name breaks ties), drives
   always by letter. */
static int Rank(const ITEM far *it)
{
    static const BYTE rank[] = { 8, 6, 4, 0, 2 };   /* by IVK_* */
    return (it->flags & IF_PINNED) ? 1 : rank[it->kind];
}

static int CompareItems(const void far *pa, const void far *pb)
{
    const ITEM far *a = pa, far *b = pb;
    LPSTR pool = sorting->pool;
    int d = 0;

    if (Rank(a) != Rank(b))
        return Rank(a) - Rank(b);
    if (a->kind == IVK_DRIVE)
        return lstrcmpi(pool + a->file, pool + b->file);
    switch (sorting->sortKey) {
    case IVSORT_SIZE:
        d = a->size < b->size ? -1 : a->size > b->size;
        break;
    case IVSORT_TYPE:
        d = lstrcmpi(PathExtension(pool + a->file), PathExtension(pool + b->file));
        break;
    case IVSORT_DATE:
        d = a->date != b->date ? (a->date < b->date ? -1 : 1) : (a->time < b->time ? -1 : a->time > b->time);
        break;
    case IVSORT_FOLDER:                    /* absolute paths: compare up to the names */
        d = _fstrnicmp(pool + a->file, pool + b->file,
                       min(PathFileName(pool + a->file) - (pool + a->file), PathFileName(pool + b->file) - (pool + b->file)));
        break;
    }
    if (!d)
        d = lstrcmpi(pool + a->name, pool + b->name);
    return sorting->sortDesc ? -d : d;
}

/* Sorts, keeping the focus on the same item. */
static void Sort(VIEW *v)
{
    int i;

    if (v->focus >= 0)
        v->it[v->focus].flags |= IF_FOCUS;
    sorting = v;
    SortRecords(v->it, v->count, sizeof(ITEM), CompareItems);
    v->focus = v->anchor = -1;
    for (i = 0; i < v->count; i++)
        if (v->it[i].flags & IF_FOCUS) {
            v->it[i].flags &= ~IF_FOCUS;
            v->focus = v->anchor = i;
        }
}

void IconViewFinish(HWND iv)
{
    VIEW *v = View(iv);
    LPSTR p;
    int i;

    IconViewMeasure(iv);
    Sort(v);
    for (p = PathListLock(&v->keep); *p; p = PathListNext(p))
        if ((i = IconViewFind(iv, p)) >= 0) {
            v->it[i].flags |= IF_SELECTED;
            if (v->focus < 0)
                v->focus = v->anchor = i;
        }
    PathListUnlock(&v->keep);
    PathListFree(&v->keep);
    IconViewLayout(iv);
    InvalidateRect(iv, NULL, TRUE);
}

void IconViewSetMode(HWND iv, int mode)
{
    VIEW *v = View(iv);
    if (v->mode == mode)
        return;
    IconViewDropEdit(iv);
    v->mode = mode;
    v->scroll = v->hscroll = 0;
    IconViewLayout(iv);
    InvalidateRect(iv, NULL, TRUE);
}

void IconViewSort(HWND iv, int key, BOOL descending)
{
    VIEW *v = View(iv);
    v->sortKey = key;
    v->sortDesc = descending;
    IconViewDropEdit(iv);
    Sort(v);
    InvalidateRect(iv, NULL, TRUE);
}

int  IconViewSortKey(HWND iv)  { return View(iv)->sortKey; }
BOOL IconViewSortDescending(HWND iv) { return View(iv)->sortDesc; }

static void InvalidateItem(HWND w, int i)
{
    RECT r;
    if (i >= 0) {
        IconViewCellRect(w, i, &r);
        InvalidateRect(w, &r, TRUE);
    }
}

/* ---- selection ---- */

void IconViewSetSel(HWND w, int i, BOOL on)
{
    VIEW *v = View(w);
    if (!Sel(v, i) != !on) {
        v->it[i].flags ^= IF_SELECTED;
        InvalidateItem(w, i);
    }
}

/* Scrolls so that item i is in view. */
static void ScrollTo(HWND w, int i)
{
    VIEW *v = View(w);
    RECT c;
    long x, y;
    int cx, cy, top;

    if (i < 0 || IconViewIsDesktop(w))
        return;
    IconViewCell(w, i, &x, &y, &cx, &cy);
    GetClientRect(w, &c);
    top = v->mode == IVM_DETAILS ? HEAD_H : 0;
    if (v->mode == IVM_LIST) {
        if (x >= v->hscroll && x + cx - v->hscroll <= c.right)
            return;
        v->hscroll = x < v->hscroll ? x - 2 : x + cx - c.right;
    } else {
        if (y - v->scroll >= top && y + cy - v->scroll <= c.bottom)
            return;
        v->scroll = y - v->scroll < top ? y - top - 2 : y + cy - c.bottom;
    }
    IconViewLayout(w);
    InvalidateRect(w, NULL, TRUE);
}

void IconViewSetFocusItem(HWND w, int i)
{
    VIEW *v = View(w);
    InvalidateItem(w, v->focus);
    v->focus = i;
    InvalidateItem(w, i);
    ScrollTo(w, i);
}

/* Selects items from..to (inclusive, any order); only: others off. */
void IconViewSelectRange(HWND w, int from, int to, BOOL only)
{
    VIEW *v = View(w);
    int i, lo = min(from, to), hi = max(from, to);
    for (i = 0; i < v->count; i++)
        if (i >= lo && i <= hi)
            IconViewSetSel(w, i, TRUE);
        else if (only)
            IconViewSetSel(w, i, FALSE);
    IconViewNotify(w, IVN_SELECT);
}

void IconViewSelect(HWND iv, int i)
{
    VIEW *v = View(iv);
    if (i < 0) {
        IconViewSelectRange(iv, -1, -1, TRUE);
    } else {
        IconViewSelectRange(iv, i, i, TRUE);
        v->anchor = i;
    }
    IconViewSetFocusItem(iv, i);
}

void IconViewSelectAll(HWND iv, BOOL invert)
{
    VIEW *v = View(iv);
    int i;
    for (i = 0; i < v->count; i++)
        IconViewSetSel(iv, i, invert ? !Sel(v, i) : TRUE);
    IconViewNotify(iv, IVN_SELECT);
}

/* Every item whose name matches a pattern ("*.txt") is selected too. */
int IconViewSelectMatching(HWND iv, LPCSTR pattern)
{
    VIEW *v = View(iv);
    int i, n = 0;

    for (i = 0; i < v->count; i++)
        if (v->it[i].kind <= IVK_FOLDER && PathMatchesPattern(PathFileName(v->pool + v->it[i].file), pattern)) {
            IconViewSetSel(iv, i, TRUE);
            n++;
        }
    IconViewNotify(iv, IVN_SELECT);
    return n;
}

void IconViewDropHighlight(HWND iv, int i)
{
    VIEW *v = View(iv);
    if (i != v->drop) {
        InvalidateItem(iv, v->drop);
        v->drop = i;
        InvalidateItem(iv, i);
        UpdateWindow(iv);
    }
}

/* ---- the window ---- */

/* A new view: its blocks and its Details columns; -1 if there is no room. */
static LRESULT Create(HWND w)
{
    VIEW *v = LocalHeapAlloc(sizeof(VIEW));

    if (!v)
        return -1;
    v->hItems = GlobalAlloc(GMEM_MOVEABLE, GROW * sizeof(ITEM));
    v->hPool = GlobalAlloc(GMEM_MOVEABLE, 2048);
    if (!v->hItems || !v->hPool)
        return -1;
    v->it = (ITEM far *)GlobalLock(v->hItems);
    v->pool = GlobalLock(v->hPool);
    v->cap = GROW;
    v->poolCap = 2048;
    v->cellW = SMALL_W;
    v->hover = v->hot = -1;
    if (GetWindowLong(w, GWL_STYLE) & IVS_FOUND) {      /* Find's results */
        static const BYTE keys[] = { IVSORT_NAME, IVSORT_FOLDER, IVSORT_SIZE, IVSORT_TYPE, IVSORT_DATE };
        static const int widths[] = { 130, 150, 60, 100, 136 };
        v->ncols = 5;
        _fmemcpy(v->colKey, keys, sizeof keys);
        _fmemcpy(v->colW, widths, sizeof widths);
        v->mode = IVM_DETAILS;
    } else {
        static const BYTE keys[] = { IVSORT_NAME, IVSORT_SIZE, IVSORT_TYPE, IVSORT_DATE };
        static const int widths[] = { 150, 60, 110, 136 };
        v->ncols = 4;
        _fmemcpy(v->colKey, keys, sizeof keys);
        _fmemcpy(v->colW, widths, sizeof widths);
    }
    v->headDown = v->sizing = -1;
    SetWindowWord(w, 0, LocalHeapOffset(v));
    IconViewClear(w, FALSE);
    return 0;
}

/* The view goes: its tip, its items and their blocks. */
static void Destroy(HWND w)
{
    VIEW *v = View(w);

    if (!v)                         /* WM_CREATE failed */
        return;
    IconViewTipOff(w);
    IconViewClear(w, FALSE);        /* the thumbnails' pictures go */
    GlobalUnlock(v->hItems);
    GlobalUnlock(v->hPool);
    GlobalFree(v->hItems);
    GlobalFree(v->hPool);
    PathListFree(&v->keep);
    LocalHeapFree(v);
}

/* Repaints the selected items and the focused one. */
static void FocusChange(HWND w)
{
    VIEW *v = View(w);
    int i;

    for (i = 0; i < v->count; i++)
        if (Sel(v, i) || i == v->focus)
            InvalidateItem(w, i);
}

/* The wheel (ui.h): 3 lines a notch, a page for PageUp/PageDown. */
static void Wheel(HWND w, WPARAM key)
{
    int code = key == VK_PRIOR ? SB_PAGEUP : key == VK_NEXT ? SB_PAGEDOWN : key == VK_UP ? SB_LINEUP : SB_LINEDOWN, n;

    for (n = key == VK_UP || key == VK_DOWN ? 3 : 1; n; n--)
        IconViewScroll(w, View(w)->mode == IVM_LIST, code, 0);
}

/* The mouse's messages. */
static void Mouse(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    int x = (int)LOWORD(lp), y = (int)HIWORD(lp);

    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        IconViewButtonDown(w, msg, wp, x, y);
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        IconViewButtonUp(w, msg, wp, x, y);
        break;
    case WM_MOUSEMOVE:
        IconViewMouseMove(w, wp, x, y);
        break;
    case WM_LBUTTONDBLCLK:
        KillTimer(w, TIMER_EDIT);
        if (IconViewHitTest(w, x, y) >= 0 && !g_viewOptions.singleClick)  /* (the first click opened it) */
            IconViewNotify(w, IVN_OPEN);
        break;
    case WM_MOUSEWHEEL:
        Wheel(w, (short)HIWORD(wp) > 0 ? VK_UP : VK_DOWN);
        break;
    }
}

/* The keyboard's: the wheel's keys too (ui.h); Alt keys FALSE: as usual. */
static BOOL Keys(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_SYSKEYDOWN)
        return IconViewSysKey(w, wp, lp);
    if (msg == WM_CHAR)
        IconViewTypeSelect(w, wp);
    else if (IS_WHEEL_KEY(wp, lp))
        Wheel(w, wp);
    else
        IconViewKey(w, wp);
    return TRUE;
}

LRESULT CALLBACK __export IconViewProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    HOOK_DESCRIBE(msg, IconViewDescribe(w, (LPSTR)lp, wp));
    switch (msg) {
    case WM_CREATE:
        return Create(w);
    case WM_DESTROY:
        Destroy(w);
        return 0;
    case WM_SIZE:
        IconViewLayout(w);
        InvalidateRect(w, NULL, TRUE);
        return 0;
    case WM_ERASEBKGND:                 /* Paint fills what it repaints, the desktop's wallpaper too */
        return 1;
    case WM_PAINT:
        IconViewPaint(w);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:                  /* selection colours depend on the focus */
        FocusChange(w);
        return 0;
    case MSG_ENDEDIT:
        IconViewEndEdit(w, wp);
        return 0;
    case WM_SETCURSOR:                  /* column edges of the Details header */
        if (IconViewCursor(w, LOWORD(lp)))
            return TRUE;
        break;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_LBUTTONUP: case WM_RBUTTONUP:
    case WM_MOUSEMOVE: case WM_LBUTTONDBLCLK: case WM_MOUSEWHEEL:
        Mouse(w, msg, wp, lp);
        return 0;
    case WM_CANCELMODE:
        IconViewCancel(w);
        break;
    case WM_TIMER:
        IconViewTimer(w, wp);
        return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_CHAR:
        if (Keys(w, msg, wp, lp))
            return 0;
        break;
    case WM_GETDLGCODE:                 /* in a dialog (Find) too: Enter opens, Tab goes to the parent */
        return DLGC_WANTALLKEYS;
    case WM_VSCROLL:
    case WM_HSCROLL:
        IconViewScroll(w, msg == WM_HSCROLL, wp, LOWORD(lp));
        return 0;
    case WM_DROPFILES:                  /* from File Manager and the like */
        return SendMessage(GetParent(w), msg, wp, lp);
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL IconViewRegister(void)
{

    return WindowRegister(ICONVIEW_CLASS, IconViewProc, CS_DBLCLKS, sizeof(WORD), NULL, (HBRUSH)(COLOR_WINDOW + 1));
}
