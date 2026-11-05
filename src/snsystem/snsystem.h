/*
 * SNSYSTEM.DLL, the part of Snowball at work in every program:
 *   draw.c         drawing primitives, shared with the shell
 *   colors.c       the Windows 98 colours Windows 3.1 has no index for
 *   skin.c         the Windows 98 look of every program's windows and dialogs
 *                  (skinctl.c their controls, skinmenu.c popup menus)
 *   hooks.c        system hooks: taskbar events, Alt+Tab, maximized windows
 *   cpu.c          processor time of Windows and of each task
 *   filedialogs.c  every program's Open and Save As, shown by the shell
 *   snsystem.c     the library's entry and exit
 */
#ifndef SNSYSTEM_H
#define SNSYSTEM_H

#include <windows.h>
#include <string.h>
#include <commdlg.h>
#include "snowball.h"

#ifdef SNSYSTEM_DLL
#define SNSYSTEM_API FAR PASCAL __export
#else
#define SNSYSTEM_API FAR PASCAL
#endif

/* ---- draw.c ---- */
HFONT SNSYSTEM_API DrawFont(BOOL bold);
void  SNSYSTEM_API DrawSolidRect(HDC dc, const RECT FAR *r, COLORREF c);
void  SNSYSTEM_API DrawGradient(HDC dc, const RECT FAR *r, COLORREF from, COLORREF to, BOOL vertical);
void  SNSYSTEM_API DrawFrame(HDC dc, const RECT FAR *r, COLORREF topLeft, COLORREF bottomRight);
void  SNSYSTEM_API DrawBevel(HDC dc, const RECT FAR *r, BOOL sunken);
int   SNSYSTEM_API DrawScreenDepth(void);                 /* bits per pixel */
void  SNSYSTEM_API DrawPreviewColors(const COLORREF FAR *sys, const COLORREF FAR *sc);  /* NULL, NULL: Windows' */
void  SNSYSTEM_API DrawGroupFrame(HDC dc, const RECT FAR *r, int gap, RECT FAR *caption);
void  SNSYSTEM_API DrawTextEllipsis(HDC dc, LPCSTR text, RECT FAR *r, UINT format);
void  SNSYSTEM_API DrawWallpaper(HDC dc);                 /* for windows placed at (0,0) */
/* Drawing off the screen: Begin gives a DC that draws, in dc's coordinates,
   into a bitmap of r (or dc itself, with no memory for one); End puts it on
   dc at once, so the screen never shows half a picture. */
typedef struct { HDC dc, mem; HBITMAP bmp, old; RECT r; } OFFSCREEN;
HDC   SNSYSTEM_API DrawOffscreenBegin(OFFSCREEN FAR *o, HDC dc, const RECT FAR *r);
void  SNSYSTEM_API DrawOffscreenEnd(OFFSCREEN FAR *o);
void  SNSYSTEM_API DrawIconImage(HDC dc, int x, int y, HICON icon, BOOL small);
HDC   SNSYSTEM_API DrawMemoryDC(void);                    /* shared: select a bitmap in, then the old one back */
BOOL  SNSYSTEM_API DrawIconImages(HICON icon, HINSTANCE module, LPCSTR group);
void  SNSYSTEM_API DrawSetAnimations(BOOL on);           /* Display Properties' Effects */
BOOL  SNSYSTEM_API DrawAnimations(void);
void  SNSYSTEM_API DrawWait(DWORD ticks);                /* of CpuClock: a step of an animation */
void  SNSYSTEM_API DrawZoom(const RECT FAR *from, const RECT FAR *to);   /* (screen) when animating */
                                                          /* the images of its resource for each size */

/* ---- skin.c ---- */
void  SNSYSTEM_API SkinDialog(HWND dlg);
void  SNSYSTEM_API SkinFrame(HWND w);
void  SNSYSTEM_API SkinControl(HWND c);                   /* one control of the shell's own windows */
void  SNSYSTEM_API SkinEdges(HWND dlg);                   /* its lists' edges again (after a tab panel) */
void  SNSYSTEM_API SkinCheckBox(HDC dc, int x, int y, BOOL radio, int state, BOOL flat);
                                                          /* 13x13 box or 12x12 radio at x, y */
#define SKIN_CLOSE      101                               /* caption buttons */
#define SKIN_MAX        102
#define SKIN_MIN        103
#define SKIN_RESTORE    202
void  SNSYSTEM_API SkinCaptionButton(HDC dc, const RECT FAR *r, int which);
void  SNSYSTEM_API SkinMenuPictures(const RECT FAR *r, const HBITMAP FAR *b, int n);  /* menus shown again: their erase */

/* ---- colors.c: WIN.INI [colors]' Windows 98 keys, or made from the scheme ---- */
#define SC_GRADIENT_ACTIVE      0                         /* where the active caption's gradient goes */
#define SC_GRADIENT_INACTIVE    1
#define SC_INFO                 2                         /* tooltips */
#define SC_INFOTEXT             3
#define SC_DKSHADOW             4                         /* the dark outer shadow of edges */
#define SC_HOTTRACK             5                         /* names under the pointer */
#define SC_COUNT                6
COLORREF SNSYSTEM_API SkinColor(int sc);
COLORREF SNSYSTEM_API SkinColorMade(int sc, COLORREF base); /* without its key, from its base's colour */
int      SNSYSTEM_API SkinColorBase(int sc);              /* the COLOR_* it is made from */
LPCSTR   SNSYSTEM_API SkinColorKey(int sc);               /* its WIN.INI [colors] key */
void     SNSYSTEM_API SkinColorsChanged(void);            /* read them again */

