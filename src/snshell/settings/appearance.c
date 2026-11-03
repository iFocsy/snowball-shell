/*
 * Display Properties' Appearance page: Windows 3.1's colour schemes
 * (CONTROL.INI [color schemes], the one in use in [current]) and each
 * item's colours, chosen with Windows 98's colour button, over a picture of
 * an inactive and an active window and a message box in those colours.
 * Applied, the colours are Windows' (SetSysColors) and WIN.INI [colors].
 */
#include "internal.h"

#define SCHEMES     "color schemes"
#define NCOLORS     21

/* WIN.INI [colors] and the schemes list the colours in this order. */
static const struct { LPCSTR key; int index; } keys[NCOLORS] = {
    { "Background", COLOR_BACKGROUND }, { "AppWorkspace", COLOR_APPWORKSPACE }, { "Window", COLOR_WINDOW },
    { "WindowText", COLOR_WINDOWTEXT }, { "Menu", COLOR_MENU }, { "MenuText", COLOR_MENUTEXT },
    { "ActiveTitle", COLOR_ACTIVECAPTION }, { "InactiveTitle", COLOR_INACTIVECAPTION },
    { "TitleText", COLOR_CAPTIONTEXT }, { "ActiveBorder", COLOR_ACTIVEBORDER },
    { "InactiveBorder", COLOR_INACTIVEBORDER }, { "WindowFrame", COLOR_WINDOWFRAME },
    { "Scrollbar", COLOR_SCROLLBAR }, { "ButtonFace", COLOR_BTNFACE }, { "ButtonShadow", COLOR_BTNSHADOW },
    { "ButtonText", COLOR_BTNTEXT }, { "GrayText", COLOR_GRAYTEXT }, { "Hilight", COLOR_HIGHLIGHT },
    { "HilightText", COLOR_HIGHLIGHTTEXT }, { "InactiveTitleText", COLOR_INACTIVECAPTIONTEXT },
    { "ButtonHilight", COLOR_BTNHIGHLIGHT },
};

/* Windows 98's items, each with its colour and perhaps its text's. */
static const struct { LPCSTR name; int color, text; } items[] = {
    { "3D Objects", COLOR_BTNFACE, COLOR_BTNTEXT },
    { "Active Title Bar", COLOR_ACTIVECAPTION, COLOR_CAPTIONTEXT },
    { "Active Window Border", COLOR_ACTIVEBORDER, -1 },
    { "Application Background", COLOR_APPWORKSPACE, -1 },
    { "Desktop", COLOR_BACKGROUND, -1 },
    { "Inactive Title Bar", COLOR_INACTIVECAPTION, COLOR_INACTIVECAPTIONTEXT },
    { "Inactive Window Border", COLOR_INACTIVEBORDER, -1 },
    { "Menu", COLOR_MENU, COLOR_MENUTEXT },
    { "Scrollbar", COLOR_SCROLLBAR, -1 },
    { "Selected Items", COLOR_HIGHLIGHT, COLOR_HIGHLIGHTTEXT },
    { "Window", COLOR_WINDOW, COLOR_WINDOWTEXT },
};
#define NITEMS      (sizeof items / sizeof items[0])
#define ITEM_DESKTOP 4

static COLORREF colors[NCOLORS];        /* by COLOR_* index */

static int Item(HWND dlg)
{
    int i = (int)SendDlgItemMessage(dlg, IDC_AP_ITEM, CB_GETCURSEL, 0, 0L);
    return i >= 0 && i < NITEMS ? i : ITEM_DESKTOP;
}

static void PreviewRect(HWND dlg, RECT *r)
{
    SetRect(r, 14, 26, 238, 134);
    MapDialogRect(dlg, r);
}

static void Repaint(HWND dlg)
{
    RECT r;

    PreviewRect(dlg, &r);
    InvalidateRect(dlg, &r, FALSE);
    InvalidateRect(GetDlgItem(dlg, IDC_AP_COLOR), NULL, FALSE);
    InvalidateRect(GetDlgItem(dlg, IDC_AP_TEXT), NULL, FALSE);
}

static void ShowItem(HWND dlg)
{
    BOOL text = items[Item(dlg)].text >= 0;

    EnableWindow(GetDlgItem(dlg, IDC_AP_TEXT), text);
    EnableWindow(GetDlgItem(dlg, IDC_AP_TEXTL), text);
    Repaint(dlg);
}

