/* The Explorer bar of folder windows: the folder tree on the left, its
   title with a close button, the splitter that sizes it. */
#include "internal.h"
#include "folderwindowint.h"

/* The tree's close button, at the right of its title. */
static void TreeClose(HWND w, RECT *r)
{
    FOLDER *f = Folder(w);
    int top = FolderBandsHeight(w);

    SetRect(r, f->split - CLOSE_W - 2, top + (BAR_H - 12) / 2, f->split - 2, top + (BAR_H - 12) / 2 + 12);
}

/* A small flat X, as Windows 98's Explorer bar has. */
static void PaintTreeClose(HDC dc, const RECT *r)
{
    int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2, i;
    RECT t;

    for (i = 0; i < 6; i++) {
        SetRect(&t, x - 3 + i, y - 3 + i, x - 1 + i, y - 2 + i);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNTEXT));
        SetRect(&t, x + 1 - i, y - 3 + i, x + 3 - i, y - 2 + i);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNTEXT));
    }
}

/* The folder tree on the left (Explorer Bar > Folders), or none. */
BOOL ExplorerBarShow(HWND w, BOOL on)
{
    FOLDER *f = Folder(w);
    char desk[MAX_PATH];

    if (on == (f->tree != NULL))
        return TRUE;
    if (on) {
        f->split = 180;
        f->tree = CreateWindow(TREEVIEW_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER,
                               0, 0, 0, 0, w, (HMENU)ID_TREE, g_hInst, NULL);
        if (!f->tree)
            return FALSE;
        SkinControl(f->tree);           /* Windows 98's sunken edge for the 1-pixel border */
        DesktopFolder(desk);
        TreeViewReset(f->tree, desk);
        if (*f->path || IsWindowVisible(w))
            TreeViewShowPath(f->tree, f->path);
    } else {
        DestroyWindow(f->tree);
        f->tree = NULL;
        SetFocus(f->view);
    }
    FolderWindowArrange(w);
    FolderWindowSetTitle(w);
    return TRUE;
}

static BOOL OnSplit(HWND w, int x, int y)
{
    FOLDER *f = Folder(w);
    RECT c;
    GetClientRect(w, &c);
    return f->tree && x >= f->split && x < f->split + SPLIT_W && y >= FolderBandsHeight(w) && y < c.bottom - STATUS_H;
}

/* The sizing cursor over the splitter and while it is dragged; FALSE leaves
   the cursor to DefWindowProc. */
BOOL ExplorerBarCursor(HWND w, HWND over, UINT hit)
{
    FOLDER *f = Folder(w);

    if (over == w && hit == HTCLIENT) {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(w, &p);
        if (OnSplit(w, p.x, p.y) || f->splitting) {
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            return TRUE;
        }
    }
    return FALSE;
}

/* A click: the tree's close button closes it; on the splitter a drag starts,
   the mouse captured. */
void ExplorerBarButtonDown(HWND w, int x, int y)
{
    FOLDER *f = Folder(w);

    if (f->tree) {
        POINT p;
        RECT r;
        p.x = x;
        p.y = y;
        TreeClose(w, &r);
        if (PtInRect(&r, p)) {
            ExplorerBarShow(w, FALSE);
            return;
        }
    }
    if (OnSplit(w, x, y)) {
        f->splitting = TRUE;
        SetCapture(w);
    }
}

/* Dragging the splitter: the tree pane follows the mouse. */
void ExplorerBarMouseMove(HWND w, int x)
{
    FOLDER *f = Folder(w);

    if (f->splitting) {
        f->split = x;
        FolderWindowArrange(w);
    }
}

/* The splitter let go. */
void ExplorerBarButtonUp(HWND w)
{
    FOLDER *f = Folder(w);

    if (f->splitting) {
        f->splitting = FALSE;
        ReleaseCapture();
    }
}

/* The tree's selection: its folder shown here. */
void ExplorerBarNotify(HWND w, UINT code)
{
    FOLDER *f = Folder(w);
    char path[MAX_PATH];

    char renamed[MAX_PATH];
    POINT p;
    int i;

    if (code == TVN_SELECT && TreeViewSelectedPath(f->tree, path) && lstrcmpi(path, f->path))
        FolderWindowNavigate(w, path);
    if (code != TVN_CONTEXT)
        return;
    GetCursorPos(&p);
    ScreenToClient(f->tree, &p);
    if ((i = TreeViewHitTest(f->tree, p.x, p.y)) >= 0 && TreeViewItemPath(f->tree, i, path) &&
        (PathOnDisk(path) || TreeViewItemKind(f->tree, i) != IVK_FOLDER)) {
        /* its kind's menu: a drive's, My Computer's, the Recycle Bin's */
        FileMenusKind(w, path, TreeViewItemKind(f->tree, i), renamed);
        if (*renamed && f->tree) {
            PathToParent(path);
            TreeViewRefresh(f->tree, path);
        }
    }
}

/* The tree's title ("All Folders" and the close button) and the splitter. */
void ExplorerBarPaint(HWND w, HDC dc, const RECT *c, int top)
{
    FOLDER *f = Folder(w);
    RECT r;

    SetRect(&r, 0, top, f->split, top + BAR_H);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
    DrawFrame(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
    InflateRect(&r, -6, 0);
    r.right -= CLOSE_W;
    DrawTextEllipsis(dc, "All Folders", &r, DT_LEFT | DT_NOPREFIX);
    TreeClose(w, &r);
    PaintTreeClose(dc, &r);
    SetRect(&r, f->split, top, f->split + SPLIT_W, c->bottom - STATUS_H);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
}
