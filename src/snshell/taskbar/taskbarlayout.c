/*
 * Where everything is on the taskbar: along it, the Start button, Quick
 * Launch's band, the task buttons' band and the tray; hit testing and
 * painting go by these. Also which task button is at a point of the screen.
 * taskbarplace.c places the taskbar itself.
 */
#include "internal.h"
#include "taskbarint.h"

#define BTN_H       22                  /* a button's height (ROW_H with the gap below) */
#define BTN_MAX     160

/* The client less the 2-pixel raised edge it shows toward the screen. */
static void Inner(RECT *c)
{
    GetClientRect(g_taskbar, c);
    switch (TaskbarSide) {
    case TBE_BOTTOM: c->top += 2; break;
    case TBE_TOP:    c->bottom -= 2; break;
    case TBE_LEFT:   c->right -= 2; break;
    case TBE_RIGHT:  c->left += 2; break;
    }
}

int TaskbarTrayCount(void)
{
    return TaskbarNTray + TrayToolsCount();
}

static BOOL ShowTray(void)
{
    return TaskbarShowClock || TaskbarTrayCount();
}

/* As Windows 98's: its edge, 3 pixels, the programs' icons, the time with 7
   each side, its edge. */
static int TrayWidth(void)
{
    return !ShowTray() ? 0 : (TaskbarShowClock ? TaskbarClockW + 18 : 6) + TaskbarTrayCount() * TRAYICON_W;
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
        h = (TaskbarShowClock ? BTN_H : 6) + (TaskbarTrayCount() + TrayIconsPerRow(r->right - r->left - 4) - 1) /
            TrayIconsPerRow(r->right - r->left - 4) * TRAYICON_W;
        SetRect(r, r->left + 2, r->bottom - 2 - h, r->right - 2, r->bottom - 2);
    } else {
        SetRect(r, r->right - TrayWidth() - 2, r->top + 2, r->right - 2, r->bottom - 2);
    }
}

void TaskbarTrayIconRect(int i, RECT *r)
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
/* Where the task buttons go, and how many there are: worked out once for
   all of them (a hit test or a paint). */
static int Buttons(RECT *b)
{
    TasksBand(b);
    if (TasksGripped()) {
        if (TaskbarVertical())
            b->top += 7;
        else
            b->left += 7;
    }
    return TaskbarSlots();
}

/* The button of slot i of n in band b. */
static void SlotRect(const RECT *band, int n, int i, RECT *r)
{
    RECT b = *band;
    int perRow, w;

    if (i < 0) {                        /* on its group's button */
        SetRectEmpty(r);
        return;
    }
    if (TaskbarVertical()) {
        SetRect(r, b.left, b.top + i * ROW_H, b.right, b.top + i * ROW_H + BTN_H);
        if (r->bottom > b.bottom)
            SetRectEmpty(r);
        return;
    }
    if (TaskbarIconsOnly) {
        w = ICON_W;
        perRow = max(1, (b.right - b.left + 3) / w);
    } else {
        perRow = max(max(1, (b.right - b.left + 3) / BTN_MAX), (n + TaskbarRows - 1) / TaskbarRows);
        w = min(BTN_MAX, (b.right - b.left + 3) / perRow);     /* each leaves 3 pixels after it */
    }
    SetRect(r, b.left + i % perRow * w, b.top + i / perRow * ROW_H, b.left + (i % perRow + 1) * w - 3,
            b.top + i / perRow * ROW_H + BTN_H);
}

void TaskbarButtonRect(int i, RECT *r)
{
    RECT b;
    int n;

    if (i < 0) {
        StartRect(r);
        return;
    }
    n = Buttons(&b);
    SlotRect(&b, n, TaskbarSlot(i), r);
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

/* A button (BTN_* state): pressed (the active task's checkered), or lit,
   with its icon and text; text "": the icon in the middle (icons only);
   count > 1: a group's badge. */
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
        COLORREF fg = SetTextColor(dc, GetSysColor(COLOR_BTNFACE)), bk = SetBkColor(dc, GetSysColor(COLOR_BTNHIGHLIGHT));
        HBRUSH old = SelectObject(dc, PaintChecker());
        PatBlt(dc, r->left + 2, r->top + 2, r->right - r->left - 4, r->bottom - r->top - 4, PATCOPY);
        SelectObject(dc, old);
        SetTextColor(dc, fg);
        SetBkColor(dc, bk);
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

/* Its raised edge, toward the screen: light (the face) then white, or grey then black. */
static void PaintEdge(HDC dc, const RECT *c)
{
    RECT r;

    switch (TaskbarSide) {
    case TBE_BOTTOM: SetRect(&r, 0, 1, c->right, 2); break;
    case TBE_RIGHT:  SetRect(&r, 1, 0, 2, c->bottom); break;
    case TBE_TOP:    SetRect(&r, 0, c->bottom - 2, c->right, c->bottom - 1); break;
    default:         SetRect(&r, c->right - 2, 0, c->right - 1, c->bottom); break;
    }
    if (TaskbarSide == TBE_BOTTOM || TaskbarSide == TBE_RIGHT) {
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT));
        return;
    }
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNSHADOW));
    OffsetRect(&r, TaskbarSide == TBE_LEFT, TaskbarSide == TBE_TOP);
    DrawSolidRect(dc, &r, SkinColor(SC_DKSHADOW));
}

