/*
 * Inside SbIconView: what its three parts share - iconview.c (items,
 * sorting, selection, the window procedure), iconpaint.c (geometry and
 * painting) and iconinput.c (mouse, keyboard, infotips, rename). Only
 * they include it, after internal.h.
 */
#ifndef ICONVIEWINT_H
#define ICONVIEWINT_H

#define CELL_W      75          /* Large Icons */
#define CELL_H      70
#define THUMB_W     100         /* Thumbnails: a cell, */
#define THUMB_H     110
#define THUMB       80          /* ... its picture's box */
#define TILE_W      250         /* ... of My Computer's drives: tiles with how full each is */
#define TILE_H      56
#define BAR_W       180
#define LABEL_W     70
#define ROW_H       18          /* Small Icons, List, Details */
#define SMALL_W     150
#define LIST_MAX    250         /* widest List column */
#define HEAD_H      18          /* Details header */
#define MAX_COLS    5
#define GRIP        3           /* pixels each side of a column edge that resize it */
#define GROW        64
#define DRAG_MIN    4           /* pixels the mouse moves before a drag starts */
#define TIMER_EDIT  1           /* slow second click -> rename */
#define TIMER_TIP   2           /* the mouse rested on a folder: its infotip */
#define TIMER_POINT 3           /* single-click: pointed at long enough, selected */
#define POINT_MS    500
#define TIP_MS      600
#define MSG_ENDEDIT (WM_USER + 1)   /* wParam: keep the new name */
#define VK_APPS     0x5D        /* the Menu key (Windows 95's keyboard) */

#define IF_SHORTCUT 0x01
#define IF_SELECTED 0x02
#define IF_WAS      0x04        /* selected when the rubber band started */
#define IF_FOCUS    0x08        /* follows the focused item through a sort */
#define IF_PINNED   0x10        /* right after My Computer (the desktop's My Documents) */
#define IF_NOTHUMB  0x20        /* Thumbnails: it has no picture */

typedef struct {
    WORD  name;                 /* display name, offset in the string pool */
    WORD  file;                 /* name in dir, or an absolute path */
    HICON icon;
    BYTE  kind;
    BYTE  flags;
    DWORD size;                 /* a drive's: its capacity */
    DWORD free;                 /* a drive's free space */
    HBITMAP thumb;              /* Thumbnails: a BMP file's, made when first shown */
    WORD  date, time;
    WORD  textW;                /* width of the name in pixels */
} ITEM;
#define MAX_ITEMS   ((int)(0xFFF0U / sizeof(ITEM)) / GROW * GROW)   /* the items' block stays under 64 KB */

typedef struct {
    HGLOBAL  hItems, hPool;
    ITEM far *it;
    LPSTR    pool;
    int      count, cap;
    WORD     used, poolCap;
    int      focus, anchor, drop, cols, rows;
    long     scroll, hscroll;           /* pixels: a big folder goes past 32767 */
    long     vmax, hmax;                /* how far each goes; the scroll bars show them scaled */
    int      mode, sortKey, cellW;
    BOOL     sortDesc;
    int      ncols, colW[MAX_COLS];
    BYTE     colKey[MAX_COLS];      /* IVS_* of each Details column */
    int      headDown, sizing, sizeX;   /* Details header: pressed column, resized column */
    int      pressed;           /* item the held button went down on */
    UINT     button;            /* WM_LBUTTONDOWN or WM_RBUTTONDOWN while held */
    POINT    down;
    BOOL     band, slowClick;
    BOOL     keyMenu;           /* the context menu asked for with a key: at the focused item */
    int      hover;             /* the folder the mouse rests on (its infotip), -1 none */
    BOOL     tipUp;
    POINT    moved;             /* where the mouse last really moved (screen) */
    int      hot;               /* single-click: the item pointed at, underlined; -1 none */
    RECT     bandRect;
    HWND     edit;
    PATHLIST keep;              /* names selected before a refresh */
    char     dir[MAX_PATH];
} VIEW;

#define View(w)     ((VIEW *)LocalHeapPointer(GetWindowWord(w, 0)))
#define Sel(v, i)   ((v)->it[i].flags & IF_SELECTED)

/* ---- iconview.c ---- */
void IconViewNotify(HWND w, int code);            /* WM_COMMAND to the parent: IVN_* */
BOOL IconViewIsDesktop(HWND w);
void IconViewSetSel(HWND w, int i, BOOL on);      /* repaints it if it changes */
void IconViewSetFocusItem(HWND w, int i);         /* scrolled into view */
void IconViewSelectRange(HWND w, int from, int to, BOOL only);   /* from..to; only: others off */

/* ---- iconpaint.c ---- */
void IconViewLayout(HWND w);                      /* rows, columns, scroll bars */
void IconViewCell(HWND w, int i, long *x, long *y, int *cx, int *cy);  /* in the whole view, not scrolled */
void IconViewCellRect(HWND w, int i, RECT *r);
void IconViewPartRects(HWND w, int i, RECT *icon, RECT *label);  /* the parts the mouse hits */
void IconViewPaint(HWND w);                       /* WM_PAINT */
void IconViewMeasure(HWND w);                     /* name widths, the List column width */
void IconViewScroll(HWND w, BOOL horizontal, int code, int pos); /* WM_VSCROLL, WM_HSCROLL */

/* ---- iconinput.c ---- */
void IconViewDropEdit(HWND w);                    /* closes the label editor */
void IconViewEndEdit(HWND w, BOOL accept);        /* MSG_ENDEDIT */
void IconViewButtonDown(HWND w, UINT msg, WPARAM keys, int x, int y);   /* WM_[LR]BUTTONDOWN */
void IconViewButtonUp(HWND w, UINT msg, WPARAM keys, int x, int y);     /* WM_[LR]BUTTONUP */
void IconViewTipOff(HWND w);                      /* the infotip goes */
void IconViewMouseMove(HWND w, WPARAM keys, int x, int y);   /* WM_MOUSEMOVE */
void IconViewKey(HWND w, WPARAM key);             /* WM_KEYDOWN */
void IconViewTypeSelect(HWND w, WPARAM ch);       /* WM_CHAR */
BOOL IconViewCursor(HWND w, UINT hit);            /* WM_SETCURSOR; TRUE if set */
void IconViewCancel(HWND w);                      /* WM_CANCELMODE */
void IconViewTimer(HWND w, WPARAM id);            /* WM_TIMER: the infotip's, the slow click's */
BOOL IconViewSysKey(HWND w, WPARAM key, LPARAM lp);  /* WM_SYSKEYDOWN; FALSE: as usual */

#endif
