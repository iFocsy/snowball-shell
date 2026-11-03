/*
 * The taskbar: Start button, Quick Launch, one button per top-level window,
 * tray clock, and its menu (Toolbars, Cascade, Tile, Minimize All, Task
 * Manager, Properties). The Start button's own menu: Windows 98's Open,
 * Explore and Find for the Start Menu folder, then quick links to the main
 * parts of Windows. Alt+Tab goes to the switcher. A button flashes with its
 * window's caption (FlashWindow). Something dragged resting on a button
 * brings its window forward (Windows 98), from Snowball's views or File
 * Manager. Painted by hand in a single window (no child controls).
 * SNSYSTEM.DLL posts window events; a slow timer reconciles anything missed.
 * Reading another program's window title is a message to that program
 * (Windows switches to it and waits), so titles are cached and read only
 * when the hook reports a change.
 * Its parts: taskbar.c the window, its messages, the task list and menus;
 * taskbarlayout.c where things are and painting; taskbartray.c the tray.
 */
#include "internal.h"
#include "taskbarint.h"

#define SBM_PARK    (WM_USER + 120)     /* move a minimized window's icon away */
#define TBC_TASKMAN 5                   /* the taskbar's menu: ARR_* and these */
#define TBC_QL      6
#define TBC_PROPS   7
#define TBC_OPEN    8                   /* the Start button's menu */
#define TBC_EXPLORE 9
#define TBC_FIND    10
#define TBC_COMPUTER 11
#define TBC_RUN     12
#define TBC_SHUTDOWN 13
#define TBC_LINK    20                  /* + index in links[] */
#define TBC_TASK    300                 /* + index in TaskbarTasks: a group's list */
#define WM_DRAGSELECT   0x022E          /* Windows 3.1's DragObject (File Manager): */
#define WM_DRAGMOVE     0x022F          /* over a window that takes the drop */

HWND g_taskbar;
HWND TaskbarTasks[MAX_TASKS];
char TaskbarTitles[MAX_TASKS][TITLE_MAX];
BOOL TaskbarLit[MAX_TASKS];             /* flashing: the caption looks active, the window is not */
static HMODULE modules[MAX_TASKS];      /* each task's program: grouping */
int TaskbarNTasks;
HWND TaskbarActive;
int TaskbarPressed = NONE;              /* TaskbarHitButton's codes */
int TaskbarHot = NONE;                  /* under the mouse */
BOOL TaskbarStartOpen, TaskbarMenuUp;
static HWND dragOn;                     /* the window whose button something is dragged over */
static DWORD dragSince;

/* A visible top-level window that is not ours, nor a tray program's or one
   kept off the taskbar; none of this asks the window. */
static BOOL Qualifies(HWND w)
{
    char cls[16];

    if (!IsWindow(w) || !IsWindowVisible(w) || GetWindow(w, GW_OWNER) ||
        (GetWindowLong(w, GWL_STYLE) & WS_CHILD))
        return FALSE;
    GetClassName(w, cls, sizeof cls);
    return lstrcmp(cls, DESKTOP_CLASS) && lstrcmp(cls, TASKBAR_CLASS) && TaskbarTrayProgram(w) < 0 && !TaskbarHidden(w);
}

/* The i-th button's window and its cached title; NULL past the last. */
HWND TaskbarButtonWindow(int i, LPCSTR *title)
{
    if (i >= TaskbarNTasks)
        return NULL;
    *title = TaskbarTitles[i];
    return TaskbarTasks[i];
}

LPCSTR TaskbarTitle(HWND w)
{
    int i;

    for (i = 0; i < TaskbarNTasks; i++)
        if (TaskbarTasks[i] == w)
            return TaskbarTitles[i];
    return "";
}

HICON TaskbarIcon(HWND w)
{
    HICON icon = (HICON)GetClassWord(w, GCW_HICON);
    return icon ? icon : IconStock(IDI_PROGRAM);
}

int TaskbarFind(HWND w)
{
    int i;
    for (i = 0; i < TaskbarNTasks; i++)
        if (TaskbarTasks[i] == w)
            return i;
    return -1;
}

BOOL TaskbarHasButton(HWND w)
{
    return TaskbarFind(w) >= 0;
}

