/* taskmanager\: Task Manager, one window of three pages as in Windows 2000:
   Applications (the programs with a window), Processes (every task, and
   the libraries and drivers on request), Performance (processor, memory).
   taskmanager.c owns the window and calls into the pages; each page keeps
   its own state. */
#ifndef TASKMANAGER_H
#define TASKMANAGER_H

/* ---- taskmanager.c: the window, its tabs, menus and status bar ---- */
#define TMC_RUN         460             /* menu commands */
#define TMC_EXIT        461
#define TMC_ONTOP       462
#define TMC_MINUSE      463
#define TMC_REFRESH     464
#define TMC_SPEED       465             /* + 0..3: High, Normal, Low, Paused */
#define TMC_TILEH       470             /* Windows */
#define TMC_TILEV       471
#define TMC_MINIMIZE    472
#define TMC_MAXIMIZE    473
#define TMC_CASCADE     474
#define TMC_FRONT       475
#define TMC_ABOUT       476
#define TIMER_UPDATE    1
#define TIMER_END       2
void TaskManagerShow(void);
BOOL TaskManagerMinimizeOnUse(void);

/* ---- columnlist.c: lists with a column header, in a sunken frame ---- */
typedef struct {
    LPCSTR label;
    int    width;                       /* 0: what the others leave */
    UINT   align;                       /* DT_LEFT or DT_RIGHT */
} COLUMN;
void  ColumnListPlace(HWND list, const RECT *frame);  /* the list box, under the header */
void  ColumnListPaint(HDC dc, HWND list, const COLUMN *cols, int n);  /* the frame and header */
int   ColumnListHeaderHit(HWND list, const COLUMN *cols, int n, int x, int y);  /* -1: none */
void  ColumnListDrawRow(const DRAWITEMSTRUCT FAR *d, const COLUMN *cols, int n, LPCSTR *cells,
                        HICON icon, BOOL fullRow);    /* not fullRow: only the first cell is selected */
void  ColumnListFill(HWND list, int n, DWORD (*id)(int row));  /* rows by id; the selected one stays */
DWORD ColumnListSelected(HWND list);              /* the selected row's id, 0 if none */

/* ---- applications.c ---- */
void ApplicationsLayout(HWND dlg, const RECT *page);
void ApplicationsPaint(HDC dc, HWND dlg);
void ApplicationsDraw(const DRAWITEMSTRUCT FAR *d);
void ApplicationsRefresh(HWND dlg);
void ApplicationsCommand(HWND dlg, UINT id, UINT code);  /* its buttons, its list, the Windows menu */
void ApplicationsEndTask(HWND dlg, HWND w, HTASK task);  /* asked to close, then perhaps forced */
void ApplicationsWaitEnd(HWND dlg);               /* TIMER_END */
void ApplicationsDone(void);

/* ---- processes.c ---- */
BOOL ProcessesInit(void);
void ProcessesDone(void);
void ProcessesLayout(HWND dlg, const RECT *page);
void ProcessesPaint(HDC dc, HWND dlg);
void ProcessesDraw(const DRAWITEMSTRUCT FAR *d);
void ProcessesRefresh(HWND dlg);
void ProcessesCommand(HWND dlg, UINT id, UINT code);
void ProcessesClick(HWND dlg, int x, int y);      /* a column header sorts */

/* ---- performance.c ---- */
BOOL  PerformanceInit(void);
void  PerformanceDone(void);
void  PerformanceLayout(const RECT *page);
void  PerformancePaint(HDC dc);
void  PerformanceSample(void);                    /* each update, whatever the page */
int   PerformanceCpu(void);                       /* percent of the last update */
DWORD PerformanceMemoryUsed(void);                /* physical memory, bytes */
DWORD PerformanceMemoryTotal(void);

#endif
