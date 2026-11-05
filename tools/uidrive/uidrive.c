/*
 * UIDRIVE.EXE - scripted UI driver for testing inside Windows 3.11.
 *
 * Reads commands from T:\UIDRIVE.TXT, one per line, logs to T:\UIDRIVE.LOG.
 * A <class> is the first visible top-level window of that class, or with
 * "Parent/Child" its first visible window of class Child (coordinates then
 * in that child), e.g. click SbFolder/SbIconView 37 18:
 *   run <command line>          WinExec, shown normally
 *   wait <ms>                   keep handling messages for that long (idle)
 *   spin <ms>                   the same, busy: shows as processor use
 *   waitgone <ms> <title>       the same until no visible top-level window's title
 *                                 starts so (at most ms); logs how long it took
 *   click <class> <x> <y>       left click at client coordinates of the first
 *   rclick <class> <x> <y>        visible top-level window of that class
 *   dclick <class> <x> <y>        (right click, double click)
 *   key <class> <vk>            key press (virtual-key code, decimal)
 *   drag <class> <x> <y> <class2> <x2> <y2> [r]
 *                               drag (right button with r) from a client point
 *                                 of one window to one of another; the real
 *                                 cursor moves, as drag loops read it
 *   hwkey <vk> down|up          a key as the keyboard sends it (USER's keybd_event:
 *                                 hooks and Alt+Tab see it)
 *   hwmove <class> <x> <y>      the mouse as the mouse driver moves it (USER's
 *   hwclick <class> <x> <y> [r]   mouse_event: menus opened by it follow the mouse)
 *   hwbutton <class> <x> <y> down|up   the left button alone, there
 *   items <class> [id]          log a combo box's or list box's entries (id: that control of it)
 *   children <class>            log a window's controls: id, class, text
 *   selectid <class> <id> <n>   entry n of control id chosen (the dialog told, as by the user)
 *   hwdrag <class> <x> <y> <class2> <x2> <y2>
 *                               a left-button drag by the mouse driver, in 8 steps
 *   move <class> <x> <y>        the real cursor to a client point, with WM_MOUSEMOVE
 *   type <text>                 characters to whichever window has the focus
 *   char <code>                 one character (decimal) to the focused window
 *   command <class> <id>        WM_COMMAND to that window
 *   mdi <class> max|restore|next  the active document window (MDI) of that frame
 *   menu <class> <text>         the command of its menu item whose text (without
 *                                 &) starts so, e.g. menu Notepad Save As
 *   close <class>               WM_CLOSE
 *   flash <class>               FlashWindow once (the caption's look turns over)
 *   show <class> <n>            ShowWindow with SW_ n (3 maximized, 9 restored)
 *   calmira <code> [text]       Calmira's registered message (text as atom)
 *   dde <service> <topic> <commands>
 *                               DDEML execute, e.g. dde PROGMAN PROGMAN [CreateGroup(X)]
 *   ddereq <service> <topic> <item>
 *                               DDEML request, the text answer goes to the log
 *   snap <file> [class]         screenshot to a 24-bit BMP: the whole screen, or that window;
 *                               its facts (facts.c: the same in words) beside it as .TXT
 *   windows [all]               log the visible (all: also hidden) top-level windows
 *   mem                         log free memory (KB) and free system, GDI, USER resources (%)
 *   cpu <ms>                    count rounds of plain work for that long: fewer when
 *                                 something else takes the processor (a driver's timer)
 *   bench <n> <class>           repaint that window and all its controls n times; log the
 *                                 time each took (timeGetTime, 1 ms)
 *   exitwindows                 end the Windows session
 * Waiting for things rather than for a time:
 *   waitfor <ms> <class> [title]  until that window is there (and its title
 *                                 starts so), at most ms; FAIL if it never is
 *   settle <ms> [quiet]         until the screen has not changed for quiet ms
 *                                 (700: longer than a tip takes), at most ms
 * Checks: a FAIL line in the log when the window is not as said. A <target> is
 * a <class> as above, or <class>@<id> for that control of it:
 *   expect window|nowindow <target>
 *   expect text <target> <text>   its text exactly (a list's: the selected entry)
 *   expect count <target> <n>     a list box's or combo box's entries
 *   expect checked <target> <n>   a button's check (0, 1, 2)
 *   expect enabled|disabled|focus <target>
 *   expect menu <class> <item> [checked|unchecked|grayed|enabled]
 *                                 an item of its menu, as it would show when
 *                                 opened (text before any tab, & left out)
 *   heap mark                   counts the blocks in GDI's and USER's heaps
 *   heap check [n [what]]       FAIL if either has more than n (0) blocks more now
 *                                 (what: the steps since the mark, for the FAIL line)
 * Always: the caret does not blink; the log starts with DOS's date and time,
 * and DOS's clock is held at that minute (set back every 20 s);
 * Windows' parameter errors in Snowball (its task) go to the log as FAIL
 * lines (other programs' errors, and warnings, as notes); a
 * fault in any program (a GP fault...) ends it, without Windows' box, and is
 * a FAIL line, also in T:\FAULT.TXT; the log ends with the count of FAILs.
 */
#include <windows.h>
#include <ddeml.h>
#include <toolhelp.h>
#include <mmsystem.h>
#include <dos.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *out;
static HINSTANCE inst;
static DWORD ddeInst;
static char findClass[64], findTitle[80];
static HWND foundWnd;
static char goneTitle[80];
static BOOL titled;
static int failures;

static void fail(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(out, "  FAIL: ");
    vfprintf(out, fmt, ap);
    fputc('\n', out);
    va_end(ap);
    failures++;
}

/* Windows' errors as ToolHelp tells them, kept here (the callback runs in the
   erring task: no files there) until the next step logs them: the program
   (task) that made the call, where in Windows it was refused, and the call
   that led there: the first return address on the task's stack that follows
   a far call (9A) from a module not Windows' own. */
#define MAX_ERRORS 16
static struct {
    WORD kind, code;
    DWORD addr, call, bad;              /* bad: the value refused */
    WORD callSeg;
    char task[MAX_MODULE_NAME + 1], callModule[MAX_MODULE_NAME + 1];
} errors[MAX_ERRORS];
static int nErrors, lostErrors;
static HTASK myTask;                    /* (its own errors are not told) */

/* A GP fault (or another fault) in any program: ToolHelp's interrupt
   callback notes it in T:\FAULT.TXT (from the faulting task, so the program
   it is in may be the shell that Windows ends with) and ends the program
   without Windows' box, which would stop every other program, this too.
   ToolHelp calls it with AX, the interrupt's number, a handle of its own
   and the faulting IP, CS and flags on the stack; it keeps all registers. */
static BOOL faultSeen;
static char faultText[96];

