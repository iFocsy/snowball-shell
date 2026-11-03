/*
 * Where everything is on the taskbar, and the taskbar on the screen. Along
 * it, the Start button, Quick Launch's band, the task buttons' band and the
 * tray; hit testing and painting go by these. It goes to any side of the
 * screen (dragged by its empty part) and grows by rows of buttons, or
 * wider at a side (dragged by its inner edge). Taskbar Properties' options
 * (on top, auto hide, clock) place it; what it leaves of the screen is the
 * work area, where Cascade and Tile arrange the windows. The Start menu
 * opens by the Start button, the taskbar slid into view first. Also which
 * task button is at a point of the screen.
 */
#include "internal.h"
#include "taskbarint.h"
#include <stdlib.h>

#define BTN_H       22                  /* a button's height; */
#define ROW_H       24                  /* ... a row of them, with the gap below */
#define START_W     54
#define SIDE_W      104                 /* first width at a side of the screen */
#define SIDE_MIN    (TaskbarIconsOnly ? 2 + 30 + 8 : 2 + START_W + 4)   /* (icons only: an icon wide) */
#define BTN_MAX     160
#define HIDDEN_H    2                   /* what auto hide leaves of the taskbar */

static BOOL onTop, autoHide, hidden;
static int away;                        /* auto hide: ticks the mouse has been elsewhere */
static int edge = TBE_BOTTOM;           /* TBE_*: the side of the screen */
static int rows = 1;                    /* at the top or bottom: rows of buttons */
static int sideWidth = SIDE_W;          /* at a side: its width */
int TaskbarSizing;                      /* SZ_*: the taskbar dragged */
POINT TaskbarGrabbed;

int TaskbarEdge(void) { return edge; }

BOOL TaskbarVertical(void)
{
    return edge == TBE_LEFT || edge == TBE_RIGHT;
}

/* How far it reaches into the screen. */
static int Thickness(void)
{
    return TaskbarVertical() ? sideWidth : TB_H + (rows - 1) * ROW_H;
}

/* The client less the 2-pixel raised edge it shows toward the screen. */
static void Inner(RECT *c)
{
    GetClientRect(g_taskbar, c);
    switch (edge) {
    case TBE_BOTTOM: c->top += 2; break;
    case TBE_TOP:    c->bottom -= 2; break;
    case TBE_LEFT:   c->right -= 2; break;
    case TBE_RIGHT:  c->left += 2; break;
    }
}

static BOOL ShowTray(void)
{
    return TaskbarShowClock || TaskbarNTray;
}

/* As Windows 98's: its edge, 3 pixels, the programs' icons, the time with 7
   each side, its edge. */
static int TrayWidth(void)
{
    return !ShowTray() ? 0 : (TaskbarShowClock ? TaskbarClockW + 18 : 6) + TaskbarNTray * TRAYICON_W;
}

/* Down a side: icons a row, each as many as fit, above the time. */
static int TrayIconsPerRow(int width)
{
    return max(1, (width - 6) / TRAYICON_W);
}

/* At the right, as high as the rows; at a side, at the bottom. */
void TaskbarTrayRect(RECT *r)
{
    int h;

    Inner(r);
    if (TaskbarVertical()) {
        h = (TaskbarShowClock ? BTN_H : 6) + (TaskbarNTray + TrayIconsPerRow(r->right - r->left - 4) - 1) /
            TrayIconsPerRow(r->right - r->left - 4) * TRAYICON_W;
        SetRect(r, r->left + 2, r->bottom - 2 - h, r->right - 2, r->bottom - 2);
    } else {
        SetRect(r, r->right - TrayWidth() - 2, r->top + 2, r->right - 2, r->bottom - 2);
    }
}

static void TrayIconRect(int i, RECT *r)
{
    RECT t;
    int n;

    TaskbarTrayRect(&t);
    if (TaskbarVertical()) {
        n = TrayIconsPerRow(t.right - t.left);
        SetRect(r, t.left + 3 + i % n * TRAYICON_W, t.top + 3 + i / n * TRAYICON_W, 0, 0);
    } else {
        SetRect(r, t.left + 3 + i * TRAYICON_W, t.top + (t.bottom - t.top - 16) / 2, 0, 0);
    }
    r->right = r->left + 16;
    r->bottom = r->top + 16;
}

