/*
 * Facts: what a screenshot shows, in words, for the tests to compare instead
 * of pixels (tests\run.ps1). The screen's visible windows, front first, or
 * one window; in each, every visible control: class, id, text, its state
 * (off, checked, default, focus), a list's entries, a menu bar. Snowball's
 * own controls describe themselves ("SnowballDescribe", see
 * src\hooks\hooks.h). Then the menus open now (SNSYSTEM keeps
 * them), the items Snowball draws itself described by their owner
 * ("SnowballDescribeMenu"). No handles, no places inside windows, no times.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define DESCRIBE_SIZE       16384
#define DESCRIBE_MENU_SIZE  2048        /* as src\hooks\hooks.h */
#define MAX_MENUS           8

typedef int (FAR PASCAL *OPENMENUS)(HMENU FAR *menus, HWND FAR *owners, int max);

static FILE *f;
static LPSTR buf;
static UINT describeMsg, describeMenuMsg;

static void indent(int depth)
{
    while (depth-- > 0)
        fputs("  ", f);
}

/* A describe's lines, each indented. */
static void lines(LPSTR text, int depth)
{
    char line[300];
    int n;

    while (*text) {
        for (n = 0; text[n] && text[n] != '\n'; n++)
            ;
        lstrcpyn(line, text, min(n + 1, (int)sizeof line));
        indent(depth);
        fprintf(f, "%s\n", line);
        text += n + (text[n] == '\n');
    }
}

/* A menu's items: label, off, checked, a submenu (>); drawn ones, "drawn". */
static void menu_items(HMENU m, int depth)
{
    char text[128];
    UINT st, flags;
    int i, n = GetMenuItemCount(m);

    for (i = 0; i < n; i++) {
        st = GetMenuState(m, i, MF_BYPOSITION);
        flags = (st & MF_POPUP) ? (st & 0xFF) : st;     /* a submenu's high byte: its item count */
        indent(depth);
        if (flags & MF_SEPARATOR) {
            fputs("---\n", f);
            continue;
        }
        text[0] = '\0';
        GetMenuString(m, i, text, sizeof text, MF_BYPOSITION);
        fprintf(f, "'%s'%s%s%s%s\n", text, (flags & MF_OWNERDRAW) && !text[0] ? " drawn" : "",
                flags & (MF_GRAYED | MF_DISABLED) ? " off" : "", flags & MF_CHECKED ? " checked" : "",
                flags & MF_POPUP ? " >" : "");
    }
}

/* A list box's or combo box's entries, the selected ones; owner-drawn with
   no strings: how many. */
static void entries(HWND c, BOOL combo, int depth)
{
    char text[200];
    LONG style = GetWindowLong(c, GWL_STYLE);
    int i, cur, n = (int)SendMessage(c, combo ? CB_GETCOUNT : LB_GETCOUNT, 0, 0L);
    BOOL strings = combo ? !(style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)) || (style & CBS_HASSTRINGS)
                         : !(style & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) || (style & LBS_HASSTRINGS);

    if (n <= 0)
        return;
    if (!strings) {
        indent(depth);
        fprintf(f, "%d entries\n", n);
        return;
    }
    cur = combo ? (int)SendMessage(c, CB_GETCURSEL, 0, 0L) : -1;
    for (i = 0; i < n && i < 400; i++) {
        text[0] = '\0';
        if (SendMessage(c, combo ? CB_GETLBTEXTLEN : LB_GETTEXTLEN, i, 0L) < (LONG)sizeof text)
            SendMessage(c, combo ? CB_GETLBTEXT : LB_GETTEXT, i, (LPARAM)(LPSTR)text);
        indent(depth);
        fprintf(f, "entry '%s'%s\n", text, (combo ? i == cur : SendMessage(c, LB_GETSEL, i, 0L) > 0) ? " selected" : "");
    }
}