#pragma off (check_stack)               /* (on the erring task's stack) */
BOOL CALLBACK __export __loadds notify(WORD id, DWORD data)
{
    static const char *windows[] = { "UIDRIVE", "TOOLHELP", "KERNEL", "USER", "GDI" };
    static TASKENTRY te;                /* (in DS: SS is the erring task's) */
    static GLOBALENTRY ge;
    static MODULEENTRY me;
    WORD ss_, sp_, FAR *w, n, i, k;
    DWORD limit;

    if ((id != NFY_LOGPARAMERROR && id != NFY_LOGERROR) || GetCurrentTask() == myTask)
        return FALSE;
    if (nErrors == MAX_ERRORS) {
        lostErrors++;
        return FALSE;
    }
    errors[nErrors].kind = id;
    if (id == NFY_LOGPARAMERROR) {
        errors[nErrors].code = ((NFYLOGPARAMERROR FAR *)data)->wErrCode;
        errors[nErrors].addr = (DWORD)((NFYLOGPARAMERROR FAR *)data)->lpfnErrorAddr;
        errors[nErrors].bad = (DWORD)((NFYLOGPARAMERROR FAR *)data)->lpBadParam;
    } else {
        errors[nErrors].code = ((NFYLOGERROR FAR *)data)->wErrCode;
        errors[nErrors].addr = 0;
    }
    te.dwSize = sizeof te;
    if (TaskFindHandle(&te, GetCurrentTask()))
        lstrcpy(errors[nErrors].task, te.szModule);
    else
        lstrcpy(errors[nErrors].task, "?");
    errors[nErrors].call = 0;
    _asm {
        mov ss_, ss
        mov sp_, sp
    }
    limit = GetSelectorLimit(ss_);
    n = (WORD)((limit - sp_ + 1) / 2);
    if (n > 4096)
        n = 4096;
    w = (WORD FAR *)MAKELP(ss_, sp_);
    for (i = 0; i + 1 < n && !errors[nErrors].call; i++) {
        BYTE FAR *call = (BYTE FAR *)MAKELP(w[i + 1], w[i] - 5);
        if (w[i] < 5 || !(w[i + 1] & 4) || IsBadReadPtr(call, 5) || *call != 0x9A)
            continue;
        ge.dwSize = sizeof ge;
        me.dwSize = sizeof me;
        if (!GlobalEntryHandle(&ge, (HGLOBAL)LOWORD(GlobalHandle(w[i + 1]))) || ge.wType != GT_CODE ||
            !ModuleFindHandle(&me, (HMODULE)ge.hOwner))
            continue;
        for (k = 0; k < 5 && lstrcmp(me.szModule, windows[k]); k++)
            ;
        if (k == 5) {
            errors[nErrors].call = MAKELONG(w[i], w[i + 1]);
            errors[nErrors].callSeg = ge.wData;
            lstrcpy(errors[nErrors].callModule, me.szModule);
        }
    }
    nErrors++;
    return FALSE;                       /* others see it too */
}

void __far __cdecl __loadds fault_cb(WORD ax, WORD number, WORD handle, WORD ip, WORD cs, WORD flags)
{
    static GLOBALENTRY ge;              /* (in DS: SS is the faulting task's) */
    static MODULEENTRY me;
    HFILE f;

    _asm {
        push bx
        push cx
        push dx
        push es
    }
    if (number == 0 || number == 6 || number == 12 || number == 13) {
        ge.dwSize = sizeof ge;
        me.dwSize = sizeof me;
        if (!GlobalEntryHandle(&ge, (HGLOBAL)LOWORD(GlobalHandle(cs))) || !ModuleFindHandle(&me, (HMODULE)ge.hOwner))
            lstrcpy(me.szModule, "?");
        wsprintf(faultText, "%s at %s %04X:%04X", (LPSTR)(number == 13 ? "GP fault" : number == 12 ? "stack fault" :
                 number == 6 ? "invalid instruction" : "division by zero"), (LPSTR)me.szModule, ge.wData, ip);
        faultSeen = TRUE;
        if ((f = _lcreat("T:\\FAULT.TXT", 0)) != HFILE_ERROR) {
            _lwrite(f, faultText, lstrlen(faultText));
            _lclose(f);
        }
        TerminateApp(NULL, NO_UAE_BOX);
    }
    _asm {
        pop es
        pop dx
        pop cx
        pop bx
    }
}
#pragma on (check_stack)

/* The module whose code is at addr ("?" when not known); its segment number. */
static WORD module_at(DWORD addr, char *name)
{
    GLOBALENTRY ge;
    MODULEENTRY me;

    strcpy(name, "?");
    ge.dwSize = sizeof ge;
    me.dwSize = sizeof me;
    if (addr && GlobalEntryHandle(&ge, (HGLOBAL)LOWORD(GlobalHandle(SELECTOROF(addr)))) &&
        ModuleFindHandle(&me, (HMODULE)ge.hOwner)) {
        strcpy(name, me.szModule);
        return ge.wData;
    }
    return 0;
}

/* The function exported by that module whose code is at addr, or just
   before it: its ordinal (0 if none); *past how far past its start. */
static WORD export_at(const char *module, DWORD addr, WORD *past)
{
    HMODULE m = GetModuleHandle(module);
    WORD ord, best = 0, start = 0;
    FARPROC f;

    for (ord = 1; m && ord < 1024; ord++) {
        f = GetProcAddress(m, MAKEINTRESOURCE(ord));
        if (f && SELECTOROF(f) == SELECTOROF(addr) && OFFSETOF(f) <= OFFSETOF(addr) && OFFSETOF(f) >= start) {
            best = ord;
            start = OFFSETOF(f);
        }
    }
    *past = OFFSETOF(addr) - start;
    return best;
}

/* Logs the errors told since the last time: FAIL for Snowball's (its task
   is SNOWBALL), a note for warnings and other programs'; each with the
   function that refused it, as MODULE.ordinal+offset (tests\apinames.py
   names it). A fault: FAIL. */
static void log_errors(void)
{
    char module[MAX_MODULE_NAME + 1];
    int i;
    WORD seg, ord, past;

    for (i = 0; i < nErrors; i++) {
        BOOL warning = (errors[i].code & ERR_WARNING) != 0, ours = !stricmp(errors[i].task, "SNOWBALL");
        seg = module_at(errors[i].addr, module);
        ord = export_at(module, errors[i].addr, &past);
        if (errors[i].kind == NFY_LOGERROR && ours)
            fail("Windows error %04X in %s", errors[i].code, errors[i].task);
        else if (errors[i].kind == NFY_LOGERROR)
            fprintf(out, "  note: Windows error %04X in %s\n", errors[i].code, errors[i].task);
        else if (!warning && ours)
            fail("parameter error %04X (%08lX) in %s, refused by %s.%u+%04X, called from %s %04X:%04X", errors[i].code,
                 errors[i].bad, errors[i].task, module, ord, past, errors[i].call ? errors[i].callModule : "?",
                 errors[i].callSeg, OFFSETOF(errors[i].call) - 5);
        else
            fprintf(out, "  note: parameter %s %04X in %s, refused by %s.%u+%04X, called from %s %04X:%04X\n",
                    warning ? "warning" : "error", errors[i].code, errors[i].task, module, ord, past,
                    errors[i].call ? errors[i].callModule : "?", errors[i].callSeg, OFFSETOF(errors[i].call) - 5);
    }
    if (lostErrors)
        fail("%d more errors than could be kept", lostErrors);
    nErrors = lostErrors = 0;
    if (faultSeen) {
        fail("%s", faultText);
        faultSeen = FALSE;
    }
}

/* DOS's time when the script began; set back to it every 20 s, so that every
   screenshot shows the same minute (the taskbar's clock). */
static struct dostime_t clockStart;
static DWORD clockSet;

static void hold_clock(void)
{
    if (GetTickCount() - clockSet >= 20000) {
        _dos_settime(&clockStart);
        clockSet = GetTickCount();
    }
}

/* Handles messages for ms; idle (asleep between them, a timer wakes it)
   unless busy. */
static void pump_as(DWORD ms, BOOL busy)
{
    DWORD end = GetTickCount() + ms;
    UINT timer = SetTimer(NULL, 0, 50, NULL);
    MSG msg;

    for (;;) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        hold_clock();
        if (GetTickCount() >= end)
            break;
        if (busy)
            Yield();
        else
            WaitMessage();
    }
    KillTimer(NULL, timer);
}

