/* The desktop: one bottom-most window over the wallpaper with My Computer,
   My Documents, the Recycle Bin and the contents of the Desktop folder. */
#include "internal.h"

static HWND desk, view;

void DesktopFolder(LPSTR out)
{
    lstrcpy(out, g_home);
    lstrcat(out, "Desktop");
}

/* My Documents: Windows 98's C:\My Documents (SNOWBALL.INI [Desktop]
   MyDocuments may say otherwise), made if it is not there. */
LPCSTR DesktopMyDocuments(void)
{
    static char path[MAX_PATH];

    if (!path[0]) {
        SettingsGetString("Desktop", "MyDocuments", "C:\\My Documents", path, sizeof path);
        if (!PathIsFolder(path))
            DosMakeFolder(path);
    }
    return path;
}

/* The desktop's My Documents is the folder itself: it is neither deleted,
   moved nor renamed from there. */
BOOL DesktopIsMyDocuments(HWND iv, int item)
{
    char path[MAX_PATH];

    return iv == view && IconViewItemPath(iv, item, path) && !lstrcmpi(path, DesktopMyDocuments());
}

static void Refresh(BOOL keep)
{
    char dir[MAX_PATH];

    DesktopFolder(dir);
    IconViewClear(view, keep);
    IconViewAdd(view, "My Computer", "", IVK_COMPUTER, NULL);
    IconViewAdd(view, "My Documents", DesktopMyDocuments(), IVK_FOLDER, NULL);
    IconViewPin(view, IconViewCount(view) - 1);
    IconViewAdd(view, "Recycle Bin", BIN_PATH, IVK_BIN, NULL);
    IconViewLoadFolder(view, dir, NULL);
    IconViewFinish(view);
}

void DesktopRepaint(void)
{
    if (desk)
        RedrawWindow(desk, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void DesktopSetArea(const RECT *r)
{
    if (view)
        MoveWindow(view, r->left, r->top, r->right - r->left, r->bottom - r->top, TRUE);
}

/* Every selected item, as Windows 98 opens them. */
static void Open(void)
{
    char path[MAX_PATH];
    int i = -1;

    while ((i = IconViewNextSelected(view, i)) >= 0)
        switch (IconViewItemKind(view, i)) {
        case IVK_COMPUTER: FolderWindowOpen(""); break;
        case IVK_BIN:      FolderWindowOpen(BIN_PATH); break;
        default:           if (IconViewItemPath(view, i, path)) LaunchItem(desk, path);
        }
}

LRESULT CALLBACK __export DesktopProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    char dir[MAX_PATH];
    LRESULT res;

    if (SendToMessage(msg, wp, lp, &res))
        return res;
    switch (msg) {
    case WM_WINDOWPOSCHANGING:          /* always stay behind every other window */
        ((WINDOWPOS FAR *)lp)->hwndInsertAfter = HWND_BOTTOM;
        return 0;
    case WM_ERASEBKGND:
        DrawWallpaper((HDC)wp);
        return 1;
    case WM_SETFOCUS:
        SetFocus(view);
        return 0;
    case WM_COMMAND:
        if (wp == ID_VIEW) {
            switch (HIWORD(lp)) {
            case IVN_OPEN:    Open(); break;
            case IVN_CONTEXT: FileCommandsContextMenu(w, view); break;
            case IVN_DRAG:
            case IVN_RDRAG:   DragDropRun(view, HIWORD(lp) == IVN_RDRAG); break;
            case IVN_RENAME:  return FileCommandsRenamed(w, view);
            case IVN_SORTED:  FileCommandsSaveView(view); break;
            }
        } else if (wp == CMD_OPEN) {
            Open();
        } else if (wp == CMD_PROPERTIES && !IconViewSelectedCount(view)) {
            DisplayPropertiesShow(0);
        } else {
            FileCommandsRun(w, view, wp);
        }
        return 0;
    case SBM_CHANGED:                   /* NULL: Folder Options changed */
        DesktopFolder(dir);
        if (!lp || !lstrcmpi((LPCSTR)lp, dir) || !lstrcmp((LPCSTR)lp, BIN_PATH))
            Refresh(TRUE);
        return 0;
    case WM_DROPFILES:
        FileCommandsDropFiles(view, (HDROP)wp);
        return 0;
    case SBM_OPEN:                      /* from a second SNOWBALL.EXE */
        GlobalGetAtomName((ATOM)LOWORD(lp), dir, sizeof dir);
        GlobalDeleteAtom((ATOM)LOWORD(lp));
        if (!_fstrnicmp(dir, "/E,", 3))       /* "SNOWBALL /E,C:\" explores */
            FolderWindowExplore(dir + 3);
        else if (!dir[0] || !lstrcmp(dir, BIN_PATH) || PathIsFolder(dir))
            FolderWindowOpen(dir);
        else
            LaunchCommand(w, dir, NULL, SW_SHOWNORMAL);
        return 0;
    case WM_KEYDOWN:
        FileCommandsKey(w, view, wp);
        return 0;
    case WM_CLOSE:                      /* Alt+F4 on the desktop: shut down */
        ShutDownDialogShow(w);
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL DesktopCreate(void)
{
    WNDCLASS wc;
    char dir[MAX_PATH];
    int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);

    _fmemset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DesktopProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = IconStock(IDI_SNOWBALL);
    wc.lpszClassName = DESKTOP_CLASS;
    if (!RegisterClass(&wc))
        return FALSE;
    desk = CreateWindow(DESKTOP_CLASS, "Desktop", WS_POPUP | WS_CLIPCHILDREN,
                        0, 0, cx, cy, NULL, NULL, g_hInst, NULL);
    view = CreateWindow(ICONVIEW_CLASS, NULL, WS_CHILD | WS_VISIBLE | IVS_DESKTOP,
                        0, 0, cx, cy, desk, (HMENU)ID_VIEW, g_hInst, NULL);
    if (!desk || !view)
        return FALSE;
    DesktopFolder(dir);
    if (!PathIsFolder(dir))
        DosMakeFolder(dir);
    DragAcceptFiles(view, TRUE);
    FileCommandsLoadView(view);
    Refresh(FALSE);
    ShowWindow(desk, SW_SHOWNOACTIVATE);
    return TRUE;
}