BOOL TaskbarIconsOnly, TaskbarGrouping;

#define ICON_W      30                  /* an icons-only button; the Start button then its flag */
#define FLAG_W      26

static void StartRect(RECT *r)
{
    Inner(r);
    SetRect(r, r->left + 2, r->top + 2, min(r->left + 2 + (TaskbarIconsOnly ? FLAG_W : START_W), r->right - 2),
            r->top + 2 + BTN_H);
}

/* Each band after the Start button: its etched line, 4 pixels on its grip,
   7 more on its content; along the taskbar: x, down a side: y. Quick
   Launch's band is the first (one row high at the top or bottom). */
static int QuickLaunchStart(void)
{
    RECT r;

    StartRect(&r);
    return TaskbarVertical() ? r.bottom + 2 + 4 : r.right + 2 + 4;
}

void TaskbarQuickLaunchBand(RECT *b)
{
    RECT c;
    int at = QuickLaunchStart();

    Inner(&c);
    if (TaskbarVertical())
        SetRect(b, c.left + 2, at, c.right - 2, at + QuickLaunchLength(TRUE, c.right - c.left - 4));
    else
        SetRect(b, at, c.top + 2, at + QuickLaunchLength(FALSE, 0), c.top + 2 + BTN_H);
}

static BOOL TasksGripped(void)
{
    RECT q;

    TaskbarQuickLaunchBand(&q);
    return q.right > q.left && q.bottom > q.top;
}

/* The task buttons' band: after Quick Launch's (its grip 8 pixels on),
   else 4 pixels after Start; up to 8 pixels before the tray. */
static void TasksBand(RECT *b)
{
    RECT c, q, t;

    Inner(&c);
    TaskbarTrayRect(&t);
    if (TasksGripped())
        TaskbarQuickLaunchBand(&q);
    else
        StartRect(&q);
    if (TaskbarVertical())
        SetRect(b, c.left + 2, q.bottom + (TasksGripped() ? 8 : 4), c.right - 2, ShowTray() ? t.top - 8 : c.bottom - 2);
    else
        SetRect(b, q.right + (TasksGripped() ? 8 : 4), c.top + 2, ShowTray() ? t.left - 8 : c.right, c.bottom - 2);
}

/* Button i (-1 Start): across a row then the next, each as wide as the
   room leaves (Windows 98's widest at most); down a side, one under the
   other, those past the band left out (empty). */
void TaskbarButtonRect(int i, RECT *r)
{
    RECT b;
    int perRow, w, n;

    if (i < 0) {
        StartRect(r);
        return;
    }
    if ((i = TaskbarSlot(i)) < 0) {     /* on its group's button */
        SetRectEmpty(r);
        return;
    }
    TasksBand(&b);
    if (TasksGripped()) {
        if (TaskbarVertical())
            b.top += 7;
        else
            b.left += 7;
    }
    if (TaskbarVertical()) {
        SetRect(r, b.left, b.top + i * ROW_H, b.right, b.top + i * ROW_H + BTN_H);
        if (r->bottom > b.bottom)
            SetRectEmpty(r);
        return;
    }
    n = TaskbarSlots();
    if (TaskbarIconsOnly) {
        w = ICON_W;
        perRow = max(1, (b.right - b.left + 3) / w);
    } else {
        perRow = max(max(1, (b.right - b.left + 3) / BTN_MAX), (n + rows - 1) / rows);
        w = min(BTN_MAX, (b.right - b.left + 3) / perRow);     /* each leaves 3 pixels after it */
    }
    SetRect(r, b.left + i % perRow * w, b.top + i / perRow * ROW_H, b.left + (i % perRow + 1) * w - 3,
            b.top + i / perRow * ROW_H + BTN_H);
}

/* The etched line before a band and before the tray: at x along the
   taskbar, at y down a side. */
