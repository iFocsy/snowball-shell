/*
 * Windows 98's colour button: the colour, a line, an arrow. Pressed, a
 * palette opens under it: the 20 colours Windows always has, and Other...
 * for the common Color dialog. The palette runs its own message loop with
 * the mouse captured: a click on a colour chooses it, anywhere else or Esc
 * cancels; the arrows and Enter work too.
 */
#include "internal.h"

#define PALETTE_CLASS "SbPalette"
#define COLS        5
#define ROWS        4
#define CELL        18                  /* a colour's square, with its gap */
#define PAD         4
#define OTHER_H     20

static const COLORREF colors[COLS * ROWS] = {
    RGB(0, 0, 0),       RGB(128, 0, 0),     RGB(0, 128, 0),     RGB(128, 128, 0),   RGB(0, 0, 128),
    RGB(128, 0, 128),   RGB(0, 128, 128),   RGB(128, 128, 128), RGB(192, 192, 192), RGB(255, 0, 0),
    RGB(0, 255, 0),     RGB(255, 255, 0),   RGB(0, 0, 255),     RGB(255, 0, 255),   RGB(0, 255, 255),
    RGB(255, 255, 255), RGB(192, 220, 192), RGB(166, 202, 240), RGB(255, 251, 240), RGB(160, 160, 164),
};
static int hot;                         /* the colour framed; COLS * ROWS: Other... */

static void CellRect(int i, RECT *r)
{
    if (i == COLS * ROWS)
        SetRect(r, PAD, PAD + ROWS * CELL + 2, PAD + COLS * CELL - 2, PAD + ROWS * CELL + OTHER_H);
    else
        SetRect(r, PAD + i % COLS * CELL, PAD + i / COLS * CELL, PAD + i % COLS * CELL + CELL - 2,
                PAD + i / COLS * CELL + CELL - 2);
}

static int CellAt(int x, int y)
{
    POINT p;
    RECT r;
    int i;

    p.x = x;
    p.y = y;
    for (i = 0; i <= COLS * ROWS; i++) {
        CellRect(i, &r);
        if (PtInRect(&r, p))
            return i;
    }
    return -1;
}

