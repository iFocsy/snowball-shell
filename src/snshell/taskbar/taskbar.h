/* taskbar\: the taskbar and what is on it or opens from it. */
#ifndef TASKBAR_H
#define TASKBAR_H

/* ---- taskbar.c ---- */
extern HWND g_taskbar;
BOOL   TaskbarCreate(void);
#define TBE_BOTTOM      0               /* TaskbarEdge: the side of the screen it is at */
#define TBE_TOP         1
#define TBE_LEFT        2
#define TBE_RIGHT       3
int    TaskbarEdge(void);
void   TaskbarWorkArea(RECT *r);                  /* the screen less the taskbar's part */
HWND   TaskbarButtonWindow(int i, LPCSTR *title);    /* the i-th button's window; NULL past the last */
HICON  TaskbarIcon(HWND w);
LPCSTR TaskbarTitle(HWND w);                      /* cached; "" if it has no button */
BOOL   TaskbarHasButton(HWND w);
void   TaskbarSwitchTo(HWND w);
HWND   TaskbarTaskAt(POINT screen);               /* the window of the task button there, or NULL */
void   TaskbarDragOver(const POINT FAR *screen);  /* a drag is there (NULL: ended): resting on a
                                                     button brings its window forward */
void   TaskbarDropOnTask(HWND owner, HWND task, HDROP drop);  /* NULL drop: only says it cannot */
void   TaskbarApply(void);                        /* after Taskbar Properties */
#define ARR_CASCADE     1               /* TaskbarArrange: the windows not minimized */
#define ARR_TILEH       2
#define ARR_TILEV       3
#define ARR_MINALL      4
void   TaskbarArrange(UINT how);

/* ---- quicklaunch.c: Quick Launch, by the Start button ---- */
void QuickLaunchLoad(void);                       /* (again) from its folder */
int  QuickLaunchLength(BOOL down, int width);     /* along the taskbar, grip and all; 0 when not
                                                     shown. down: rows of buttons that wide */
void QuickLaunchPlace(const RECT *band, BOOL down);
int  QuickLaunchHitTest(int x, int y);            /* the button there, or -1 */
BOOL QuickLaunchHas(int x, int y);                /* the point is in its band */
void QuickLaunchPaintGrip(HDC dc, const RECT *band, BOOL down);  /* any band's */
void QuickLaunchPaint(HDC dc, int hot, int pressed);
void QuickLaunchName(int i, LPSTR out);
void QuickLaunchOpen(HWND owner, int i);
void QuickLaunchMenu(HWND owner, int i);
#define QLC_OPEN        0x0E00          /* its menu's commands, to QuickLaunchCommand */
#define QLC_DELETE      0x0E01
#define QLC_PROPS       0x0E02
void QuickLaunchCommand(HWND owner, UINT cmd);
void QuickLaunchAddFiles(HDROP drop);
#define SD_TOGGLE       0               /* QuickLaunchShowDesktop: Show Desktop's button, Win+D */
#define SD_MINIMIZE     1               /* Win+M */
#define SD_RESTORE      2               /* Win+Shift+M */
void QuickLaunchShowDesktop(int how);

/* ---- switcher.c: Alt+Tab ---- */
void SwitcherCommand(UINT what);                  /* SBM_SWITCH */

/* ---- startmenu.c ---- */
void StartMenuShow(HWND owner, const RECT *button);   /* above the taskbar, by the Start button */
BOOL StartMenuMessage(HWND w, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res);
void StartMenuFolder(LPSTR out);                  /* ...\Start Menu */

/* ---- startmenuorder.c: the order the Start menu's items were dragged into ---- */
int  StartMenuOrderPlace(LPCSTR path);            /* 1, 2...; 0: none, the usual order */
void StartMenuOrderSet(LPCSTR dir, PATHLIST *names);  /* file names in their order */
void StartMenuOrderRename(LPCSTR from, LPCSTR to);   /* renamed or moved */
void StartMenuOrderForget(LPCSTR dir);            /* Sort by Name */

/* ---- programmanager.c: Program Manager's groups and DDE ---- */
BOOL ProgramManagerAddItem(LPCSTR dir, LPCSTR title, LPCSTR cmd);  /* a shortcut, its name unique */
BOOL ProgramManagerAddExplorer(LPCSTR dir);       /* "Windows Explorer" in dir */
void ProgramManagerConvertGroups(void);           /* first run only */
BOOL ProgramManagerServe(void);                   /* answer setup programs' DDE */

#endif
