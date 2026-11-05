/*
 * The tests' hooks (hooks.h): SbIconView's words, how it shows its items,
 * then each in its order: kind, name, shortcut, selected, focused.
 */
#include "internal.h"
#include "ui\iconviewint.h"

static const char *const modes[] = { "large icons", "small icons", "list", "details", "thumbnails" };
static const char *const kinds[] = { "file", "folder", "drive", "computer", "recycle bin" };

LRESULT IconViewDescribe(HWND w, LPSTR buf, WORD size)
{
    VIEW *v = View(w);
    ITEM far *it;
    int i;

    if (!v)
        return 0;
    DescribeLine(buf, size, "%s, %d items", (LPSTR)modes[v->mode >= 0 && v->mode < 5 ? v->mode : 0], v->count);
    for (i = 0; i < v->count; i++) {
        it = v->it + i;
        DescribeLine(buf, size, "%s '%.200s'%s%s%s", (LPSTR)kinds[it->kind < 5 ? it->kind : 0], v->pool + it->name,
                     (LPSTR)(it->flags & IF_SHORTCUT ? " shortcut" : ""),
                     (LPSTR)(it->flags & IF_SELECTED ? " selected" : ""), (LPSTR)(i == v->focus ? " focused" : ""));
    }
    return lstrlen(buf);
}
