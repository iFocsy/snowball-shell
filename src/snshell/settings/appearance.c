/*
 * Display Properties' Appearance page: the colour schemes (every theme
 * that has colours, themes.c; the one in use is CONTROL.INI's [current])
 * and each item's colours, chosen with Windows 98's colour button, over a
 * picture of an inactive and an active window and a message box in those
 * colours (colorscheme.c). Save As keeps the colours as a theme (Custom
 * for a new one); Delete takes them out of the theme, and the theme too if
 * that was all it had. Applied, the colours are Windows'. The Windows 98
 * colours Windows 3.1 has no index for (title bars' Color 2, tooltips)
 * follow the scheme until one is chosen.
 */
#include "internal.h"

/* Windows 98's items: their colour, Color 2 and text's (-1: none), as
   COLOR_*, or X(sc): one of the colours Windows 3.1 has no index for. */
#define X(sc)       (100 + (sc))
static const struct { LPCSTR name; int color, color2, text; } items[] = {
    { "3D Objects", COLOR_BTNFACE, -1, COLOR_BTNTEXT },
    { "Active Title Bar", COLOR_ACTIVECAPTION, X(SC_GRADIENT_ACTIVE), COLOR_CAPTIONTEXT },
    { "Active Window Border", COLOR_ACTIVEBORDER, -1, -1 },
    { "Application Background", COLOR_APPWORKSPACE, -1, -1 },
    { "Desktop", COLOR_BACKGROUND, -1, -1 },
    { "Inactive Title Bar", COLOR_INACTIVECAPTION, X(SC_GRADIENT_INACTIVE), COLOR_INACTIVECAPTIONTEXT },
    { "Inactive Window Border", COLOR_INACTIVEBORDER, -1, -1 },
    { "Menu", COLOR_MENU, -1, COLOR_MENUTEXT },
    { "Scrollbar", COLOR_SCROLLBAR, -1, -1 },
    { "Selected Items", COLOR_HIGHLIGHT, -1, COLOR_HIGHLIGHTTEXT },
    { "ToolTip", X(SC_INFO), -1, X(SC_INFOTEXT) },
    { "Window", COLOR_WINDOW, -1, COLOR_WINDOWTEXT },
};
#define NITEMS      (sizeof items / sizeof items[0])
#define ITEM_DESKTOP 4

static COLORSCHEME scheme;              /* as chosen on the page */

/* A colour of an item, by COLOR_* or X(SC_*). */
static COLORREF Color(int i)
{
    return i < X(0) ? scheme.colors[i] : ColorSchemeColor(&scheme, COLOR_KEYS_31 + i - X(0));
}

static BOOL Choose(HWND button, int i)
{
    COLORREF c = Color(i);

    if (!ColorPickerChoose(button, &c))
        return FALSE;
    if (i < X(0))
        scheme.colors[i] = c;
    else
        ColorSchemeSetColor(&scheme, COLOR_KEYS_31 + i - X(0), c);
    return TRUE;
}

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
    InvalidateRect(GetDlgItem(dlg, IDC_AP_COLOR2), NULL, FALSE);
    InvalidateRect(GetDlgItem(dlg, IDC_AP_TEXT), NULL, FALSE);
}

static void ShowItem(HWND dlg)
{
    BOOL text = items[Item(dlg)].text >= 0;
    BOOL two = items[Item(dlg)].color2 >= 0 && DrawScreenDepth() > 8;     /* no gradients: no Color 2 */

    EnableWindow(GetDlgItem(dlg, IDC_AP_TEXT), text);
    EnableWindow(GetDlgItem(dlg, IDC_AP_TEXTL), text);
    EnableWindow(GetDlgItem(dlg, IDC_AP_COLOR2), two);
    EnableWindow(GetDlgItem(dlg, IDC_AP_COLOR2L), two);
    Repaint(dlg);
}

/* A theme's colours, into the page's. */
static BOOL LoadScheme(LPCSTR name)
{
    char value[COLOR_TEXT];

    return ThemeGet(name, "Colors", value, sizeof value) && ColorSchemeParse(value, &scheme);
}

static void SchemeName(HWND dlg, LPSTR out, int size)
{
    HWND cb = GetDlgItem(dlg, IDC_AP_SCHEME);
    int i = (int)SendMessage(cb, CB_GETCURSEL, 0, 0L);

    out[0] = '\0';
    if (i >= 0 && (int)SendMessage(cb, CB_GETLBTEXTLEN, i, 0L) < size)
        SendMessage(cb, CB_GETLBTEXT, i, (LPARAM)out);
}

static void CanDelete(HWND dlg)
{
    char name[THEME_NAME];

    SchemeName(dlg, name, sizeof name);
    EnableWindow(GetDlgItem(dlg, IDC_AP_DELETE), *name != '\0');
}