/* A scheme's colours: hexadecimal, comma separated, in [colors] order. */
static BOOL LoadScheme(LPCSTR name)
{
    char value[256];
    LPSTR p = value;
    DWORD c;
    int k;

    if (!GetPrivateProfileString(SCHEMES, name, "", value, sizeof value, "CONTROL.INI"))
        return FALSE;
    for (k = 0; k < NCOLORS && *p; k++) {
        for (c = 0; *p && *p != ','; p++)
            c = c * 16 + (*p <= '9' ? *p - '0' : (*p | 0x20) - 'a' + 10);
        colors[keys[k].index] = c & 0xFFFFFFL;    /* the high byte marks palette colours */
        if (*p == ',')
            p++;
    }
    return TRUE;
}

static void SchemeName(HWND dlg, LPSTR out, int size)
{
    HWND cb = GetDlgItem(dlg, IDC_AP_SCHEME);
    int i = (int)SendMessage(cb, CB_GETCURSEL, 0, 0L);

    out[0] = '\0';
    if (i >= 0 && (int)SendMessage(cb, CB_GETLBTEXTLEN, i, 0L) < size)
        SendMessage(cb, CB_GETLBTEXT, i, (LPARAM)out);
}

void AppearanceInit(HWND dlg)
{
    char now[64];
    HGLOBAL h;
    LPSTR names, p;
    int i;

    for (i = 0; i < NCOLORS; i++)
        colors[keys[i].index] = GetSysColor(keys[i].index);
    if ((h = GlobalAlloc(GMEM_MOVEABLE, 4096)) != NULL) {
        names = GlobalLock(h);
        GetPrivateProfileString(SCHEMES, NULL, "", names, 4096, "CONTROL.INI");
        for (p = names; *p; p += lstrlen(p) + 1)
            SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_ADDSTRING, 0, (LPARAM)p);
        GlobalUnlock(h);
        GlobalFree(h);
    }
    GetPrivateProfileString("current", SCHEMES, "", now, sizeof now, "CONTROL.INI");
    SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)(LPSTR)now);
    for (i = 0; i < NITEMS; i++)
        SendDlgItemMessage(dlg, IDC_AP_ITEM, CB_ADDSTRING, 0, (LPARAM)items[i].name);
    SendDlgItemMessage(dlg, IDC_AP_ITEM, CB_SETCURSEL, ITEM_DESKTOP, 0L);
    ShowItem(dlg);
}

static void SaveScheme(HWND dlg)
{
    char name[64], value[NCOLORS * 9];
    int k;

    SchemeName(dlg, name, sizeof name);
    if (!PromptDialogShow(dlg, "Save Scheme", "&Save this color scheme as:", name, sizeof name) || !*name)
        return;
    value[0] = '\0';
    for (k = 0; k < NCOLORS; k++)
        wsprintf(value + lstrlen(value), k ? ",%lX" : "%lX", colors[keys[k].index]);
    WritePrivateProfileString(SCHEMES, name, value, "CONTROL.INI");
    if (SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPSTR)name) == CB_ERR)
        SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
    SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)(LPSTR)name);
}

static void DeleteScheme(HWND dlg)
{
    char name[64], text[128];
    int i = (int)SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_GETCURSEL, 0, 0L);

    SchemeName(dlg, name, sizeof name);
    if (!*name)
        return;
    wsprintf(text, "Are you sure you want to remove the scheme '%s'?", (LPSTR)name);
    if (MessageBox(dlg, text, "Delete Scheme", MB_ICONQUESTION | MB_YESNO) != IDYES)
        return;
    WritePrivateProfileString(SCHEMES, name, NULL, "CONTROL.INI");
    SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_DELETESTRING, i, 0L);
}