/* A minimized window's icon goes below the screen (its button stands for
   it). Hidden and placed again, so that its title goes along with it;
   placing it minimizes it again, and then it is found parked. */
static void Park(HWND w)
{
    WINDOWPLACEMENT p;
    int below = GetSystemMetrics(SM_CYSCREEN);

    p.length = sizeof p;
    if (!IsIconic(w) || TaskbarHidden(w) || !GetWindowPlacement(w, &p) || p.ptMinPosition.y >= below)
        return;
    p.ptMinPosition.x = 0;
    p.ptMinPosition.y = below;
    p.flags |= WPF_SETMINPOSITION;
    p.showCmd = SW_SHOWMINNOACTIVE;
    ShowWindow(w, SW_HIDE);
    SetWindowPlacement(w, &p);
}

static void Drop(int i)
{
    _fmemmove(TaskbarTasks + i, TaskbarTasks + i + 1, (TaskbarNTasks - i - 1) * sizeof(HWND));
    _fmemmove(TaskbarTitles[i], TaskbarTitles[i + 1], (TaskbarNTasks - i - 1) * TITLE_MAX);
    _fmemmove(TaskbarLit + i, TaskbarLit + i + 1, (TaskbarNTasks - i - 1) * sizeof(BOOL));
    _fmemmove(modules + i, modules + i + 1, (TaskbarNTasks - i - 1) * sizeof(HMODULE));
    TaskbarNTasks--;
}

static HMODULE ModuleOf(HWND w)
{
    char path[MAX_PATH];

    GetModuleFileName((HINSTANCE)GetWindowWord(w, GWW_HINSTANCE), path, sizeof path);
    return GetModuleHandle(path);
}

/* With grouping, a program's windows share the button of its first one
   (the lead); the others have none. */
int TaskbarLead(int i)
{
    int j;

    if (TaskbarGrouping)
        for (j = 0; j < i; j++)
            if (modules[j] == modules[i])
                return j;
    return i;
}

int TaskbarGroupSize(int lead)
{
    int j, n = 0;

    for (j = lead; j < TaskbarNTasks; j++)
        if (TaskbarLead(j) == lead)
            n++;
    return n;
}

int TaskbarSlot(int i)
{
    int j, n = 0;

    if (TaskbarLead(i) != i)
        return -1;
    for (j = 0; j < i; j++)
        if (TaskbarLead(j) == j)
            n++;
    return n;
}

int TaskbarSlots(void)
{
    int j, n = 0;

    for (j = 0; j < TaskbarNTasks; j++)
        if (TaskbarLead(j) == j)
            n++;
    return n;
}

/* Reads one title (the only message the taskbar sends to other programs);
   a window without a title loses its button. */
static void ReadTitle(int i)
{
    if (!GetWindowText(TaskbarTasks[i], TaskbarTitles[i], TITLE_MAX))
        Drop(i);
    InvalidateRect(g_taskbar, NULL, FALSE);
}

/* Adds and drops windows so the buttons match reality. */
void TaskbarRescan(void)
{
    HWND w;
    int i, changed = 0;
    BOOL lost = HookLost();             /* a title change may have been missed */

    for (i = TaskbarNTasks - 1; i >= 0; i--)
        if (!Qualifies(TaskbarTasks[i])) {
            Drop(i);
            changed++;
        } else if (lost) {
            ReadTitle(i);
        }
    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT))
        if (TaskbarFind(w) < 0 && TaskbarNTasks < MAX_TASKS && Qualifies(w) &&
            GetWindowText(w, TaskbarTitles[TaskbarNTasks], TITLE_MAX)) {
            TaskbarLit[TaskbarNTasks] = FALSE;
            modules[TaskbarNTasks] = ModuleOf(w);
            TaskbarTasks[TaskbarNTasks++] = w;
            Park(w);
            changed++;
        }
    w = GetActiveWindow();
    if (w != g_taskbar && w != TaskbarActive) {
        TaskbarActive = w;
        changed++;
    }
    if (changed)
        InvalidateRect(g_taskbar, NULL, FALSE);
}

static UINT Popup(HMENU m, UINT flags)
{
    POINT p;
    UINT r;

    GetCursorPos(&p);
    TooltipHide();
    TaskbarMenuUp = TRUE;
    r = TrackPopupMenu(m, flags | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, 0, g_taskbar, NULL);
    TaskbarMenuUp = FALSE;
    return r;
}

