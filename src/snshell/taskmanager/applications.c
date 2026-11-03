/*
 * Applications: the programs with a window, as the taskbar knows them (no
 * title is asked of them). Switch To, New Task, and End Task: the program
 * is asked to close, and if it has not after a while, Windows 98's question
 * whether to end it anyway.
 */
#include "internal.h"
#undef MAX_PATH                         /* toolhelp.h has its own */
#include <toolhelp.h>

#define BUTTON_W    75
#define BUTTON_H    23
#define END_WAIT    5000                /* ms a program has to close */

static const COLUMN cols[] = {
    { "Task", 0, DT_LEFT },
    { "Status", 80, DT_LEFT },
};
static HWND manager;
static DWORD ending;                    /* the one asked to end: MAKELONG(window or 0, task) */
static DWORD endAsked;

/* The row-th program's window (Task Manager itself is not listed). */
static HWND App(int row, LPCSTR *title)
{
    HWND w;
    int i;

    for (i = 0; (w = TaskbarButtonWindow(i, title)) != NULL; i++)
        if (w != manager && row-- == 0)
            return w;
    return NULL;
}

static DWORD AppId(int row)
{
    LPCSTR title;
    HWND w = App(row, &title);
    return MAKELONG(w, GetWindowTask(w));
}

static HWND Selected(HWND dlg)
{
    return (HWND)LOWORD(ColumnListSelected(GetDlgItem(dlg, IDC_TASKMAN_LIST)));
}

static void EnableButtons(HWND dlg)
{
    BOOL any = Selected(dlg) != NULL;

    EnableWindow(GetDlgItem(dlg, IDC_TASKMAN_SWITCH), any);
    EnableWindow(GetDlgItem(dlg, IDC_TASKMAN_END), any);
}

void ApplicationsLayout(HWND dlg, const RECT *page)
{
    RECT f = *page;
    int x = page->right - 3 * BUTTON_W - 2 * 6, y = page->bottom - BUTTON_H;

    f.bottom = y - 8;
    ColumnListPlace(GetDlgItem(dlg, IDC_TASKMAN_LIST), &f);
    MoveWindow(GetDlgItem(dlg, IDC_TASKMAN_END), x, y, BUTTON_W, BUTTON_H, TRUE);
    MoveWindow(GetDlgItem(dlg, IDC_TASKMAN_SWITCH), x + BUTTON_W + 6, y, BUTTON_W, BUTTON_H, TRUE);
    MoveWindow(GetDlgItem(dlg, IDC_TASKMAN_NEW), x + 2 * (BUTTON_W + 6), y, BUTTON_W, BUTTON_H, TRUE);
}

void ApplicationsPaint(HDC dc, HWND dlg)
{
    ColumnListPaint(dc, GetDlgItem(dlg, IDC_TASKMAN_LIST), cols, 2);
}

void ApplicationsDraw(const DRAWITEMSTRUCT FAR *d)
{
    HWND w = (HWND)LOWORD(d->itemData);
    LPCSTR cells[2];

    cells[0] = TaskbarTitle(w);
    cells[1] = "Running";
    ColumnListDrawRow(d, cols, 2, cells, TaskbarIcon(w), FALSE);
}

void ApplicationsRefresh(HWND dlg)
{
    LPCSTR title;
    int n;

    manager = dlg;
    for (n = 0; App(n, &title); n++)
        ;
    ColumnListFill(GetDlgItem(dlg, IDC_TASKMAN_LIST), n, AppId);
    EnableButtons(dlg);
}

static BOOL Alive(DWORD id)
{
    HWND w = (HWND)LOWORD(id);
    HTASK task = (HTASK)HIWORD(id);

    return w ? IsWindow(w) && GetWindowTask(w) == task : IsTask(task);
}

static BOOL IsDosSession(HTASK task)
{
    TASKENTRY te;

    te.dwSize = sizeof te;
    return TaskFindHandle(&te, task) && !lstrcmp(te.szModule, "WINOLDAP");
}

/* Asks politely: a window is closed, a task without one told to quit.
   Windows 3.1 ends a DOS session only from its own window (Exit, or
   Settings > Terminate); asked to close, the window says so itself, as
   for Program Manager's Task List. So a DOS session is never forced. */
void ApplicationsEndTask(HWND dlg, HWND w, HTASK task)
{
    if (w)
        PostMessage(w, WM_CLOSE, 0, 0L);
    else
        PostAppMessage(task, WM_QUIT, 0, 0L);
    if (IsDosSession(task))
        return;
    ending = MAKELONG(w, task);
    endAsked = GetTickCount();
    SetTimer(dlg, TIMER_END, 250, NULL);
}

BOOL CALLBACK __export ApplicationsEndTaskProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        SkinDialog(dlg);
        SetWindowText(dlg, (LPCSTR)lp);
        WindowCenter(dlg);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK || wp == IDCANCEL)
            EndDialog(dlg, wp);
        return TRUE;
    }
    return FALSE;
}

/* The program asked to end is gone, or after END_WAIT may be forced to. */
void ApplicationsWaitEnd(HWND dlg)
{
    char title[96];

    if (Alive(ending) && GetTickCount() - endAsked < END_WAIT)
        return;
    KillTimer(dlg, TIMER_END);
    if (Alive(ending)) {
        lstrcpyn(title, TaskbarTitle((HWND)LOWORD(ending)), sizeof title);
        if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_ENDTASK), dlg, ApplicationsEndTaskProc,
                           (LPARAM)(LPSTR)title) == IDOK && Alive(ending))
            TerminateApp((HTASK)HIWORD(ending), NO_UAE_BOX);
    }
    ending = 0;
    ApplicationsRefresh(dlg);
}

void ApplicationsCommand(HWND dlg, UINT id, UINT code)
{
    HWND w = Selected(dlg);

    switch (id) {
    case IDC_TASKMAN_LIST:
        if (code == LBN_SELCHANGE)
            EnableButtons(dlg);
        else if (code == LBN_DBLCLK)
            ApplicationsCommand(dlg, IDC_TASKMAN_SWITCH, 0);
        break;
    case IDC_TASKMAN_SWITCH:
        if (w && IsWindow(w)) {
            TaskbarSwitchTo(w);
            if (TaskManagerMinimizeOnUse())
                ShowWindow(dlg, SW_MINIMIZE);
        }
        break;
    case IDC_TASKMAN_END:
        if (w)
            ApplicationsEndTask(dlg, w, GetWindowTask(w));
        break;
    case IDC_TASKMAN_NEW:
        RunDialogShow(dlg);
        break;
    case TMC_MINIMIZE:
    case TMC_MAXIMIZE:
        if (w && IsWindow(w))
            ShowWindow(w, id == TMC_MINIMIZE ? SW_MINIMIZE : SW_SHOWMAXIMIZED);
        break;
    case TMC_FRONT:                     /* in front, Task Manager still active */
        if (w && IsWindow(w)) {
            if (IsIconic(w))
                ShowWindow(w, SW_SHOWNOACTIVATE);
            SetWindowPos(w, dlg, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        break;
    }
}

void ApplicationsDone(void)
{
    manager = NULL;
    ending = 0;
}