BOOL AppearanceCommand(HWND dlg, UINT id, UINT code)
{
    char name[64];
    COLORREF FAR *c;

    switch (id) {
    case IDC_AP_SCHEME:
        if (code != CBN_SELCHANGE)
            return FALSE;
        SchemeName(dlg, name, sizeof name);
        LoadScheme(name);
        Repaint(dlg);
        return TRUE;
    case IDC_AP_ITEM:
        if (code == CBN_SELCHANGE)
            ShowItem(dlg);
        return FALSE;
    case IDC_AP_COLOR:
    case IDC_AP_TEXT:
        c = &colors[id == IDC_AP_COLOR ? items[Item(dlg)].color : items[Item(dlg)].text];
        if (code != BN_CLICKED || !ColorPickerChoose(GetDlgItem(dlg, id), c))
            return FALSE;
        SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_SETCURSEL, (WPARAM)-1, 0L);   /* no scheme's now */
        Repaint(dlg);
        return TRUE;
    case IDC_AP_SAVE:
        SaveScheme(dlg);
        return FALSE;
    case IDC_AP_DELETE:
        DeleteScheme(dlg);
        return FALSE;
    }
    return FALSE;
}

BOOL AppearanceDrawItem(HWND dlg, const DRAWITEMSTRUCT FAR *d)
{
    int i = Item(dlg);

    if (d->CtlID == IDC_AP_COLOR)
        ColorPickerDraw(d, colors[items[i].color]);
    else if (d->CtlID == IDC_AP_TEXT)
        ColorPickerDraw(d, items[i].text >= 0 ? colors[items[i].text] : colors[COLOR_BTNFACE]);
    else
        return FALSE;
    return TRUE;
}

void AppearanceApply(HWND dlg)
{
    char name[64], value[16];
    int indices[NCOLORS], k;
    COLORREF values[NCOLORS];

    for (k = 0; k < NCOLORS; k++) {
        indices[k] = keys[k].index;
        values[k] = colors[keys[k].index];
        wsprintf(value, "%d %d %d", GetRValue(values[k]), GetGValue(values[k]), GetBValue(values[k]));
        WriteProfileString("colors", keys[k].key, value);
    }
    SchemeName(dlg, name, sizeof name);
    WritePrivateProfileString("current", SCHEMES, name, "CONTROL.INI");
    SetSysColors(NCOLORS, indices, values);
}

/* ---- the picture ---- */

static void Text(HDC dc, LPCSTR s, RECT *r, COLORREF c, BOOL bold, UINT align)
{
    HFONT old = SelectObject(dc, DrawFont(bold));

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    DrawText(dc, s, -1, r, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, old);
}

/* A window's frame in the scheme's colours; r becomes its inside. */
static void Frame(HDC dc, RECT *r, BOOL active)
{
    COLORREF border = colors[active ? COLOR_ACTIVEBORDER : COLOR_INACTIVEBORDER];

    DrawFrame(dc, r, colors[COLOR_BTNFACE], RGB(0, 0, 0));
    InflateRect(r, -1, -1);
    DrawFrame(dc, r, colors[COLOR_BTNHIGHLIGHT], colors[COLOR_BTNSHADOW]);
    InflateRect(r, -1, -1);
    DrawFrame(dc, r, border, border);
    InflateRect(r, -1, -1);
    DrawFrame(dc, r, border, border);
    InflateRect(r, -1, -1);
}

/* A caption across the top of r, which then starts below it. */
static void Caption(HDC dc, RECT *r, LPCSTR title, BOOL active, BOOL buttons)
{
    int h = GetSystemMetrics(SM_CYCAPTION) - 1, b = h - 4;
    RECT c, t;

    SetRect(&c, r->left, r->top, r->right, r->top + h);
    DrawGradient(dc, &c, colors[active ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION],
                 active ? CAPTION_END_ACTIVE : CAPTION_END_INACTIVE, FALSE);
    t = c;
    t.left += 4;
    Text(dc, title, &t, colors[active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT], TRUE, DT_LEFT);
    SetRect(&t, c.right - 2 - b - 2, c.top + 2, c.right - 2, c.top + 2 + b);
    SkinCaptionButton(dc, &t, SKIN_CLOSE);
    if (buttons) {
        OffsetRect(&t, -(b + 4), 0);
        SkinCaptionButton(dc, &t, SKIN_MAX);
        OffsetRect(&t, -(b + 2), 0);
        SkinCaptionButton(dc, &t, SKIN_MIN);
    }
    r->top = c.bottom + 1;
}

/* An arrow of 4, 3, 2 and 1 pixels, up or down, centred in r. */
static void Arrow(HDC dc, const RECT *r, BOOL up)
{
    int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2 - 2, i;
    RECT t;

    for (i = 0; i < 4; i++) {
        SetRect(&t, x - 3 + i, y + (up ? 3 - i : i), x + 4 - i, y + (up ? 4 - i : i + 1));
        DrawSolidRect(dc, &t, colors[COLOR_BTNTEXT]);
    }
}