void AppearanceInit(HWND dlg)
{
    char now[THEME_NAME], name[THEME_NAME];
    PATHLIST names, groups;
    int i;

    ColorSchemeCurrent(&scheme);
    PathListInit(&names);
    PathListInit(&groups);
    ThemesList(&names, &groups);
    for (i = 0; PathListAt(&names, i, name); i++)
        if (ThemeParts(name) & THEME_COLORS)
            SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
    PathListFree(&names);
    PathListFree(&groups);
    GetPrivateProfileString("current", "color schemes", "", now, sizeof now, "CONTROL.INI");
    SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)(LPSTR)now);
    CanDelete(dlg);
    for (i = 0; i < NITEMS; i++)
        SendDlgItemMessage(dlg, IDC_AP_ITEM, CB_ADDSTRING, 0, (LPARAM)items[i].name);
    SendDlgItemMessage(dlg, IDC_AP_ITEM, CB_SETCURSEL, ITEM_DESKTOP, 0L);
    ShowItem(dlg);
}

static void SaveScheme(HWND dlg)
{
    char name[THEME_NAME], value[COLOR_TEXT];

    SchemeName(dlg, name, sizeof name);
    if (!PromptDialogShow(dlg, "Save Scheme", "&Save this color scheme as:", name, sizeof name) || !*name)
        return;
    ThemeAdd(name, "");                 /* (a new one: Custom) */
    ColorSchemeFormat(&scheme, value);
    ThemeSet(name, "Colors", value);
    if (SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPSTR)name) == CB_ERR)
        SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
    SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)(LPSTR)name);
    CanDelete(dlg);
}

static BOOL DeleteScheme(HWND dlg)
{
    char name[THEME_NAME], text[THEME_NAME + 64];
    int i = (int)SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_GETCURSEL, 0, 0L);

    SchemeName(dlg, name, sizeof name);
    if (!*name)
        return FALSE;
    wsprintf(text, "Are you sure you want to remove the scheme '%s'?", (LPSTR)name);
    if (MessageBox(dlg, text, "Delete Scheme", MB_ICONQUESTION | MB_YESNO) != IDYES)
        return FALSE;
    ThemeDrop(name, THEME_COLORS);
    if (!ThemeParts(name))
        ThemeDelete(name);
    SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_DELETESTRING, i, 0L);
    CanDelete(dlg);
    return TRUE;
}

BOOL AppearanceCommand(HWND dlg, UINT id, UINT code)
{
    char name[THEME_NAME];
    int i = Item(dlg);

    switch (id) {
    case IDC_AP_SCHEME:
        if (code != CBN_SELCHANGE)
            return FALSE;
        SchemeName(dlg, name, sizeof name);
        LoadScheme(name);
        CanDelete(dlg);
        Repaint(dlg);
        return TRUE;
    case IDC_AP_ITEM:
        if (code == CBN_SELCHANGE)
            ShowItem(dlg);
        return FALSE;
    case IDC_AP_COLOR:
    case IDC_AP_COLOR2:
    case IDC_AP_TEXT:
        if (code != BN_CLICKED || !Choose(GetDlgItem(dlg, id), id == IDC_AP_COLOR ? items[i].color :
                                          id == IDC_AP_COLOR2 ? items[i].color2 : items[i].text))
            return FALSE;
        SendDlgItemMessage(dlg, IDC_AP_SCHEME, CB_SETCURSEL, (WPARAM)-1, 0L);   /* no scheme's now */
        CanDelete(dlg);
        Repaint(dlg);
        return TRUE;
    case IDC_AP_SAVE:
        SaveScheme(dlg);
        return FALSE;
    case IDC_AP_DELETE:
        return DeleteScheme(dlg);
    case IDC_AP_THEMES:
        ThemeManagerShow();
        return FALSE;
    }
    return FALSE;
}

BOOL AppearanceDrawItem(HWND dlg, const DRAWITEMSTRUCT FAR *d)
{
    int i = Item(dlg);

    if (d->CtlID == IDC_AP_COLOR)
        ColorPickerDraw(d, Color(items[i].color));
    else if (d->CtlID == IDC_AP_COLOR2)
        ColorPickerDraw(d, Color(items[i].color2 >= 0 ? items[i].color2 : COLOR_BTNFACE));
    else if (d->CtlID == IDC_AP_TEXT)
        ColorPickerDraw(d, Color(items[i].text >= 0 ? items[i].text : COLOR_BTNFACE));
    else
        return FALSE;
    return TRUE;
}

void AppearanceApply(HWND dlg)
{
    char name[THEME_NAME];

    SchemeName(dlg, name, sizeof name);
    ColorSchemeApply(&scheme, name);
}

/* In its sunken edge. */
void AppearancePaint(HWND dlg, HDC dc)
{
    RECT a;

    PreviewRect(dlg, &a);
    DrawBevel(dc, &a, TRUE);
    InflateRect(&a, -2, -2);
    ColorSchemePicture(dc, &a, &scheme, TRUE);
}
