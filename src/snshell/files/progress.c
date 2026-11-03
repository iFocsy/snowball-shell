/*
 * The progress window of file operations, after Windows 98: a flat
 * animation, the name, "From 'a' to 'b'", a block progress bar, the time
 * left. Every part is painted off screen and copied in one go, so nothing
 * flickers while it changes many times a second.
 */
#include "internal.h"

#define TIMER_FRAME 1

#define FRAME_MS    70

#define CHEVRON_W   9               /* one '>' of the animation, in pixels */

#define CHEVRON_GAP 7

#define SHOW_AFTER  600             /* ms: quick operations never show the window */

#define TEXT_EVERY  100             /* ms between file name updates */

#define PUMP_EVERY  40              /* ms between letting Windows run */

enum { P_ANIM, P_NAME, P_FROM, P_BAR, P_TIME, PARTS };

static HWND dlg;

static int kind, frame, shownFill;

static BOOL cancel;

static DWORD started, markTime, markDone, lastText, lastPump, done, total;

static RECT part[PARTS];            /* client rectangles of the painted parts */

static char texts[PARTS][MAX_PATH + 40];    /* "From 'a' to 'b'" is cut to fit */

static const char *Titles[] = { "Copying...", "Moving...", "Deleting...", "Deleting..." };

/* 0..1000 without overflowing 32 bits. */
static int Permille(DWORD part, DWORD whole)
{
    if (!whole)
        return 0;
    if (part >= whole)
        return 1000;
    return whole < 4000000L ? (int)(part * 1000 / whole) : (int)(part / (whole / 1000));
}

/* Pixels of the bar's inside that are filled. */
static int Fill(void)
{
    return (int)((long)(part[P_BAR].right - part[P_BAR].left - 4) * Permille(done, total) / 1000);
}

/* Windows 98 progress bar: sunken edge, blocks in the highlight colour. */
static void DrawBar(HDC dc, RECT *all)
{
    RECT in = *all, b;
    int step, x;

    shownFill = Fill();
    DrawFrame(dc, all, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
    InflateRect(&in, -2, -2);
    step = (in.bottom - in.top) * 2 / 3 + 2;
    for (x = in.left; x < in.left + shownFill; x += step) {
        SetRect(&b, x, in.top, min(x + step - 2, in.right), in.bottom);
        DrawSolidRect(dc, &b, GetSysColor(COLOR_HIGHLIGHT));
    }
}

/* c1 -> c2 by n/8. */
static COLORREF Mix(COLORREF c1, COLORREF c2, int n)
{
    return RGB(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * n / 8,
               GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * n / 8,
               GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * n / 8);
}

/* A row of flat chevrons; a bright wave runs along it, left to right, and
   fades behind. */
static void DrawChevrons(HDC dc, RECT *all)
{
    static const BYTE fade[] = { 0, 3, 5, 6 };  /* eighths towards the face colour */
    int w = all->right, h = all->bottom;
    int count = (w + CHEVRON_GAP) / (CHEVRON_W + CHEVRON_GAP), cycle = count + 6;
    int x = (w - count * (CHEVRON_W + CHEVRON_GAP) + CHEVRON_GAP) / 2, y = (h - 2 * CHEVRON_W) / 2, i, d;
    COLORREF face = GetSysColor(COLOR_BTNFACE), lit = GetSysColor(COLOR_HIGHLIGHT);
    HPEN oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    HBRUSH brush, oldBrush;
    POINT v[6];

    for (i = 0; i < count; i++, x += CHEVRON_W + CHEVRON_GAP) {
        d = (frame - i + cycle) % cycle;        /* how long ago the wave passed */
        v[0].x = x;                 v[0].y = y;
        v[1].x = x + 4;             v[1].y = y;
        v[2].x = x + CHEVRON_W;     v[2].y = y + CHEVRON_W;
        v[3].x = x + 4;             v[3].y = y + 2 * CHEVRON_W;
        v[4].x = x;                 v[4].y = y + 2 * CHEVRON_W;
        v[5].x = x + CHEVRON_W - 4; v[5].y = y + CHEVRON_W;
        brush = CreateSolidBrush(Mix(lit, face, d < 4 ? fade[d] : 7));
        oldBrush = SelectObject(dc, brush);
        Polygon(dc, v, 6);
        SelectObject(dc, oldBrush);
        DeleteObject(brush);
    }
    SelectObject(dc, oldPen);
}

/* One part, drawn on a bitmap of its size and copied to the window. */
static void PaintPart(HDC dc, int i)
{
    int w = part[i].right - part[i].left, h = part[i].bottom - part[i].top;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, w, h), old = SelectObject(mem, bmp);
    HFONT oldFont;
    RECT all;

    SetRect(&all, 0, 0, w, h);
    DrawSolidRect(mem, &all, GetSysColor(COLOR_BTNFACE));
    if (i == P_ANIM) {
        DrawChevrons(mem, &all);
    } else if (i == P_BAR) {
        DrawBar(mem, &all);
    } else {
        oldFont = SelectObject(mem, DrawFont(FALSE));
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, GetSysColor(COLOR_BTNTEXT));
        DrawTextEllipsis(mem, texts[i], &all, DT_LEFT | DT_NOPREFIX);
        SelectObject(mem, oldFont);
    }
    BitBlt(dc, part[i].left, part[i].top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void SetPart(int i, LPCSTR text)
{
    if (lstrcmp(texts[i], text)) {
        lstrcpyn(texts[i], text, sizeof texts[i]);
        InvalidateRect(dlg, &part[i], FALSE);
    }
}

static void DialogRect(HWND w, int i, int x, int y, int cx, int cy)
{
    SetRect(&part[i], x, y, x + cx, y + cy);
    MapDialogRect(w, &part[i]);
}

BOOL CALLBACK __export ProgressProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    int i;

    switch (msg) {
    case WM_INITDIALOG:                 /* the layout, in dialog units */
        SkinDialog(w);
        DialogRect(w, P_ANIM, 7, 7, 246, 20);
        DialogRect(w, P_NAME, 7, 31, 246, 10);
        DialogRect(w, P_FROM, 7, 42, 246, 10);
        DialogRect(w, P_BAR, 7, 55, 246, 11);
        DialogRect(w, P_TIME, 7, 75, 190, 10);
        return TRUE;
    case WM_PAINT:
        BeginPaint(w, &ps);
        for (i = 0; i < PARTS; i++)
            if (RectVisible(ps.hdc, &part[i]))
                PaintPart(ps.hdc, i);
        EndPaint(w, &ps);
        return TRUE;
    case WM_TIMER:
        frame = (frame + 1) % 1000;
        InvalidateRect(w, &part[P_ANIM], FALSE);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDCANCEL)
            cancel = TRUE;
        return TRUE;
    }
    return FALSE;
}