static void BandLine(HDC dc, int at)
{
    RECT r, c;

    Inner(&c);
    if (TaskbarVertical())
        SetRect(&r, c.left + 2, at, c.right - 2, at + 1);
    else
        SetRect(&r, at, c.top + 2, at + 1, c.bottom - 2);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNSHADOW));
    OffsetRect(&r, !TaskbarVertical(), TaskbarVertical());
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT));
}

#define BTN_DOWN    0x01                /* pressed */
#define BTN_START   0x02                /* the Start button: its flag, a plain face down */
#define BTN_BOLD    0x04                /* the active task's */
#define BTN_LIT     0x08                /* flashing: the selection's colours */

/* A button (BTN_* state): pressed (the active task's checkered), or lit,
   with its icon and text. */
/* A group's count, white on the selection's colour, at its icon's lower right. */
static void DrawBadge(HDC dc, int x, int y, int count)
{
    char n[4];
    RECT b;
    int w;

    wsprintf(n, count > 9 ? "9+" : "%d", count);
    SelectObject(dc, DrawFont(FALSE));
    w = LOWORD(GetTextExtent(dc, n, lstrlen(n))) + 3;
    SetRect(&b, x + 18 - w, y + 7, x + 18, y + 18);
    DrawSolidRect(dc, &b, GetSysColor(COLOR_HIGHLIGHT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
    DrawText(dc, n, -1, &b, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* text "": the icon in the middle (icons only); count > 1: a group's badge. */
static void DrawButton(HDC dc, RECT *r, HICON icon, LPCSTR text, UINT state, int count)
{
    int ix;
    BOOL down = (state & BTN_DOWN) != 0, start = (state & BTN_START) != 0, flashing = (state & BTN_LIT) != 0;
    RECT t = *r;

    if (flashing && !down) {
        InflateRect(&t, -2, -2);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_HIGHLIGHT));
        t = *r;
    } else if (down && !start) {                       /* active task: checkered face */
        static WORD pat[8] = { 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA };
        HBITMAP b = CreateBitmap(8, 8, 1, 1, pat);
        HBRUSH br = CreatePatternBrush(b), old;
        COLORREF fg = SetTextColor(dc, GetSysColor(COLOR_BTNFACE)), bk = SetBkColor(dc, GetSysColor(COLOR_BTNHIGHLIGHT));
        old = SelectObject(dc, br);
        PatBlt(dc, r->left + 2, r->top + 2, r->right - r->left - 4, r->bottom - r->top - 4, PATCOPY);
        SelectObject(dc, old);
        SetTextColor(dc, fg);
        SetBkColor(dc, bk);
        DeleteObject(br);
        DeleteObject(b);
    } else {
        InflateRect(&t, -2, -2);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNFACE));
        t = *r;
    }
    DrawBevel(dc, r, down);
    OffsetRect(&t, down, down);
    ix = *text ? t.left + 4 : t.left + (t.right - t.left - 16) / 2;
    if (start)
        PaintStripImage(dc, ix, t.top + (t.bottom - t.top - 16) / 2, IDB_STARTFLAG, 0);
    else
        DrawIconImage(dc, ix, t.top + (t.bottom - t.top - 16) / 2, icon, TRUE);
    if (count > 1)
        DrawBadge(dc, ix, t.top + (t.bottom - t.top - 16) / 2, count);
    if (!*text)
        return;
    t.left += 22;
    t.right -= 3;
    SelectObject(dc, DrawFont((state & BTN_BOLD) != 0));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(flashing && !down ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT));
    DrawTextEllipsis(dc, text, &t, DT_LEFT | DT_NOPREFIX);
}