/* The id of the first item in menu m (and its submenus) whose text, & left
   out, starts with text; 0 if none. */
static UINT menu_id(HMENU m, const char *text)
{
    char item[64], *s, *d;
    int i, n = GetMenuItemCount(m);
    UINT id;

    for (i = 0; i < n; i++) {
        if (GetSubMenu(m, i)) {
            if ((id = menu_id(GetSubMenu(m, i), text)) != 0)
                return id;
            continue;
        }
        GetMenuString(m, i, item, sizeof item, MF_BYPOSITION);
        for (s = d = item; *s; s++)
            if (*s != '&')
                *d++ = *s;
        *d = '\0';
        if (*item && !strnicmp(item, text, strlen(text)))
            return GetMenuItemID(m, i);
    }
    return 0;
}

/* USER's keyboard event entry, as the keyboard driver calls it:
   AL virtual key, AH 80h for a release, BL scan code. */
static void hwkey(int vk, BOOL up)
{
    FARPROC entry = GetProcAddress(GetModuleHandle("USER"), "KEYBD_EVENT");
    WORD ax_ = (WORD)(vk | (up ? 0x8000 : 0)), bx_ = (WORD)MapVirtualKey(vk, 0);

    if (!entry)
        return;
    _asm {
        mov ax, ax_
        mov bx, bx_
        xor cx, cx
        xor dx, dx
        xor si, si
        xor di, di
        call dword ptr entry
    }
}

static void pump(DWORD ms)
{
    pump_as(ms, FALSE);
}

/* The mouse as its driver reports it (USER's mouse_event): to a client point
   of w, with button flags (2 left down, 4 left up, 8 right down, 16 right up). */
static void hwmouse(HWND w, int x, int y, WORD buttons)
{
    FARPROC entry = GetProcAddress(GetModuleHandle("USER"), "MOUSE_EVENT");
    POINT p;
    WORD ax_, bx_, cx_;

    if (!entry) {
        fprintf(out, "  no MOUSE_EVENT in USER\n");
        return;
    }
    p.x = x;
    p.y = y;
    ClientToScreen(w, &p);
    ax_ = buttons ? buttons : 0x8001;   /* buttons where it is, or moved (absolute) */
    bx_ = (WORD)((long)p.x * 65535L / (GetSystemMetrics(SM_CXSCREEN) - 1));
    cx_ = (WORD)((long)p.y * 65535L / (GetSystemMetrics(SM_CYSCREEN) - 1));
    _asm {
        mov ax, ax_
        mov bx, bx_
        mov cx, cx_
        mov dx, 2
        xor si, si
        xor di, di
        call dword ptr entry
    }
}

static void hwdrag(HWND w, int x, int y, HWND w2, int x2, int y2)
{
    POINT a, b;
    int i;

    a.x = x; a.y = y;
    b.x = x2; b.y = y2;
    ClientToScreen(w2, &b);
    ScreenToClient(w, &b);              /* both in w's coordinates */
    hwmouse(w, a.x, a.y, 0);
    pump(100);
    hwmouse(w, a.x, a.y, 2);
    pump(150);
    for (i = 1; i <= 8; i++) {
        hwmouse(w, a.x + (b.x - a.x) * i / 8, a.y + (b.y - a.y) * i / 8, 0);
        pump(150);
    }
    hwmouse(w, b.x, b.y, 4);
}

BOOL CALLBACK __export find_top(HWND w, LPARAM l)
{
    char cls[64], title[80];

    if (!IsWindowVisible(w))
        return TRUE;
    GetClassName(w, cls, sizeof cls);
    if (lstrcmpi(cls, findClass) == 0 &&
        (!findTitle[0] || (GetWindowText(w, title, sizeof title) && !strnicmp(title, findTitle, strlen(findTitle))))) {
        foundWnd = w;
        return FALSE;
    }
    return TRUE;
}

/* The first visible window of that class under parent, at any depth (no
   callback: an EXE's needs MakeProcInstance). */
static HWND find_in(HWND parent, const char *cls)
{
    char name[64];
    HWND c, found;

    for (c = GetWindow(parent, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        if (!IsWindowVisible(c))
            continue;
        GetClassName(c, name, sizeof name);
        if (!lstrcmpi(name, cls))
            return c;
        if ((found = find_in(c, cls)) != NULL)
            return found;
    }
    return NULL;
}

/* The window a <class> or "Parent/Child" names (the top one's title starting
   with findTitle, if set); logged unless quiet. */
static HWND find_window_q(const char *path, BOOL quiet)
{
    char cls[64], *slash;

    lstrcpyn(cls, path, sizeof cls);
    if ((slash = strchr(cls, '/')) != NULL) {
        *slash = '\0';
        lstrcpy(findClass, cls);
        foundWnd = NULL;
        EnumWindows((WNDENUMPROC)find_top, 0);
        if (foundWnd)
            foundWnd = find_in(foundWnd, slash + 1);
        if (quiet) {
        } else if (!foundWnd) {
            fprintf(out, "  no visible window %s\n", path);
        } else {
            char parentClass[64];
            GetClassName(GetParent(foundWnd), parentClass, sizeof parentClass);
            fprintf(out, "  -> %04X in a %s\n", (UINT)foundWnd, parentClass);
        }
        return foundWnd;
    }
    lstrcpy(findClass, cls);
    foundWnd = NULL;
    EnumWindows((WNDENUMPROC)find_top, 0);
    if (!foundWnd && !quiet)
        fprintf(out, "  no visible window of class %s\n", cls);
    return foundWnd;
}

static HWND find_window(const char *path)
{
    return find_window_q(path, FALSE);
}

/* An expect's <target>: a <class>, or <class>@<id> for that control of it. */
static HWND find_target(const char *target)
{
    char path[64], *at;
    HWND w;

    lstrcpyn(path, target, sizeof path);
    if ((at = strchr(path, '@')) != NULL)
        *at = '\0';
    w = find_window_q(path, TRUE);
    return w && at ? GetDlgItem(w, atoi(at + 1)) : w;
}

/* A checksum of the screen, a strip at a time in the display's own format. */
static DWORD screen_sum(void)
{
    HDC sdc = GetDC(NULL), mdc = CreateCompatibleDC(sdc);
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN), y;
    HBITMAP bmp = CreateCompatibleBitmap(sdc, w, 16), old = SelectObject(mdc, bmp);
    BITMAP bm;
    HGLOBAL mem;
    WORD FAR *p;
    WORD s1 = 0, s2 = 0;
    UINT i, n;

    GetObject(bmp, sizeof bm, &bm);
    mem = GlobalAlloc(GMEM_MOVEABLE, (DWORD)bm.bmWidthBytes * bm.bmHeight * bm.bmPlanes);
    p = (WORD FAR *)GlobalLock(mem);
    for (y = 0; y < h; y += 16) {
        BitBlt(mdc, 0, 0, w, 16, sdc, 0, y, SRCCOPY);
        n = (UINT)GetBitmapBits(bmp, GlobalSize(mem), p) / 2;
        for (i = 0; i < n; i++) {
            s1 += p[i];
            s2 += s1;
        }
    }
    GlobalUnlock(mem);
    GlobalFree(mem);
    SelectObject(mdc, old);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(NULL, sdc);
    return MAKELONG(s1, s2);
}