/* ---- hooks.c ---- */
BOOL  SNSYSTEM_API HookInstall(HWND notify, BOOL skinPrograms);
void  SNSYSTEM_API HookRemove(void);
BOOL  SNSYSTEM_API HookLost(void);                        /* an event was lost (full queue); clears */
void  SNSYSTEM_API HookTitleSeen(void);                   /* the posted title change was handled */
void  SNSYSTEM_API HookWorkArea(const RECT FAR *r);         /* what maximized windows fill */
void  SNSYSTEM_API HookHotKeys(const WORD FAR *keys, int n);  /* shortcut keys: SBM_HOTKEY */
void  SNSYSTEM_API HookTaskButtons(const HWND FAR *w, const RECT FAR *r, int n);  /* (screen) */
void  SNSYSTEM_API HookZoom(HWND w, UINT cmd);           /* before SC_MINIMIZE, SC_RESTORE, SC_MAXIMIZE */
void  SNSYSTEM_API HookSkinPrograms(BOOL on);            /* the Windows 98 style for every program */
void  SNSYSTEM_API HookSkinShell(BOOL on);               /* ... for the shell's own windows and menus */
void  SNSYSTEM_API SkinScrollArrows(HWND w);             /* SBM_ARROWS: Windows 98's over Windows 3.1's */
BOOL  SNSYSTEM_API SkinWanted(HWND w);                   /* w is to have the Windows 98 style */

/* ---- cpu.c ---- */
BOOL  SNSYSTEM_API CpuStart(void);                        /* measuring from now on */
void  SNSYSTEM_API CpuStop(void);
DWORD SNSYSTEM_API CpuClock(void);                        /* 1193182 Hz ticks; wraps hourly */
DWORD SNSYSTEM_API CpuIdle(void);                         /* ticks spent idle, same clock */
DWORD SNSYSTEM_API CpuTime(HTASK task);                   /* ms of processor used; NULL: idle */

/* ---- cursors.c: the system's mouse pointers from a theme ---- */
#define CURSOR_ARROW    0               /* Normal Select */
#define CURSOR_IBEAM    1               /* Text Select */
#define CURSOR_WAIT     2               /* Busy */
#define CURSOR_CROSS    3               /* Precision Select */
#define CURSOR_UPARROW  4               /* Alternate Select */
#define CURSOR_SIZEALL  5               /* Move */
#define CURSOR_SIZENWSE 6               /* Diagonal Resize 1 */
#define CURSOR_SIZENESW 7               /* Diagonal Resize 2 */
#define CURSOR_SIZEWE   8               /* Horizontal Resize */
#define CURSOR_SIZENS   9               /* Vertical Resize */
#define CURSORS         10
BOOL  SNSYSTEM_API CursorsSet(int which, LPCSTR file);   /* a .CUR or .ANI (short path); NULL: Windows' own */
HCURSOR SNSYSTEM_API CursorsLoad(LPCSTR file);            /* as it would be set: to show it */

/* ---- filedialogs.c ---- */
typedef struct {                                          /* SBM_FILEDLG */
    OPENFILENAME FAR *ofn;
    HTASK task;                                           /* the program, waiting until done */
    BOOL  save, done, ok;
    char  dir[MAX_PATH];                                  /* its current folder */
} FILEREQUEST;
BOOL  SNSYSTEM_API FileDialogsServe(HWND w);              /* Open and Save As shown by w; NULL: COMMDLG's */

#ifdef SNSYSTEM_DLL                                       /* inside the library */
extern HINSTANCE g_hInst;
void  SkinWindow(HWND w);                                 /* hooks.c -> skin.c */
void  SkinForget(HWND w);                                 /* w is being destroyed: its marks go */
BOOL  SkinHas(HWND w);                                    /* skinned: SkinFrameProc zooms it */
void  SkinWindowOff(HWND w);                              /* another program's window: the skin off */
BOOL  SkinIsMenu(HWND w);
void  SkinMenu(HWND w);
void  SkinMenus(BOOL on);                                 /* those made before; FALSE: undone */
BOOL  SkinMenuPoint(MSG FAR *m);                          /* a mouse message to the menu loop */
void  SkinMenuDrawn(const DRAWITEMSTRUCT FAR *d);       /* an owner-drawn item (NULL: a menu starts) */
BOOL  HookSkinning(void);                                 /* skin.c -> hooks.c: in this task */
COLORREF DrawSysColor(int index);                       /* GetSysColor, or the picture's */
COLORREF DrawSkinColor(int sc);                         /* SkinColor, ... */
void  SkinScrollMessage(HWND w, UINT msg, HWND notify);   /* hooks.c -> skinscroll.c: a message sent to w */
void  CpuEnd(BOOL keep);                                  /* CpuStop; WEP's: FALSE */
void  DrawFree(void);                                     /* WEP: the icons' images, the memory DC */
void  CursorsRestore(void);                               /* HookRemove: Windows' own pointers back */
void  HookArea(RECT FAR *r);                              /* the work area (else the screen) */
void  HookMoved(HWND w);                                  /* moved: snapped if let go at an edge */
#endif

#endif