static void Paint(HWND w)
{
    PAINTSTRUCT ps;
    RECT r, c;
    HFONT old;
    int i;

    BeginPaint(w, &ps);
    GetClientRect(w, &c);
    DrawSolidRect(ps.hdc, &c, GetSysColor(COLOR_BTNFACE));
    DrawBevel(ps.hdc, &c, FALSE);
    for (i = 0; i < COLS * ROWS; i++) {
        CellRect(i, &r);
        DrawBevel(ps.hdc, &r, TRUE);
        InflateRect(&r, -2, -2);
        DrawSolidRect(ps.hdc, &r, colors[i]);
        if (i == hot) {
            InflateRect(&r, 3, 3);
            DrawFrame(ps.hdc, &r, RGB(0, 0, 0), RGB(0, 0, 0));
        }
    }
    CellRect(COLS * ROWS, &r);
    DrawBevel(ps.hdc, &r, FALSE);
    old = SelectObject(ps.hdc, DrawFont(FALSE));
    SetBkMode(ps.hdc, TRANSPARENT);
    SetTextColor(ps.hdc, GetSysColor(COLOR_BTNTEXT));
    DrawText(ps.hdc, "&Other...", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (hot == COLS * ROWS) {
        InflateRect(&r, -3, -3);
        DrawFocusRect(ps.hdc, &r);
    }
    SelectObject(ps.hdc, old);
    EndPaint(w, &ps);
}

LRESULT CALLBACK __export ColorPickerPaletteProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        Paint(w);
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

void ColorPickerDraw(const DRAWITEMSTRUCT FAR *d, COLORREF c)
{
    RECT r = d->rcItem, t;
    BOOL down = (d->itemState & ODS_SELECTED) != 0, off = (d->itemState & ODS_DISABLED) != 0;
    COLORREF arrow = GetSysColor(off ? COLOR_GRAYTEXT : COLOR_BTNTEXT);
    int x, y, i;

    DrawSolidRect(d->hDC, &r, GetSysColor(COLOR_BTNFACE));
    DrawBevel(d->hDC, &r, down);
    OffsetRect(&r, down, down);
    if (!off) {                         /* the colour */
        SetRect(&t, r.left + 4, r.top + 4, r.right - 16, r.bottom - 4);
        DrawFrame(d->hDC, &t, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
        InflateRect(&t, -1, -1);
        DrawSolidRect(d->hDC, &t, c);
    }
    SetRect(&t, r.right - 13, r.top + 3, r.right - 12, r.bottom - 3);    /* the etched line */
    DrawSolidRect(d->hDC, &t, GetSysColor(COLOR_BTNSHADOW));
    OffsetRect(&t, 1, 0);
    DrawSolidRect(d->hDC, &t, GetSysColor(COLOR_BTNHIGHLIGHT));
    x = r.right - 9;
    y = (r.top + r.bottom) / 2 - 1;
    for (i = 0; i < 3; i++) {           /* the arrow: 5, 3, 1 pixels */
        SetRect(&t, x - 2 + i, y + i, x + 3 - i, y + i + 1);
        DrawSolidRect(d->hDC, &t, arrow);
    }
    if (d->itemState & ODS_FOCUS) {
        SetRect(&t, d->rcItem.left + 3, d->rcItem.top + 3, d->rcItem.right - 3, d->rcItem.bottom - 3);
        DrawFocusRect(d->hDC, &t);
    }
}

/* The common Color dialog, from the colour shown. */
static BOOL Other(HWND owner, COLORREF FAR *c)
{
    static COLORREF custom[16];
    CHOOSECOLOR cc;
    int i;

    if (!custom[0])
        for (i = 0; i < 16; i++)
            custom[i] = RGB(255, 255, 255);
    _fmemset(&cc, 0, sizeof cc);
    cc.lStructSize = sizeof cc;
    cc.hwndOwner = owner;
    cc.rgbResult = *c;
    cc.lpCustColors = custom;
    cc.Flags = CC_RGBINIT;
    if (!ChooseColor(&cc))
        return FALSE;
    *c = cc.rgbResult;
    return TRUE;
}

BOOL ColorPickerChoose(HWND button, COLORREF FAR *c)
{
    static BOOL registered;
    WNDCLASS wc;
    RECT b, r;
    HWND w;
    MSG msg;
    int i, chosen = -1;
    BOOL done = FALSE;

    if (!registered) {
        _fmemset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = ColorPickerPaletteProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = PALETTE_CLASS;
        registered = RegisterClass(&wc);
    }
    for (hot = 0; hot < COLS * ROWS && colors[hot] != (*c & 0xFFFFFFL); hot++)
        ;
    if (hot == COLS * ROWS)
        hot = 0;
    GetWindowRect(button, &b);
    SetRect(&r, 0, 0, 2 * PAD + COLS * CELL - 2, 2 * PAD + ROWS * CELL + OTHER_H);
    w = CreateWindow(PALETTE_CLASS, "", WS_POPUP, b.left, b.bottom, r.right, r.bottom,
                     GetParent(button), NULL, g_hInst, NULL);
    if (!w)
        return FALSE;
    ShowWindow(w, SW_SHOWNA);
    UpdateWindow(w);
    SetCapture(w);
    while (!done) {
        if (!GetMessage(&msg, NULL, 0, 0)) {
            PostQuitMessage(msg.wParam);
            break;
        }
        switch (msg.message) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:            /* outside: cancel */
            GetWindowRect(w, &r);
            if (!PtInRect(&r, msg.pt))
                done = TRUE;
            break;
        case WM_LBUTTONUP:
            ScreenToClient(w, &msg.pt);
            if ((i = CellAt(msg.pt.x, msg.pt.y)) >= 0) {
                chosen = i;
                done = TRUE;
            }
            break;
        case WM_MOUSEMOVE:
            ScreenToClient(w, &msg.pt);
            if ((i = CellAt(msg.pt.x, msg.pt.y)) >= 0 && i != hot) {
                hot = i;
                InvalidateRect(w, NULL, FALSE);
            }
            break;
        case WM_KEYDOWN:
            switch (msg.wParam) {
            case VK_ESCAPE: done = TRUE; break;
            case VK_RETURN: chosen = hot; done = TRUE; break;
            case VK_LEFT:   hot = max(0, hot - 1); break;
            case VK_RIGHT:  hot = min(COLS * ROWS, hot + 1); break;
            case VK_UP:     hot = hot == COLS * ROWS ? COLS * (ROWS - 1) : max(0, hot - COLS); break;
            case VK_DOWN:   hot = min(COLS * ROWS, hot + COLS); break;
            }
            InvalidateRect(w, NULL, FALSE);
            break;
        default:
            if (msg.message < WM_KEYFIRST || msg.message > WM_MOUSELAST)
                DispatchMessage(&msg);  /* paint and timers; input stays with the palette */
        }
        if (GetCapture() != w)
            done = TRUE;
    }
    ReleaseCapture();
    DestroyWindow(w);
    if (chosen == COLS * ROWS)
        return Other(GetParent(button), c);
    if (chosen < 0)
        return FALSE;
    *c = colors[chosen];
    return TRUE;
}
