/*
 * The tests' hooks (hooks.h): the items of an open menu of the Start menu,
 * which it draws itself, by their labels.
 */
#include "internal.h"
#include "taskbar\startmenuint.h"

LRESULT StartMenuDescribeMenu(HMENU m, LPSTR buf)
{
    ENTRY far *e;
    BOOL any = FALSE;
    int i;

    for (i = 0; i < StartMenuCount; i++) {
        e = StartMenuEntries[i];
        if (e->parent != m)
            continue;
        any = TRUE;
        if (e->separator)
            DescribeLine(buf, DESCRIBE_MENU_SIZE, "---");
        else
            DescribeLine(buf, DESCRIBE_MENU_SIZE, "'%.200s'%s%s", e->label ? e->label : (LPSTR)"",
                         (LPSTR)(e->sub ? " >" : ""), (LPSTR)(e->disabled ? " off" : ""));
    }
    return any ? lstrlen(buf) : 0;
}
