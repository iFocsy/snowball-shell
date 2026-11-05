/* ui\: windows and painting that know nothing about what the shell does. */
#ifndef UI_H
#define UI_H

/* ---- paint.c ---- */
void  PaintStripImage(HDC dc, int x, int y, int strip, int index);   /* square images, magenta transparent */
void  PaintBitmap(HDC dc, int x, int y, int id);  /* a whole bitmap, magenta transparent */
HBRUSH PaintChecker(void);                        /* 50% of the DC's text and background colours */
void  PaintProgressBar(HDC dc, const RECT *r, int permille, COLORREF blocks);
#define STATUS_H        19              /* height of a status bar (the frame's face below it) */
void  PaintHeaderCell(HDC dc, const RECT *r, LPCSTR label, UINT align, BOOL pressed);
void  PaintListItem(const DRAWITEMSTRUCT FAR *d, int x, HICON icon, LPCSTR name, BOOL bold);  /* icon (if any), name */
void  PaintStatusBar(HDC dc, const RECT *c, int n, const int *widths, LPCSTR FAR *texts,
                     const HICON *icons, BOOL grip);     /* width 0: the rest; images may be NULL */
BOOL  StatusBarGrip(HWND w, LPARAM screenPoint);        /* in its status bar's size grip */

/* ---- dib.c: BMP files, read whole and checked ---- */
typedef struct {
    HGLOBAL mem;                        /* the file after its BITMAPFILEHEADER */
    int     w, h;
    DWORD   bits;                       /* where the pixels start in mem */
    int     shownW, shownH;             /* a small view: its size on the screen; else 0 */
} DIB;
BOOL  DibLoad(LPCSTR path, DIB *d);               /* FALSE: not a bitmap GDI can safely draw */
void  DibDraw(HDC dc, const DIB *d, int x, int y, int cx, int cy);   /* stretched to cx by cy */
void  DibFree(DIB *d);

/* ---- window.c ---- */
void  WindowCenter(HWND w);                       /* across, and a third down the screen */
BOOL  WindowRegister(LPCSTR name, WNDPROC proc, UINT style, int extra, HICON icon, HBRUSH background);
void  WindowRectIn(HWND w, HWND in, RECT *r);     /* w's rectangle in in's client coordinates */

/* ---- messageloop.c: the shell's one message loop ---- */
int   MessageLoopRun(void);                       /* until Windows ends */
BOOL  MessageLoopPump(HWND dlg);                  /* during long work; FALSE: Windows ends */
void  MessageLoopTrackDialog(HWND dlg, BOOL active);  /* modeless dialogs, from WM_ACTIVATE/WM_DESTROY */
BOOL  MessageLoopDialogMessage(MSG FAR *msg);     /* the active one's keys */

/* ---- drive.c ---- */
void  DriveTypeName(int drive, LPSTR out);        /* "Local Disk", "Removable Disk"... */
void  DriveName(int drive, LPSTR out);            /* "3 1/2 Floppy (A:)" with the half sign, "(C:)" */

/* ---- icons.c: icons by file, shortcut and drive, cached ---- */
HICON IconStock(int id);                          /* the shell's own (a theme's for the desktop's) */
void  IconsThemeChanged(void);                    /* the desktop's icons are read again */
HICON IconForFile(LPCSTR path, BOOL isDir);       /* never NULL */
void  IconsForgetDocuments(void);                 /* file types changed: documents' icons again */
void  IconsForget(LPCSTR key);                    /* a folder's or program's icon again */
void  IconsArtFile(LPSTR path);                   /* W98ART.DLL beside SNOWBALL.EXE */
HICON IconForShortcut(LPCSTR shcPath);
HICON IconForDrive(int drive);                    /* 0 = A: */