void TaskbarPaint(HWND w)
{
    PAINTSTRUCT ps;
    TEXTMETRIC tm;
    RECT r, c;
    OFFSCREEN o;
    HDC dc;
    HFONT oldFont;
    char text[80];
    int i;

    BeginPaint(w, &ps);
    dc = DrawOffscreenBegin(&o, ps.hdc, &ps.rcPaint);     /* then onto the screen at once */
    SetBkMode(dc, TRANSPARENT);         /* (the clock's too: an icons-only button sets nothing) */
    oldFont = SelectObject(dc, DrawFont(FALSE));
    GetClientRect(w, &c);
    DrawSolidRect(dc, &c, GetSysColor(COLOR_BTNFACE));
    switch (edge) {                     /* its raised edge, toward the screen */
    case TBE_BOTTOM: SetRect(&r, 0, 1, c.right, 2); break;
    case TBE_RIGHT:  SetRect(&r, 1, 0, 2, c.bottom); break;
    case TBE_TOP:    SetRect(&r, 0, c.bottom - 2, c.right, c.bottom - 1); break;
    case TBE_LEFT:   SetRect(&r, c.right - 2, 0, c.right - 1, c.bottom); break;
    }
    if (edge == TBE_BOTTOM || edge == TBE_RIGHT) {      /* light (the face), then white */
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT));
    } else {                                            /* grey, then black */
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNSHADOW));
        OffsetRect(&r, edge == TBE_LEFT, edge == TBE_TOP);
        DrawSolidRect(dc, &r, RGB(0, 0, 0));
    }

    TaskbarButtonRect(-1, &r);
    DrawButton(dc, &r, NULL, TaskbarIconsOnly ? "" : "Start",
               BTN_START | BTN_BOLD | (TaskbarStartOpen || TaskbarPressed == -1 ? BTN_DOWN : 0), 0);
    if (TasksGripped()) {
        TaskbarQuickLaunchBand(&r);
        QuickLaunchPlace(&r, TaskbarVertical());
        BandLine(dc, QuickLaunchStart() - 4);
        QuickLaunchPaint(dc, TaskbarHot <= QL(0) ? QL(0) - TaskbarHot : -1, TaskbarPressed <= QL(0) ? QL(0) - TaskbarPressed : -1);
        TasksBand(&r);
        BandLine(dc, (TaskbarVertical() ? r.top : r.left) - 4);
        QuickLaunchPaintGrip(dc, &r, TaskbarVertical());
    }

    for (i = 0; i < TaskbarNTasks; i++) {
        int j, n = 0, shown = i;        /* a group: down, bold, lit if any of it is; the active one's title */
        BOOL active = FALSE, lit = FALSE;
        if (TaskbarSlot(i) < 0)
            continue;
        for (j = i; j < TaskbarNTasks; j++)
            if (TaskbarLead(j) == i) {
                n++;
                lit |= TaskbarLit[j];
                if (TaskbarTasks[j] == TaskbarActive) {
                    active = TRUE;
                    shown = j;
                }
            }
        TaskbarButtonRect(i, &r);
        DrawButton(dc, &r, TaskbarIcon(TaskbarTasks[i]), TaskbarIconsOnly ? "" : TaskbarTitles[shown],
                   (active || TaskbarPressed == i ? BTN_DOWN : 0) | (active ? BTN_BOLD : 0) | (lit ? BTN_LIT : 0), n);
    }

    if (ShowTray()) {                   /* the programs' icons, then the time at the right
                                           (a side: below them, in the middle), a pixel below the middle */
        TaskbarTrayRect(&r);
        BandLine(dc, (TaskbarVertical() ? r.top : r.left) - 4);
        DrawFrame(dc, &r, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
        for (i = 0; i < TaskbarNTray; i++) {
            TrayIconRect(i, &c);
            DrawIconImage(dc, c.left + (TaskbarPressed == TRAYICON(i)), c.top + (TaskbarPressed == TRAYICON(i)), TaskbarTrayIcons[i], TRUE);
        }
    }
    if (TaskbarShowClock) {
        TaskbarTrayRect(&r);
        if (TaskbarVertical())
            r.top = r.bottom - BTN_H;
        TaskbarClockText(text);
        SelectObject(dc, DrawFont(FALSE));
        GetTextMetrics(dc, &tm);
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        TextOut(dc, TaskbarVertical() ? (r.left + r.right - TaskbarClockW) / 2 : r.right - 8 - TaskbarClockW,
                r.top + (r.bottom - r.top - tm.tmHeight) / 2 + 1, text, lstrlen(text));
    }
    SelectObject(dc, oldFont);
    DrawOffscreenEnd(&o);
    EndPaint(w, &ps);
}