void AppearancePaint(HWND dlg, HDC dc)
{
    RECT a, r, t, s;
    int h = GetSystemMetrics(SM_CYCAPTION) - 1, sb = GetSystemMetrics(SM_CXVSCROLL), x;

    PreviewRect(dlg, &a);
    DrawBevel(dc, &a, TRUE);
    InflateRect(&a, -2, -2);
    DrawSolidRect(dc, &a, colors[COLOR_BACKGROUND]);

    SetRect(&r, a.left + 8, a.top + 8, a.right - 40, a.top + 8 + h + 8);           /* inactive */
    DrawSolidRect(dc, &r, colors[COLOR_BTNFACE]);
    Frame(dc, &r, FALSE);
    Caption(dc, &r, "Inactive Window", FALSE, TRUE);

    SetRect(&r, a.left + 16, a.top + h + 12, a.right - 24, a.bottom - 16);         /* active */
    DrawSolidRect(dc, &r, colors[COLOR_BTNFACE]);
    Frame(dc, &r, TRUE);
    Caption(dc, &r, "Active Window", TRUE, TRUE);
    SetRect(&t, r.left, r.top, r.right, r.top + h);                                 /* its menu */
    DrawSolidRect(dc, &t, colors[COLOR_MENU]);
    x = t.left + 6;
    SetRect(&s, x, t.top, x + 44, t.bottom);
    Text(dc, "Normal", &s, colors[COLOR_MENUTEXT], FALSE, DT_LEFT);
    OffsetRect(&s, 44, 0);
    Text(dc, "Disabled", &s, colors[COLOR_GRAYTEXT], FALSE, DT_LEFT);
    SetRect(&s, s.right + 2, t.top + 1, s.right + 50, t.bottom - 1);
    DrawSolidRect(dc, &s, colors[COLOR_HIGHLIGHT]);
    s.left += 4;
    Text(dc, "Selected", &s, colors[COLOR_HIGHLIGHTTEXT], FALSE, DT_LEFT);
    SetRect(&t, r.left + 2, t.bottom + 2, r.right - 2, r.bottom - 2);               /* its contents */
    DrawBevel(dc, &t, TRUE);
    InflateRect(&t, -2, -2);
    DrawSolidRect(dc, &t, colors[COLOR_WINDOW]);
    SetRect(&s, t.right - sb, t.top, t.right, t.bottom);                            /* scroll bar */
    DrawSolidRect(dc, &s, colors[COLOR_SCROLLBAR]);
    SetRect(&s, t.right - sb, t.top, t.right, t.top + sb);
    DrawSolidRect(dc, &s, colors[COLOR_BTNFACE]);
    DrawBevel(dc, &s, FALSE);
    Arrow(dc, &s, TRUE);
    SetRect(&s, t.right - sb, t.bottom - sb, t.right, t.bottom);
    DrawSolidRect(dc, &s, colors[COLOR_BTNFACE]);
    DrawBevel(dc, &s, FALSE);
    Arrow(dc, &s, FALSE);
    SetRect(&s, t.left + 4, t.top + 2, t.right - sb - 4, t.top + 2 + h);
    Text(dc, "Window Text", &s, colors[COLOR_WINDOWTEXT], FALSE, DT_LEFT);

    x = (a.left + a.right) / 2;                                                     /* message box */
    SetRect(&r, x - 70, a.bottom - 2 * h - 34, x + 50, a.bottom - 6);
    DrawSolidRect(dc, &r, colors[COLOR_BTNFACE]);
    Frame(dc, &r, TRUE);
    Caption(dc, &r, "Message Box", TRUE, FALSE);
    SetRect(&s, r.left + 6, r.top, r.right, r.top + h);
    Text(dc, "Message Text", &s, colors[COLOR_WINDOWTEXT], FALSE, DT_LEFT);
    SetRect(&s, x - 30, r.bottom - 22, x + 10, r.bottom - 4);
    DrawBevel(dc, &s, FALSE);
    Text(dc, "OK", &s, colors[COLOR_BTNTEXT], FALSE, DT_CENTER);
}
