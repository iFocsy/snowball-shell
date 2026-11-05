/*
 * The tests' hooks (hooks.h): menus whose items Snowball draws itself, by
 * their labels: Send To's; and the popup menus a hidden keeper owns
 * (popupmenu.c), answered by the window that asked for them.
 */
#include "internal.h"

LRESULT SendToDescribeMenu(HMENU m, LPSTR buf)
{
    char label[MAX_PATH];
    int i;

    if (!SendToIsMenu(m))
        return 0;
    for (i = 0; i < GetMenuItemCount(m); i++) {
        if (!SendToItemLabel(GetMenuItemID(m, i), label))
            GetMenuString(m, i, label, sizeof label, MF_BYPOSITION);
        DescribeLine(buf, DESCRIBE_MENU_SIZE, "'%.200s'", (LPSTR)label);
    }
    return lstrlen(buf);
}

LRESULT PopupMenuDescribeMenu(WPARAM wp, LPARAM lp)
{
    HWND owner = PopupMenuOwner();

    return owner ? SendMessage(owner, DescribeMenuMessage(), wp, lp) : 0;
}
