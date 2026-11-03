/*
 * What the parts of the skin share (skin.c, skinctl.c, skinmenu.c): for
 * them only, included after snsystem.h.
 */
#ifndef SKININT_H
#define SKININT_H

/* Properties of a skinned window (all removed when it is destroyed). */
#define OLD_LO  "SbOldLo"
#define OLD_HI  "SbOldHi"
#define FRAMED  "SbFramed"      /* edit or list box that gets a sunken edge */

/* ---- skin.c ---- */
LRESULT SkinCallOld(HWND w, UINT msg, WPARAM wp, LPARAM lp);   /* the previous procedure */
void    SkinSubclass(HWND w, WNDPROC proc);                    /* once */
LRESULT CALLBACK __export SkinFrameProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);

/* ---- skinctl.c ---- */
void    SkinRaisedEdge(HDC dc, RECT *r);                       /* menus' edge too */

#endif