/* Until the screen has stayed the same for quiet ms, at most most ms. */
static void settle(DWORD most, DWORD quiet)
{
    DWORD start = GetTickCount(), since = start, sum = screen_sum(), now, s;

    for (;;) {
        pump(100);
        now = GetTickCount();
        if ((s = screen_sum()) != sum) {
            sum = s;
            since = now;
        } else if (now - since >= quiet) {
            fprintf(out, "  settled after %lu ms\n", now - start);
            return;
        }
        if (now - start >= most) {
            fprintf(out, "  still changing after %lu ms\n", now - start);
            return;
        }
    }
}

/* Until that window is there, at most most ms. */
static void waitfor(DWORD most, const char *path, const char *title)
{
    DWORD start = GetTickCount();
    HWND w;

    lstrcpyn(findTitle, title, sizeof findTitle);
    while ((w = find_window_q(path, TRUE)) == NULL && GetTickCount() - start < most)
        pump(50);
    findTitle[0] = '\0';
    if (w)
        fprintf(out, "  -> %04X after %lu ms\n", (WORD)w, GetTickCount() - start);
    else
        fail("no %s %s after %lu ms", path, title, GetTickCount() - start);
}

/* The popup holding the item whose text (& left out, up to a tab) is text,
   at any depth; *pos its position there. */
static HMENU menu_item(HMENU m, const char *text, int *pos)
{
    char item[64], *s, *d;
    int i, n = GetMenuItemCount(m);
    HMENU found;

    for (i = 0; i < n; i++) {
        if (GetSubMenu(m, i)) {
            if ((found = menu_item(GetSubMenu(m, i), text, pos)) != NULL)
                return found;
            continue;
        }
        GetMenuString(m, i, item, sizeof item, MF_BYPOSITION);
        for (s = d = item; *s && *s != '\t'; s++)
            if (*s != '&')
                *d++ = *s;
        *d = '\0';
        if (*item && !strcmp(item, text)) {
            *pos = i;
            return m;
        }
    }
    return NULL;
}

/* Lets w ready each of its menus as when it opens (WM_INITMENUPOPUP). */
static void menu_init(HWND w, HMENU m)
{
    int i, n = GetMenuItemCount(m);

    for (i = 0; i < n; i++)
        if (GetSubMenu(m, i)) {
            SendMessage(w, WM_INITMENUPOPUP, (WPARAM)GetSubMenu(m, i), MAKELPARAM(i, FALSE));
            menu_init(w, GetSubMenu(m, i));
        }
}

static void expect(const char *arg)
{
    char what[16], target[64], rest[128], text[128];
    const char *p = arg;
    HWND w;
    int n;

    what[0] = target[0] = '\0';
    sscanf(p, "%15s %63s", what, target);
    for (n = 0; n < 2; n++) {           /* rest: after the first two words */
        while (*p == ' ') p++;
        while (*p && *p != ' ') p++;
    }
    while (*p == ' ') p++;
    lstrcpyn(rest, p, sizeof rest);

    w = find_target(target);
    if (!strcmp(what, "nowindow")) {
        if (w)
            fail("%s is there", target);
        return;
    }
    if (!w) {
        fail("no %s", target);
        return;
    }
    if (!strcmp(what, "window")) {
    } else if (!strcmp(what, "text")) {
        GetClassName(w, text, sizeof text);
        if (!lstrcmpi(text, "ListBox") || !lstrcmpi(text, "ComboBox")) {
            BOOL combo = !lstrcmpi(text, "ComboBox");
            int sel = (int)SendMessage(w, combo ? CB_GETCURSEL : LB_GETCURSEL, 0, 0L);
            text[0] = '\0';
            if (sel >= 0)
                SendMessage(w, combo ? CB_GETLBTEXT : LB_GETTEXT, sel, (LPARAM)(LPSTR)text);
            if (combo && sel < 0)
                GetWindowText(w, text, sizeof text);
        } else {
            GetWindowText(w, text, sizeof text);
        }
        if (strcmp(text, rest))
            fail("%s says '%s', not '%s'", target, text, rest);
    } else if (!strcmp(what, "count")) {
        GetClassName(w, text, sizeof text);
        n = (int)SendMessage(w, lstrcmpi(text, "ComboBox") ? LB_GETCOUNT : CB_GETCOUNT, 0, 0L);
        if (n != atoi(rest))
            fail("%s has %d entries, not %s", target, n, rest);
    } else if (!strcmp(what, "checked")) {
        n = (int)SendMessage(w, BM_GETCHECK, 0, 0L);
        if (n != atoi(rest))
            fail("%s is checked %d, not %s", target, n, rest);
    } else if (!strcmp(what, "enabled") || !strcmp(what, "disabled")) {
        if (!IsWindowEnabled(w) != (what[0] == 'd'))
            fail("%s is %s", target, IsWindowEnabled(w) ? "enabled" : "disabled");
    } else if (!strcmp(what, "focus")) {
        if (GetFocus() != w)
            fail("%s has not the focus (%04X has)", target, (WORD)GetFocus());
    } else if (!strcmp(what, "menu")) {
        static const char *states[] = { "checked", "unchecked", "grayed", "enabled" };
        HMENU bar = GetMenu(w), m;
        char *last = strrchr(rest, ' ');
        int state = -1, pos;
        UINT flags;

        for (n = 0; last && n < 4; n++)
            if (!strcmp(last + 1, states[n])) {
                state = n;
                *last = '\0';
            }
        if (!bar) {
            fail("%s has no menu", target);
            return;
        }
        SendMessage(w, WM_INITMENU, (WPARAM)bar, 0L);
        menu_init(w, bar);
        if ((m = menu_item(bar, rest, &pos)) == NULL) {
            fail("%s's menu has no item '%s'", target, rest);
            return;
        }
        flags = GetMenuState(m, pos, MF_BYPOSITION);
        if ((state == 0 && !(flags & MF_CHECKED)) || (state == 1 && (flags & MF_CHECKED)) ||
            (state == 2 && !(flags & MF_GRAYED)) || (state == 3 && (flags & (MF_GRAYED | MF_DISABLED))))
            fail("'%s' is %s%s", rest, flags & MF_CHECKED ? "checked, " : "",
                 flags & (MF_GRAYED | MF_DISABLED) ? "grayed" : "enabled");
    } else {
        fail("expect %s?", what);
    }
}

