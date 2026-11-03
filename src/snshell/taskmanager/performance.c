/*
 * Performance, laid out as Windows 2000 does: processor and memory meters
 * with their history graphs, then boxes of figures. The history is kept
 * while Task Manager is open, one sample per update.
 */
#include "internal.h"
#undef MAX_PATH                         /* toolhelp.h has its own */
#include <toolhelp.h>

#define HIST        256                 /* samples kept */
#define STEP        3                   /* pixels between samples */
#define GRID        12
#define GAP         6
#define METER_W     74
#define BOX_H       66                  /* a box of three lines */
#define LINE_H      14
#define DARK        RGB(0, 128, 64)     /* grid, unlit meter */
#define GREEN       RGB(0, 255, 0)
#define YELLOW      RGB(255, 255, 0)

enum { B_CPU, B_CPUHIST, B_MEM, B_MEMHIST, B_TOTALS, B_PHYS, B_VIRT, B_RES, NBOX };
static const char *names[NBOX] = {
    "CPU Usage", "CPU Usage History", "Mem Usage", "Memory Usage History",
    "Totals", "Physical Memory (K)", "Virtual Memory (K)", "Free Resources",
};

static HGLOBAL mem;
static BYTE FAR *hist;                  /* cpu, then memory: percents, a ring each */
static int count, head, scroll;
static int cpu;
static DWORD lastClock, lastIdle;
static MEMMANINFO mm;
static RECT boxes[NBOX];

BOOL PerformanceInit(void)
{
    mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 2 * HIST);
    hist = mem ? GlobalLock(mem) : NULL;
    count = head = scroll = cpu = 0;
    lastClock = CpuClock();
    lastIdle = CpuIdle();
    PerformanceSample();
    return hist != NULL;
}

void PerformanceDone(void)
{
    if (mem) {
        GlobalUnlock(mem);
        GlobalFree(mem);
    }
    mem = NULL;
    hist = NULL;
}

DWORD PerformanceMemoryTotal(void)
{
    return mm.dwTotalPages * mm.wPageSize;
}

DWORD PerformanceMemoryUsed(void)
{
    return (mm.dwTotalPages - mm.dwFreePages) * mm.wPageSize;
}

int PerformanceCpu(void)
{
    return cpu;
}

/* Processor use since the last sample (what was not idle), and memory. */
void PerformanceSample(void)
{
    DWORD now = CpuClock(), idle = CpuIdle(), span = now - lastClock;

    if (span >= 100)
        cpu = 100 - (int)min(100, (idle - lastIdle) / (span / 100));
    lastClock = now;
    lastIdle = idle;
    mm.dwSize = sizeof mm;
    MemManInfo(&mm);
    if (!hist)
        return;
    head = (head + 1) % HIST;
    count = min(count + 1, HIST);
    hist[head] = (BYTE)cpu;
    hist[HIST + head] = (BYTE)(mm.dwTotalPages ? (mm.dwTotalPages - mm.dwFreePages) * 100 / mm.dwTotalPages : 0);
    scroll = (scroll + STEP) % GRID;
}

void PerformanceLayout(const RECT *page)
{
    int w = page->right - page->left, half = (w - GAP) / 2;
    int graphH = max(40, (page->bottom - page->top - 2 * BOX_H - 3 * GAP) / 2);
    int y = page->top, i;

    for (i = 0; i < 2; i++, y += graphH + GAP) {
        SetRect(&boxes[2 * i], page->left, y, page->left + METER_W, y + graphH);
        SetRect(&boxes[2 * i + 1], page->left + METER_W + GAP, y, page->right, y + graphH);
    }
    for (i = 0; i < 2; i++, y += BOX_H + GAP) {
        SetRect(&boxes[B_TOTALS + 2 * i], page->left, y, page->left + half, y + BOX_H);
        SetRect(&boxes[B_TOTALS + 2 * i + 1], page->right - half, y, page->right, y + BOX_H);
    }
}

/* The black area inside a box. */
static void Inside(int box, RECT *r)
{
    *r = boxes[box];
    InflateRect(r, -8, 0);
    r->top += 16;
    r->bottom -= 8;
}

