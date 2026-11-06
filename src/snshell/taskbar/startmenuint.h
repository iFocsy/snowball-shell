/* What startmenu.c (the menu: its items, drawn), startmenucolumns.c (the
   two-column style) and startmenudrag.c (an item's own menu, dragging
   items, the pictures of menus closed for that) share; only they include it, after internal.h. */
#ifndef STARTMENUINT_H
#define STARTMENUINT_H

#define BANNER_W    21
#define MAX_ENTRIES 500
#define SMM_MARK    (WM_USER + 130)     /* the menu followed the mouse: mark the drop */
#define DRAG_MIN    4                   /* pixels the mouse moves before a press is a drag */
#define MAX_DEPTH   8                   /* submenus to an item, the root's included */
typedef struct {
    LPSTR label;
    LPSTR path;                 /* NULL for specials and separators */
    HMENU parent, sub;
    HICON icon;
    int   top;                  /* root items: offset of their banner slice */
    int   place;                /* StartMenuOrderPlace: 0, or where it was dragged */
    RECT  rc;                   /* where it was painted, on the screen */
    WORD  painted;              /* when: the latest painted at a point is shown there */
    BYTE  pos, root, separator, filled, special, disabled;
    BYTE  mnemonic;             /* label has an &-prefixed access key */
    BYTE  empty;                /* "(Empty)": its folder has nothing */
    BYTE  column;               /* starts a new column of its menu */
    BYTE  bar;                  /* ... with a line before it */
    BYTE  pane;                 /* two columns: in the left one (the window colours) */
    BYTE  band;                 /* two columns: a slice of the band at the top or the bottom */
    BYTE  bold, center;         /* its label */
    BYTE  fixed;                /* pinned or most used: its own menu, never dragged */
    BYTE  spacer;               /* two columns: room that ends both together, */
    int   height;               /* this high (a band's slice too) */
} ENTRY;

enum { TRACK_CLOSED, TRACK_ITEM, TRACK_DROP };
enum { BAND_NONE, BAND_HEAD, BAND_FOOT };
enum { SPECIAL_NONE, SPECIAL_RUN, SPECIAL_SHUTDOWN, SPECIAL_FIND, SPECIAL_TASKBAR, SPECIAL_FOLDEROPTS,
       SPECIAL_COMPUTER, SPECIAL_THEMES, SPECIAL_NETWORK };                /* an item that is none of the files */

typedef struct {                /* what an opening ended with */
    int      what;              /* TRACK_* */
    char     path[MAX_PATH];    /* the item right-clicked or dropped */
    char     dir[MAX_PATH];     /* dropped into this folder */
    PATHLIST order;             /* ... whose items are now in this order */
    BYTE     pos[MAX_DEPTH];    /* the menu opens again there */
    BOOL     fixed;             /* the item was pinned or most used */
    int      depth;
} OUTCOME;

extern ENTRY far *StartMenuEntries[MAX_ENTRIES];   /* this opening's items */
extern int     StartMenuCount;
extern HWND    StartMenuOwner;                 /* the taskbar */
extern WORD    StartMenuPaints;                /* items painted in this opening (the latest at a point shows there) */
extern OUTCOME StartMenuOut;

/* ---- startmenu.c ---- */
BOOL StartMenuBig(ENTRY far *e);                  /* a root item, large */
BOOL StartMenuBannered(ENTRY far *e);             /* the banner on its left */
ENTRY far *StartMenuAdd(HMENU m, LPCSTR label, LPCSTR path, BOOL root);
void StartMenuAppend(ENTRY far *e);               /* into its menu, after the others */
ENTRY far *StartMenuRootItem(HMENU m, LPCSTR label, LPCSTR path, int icon);
ENTRY far *StartMenuProgram(HMENU m, LPCSTR label, LPCSTR exe);   /* of the Windows folder */
ENTRY far *StartMenuSettingsItem(HMENU root);     /* Settings, its items in */
ENTRY far *StartMenuFindItem(HMENU root);
int  StartMenuItemHeight(ENTRY far *e);
void StartMenuBannerText(HDC dc, int x, int y, BOOL up);   /* "Windows 3.11", from x, y */

/* ---- startmenucolumns.c ---- */
HMENU StartMenuColumns(void);                     /* the two-column root menu */
int  StartMenuColumnsWidth(ENTRY far *e, int w);  /* an item's, measured w */
void StartMenuColumnsBand(HDC dc, ENTRY far *e, const RECT *r, BOOL sel);
void StartMenuColumnsGap(HDC dc, ENTRY far *e, const RECT *r);       /* the pixel before a right slice */

/* ---- startmenudrag.c ---- */
void StartMenuDragBegin(const RECT *button);
void StartMenuDragEnd(void);                      /* StartMenuOut says how the opening ended */
void StartMenuDragPaintMark(HDC dc, ENTRY far *e, DWORD org);
void StartMenuDragMark(void);                     /* SMM_MARK */
void StartMenuDragMove(HWND w);                   /* TRACK_DROP: the item to its place */
void StartMenuDragReopen(void);                   /* the menu opens again where it was */
void StartMenuGhostsGone(void);
void StartMenuPicturesGone(void);                 /* closed for good */

#endif
