/*
 * Windows 98 look for dialogs and their controls (part of the skin, see
 * skin.c): push buttons, check boxes, radio buttons, group boxes, combo
 * boxes, edit fields and the sunken edges of lists.
 *
 * Controls keep their own behaviour (keyboard, radio groups, check state);
 * the skin only replaces how they are drawn.
 */
#include "snsystem.h"
#include "skinint.h"

enum { KIND_PUSH, KIND_CHECK, KIND_RADIO, KIND_GROUP, KIND_OTHER };

static HBRUSH faceBrush;
static HBITMAP radios[2];       /* unchecked, checked (enabled look) */

LRESULT CALLBACK __export SkinDialogProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK __export SkinButtonProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK __export SkinComboProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK __export SkinComboEditProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK __export SkinEditProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);

static void SkinControls(HWND dlg);

static int ButtonKind(HWND w)
{
    switch ((int)(GetWindowLong(w, GWL_STYLE) & 0x0F)) {
    case BS_PUSHBUTTON: case BS_DEFPUSHBUTTON:                      return KIND_PUSH;
    case BS_CHECKBOX: case BS_AUTOCHECKBOX: case BS_3STATE: case BS_AUTO3STATE: return KIND_CHECK;
    case BS_RADIOBUTTON: case BS_AUTORADIOBUTTON:                   return KIND_RADIO;
    case BS_GROUPBOX:                                                return KIND_GROUP;
    }
    return KIND_OTHER;
}

/* Text of a control; disabled text is embossed like Windows 98. */
static void Label(HDC dc, HWND w, RECT *r, UINT fmt)
{
    char text[128];
    RECT e = *r;

    GetWindowText(w, text, sizeof text);
    SelectObject(dc, DrawFont(FALSE));
    SetBkMode(dc, TRANSPARENT);
    if (IsWindowEnabled(w)) {
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        DrawText(dc, text, -1, r, fmt);
        return;
    }
    OffsetRect(&e, 1, 1);
    SetTextColor(dc, GetSysColor(COLOR_BTNHIGHLIGHT));
    DrawText(dc, text, -1, &e, fmt);
    SetTextColor(dc, GetSysColor(COLOR_BTNSHADOW));
    DrawText(dc, text, -1, r, fmt);
}

static void FocusAround(HDC dc, HWND w, RECT *text, UINT fmt)
{
    char buf[128];
    RECT r = *text;

    if (GetFocus() != w)
        return;
    GetWindowText(w, buf, sizeof buf);
    DrawText(dc, buf, -1, &r, fmt | DT_CALCRECT);
    InflateRect(&r, 1, 1);
    r.bottom++;
    DrawFocusRect(dc, &r);
}

/* 12x12 radio circles: outer ring shadow/highlight, inner ring black/face. */
static HBITMAP RadioBitmap(BOOL checked)
{
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    HBITMAP b = CreateCompatibleBitmap(screen, 12, 12), old = SelectObject(dc, b);
    COLORREF face = GetSysColor(COLOR_BTNFACE);
    int x, y;

    ReleaseDC(NULL, screen);
    for (y = 0; y < 12; y++)
        for (x = 0; x < 12; x++) {
            long dx = 2 * x - 11, dy = 2 * y - 11, d = dx * dx + dy * dy;   /* (2r)^2 */
            BOOL topLeft = dx + dy < 0;
            COLORREF c = face;
            if (d < 64)        c = (checked && d < 12) ? RGB(0, 0, 0) : GetSysColor(COLOR_WINDOW);
            else if (d < 100)  c = topLeft ? RGB(0, 0, 0) : face;
            else if (d < 144)  c = topLeft ? GetSysColor(COLOR_BTNSHADOW) : GetSysColor(COLOR_BTNHIGHLIGHT);
            SetPixel(dc, x, y, c);
        }
    SelectObject(dc, old);
    DeleteDC(dc);
    return b;
}

