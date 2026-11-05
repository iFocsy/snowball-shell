/*
 * The tray, at the end of the taskbar: the icons of the programs chosen on
 * Taskbar Properties' Tray page, shown there instead of as buttons (a
 * click starts or shows one), and the time (12 or 24 hours, as Control
 * Panel says; a double click: Date/Time). Others chosen there get no
 * button at all (Calmira's applet tray and exclusions). Also the item
 * under the mouse and its tooltip: Quick Launch's buttons, the tray's
 * programs, the clock's date, task buttons too narrow for their titles.
 * traytools.c draws the tray's own icons, after the programs'.
 */
#include "internal.h"
#include "taskbarint.h"
#include <dos.h>

BOOL TaskbarShowClock;
int TaskbarClockW;                      /* the time's width, in pixels */
static char trayPrograms[MAX_TRAY][MAX_PATH];   /* SNOWBALL.INI [Tray]: icons by the clock */
HICON TaskbarTrayIcons[MAX_TRAY];
int TaskbarNTray;
static char hiddenNames[MAX_HIDDEN][13];        /* [Hidden]: no button ("CLOCK.EXE") */
static int nHidden;

/* The file name of a window's program ("CLOCK.EXE"). */
static void ProgramOf(HWND w, LPSTR name)
{
    char path[MAX_PATH];

    GetModuleFileName((HINSTANCE)GetWindowWord(w, GWW_HINSTANCE), path, sizeof path);
    lstrcpyn(name, PathFileName(path), 13);
    AnsiUpper(name);
}

/* The tray program (index) a window belongs to, or -1. */
int TaskbarTrayProgram(HWND w)
{
    char name[13];
    int i;

    ProgramOf(w, name);
    for (i = 0; i < TaskbarNTray; i++)
        if (!lstrcmpi(name, PathFileName(trayPrograms[i])))
            return i;
    return -1;
}

BOOL TaskbarHidden(HWND w)
{
    char name[13];
    int i;

    ProgramOf(w, name);
    for (i = 0; i < nHidden; i++)
        if (!lstrcmpi(name, hiddenNames[i]))
            return TRUE;
    return FALSE;
}

/* The lists of Taskbar Properties' Tray page: paths "1=..." in a section. */
void TaskbarLoadPrograms(void)
{
    char key[4], path[MAX_PATH];
    int i;

    TaskbarNTray = nHidden = 0;
    for (i = 1; i <= MAX_HIDDEN; i++) {
        wsprintf(key, "%d", i);
        SettingsGetString("Tray", key, "", path, sizeof path);
        if (path[0] && TaskbarNTray < MAX_TRAY) {
            lstrcpy(trayPrograms[TaskbarNTray], path);
            TaskbarTrayIcons[TaskbarNTray++] = IconForFile(path, FALSE);
        }
        SettingsGetString("Hidden", key, "", path, sizeof path);
        if (path[0]) {
            lstrcpyn(hiddenNames[nHidden], PathFileName(path), 13);
            AnsiUpper(hiddenNames[nHidden++]);
        }
    }
    TrayToolsLoad();
}

/* The time; the clock timer is set to fire again on the next minute. */
void TaskbarClockText(LPSTR out)
{
    char sep[4], am[8], pm[8];
    struct dostime_t t;
    int h;

    _dos_gettime(&t);
    SetTimer(g_taskbar, TIMER_CLOCK, (60 - t.second) * 1000U, NULL);
    GetProfileString("intl", "sTime", ":", sep, sizeof sep);
    if (GetProfileInt("intl", "iTime", 0)) {
        wsprintf(out, GetProfileInt("intl", "iTLZero", 0) ? "%02d%s%02d" : "%d%s%02d", t.hour, (LPSTR)sep, t.minute);
        return;
    }
    GetProfileString("intl", "s1159", "AM", am, sizeof am);
    GetProfileString("intl", "s2359", "PM", pm, sizeof pm);
    h = t.hour % 12 ? t.hour % 12 : 12;
    wsprintf(out, "%d%s%02d %s", h, (LPSTR)sep, t.minute, (LPSTR)(t.hour < 12 ? am : pm));
}