int TaskbarHitButton(int x, int y)
{
    RECT r;
    POINT p;
    int i;

    p.x = x; p.y = y;
    TaskbarButtonRect(-1, &r);
    if (PtInRect(&r, p))
        return -1;
    TaskbarQuickLaunchBand(&r);
    QuickLaunchPlace(&r, TaskbarVertical());
    if ((i = QuickLaunchHitTest(x, y)) >= 0)
        return QL(i);
    for (i = 0; i < TaskbarNTasks; i++) {
        TaskbarButtonRect(i, &r);
        if (PtInRect(&r, p))
            return i;
    }
    for (i = 0; i < TaskbarNTray; i++) {
        TrayIconRect(i, &r);
        if (PtInRect(&r, p))
            return TRAYICON(i);
    }
    TaskbarTrayRect(&r);
    if (ShowTray() && PtInRect(&r, p))
        return TRAY;
    return NONE;
}

/* Where it is on the screen: all of it, or (auto hide) a line at its edge. */
static void BarRect(BOOL show, RECT *r)
{
    int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);
    int t = Thickness(), in = show ? t : HIDDEN_H;

    switch (edge) {
    case TBE_BOTTOM: SetRect(r, 0, cy - in, cx, cy - in + t); break;
    case TBE_TOP:    SetRect(r, 0, in - t, cx, in); break;
    case TBE_LEFT:   SetRect(r, in - t, 0, in, cy); break;
    case TBE_RIGHT:  SetRect(r, cx - in, 0, cx - in + t, cy); break;
    }
}