/* To the front; a window busy with a dialog gets the dialog. */
void TaskbarSwitchTo(HWND w)
{
    if (IsIconic(w))
        ShowWindow(w, SW_RESTORE);
    BringWindowToTop(w);
    SetActiveWindow(GetLastActivePopup(w));
}

/* Something dragged is over p (NULL: the drag ended or left the taskbar);
   resting on a task button, it brings the button's window forward, so that
   it can be dropped in there. */
void TaskbarDragOver(const POINT FAR *p)
{
    HWND on = p ? TaskbarTaskAt(*p) : NULL;

    if (on == dragOn)
        return;
    dragOn = on;
    dragSince = GetTickCount();
    if (on)
        SetTimer(g_taskbar, TIMER_DRAG, 200, NULL);
    else
        KillTimer(g_taskbar, TIMER_DRAG);
}

static void DragRests(void)
{
    POINT p;

    GetCursorPos(&p);
    TaskbarDragOver(&p);
    if (dragOn && GetTickCount() - dragSince >= DRAG_MS) {
        KillTimer(g_taskbar, TIMER_DRAG);
        if (dragOn != TaskbarActive || IsIconic(dragOn))
            TaskbarSwitchTo(dragOn);
    }
}

/* Files dropped on a task button: into the window, if it takes files;
   else Windows 98's word on how it is done. */
void TaskbarDropOnTask(HWND owner, HWND task, HDROP drop)
{
    if (drop && (GetWindowLong(task, GWL_EXSTYLE) & WS_EX_ACCEPTFILES)) {
        PostMessage(task, WM_DROPFILES, (WPARAM)drop, 0L);
        return;
    }
    if (drop)
        DragFinish(drop);
    MessageBox(owner, "You cannot drop an item onto a button on the taskbar.\n\nHowever, if you drag the "
                      "item over a button without releasing the mouse button, the window will open after a "
                      "moment, allowing you to drop the item inside the window.", "Taskbar", MB_OK | MB_ICONINFORMATION);
}

/* The Windows key: alone, the Start menu; Win+E Explorer, Win+R Run, Win+F
   Find, Win+D the desktop (and back), Win+M everything minimized (Shift:
   back), Win+Break System Properties - as Windows 98's. */
static void WinKey(WPARAM key, BOOL shift)
{
    PATHLIST l;

    switch (key) {
    case 0:         SetActiveWindow(g_taskbar); TaskbarShowStart(); break;
    case 'E':       FolderWindowExplore(""); break;
    case 'R':       RunDialogShow(g_taskbar); break;
    case 'F':       FindShow(""); break;
    case 'D':       QuickLaunchShowDesktop(SD_TOGGLE); break;
    case 'M':       QuickLaunchShowDesktop(shift ? SD_RESTORE : SD_MINIMIZE); break;
    case VK_PAUSE:
        PathListInit(&l);
        PathListAdd(&l, "");
        PropertiesShow(g_taskbar, &l, IVK_COMPUTER);
        PathListFree(&l);
        break;
    }
}

/* A group's windows, to pick one: a menu by its button (the skin keeps
   menus above the taskbar), the active one checked. */
static int groupShown = NONE;           /* opened by resting on it: not again until the mouse leaves */

static void GroupMenu(int lead)
{
    HMENU m = CreatePopupMenu();
    RECT r;
    int j;

    for (j = lead; j < TaskbarNTasks; j++)
        if (TaskbarLead(j) == lead)
            AppendMenu(m, MF_STRING | (TaskbarTasks[j] == TaskbarActive ? MF_CHECKED : 0), TBC_TASK + j,
                       TaskbarTitles[j]);
    TaskbarButtonRect(lead, &r);
    ClientToScreen(g_taskbar, (POINT FAR *)&r.left);
    TooltipHide();
    TaskbarMenuUp = TRUE;
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_RIGHTBUTTON, r.left, r.top, 0, g_taskbar, NULL);
    TaskbarMenuUp = FALSE;
    DestroyMenu(m);
}

void TaskbarGroupLeft(void)
{
    groupShown = NONE;
}

static void ClickTask(HWND w)
{
    if (!IsWindow(w))
        return;
    if (w == TaskbarActive && !IsIconic(w))
        ShowWindow(w, SW_MINIMIZE);
    else
        TaskbarSwitchTo(w);
}