void ProgressOpen(HWND owner, int op)
{
    int i;

    kind = op;
    cancel = FALSE;
    frame = shownFill = 0;
    done = total = 0;
    started = lastText = lastPump = GetTickCount();
    markTime = markDone = 0;
    for (i = 0; i < PARTS; i++)
        texts[i][0] = '\0';
    dlg = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_PROGRESS), owner, ProgressProc);
    if (dlg)
        SetWindowText(dlg, Titles[op]);
}

/* Last part of a folder path, "C:\" for a root. */
static void FolderName(LPCSTR dir, LPSTR out)
{
    lstrcpy(out, lstrlen(dir) > 3 ? PathFileName((LPSTR)dir) : dir);
}

void ProgressItem(LPCSTR path, LPCSTR destDir)
{
    char from[MAX_PATH], a[MAX_PATH], b[MAX_PATH], text[2 * MAX_PATH + 20];
    DWORD now = GetTickCount();

    if (!dlg || now - lastText < TEXT_EVERY)    /* nobody reads hundreds of names a second */
        return;
    lastText = now;
    SetPart(P_NAME, PathFileName((LPSTR)path));
    lstrcpy(from, path);
    PathToParent(from);
    FolderName(from, a);
    if (destDir) {
        FolderName(destDir, b);
        wsprintf(text, "From '%s' to '%s'", (LPSTR)a, (LPSTR)b);
    } else {
        wsprintf(text, "From '%s'", (LPSTR)a);
    }
    SetPart(P_FROM, text);
}

void ProgressStep(DWORD d, DWORD t)
{
    DWORD now = GetTickCount(), left;
    char text[40];
    int share;

    if (!dlg)
        return;
    done = d;
    total = max(t, d);
    if (Fill() != shownFill)
        InvalidateRect(dlg, &part[P_BAR], FALSE);
    /* the time left follows the rate of the last few seconds */
    if (!done || done == total)
        return;
    if (!markTime) {
        markTime = now;
        markDone = done;
        return;
    }
    if (now - markTime < 2000 || done == markDone)
        return;
    share = Permille(done - markDone, total - markDone);
    left = (now - markTime) / 10 * (1000 - share) / max(1, share) / 100;
    markTime = now;
    markDone = done;
    if (left < 60)
        wsprintf(text, "%lu Second%s Remaining", max(1, left), (LPSTR)(left > 1 ? "s" : ""));
    else
        wsprintf(text, "%lu Minute%s Remaining", left / 60, (LPSTR)(left >= 120 ? "s" : ""));
    SetPart(P_TIME, text);
}

BOOL ProgressCancelled(void)
{
    DWORD now = GetTickCount();

    if (now - lastPump < PUMP_EVERY)
        return cancel;
    lastPump = now;
    if (dlg && !IsWindowVisible(dlg) && now - started > SHOW_AFTER) {
        WindowCenter(dlg);
        ShowWindow(dlg, SW_SHOWNORMAL);
        SetTimer(dlg, TIMER_FRAME, FRAME_MS, NULL);
    }
    if (!MessageLoopPump(dlg))                     /* Windows is ending: stop */
        cancel = TRUE;
    return cancel;
}

void ProgressClose(void)
{
    if (dlg)
        DestroyWindow(dlg);
    dlg = NULL;
}

HWND ProgressWindow(void)
{
    return dlg;
}
