/*
 * The tests' hooks (hooks.h): the taskbar's words, where it is, its buttons
 * with the active and flashing ones, Quick Launch's buttons, the tray (not
 * the time on the clock, which changes by itself).
 */
#include "internal.h"
#include "taskbar\taskbarint.h"

LRESULT TaskbarDescribe(HWND w, LPSTR buf, WORD size)
{
    char name[MAX_PATH];
    LPCSTR side;
    int i;

    switch (TaskbarSide) {
    case TBE_TOP:   side = "top"; break;
    case TBE_LEFT:  side = "left"; break;
    case TBE_RIGHT: side = "right"; break;
    default:        side = "bottom";
    }
    DescribeLine(buf, size, "at the %s, %d row(s)%s%s%s", side, TaskbarRows,
                 (LPSTR)(TaskbarLocked ? ", locked" : ""), (LPSTR)(TaskbarStartOpen ? ", Start menu open" : ""),
                 (LPSTR)(TaskbarIconsOnly ? ", icons only" : ""));
    for (i = 0; i < TaskbarNTasks; i++)
        DescribeLine(buf, size, "button '%.200s'%s%s", (LPSTR)TaskbarTitles[i],
                     (LPSTR)(TaskbarTasks[i] == TaskbarActive ? " active" : ""), (LPSTR)(TaskbarLit[i] ? " flashing" : ""));
    for (i = 0; i < QuickLaunchCount(); i++) {
        QuickLaunchName(i, name);
        DescribeLine(buf, size, "quick launch '%.200s'", (LPSTR)name);
    }
    DescribeLine(buf, size, "tray: %d icon(s), clock %s", TaskbarTrayCount(), (LPSTR)(TaskbarShowClock ? "shown" : "hidden"));
    return lstrlen(buf);
}