static void BarMenu(void)
{
    HMENU m = CreatePopupMenu(), bars = CreatePopupMenu();

    AppendMenu(bars, MF_STRING | (SettingsGetInt("Taskbar", "QuickLaunch", 1) ? MF_CHECKED : 0), TBC_QL, "&Quick Launch");
    AppendMenu(m, MF_POPUP, (UINT)bars, "&Toolbars");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, ARR_CASCADE, "&Cascade Windows");
    AppendMenu(m, MF_STRING, ARR_TILEH, "Tile Windows &Horizontally");
    AppendMenu(m, MF_STRING, ARR_TILEV, "Tile Windows V&ertically");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, ARR_MINALL, "&Minimize All Windows");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, TBC_TASKMAN, "Tas&k Manager...");
    AppendMenu(m, MF_STRING, TBC_PROPS, "P&roperties");
    Popup(m, 0);
    DestroyMenu(m);
}

/* The Start button's quick links: programs of Windows itself. */
static const struct { LPCSTR label, command; } links[] = {
    { "&Control Panel", "CONTROL.EXE" },
    { "&Windows Setup", "WINSETUP.EXE" },
    { "&Printers", "PRINTMAN.EXE" },
    { "&File Manager", "WINFILE.EXE" },
    { "Progr&am Manager", "PROGMAN.EXE" },
    { "MS-DOS &Prompt", "DOSPRMPT.PIF" },
    { "System Confi&guration Editor", "SYSEDIT.EXE" },
};
#define NLINKS  (sizeof links / sizeof links[0])

static void StartButtonMenu(void)
{
    HMENU m = CreatePopupMenu();
    int i;

    AppendMenu(m, MF_STRING, TBC_OPEN, "&Open");
    AppendMenu(m, MF_STRING, TBC_EXPLORE, "&Explore");
    AppendMenu(m, MF_STRING, TBC_FIND, "F&ind...");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, TBC_COMPUTER, "&My Computer");
    for (i = 0; i < NLINKS; i++)
        AppendMenu(m, MF_STRING, TBC_LINK + i, links[i].label);
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, TBC_TASKMAN, "Tas&k Manager...");
    AppendMenu(m, MF_STRING, TBC_PROPS, "&Taskbar && Start Menu...");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, TBC_RUN, "&Run...");
    AppendMenu(m, MF_STRING, TBC_SHUTDOWN, "Sh&ut Down...");
    Popup(m, 0);
    DestroyMenu(m);
}

static void StartButtonCommand(UINT id)
{
    char dir[MAX_PATH];

    StartMenuFolder(dir);
    switch (id) {
    case TBC_OPEN:     FolderWindowOpen(dir); break;
    case TBC_EXPLORE:  FolderWindowExplore(dir); break;
    case TBC_FIND:     FindShow(dir); break;
    case TBC_COMPUTER: FolderWindowOpen(""); break;
    case TBC_RUN:      RunDialogShow(g_taskbar); break;
    case TBC_SHUTDOWN: ShutDownDialogShow(g_taskbar); break;
    default:
        if (id >= TBC_LINK && id < TBC_LINK + NLINKS)
            LaunchCommand(g_taskbar, links[id - TBC_LINK].command, NULL, SW_SHOWNORMAL);
    }
}

static HWND menuTarget;

static void TaskMenu(HWND w)
{
    HMENU m = GetSystemMenu(w, FALSE);

    if (!m)
        return;
    SetActiveWindow(w);
    menuTarget = w;
    Popup(m, 0);
}

/* The mouse moved: the taskbar dragged along, or the item under it. */
static void MouseMove(int x, int y)
{
    POINT p;

    if (TaskbarSizing) {
        GetCursorPos(&p);
        TaskbarDragged(p);
        return;
    }
    TaskbarHover(TaskbarHitButton(x, y));
}

/* Pressed: the taskbar grabbed (its empty part or inner edge), or a button. */
static void ButtonDown(HWND w, int x, int y)
{
    TooltipHide();
    if (TaskbarOnInnerEdge(x, y) || TaskbarHitButton(x, y) == NONE) {
        TaskbarSizing = TaskbarOnInnerEdge(x, y) ? SZ_SIZE : SZ_GRAB;
        GetCursorPos(&TaskbarGrabbed);
        SetCapture(w);
        return;
    }
    TaskbarPressed = TaskbarHitButton(x, y);
    InvalidateRect(w, NULL, FALSE);
    if (TaskbarPressed == -1)
        TaskbarShowStart();
    else
        SetCapture(w);
}