/* Task i's button: a group's is down, bold and lit if any of it is, with the active one's title. */
static void PaintTask(HDC dc, const RECT *band, int slots, int i)
{
    int j, n = 0, shown = i, slot = TaskbarSlot(i);
    BOOL active = FALSE, lit = FALSE;
    RECT r;

    if (slot < 0)
        return;
    for (j = i; j < TaskbarNTasks; j++)
        if (TaskbarLead(j) == i) {
            n++;
            lit |= TaskbarLit[j];
            if (TaskbarTasks[j] == TaskbarActive) {
                active = TRUE;
                shown = j;
            }
        }
    SlotRect(band, slots, slot, &r);
    DrawButton(dc, &r, TaskbarIcon(TaskbarTasks[i]), TaskbarIconsOnly ? "" : TaskbarTitles[shown],
               (active || TaskbarPressed == i ? BTN_DOWN : 0) | (active ? BTN_BOLD : 0) | (lit ? BTN_LIT : 0), n);
}

/* The programs' icons, then the time at the right (a side: below them, in
   the middle), a pixel below the middle. */
static void PaintTray(HDC dc)
{
    TEXTMETRIC tm;
    char text[24];
    RECT r, c;
    int i;

    TaskbarTrayRect(&r);
    if (ShowTray()) {
        BandLine(dc, (TaskbarVertical() ? r.top : r.left) - 4);
        DrawFrame(dc, &r, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
        for (i = 0; i < TaskbarTrayCount(); i++) {
            TaskbarTrayIconRect(i, &c);
            OffsetRect(&c, TaskbarPressed == TRAYICON(i), TaskbarPressed == TRAYICON(i));
            if (i < TaskbarNTray)
                DrawIconImage(dc, c.left, c.top, TaskbarTrayIcons[i], TRUE);
            else
                TrayToolsPaint(dc, i - TaskbarNTray, &c);
        }
    }
    if (!TaskbarShowClock)
        return;
    if (TaskbarVertical())
        r.top = r.bottom - BTN_H;
    TaskbarClockText(text);
    SelectObject(dc, DrawFont(FALSE));
    GetTextMetrics(dc, &tm);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    TextOut(dc, TaskbarVertical() ? (r.left + r.right - TaskbarClockW) / 2 : r.right - 8 - TaskbarClockW,
            r.top + (r.bottom - r.top - tm.tmHeight) / 2 + 1, text, lstrlen(text));
}

void TaskbarPaint(HWND w)
{
    PAINTSTRUCT ps;
    RECT zoom[MAX_TASKS];
    RECT r, c, band;
    OFFSCREEN o;
    HDC dc;
    HFONT oldFont;
    int i, slots;

    BeginPaint(w, &ps);
    dc = DrawOffscreenBegin(&o, ps.hdc, &ps.rcPaint);     /* then onto the screen at once */
    SetBkMode(dc, TRANSPARENT);         /* (the clock's too: an icons-only button sets nothing) */
    oldFont = SelectObject(dc, DrawFont(FALSE));
    GetClientRect(w, &c);
    DrawSolidRect(dc, &c, GetSysColor(COLOR_BTNFACE));
    PaintEdge(dc, &c);
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
    slots = Buttons(&band);
    for (i = 0; i < TaskbarNTasks; i++) {
        PaintTask(dc, &band, slots, i);
        SlotRect(&band, slots, TaskbarSlot(TaskbarLead(i)), &zoom[i]);    /* (a group's, its lead's) */
        MapWindowPoints(w, NULL, (POINT FAR *)&zoom[i], 2);
    }
    HookTaskButtons(TaskbarTasks, zoom, TaskbarNTasks);                  /* where windows zoom to */
    PaintTray(dc);
    SelectObject(dc, oldFont);
    DrawOffscreenEnd(&o);
    EndPaint(w, &ps);
}

int TaskbarHitButton(int x, int y)
{
    RECT r, band;
    POINT p;
    int i, slots;

    p.x = x; p.y = y;
    TaskbarButtonRect(-1, &r);
    if (PtInRect(&r, p))
        return -1;
    TaskbarQuickLaunchBand(&r);
    QuickLaunchPlace(&r, TaskbarVertical());
    if ((i = QuickLaunchHitTest(x, y)) >= 0)
        return QL(i);
    slots = Buttons(&band);
    for (i = 0; i < TaskbarNTasks; i++) {
        SlotRect(&band, slots, TaskbarSlot(i), &r);
        if (PtInRect(&r, p))
            return i;
    }
    for (i = 0; i < TaskbarTrayCount(); i++) {
        TaskbarTrayIconRect(i, &r);
        if (PtInRect(&r, p))
            return TRAYICON(i);
    }
    TaskbarTrayRect(&r);
    if (ShowTray() && PtInRect(&r, p))
        return TRAY;
    return NONE;
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
