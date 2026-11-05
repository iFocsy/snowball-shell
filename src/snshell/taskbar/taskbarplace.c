/*
 * The taskbar on the screen. It goes to any side (dragged by its empty
 * part) and grows by rows of buttons, or wider at a side (dragged by its
 * inner edge). Taskbar Properties' options (on top, auto hide, clock) place
 * it; what it leaves of the screen is the work area, where Cascade and Tile
 * arrange the windows. The Start menu opens by the Start button, the
 * taskbar slid into view first.
 */
#include "internal.h"
#include "taskbarint.h"
#include <stdlib.h>

#define SIDE_W      104                 /* first width at a side of the screen */
#define SIDE_MIN    (TaskbarIconsOnly ? 2 + 30 + 8 : 2 + START_W + 4)   /* (icons only: an icon wide) */
#define HIDDEN_H    2                   /* what auto hide leaves of the taskbar */

static BOOL onTop, autoHide, hidden;
static int away;                        /* auto hide: ticks the mouse has been elsewhere */
int TaskbarSide = TBE_BOTTOM;           /* TBE_*: the side of the screen */
int TaskbarRows = 1;                    /* at the top or bottom: rows of buttons */
static int sideWidth = SIDE_W;          /* at a side: its width */
int TaskbarSizing;                      /* SZ_*: the taskbar dragged */
POINT TaskbarGrabbed;
BOOL TaskbarLocked;

int TaskbarEdge(void)
{
    return TaskbarSide;
}

BOOL TaskbarVertical(void)
{
    return TaskbarSide == TBE_LEFT || TaskbarSide == TBE_RIGHT;
}

/* How far it reaches into the screen. */
static int Thickness(void)
{
    return TaskbarVertical() ? sideWidth : TB_H + (TaskbarRows - 1) * ROW_H;
}

