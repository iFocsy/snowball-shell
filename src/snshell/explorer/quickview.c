/*
 * Quick View, as Windows 95 and 98 have it: a file looked at without its
 * program - text as text (its first 60 KB, tabs set out), a BMP picture
 * (up to 4 MB) as the picture - in a window of its own whose File menu has
 * Open File for Editing; the status bar says so.
 */
#include "internal.h"

#define QUICKVIEW_CLASS "SbQuickView"
#define QV_MAX      60000U             /* text read */
#define QV_LINES    6000
#define QVC_EDIT    1
#define QVC_EXIT    2
#define QVC_ABOUT   3

typedef struct {
    HGLOBAL  self;
    char     path[MAX_PATH];
    DIB      dib;                       /* a BMP's picture */
    HGLOBAL  hData;                     /* else the text */
    DWORD    size;
    HGLOBAL  hLines;                    /* where each line starts */
    int      lines, top, lineH;
    BOOL     picture;
} QVIEW;

static BOOL registered;

#define Qv(w)   ((QVIEW FAR *)GetWindowLong(w, 0))

BOOL QuickViewCan(LPCSTR path)
{
    static const char *Exts[] = { ".TXT", ".INI", ".LOG", ".BAT", ".SYS", ".CFG", ".DIZ", ".ME", ".1ST",
                                  ".C", ".H", ".ASM", ".PAS", ".BAS", ".HTM", ".CSV", ".BMP" };
    char ext[8];
    int i;

    lstrcpyn(ext, PathExtension((LPSTR)path), sizeof ext);
    AnsiUpper(ext);
    for (i = 0; i < sizeof Exts / sizeof Exts[0]; i++)
        if (!lstrcmp(ext, Exts[i]))
            return TRUE;
    return FALSE;
}

/* What a view holds, and the view itself, freed. */
static void Free(QVIEW FAR *q)
{
    HGLOBAL self = q->self;

    DibFree(&q->dib);
    if (q->hData)
        GlobalFree(q->hData);
    if (q->hLines)
        GlobalFree(q->hLines);
    GlobalUnlock(self);
    GlobalFree(self);
}

/* The file read: a BMP's picture (checked: a damaged one is not shown),
   else the text and where its lines start. */
static BOOL Load(QVIEW FAR *q)
{
    BYTE FAR *d;
    WORD FAR *at;
    HFILE h;
    LONG len;
    DWORD i;
    UINT n;

    if (!lstrcmpi(PathExtension(q->path), ".BMP"))
        return q->picture = DibLoad(q->path, &q->dib);
    if ((h = DosOpenRead(q->path)) == HFILE_ERROR)
        return FALSE;
    len = _llseek(h, 0, 2);
    _llseek(h, 0, 0);
    if (len < 0 || (q->hData = GlobalAlloc(GMEM_MOVEABLE, min(len, (LONG)QV_MAX) + 1)) == NULL) {
        _lclose(h);
        return FALSE;
    }
    d = (BYTE FAR *)GlobalLock(q->hData);
    n = _lread(h, d, (UINT)min(len, (LONG)QV_MAX));
    q->size = n == (UINT)HFILE_ERROR ? 0 : n;
    _lclose(h);
    if ((q->hLines = GlobalAlloc(GMEM_MOVEABLE, QV_LINES * sizeof(WORD))) != NULL) {
        at = (WORD FAR *)GlobalLock(q->hLines);
        at[q->lines++] = 0;
        for (i = 0; i < q->size && q->lines < QV_LINES; i++)
            if (d[i] == '\n')
                at[q->lines++] = (WORD)(i + 1);
        GlobalUnlock(q->hLines);
    }
    GlobalUnlock(q->hData);
    return TRUE;
}

