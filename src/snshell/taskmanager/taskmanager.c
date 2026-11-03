/*
 * The Task Manager window: a dialog that can be resized, with a menu, the
 * three pages under tabs, and a status bar. One timer updates it at the
 * speed chosen: the page shown, the status bar, and always the Performance
 * history. Its place, page and options are kept in SNOWBALL.INI.
 */
#include "internal.h"

#define MARGIN      6
#define INSET       8                   /* the page inside the tab panel */
#define MIN_W       320                 /* the smallest window, all pages still readable */
#define MIN_H       380
#define PAGES       3
#define WINDOWS_AT  3                   /* the Windows menu's place, on Applications */
#define SECTION     "TaskManager"

static const UINT speeds[] = { 500, 2000, 4000, 0 };    /* High, Normal, Low, Paused */
static HWND manager;
static HMENU windowsMenu;
static int page, speed;
static BOOL onTop, minUse;
static RECT pageRect;

BOOL TaskManagerMinimizeOnUse(void)
{
    return minUse;
}

static HMENU MakeMenu(void)
{
    HMENU bar = CreateMenu(), m = CreatePopupMenu(), s = CreatePopupMenu();

    AppendMenu(m, MF_STRING, TMC_RUN, "&New Task (Run...)");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, TMC_EXIT, "E&xit Task Manager");
    AppendMenu(bar, MF_POPUP, (UINT)m, "&File");
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, TMC_ONTOP, "&Always On Top");
    AppendMenu(m, MF_STRING, TMC_MINUSE, "&Minimize On Use");
    AppendMenu(bar, MF_POPUP, (UINT)m, "&Options");
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, TMC_REFRESH, "&Refresh Now");
    AppendMenu(s, MF_STRING, TMC_SPEED, "&High");
    AppendMenu(s, MF_STRING, TMC_SPEED + 1, "&Normal");
    AppendMenu(s, MF_STRING, TMC_SPEED + 2, "&Low");
    AppendMenu(s, MF_STRING, TMC_SPEED + 3, "&Paused");
    AppendMenu(m, MF_POPUP, (UINT)s, "&Update Speed");
    AppendMenu(bar, MF_POPUP, (UINT)m, "&View");
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, TMC_ABOUT, "&About Task Manager");
    AppendMenu(bar, MF_POPUP, (UINT)m, "&Help");
    windowsMenu = CreatePopupMenu();
    AppendMenu(windowsMenu, MF_STRING, TMC_TILEH, "Tile &Horizontally");
    AppendMenu(windowsMenu, MF_STRING, TMC_TILEV, "Tile &Vertically");
    AppendMenu(windowsMenu, MF_STRING, TMC_MINIMIZE, "&Minimize");
    AppendMenu(windowsMenu, MF_STRING, TMC_MAXIMIZE, "Ma&ximize");
    AppendMenu(windowsMenu, MF_STRING, TMC_CASCADE, "&Cascade");
    AppendMenu(windowsMenu, MF_STRING, TMC_FRONT, "&Bring To Front");
    return bar;
}