/* ---- iconview.c: the view of the desktop and folder windows ---- */
#define ICONVIEW_CLASS  "SbIconView"
#define IVS_DESKTOP     0x0001          /* style: columns, wallpaper, white labels */
#define IVS_FOUND       0x0002          /* style: Find's results, Details with In Folder */
/* WM_COMMAND notifications to the parent, in HIWORD(lParam) */
#define IVN_OPEN        1               /* Enter or double click */
#define IVN_CONTEXT     2               /* right click (on the selection if any) */
#define IVN_SELECT      3
#define IVN_DRAG        4               /* the selection is dragged with the left button */
#define IVN_RDRAG       5               /* ... with the right button */
#define IVN_RENAME      6               /* label edited (IconViewEditText); return TRUE if renamed */
#define IVN_SORTED      7               /* a Details column header sorted the view */
/* Modes */
#define IVM_LARGE       0
#define IVM_SMALL       1
#define IVM_LIST        2
#define IVM_DETAILS     3
#define IVM_THUMBS      4               /* Thumbnails: pictures shrunk (BMP), else large icons */
/* Sort keys, in the order of View > Arrange Icons */
#define IVSORT_NAME     0
#define IVSORT_TYPE     1
#define IVSORT_SIZE     2
#define IVSORT_DATE     3
#define IVSORT_FOLDER   4               /* In Folder (Find's results) */
/* Item kinds, in display order after sorting: */
#define IVK_FILE        0
#define IVK_FOLDER      1
#define IVK_DRIVE       2
#define IVK_COMPUTER    3
#define IVK_BIN         4
BOOL   IconViewRegister(void);
void   IconViewClear(HWND iv, BOOL keep);        /* keep: refresh, same folder */
void   IconViewSetFolder(HWND iv, LPCSTR dir);    /* the folder shown ("" none) */
LPCSTR IconViewFolder(HWND iv);
void   IconViewAdd(HWND iv, LPCSTR name, LPCSTR path, int kind, const DOSENTRY *info);
                                                  /* path: name in dir, or absolute; info may be NULL */
void   IconViewLoadFolder(HWND iv, LPCSTR dir, LPCSTR only);  /* only: files matching ("*.txt"), NULL all */
void   IconViewLoadDrives(HWND iv);
void   IconViewFinish(HWND iv);                   /* sort and lay out */
void   IconViewSetMode(HWND iv, int mode);
int    IconViewMode(HWND iv);
void   IconViewSort(HWND iv, int key, BOOL descending);
int    IconViewSortKey(HWND iv);
BOOL   IconViewSortDescending(HWND iv);
int    IconViewCount(HWND iv);
int    IconViewFind(HWND iv, LPCSTR fileName);    /* by name in the folder, -1 if none */
int    IconViewFocused(HWND iv);                  /* -1 if no item has the focus */
int    IconViewNextSelected(HWND iv, int after);  /* start with -1; -1 when done */
int    IconViewSelectedCount(HWND iv);
void   IconViewSelect(HWND iv, int item);         /* only this one, focused and visible */
void   IconViewSelectAll(HWND iv, BOOL invert);
int    IconViewSelectMatching(HWND iv, LPCSTR pattern);   /* also selects these; how many */
BOOL   IconViewItemPath(HWND iv, int item, LPSTR out);
int    IconViewItemKind(HWND iv, int item);
DWORD  IconViewItemSize(HWND iv, int item);     /* a drive's: its capacity (fixed disks) */
DWORD  IconViewItemFree(HWND iv, int item);     /* a drive's free space */
void   IconViewPin(HWND iv, int item);            /* sorts right after My Computer */
BOOL   IconViewMenuPoint(HWND iv, POINT *p);      /* IVN_CONTEXT from a key: where its menu goes */
int    IconViewHitTest(HWND iv, int x, int y);    /* client point -> item or -1 */
void   IconViewDropHighlight(HWND iv, int item);  /* -1 = none */
void   IconViewEditLabel(HWND iv, int item);      /* in-place rename */
void   IconViewEditText(HWND iv, LPSTR out, int size);

/* ---- menuitem.c: owner-drawn menu items with an icon (Start menu, Send To) ---- */
#define MENUITEM_H      20              /* an item with a small icon */
#define MIP_SELECTED    0x0001          /* MenuItemPaint flags */
#define MIP_DISABLED    0x0002
#define MIP_MNEMONIC    0x0004          /* the label has an &-prefixed access key */
#define MIP_LARGE       0x0008          /* a large icon */
#define MIP_BOLD        0x0010
#define MIP_PANE        0x0020          /* in the window colours (the two-column Start menu's left) */
#define MIP_CENTER      0x0040          /* the label alone, centred */
int    MenuItemWidth(LPCSTR label, UINT flags);
void   MenuItemPaint(HDC dc, const RECT *r, HICON icon, LPCSTR label, UINT flags);
BOOL   MenuItemMatchesKey(LPCSTR label, BOOL mnemonic, WPARAM key);   /* WM_MENUCHAR */

/* ---- autocomplete.c: paths completed as typed (address bar, Run) ---- */
void AutoCompleteAttach(HWND edit);

/* ---- slider.c: Windows 98's slider ---- */
#define SLIDER_CLASS    "SbSlider"
#define SLN_CHANGE      1               /* WM_COMMAND notification: moved */
BOOL   SliderRegister(void);
void   SliderSetCount(HWND s, int n);              /* positions 0..n-1 */
void   SliderSetPos(HWND s, int pos);
int    SliderPos(HWND s);

/* ---- colorpicker.c: Windows 98's colour button and its palette ---- */
void   ColorPickerDraw(const DRAWITEMSTRUCT FAR *d, COLORREF c);   /* a BS_OWNERDRAW button's WM_DRAWITEM */
BOOL   ColorPickerChoose(HWND button, COLORREF FAR *c);            /* the palette under it; FALSE: cancelled */