/* The screen less what the taskbar keeps of it. */
static void Area(int keep, RECT *r)
{
    SetRect(r, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    switch (edge) {
    case TBE_BOTTOM: r->bottom -= keep; break;
    case TBE_TOP:    r->top += keep; break;
    case TBE_LEFT:   r->left += keep; break;
    case TBE_RIGHT:  r->right -= keep; break;
    }
}

/* What windows and the desktop's icons have: all but the taskbar (its
   line, auto hidden). */
void TaskbarWorkArea(RECT *r)
{
    Area(autoHide ? HIDDEN_H : Thickness(), r);
}

/* Auto hide: down to a line at its edge of the screen, or back. */
static void Slide(BOOL show)
{
    RECT r;

    hidden = !show;
    away = 0;
    BarRect(show, &r);
    SetWindowPos(g_taskbar, HWND_TOPMOST, r.left, r.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

/* The pointer at the very edge the taskbar hides at. */
static BOOL AtEdge(POINT p)
{
    switch (edge) {
    case TBE_TOP:   return p.y < HIDDEN_H;
    case TBE_LEFT:  return p.x < HIDDEN_H;
    case TBE_RIGHT: return p.x >= GetSystemMetrics(SM_CXSCREEN) - HIDDEN_H;
    }
    return p.y >= GetSystemMetrics(SM_CYSCREEN) - HIDDEN_H;
}

void TaskbarAutoHide(void)
{
    POINT p;
    RECT r;

    GetCursorPos(&p);
    GetWindowRect(g_taskbar, &r);
    if (hidden) {
        if (AtEdge(p))
            Slide(TRUE);
    } else if (PtInRect(&r, p) || TaskbarStartOpen || TaskbarMenuUp || GetCapture()) {
        away = 0;
    } else if (++away >= 4) {
        Slide(FALSE);
    }
}

void TaskbarShowStart(void)
{
    RECT button;

    if (TaskbarStartOpen)
        return;
    if (hidden)
        Slide(TRUE);
    TaskbarStartOpen = TRUE;
    InvalidateRect(g_taskbar, NULL, FALSE);
    UpdateWindow(g_taskbar);
    TaskbarButtonRect(-1, &button);
    ClientToScreen(g_taskbar, (POINT FAR *)&button.left);
    ClientToScreen(g_taskbar, (POINT FAR *)&button.right);
    StartMenuShow(g_taskbar, &button);
    TaskbarStartOpen = FALSE;
    TaskbarPressed = NONE;
    InvalidateRect(g_taskbar, NULL, FALSE);
}

/* The window of the task button at a screen point, NULL if none. */
HWND TaskbarTaskAt(POINT p)
{
    int i;

    if (WindowFromPoint(p) != g_taskbar)
        return NULL;
    ScreenToClient(g_taskbar, &p);
    i = TaskbarHitButton(p.x, p.y);
    return i >= 0 ? TaskbarTasks[i] : NULL;
}

static void Place(HWND w, int x, int y, int cx, int cy)
{
    if (IsZoomed(w))
        ShowWindow(w, SW_SHOWNOACTIVATE);
    SetWindowPos(w, NULL, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE |
                 ((GetWindowLong(w, GWL_STYLE) & WS_THICKFRAME) ? 0 : SWP_NOSIZE));
}

/* Cascade (the front window last, furthest down) or tile the windows that
   are not minimized, over the screen above the taskbar; or minimize all. */
void TaskbarArrange(UINT how)
{
    HWND w[MAX_TASKS], v;
    RECT a;
    int n = 0, i, j, k, lines, m, cx, cy;
    int step = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME), per;

    Area(Thickness(), &a);
    cx = a.right - a.left;
    cy = a.bottom - a.top;

    for (v = GetWindow(GetDesktopWindow(), GW_CHILD); v; v = GetWindow(v, GW_HWNDNEXT))
        if (TaskbarFind(v) >= 0 && !IsIconic(v))
            w[n++] = v;                 /* front first */
    if (how == ARR_MINALL) {
        while (n)
            ShowWindow(w[--n], SW_SHOWMINNOACTIVE);
        SetActiveWindow(FindWindow(DESKTOP_CLASS, NULL));   /* not a minimized one */
    } else if (how == ARR_CASCADE) {
        per = max(1, (cy / 4) / step + 1);
        for (k = 0; k < n; k++)
            Place(w[n - 1 - k], a.left + k % per * step, a.top + k % per * step, cx * 3 / 4, cy * 3 / 4);
    } else {                            /* strips up to 3, then a grid */
        for (lines = 1; lines * lines < n; lines++)
            ;
        if (n <= 3)
            lines = n;
        for (j = k = 0; j < lines; j++) {
            m = n / lines + (j < n % lines);
            for (i = 0; i < m; i++, k++)
                if (how == ARR_TILEH)
                    Place(w[k], a.left + i * cx / m, a.top + j * cy / lines, cx / m, cy / lines);
                else
                    Place(w[k], a.left + j * cx / lines, a.top + i * cy / m, cx / lines, cy / m);
        }
    }
}

/* Taskbar Properties' options: on top (or covered by windows, maximized
   ones taking the whole screen), auto hide, the clock, Quick Launch. */
/* At its side and size: maximized windows and the desktop's icons keep
   clear of it. */
static void PlaceBar(void)
{
    RECT r;

    Area(autoHide ? HIDDEN_H : onTop ? Thickness() : 0, &r);
    HookWorkArea(&r);
    TaskbarWorkArea(&r);
    DesktopSetArea(&r);
    BarRect(TRUE, &r);
    SetWindowPos(g_taskbar, onTop || autoHide ? HWND_TOPMOST : HWND_NOTOPMOST, r.left, r.top,
                 r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_NOCOPYBITS);  /* (no old picture copied) */
    hidden = FALSE;
    InvalidateRect(g_taskbar, NULL, TRUE);
}

void TaskbarApply(void)
{
    onTop = SettingsGetInt("Taskbar", "OnTop", 1);
    autoHide = SettingsGetInt("Taskbar", "AutoHide", 0);
    TaskbarShowClock = SettingsGetInt("Taskbar", "Clock", 1);
    TaskbarIconsOnly = SettingsGetInt("Taskbar", "IconsOnly", 0);
    TaskbarGrouping = SettingsGetInt("Taskbar", "Group", 0);
    edge = SettingsGetInt("Taskbar", "Edge", TBE_BOTTOM) & 3;
    rows = max(1, SettingsGetInt("Taskbar", "Rows", 1));
    sideWidth = max(SIDE_MIN, SettingsGetInt("Taskbar", "Width", SIDE_W));
    if (!TaskbarShowClock)
        KillTimer(g_taskbar, TIMER_CLOCK);
    else
        TaskbarClockMeasure();
    TaskbarLoadPrograms();
    QuickLaunchLoad();
    PlaceBar();
    if (autoHide)
        SetTimer(g_taskbar, TIMER_HIDE, 200, NULL);
    else
        KillTimer(g_taskbar, TIMER_HIDE);
    TaskbarRescan();                           /* the Tray page's lists may have changed */
}

/* ---- dragged to another side, or wider ---- */

/* Within 3 pixels of the edge toward the middle of the screen: resizing. */
BOOL TaskbarOnInnerEdge(int x, int y)
{
    RECT c;

    GetClientRect(g_taskbar, &c);
    switch (edge) {
    case TBE_TOP:   return y >= c.bottom - 3;
    case TBE_LEFT:  return x >= c.right - 3;
    case TBE_RIGHT: return x < 3;
    }
    return y < 3;
}

/* Dragged: to the side of the screen nearest the pointer (as a share of
   the screen's width or height); by the inner edge: rows of buttons, or
   the width at a side (up to half the screen). */
void TaskbarDragged(POINT p)
{
    int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN), was, to, n;
    long d[4];

    if (TaskbarSizing == SZ_GRAB && abs(p.x - TaskbarGrabbed.x) + abs(p.y - TaskbarGrabbed.y) > 4)
        TaskbarSizing = SZ_MOVE;
    if (TaskbarSizing == SZ_MOVE) {
        d[TBE_BOTTOM] = (long)(cy - p.y) * cx;
        d[TBE_TOP] = (long)p.y * cx;
        d[TBE_LEFT] = (long)p.x * cy;
        d[TBE_RIGHT] = (long)(cx - p.x) * cy;
        for (to = n = 0; n < 4; n++)
            if (d[n] < d[to])
                to = n;
        if (to != edge) {
            edge = to;
            PlaceBar();
        }
    } else if (TaskbarSizing == SZ_SIZE && TaskbarVertical()) {
        was = sideWidth;
        sideWidth = max(SIDE_MIN, min(cx / 2, edge == TBE_LEFT ? p.x + 1 : cx - p.x));
        if (sideWidth != was)
            PlaceBar();
    } else if (TaskbarSizing == SZ_SIZE) {
        was = rows;
        n = edge == TBE_TOP ? p.y + 1 : cy - p.y;
        rows = max(1, min((cy / 2 - TB_H) / ROW_H + 1, (n - TB_H + ROW_H / 2) / ROW_H + 1));
        if (rows != was)
            PlaceBar();
    }
}

void TaskbarDragDone(void)
{
    BOOL moved = TaskbarSizing == SZ_MOVE || TaskbarSizing == SZ_SIZE;

    TaskbarSizing = SZ_NONE;
    ReleaseCapture();
    if (!moved)
        return;
    SettingsSetInt("Taskbar", "Edge", edge);
    SettingsSetInt("Taskbar", "Rows", rows);
    SettingsSetInt("Taskbar", "Width", sideWidth);
}

/* Over the inner edge, or while resizing: the sizing arrows. */
BOOL TaskbarCursor(HWND w, UINT hit)
{
    POINT p;

    if (hit != HTCLIENT)
        return FALSE;
    GetCursorPos(&p);
    ScreenToClient(w, &p);
    if (TaskbarSizing == SZ_SIZE || (!TaskbarSizing && TaskbarOnInnerEdge(p.x, p.y))) {
        SetCursor(LoadCursor(NULL, TaskbarVertical() ? IDC_SIZEWE : IDC_SIZENS));
        return TRUE;
    }
    return FALSE;
}