/* Released: the drag done, or a click on the button pressed. */
static void ButtonUp(HWND w, int x, int y)
{
    int i;

    if (TaskbarSizing) {
        TaskbarDragDone();
        return;
    }
    ReleaseCapture();
    i = TaskbarHitButton(x, y);
    if (i >= 0 && i == TaskbarPressed && TaskbarGroupSize(i) > 1)
        GroupMenu(i);
    else if (i >= 0 && i == TaskbarPressed)
        ClickTask(TaskbarTasks[i]);
    else if (i <= QL(0) && i == TaskbarPressed)
        QuickLaunchOpen(w, QL(0) - i);
    else if (i <= TRAYICON(0) && i > QL(0) && i == TaskbarPressed)
        TaskbarClickTray(TRAYICON(0) - i);
    TaskbarPressed = NONE;
    InvalidateRect(w, NULL, FALSE);
}

/* A right click: the menu of the task, the Start button, Quick Launch or the taskbar. */
static void RightButtonUp(HWND w, int x, int y)
{
    int i;

    TooltipHide();
    i = TaskbarHitButton(x, y);
    if (i >= 0)
        TaskMenu(TaskbarTasks[i]);
    else if (i == -1)
        StartButtonMenu();
    else if (i <= QL(0)) {
        TaskbarMenuUp = TRUE;
        QuickLaunchMenu(w, QL(0) - i);
        TaskbarMenuUp = FALSE;
    } else if (i == NONE)
        BarMenu();
}

/* Something File Manager drags moved over the taskbar. */
static void DragMove(void)
{
    POINT p;

    GetCursorPos(&p);
    TaskbarDragOver(&p);
}

/* Files dropped from File Manager: on a task button or on Quick Launch. */
static void DropFiles(HWND w, HDROP drop)
{
    POINT p;
    RECT r;
    int i;

    TaskbarDragOver(NULL);
    DragQueryPoint(drop, &p);
    if ((i = TaskbarHitButton(p.x, p.y)) >= 0) {
        TaskbarDropOnTask(w, TaskbarTasks[i], drop);
        return;
    }
    TaskbarQuickLaunchBand(&r);
    QuickLaunchPlace(&r, TaskbarVertical());
    if (QuickLaunchHas(p.x, p.y)) {
        QuickLaunchAddFiles(drop);
        InvalidateRect(w, NULL, FALSE);
    }
    DragFinish(drop);
}

/* The menus' commands: the taskbar's, the Start button's, Quick Launch's, a task's. */
static void Command(HWND w, WPARAM id)
{
    if (id >= TBC_TASK && id < TBC_TASK + MAX_TASKS && id - TBC_TASK < (WPARAM)TaskbarNTasks)
        TaskbarSwitchTo(TaskbarTasks[id - TBC_TASK]);       /* a window of a group's list */
    else if (id == TBC_TASKMAN)
        TaskManagerShow();
    else if (id == TBC_PROPS)
        TaskbarPropertiesShow();
    else if (id >= QLC_OPEN && id <= QLC_PROPS)
        QuickLaunchCommand(w, id);
    else if (id >= TBC_OPEN && id < TBC_LINK + NLINKS)
        StartButtonCommand(id);
    else if (id == TBC_QL) {
        SettingsSetInt("Taskbar", "QuickLaunch", !SettingsGetInt("Taskbar", "QuickLaunch", 1));
        TaskbarApply();
    }
    else if (id >= ARR_CASCADE && id <= ARR_MINALL)
        TaskbarArrange(id);
    else if (IsWindow(menuTarget) && id >= 0xF000)  /* a task button's system menu */
        PostMessage(menuTarget, WM_SYSCOMMAND, id, 0L);
}

