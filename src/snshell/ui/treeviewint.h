/* Inside SbTree: what treeview.c (its rows, the window procedure) and
   treepaint.c share. Only they include it, after internal.h. */
#ifndef TREEVIEWINT_H
#define TREEVIEWINT_H

#define ROW_H       18
#define INDENT      19

#define TF_EXPANDED 0x01
#define TF_KIDS     0x02                /* has (or may have) children */
#define TF_LAST     0x04                /* last of its siblings */

typedef struct {
    WORD  text, path;                   /* offsets in the string pool */
    HICON icon;
    BYTE  depth, flags, kind;           /* kind: IVK_* */
    WORD  lines;                        /* bit d: the ancestor at depth d has more siblings */
} NODE;

typedef struct {
    HGLOBAL  hRows, hPool;
    NODE far *row;
    LPSTR    pool;
    int      count, cap, top, sel, drop;
    WORD     used, poolCap;
    BOOL     plain, files;              /* one folder's tree; with its files */
} TREE;

#define Tree(w)     ((TREE *)LocalHeapPointer(GetWindowWord(w, 0)))

/* ---- treepaint.c ---- */
void TreeViewPaint(HWND w);                       /* WM_PAINT */

#endif
