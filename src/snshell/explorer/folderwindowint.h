/* What folderwindow.c, folderbands.c (Standard Buttons, the address bar)
   and explorerbar.c (the folder tree on the left) share; only they include
   it, after internal.h. */
#ifndef FOLDERWINDOWINT_H
#define FOLDERWINDOWINT_H

#define ID_TREE         2
#define ID_TOOLS        3
#define ID_ADDRESS      4
#define BAR_H           18              /* "All Folders" above the tree */
#define SPLIT_W         4
#define SIZE_W          110             /* the status bar's size panel */
#define DRIVE_W         110             /* ... and its drive's free space */
#define GRIP_W          10              /* a band's grip, before what it holds */
#define BAND_GAP        2               /* above and below what a band holds */
#define LIST_H          200             /* the address list, dropped down */
#define COMPACT_MIN     120             /* Compact: the address field's least width */
#define HISTORY         16              /* folders Back and Forward remember */
#define SECTION         "Folders"
#define CLOSE_W         14              /* the tree's close button, on its title */
#define FWM_PLACE       (WM_USER + 300) /* wParam: the place chosen in the address list */
#define FWM_GO          (WM_USER + 301) /* Enter in the address bar */

typedef struct {
    int  count, at;                     /* at: the folder shown */
    char path[HISTORY][MAX_PATH];
} TRAIL;

typedef struct {
    HWND    view, tree, tools, address; /* tree: Explorer windows only */
    int     split;                      /* width of the tree pane */
    BOOL    splitting;
    DWORD   free;                       /* bytes free on the folder's disk */
    char    filter[16];                 /* View > Filter: only these files ("*.txt"), "" all */
    HICON   icon;                       /* the folder's, in the address bar */
    HGLOBAL hTrail;
    TRAIL  *trail;
    char    path[MAX_PATH];
} FOLDER;

#define IsBin(path) (!lstrcmp(path, BIN_PATH))
#define Folder(w)   ((FOLDER *)LocalHeapPointer(GetWindowWord(w, 0)))

/* ---- folderwindow.c ---- */
void  FolderWindowArrange(HWND w);                /* its parts placed again */
void  FolderWindowSetTitle(HWND w);
void  FolderWindowNavigate(HWND w, LPCSTR path);  /* shown, and kept in the history */

/* ---- folderbands.c ---- */
HMENU FolderBandsMenuBar(BOOL bin);
BOOL  FolderBandsMake(HWND w);
int   FolderBandsHeight(HWND w);                  /* 0: no band shown */
void  FolderBandsArrange(HWND w, const RECT *c);
void  FolderBandsPaint(HWND w, HDC dc, const RECT *c);
void  FolderBandsTools(HWND w);                   /* what the buttons can do now */
void  FolderBandsAddress(HWND w);                 /* the folder shown, in the address bar */
void  FolderBandsToggle(HWND w, UINT cmd);        /* View > Toolbars */
void  FolderBandsViews(HWND w, BOOL list);        /* the Views button: the next one, or the list */
void  FolderBandsInitMenu(HMENU m);
BOOL  FolderBandsKeyClosed(void);                 /* the address list was closed by Enter or Esc */

/* ---- explorerbar.c ---- */
BOOL  ExplorerBarShow(HWND w, BOOL on);           /* View > Explorer Bar > Folders, or None */
void  ExplorerBarPaint(HWND w, HDC dc, const RECT *c, int top);
BOOL  ExplorerBarCursor(HWND w, HWND over, UINT hit);   /* WM_SETCURSOR; TRUE if set */
void  ExplorerBarButtonDown(HWND w, int x, int y);
void  ExplorerBarMouseMove(HWND w, int x);
void  ExplorerBarButtonUp(HWND w);
void  ExplorerBarNotify(HWND w, UINT code);       /* the tree's WM_COMMAND */

#endif