static void window(HWND w, int depth)
{
    char cls[40], text[256];
    LONG style = GetWindowLong(w, GWL_STYLE);
    BOOL child = (style & WS_CHILD) != 0;
    int type = (int)(style & 0x0F);
    RECT r;
    HWND c;

    GetClassName(w, cls, sizeof cls);
    if (!child && !lstrcmp(cls, "#32768"))      /* a menu's window: with the open menus */
        return;
    GetWindowText(w, text, sizeof text);
    indent(depth);
    fputs(cls, f);
    if (child)
        fprintf(f, " #%d", GetDlgCtrlID(w));
    if (!lstrcmpi(cls, "Static") && type == SS_ICON)
        fputs(" icon", f);
    else if (text[0])
        fprintf(f, " '%s'", text);
    if (!lstrcmpi(cls, "Button")) {
        if (type == BS_DEFPUSHBUTTON)
            fputs(" default", f);
        else if (type == BS_OWNERDRAW)
            fputs(" drawn", f);
        else if (type != BS_PUSHBUTTON && type != BS_GROUPBOX && type != BS_USERBUTTON)
            switch ((int)SendMessage(w, BM_GETCHECK, 0, 0L)) {
            case 1: fputs(" checked", f); break;
            case 2: fputs(" mixed", f); break;
            }
    }
    if (!IsWindowEnabled(w))
        fputs(" off", f);
    if (GetFocus() == w)
        fputs(" focus", f);
    if (!child) {
        GetWindowRect(w, &r);
        fprintf(f, " at %d,%d %dx%d", r.left, r.top, r.right - r.left, r.bottom - r.top);
        if (IsIconic(w))
            fputs(" minimized", f);
        if (IsZoomed(w))
            fputs(" maximized", f);
        if (GetActiveWindow() == w)
            fputs(" active", f);
    }
    fputc('\n', f);
    buf[0] = '\0';
    if (SendMessage(w, describeMsg, DESCRIBE_SIZE, (LPARAM)buf) > 0 && buf[0])
        lines(buf, depth + 1);
    if (!lstrcmpi(cls, "ListBox") || !lstrcmpi(cls, "ComboBox"))
        entries(w, !lstrcmpi(cls, "ComboBox"), depth + 1);
    if (!child && GetMenu(w) && IsMenu(GetMenu(w))) {
        indent(depth + 1);
        fputs("menu bar\n", f);
        menu_items(GetMenu(w), depth + 2);
    }
    for (c = GetWindow(w, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        if (IsWindowVisible(c))
            window(c, depth + 1);
}

static void open_menus(void)
{
    HMENU menus[MAX_MENUS];
    HWND owners[MAX_MENUS];
    HINSTANCE sys = (HINSTANCE)GetModuleHandle("SNSYSTEM");
    OPENMENUS open = sys ? (OPENMENUS)GetProcAddress(sys, "HOOKOPENMENUS") : NULL;
    char cls[40];
    int i, n;

    if (!open)
        return;
    n = open(menus, owners, MAX_MENUS);
    for (i = 0; i < n; i++) {
        cls[0] = '\0';
        if (IsWindow(owners[i]))
            GetClassName(owners[i], cls, sizeof cls);
        fprintf(f, "open menu %d (of %s)\n", i + 1, cls);
        buf[0] = '\0';
        if (IsWindow(owners[i]) && SendMessage(owners[i], describeMenuMsg, (WPARAM)menus[i], (LPARAM)buf) > 0 && buf[0])
            lines(buf, 1);
        else
            menu_items(menus[i], 1);
    }
}

/* The facts of the screen (win NULL) or of one window, into file. */
void facts_write(const char *file, HWND win)
{
    HGLOBAL mem;
    HWND w;

    if ((f = fopen(file, "w")) == NULL)
        return;
    if (!describeMsg) {
        describeMsg = RegisterWindowMessage("SnowballDescribe");
        describeMenuMsg = RegisterWindowMessage("SnowballDescribeMenu");
    }
    mem = GlobalAlloc(GMEM_MOVEABLE, DESCRIBE_SIZE);
    buf = mem ? (LPSTR)GlobalLock(mem) : NULL;
    if (buf) {
        if (win)
            window(win, 0);
        else
            for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT))
                if (IsWindowVisible(w))
                    window(w, 0);
        open_menus();
        GlobalUnlock(mem);
    }
    if (mem)
        GlobalFree(mem);
    fclose(f);
}