/* The time measured; TRUE when its width changed, and the buttons' room with it. */
BOOL TaskbarClockMeasure(void)
{
    char text[24];
    HDC dc = GetDC(g_taskbar);
    HFONT old = SelectObject(dc, DrawFont(FALSE));
    int was = TaskbarClockW;

    TaskbarClockText(text);
    TaskbarClockW = LOWORD(GetTextExtent(dc, text, lstrlen(text)));
    SelectObject(dc, old);
    ReleaseDC(g_taskbar, dc);
    return TaskbarClockW != was;
}

/* The tooltip of what the mouse rests on: a Quick Launch button's name,
   the title of a task button too narrow for it, the clock's date. */
void TaskbarTip(void)
{
    char text[TITLE_MAX];
    POINT p;
    RECT r;
    HDC dc;
    HFONT old;
    int width;

    *text = '\0';
    if (TaskbarHot <= QL(0)) {
        QuickLaunchName(QL(0) - TaskbarHot, text);
    } else if (TaskbarHot <= TRAYICON(TaskbarNTray) && TaskbarHot > QL(0)) {
        TrayToolsTip(TRAYICON(TaskbarNTray) - TaskbarHot, text);
    } else if (TaskbarHot <= TRAYICON(0) && TaskbarHot > QL(0)) {
        DisplayNameOfFile(PathFileName(trayPrograms[TRAYICON(0) - TaskbarHot]), text);
        if (*PathExtension(text))
            *PathExtension(text) = '\0';
    } else if (TaskbarHot == TRAY) {
        DWORD now = DosNow();
        FormatLongDateTime(HIWORD(now), LOWORD(now), FALSE, text);
    } else if (TaskbarHot >= 0 && TaskbarHot < TaskbarNTasks) {
        TaskbarButtonRect(TaskbarHot, &r);
        dc = GetDC(g_taskbar);
        old = SelectObject(dc, DrawFont(TaskbarHot == TaskbarFind(TaskbarActive)));
        width = LOWORD(GetTextExtent(dc, TaskbarTitles[TaskbarHot], lstrlen(TaskbarTitles[TaskbarHot])));
        SelectObject(dc, old);
        ReleaseDC(g_taskbar, dc);
        if (width > r.right - r.left - 25)
            lstrcpy(text, TaskbarTitles[TaskbarHot]);
    }
    if (*text) {
        GetCursorPos(&p);
        TooltipShow(text, p.x, p.y + 20);
    }
}

/* The item under the mouse: Quick Launch's raise, a tooltip later. */
void TaskbarHover(int code)
{
    BOOL ql = TaskbarHot <= QL(0) || code <= QL(0);

    if (code == TaskbarHot)
        return;
    TaskbarHot = code;
    TaskbarGroupLeft();                 /* (a group's list may open again on it) */
    TooltipHide();
    KillTimer(g_taskbar, TIMER_TIP);
    if (code != NONE) {
        SetTimer(g_taskbar, TIMER_TIP, TIP_MS, NULL);
        SetTimer(g_taskbar, TIMER_HOT, 100, NULL);
    } else {
        KillTimer(g_taskbar, TIMER_HOT);
    }
    if (ql)
        InvalidateRect(g_taskbar, NULL, FALSE);
}

/* A tray program's icon clicked: its window forward (if in front, down);
   not running, it starts. */
void TaskbarClickTray(int i)
{
    HWND w;

    if (i >= TaskbarNTray) {
        TrayToolsClick(i - TaskbarNTray);
        return;
    }
    for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT))
        if (IsWindowVisible(w) && !GetWindow(w, GW_OWNER) && TaskbarTrayProgram(w) == i)
            break;
    if (!w)
        LaunchCommand(g_taskbar, trayPrograms[i], NULL, SW_SHOWNORMAL);
    else if (w == GetActiveWindow() && !IsIconic(w))
        ShowWindow(w, SW_MINIMIZE);
    else
        TaskbarSwitchTo(w);
}

/* A double click on the clock: Date/Time Properties; FALSE elsewhere (a click). */
BOOL TaskbarDoubleClick(HWND w, int x, int y)
{
    if (TaskbarHitButton(x, y) != TRAY)
        return FALSE;
    TooltipHide();
    DateTimeShow();
    return TRUE;
}

/* The time or its form changed: the clock measured and painted again. */
void TaskbarClockChanged(HWND w)
{
    if (TaskbarShowClock) {
        TaskbarClockMeasure();
        InvalidateRect(w, NULL, FALSE);
    }
}
