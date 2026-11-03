/* taskbar\: what taskbar.c, taskbarlayout.c and taskbartray.c share (the
   taskbar's state, its parts' functions); nothing else includes it. */
#ifndef TASKBARINT_H
#define TASKBARINT_H

#define TB_H        28                  /* one row's height at the top or bottom */
#define MAX_TASKS   40
#define TITLE_MAX   80
#define TIMER_CLOCK 1                   /* fires on the minute */
#define TIMER_SCAN  2
#define TIMER_HOT   3                   /* has the mouse left the item under it? */
#define TIMER_TIP   4                   /* the mouse rested: its tooltip */
#define TIMER_HIDE  5                   /* auto hide: is the mouse near? */
#define TIMER_DRAG  6                   /* something dragged rests on a button? */
#define TIMER_OPEN  7                   /* OPEN.COM's request from a DOS box? (each second) */
#define DRAG_MS     1000                /* ... this long: its window comes forward */
#define SCAN_MS     10000               /* catch-up only: the hooks report changes */
#define TIP_MS      600
#define NONE        (-2)                /* TaskbarHitButton: no button; -1 Start, >= 0 a task, */
#define TRAY        (-3)                /* the clock, */
#define TRAYICON(i) (-4 - (i))          /* -4..-9 a tray program's icon, */
#define MAX_TRAY    6
#define TRAYICON_W  18                  /* its 16 pixels and a gap */
#define QL(i)       (-10 - (i))         /* <= -10 a Quick Launch button */
enum { SZ_NONE, SZ_GRAB, SZ_MOVE, SZ_SIZE };

/* ---- taskbar.c: the window, its messages, the task list ---- */
extern HWND TaskbarTasks[MAX_TASKS];
extern char TaskbarTitles[MAX_TASKS][TITLE_MAX];
extern BOOL TaskbarLit[MAX_TASKS];      /* flashing: the caption looks active, the window is not */
extern int  TaskbarNTasks;
extern HWND TaskbarActive;
extern int  TaskbarPressed;             /* TaskbarHitButton's codes */
extern int  TaskbarHot;                 /* under the mouse */
extern BOOL TaskbarStartOpen, TaskbarMenuUp;
int  TaskbarFind(HWND w);                         /* its button's index, or -1 */
void TaskbarRescan(void);                         /* adds and drops buttons to match the windows */

/* ---- taskbarlayout.c: where everything is on it, and it on the screen ---- */
extern int   TaskbarSizing;             /* SZ_*: the taskbar dragged */
extern POINT TaskbarGrabbed;
BOOL TaskbarVertical(void);                       /* at a side of the screen */
void TaskbarQuickLaunchBand(RECT *b);
void TaskbarButtonRect(int i, RECT *r);           /* -1 the Start button */
void TaskbarTrayRect(RECT *r);
void TaskbarPaint(HWND w);                        /* WM_PAINT */
int  TaskbarHitButton(int x, int y);              /* NONE, -1 Start, a task, TRAY, TRAYICON(i), QL(i) */
void TaskbarAutoHide(void);                       /* TIMER_HIDE */
void TaskbarShowStart(void);                      /* the Start menu, until it closes */
BOOL TaskbarOnInnerEdge(int x, int y);
void TaskbarDragged(POINT p);
void TaskbarDragDone(void);
BOOL TaskbarCursor(HWND w, UINT hit);             /* WM_SETCURSOR */

/* ---- taskbartray.c: the tray (its programs, the clock), tooltips ---- */
extern BOOL  TaskbarShowClock;
extern BOOL  TaskbarIconsOnly;          /* [Taskbar] IconsOnly: no titles, the Start button its flag */
extern BOOL  TaskbarGrouping;           /* [Taskbar] Group: a program's windows on one button */
int  TaskbarLead(int i);                          /* the task whose button task i is on */
int  TaskbarGroupSize(int lead);                  /* the windows on that button */
int  TaskbarSlot(int i);                          /* its button's place; -1: on its lead's */
int  TaskbarSlots(void);                          /* the task buttons */
void TaskbarGroupLeft(void);                      /* the mouse left a button */
extern int   TaskbarClockW;             /* the time's width, in pixels */
extern HICON TaskbarTrayIcons[MAX_TRAY];
extern int   TaskbarNTray;
int  TaskbarTrayProgram(HWND w);                  /* the tray program a window belongs to, or -1 */
BOOL TaskbarHidden(HWND w);                       /* its program gets no button */
void TaskbarLoadPrograms(void);                   /* Taskbar Properties' Tray page */
void TaskbarClockText(LPSTR out);
BOOL TaskbarClockMeasure(void);                   /* TRUE: its width changed */
void TaskbarTip(void);                            /* TIMER_TIP */
void TaskbarHover(int code);                      /* the item under the mouse: a TaskbarHitButton code */
void TaskbarClickTray(int i);
BOOL TaskbarDoubleClick(HWND w, int x, int y);    /* the clock's; FALSE elsewhere */
void TaskbarClockChanged(HWND w);                 /* WM_WININICHANGE, WM_TIMECHANGE */

#endif
