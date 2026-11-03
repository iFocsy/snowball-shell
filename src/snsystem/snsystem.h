/*
 * SNSYSTEM.DLL, the part of Snowball at work in every program:
 *   draw.c         drawing primitives, shared with the shell
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
BOOL  SNSYSTEM_API DrawIconImages(HICON icon, HINSTANCE module, LPCSTR group);
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
#define CAPTION_END_ACTIVE      RGB(16, 132, 208)         /* where captions' gradients go */
#define CAPTION_END_INACTIVE    RGB(192, 192, 192)

/* ---- hooks.c ---- */
BOOL  SNSYSTEM_API HookInstall(HWND notify, BOOL skinPrograms);
void  SNSYSTEM_API HookRemove(void);
BOOL  SNSYSTEM_API HookLost(void);                        /* an event was lost (full queue); clears */
void  SNSYSTEM_API HookTitleSeen(void);                   /* the posted title change was handled */
void  SNSYSTEM_API HookWorkArea(const RECT FAR *r);         /* what maximized windows fill */
void  SNSYSTEM_API HookHotKeys(const WORD FAR *keys, int n);  /* shortcut keys: SBM_HOTKEY */

/* ---- cpu.c ---- */
BOOL  SNSYSTEM_API CpuStart(void);                        /* measuring from now on */
void  SNSYSTEM_API CpuStop(void);
DWORD SNSYSTEM_API CpuClock(void);                        /* 1193182 Hz ticks; wraps hourly */
DWORD SNSYSTEM_API CpuIdle(void);                         /* ticks spent idle, same clock */
DWORD SNSYSTEM_API CpuTime(HTASK task);                   /* ms of processor used; NULL: idle */

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
BOOL  SkinIsMenu(HWND w);
void  SkinMenu(HWND w);
void  SkinMenus(BOOL on);                                 /* those made before; FALSE: undone */
BOOL  SkinMenuPoint(MSG FAR *m);                          /* a mouse message to the menu loop */
BOOL  HookSkinning(void);                                 /* skin.c -> hooks.c: in this task */
void  HookArea(RECT FAR *r);                              /* the work area (else the screen) */
#endif

#endif