/* The timers, by id; any other is the clock's new minute. */
static void Timer(HWND w, WPARAM id)
{
    POINT p;
    RECT r;
    BOOL moved;

    switch (id) {
    case TIMER_SCAN:
        TaskbarRescan();
        break;
    case TIMER_HIDE:
        TaskbarAutoHide();
        break;
    case TIMER_DRAG:
        DragRests();
        break;
    case TIMER_OPEN:
        LaunchOpenRequest(w);
        break;
    case TIMER_TIP:                     /* none over a menu (timers go on in its loop) */
        KillTimer(w, TIMER_TIP);
        if (!TaskbarMenuUp && !TaskbarStartOpen && TaskbarHot >= 0 && TaskbarHot < TaskbarNTasks &&
            TaskbarGroupSize(TaskbarHot) > 1 && TaskbarHot != groupShown) {
            groupShown = TaskbarHot;    /* resting on a group: its list (once, until the mouse leaves) */
            GroupMenu(TaskbarHot);
        } else if (!TaskbarMenuUp && !TaskbarStartOpen) {
            TaskbarTip();
        }
        break;
    case TIMER_HOT:
        GetCursorPos(&p);
        if (WindowFromPoint(p) != w)
            TaskbarHover(NONE);
        break;
    default:                            /* a new minute: repainting the tray re-arms it */
        moved = TaskbarClockMeasure();
        TaskbarTrayRect(&r);
        InvalidateRect(w, moved ? NULL : &r, FALSE);
        break;
    }
}

/* A window's title changed (SNSYSTEM's hook): its button's text. */
static void TitleChanged(HWND t)
{
    int i;

    HookTitleSeen();                    /* later changes of that window post again */
    if ((i = TaskbarFind(t)) >= 0)
        ReadTitle(i);
    else
        TaskbarRescan();                       /* it may qualify now that it has a title */
}

/* Another window active: its button down, and no longer lit. */
static void Activated(HWND w, HWND to)
{
    int i;

    if (to != w) {
        TaskbarActive = to;
        if ((i = TaskbarFind(TaskbarActive)) >= 0)
            TaskbarLit[i] = FALSE;
        InvalidateRect(w, NULL, FALSE);
    }
}

/* A caption flashed (FlashWindow): its button lit, or not, with it. */
static void Flash(HWND w, HWND t, LPARAM on)
{
    int i;

    if ((i = TaskbarFind(t)) >= 0 && TaskbarLit[i] != (on && t != GetActiveWindow())) {
        TaskbarLit[i] = !TaskbarLit[i];
        InvalidateRect(w, NULL, FALSE);
    }
}

/* A window minimized, maximized or restored: its icon parked, its sound. */
static void MinMax(HWND w, WPARAM wp, LPARAM lp)
{
    if (LOWORD(lp) == SW_MINIMIZE || LOWORD(lp) == SW_SHOWMINIMIZED || LOWORD(lp) == SW_SHOWMINNOACTIVE)
        PostMessage(w, SBM_PARK, wp, 0L);
    if (LOWORD(lp) == SW_MINIMIZE || LOWORD(lp) == SW_SHOWMINIMIZED)
        SoundPlay(SOUND_MINIMIZE);
    else if (LOWORD(lp) == SW_SHOWMAXIMIZED)
        SoundPlay(SOUND_MAXIMIZE);
    else if (LOWORD(lp) == SW_RESTORE && HIWORD(lp))
        SoundPlay(HIWORD(lp) == 1 ? SOUND_RESTOREUP : SOUND_RESTOREDOWN);
}

/* Closing: the timers and the hook go, and the shell's message loop ends. */
static void Destroy(HWND w)
{
    KillTimer(w, TIMER_CLOCK);
    KillTimer(w, TIMER_SCAN);
    KillTimer(w, TIMER_HIDE);
    KillTimer(w, TIMER_OPEN);
    HookRemove();
    PostQuitMessage(0);
}