static void Paint(HWND w)
{
    QVIEW FAR *q = Qv(w);
    PAINTSTRUCT ps;
    BYTE FAR *d;
    WORD FAR *at;
    RECT c;
    HFONT old;
    int i, y, len;

    BeginPaint(w, &ps);
    GetClientRect(w, &c);
    c.bottom -= STATUS_H;
    DrawSolidRect(ps.hdc, &c, GetSysColor(COLOR_WINDOW));
    if (q->picture) {                   /* at its own size, from the top left */
        DibDraw(ps.hdc, &q->dib, 4, 4, q->dib.w, q->dib.h);
    } else if (q->hLines) {
        d = (BYTE FAR *)GlobalLock(q->hData);
        at = (WORD FAR *)GlobalLock(q->hLines);
        old = SelectObject(ps.hdc, GetStockObject(ANSI_FIXED_FONT));
        SetTextColor(ps.hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkMode(ps.hdc, TRANSPARENT);
        for (i = q->top, y = 2; i < q->lines && y < c.bottom; i++, y += q->lineH) {
            len = (int)((i + 1 < q->lines ? at[i + 1] : (WORD)q->size) - at[i]);
            while (len > 0 && (d[at[i] + len - 1] == '\n' || d[at[i] + len - 1] == '\r'))
                len--;
            TabbedTextOut(ps.hdc, 4, y, (LPCSTR)(d + at[i]), len, 0, NULL, 4);
        }
        SelectObject(ps.hdc, old);
        GlobalUnlock(q->hLines);
        GlobalUnlock(q->hData);
    }
    GetClientRect(w, &c);
    {
        static const int whole[] = { 0 };
        LPCSTR text = "To edit, click Open File for Editing on the File menu.";
        PaintStatusBar(ps.hdc, &c, 1, whole, &text, NULL, !IsZoomed(w));
    }
    EndPaint(w, &ps);
}

static void Scroll(HWND w, int to)
{
    QVIEW FAR *q = Qv(w);
    RECT c;
    int page;

    GetClientRect(w, &c);
    page = max(1, (c.bottom - STATUS_H) / q->lineH);
    to = max(0, min(to, q->lines - page));
    if (q->picture || to == q->top)
        return;
    q->top = to;
    SetScrollPos(w, SB_VERT, to, TRUE);
    c.bottom -= STATUS_H;
    InvalidateRect(w, &c, FALSE);
}

LRESULT CALLBACK __export QuickViewProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    QVIEW FAR *q = Qv(w);
    RECT c;
    int page;

    switch (msg) {
    case WM_PAINT:
        Paint(w);
        return 0;
    case WM_SIZE:
        InvalidateRect(w, NULL, FALSE);
        if (q && !q->picture) {
            page = max(1, ((int)HIWORD(lp) - STATUS_H) / q->lineH);
            SetScrollRange(w, SB_VERT, 0, max(0, q->lines - page), TRUE);
            Scroll(w, q->top);
        }
        return 0;
    case WM_VSCROLL:
        GetClientRect(w, &c);
        page = max(1, (c.bottom - STATUS_H) / q->lineH);
        switch (wp) {
        case SB_LINEUP:        Scroll(w, q->top - 1); break;
        case SB_LINEDOWN:      Scroll(w, q->top + 1); break;
        case SB_PAGEUP:        Scroll(w, q->top - page); break;
        case SB_PAGEDOWN:      Scroll(w, q->top + page); break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:    Scroll(w, LOWORD(lp)); break;
        }
        return 0;
    case WM_KEYDOWN:
        GetClientRect(w, &c);
        page = max(1, (c.bottom - STATUS_H) / q->lineH);
        switch (wp) {
        case VK_UP:     Scroll(w, q->top - 1); break;
        case VK_DOWN:   Scroll(w, q->top + 1); break;
        case VK_PRIOR:  Scroll(w, q->top - page); break;
        case VK_NEXT:   Scroll(w, q->top + page); break;
        case VK_HOME:   Scroll(w, 0); break;
        case VK_END:    Scroll(w, q->lines); break;
        case VK_ESCAPE: DestroyWindow(w); break;
        }
        return 0;
    case WM_NCHITTEST:                  /* the size grip */
        if (StatusBarGrip(w, lp))
            return HTBOTTOMRIGHT;
        break;
    case WM_COMMAND:
        if (wp == QVC_EDIT) {
            LaunchItem(w, q->path);
            DestroyWindow(w);
        } else if (wp == QVC_EXIT) {
            DestroyWindow(w);
        } else if (wp == QVC_ABOUT) {
            AboutDialogShow(w);
        }
        return 0;
    case WM_DESTROY:
        if (q) {
            SetWindowLong(w, 0, 0L);
            Free(q);
        }
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

void QuickViewShow(LPCSTR path)
{
    HGLOBAL h;
    QVIEW FAR *q;
    HMENU menu, file;
    char title[MAX_PATH + 16], name[MAX_PATH];
    TEXTMETRIC tm;
    RECT c;
    HDC dc;
    HWND w;

    if (!registered) {
        registered = WindowRegister(QUICKVIEW_CLASS, QuickViewProc, 0, sizeof(LONG), IconStock(IDI_FILE), NULL);
    }
    if ((h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(QVIEW))) == NULL)
        return;
    q = (QVIEW FAR *)GlobalLock(h);
    q->self = h;
    lstrcpy(q->path, path);
    if (!Load(q)) {
        FileOperationError(NULL, "open", path, DosLastError());
        Free(q);
        return;
    }
    dc = GetDC(NULL);
    SelectObject(dc, GetStockObject(ANSI_FIXED_FONT));
    GetTextMetrics(dc, &tm);
    ReleaseDC(NULL, dc);
    q->lineH = tm.tmHeight;
    menu = CreateMenu();
    file = CreatePopupMenu();
    AppendMenu(file, MF_STRING, QVC_EDIT, "&Open File for Editing");
    AppendMenu(file, MF_SEPARATOR, 0, NULL);
    AppendMenu(file, MF_STRING, QVC_EXIT, "E&xit");
    AppendMenu(menu, MF_POPUP, (UINT)file, "&File");
    file = CreatePopupMenu();
    AppendMenu(file, MF_STRING, QVC_ABOUT, "&About Snowball");
    AppendMenu(menu, MF_POPUP, (UINT)file, "&Help");
    DisplayNameOfFile(PathFileName((LPSTR)path), name);
    wsprintf(title, "%s - Quick View", (LPSTR)name);
    w = CreateWindow(QUICKVIEW_CLASS, title, WS_OVERLAPPEDWINDOW | (q->picture ? 0 : WS_VSCROLL),
                     60, 50, 520, 400, NULL, menu, g_hInst, NULL);
    if (!w) {
        DestroyMenu(menu);
        Free(q);
        return;
    }
    SetWindowLong(w, 0, (LONG)q);
    GetClientRect(w, &c);               /* the scroll range, now that it has the text */
    SendMessage(w, WM_SIZE, SIZE_RESTORED, MAKELPARAM(c.right, c.bottom));
    ShowWindow(w, SW_SHOWNORMAL);
}