/* LED bars lit from the bottom up to percent, the figure below them. */
static void Meter(HDC dc, const RECT *r, int percent, LPCSTR figure)
{
    int h = r->bottom - r->top, w = r->right - r->left, bars = max(0, (h - 22) / 3);
    int lit = (percent * bars + 50) / 100, i, mid = r->left + w / 2;
    RECT b, t;

    SetRect(&t, r->left, r->bottom - 18, r->right, r->bottom - 4);
    SetTextColor(dc, GREEN);
    DrawText(dc, figure, -1, &t, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    for (i = 0; i < bars; i++) {
        int y = r->bottom - 20 - 3 * i;
        COLORREF c = i < lit ? GREEN : DARK;
        SetRect(&b, r->left + 12, y - 2, mid - 1, y);
        DrawSolidRect(dc, &b, c);
        SetRect(&b, mid + 1, y - 2, r->right - 12, y);
        DrawSolidRect(dc, &b, c);
    }
}

/* The grid, scrolling with the samples, and the line of one history. */
static void Graph(HDC dc, const RECT *r, const BYTE FAR *h, COLORREF color)
{
    int w = r->right - r->left, ht = r->bottom - r->top, x, i, n;
    HPEN grid = CreatePen(PS_SOLID, 1, DARK), line = CreatePen(PS_SOLID, 1, color);
    HPEN old = SelectObject(dc, grid);

    for (x = r->right - 1 - scroll; x >= r->left; x -= GRID) {
        MoveTo(dc, x, r->top);
        LineTo(dc, x, r->bottom);
    }
    for (i = r->bottom - 1; i >= r->top; i -= GRID) {
        MoveTo(dc, r->left, i);
        LineTo(dc, r->right, i);
    }
    SelectObject(dc, line);
    n = min(count, w / STEP + 1);
    for (i = 0; i < n; i++) {
        int y = r->bottom - 1 - (h[(head - i + HIST) % HIST] * (ht - 1) + 50) / 100;
        x = r->right - 1 - i * STEP;
        if (i == 0)
            MoveTo(dc, x, y);
        else
            LineTo(dc, x, y);
    }
    SelectObject(dc, old);
    DeleteObject(grid);
    DeleteObject(line);
}

/* A box's black area, drawn off screen then copied (no flicker). */
static void Black(HDC dc, int box, int percent, LPCSTR figure)
{
    RECT r, z;
    HDC mdc;
    HBITMAP bmp, oldBmp;
    HFONT oldFont;

    Inside(box, &r);
    if (r.right <= r.left || r.bottom <= r.top)
        return;
    mdc = CreateCompatibleDC(dc);
    bmp = CreateCompatibleBitmap(dc, r.right - r.left, r.bottom - r.top);
    oldBmp = SelectObject(mdc, bmp);
    oldFont = SelectObject(mdc, DrawFont(FALSE));
    SetBkMode(mdc, TRANSPARENT);
    SetRect(&z, 0, 0, r.right - r.left, r.bottom - r.top);
    DrawSolidRect(mdc, &z, RGB(0, 0, 0));
    if (box == B_CPUHIST || box == B_MEMHIST)
        Graph(mdc, &z, hist + (box == B_MEMHIST ? HIST : 0), box == B_MEMHIST ? YELLOW : GREEN);
    else
        Meter(mdc, &z, percent, figure);
    BitBlt(dc, r.left, r.top, z.right, z.bottom, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, oldFont);
    SelectObject(mdc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mdc);
}

/* Three lines of label and figure; the figures are painted opaque, so an
   update needs no erasing. */
static void Figures(HDC dc, int box, LPCSTR labels, const DWORD *values, BOOL percent)
{
    char text[24];
    RECT r;
    int i;

    for (i = 0; i < 3; i++) {
        SetRect(&r, boxes[box].left + 8, boxes[box].top + 16 + i * LINE_H,
                boxes[box].right - 8, boxes[box].top + 16 + (i + 1) * LINE_H);
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
        DrawText(dc, labels, -1, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        labels += lstrlen(labels) + 1;
        FormatNumber(values[i], text);
        if (percent)
            lstrcat(text, "%");
        DrawText(dc, text, -1, &r, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
}

static int Count(BOOL modules)
{
    MODULEENTRY me;
    HWND w;
    int n = 0;
    BOOL more;

    if (modules) {
        me.dwSize = sizeof me;
        for (more = ModuleFirst(&me); more; more = ModuleNext(&me))
            n++;
    } else {
        for (w = GetWindow(GetDesktopWindow(), GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT))
            n++;
    }
    return n;
}

void PerformancePaint(HDC dc)
{
    char figure[24];
    DWORD v[3];
    RECT caption;
    int i;
    DWORD kb = mm.wPageSize / 1024;     /* per page */

    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    for (i = 0; i < NBOX; i++) {
        DrawGroupFrame(dc, &boxes[i], LOWORD(GetTextExtent(dc, names[i], lstrlen(names[i]))) + 4, &caption);
        caption.left += 2;
        DrawText(dc, names[i], -1, &caption, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    }
    wsprintf(figure, "%d %%", cpu);
    Black(dc, B_CPU, cpu, figure);
    Black(dc, B_CPUHIST, 0, NULL);
    FormatNumber(PerformanceMemoryUsed() / 1024, figure);
    lstrcat(figure, "K");
    Black(dc, B_MEM, (int)(mm.dwTotalPages ? (mm.dwTotalPages - mm.dwFreePages) * 100 / mm.dwTotalPages : 0), figure);
    Black(dc, B_MEMHIST, 0, NULL);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    v[0] = GetNumTasks();
    v[1] = Count(TRUE);
    v[2] = Count(FALSE);
    Figures(dc, B_TOTALS, "Tasks\0Modules\0Windows", v, FALSE);
    v[0] = mm.dwTotalPages * kb;
    v[1] = mm.dwFreePages * kb;
    v[2] = mm.dwMaxPagesLockable * kb;
    Figures(dc, B_PHYS, "Total\0Available\0Lockable", v, FALSE);
    v[0] = GetFreeSpace(0) / 1024;
    v[1] = mm.dwLargestFreeBlock / 1024;
    v[2] = mm.dwSwapFilePages * kb;
    Figures(dc, B_VIRT, "Free\0Largest Block\0Swap File", v, FALSE);
    v[0] = GetFreeSystemResources(GFSR_SYSTEMRESOURCES);
    v[1] = GetFreeSystemResources(GFSR_USERRESOURCES);
    v[2] = GetFreeSystemResources(GFSR_GDIRESOURCES);
    Figures(dc, B_RES, "System\0User\0GDI", v, TRUE);
}
