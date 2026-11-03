/*
 * Find: Files or Folders, as Windows 98 has it. Named takes wildcards, or
 * any part of a name ("note" finds NOTEPAD.EXE), several split by ";";
 * Containing text looks inside files (case does not matter); Look in and
 * Include subfolders say where. The results fill a Details view with an In
 * Folder column and work like any folder's items (open, drag, delete...).
 * The search walks the disk in the shell's task and lets Windows run every
 * few dozen milliseconds, so everything stays responsive; Stop ends it.
 */
#include "internal.h"

#define ID_RESULTS  450
#define TIMER_ANIM  1
#define PUMP_EVERY  40
#define SORT_EVERY  600                 /* ms between showing new results */
#define CHUNK       1024

static HWND finder, results;
static BOOL searching, stop, closing;
static int found, frame, resultsTop;
static DWORD lastPump, lastSort;
static char named[128], text[64], status[MAX_PATH + 16], start[MAX_PATH];
static RECT animRect;

/* ---- matching ---- */

/* name against one pattern with * and ?, case ignored. */
static BOOL Match(LPCSTR name, LPCSTR pat)
{
    for (; *pat; pat++, name++) {
        if (*pat == '*') {
            while (*pat == '*')
                pat++;
            if (!*pat)
                return TRUE;
            for (; *name; name++)
                if (Match(name, pat))
                    return TRUE;
            return FALSE;
        }
        if (!*name || (*pat != '?' &&
            (BYTE)(DWORD)AnsiLower((LPSTR)(DWORD)(BYTE)*pat) != (BYTE)(DWORD)AnsiLower((LPSTR)(DWORD)(BYTE)*name)))
            return FALSE;
    }
    return !*name;
}

/* Any of the patterns in Named; one without wildcards matches inside names. */
static BOOL NameMatches(LPCSTR name)
{
    char pat[130];
    LPSTR p, end;

    if (!*named)
        return TRUE;
    for (p = named; *p; p = *end ? end + 1 : end) {
        for (end = p; *end && *end != ';'; end++)
            ;
        while (*p == ' ')
            p++;
        lstrcpyn(pat, p, min((int)(end - p) + 1, (int)sizeof pat - 2));
        if (!pat[0])
            continue;
        if (!lstrcmp(pat, "*.*"))
            return TRUE;
        if (!_fstrchr(pat, '*') && !_fstrchr(pat, '?')) {
            _fmemmove(pat + 1, pat, lstrlen(pat) + 1);
            pat[0] = '*';
            lstrcat(pat, "*");
        }
        if (Match(name, pat))
            return TRUE;
    }
    return FALSE;
}

/* Whether the file holds text, case ignored; read in chunks that overlap
   by the text's length so nothing is missed at a boundary. */
static BOOL Contains(LPCSTR path)
{
    static char buf[CHUNK + 64];
    int n = lstrlen(text), have = 0, got, i, k;
    HFILE h = DosOpenRead(path);

    if (h == HFILE_ERROR)
        return FALSE;
    while ((got = _lread(h, buf + have, CHUNK)) > 0 && got != HFILE_ERROR) {
        have += got;
        AnsiLowerBuff(buf, have);
        for (i = 0; i + n <= have; i++) {
            for (k = 0; k < n && buf[i + k] == text[k]; k++)
                ;
            if (k == n) {
                _lclose(h);
                return TRUE;
            }
        }
        if (have >= n) {                        /* keep the tail */
            _fmemmove(buf, buf + have - n + 1, n - 1);
            have = n - 1;
        }
    }
    _lclose(h);
    return FALSE;
}

/* ---- the search ---- */

static void ShowStatus(void)
{
    RECT r;
    GetClientRect(finder, &r);
    r.top = r.bottom - STATUS_H;
    InvalidateRect(finder, &r, FALSE);
}

/* Lets Windows run now and then, and shows what was found so far. */
static BOOL KeepGoing(LPCSTR dir)
{
    DWORD now = GetTickCount();

    if (now - lastPump < PUMP_EVERY)
        return !stop;
    lastPump = now;
    wsprintf(status, "Searching %s", dir);
    ShowStatus();
    if (now - lastSort > SORT_EVERY) {
        lastSort = now;
        IconViewFinish(results);
    }
    if (!MessageLoopPump(finder))
        stop = TRUE;
    return !stop;
}