LRESULT CALLBACK __export TaskbarProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res;

    if (SendToMessage(msg, wp, lp, &res) || StartMenuMessage(w, msg, wp, lp, &res))
        return res;
    switch (msg) {
    case WM_PAINT:
        TaskbarPaint(w);
        return 0;
    case WM_MOUSEACTIVATE:              /* clicks must not steal activation */
        return MA_NOACTIVATE;
    case WM_SETCURSOR:                  /* the inner edge resizes */
        if (TaskbarCursor(w, LOWORD(lp)))
            return TRUE;
        break;
    case WM_MOUSEMOVE:
        MouseMove(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_LBUTTONDBLCLK:              /* the clock: Date/Time; elsewhere a click */
        if (TaskbarDoubleClick(w, LOWORD(lp), HIWORD(lp)))
            return 0;
        /* fall through */
    case WM_LBUTTONDOWN:
        ButtonDown(w, LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        ButtonUp(w, LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_RBUTTONUP:
        RightButtonUp(w, LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_DRAGSELECT:                 /* File Manager drags over the taskbar, */
        if (!wp)                        /* ... or leaves it */
            TaskbarDragOver(NULL);
        break;
    case WM_DRAGMOVE:
        DragMove();
        break;
    case WM_DROPFILES:                  /* on Quick Launch: shortcuts to them; on a task: to it */
        DropFiles(w, (HDROP)wp);
        return 0;
    case WM_COMMAND:
        Command(w, wp);
        return 0;
    case WM_WININICHANGE:               /* the time's form (Control Panel), the time */
    case WM_TIMECHANGE:
        TaskbarClockChanged(w);
        return 0;
    case WM_TIMER:
        Timer(w, wp);
        return 0;
    case SBM_SHELL:
        SoundPlay(wp == HSHELL_WINDOWCREATED ? SOUND_OPEN : SOUND_CLOSE);
        TaskbarRescan();
        return 0;
    case SBM_SOUND:
        SoundPlay(wp);
        return 0;
    case SBM_WINKEY:                    /* from SNSYSTEM */
        WinKey(wp, (BOOL)lp);
        return 0;
    case SBM_TITLE:
        TitleChanged((HWND)wp);
        return 0;
    case SBM_ACTIVATE:
        Activated(w, (HWND)wp);
        return 0;
    case SBM_FLASH:                     /* lit while the caption looks active and the window is not */
        Flash(w, (HWND)wp, lp);
        return 0;
    case SBM_MINMAX:                    /* the sounds: not for parking's own minimizing */
        MinMax(w, wp, lp);
        return 0;
    case SBM_PARK:
        Park((HWND)wp);
        TaskbarRescan();
        return 0;
    case SBM_SERVE:                     /* patching COMMDLG while Windows still starts hung it at times */
        FileDialogsServe(w);
        return 0;
    case SBM_FILEDLG:                   /* from SNSYSTEM: a program's Open or Save As */
        OpenSaveDialogRequest((FILEREQUEST FAR *)lp);
        return 0;
    case SBM_STARTMENU:
        SetActiveWindow(w);
        TaskbarShowStart();
        return 0;
    case SBM_SWITCH:                    /* Alt+Tab, from SNSYSTEM */
        SwitcherCommand(wp);
        return 0;
    case SBM_HOTKEY:                    /* a shortcut's key, from SNSYSTEM */
        ShortcutKeysRun(wp);
        return 0;
    case SBM_TASKMAN:                   /* Ctrl+Shift+Esc, from a second start */
        TaskManagerShow();
        return 0;
    case WM_DESTROY:
        Destroy(w);
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

BOOL TaskbarCreate(void)
{
    WNDCLASS wc;
    int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);

    _fmemset(&wc, 0, sizeof wc);
    wc.style = CS_DBLCLKS;              /* the clock's double click */
    wc.lpfnWndProc = TaskbarProc;
    wc.hInstance = g_hInst;
    wc.hIcon = IconStock(IDI_SNOWBALL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = TASKBAR_CLASS;
    if (!RegisterClass(&wc))
        return FALSE;
    g_taskbar = CreateWindowEx(WS_EX_TOPMOST, TASKBAR_CLASS, "Taskbar", WS_POPUP,
                               0, cy - TB_H, cx, TB_H, NULL, NULL, g_hInst, NULL);
    if (!g_taskbar)
        return FALSE;
    HookInstall(g_taskbar, SettingsGetInt("Skin", "AllPrograms", 1));
    DragAcceptFiles(g_taskbar, TRUE);
    TaskbarApply();
    TaskbarRescan();
    ShortcutKeysScan();
    SoundsRegister();
    SetTimer(g_taskbar, TIMER_SCAN, SCAN_MS, NULL);
    SetTimer(g_taskbar, TIMER_OPEN, 1000, NULL);   /* the clock timer starts at the first paint */
    ShowWindow(g_taskbar, SW_SHOWNOACTIVATE);
    return TRUE;
}