static void Checks(HWND dlg)
{
    HMENU m = GetMenu(dlg);
    int i;

    CheckMenuItem(m, TMC_ONTOP, onTop ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(m, TMC_MINUSE, minUse ? MF_CHECKED : MF_UNCHECKED);
    for (i = 0; i < 4; i++)
        CheckMenuItem(m, TMC_SPEED + i, i == speed ? MF_CHECKED : MF_UNCHECKED);
}

/* The page shown and the status bar, after a new sample if asked. */
static void Update(HWND dlg, BOOL sample)
{
    RECT r;

    if (sample)
        PerformanceSample();
    if (IsIconic(dlg))
        return;
    if (page == 0)
        ApplicationsRefresh(dlg);
    else if (page == 1)
        ProcessesRefresh(dlg);
    else
        InvalidateRect(dlg, &pageRect, FALSE);
    GetClientRect(dlg, &r);
    r.top = r.bottom - STATUS_H;
    InvalidateRect(dlg, &r, FALSE);
}

/* The controls of the other pages hide; the Windows menu is there only
   for Applications. */
static void ShowPage(HWND dlg, int p)
{
    HMENU bar = GetMenu(dlg);
    HWND c;
    int id;

    page = p;
    for (c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        id = GetDlgCtrlID(c);
        if (id >= IDC_TASKMAN_LIST && id <= IDC_TASKMAN_KILL)
            ShowWindow(c, (id < IDC_TASKMAN_PROCS ? 0 : 1) == p ? SW_SHOW : SW_HIDE);
    }
    if (GetFocus() && !IsWindowVisible(GetFocus()))
        SetFocus(GetDlgItem(dlg, IDC_TABS));
    if (GetMenuItemCount(bar) > 4)
        RemoveMenu(bar, WINDOWS_AT, MF_BYPOSITION);
    if (p == 0)
        InsertMenu(bar, WINDOWS_AT, MF_BYPOSITION | MF_POPUP, (UINT)windowsMenu, "&Windows");
    DrawMenuBar(dlg);
    Update(dlg, FALSE);
    RedrawWindow(dlg, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static void Layout(HWND dlg)
{
    RECT c, t;

    GetClientRect(dlg, &c);
    SetRect(&t, MARGIN, MARGIN, c.right - MARGIN, c.bottom - STATUS_H - 2);
    MoveWindow(GetDlgItem(dlg, IDC_TABS), t.left, t.top, t.right - t.left, t.bottom - t.top, TRUE);
    SetRect(&pageRect, t.left + INSET, t.top + TABS_H + INSET, t.right - INSET, t.bottom - INSET);
    ApplicationsLayout(dlg, &pageRect);
    ProcessesLayout(dlg, &pageRect);
    PerformanceLayout(&pageRect);
}

static void PaintStatus(HDC dc, HWND dlg)
{
    static const int widths[] = { 96, 104, 0 };
    char procs[24], usage[24], memory[48], used[16], total[16];
    LPCSTR texts[3];
    RECT c;

    wsprintf(procs, "Processes: %d", GetNumTasks());
    wsprintf(usage, "CPU Usage: %d%%", PerformanceCpu());
    FormatNumber(PerformanceMemoryUsed() / 1024, used);
    FormatNumber(PerformanceMemoryTotal() / 1024, total);
    wsprintf(memory, "Mem Usage: %sK / %sK", (LPSTR)used, (LPSTR)total);
    texts[0] = procs;
    texts[1] = usage;
    texts[2] = memory;
    GetClientRect(dlg, &c);
    PaintStatusBar(dc, &c, 3, widths, texts, NULL, !IsZoomed(dlg));
}

static void SetSpeed(HWND dlg, int s)
{
    speed = s;
    KillTimer(dlg, TIMER_UPDATE);
    if (speeds[speed])
        SetTimer(dlg, TIMER_UPDATE, speeds[speed], NULL);
    Checks(dlg);
}

static void SetOnTop(HWND dlg, BOOL on)
{
    onTop = on;
    SetWindowPos(dlg, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    Checks(dlg);
}

/* Where it was last time, or in the middle of the screen. */
static void Place(HWND dlg)
{
    int x = SettingsGetInt(SECTION, "Left", -1), y = SettingsGetInt(SECTION, "Top", -1);
    int w = SettingsGetInt(SECTION, "Width", 0), h = SettingsGetInt(SECTION, "Height", 0);

    if (x >= 0 && y >= 0 && w > 0 && h > 0)
        SetWindowPos(dlg, NULL, x, y, max(w, MIN_W), max(h, MIN_H), SWP_NOZORDER | SWP_NOACTIVATE);
    else
        WindowCenter(dlg);
}

static void Save(HWND dlg)
{
    WINDOWPLACEMENT p;

    p.length = sizeof p;
    GetWindowPlacement(dlg, &p);
    SettingsSetInt(SECTION, "Left", p.rcNormalPosition.left);
    SettingsSetInt(SECTION, "Top", p.rcNormalPosition.top);
    SettingsSetInt(SECTION, "Width", p.rcNormalPosition.right - p.rcNormalPosition.left);
    SettingsSetInt(SECTION, "Height", p.rcNormalPosition.bottom - p.rcNormalPosition.top);
    SettingsSetInt(SECTION, "Page", page);
    SettingsSetInt(SECTION, "Speed", speed);
    SettingsSetInt(SECTION, "OnTop", onTop);
    SettingsSetInt(SECTION, "MinimizeOnUse", minUse);
}

static void Init(HWND dlg)
{
    HWND tabs = GetDlgItem(dlg, IDC_TABS);

    manager = dlg;
    page = min(max(SettingsGetInt(SECTION, "Page", 0), 0), PAGES - 1);
    minUse = SettingsGetInt(SECTION, "MinimizeOnUse", 1);
    CheckDlgButton(dlg, IDC_TASKMAN_LIBS, SettingsGetInt(SECTION, "Libraries", 0));
    SetMenu(dlg, MakeMenu());
    TabsAdd(tabs, "Applications");
    TabsAdd(tabs, "Processes");
    TabsAdd(tabs, "Performance");
    TabsSelect(tabs, page);
    SkinDialog(dlg);
    Place(dlg);
    Layout(dlg);
    SetSpeed(dlg, min(max(SettingsGetInt(SECTION, "Speed", 1), 0), 3));
    SetOnTop(dlg, SettingsGetInt(SECTION, "OnTop", 0));
    ShowPage(dlg, page);
}

static void Command(HWND dlg, UINT id, UINT code)
{
    switch (id) {
    case IDC_TABS:
        if (code == TCN_SELECT)
            ShowPage(dlg, TabsSelected(GetDlgItem(dlg, IDC_TABS)));
        break;
    case TMC_RUN:
        RunDialogShow(dlg);
        break;
    case TMC_EXIT:
    case IDCANCEL:
        DestroyWindow(dlg);
        break;
    case TMC_ONTOP:
        SetOnTop(dlg, !onTop);
        break;
    case TMC_MINUSE:
        minUse = !minUse;
        Checks(dlg);
        break;
    case TMC_REFRESH:
        Update(dlg, TRUE);
        break;
    case TMC_SPEED: case TMC_SPEED + 1: case TMC_SPEED + 2: case TMC_SPEED + 3:
        SetSpeed(dlg, id - TMC_SPEED);
        break;
    case TMC_TILEH:
        TaskbarArrange(ARR_TILEH);
        break;
    case TMC_TILEV:
        TaskbarArrange(ARR_TILEV);
        break;
    case TMC_CASCADE:
        TaskbarArrange(ARR_CASCADE);
        break;
    case TMC_ABOUT:
        AboutDialogShow(dlg);
        break;
    default:                            /* a page's controls (Enter: Switch To) */
        if (page == 1 && id >= IDC_TASKMAN_PROCS && id <= IDC_TASKMAN_KILL)
            ProcessesCommand(dlg, id, code);
        else if (page == 0)
            ApplicationsCommand(dlg, id, code);
        break;
    }
}

BOOL CALLBACK __export TaskManagerProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HFONT old;

    switch (msg) {
    case WM_INITDIALOG:
        if (!ProcessesInit() || !PerformanceInit()) {
            ErrorBox(NULL, "There is not enough memory to start Task Manager.");
            DestroyWindow(dlg);
            return TRUE;
        }
        Init(dlg);
        return TRUE;
    case WM_ACTIVATE:                   /* modeless: the main loop hands it its keys */
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        break;
    case WM_NCHITTEST:
        if (!StatusBarGrip(dlg, lp))
            return FALSE;
        SetWindowLong(dlg, DWL_MSGRESULT, HTBOTTOMRIGHT);
        return TRUE;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO FAR *)lp)->ptMinTrackSize.x = MIN_W;
        ((MINMAXINFO FAR *)lp)->ptMinTrackSize.y = MIN_H;
        return TRUE;
    case WM_SIZE:
        if (manager && wp != SIZE_MINIMIZED) {
            Layout(dlg);
            RedrawWindow(dlg, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }
        break;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight =
            ((MEASUREITEMSTRUCT FAR *)lp)->CtlID == IDC_TASKMAN_PROCS ? 16 : 18;
        return TRUE;
    case WM_DRAWITEM:
        if (((const DRAWITEMSTRUCT FAR *)lp)->CtlID == IDC_TASKMAN_PROCS)
            ProcessesDraw((const DRAWITEMSTRUCT FAR *)lp);
        else
            ApplicationsDraw((const DRAWITEMSTRUCT FAR *)lp);
        return TRUE;
    case WM_PAINT:
        BeginPaint(dlg, &ps);
        old = SelectObject(ps.hdc, DrawFont(FALSE));
        SetBkMode(ps.hdc, TRANSPARENT);
        if (page == 0)
            ApplicationsPaint(ps.hdc, dlg);
        else if (page == 1)
            ProcessesPaint(ps.hdc, dlg);
        else
            PerformancePaint(ps.hdc);
        PaintStatus(ps.hdc, dlg);
        SelectObject(ps.hdc, old);
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_LBUTTONDOWN:
        if (page == 1)
            ProcessesClick(dlg, (int)LOWORD(lp), (int)HIWORD(lp));
        break;
    case WM_TIMER:
        if (wp == TIMER_END)
            ApplicationsWaitEnd(dlg);
        else
            Update(dlg, TRUE);
        return TRUE;
    case WM_COMMAND:
        Command(dlg, wp, HIWORD(lp));
        return TRUE;
    case WM_DESTROY:
        KillTimer(dlg, TIMER_UPDATE);
        KillTimer(dlg, TIMER_END);
        if (manager) {
            Save(dlg);
            if (page != 0)              /* not in the menu bar, which goes with the window */
                DestroyMenu(windowsMenu);
        }
        MessageLoopTrackDialog(dlg, FALSE);
        ApplicationsDone();
        ProcessesDone();
        PerformanceDone();
        manager = NULL;
        return TRUE;
    }
    return FALSE;
}

void TaskManagerShow(void)
{
    if (manager) {
        if (IsIconic(manager))
            ShowWindow(manager, SW_RESTORE);
        SetActiveWindow(manager);
    } else if (CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_TASKMAN), NULL, TaskManagerProc) && manager) {
        ShowWindow(manager, SW_SHOWNORMAL);
    }
}
