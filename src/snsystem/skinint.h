/*
 * What the parts of the skin share (skin.c, skinmdi.c, skinctl.c,
 * skinmenu.c): for
 * them only, included after snsystem.h.
 */
#ifndef SKININT_H
#define SKININT_H

/* Properties of a skinned window (all removed when it is destroyed). */
#define OLD_LO  "SbOldLo"
#define OLD_HI  "SbOldHi"
#define FRAMED  "SbFramed"      /* edit or list box that gets a sunken edge */
#define MDIBARS "SbMdiBars"     /* a frame whose menu bar has our document items: */
                                /* the document window they are for */
#define MDIMENU "SbMdiMenu"     /* ... and its window menu, opened by our icon item */

/* ---- skin.c ---- */
LRESULT SkinCallOld(HWND w, UINT msg, WPARAM wp, LPARAM lp);   /* the previous procedure */
void    SkinSubclass(HWND w, WNDPROC proc);                    /* once */
void    SkinUnsubclass(HWND w);                                /* and back */
LRESULT CALLBACK __export SkinFrameProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);
void    SkinDrawButton(HDC dc, const RECT *b, int which, BOOL down, BOOL enabled);  /* SKIN_* */

/* ---- skinmdi.c ---- */
void    SkinMdiMenu(HWND frame);                             /* after it drew: our items in */
BOOL    SkinMdiItem(HWND frame, UINT msg, LPARAM lp);        /* WM_MEASUREITEM, WM_DRAWITEM */

/* ---- skinctl.c ---- */
void    SkinRaisedEdge(HDC dc, RECT *r);                       /* menus' edge too */

#endif