static void Walk(LPSTR dir, BOOL subfolders)
{
    static DOSENTRY e;
    char name[MAX_PATH];
    DOSFIND f;
    int n = lstrlen(dir);

    if (n + 5 > MAX_PATH || !KeepGoing(dir))
        return;
    PathAppend(dir, "*.*");
    if (DosFindFirst(dir, &f, &e)) {
        do {
            BOOL folder = (e.attr & FA_DIR) != 0;
            dir[n] = '\0';
            if (n + lstrlen(e.name) + 2 > MAX_PATH || (n <= 3 && !lstrcmpi(e.name, BIN_FOLDER)))
                continue;
            PathAppend(dir, e.name);
            if (NameMatches(e.name) && (!*text || (!folder && Contains(dir)))) {
                DisplayNameOfFile(e.name, name);
                IconViewAdd(results, name, dir, folder ? IVK_FOLDER : IVK_FILE, &e);
                found++;
            }
            if (folder && subfolders)
                Walk(dir, TRUE);                /* e is reused below: nothing of it is needed after */
            if (!KeepGoing(dir))
                break;
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
    dir[n] = '\0';
}

static void Search(void)
{
    char dir[MAX_PATH];

    GetDlgItemText(finder, IDC_FIND_NAMED, named, sizeof named);
    GetDlgItemText(finder, IDC_FIND_TEXT, text, sizeof text);
    GetDlgItemText(finder, IDC_FIND_LOOKIN, dir, sizeof dir);
    AnsiLower(text);
    if (!PathIsFolder(dir)) {
        wsprintf(status, "Cannot find the folder '%s'.", (LPSTR)dir);
        MessageBox(finder, status, "Find", MB_ICONSTOP | MB_OK);
        status[0] = '\0';
        return;
    }
    searching = TRUE;
    stop = FALSE;
    found = 0;
    lastSort = GetTickCount();
    IconViewClear(results, FALSE);
    EnableWindow(GetDlgItem(finder, IDC_FIND_FIND), FALSE);
    EnableWindow(GetDlgItem(finder, IDC_FIND_STOP), TRUE);
    SetTimer(finder, TIMER_ANIM, 100, NULL);
    Walk(dir, IsDlgButtonChecked(finder, IDC_FIND_SUBFOLDERS));
    KillTimer(finder, TIMER_ANIM);
    frame = 0;
    InvalidateRect(finder, &animRect, TRUE);
    IconViewFinish(results);
    searching = FALSE;
    if (closing) {
        DestroyWindow(finder);
        return;
    }
    wsprintf(status, "%d file(s) found", found);
    ShowStatus();
    EnableWindow(GetDlgItem(finder, IDC_FIND_FIND), TRUE);
    EnableWindow(GetDlgItem(finder, IDC_FIND_STOP), FALSE);
}

/* ---- the window ---- */

static void Arrange(HWND dlg)
{
    RECT c;
    GetClientRect(dlg, &c);
    MoveWindow(results, 0, resultsTop, c.right, max(0, c.bottom - resultsTop - STATUS_H), TRUE);
    InvalidateRect(dlg, NULL, TRUE);
}

/* The magnifier circles while a search runs. */
static void PaintAnimation(HDC dc)
{
    static const signed char dx[] = { 0, 3, 4, 3, 0, -3, -4, -3 }, dy[] = { -4, -3, 0, 3, 4, 3, 0, -3 };
    int f = searching ? frame % 8 : 0;
    DrawIconImage(dc, (animRect.left + animRect.right) / 2 - 16 + (searching ? dx[f] : 0),
             (animRect.top + animRect.bottom) / 2 - 16 + (searching ? dy[f] : 0), IconStock(IDI_SM_FIND), FALSE);
}

static void OpenSelected(void)
{
    char path[MAX_PATH];
    int i = -1;
    while ((i = IconViewNextSelected(results, i)) >= 0)
        if (IconViewItemPath(results, i, path))
            LaunchItem(finder, path);
}

BOOL CALLBACK __export FindProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    static const int whole = 0;         /* the status bar's one panel */
    PAINTSTRUCT ps;
    RECT r;
    HFONT old;
    LPCSTR panel;
    char root[4];
    int d;

    switch (msg) {
    case WM_INITDIALOG:
        finder = dlg;
        SkinDialog(dlg);
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Name & Location");
        SetRect(&r, 0, 104, 0, 0);
        MapDialogRect(dlg, &r);
        resultsTop = r.top;
        SetRect(&animRect, 236, 66, 294, 98);
        MapDialogRect(dlg, &animRect);
        results = CreateWindow(ICONVIEW_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_BORDER | IVS_FOUND,
                               0, 0, 0, 0, dlg, (HMENU)ID_RESULTS, g_hInst, NULL);
        IconViewSetFolder(results, "");
        for (d = 2; d < 26; d++)
            if (GetDriveType(d)) {
                wsprintf(root, "%c:\\", 'A' + d);
                SendDlgItemMessage(dlg, IDC_FIND_LOOKIN, CB_ADDSTRING, 0, (LPARAM)(LPSTR)root);
            }
        SetDlgItemText(dlg, IDC_FIND_LOOKIN, start);
        CheckDlgButton(dlg, IDC_FIND_SUBFOLDERS, TRUE);
        EnableWindow(GetDlgItem(dlg, IDC_FIND_STOP), FALSE);
        Arrange(dlg);
        SetFocus(GetDlgItem(dlg, IDC_FIND_NAMED));
        return FALSE;                   /* the focus is set */
    case WM_SIZE:
        if (results)
            Arrange(dlg);
        return TRUE;
    case WM_NCHITTEST:
        if (!StatusBarGrip(dlg, lp))
            return FALSE;
        SetWindowLong(dlg, DWL_MSGRESULT, HTBOTTOMRIGHT);
        return TRUE;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO FAR *)lp)->ptMinTrackSize.x = 420;
        ((MINMAXINFO FAR *)lp)->ptMinTrackSize.y = 260;
        return TRUE;
    case WM_PAINT:
        BeginPaint(dlg, &ps);
        GetClientRect(dlg, &r);
        old = SelectObject(ps.hdc, DrawFont(FALSE));
        SetBkMode(ps.hdc, TRANSPARENT);
        panel = status;
        PaintStatusBar(ps.hdc, &r, 1, &whole, &panel, NULL, !IsZoomed(dlg));
        SelectObject(ps.hdc, old);
        PaintAnimation(ps.hdc);
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_TIMER:
        frame++;
        InvalidateRect(dlg, &animRect, TRUE);
        return TRUE;
    case WM_KEYDOWN:                    /* from the results, which take every key */
        if (wp == VK_TAB)
            SetFocus(GetNextDlgTabItem(dlg, results, GetKeyState(VK_SHIFT) < 0));
        else
            FileCommandsKey(dlg, results, wp);
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDC_FIND_FIND:
            if (!searching)
                Search();
            return TRUE;
        case IDC_FIND_STOP:
            stop = TRUE;
            return TRUE;
        case IDC_FIND_NEW:
            stop = TRUE;
            SetDlgItemText(dlg, IDC_FIND_NAMED, "");
            SetDlgItemText(dlg, IDC_FIND_TEXT, "");
            if (!searching) {
                IconViewClear(results, FALSE);
                IconViewFinish(results);
                status[0] = '\0';
                ShowStatus();
            }
            SetFocus(GetDlgItem(dlg, IDC_FIND_NAMED));
            return TRUE;
        case IDCANCEL:
            PostMessage(dlg, WM_CLOSE, 0, 0L);
            return TRUE;
        case ID_RESULTS:
            switch (HIWORD(lp)) {
            case IVN_OPEN:    OpenSelected(); break;
            case IVN_CONTEXT: FileCommandsContextMenu(dlg, results); break;
            case IVN_DRAG:
            case IVN_RDRAG:   DragDropRun(results, HIWORD(lp) == IVN_RDRAG); break;
            case IVN_RENAME:  SetWindowLong(dlg, DWL_MSGRESULT, FileCommandsRenamed(dlg, results)); break;
            }
            return TRUE;
        case CMD_OPEN:
            OpenSelected();
            return TRUE;
        default:
            if ((wp >= CMD_OPEN && wp <= CMD_FIND) || wp == CMD_OPENWITH || wp == CMD_QUICKVIEW ||
                (wp > CMD_ACTION && wp < CMD_ACTION + FT_ACTIONS))     /* menus of the results */
                FileCommandsRun(dlg, results, wp);
        }
        break;
    case WM_CLOSE:
        if (searching) {                /* the search ends first, then the window */
            stop = closing = TRUE;
            return TRUE;
        }
        DestroyWindow(dlg);
        return TRUE;
    case WM_ACTIVATE:                   /* modeless: the main loop hands it its keys */
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        break;
    case WM_DESTROY:
        MessageLoopTrackDialog(dlg, FALSE);
        finder = results = NULL;
        closing = FALSE;
        return TRUE;
    }
    return FALSE;
}

/* Opens Find (or brings it back) to look in a folder. */
void FindShow(LPCSTR lookIn)
{
    if (finder) {
        if (IsIconic(finder))
            ShowWindow(finder, SW_RESTORE);
        if (*lookIn && !searching)
            SetDlgItemText(finder, IDC_FIND_LOOKIN, lookIn);
        BringWindowToTop(finder);
        return;
    }
    lstrcpy(start, *lookIn ? lookIn : "C:\\");
    status[0] = '\0';
    if (CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_FIND), NULL, FindProc)) {
        WindowCenter(finder);
        ShowWindow(finder, SW_SHOWNORMAL);
    }
}
