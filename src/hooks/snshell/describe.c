/*
 * The tests' hooks (hooks.h): the two registered messages, and a line added
 * to a control's answer.
 */
#include "internal.h"
#include <stdarg.h>

static UINT describe, describeMenu;

UINT DescribeMessage(void)
{
    if (!describe)
        describe = RegisterWindowMessage("SnowballDescribe");
    return describe;
}

UINT DescribeMenuMessage(void)
{
    if (!describeMenu)
        describeMenu = RegisterWindowMessage("SnowballDescribeMenu");
    return describeMenu;
}

/* A line added to buf (size bytes in all) if it fits, else left out: what
   is there stays whole. The length after. */
LRESULT DescribeLine(LPSTR buf, WORD size, LPCSTR fmt, ...)
{
    char line[300];
    va_list args;
    WORD used = lstrlen(buf), n;

    va_start(args, fmt);
    n = wvsprintf(line, fmt, (const void FAR *)args);
    va_end(args);
    if (used + n + 2 <= size) {
        lstrcpy(buf + used, line);
        lstrcpy(buf + used + n, "\n");
        used += n + 1;
    }
    return used;
}
