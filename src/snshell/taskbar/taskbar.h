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
void   TaskbarResourceMeter(void);                /* System Tools' Resource Meter: in the tray */
#define MAX_TRAY        6               /* [Tray]: programs shown by the clock */
#define MAX_HIDDEN      16              /* [Hidden]: programs with no button */
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
void StartMenuBannerColors(COLORREF *top, COLORREF *bottom, COLORREF *text);   /* the scheme's */

/* ---- startmenucolumns.c: the two-column Start menu's size ---- */
#define PANE_W_MIN      160             /* [Taskbar] MenuWidth: its left pane's least width, */
#define PANE_W_MAX      340             /* in steps */
#define PANE_W_STEP     10
int  StartMenuColumnsPrograms(void);              /* [Taskbar] MostUsed: how many of them it lists */
int  StartMenuColumnsPaneWidth(void);

/* ---- startmenuorder.c: the order the Start menu's items were dragged into ---- */
int  StartMenuOrderPlace(LPCSTR path);            /* 1, 2...; 0: none, the usual order */
void StartMenuOrderSet(LPCSTR dir, PATHLIST *names);  /* file names in their order */
void StartMenuOrderRename(LPCSTR from, LPCSTR to);   /* renamed or moved */
void StartMenuOrderForget(LPCSTR dir);            /* Sort by Name */

/* ---- programmanager.c: Program Manager's DDE, its items as shortcuts ---- */
void ProgramManagerFolder(LPSTR out);             /* ...\Start Menu\Programs */
void ProgramManagerItemName(LPCSTR dir, LPCSTR title, LPCSTR ext, int nth, LPSTR out);  /* nth 0: not taken */
void ProgramManagerItemNameAs(LPCSTR dir, LPCSTR title, LPCSTR ext, int nth, BOOL longNames, LPSTR out);
BOOL ProgramManagerWriteItem(LPCSTR dir, LPCSTR title, LPCSTR cmd, LPCSTR workDir,
                             LPCSTR icon, int iconIndex, BOOL minimized, int nth);
BOOL ProgramManagerAddItem(LPCSTR dir, LPCSTR title, LPCSTR cmd);  /* a shortcut, its name unique */
BOOL ProgramManagerServe(void);                   /* answer setup programs' DDE */

/* ---- programgroups.c: Program Manager's .GRP files as Start menu folders ---- */
BOOL ProgramManagerAddExplorer(LPCSTR dir);       /* "Windows Explorer" in dir */
void ProgramManagerConvertGroups(void);           /* at start: a new Start menu's groups */
void ProgramManagerSync(HWND owner);              /* groups new or changed since, then says so */
BOOL ProgramManagerIsGroupFile(LPCSTR path);      /* a .GRP Program Manager wrote */
BOOL ProgramManagerImportGroup(HWND owner, LPCSTR path);  /* asked first */

/* ---- frequent.c: the two-column Start menu's pinned and most used ---- */
#define FREQUENT_MAX    30              /* most used programs counted */
void FrequentPinned(PATHLIST *l);
BOOL FrequentIsPinned(LPCSTR path);
void FrequentPin(LPCSTR path, BOOL on);
void FrequentCount(LPCSTR path);                  /* a program started (LaunchItem) */
void FrequentMostUsed(PATHLIST *l, int n);
BOOL FrequentAnyUsed(void);
void FrequentClear(void);                         /* the most used forgotten (Clear List) */
void FrequentMenu(HWND owner, LPCSTR path);       /* such an item's right click */

#endif