static void PaintPush(HDC dc, HWND w, RECT *r)
{
    BOOL pressed = (SendMessage(w, BM_GETSTATE, 0, 0L) & 0x0004) != 0;
    BOOL isDefault = (GetWindowLong(w, GWL_STYLE) & 0x0F) == BS_DEFPUSHBUTTON;
    RECT in = *r, text;

    DrawSolidRect(dc, r, GetSysColor(COLOR_BTNFACE));
    if (pressed || isDefault) {
        DrawFrame(dc, r, RGB(0, 0, 0), RGB(0, 0, 0));
        InflateRect(&in, -1, -1);
    }
    if (pressed)
        DrawFrame(dc, &in, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNSHADOW));
    else
        DrawBevel(dc, &in, FALSE);
    text = *r;
    if (pressed)
        OffsetRect(&text, 1, 1);
    Label(dc, w, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (GetFocus() == w) {
        InflateRect(&in, -3, -3);
        if (pressed)
            OffsetRect(&in, 1, 1);
        DrawFocusRect(dc, &in);
    }
}

/* A check box (13x13) or a radio button (12x12) with its top left at x, y;
   state 0 off, 1 on, 2 grey; flat: face colour inside (pressed, disabled). */
void SNSYSTEM_API SkinCheckBox(HDC dc, int x, int y, BOOL radio, int state, BOOL flat)
{
    RECT box;

    if (radio) {
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP old;
        if (!radios[state ? 1 : 0])
            radios[state ? 1 : 0] = RadioBitmap(state != 0);
        old = SelectObject(mem, radios[state ? 1 : 0]);
        BitBlt(dc, x, y, 12, 12, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
        return;
    }
    SetRect(&box, x, y, x + 13, y + 13);
    DrawBevel(dc, &box, TRUE);
    InflateRect(&box, -2, -2);
    DrawSolidRect(dc, &box, GetSysColor(flat || state == 2 ? COLOR_BTNFACE : COLOR_WINDOW));
    if (state) {                        /* the Windows 98 tick: 7 columns, 3 pixels tall */
        int i, x0 = box.left + 1, y0 = box.top + 1;
        RECT p;
        for (i = 0; i < 7; i++) {
            int ty = y0 + (i < 3 ? i + 2 : 6 - i);
            SetRect(&p, x0 + i, ty, x0 + i + 1, ty + 3);
            DrawSolidRect(dc, &p, state == 2 ? GetSysColor(COLOR_BTNSHADOW) : RGB(0, 0, 0));
        }
    }
}

static void PaintCheck(HDC dc, HWND w, RECT *r, BOOL radio)
{
    int state = (int)SendMessage(w, BM_GETCHECK, 0, 0L);
    BOOL pressed = (SendMessage(w, BM_GETSTATE, 0, 0L) & 0x0004) != 0;
    int size = radio ? 12 : 13;
    RECT text;

    DrawSolidRect(dc, r, GetSysColor(COLOR_BTNFACE));
    SkinCheckBox(dc, r->left, r->top + (r->bottom - r->top - size) / 2, radio, state, pressed || !IsWindowEnabled(w));
    text = *r;
    text.left += size + 4;
    Label(dc, w, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FocusAround(dc, w, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* Etched frame with the caption in a gap at the top left. */
static void PaintGroup(HDC dc, HWND w, RECT *r)
{
    char text[128];
    RECT t;

    SelectObject(dc, DrawFont(FALSE));
    GetWindowText(w, text, sizeof text);
    DrawGroupFrame(dc, r, *text ? LOWORD(GetTextExtent(dc, text, lstrlen(text))) + 4 : 0, &t);
    if (*text) {
        t.left += 2;
        Label(dc, w, &t, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
}

/* Off the screen, then onto it at once: no face without its check or text. */
static void PaintButton(HWND w)
{
    PAINTSTRUCT ps;
    RECT r;
    OFFSCREEN o;
    HDC dc;
    HFONT old;

    BeginPaint(w, &ps);
    GetClientRect(w, &r);
    if (ButtonKind(w) == KIND_GROUP) {  /* see-through (only its frame and caption): straight on */
        o.mem = NULL;
        dc = ps.hdc;
    } else {
        dc = DrawOffscreenBegin(&o, ps.hdc, &r);
    }
    old = SelectObject(dc, DrawFont(FALSE));
    switch (ButtonKind(w)) {
    case KIND_PUSH:  PaintPush(dc, w, &r); break;
    case KIND_CHECK: PaintCheck(dc, w, &r, FALSE); break;
    case KIND_RADIO: PaintCheck(dc, w, &r, TRUE); break;
    case KIND_GROUP: PaintGroup(dc, w, &r); break;
    }
    SelectObject(dc, old);
    DrawOffscreenEnd(&o);
    EndPaint(w, &ps);
}

LRESULT CALLBACK __export SkinButtonProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res;
    WORD state;

    switch (msg) {
    case WM_PAINT:
        PaintButton(w);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        /* The mouse going over: Windows 3.1 draws nothing then (only while
           the button is held, going on or off it), so ours is drawn again
           only when that changed the button: no repaint each move. (Its
           drawing is not turned off for the move: a held button hidden by
           WM_SETREDRAW loses the mouse, and the click with it.) */
        state = (WORD)SendMessage(w, BM_GETSTATE, 0, 0L);
        res = SkinCallOld(w, msg, wp, lp);
        if (IsWindow(w) && (WORD)SendMessage(w, BM_GETSTATE, 0, 0L) != state) {
            InvalidateRect(w, NULL, FALSE);
            UpdateWindow(w);
        }
        return res;
    }
    res = SkinCallOld(w, msg, wp, lp);
    switch (msg) {                      /* the control drew its old look: redraw ours */
    case BM_SETSTATE: case BM_SETCHECK: case BM_SETSTYLE:
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: case WM_SETTEXT:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_KEYDOWN: case WM_KEYUP:
        if (IsWindow(w)) {
            InvalidateRect(w, NULL, FALSE);
            UpdateWindow(w);
        }
    }
    return res;
}

/* Combo boxes: sunken edge and a Windows 98 drop-down button over the old ones. */
/* Where an editable combo box's edit lies, in the combo box. */
static BOOL ComboEdit(HWND w, RECT *r)
{
    HWND e = GetWindow(w, GW_CHILD);

    if ((GetWindowLong(w, GWL_STYLE) & 3) != CBS_DROPDOWN || !e)
        return FALSE;
    GetWindowRect(e, r);
    ScreenToClient(w, (POINT FAR *)&r->left);
    ScreenToClient(w, (POINT FAR *)&r->right);
    return TRUE;
}

/* Windows 98's other raised edge, of scroll arrows and menus: light and
   black outside, white and grey inside. r ends inside it. */
void SkinRaisedEdge(HDC dc, RECT *r)
{
    DrawFrame(dc, r, GetSysColor(COLOR_BTNFACE), RGB(0, 0, 0));
    InflateRect(r, -1, -1);
    DrawFrame(dc, r, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
    InflateRect(r, -1, -1);
}

static void PaintCombo(HWND w)
{
    HDC dc = GetDC(w);
    RECT r, b, e, in;
    int cx = GetSystemMetrics(SM_CXVSCROLL);
    BOOL down = (BOOL)SendMessage(w, CB_GETDROPPEDSTATE, 0, 0L);

    GetClientRect(w, &r);
    DrawBevel(dc, &r, TRUE);
    if (ComboEdit(w, &e)) {             /* the white field around the edit */
        SetRect(&in, r.left + 2, r.top + 2, r.right - 2 - cx, r.bottom - 2);
        SaveDC(dc);
        ExcludeClipRect(dc, e.left, e.top, e.right, e.bottom);
        DrawSolidRect(dc, &in, GetSysColor(COLOR_WINDOW));
        RestoreDC(dc, -1);
    }
    SetRect(&b, r.right - 2 - cx, r.top + 2, r.right - 2, r.bottom - 2);
    DrawSolidRect(dc, &b, GetSysColor(COLOR_BTNFACE));
    if (down)
        DrawFrame(dc, &b, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNSHADOW));
    else                                /* a push button's white would vanish on the field */
        SkinRaisedEdge(dc, &b);         /* (b keeps its centre) */
    {                                                   /* 7x4 arrow */
        int i, x = (b.left + b.right) / 2 - 3 + down, y = (b.top + b.bottom) / 2 - 2 + down;
        RECT a;
        for (i = 0; i < 4; i++) {
            SetRect(&a, x + i, y + i, x + 7 - i, y + i + 1);
            DrawSolidRect(dc, &a, IsWindowEnabled(w) ? RGB(0, 0, 0) : GetSysColor(COLOR_BTNSHADOW));
        }
    }
    ReleaseDC(w, dc);
}

/* Windows 3.1 redraws the field, its black border too, after any of these. */
LRESULT CALLBACK __export SkinComboProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res = SkinCallOld(w, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_SETFOCUS:
    case WM_KILLFOCUS: case WM_ENABLE: case CB_SHOWDROPDOWN: case WM_KEYDOWN:
    case WM_CHAR: case WM_SETTEXT: case WM_SETFONT: case WM_SIZE:
    case CB_SETCURSEL: case CB_SELECTSTRING: case CB_RESETCONTENT:
        if (IsWindow(w))
            PaintCombo(w);
    }
    return res;
}

/* An editable combo box's edit has a border of its own in Windows 3.1: it
   becomes the white field, and the field's sunken edge where the edit meets
   the combo box's (the combo box draws the rest of it, and its button). */
static void PaintComboEdit(HWND e)
{
    HWND w = GetParent(e);
    HDC dc = GetDC(e);
    RECT r, at, c, l;
    COLORREF white = GetSysColor(COLOR_WINDOW);

    GetClientRect(e, &r);
    GetClientRect(w, &c);
    ComboEdit(w, &at);
    DrawFrame(dc, &r, white, white);
    InflateRect(&r, -1, -1);
    DrawFrame(dc, &r, white, white);
    InflateRect(&r, 1, 1);
    if (at.bottom >= c.bottom) {
        SetRect(&l, r.left, r.bottom - 1, r.right, r.bottom);
        DrawSolidRect(dc, &l, GetSysColor(COLOR_BTNHIGHLIGHT));
        OffsetRect(&l, 0, -1);
        DrawSolidRect(dc, &l, GetSysColor(COLOR_BTNFACE));
    }
    if (at.top <= 0) {
        SetRect(&l, r.left, r.top, r.right, r.top + 1);
        DrawSolidRect(dc, &l, GetSysColor(COLOR_BTNSHADOW));
        OffsetRect(&l, 0, 1);
        DrawSolidRect(dc, &l, RGB(0, 0, 0));
    }
    if (at.left <= 0) {
        SetRect(&l, r.left, r.top, r.left + 1, r.bottom - 1);
        DrawSolidRect(dc, &l, GetSysColor(COLOR_BTNSHADOW));
        SetRect(&l, r.left + 1, r.top + 1, r.left + 2, r.bottom - 2);
        DrawSolidRect(dc, &l, RGB(0, 0, 0));
    }
    ReleaseDC(e, dc);
}

LRESULT CALLBACK __export SkinComboEditProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res = SkinCallOld(w, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: case WM_SIZE: case WM_MOVE:
        if (IsWindow(w))
            PaintComboEdit(w);
    }
    return res;
}

/* An edit field: Windows 3.1's edit removes WS_BORDER and draws a one pixel
   border itself, its text kept 3 pixels in. The Windows 98 sunken edge goes
   over that border and the pixel inside it. */
LRESULT CALLBACK __export SkinEditProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res = SkinCallOld(w, msg, wp, lp);
    RECT r;
    HDC dc;

    switch (msg) {
    case WM_PAINT: case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: case WM_SIZE:
        if (IsWindow(w)) {
            dc = GetDC(w);
            GetClientRect(w, &r);
            DrawBevel(dc, &r, TRUE);
            ReleaseDC(w, dc);
        }
    }
    return res;
}

/* Edit fields with Windows 3.1's own border: their text starts 3 pixels in. */
static BOOL BorderedEdit(HWND c)
{
    char cls[16];
    RECT f;

    GetClassName(GetParent(c), cls, sizeof cls);
    if (!lstrcmpi(cls, "ComboBox"))
        return FALSE;
    SendMessage(c, EM_GETRECT, 0, (LPARAM)(RECT FAR *)&f);
    return f.left >= 2 && f.top >= 2;
}

/* Sunken edges for the lists of a dialog, drawn around them. */
static void FrameChildren(HWND dlg)
{
    HDC dc = GetDC(dlg);
    HWND c;
    RECT r;

    for (c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        if (GetProp(c, FRAMED) && IsWindowVisible(c)) {
            GetWindowRect(c, &r);
            ScreenToClient(dlg, (POINT FAR *)&r.left);
            ScreenToClient(dlg, (POINT FAR *)&r.right);
            InflateRect(&r, 2, 2);
            DrawBevel(dc, &r, TRUE);
        }
    ReleaseDC(dlg, dc);
}

LRESULT CALLBACK __export SkinDialogProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res;

    switch (msg) {
    case WM_CTLCOLOR:
        if (HIWORD(lp) == CTLCOLOR_EDIT || HIWORD(lp) == CTLCOLOR_LISTBOX)
            break;
        SetBkColor((HDC)wp, GetSysColor(COLOR_BTNFACE));
        SetTextColor((HDC)wp, GetSysColor(COLOR_BTNTEXT));
        return (LRESULT)(UINT)faceBrush;
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(w, &r);
        DrawSolidRect((HDC)wp, &r, GetSysColor(COLOR_BTNFACE));
        return 1;
    }
    }
    res = SkinFrameProc(w, msg, wp, lp);        /* caption and frame, then the dialog */
    if (msg == WM_PAINT)
        FrameChildren(w);
    else if (msg == WM_INITDIALOG)              /* its controls exist now */
        SkinControls(w);
    return res;
}

void SNSYSTEM_API SkinEdges(HWND dlg)
{
    FrameChildren(dlg);
}

/* Windows 98 controls for the children of a dialog (safe to repeat). */
void SNSYSTEM_API SkinControl(HWND c)
{
    char cls[16];
    LONG style = GetWindowLong(c, GWL_STYLE);

    if (!faceBrush)
        faceBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    GetClassName(c, cls, sizeof cls);
    SendMessage(c, WM_SETFONT, (WPARAM)DrawFont(FALSE), 0L);
    if (!lstrcmpi(cls, "Button") && ButtonKind(c) != KIND_OTHER) {
        SkinSubclass(c, SkinButtonProc);
    } else if (!lstrcmpi(cls, "ComboBox")) {
        SkinSubclass(c, SkinComboProc);
        if ((style & 3) == CBS_DROPDOWN && GetWindow(c, GW_CHILD)) {
            SkinSubclass(GetWindow(c, GW_CHILD), SkinComboEditProc);
            InvalidateRect(GetWindow(c, GW_CHILD), NULL, FALSE);
        }
    } else if (!lstrcmpi(cls, "Edit") && BorderedEdit(c)) {
        SkinSubclass(c, SkinEditProc);
        InvalidateRect(c, NULL, FALSE);
    } else if ((!lstrcmpi(cls, "ListBox") || !lstrcmpi(cls, "SbIconView") || !lstrcmpi(cls, "SbTree")) &&
               (style & WS_BORDER)) {          /* Snowball's file views */
        RECT r;                         /* the 2-pixel edge replaces the 1-pixel border */
        HWND parent = GetParent(c);
        GetWindowRect(c, &r);
        ScreenToClient(parent, (POINT FAR *)&r.left);
        ScreenToClient(parent, (POINT FAR *)&r.right);
        SetWindowLong(c, GWL_STYLE, style & ~WS_BORDER);
        SetWindowPos(c, NULL, r.left + 1, r.top + 1, r.right - r.left - 2, r.bottom - r.top - 2,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SetProp(c, FRAMED, (HANDLE)1);
    }
}

/* FRAMED marks a list that is not subclassed: nothing else removes it, and a
   property left on a destroyed window stays in USER's heap. */
void SkinForget(HWND w)
{
    RemoveProp(w, FRAMED);
}

static void SkinControls(HWND dlg)
{
    HWND c;
    for (c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        SkinControl(c);
}

/* Gives an already initialized dialog and its controls the Windows 98 look. */
void SNSYSTEM_API SkinDialog(HWND dlg)
{
    SkinControls(dlg);
    SkinSubclass(dlg, SkinDialogProc);
    SetWindowPos(dlg, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect(dlg, NULL, TRUE);
}
