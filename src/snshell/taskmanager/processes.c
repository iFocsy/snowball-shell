/*
 * Processes: every task (programs with or without a window, DOS sessions),
 * and on request every library and driver loaded. Memory is what the global
 * heap holds for each: a task's own blocks and those of its module (code,
 * data, resources; shared by the instances of a program). Processor time
 * comes from SNSYSTEM (cpu.c). Rows keep their place between updates; a
 * column header sorts them.
 */
#include "internal.h"
#undef MAX_PATH                         /* toolhelp.h has its own */
#include <toolhelp.h>

#define MAX_ROWS    192
#define CHECK_H     20
#define BUTTON_W    75
#define BUTTON_H    23
#define IDLE_ID     1L                  /* the idle row's id (module handles are never 1) */

enum { PK_IDLE, PK_TASK, PK_DOS, PK_LIB };

typedef struct {
    HTASK   task;
    HMODULE module;                     /* none for the idle row */
    DWORD   ms, lastMs;                 /* processor time, now and at the last update */
    DWORD   mem;
    char    name[13];                   /* "NOTEPAD.EXE" */
    BYTE    kind, cpu, seen;
} PROC;

static const COLUMN cols[] = {
    { "Image Name", 0, DT_LEFT },
    { "Task", 44, DT_RIGHT },
    { "CPU", 36, DT_RIGHT },
    { "CPU Time", 64, DT_RIGHT },
    { "Mem Usage", 72, DT_RIGHT },
};
#define NCOLS   (sizeof cols / sizeof cols[0])

static HGLOBAL mem;
static PROC FAR *rows;
static int nrows;
static int sortCol = -1;
static BOOL sortDesc;
static DWORD lastClock;

static DWORD RowId(int i)
{
    return rows[i].kind == PK_IDLE ? IDLE_ID : MAKELONG(rows[i].module, rows[i].task);
}

/* The row of this task or library, made if new (NULL if there is no room). */
static PROC FAR *Keep(int kind, HTASK task, HMODULE module)
{
    MODULEENTRY me;
    PROC FAR *p;
    int i;

    for (i = 0; i < nrows; i++)
        if (rows[i].task == task && rows[i].module == module) {
            rows[i].seen = TRUE;
            return &rows[i];
        }
    if (nrows == MAX_ROWS)
        return NULL;
    p = &rows[nrows++];
    _fmemset(p, 0, sizeof *p);
    p->task = task;
    p->module = module;
    p->kind = kind;
    p->seen = TRUE;
    p->ms = p->lastMs = kind == PK_LIB ? 0 : CpuTime(task);
    me.dwSize = sizeof me;
    if (module && ModuleFindHandle(&me, module))
        lstrcpyn(p->name, PathFileName(me.szExePath), sizeof p->name);
    return p;
}

static BOOL IsTaskModule(HMODULE module)
{
    int i;
    for (i = 0; i < nrows; i++)
        if (rows[i].kind != PK_LIB && rows[i].module == module)
            return TRUE;
    return FALSE;
}

typedef struct { WORD h; BYTE row; } OWNER;

static int CompareOwners(const void far *a, const void far *b)
{
    WORD x = ((const OWNER far *)a)->h, y = ((const OWNER far *)b)->h;
    return x < y ? -1 : x > y;
}

/* What each row's owners (its module; a task's own blocks too) hold in the
   global heap. The heap has thousands of blocks: each one's owner is looked
   up in the sorted owners, not compared with every row. */
static void Memory(void)
{
    OWNER owners[2 * MAX_ROWS];
    GLOBALENTRY ge;
    BOOL more;
    int i, n = 0, lo, hi, mid;

    for (i = 0; i < nrows; i++) {
        rows[i].mem = 0;
        if (rows[i].module) {
            owners[n].h = (WORD)rows[i].module;
            owners[n++].row = (BYTE)i;
        }
        if (rows[i].kind != PK_LIB && rows[i].task) {
            owners[n].h = (WORD)rows[i].task;
            owners[n++].row = (BYTE)i;
        }
    }
    SortRecords(owners, n, sizeof owners[0], CompareOwners);
    ge.dwSize = sizeof ge;
    for (more = GlobalFirst(&ge, GLOBAL_ALL); more; more = GlobalNext(&ge, GLOBAL_ALL)) {
        if (ge.wType == GT_FREE || !ge.hOwner)
            continue;
        for (lo = 0, hi = n; lo < hi; )         /* the first of that owner */
            if (owners[mid = (lo + hi) / 2].h < (WORD)ge.hOwner)
                lo = mid + 1;
            else
                hi = mid;
        for (; lo < n && owners[lo].h == (WORD)ge.hOwner; lo++)
            rows[owners[lo].row].mem += ge.dwBlockSize;
    }
}

