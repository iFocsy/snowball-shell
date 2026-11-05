/*
 * The tests' hooks (hooks.h): SbTree's words, the rows shown, indented by
 * depth, the open and the selected ones.
 */
#include "internal.h"
#include "ui\treeviewint.h"

LRESULT TreeViewDescribe(HWND w, LPSTR buf, WORD size)
{
    TREE *t = Tree(w);
    char pad[24];
    int i;

    for (i = 0; t && i < t->count; i++) {
        _fmemset(pad, ' ', sizeof pad);
        pad[min(2 * t->row[i].depth, sizeof pad - 1)] = '\0';
        DescribeLine(buf, size, "%s%.200s%s%s", (LPSTR)pad, t->pool + t->row[i].text,
                     (LPSTR)(t->row[i].flags & TF_EXPANDED ? " open" : ""), (LPSTR)(i == t->sel ? " selected" : ""));
    }
    return lstrlen(buf);
}