/* ---- popupmenu.c ---- */
UINT   PopupMenuTrack(HMENU m, HWND owner);       /* at the cursor; the command chosen, 0 if none */

/* ---- tooltip.c: one tooltip at a time ---- */
void   TooltipShow(LPCSTR text, int x, int y);    /* screen point: its top left */
void   TooltipHide(void);

/* ---- toolbar.c: Windows 98's flat toolbars ---- */
#define TOOLBAR_CLASS   "SbToolbar"
#define TBS_BUTTON      0
#define TBS_SEPARATOR   1
#define TBS_DROPDOWN    2               /* with an arrow of its own */
#define TBN_DROPDOWN    1               /* WM_COMMAND notification: the arrow was pressed */
BOOL   ToolbarRegister(void);
void   ToolbarSetImages(HWND tb, int strip, int cold, int grey);   /* bitmap ids; cold, grey 0: strip */
void   ToolbarAdd(HWND tb, int image, UINT cmd, LPCSTR label, UINT style);
void   ToolbarEnable(HWND tb, UINT cmd, BOOL on);
void   ToolbarCheck(HWND tb, UINT cmd, BOOL on);  /* stays sunk */
void   ToolbarShowLabels(HWND tb, BOOL on);
int    ToolbarHeight(HWND tb);
int    ToolbarWidth(HWND tb);
BOOL   ToolbarButtonRect(HWND tb, UINT cmd, RECT *r);     /* client rectangle, arrow included */

/* ---- places.c: Desktop, My Computer, drives, the folders to one, in a combo box ---- */
#define PLACE_DESKTOP   "::Desktop"     /* the Desktop, as a place */
#define PLACE_H         18              /* an item's height */
void   PlacesFill(HWND cb, LPCSTR path);          /* path selected */
void   PlacesDraw(const DRAWITEMSTRUCT FAR *d);   /* WM_DRAWITEM */
void   PlacesName(LPCSTR path, LPSTR out);        /* "Desktop", "My Computer", "(C:)", "Windows" */
HICON  PlacesIcon(LPCSTR path);

/* ---- tabs.c: the tabs and panel of property sheets ---- */
#define TABS_CLASS      "SbTabs"
#define TCN_SELECT      1               /* WM_COMMAND notification: another tab */
#define TABS_H          18              /* the strip of tabs; the panel is below */
#define TABS_PAGES      5               /* pages a sheet can have (TabsShowPage) */
BOOL   TabsRegister(void);
BOOL   SheetButton(HWND dlg, UINT id, void (*apply)(HWND dlg));  /* OK, Apply, Cancel */
void   TabsAdd(HWND tabs, LPCSTR label);
void   TabsSelect(HWND tabs, int i);
int    TabsSelected(HWND tabs);
void   TabsShowPage(HWND dlg, int page, BOOL (*wanted)(HWND dlg, int id));
                                                  /* controls IDC_PAGE1 + 50 * page..+49; wanted may be NULL */

/* ---- treeview.c: the folder tree of Explorer windows ---- */
#define TREEVIEW_CLASS  "SbTree"
/* The wheel: Windows 95's message (120 a notch), or, as VBMOUSE.DRV sends
   it, Up/Down (PageUp/PageDown) with no scan code (a key of the keyboard
   has its): it scrolls, the selection stays. */
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL   0x020A
#endif
#define IS_WHEEL_KEY(key, lp) (!(HIWORD(lp) & 0xFF) && \
    ((key) == VK_UP || (key) == VK_DOWN || (key) == VK_PRIOR || (key) == VK_NEXT))

#define TVN_SELECT      1               /* WM_COMMAND notification: a folder was chosen */
#define TVN_CONTEXT     2               /* ... a row was right-clicked: its menu (the row under the cursor) */
BOOL   TreeViewRegister(void);
void   TreeViewReset(HWND tv, LPCSTR desktopDir);
void   TreeViewResetFolder(HWND tv, LPCSTR dir, LPCSTR name, BOOL files);  /* one folder's tree */
void   TreeViewShowPath(HWND tv, LPCSTR path);    /* open the branches to it, select it */
void   TreeViewRefresh(HWND tv, LPCSTR dir);      /* dir changed on disk */
BOOL   TreeViewSelectedPath(HWND tv, LPSTR out);
int    TreeViewHitTest(HWND tv, int x, int y);
BOOL   TreeViewItemPath(HWND tv, int item, LPSTR out);
int    TreeViewItemKind(HWND tv, int item);       /* IVK_* */
void   TreeViewDropHighlight(HWND tv, int item);

#endif
