/*
 * The tests' hooks (hooks.h): the words of the small controls, read through
 * what each lets them see (beside it, under SNOWBALL_HOOKS): the tabs, a
 * toolbar's buttons, a slider's position, the tooltip's text, the Alt+Tab
 * switcher's windows.
 */
#include "internal.h"

LRESULT TabsDescribe(HWND w, LPSTR buf, WORD size)
{
    LPCSTR label;
    BOOL selected;
    int i;

    for (i = 0; (label = TabsLabel(w, i, &selected)) != NULL; i++)
        DescribeLine(buf, size, "tab '%s'%s", label, (LPSTR)(selected ? " selected" : ""));
    return lstrlen(buf);
}

LRESULT ToolbarDescribe(HWND w, LPSTR buf, WORD size)
{
    char label[20];
    UINT cmd;
    BOOL off, checked;
    int i;

    for (i = 0; ToolbarButton(w, i, &cmd, label, &off, &checked); i++)
        DescribeLine(buf, size, "button %u '%s'%s%s", cmd, (LPSTR)label, (LPSTR)(off ? " off" : ""),
                     (LPSTR)(checked ? " checked" : ""));
    return lstrlen(buf);
}

LRESULT SliderDescribe(HWND w, LPSTR buf, WORD size)
{
    return DescribeLine(buf, size, "position %d of %d", SliderPos(w), SliderCount(w));
}

/* Its lines, | between. */
LRESULT TooltipDescribe(LPSTR buf, WORD size)
{
    char one[260];
    int i;

    lstrcpyn(one, TooltipText(), sizeof one);
    for (i = 0; one[i]; i++)
        if (one[i] == '\r' || one[i] == '\n')
            one[i] = one[i] == '\n' ? '|' : ' ';
    return DescribeLine(buf, size, "'%s'", (LPSTR)one);
}

LRESULT SwitcherDescribe(LPSTR buf, WORD size)
{
    char title[80];
    BOOL selected;
    HWND w;
    int i;

    for (i = 0; (w = SwitcherWindow(i, &selected)) != NULL; i++) {
        GetWindowText(w, title, sizeof title);
        DescribeLine(buf, size, "'%s'%s", (LPSTR)title, (LPSTR)(selected ? " selected" : ""));
    }
    return lstrlen(buf);
}