static int Compare(const void far *a, const void far *b)
{
    const PROC FAR *p = a, FAR *q = b;
    int c;

    switch (sortCol) {
    case 0:  c = lstrcmpi(p->kind == PK_IDLE ? "System Idle Process" : p->name,
                          q->kind == PK_IDLE ? "System Idle Process" : q->name); break;
    case 1:  c = (WORD)p->task < (WORD)q->task ? -1 : (WORD)p->task > (WORD)q->task; break;
    case 2:  c = p->cpu < q->cpu ? -1 : p->cpu > q->cpu; break;
    case 3:  c = p->ms < q->ms ? -1 : p->ms > q->ms; break;
    default: c = p->mem < q->mem ? -1 : p->mem > q->mem; break;
    }
    return sortDesc ? -c : c;
}

/* The tasks and libraries now, their memory and processor use since the
   last update; rows that went away are dropped, new ones come last. */
static void Collect(BOOL libs)
{
    TASKENTRY te;
    MODULEENTRY me;
    PROC FAR *p;
    DWORD now = CpuClock(), span = (now - lastClock) / 1193;
    BOOL more;
    int i, n;

    for (i = 0; i < nrows; i++)
        rows[i].seen = FALSE;
    Keep(PK_IDLE, NULL, NULL);
    te.dwSize = sizeof te;
    for (more = TaskFirst(&te); more; more = TaskNext(&te))
        Keep(lstrcmp(te.szModule, "WINOLDAP") ? PK_TASK : PK_DOS, te.hTask, te.hModule);
    me.dwSize = sizeof me;
    if (libs)
        for (more = ModuleFirst(&me); more; more = ModuleNext(&me))
            if (!IsTaskModule(me.hModule))
                Keep(PK_LIB, NULL, me.hModule);
    for (i = n = 0; i < nrows; i++)
        if (rows[i].seen)
            rows[n++] = rows[i];
    nrows = n;
    Memory();
    for (i = 0; i < nrows; i++) {
        p = &rows[i];
        if (p->kind == PK_LIB)
            continue;
        p->ms = CpuTime(p->task);
        p->cpu = (BYTE)(span ? min(100, (p->ms - p->lastMs) * 100 / span) : 0);
        p->lastMs = p->ms;
    }
    lastClock = now;
    if (sortCol >= 0)
        SortRecords(rows, nrows, sizeof(PROC), Compare);
}

static PROC FAR *Selected(HWND dlg)
{
    DWORD id = ColumnListSelected(GetDlgItem(dlg, IDC_TASKMAN_PROCS));
    int i;

    for (i = 0; i < nrows; i++)
        if (RowId(i) == id)
            return &rows[i];
    return NULL;
}

static void EnableButtons(HWND dlg)
{
    PROC FAR *p = Selected(dlg);

    EnableWindow(GetDlgItem(dlg, IDC_TASKMAN_KILL),
                 p && (p->kind == PK_TASK || p->kind == PK_DOS) && p->task != GetCurrentTask());
}

BOOL ProcessesInit(void)
{
    mem = GlobalAlloc(GMEM_MOVEABLE, (DWORD)MAX_ROWS * sizeof(PROC));
    rows = mem ? (PROC FAR *)GlobalLock(mem) : NULL;
    nrows = 0;
    lastClock = CpuClock();
    return rows != NULL;
}

void ProcessesDone(void)
{
    if (mem) {
        GlobalUnlock(mem);
        GlobalFree(mem);
    }
    mem = NULL;
    rows = NULL;
}

void ProcessesLayout(HWND dlg, const RECT *page)
{
    RECT f = *page;
    int y = page->bottom - BUTTON_H;

    f.bottom = y - 8;
    ColumnListPlace(GetDlgItem(dlg, IDC_TASKMAN_PROCS), &f);
    MoveWindow(GetDlgItem(dlg, IDC_TASKMAN_LIBS), page->left, y + (BUTTON_H - CHECK_H) / 2,
               max(0, page->right - page->left - BUTTON_W - 8), CHECK_H, TRUE);
    MoveWindow(GetDlgItem(dlg, IDC_TASKMAN_KILL), page->right - BUTTON_W, y, BUTTON_W, BUTTON_H, TRUE);
}