/* w and its controls, at any depth, to be repainted whole; then painted. */
static void repaint(HWND w)
{
    HWND c;

    InvalidateRect(w, NULL, TRUE);
    UpdateWindow(w);
    for (c = GetWindow(w, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        repaint(c);
}

/* The blocks in use in GDI's and USER's heaps (ToolHelp), [0] GDI's and
   [1] USER's, all and by ToolHelp's type of block (LT_GDI_*, LT_USER_*). */
#define HEAP_TYPES 40
typedef struct { WORD all, type[HEAP_TYPES]; } HEAPCOUNT;

static void heap_blocks(HEAPCOUNT *c)
{
    SYSHEAPINFO si;
    LOCALENTRY le;
    int i;

    memset(c, 0, 2 * sizeof *c);
    si.dwSize = sizeof si;
    if (!SystemHeapInfo(&si))
        return;
    for (i = 0; i < 2; i++) {
        BOOL more;
        le.dwSize = sizeof le;
        for (more = LocalFirst(&le, i ? si.hUserSegment : si.hGDISegment); more; more = LocalNext(&le))
            if (!(le.wFlags & LF_FREE)) {
                c[i].all++;
                c[i].type[le.wType < HEAP_TYPES ? le.wType : 0]++;
            }
    }
}

/* "gdi used n bytes in b blocks, free f, segment s: t1 bytes/blocks ...":
   GDI's and USER's heaps by ToolHelp's types of block. */
/* One local heap: bytes in use, free, the segment's size, by ToolHelp's types. */
static void heap_one(FILE *out, const char *name, HGLOBAL seg)
{
    LOCALENTRY le;
    DWORD bytes[HEAP_TYPES], used = 0, unused = 0;
    WORD blocks[HEAP_TYPES], n = 0;
    BOOL more;
    int t;

    memset(bytes, 0, sizeof bytes);
    memset(blocks, 0, sizeof blocks);
    le.dwSize = sizeof le;
    for (more = LocalFirst(&le, seg); more; more = LocalNext(&le)) {
        if (le.wFlags & LF_FREE) {
            unused += le.wSize;
            continue;
        }
        t = le.wType < HEAP_TYPES ? le.wType : 0;
        bytes[t] += le.wSize;
        blocks[t]++;
        used += le.wSize;
        n++;
    }
    fprintf(out, "  %s %04X used %lu bytes in %u blocks, free %lu, segment %lu:", name, (WORD)seg, used, n, unused,
            GlobalSize(seg));
    for (t = 0; t < HEAP_TYPES; t++)
        if (blocks[t])
            fprintf(out, " t%d %lu/%u", t, bytes[t], blocks[t]);
    fprintf(out, "\n");
}

/* Every local heap of USER and GDI (USER has three: its own, menus', and
   the text of menus and windows), found in the global heap. */
static void heap_use(FILE *out)
{
    HMODULE user = GetModuleHandle("USER"), gdi = GetModuleHandle("GDI");
    static GLOBALENTRY ge;
    BOOL more;

    HMODULE userx = GetModuleHandle("USERX");
    HMODULE gdix = GetModuleHandle("GDIX");
    DWORD (FAR PASCAL *status)(void);
    DWORD s;

    ge.dwSize = sizeof ge;
    for (more = GlobalFirst(&ge, GLOBAL_ALL); more; more = GlobalNext(&ge, GLOBAL_ALL))
        if (ge.wHeapPresent && (ge.hOwner == user || ge.hOwner == gdi))
            heap_one(out, ge.hOwner == user ? "user" : "gdi", ge.hBlock);
    /* the resource extender's USER driver: menus it keeps, bytes they take */
    if (userx && (status = (DWORD (FAR PASCAL *)(void))GetProcAddress(userx, "USERXSTATUS")) != NULL) {
        s = status();
        fprintf(out, "  userx %u menus saved, %u bytes\n", LOWORD(s), HIWORD(s));
    }
    /* and its GDI driver: brushes it keeps, bytes they take */
    if (gdix && (status = (DWORD (FAR PASCAL *)(void))GetProcAddress(gdix, "GDIXSTATUS")) != NULL) {
        s = status();
        fprintf(out, "  gdix %u brushes kept, %u bytes\n", LOWORD(s), HIWORD(s));
    }
}

/* A sum of the shape of one of Windows' own cursors (its header and masks). */
static unsigned shape_sum(LPCSTR id)
{
    HCURSOR c = LoadCursor(NULL, id);
    BYTE FAR *p = c ? GlobalLock((HGLOBAL)c) : NULL;
    unsigned sum = 0;
    int i;

    if (!p)
        return 0;
    for (i = 0; i < 268; i++)
        sum = sum * 31 + p[i];
    GlobalUnlock((HGLOBAL)c);
    return sum;
}

/* "gdi +1 (type 3 +1), user +0": what grew since mark, the types that did. */
static void heap_growth(const HEAPCOUNT *now, const HEAPCOUNT *mark, char *text)
{
    int i, t;

    text[0] = '\0';
    for (i = 0; i < 2; i++) {
        sprintf(text + strlen(text), "%s%s %+d", i ? ", " : "", i ? "user" : "gdi", now[i].all - mark[i].all);
        for (t = 0; t < HEAP_TYPES; t++)
            if (now[i].type[t] != mark[i].type[t])
                sprintf(text + strlen(text), " (type %d %+d)", t, now[i].type[t] - mark[i].type[t]);
    }
}

BOOL CALLBACK __export find_titled(HWND w, LPARAM l)
{
    char title[80];

    if (IsWindowVisible(w) && GetWindowText(w, title, sizeof title) &&
        !strncmp(title, goneTitle, strlen(goneTitle))) {
        titled = TRUE;
        return FALSE;
    }
    return TRUE;
}

BOOL CALLBACK __export log_top(HWND w, LPARAM l)
{
    char cls[64], title[80];
    POINT o = { 0, 0 };
    RECT r, c;

    if (!IsWindowVisible(w) && !l)
        return TRUE;
    GetClassName(w, cls, sizeof cls);
    GetWindowText(w, title, sizeof title);
    GetWindowRect(w, &r);
    GetClientRect(w, &c);
    ClientToScreen(w, &o);
    fprintf(out, "  %04X %-20s (%d,%d)-(%d,%d) client (%d,%d) %dx%d style %08lX parent %04X owner %04X '%s'\n",
            w, cls, r.left, r.top, r.right, r.bottom, o.x, o.y, c.right, c.bottom, GetWindowLong(w, GWL_STYLE),
            GetParent(w), GetWindow(w, GW_OWNER), title);
    return TRUE;
}

/* The deepest child of top under its client point *p; *p becomes relative to it. */
/* The window a real click there would reach: visible children, top first,
   passing over those that answer HTTRANSPARENT (as group boxes do). */
static HWND child_at(HWND top, POINT *p)
{
    HWND w = top, child;
    RECT r;

    ClientToScreen(top, p);
    for (;;) {
        for (child = GetWindow(w, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            GetWindowRect(child, &r);
            if (IsWindowVisible(child) && PtInRect(&r, *p) &&
                SendMessage(child, WM_NCHITTEST, 0, MAKELPARAM(p->x, p->y)) != HTTRANSPARENT)
                break;
        }
        if (child == NULL)
            break;
        w = child;
    }
    ScreenToClient(w, p);
    fprintf(out, "  -> %04X id %d\n", (WORD)w, GetDlgCtrlID(w));
    return w;
}

/* Posts a click to the window under the point, like a real click; the real
   cursor goes there too, as controls that capture the mouse track it. */
static void click(HWND top, int x, int y, BOOL right, BOOL twice)
{
    POINT p;
    HWND w;
    UINT down = right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN;
    UINT up = right ? WM_RBUTTONUP : WM_LBUTTONUP;
    WPARAM keys = right ? MK_RBUTTON : MK_LBUTTON;

    p.x = x; p.y = y;
    ClientToScreen(top, &p);
    SetCursorPos(p.x, p.y);
    p.x = x; p.y = y;
    w = child_at(top, &p);
    PostMessage(w, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
    PostMessage(w, down, keys, MAKELPARAM(p.x, p.y));
    pump(100);
    PostMessage(w, up, 0, MAKELPARAM(p.x, p.y));
    if (twice) {
        pump(50);
        PostMessage(w, WM_LBUTTONDBLCLK, keys, MAKELPARAM(p.x, p.y));
        PostMessage(w, up, 0, MAKELPARAM(p.x, p.y));
    }
}

/* Presses on (x,y) of top, moves the cursor in steps to (x2,y2) of top2
   and lets go; the button messages go to the window under the start. */
static void drag(HWND top, int x, int y, HWND top2, int x2, int y2, BOOL right)
{
    POINT a, b, p;
    HWND w;
    int i;

    a.x = x; a.y = y;
    b.x = x2; b.y = y2;
    ClientToScreen(top2, &b);
    w = child_at(top, &a);              /* a: relative to w */
    p = a;
    ClientToScreen(w, &p);
    SetCursorPos(p.x, p.y);
    PostMessage(w, right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, right ? MK_RBUTTON : MK_LBUTTON,
                MAKELPARAM(a.x, a.y));
    pump(100);
    for (i = 1; i <= 8; i++) {
        POINT c;
        c.x = p.x + (b.x - p.x) * i / 8;
        c.y = p.y + (b.y - p.y) * i / 8;
        SetCursorPos(c.x, c.y);
        ScreenToClient(w, &c);
        PostMessage(w, WM_MOUSEMOVE, right ? MK_RBUTTON : MK_LBUTTON, MAKELPARAM(c.x, c.y));
        pump(60);
    }
    PostMessage(w, right ? WM_RBUTTONUP : WM_LBUTTONUP, 0, 0L);
}

/* Saves the screen (or what of it win covers) as a bottom-up 24-bit BMP, a
   few scanlines at a time. */
static void snap(const char *file, HWND win)
{
    HDC sdc = GetDC(NULL), mdc = CreateCompatibleDC(sdc);
    RECT r, screen;
    int w, h;
    HBITMAP bmp, old;
    DWORD stride;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER bi;
    HGLOBAL mem;
    BYTE huge *bits;
    int f, y, chunk;

    SetRect(&screen, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    r = screen;
    if (win) {
        GetWindowRect(win, &r);
        IntersectRect(&r, &r, &screen);
    }
    w = r.right - r.left;
    h = r.bottom - r.top;
    bmp = CreateCompatibleBitmap(sdc, w, h);
    stride = ((DWORD)w * 3 + 3) & ~3L;
    chunk = (int)(32000L / stride);
    old = SelectObject(mdc, bmp);
    BitBlt(mdc, 0, 0, w, h, sdc, r.left, r.top, SRCCOPY);
    SelectObject(mdc, old);

    memset(&bi, 0, sizeof bi);
    bi.biSize = sizeof bi;
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    memset(&fh, 0, sizeof fh);
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof fh + sizeof bi;
    fh.bfSize = fh.bfOffBits + stride * h;

    f = _lcreat(file, 0);
    _lwrite(f, (LPCSTR)&fh, sizeof fh);
    _lwrite(f, (LPCSTR)&bi, sizeof bi);
    mem = GlobalAlloc(GMEM_MOVEABLE, stride * chunk + 64);
    bits = (BYTE huge *)GlobalLock(mem);
    for (y = 0; y < h; y += chunk) {
        int n = (h - y < chunk) ? h - y : chunk;
        BITMAPINFOHEADER b2 = bi;
        GetDIBits(sdc, bmp, y, n, bits, (BITMAPINFO FAR *)&b2, DIB_RGB_COLORS);
        _lwrite(f, (LPCSTR)bits, (UINT)(stride * n));
    }
    _lclose(f);
    GlobalUnlock(mem);
    GlobalFree(mem);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(NULL, sdc);
}

void facts_write(const char *file, HWND win);   /* facts.c */

/* A screenshot's facts: the same name, .TXT. */
static void snap_facts(const char *bmp, HWND win)
{
    char txt[164], *dot;

    lstrcpyn(txt, bmp, 160);
    if ((dot = strrchr(txt, '.')) != NULL)
        *dot = '\0';
    lstrcat(txt, ".TXT");
    facts_write(txt, win);
}

HDDEDATA CALLBACK __export dde_cb(UINT type, UINT fmt, HCONV conv, HSZ s1, HSZ s2,
                                 HDDEDATA data, DWORD d1, DWORD d2)
{
    return NULL;
}

static void dde(const char *arg, BOOL request)
{
    char svc[32], topic[64], rest[160];
    HSZ hs, ht, hi;
    HCONV conv;
    HDDEDATA h;
    DWORD res, n;
    LPBYTE p;

    if (sscanf(arg, "%31s %63s %159[^\n]", svc, topic, rest) != 3)
        return;
    if (!ddeInst && DdeInitialize(&ddeInst, (PFNCALLBACK)MakeProcInstance((FARPROC)dde_cb, inst),
                                  APPCMD_CLIENTONLY, 0L)) {
        fprintf(out, "  -> DdeInitialize failed\n");
        ddeInst = 0;
        return;
    }
    hs = DdeCreateStringHandle(ddeInst, svc, CP_WINANSI);
    ht = DdeCreateStringHandle(ddeInst, topic, CP_WINANSI);
    conv = DdeConnect(ddeInst, hs, ht, NULL);
    if (!conv) {
        fprintf(out, "  -> no conversation (%u)\n", DdeGetLastError(ddeInst));
    } else if (request) {
        hi = DdeCreateStringHandle(ddeInst, rest, CP_WINANSI);
        h = DdeClientTransaction(NULL, 0, conv, hi, CF_TEXT, XTYP_REQUEST, 5000, &res);
        if (h) {
            static BYTE buf[2048];      /* (ddeml.h has DdeAccessData return a near pointer) */
            p = buf;
            n = DdeGetData(h, buf, sizeof buf, 0);
            fprintf(out, "  -> %lu bytes: ", n);
            for (res = 0; res < n; res++)       /* line ends as |, others in hex */
                if (p[res] == '\n')
                    fputc('|', out);
                else if (p[res] >= ' ' && p[res] < 127)
                    fputc(p[res], out);
                else if (p[res] != '\r')
                    fprintf(out, "<%02X>", p[res]);
            fputc('\n', out);
            DdeFreeDataHandle(h);
        } else {
            fprintf(out, "  -> request failed (%u)\n", DdeGetLastError(ddeInst));
        }
        DdeFreeStringHandle(ddeInst, hi);
    } else {
        h = DdeClientTransaction((LPBYTE)rest, strlen(rest) + 1, conv, NULL, 0, XTYP_EXECUTE, 5000, &res);
        fprintf(out, "  -> %s (%u)\n", h ? "ack" : "failed", h ? 0 : DdeGetLastError(ddeInst));
    }
    if (conv)
        DdeDisconnect(conv);
    DdeFreeStringHandle(ddeInst, hs);
    DdeFreeStringHandle(ddeInst, ht);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int show)
{
    char line[200], verb[16], arg[160], cls[64], cls2[64], r[4];
    FILE *script;
    UINT calmiraMsg = RegisterWindowMessage("Calmira Registered Message");
    int a, b, c, d;
    HWND w, w2;

    inst = hInst;
    out = fopen("T:\\UIDRIVE.LOG", "w");
    script = fopen("T:\\UIDRIVE.TXT", "r");
    if (!out || !script) return 1;
    {
        struct dosdate_t dd;
        struct dostime_t dt;
        _dos_getdate(&dd);
        _dos_gettime(&dt);
        clockStart = dt;
        clockSet = GetTickCount();
        fprintf(out, "DOS clock %04u-%02u-%02u %02u:%02u:%02u\n", dd.year, dd.month, dd.day, dt.hour, dt.minute,
                dt.second);
    }
    myTask = GetCurrentTask();
    SetCaretBlinkTime(65535);           /* a caret stays as it is shown: the same in each screenshot */
    NotifyRegister(NULL, (LPFNNOTIFYCALLBACK)MakeProcInstance((FARPROC)notify, hInst), NF_NORMAL);
    InterruptRegister(NULL, (FARPROC)fault_cb);

    while (fgets(line, sizeof line, script)) {
        line[strcspn(line, "\r\n")] = '\0';
        arg[0] = '\0';
        if (sscanf(line, "%15s %159[^\n]", verb, arg) < 1 || verb[0] == '#')
            continue;
        fprintf(out, "%6lu %s\n", GetTickCount(), line);
        fflush(out);
        if (!strcmp(verb, "run")) {
            fprintf(out, "  -> %u\n", WinExec(arg, SW_SHOWNORMAL));
        } else if (!strcmp(verb, "wait")) {
            pump(atol(arg));
        } else if (!strcmp(verb, "spin")) {
            pump_as(atol(arg), TRUE);
        } else if (!strcmp(verb, "click") || !strcmp(verb, "rclick") || !strcmp(verb, "dclick")) {
            if (sscanf(arg, "%63s %d %d", cls, &a, &b) == 3 && (w = find_window(cls)) != NULL)
                click(w, a, b, verb[0] == 'r', verb[0] == 'd');
        } else if (!strcmp(verb, "drag")) {
            r[0] = '\0';
            if (sscanf(arg, "%63s %d %d %63s %d %d %3s", cls, &a, &b, cls2, &c, &d, r) >= 6 &&
                (w = find_window(cls)) != NULL && (w2 = find_window(cls2)) != NULL)
                drag(w, a, b, w2, c, d, r[0] == 'r');
        } else if (!strcmp(verb, "char")) {
            PostMessage(GetFocus(), WM_CHAR, atoi(arg), 1L);
        } else if (!strcmp(verb, "type")) {     /* characters to the focused window */
            char *p;
            for (p = arg; *p; p++) {
                PostMessage(GetFocus(), WM_CHAR, (BYTE)*p, 1L);
                pump(40);
            }
        } else if (!strcmp(verb, "key")) {
            if (sscanf(arg, "%63s %d", cls, &a) == 2 && (w = find_window(cls)) != NULL) {
                PostMessage(w, WM_KEYDOWN, a, 1L);
                PostMessage(w, WM_KEYUP, a, 0xC0000001L);
            }
        } else if (!strcmp(verb, "command")) {
            if (sscanf(arg, "%63s %d", cls, &a) == 2 && (w = find_window(cls)) != NULL)
                PostMessage(w, WM_COMMAND, a, 0L);
        } else if (!strcmp(verb, "hwkey")) {
            r[0] = '\0';
            if (sscanf(arg, "%d %3s", &a, r) == 2)
                hwkey(a, r[0] == 'u');
        } else if (!strcmp(verb, "hwmove") || !strcmp(verb, "hwclick")) {
            r[0] = '\0';
            if (sscanf(arg, "%63s %d %d %3s", cls, &a, &b, r) >= 3 && (w = find_window(cls)) != NULL) {
                hwmouse(w, a, b, 0);
                if (verb[2] == 'c') {
                    pump(50);
                    hwmouse(w, a, b, r[0] == 'r' ? 8 : 2);
                    pump(100);
                    hwmouse(w, a, b, r[0] == 'r' ? 16 : 4);
                }
            }
        } else if (!strcmp(verb, "cursorstats")) { /* the debug VBESVGA.DRV's counters: cursorstats <hex offset> <words> */
            unsigned at; int words;
            if (sscanf(arg, "%x %d", &at, &words) == 2) {
                char path[144];
                HINSTANCE inst;
                GetModuleFileName(GetModuleHandle("DISPLAY"), path, sizeof path);
                inst = LoadLibrary(path);
                if ((UINT)inst >= 32) {
                    UINT sel = SELECTOROF(GlobalLock((HGLOBAL)inst));
                    for (a = 0; a < words; a++)
                        fprintf(out, " %u", *(WORD FAR *)MAKELP(sel, at + 2 * a));
                    fprintf(out, "\n");
                    GlobalUnlock((HGLOBAL)inst);
                    FreeLibrary(inst);
                }
            }
        } else if (!strcmp(verb, "hwjitter")) {    /* moved around x,y: rate times a second, for ms */
            int rate, ms;
            if (sscanf(arg, "%63s %d %d %d %d", cls, &a, &b, &rate, &ms) == 5 && (w = find_window(cls)) != NULL) {
                DWORD start = timeGetTime(), n = 0;
                while (timeGetTime() - start < (DWORD)ms) {
                    MSG m;
                    hwmouse(w, a + (int)(n & 7), b + (int)((n >> 1) & 3), 0);
                    n++;
                    while (timeGetTime() - start < n * 1000L / rate)
                        if (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) {  /* other programs get their messages */
                            TranslateMessage(&m);
                            DispatchMessage(&m);
                        }
                }
                fprintf(out, "  %lu moves\n", n);
            }
        } else if (!strcmp(verb, "drawspin")) {    /* white drawn on the screen: rate times a second, for ms */
            int dx, dy, dw, dh, rate, ms;
            if (sscanf(arg, "%d %d %d %d %d %d", &dx, &dy, &dw, &dh, &rate, &ms) == 6) {
                HDC screen = GetDC(NULL);
                DWORD start = timeGetTime(), n = 0;
                while (timeGetTime() - start < (DWORD)ms) {
                    PatBlt(screen, dx, dy, dw, dh, WHITENESS);
                    n++;
                    while (timeGetTime() - start < n * 1000L / rate)
                        ;
                }
                ReleaseDC(NULL, screen);
                fprintf(out, "  %lu draws\n", n);
            }
        } else if (!strcmp(verb, "items")) {       /* a combo box's or list box's entries */
            if (sscanf(arg, "%63s %d", cls, &a) >= 1 && (w = find_window(cls)) != NULL) {
                char item[128];
                BOOL combo;
                if (sscanf(arg, "%*s %d", &a) == 1)
                    w = GetDlgItem(w, a);       /* by its id in the window found */
                GetClassName(w, item, sizeof item);
                combo = !lstrcmpi(item, "ComboBox");
                b = (int)SendMessage(w, combo ? CB_GETCOUNT : LB_GETCOUNT, 0, 0L);
                for (a = 0; a < b; a++) {
                    SendMessage(w, combo ? CB_GETLBTEXT : LB_GETTEXT, a, (LPARAM)(LPSTR)item);
                    fprintf(out, "  %d: %s\n", a, item);
                }
            }
        } else if (!strcmp(verb, "children")) {    /* a window's controls: id, class, text */
            if (sscanf(arg, "%63s", cls) == 1 && (w = find_window(cls)) != NULL) {
                char item[64], text[64];
                HWND c;
                for (c = GetWindow(w, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
                    GetClassName(c, item, sizeof item);
                    GetWindowText(c, text, sizeof text);
                    fprintf(out, "  %5d %-12s %s'%s'\n", GetDlgCtrlID(c), item, IsWindowEnabled(c) ? "" : "(off) ", text);
                }
            }
        } else if (!strcmp(verb, "selectid")) {    /* a combo or list box entry chosen, as by the user */
            if (sscanf(arg, "%63s %d %d", cls, &a, &b) == 3 && (w = find_window(cls)) != NULL) {
                HWND c = GetDlgItem(w, a);
                char item[16];
                BOOL combo;
                GetClassName(c, item, sizeof item);
                combo = !lstrcmpi(item, "ComboBox");
                SendMessage(c, combo ? CB_SETCURSEL : LB_SETCURSEL, b, 0L);
                SendMessage(w, WM_COMMAND, a, MAKELPARAM(c, combo ? CBN_SELCHANGE : LBN_SELCHANGE));
            }
        } else if (!strcmp(verb, "hwbutton")) {
            if (sscanf(arg, "%63s %d %d %3s", cls, &a, &b, r) == 4 && (w = find_window(cls)) != NULL) {
                hwmouse(w, a, b, 0);
                pump(50);
                hwmouse(w, a, b, r[0] == 'u' ? 4 : 2);
            }
        } else if (!strcmp(verb, "hwdrag")) {
            if (sscanf(arg, "%63s %d %d %63s %d %d", cls, &a, &b, cls2, &c, &d) == 6 &&
                (w = find_window(cls)) != NULL && (w2 = find_window(cls2)) != NULL)
                hwdrag(w, a, b, w2, c, d);
        } else if (!strcmp(verb, "move")) {
            if (sscanf(arg, "%63s %d %d", cls, &a, &b) == 3 && (w = find_window(cls)) != NULL) {
                POINT p;
                p.x = a; p.y = b;
                ClientToScreen(w, &p);
                SetCursorPos(p.x, p.y);
                PostMessage(w, WM_MOUSEMOVE, 0, MAKELPARAM(a, b));
            }
        } else if (!strcmp(verb, "mdi")) {
            r[0] = '\0';
            if (sscanf(arg, "%63s %3s", cls, r) == 2 && (w = find_window(cls)) != NULL) {
                for (w2 = GetWindow(w, GW_CHILD); w2; w2 = GetWindow(w2, GW_HWNDNEXT)) {
                    GetClassName(w2, cls2, sizeof cls2);
                    if (!stricmp(cls2, "MDIClient"))
                        break;
                }
                if (w2)
                    SendMessage(w2, r[0] == 'm' ? WM_MDIMAXIMIZE : r[0] == 'n' ? WM_MDINEXT : WM_MDIRESTORE,
                                LOWORD(SendMessage(w2, WM_MDIGETACTIVE, 0, 0L)), 0L);
                fprintf(out, "  -> %04X\n", (WORD)w2);
            }
        } else if (!strcmp(verb, "menu")) {
            if (sscanf(arg, "%63s %n", cls, &a) >= 1 && (w = find_window(cls)) != NULL &&
                (b = menu_id(GetMenu(w), arg + a)) != 0)
                PostMessage(w, WM_COMMAND, b, 0L);
            fprintf(out, "  -> %d\n", w ? b : -1);
        } else if (!strcmp(verb, "close")) {
            if ((w = find_window(arg)) != NULL)
                PostMessage(w, WM_CLOSE, 0, 0L);
        } else if (!strcmp(verb, "flash")) {
            if ((w = find_window(arg)) != NULL)
                FlashWindow(w, TRUE);
        } else if (!strcmp(verb, "show")) {
            char cls[64];
            if (sscanf(arg, "%63s %d", cls, &a) == 2 && (w = find_window(cls)) != NULL)
                ShowWindow(w, a);
        } else if (!strcmp(verb, "calmira")) {
            char text[128] = "";
            sscanf(arg, "%d %127[^\n]", &a, text);
            PostMessage(FindWindow("TApplication", "Calmira"), calmiraMsg, a,
                        text[0] ? (LPARAM)GlobalAddAtom(text) : 0L);
        } else if (!strcmp(verb, "dde") || !strcmp(verb, "ddereq")) {
            dde(arg, verb[3] == 'r');
        } else if (!strcmp(verb, "snap")) {
            cls[0] = '\0';
            if (sscanf(arg, "%159s %63s", line, cls) == 2) {
                if ((w = find_window(cls)) != NULL) {
                    snap(line, w);
                    snap_facts(line, w);
                } else {
                    fail("no %s to snap", cls);
                }
            } else {
                snap(arg, NULL);
                snap_facts(arg, NULL);
            }
        } else if (!strcmp(verb, "waitfor")) {
            DWORD most;
            char title[80] = "";
            if (sscanf(arg, "%lu %63s %79[^\n]", &most, cls, title) >= 2)
                waitfor(most, cls, title);
        } else if (!strcmp(verb, "settle")) {
            DWORD most, quiet = 700;
            if (sscanf(arg, "%lu %lu", &most, &quiet) >= 1)
                settle(most, quiet);
        } else if (!strcmp(verb, "expect")) {
            expect(arg);
        } else if (!strcmp(verb, "cpu")) {
            DWORD end = timeGetTime() + atol(arg), rounds = 0;
            volatile WORD x = 0;
            while (timeGetTime() < end) {
                for (b = 0; b < 1000; b++)
                    x += b;
                rounds++;
            }
            fprintf(out, "  cpu %lu rounds\n", rounds);
        } else if (!strcmp(verb, "bench")) {
            if (sscanf(arg, "%d %63s", &a, cls) == 2 && (w = find_window(cls)) != NULL) {
                DWORD t0 = timeGetTime(), t;
                for (b = 0; b < a; b++)
                    repaint(w);
                t = timeGetTime() - t0;
                fprintf(out, "  bench %d in %lu ms: %lu.%lu ms each\n", a, t, t / a, t * 10 / a % 10);
            }
        } else if (!strcmp(verb, "heap")) {
            static HEAPCOUNT mark[2];
            HEAPCOUNT now[2];
            char grew[200];
            heap_blocks(now);
            if (!strncmp(arg, "mark", 4)) {
                memcpy(mark, now, sizeof mark);
                fprintf(out, "  gdi %u blocks, user %u\n", now[0].all, now[1].all);
            } else {
                char what[64] = "";
                a = 0;                  /* "check [n [what]]" */
                sscanf(arg + 5, "%d %63[^\n]", &a, what);
                heap_growth(now, mark, grew);
                fprintf(out, "  %s\n", grew);
                if ((int)(now[0].all - mark[0].all) > a || (int)(now[1].all - mark[1].all) > a)
                    fail("heap blocks grew%s%s: %s", what[0] ? " by " : "", what, grew);
            }
        } else if (!strcmp(verb, "waitgone")) {
            DWORD start = GetTickCount(), most;
            if (sscanf(arg, "%lu %79[^\n]", &most, goneTitle) == 2) {
                do {
                    pump(100);
                    titled = FALSE;
                    EnumWindows((WNDENUMPROC)find_titled, 0);
                } while (titled && GetTickCount() - start < most);
                fprintf(out, "  %s after %lu ms\n", titled ? "still there" : "gone", GetTickCount() - start);
            }
        } else if (!strcmp(verb, "cursors")) {
            fprintf(out, "  arrow %u, ibeam %u, wait %u\n", shape_sum(IDC_ARROW), shape_sum(IDC_IBEAM), shape_sum(IDC_WAIT));
        } else if (!strcmp(verb, "heapuse")) {
            heap_use(out);
        } else if (!strcmp(verb, "mem")) {
            fprintf(out, "  free %lu KB, system %u%%, gdi %u%%, user %u%%\n", GetFreeSpace(0) / 1024,
                    GetFreeSystemResources(GFSR_SYSTEMRESOURCES), GetFreeSystemResources(GFSR_GDIRESOURCES),
                    GetFreeSystemResources(GFSR_USERRESOURCES));
        } else if (!strcmp(verb, "windows")) {
            EnumWindows((WNDENUMPROC)log_top, !strcmp(arg, "all"));
        } else if (!strcmp(verb, "exitwindows")) {
            log_errors();
            fprintf(out, "failures %d\n", failures);
            fclose(out);
            NotifyUnRegister(NULL);
            InterruptUnRegister(NULL);
            ExitWindows(0, 0);
            return 0;
        } else {
            fail("no verb %s", verb);
        }
        pump(50);
        log_errors();
        fclose(out);                    /* on the host now, should the run be killed */
        out = fopen("T:\\UIDRIVE.LOG", "a");
    }
    fprintf(out, "end of script\nfailures %d\n", failures);
    fclose(out);
    NotifyUnRegister(NULL);
    InterruptUnRegister(NULL);
    if (ddeInst)
        DdeUninitialize(ddeInst);
    return 0;
}
