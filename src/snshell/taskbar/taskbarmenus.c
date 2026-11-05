/* The taskbar's menus and their commands: its own (Toolbars, Cascade...),
   the Start button's, a task button's system menu, a group's windows;
   and the Windows key's shortcuts. */
#include "internal.h"
#include "taskbarint.h"

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
#define TBC_LOCK    30                  /* Lock the Taskbar */
#define TBC_ONTOP   31                  /* a task's Always on Top */
#define TBC_DATETIME 32                 /* the clock's Adjust Date/Time */
#define TBC_TASK    300                 /* + index in TaskbarTasks: a group's list */

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

/* The Windows key: alone, the Start menu; Win+E Explorer, Win+R Run, Win+F
   Find, Win+D the desktop (and back), Win+M everything minimized (Shift:
   back), Win+Break System Properties - as Windows 98's. */
void TaskbarWinKey(WPARAM key, BOOL shift)
{
    PATHLIST l;

    switch (key) {
    case 0:         SetActiveWindow(g_taskbar); TaskbarShowStart(); break;
    case 'E':       FolderWindowExplore(""); break;
    case 'R':       RunDialogShow(g_taskbar); break;
    case 'F':       FindShow(""); break;
    case 'D':       QuickLaunchShowDesktop(SD_TOGGLE); break;
    case 'M':       QuickLaunchShowDesktop(shift ? SD_RESTORE : SD_MINIMIZE); break;
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        TaskbarSnap(GetActiveWindow(), key);
        break;
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
void TaskbarGroupMenu(int lead)
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

void TaskbarMenu(BOOL clock)
{
    HMENU m = CreatePopupMenu(), bars = CreatePopupMenu();

    if (clock) {
        AppendMenu(m, MF_STRING, TBC_DATETIME, "&Adjust Date/Time");
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
    }
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
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING | (TaskbarLocked ? MF_CHECKED : 0), TBC_LOCK, "&Lock the Taskbar");
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

void TaskbarStartButtonMenu(void)
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

/* Its window's system menu, Always on Top added for as long as it shows. */
void TaskbarTaskMenu(HWND w)
{
    HMENU m = GetSystemMenu(w, FALSE);
    int n;

    if (!m)
        return;
    SetActiveWindow(w);
    menuTarget = w;
    n = GetMenuItemCount(m);
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING | ((GetWindowLong(w, GWL_EXSTYLE) & WS_EX_TOPMOST) ? MF_CHECKED : 0), TBC_ONTOP,
               "Always on &Top");
    Popup(m, 0);
    if (IsWindow(w)) {
        DeleteMenu(m, n + 1, MF_BYPOSITION);
        DeleteMenu(m, n, MF_BYPOSITION);
    }
}

/* The menus' commands: the taskbar's, the Start button's, Quick Launch's, a task's. */
void TaskbarCommand(HWND w, WPARAM id)
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
    else if (id == TBC_DATETIME)
        DateTimeShow();
    else if (id == TBC_LOCK) {
        TaskbarLocked = !TaskbarLocked;
        SettingsSetInt("Taskbar", "Locked", TaskbarLocked);
    }
    else if (id == TBC_ONTOP && IsWindow(menuTarget))
        SetWindowPos(menuTarget, (GetWindowLong(menuTarget, GWL_EXSTYLE) & WS_EX_TOPMOST) ? HWND_NOTOPMOST :
                     HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    else if (id == TBC_QL) {
        SettingsSetInt("Taskbar", "QuickLaunch", !SettingsGetInt("Taskbar", "QuickLaunch", 1));
        TaskbarApply();
    }
    else if (id >= ARR_CASCADE && id <= ARR_MINALL)
        TaskbarArrange(id);
    else if (IsWindow(menuTarget) && id >= 0xF000)  /* a task button's system menu */
        PostMessage(menuTarget, WM_SYSCOMMAND, id, 0L);
}