void ProcessesPaint(HDC dc, HWND dlg)
{
    ColumnListPaint(dc, GetDlgItem(dlg, IDC_TASKMAN_PROCS), cols, NCOLS);
}

void ProcessesDraw(const DRAWITEMSTRUCT FAR *d)
{
    char task[8], cpu[8], time[16], kb[16], memory[20];
    LPCSTR cells[NCOLS];
    PROC FAR *p;
    DWORD s;

    if ((int)d->itemID < 0 || (int)d->itemID >= nrows)
        return;
    p = &rows[d->itemID];
    s = p->ms / 1000;
    wsprintf(task, "%04X", (WORD)p->task);
    wsprintf(cpu, "%02u", p->cpu);
    wsprintf(time, "%lu:%02u:%02u", s / 3600, (WORD)(s / 60 % 60), (WORD)(s % 60));
    FormatNumber((p->mem + 1023) / 1024, kb);
    wsprintf(memory, "%s K", (LPSTR)kb);
    cells[0] = p->kind == PK_IDLE ? "System Idle Process" : p->name;
    cells[1] = p->kind == PK_LIB ? "" : task;
    cells[2] = p->kind == PK_LIB ? "" : cpu;
    cells[3] = p->kind == PK_LIB ? "" : time;
    cells[4] = p->kind == PK_IDLE ? "" : memory;
    ColumnListDrawRow(d, cols, NCOLS, cells, NULL, TRUE);
}

void ProcessesRefresh(HWND dlg)
{
    Collect(IsDlgButtonChecked(dlg, IDC_TASKMAN_LIBS));
    ColumnListFill(GetDlgItem(dlg, IDC_TASKMAN_PROCS), nrows, RowId);
    EnableButtons(dlg);
}

/* Windows 2000 ends a process at once, after a warning. A DOS session's
   window is asked to close instead, as End Task does (see ApplicationsEndTask). */
static void EndProcess(HWND dlg)
{
    PROC FAR *p = Selected(dlg);
    HTASK task;
    LPCSTR title;
    HWND w;
    int i;

    if (!p)
        return;
    task = p->task;
    if (p->kind == PK_DOS) {
        for (i = 0; (w = TaskbarButtonWindow(i, &title)) != NULL && GetWindowTask(w) != task; i++)
            ;
        if (w)
            ApplicationsEndTask(dlg, w, task);
    } else if (MessageBox(dlg, "WARNING: Terminating a process can cause undesired results including "
                          "loss of data and system instability. The process will not be given the "
                          "chance to save its state or data before it is terminated. Are you sure "
                          "you want to terminate the process?", "Task Manager Warning",
                          MB_YESNO | MB_ICONEXCLAMATION) == IDYES && IsTask(task)) {
        TerminateApp(task, NO_UAE_BOX);
    }
    ProcessesRefresh(dlg);
}

void ProcessesCommand(HWND dlg, UINT id, UINT code)
{
    switch (id) {
    case IDC_TASKMAN_PROCS:
        if (code == LBN_SELCHANGE)
            EnableButtons(dlg);
        break;
    case IDC_TASKMAN_LIBS:
        SettingsSetInt("TaskManager", "Libraries", IsDlgButtonChecked(dlg, IDC_TASKMAN_LIBS));
        ProcessesRefresh(dlg);
        break;
    case IDC_TASKMAN_KILL:
        EndProcess(dlg);
        break;
    }
}

/* A click on a column header sorts by it; another click, the other way. */
void ProcessesClick(HWND dlg, int x, int y)
{
    int col = ColumnListHeaderHit(GetDlgItem(dlg, IDC_TASKMAN_PROCS), cols, NCOLS, x, y);

    if (col < 0)
        return;
    sortDesc = col == sortCol && !sortDesc;
    sortCol = col;
    SortRecords(rows, nrows, sizeof(PROC), Compare);
    ColumnListFill(GetDlgItem(dlg, IDC_TASKMAN_PROCS), nrows, RowId);
}