/* Where it is on the screen: all of it, or (auto hide) a line at its edge. */
static void BarRect(BOOL show, RECT *r)
{
    int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);
    int t = Thickness(), in = show ? t : HIDDEN_H;

    switch (TaskbarSide) {
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
    switch (TaskbarSide) {
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
#define SLIDE_STEPS 4                   /* animated: in or out in about 60 ms */
static void Slide(BOOL show)
{
    RECT from, to;
    int i;

    hidden = !show;
    away = 0;
    GetWindowRect(g_taskbar, &from);
    BarRect(show, &to);
    for (i = DrawAnimations() ? 1 : SLIDE_STEPS; i <= SLIDE_STEPS; i++) {
        SetWindowPos(g_taskbar, HWND_TOPMOST, from.left + MulDiv(to.left - from.left, i, SLIDE_STEPS),
                     from.top + MulDiv(to.top - from.top, i, SLIDE_STEPS), 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        if (i < SLIDE_STEPS) {
            UpdateWindow(g_taskbar);
            DrawWait(17898L);           /* 15 ms */
        }
    }
}

/* The pointer at the very edge the taskbar hides at. */
static BOOL AtEdge(POINT p)
{
    switch (TaskbarSide) {
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

/* Taskbar Properties' options: on top (or covered by windows, maximized
   ones taking the whole screen), auto hide, the clock, Quick Launch. */
void TaskbarApply(void)
{
    onTop = SettingsGetInt("Taskbar", "OnTop", 1);
    autoHide = SettingsGetInt("Taskbar", "AutoHide", 0);
    TaskbarShowClock = SettingsGetInt("Taskbar", "Clock", 1);
    TaskbarIconsOnly = SettingsGetInt("Taskbar", "IconsOnly", 0);
    TaskbarLocked = SettingsGetInt("Taskbar", "Locked", 0);
    TaskbarGrouping = SettingsGetInt("Taskbar", "Group", 0);
    TaskbarSide = SettingsGetInt("Taskbar", "Edge", TBE_BOTTOM) & 3;
    TaskbarRows = max(1, SettingsGetInt("Taskbar", "Rows", 1));
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

    if (TaskbarLocked)
        return FALSE;
    GetClientRect(g_taskbar, &c);
    switch (TaskbarSide) {
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
        if (to != TaskbarSide) {
            TaskbarSide = to;
            PlaceBar();
        }
    } else if (TaskbarSizing == SZ_SIZE && TaskbarVertical()) {
        was = sideWidth;
        sideWidth = max(SIDE_MIN, min(cx / 2, TaskbarSide == TBE_LEFT ? p.x + 1 : cx - p.x));
        if (sideWidth != was)
            PlaceBar();
    } else if (TaskbarSizing == SZ_SIZE) {
        was = TaskbarRows;
        n = TaskbarSide == TBE_TOP ? p.y + 1 : cy - p.y;
        TaskbarRows = max(1, min((cy / 2 - TB_H) / ROW_H + 1, (n - TB_H + ROW_H / 2) / ROW_H + 1));
        if (TaskbarRows != was)
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
    SettingsSetInt("Taskbar", "Edge", TaskbarSide);
    SettingsSetInt("Taskbar", "Rows", TaskbarRows);
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

/* ---- Snap: Windows 7's halves of the work area ---- */

#define MAX_SNAPPED 16
static struct { HWND w; UINT side; RECT was; } snapped[MAX_SNAPPED];   /* where each was */

static int Snapped(HWND w)
{
    int i;

    for (i = 0; i < MAX_SNAPPED && snapped[i].w != w; i++)
        ;
    return i < MAX_SNAPPED && IsWindow(w) ? i : -1;
}

static void Unsnap(int i)
{
    RECT *r = &snapped[i].was;

    MoveWindow(snapped[i].w, r->left, r->top, r->right - r->left, r->bottom - r->top, TRUE);
    snapped[i].w = NULL;
}

/* A program's window to the left or right half (VK_LEFT, VK_RIGHT; the
   other way: back as it was), maximized (VK_UP), or back, else minimized
   (VK_DOWN). Windows that cannot be sized only maximize and minimize. */
void TaskbarSnap(HWND w, UINT key)
{
    LONG style = GetWindowLong(w, GWL_STYLE);
    int i = Snapped(w);
    RECT a, r;

    if (!w || TaskbarFind(w) < 0)
        return;
    if (key == VK_UP || key == VK_DOWN) {
        if (key == VK_UP && (style & WS_MAXIMIZEBOX) && !IsZoomed(w)) {
            HookZoom(w, SC_MAXIMIZE);
            ShowWindow(w, SW_SHOWMAXIMIZED);
        } else if (key == VK_DOWN && IsZoomed(w)) {
            HookZoom(w, SC_RESTORE);
            ShowWindow(w, SW_RESTORE);
        } else if (key == VK_DOWN && i >= 0) {
            Unsnap(i);
        } else if (key == VK_DOWN && (style & WS_MINIMIZEBOX)) {
            HookZoom(w, SC_MINIMIZE);
            ShowWindow(w, SW_MINIMIZE);
        }
        return;
    }
    if (!(style & WS_THICKFRAME) || (i >= 0 && snapped[i].side == key))
        return;
    if (i >= 0) {                       /* snapped the other way: back */
        Unsnap(i);
        return;
    }
    if (IsIconic(w) || IsZoomed(w))
        ShowWindow(w, SW_RESTORE);
    for (i = 0; i < MAX_SNAPPED - 1 && snapped[i].w && IsWindow(snapped[i].w); i++)
        ;
    snapped[i].w = w;                   /* (all taken: the last place is reused) */
    snapped[i].side = key;
    GetWindowRect(w, &snapped[i].was);
    TaskbarWorkArea(&a);
    r = a;
    if (key == VK_LEFT)
        r.right = (a.left + a.right) / 2;
    else
        r.left = (a.left + a.right) / 2;
    MoveWindow(w, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}
