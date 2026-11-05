/*
 * The Theme Manager's pages (thememanager.c), each editing one part of the
 * theme chosen, kept at once. A page's box adds its part to the theme (as
 * the settings in use have it) or takes it out. Colors: every colour of
 * the scheme by name, changed with Windows 98's palette. Desktop: the
 * wallpaper (a picture and how it shows), the screen saver, and the
 * desktop's icons. Sounds: a .WAV for each event, played to hear it.
 * Pointers: a .CUR or .ANI for each, shown as Windows 3.1 will draw it.
 */
#include "internal.h"
#include <mmsystem.h>

#define ROW_COLOR   16
#define ROW_ICON    20
#define ROW_SOUND   16
#define ROW_POINTER 36
#define NAME_W      112                 /* a sound's or pointer's name column */

static const int boxes[THEME_PARTS] = {         /* each part's box, by THEME_* bit */
    IDC_TM_HASCOLORS, IDC_TM_HASWALL, IDC_TM_HASSAVER, IDC_TM_HASSOUNDS, IDC_TM_HASPOINTERS, IDC_TM_HASICONS,
};
static const int stock[THEME_ICONS_N] = { IDI_COMPUTER, IDI_MYDOCS, IDI_BIN, IDI_BINFULL };
static LPCSTR const own[CURSORS] = {    /* Windows' pointers, by CURSOR_* */
    IDC_ARROW, IDC_IBEAM, IDC_WAIT, IDC_CROSS, IDC_UPARROW, IDC_SIZE, IDC_SIZENWSE, IDC_SIZENESW, IDC_SIZEWE, IDC_SIZENS,
};
static COLORSCHEME scheme;              /* the theme's colours */
static PATHLIST savers;                 /* the saver list's paths, by item data */
static HICON icons[THEME_ICONS_N];      /* the theme's own icons (NULL: Snowball's) */
static HCURSOR pointers[CURSORS];       /* its pointers, as they will look (NULL: Windows' own) */

static void Rows(HWND list, int n)
{
    int i;

    SendMessage(list, LB_RESETCONTENT, 0, 0L);
    for (i = 0; i < n; i++)
        SendMessage(list, LB_ADDSTRING, 0, (LPARAM)i);
    SendMessage(list, LB_SETCURSEL, 0, 0L);
}

void ThemePagesInit(HWND dlg)
{
    Rows(GetDlgItem(dlg, IDC_TM_COLORLIST), COLOR_KEYS);
    Rows(GetDlgItem(dlg, IDC_TM_ICONLIST), THEME_ICONS_N);
    Rows(GetDlgItem(dlg, IDC_TM_SOUNDLIST), THEME_SOUNDS_N);
    Rows(GetDlgItem(dlg, IDC_TM_POINTERLIST), CURSORS);
    WallpaperStyles(GetDlgItem(dlg, IDC_TM_WALLSTYLE));
    DisplayPropertiesSavers(GetDlgItem(dlg, IDC_TM_SAVER), &savers);
}

static void Forget(void)
{
    int i;

    for (i = 0; i < THEME_ICONS_N; i++) {
        if (icons[i])
            DestroyIcon(icons[i]);
        icons[i] = NULL;
    }
    for (i = 0; i < CURSORS; i++) {
        if (pointers[i])
            DestroyCursor(pointers[i]);
        pointers[i] = NULL;
    }
}

void ThemePagesDone(void)
{
    Forget();
    PathListFree(&savers);
}

/* A part's key's value, "" if not set. */
static void Value(LPCSTR name, LPCSTR kind, LPCSTR key, LPSTR out, int size)
{
    char k[40];

    ThemeKey(kind, key, k);
    ThemeGet(name, k, out, size);
}

static void SetValue(LPCSTR name, LPCSTR kind, LPCSTR key, LPCSTR value)
{
    char k[40];

    ThemeKey(kind, key, k);
    ThemeSet(name, k, value);
}

/* The theme's icons and pointers, made for the lists. */
static void LoadImages(LPCSTR name)
{
    char value[MAX_PATH + 8];
    int i, index;

    Forget();
    for (i = 0; i < THEME_ICONS_N; i++) {
        Value(name, "Icon", ThemeIconKey(i), value, sizeof value);
        if (*value) {
            index = ThemeManagerIconIndex(value);
            DosShortPath(value, value);
            if ((UINT)(icons[i] = ExtractIcon(g_hInst, value, index)) <= 1)
                icons[i] = NULL;
        }
    }
    for (i = 0; i < CURSORS; i++) {
        Value(name, "Pointer", ThemePointerKey(i), value, sizeof value);
        if (*value) {
            DosShortPath(value, value);
            pointers[i] = CursorsLoad(value);
        }
    }
}

/* Each page's controls usable while its part is in the theme. */
static void Enable(HWND dlg, UINT parts)
{
    static const struct { int id; UINT part; } controls[] = {
        { IDC_TM_COLORLIST, THEME_COLORS }, { IDC_TM_CHANGE, THEME_COLORS }, { IDC_TM_COLORHINT, THEME_COLORS },
        { IDC_TM_WALLNAME, THEME_WALLPAPER }, { IDC_TM_WALLBROWSE, THEME_WALLPAPER },
        { IDC_TM_WALLNONE, THEME_WALLPAPER }, { IDC_TM_WALLSTYLE, THEME_WALLPAPER },
        { IDC_TM_SAVER, THEME_SAVER }, { IDC_TM_ICONLIST, THEME_ICONS }, { IDC_TM_ICONCHANGE, THEME_ICONS },
        { IDC_TM_ICONDEFAULT, THEME_ICONS }, { IDC_TM_SOUNDLIST, THEME_SOUNDS }, { IDC_TM_SOUNDBROWSE, THEME_SOUNDS },
        { IDC_TM_SOUNDPLAY, THEME_SOUNDS }, { IDC_TM_SOUNDNONE, THEME_SOUNDS }, { IDC_TM_POINTERLIST, THEME_POINTERS },
        { IDC_TM_POINTERBROWSE, THEME_POINTERS }, { IDC_TM_POINTERDEFAULT, THEME_POINTERS },
    };
    HWND c;
    int i;

    for (i = 0; i < sizeof controls / sizeof controls[0]; i++) {
        c = GetDlgItem(dlg, controls[i].id);
        if (IsWindowVisible(c))         /* (a hidden page's: TabsShowPage enables them when shown) */
            EnableWindow(c, (parts & controls[i].part) != 0);
    }
}

/* The wallpaper's file name, the screen saver chosen. */
static void ShowDesktop(HWND dlg, LPCSTR name)
{
    char value[MAX_PATH], item[MAX_PATH];
    HWND cb = GetDlgItem(dlg, IDC_TM_SAVER);
    int i, n, sel = 0;
    DWORD data;

    ThemeGet(name, "Wallpaper", value, sizeof value);
    SetDlgItemText(dlg, IDC_TM_WALLNAME, *value ? value : "(None)");
    SendDlgItemMessage(dlg, IDC_TM_WALLSTYLE, CB_SETCURSEL, ThemeWallpaperStyle(name), 0L);
    ThemeGet(name, "ScreenSaver", value, sizeof value);
    n = (int)SendMessage(cb, CB_GETCOUNT, 0, 0L);
    for (i = 0; i < n && *value; i++) {
        data = SendMessage(cb, CB_GETITEMDATA, i, 0L);
        if (data != LIST_NONE && PathListAt(&savers, (int)data, item) &&
            !lstrcmpi(PathFileName(item), PathFileName(value)))
            sel = i;
    }
    SendMessage(cb, CB_SETCURSEL, sel, 0L);
}

void ThemePagesLoad(HWND dlg, LPCSTR name)
{
    char value[COLOR_TEXT];
    UINT parts = *name ? ThemeParts(name) : 0;
    int i;

    for (i = 0; i < THEME_PARTS; i++) {
        CheckDlgButton(dlg, boxes[i], (parts >> i) & 1);
        if (IsWindowVisible(GetDlgItem(dlg, boxes[i])))
            EnableWindow(GetDlgItem(dlg, boxes[i]), *name != '\0');
    }
    if (!ThemeGet(name, "Colors", value, sizeof value) || !ColorSchemeParse(value, &scheme))
        ColorSchemeCurrent(&scheme);
    LoadImages(name);
    ShowDesktop(dlg, name);
    Enable(dlg, parts);
    InvalidateRect(GetDlgItem(dlg, IDC_TM_COLORLIST), NULL, FALSE);
    InvalidateRect(GetDlgItem(dlg, IDC_TM_ICONLIST), NULL, FALSE);
    InvalidateRect(GetDlgItem(dlg, IDC_TM_SOUNDLIST), NULL, FALSE);
    InvalidateRect(GetDlgItem(dlg, IDC_TM_POINTERLIST), NULL, FALSE);
}

/* A part ticked: in the theme as the settings in use have it (pointers
   and icons: Windows' and Snowball's own); cleared: out of it. */
static void Part(LPCSTR name, UINT part, BOOL on)
{
    char value[COLOR_TEXT + MAX_PATH];
    COLORSCHEME cs;
    int i;

    if (!on) {
        ThemeDrop(name, part);
        return;
    }
    switch (part) {
    case THEME_COLORS:
        ColorSchemeCurrent(&cs);
        ColorSchemeFormat(&cs, value);
        ThemeSet(name, "Colors", value);
        break;
    case THEME_WALLPAPER:
        ThemeSetWallpaperStyle(name, WallpaperNow(value));
        ThemeSet(name, "Wallpaper", value);
        break;
    case THEME_SAVER:
        GetPrivateProfileString("boot", "SCRNSAVE.EXE", "", value, MAX_PATH, "SYSTEM.INI");
        ThemeSet(name, "ScreenSaver", GetProfileInt("windows", "ScreenSaveActive", 0) ? value : "");
        break;
    case THEME_SOUNDS:
        for (i = 0; i < THEME_SOUNDS_N; i++) {
            GetProfileString("sounds", ThemeSoundKey(i), "", value, MAX_PATH);
            if (_fstrchr(value, ','))
                *_fstrchr(value, ',') = '\0';
            SetValue(name, "Sound", ThemeSoundKey(i), lstrcmpi(value, "none") ? value : "");
        }
        break;
    case THEME_POINTERS:
        for (i = 0; i < CURSORS; i++)
            SetValue(name, "Pointer", ThemePointerKey(i), "");
        break;
    case THEME_ICONS:
        for (i = 0; i < THEME_ICONS_N; i++)
            SetValue(name, "Icon", ThemeIconKey(i), "");
        break;
    }
}

static int Selected(HWND dlg, int id)
{
    return max(0, (int)SendDlgItemMessage(dlg, id, LB_GETCURSEL, 0, 0L));
}

static BOOL ChangeColor(HWND dlg, LPCSTR name)
{
    char value[COLOR_TEXT];
    int k = Selected(dlg, IDC_TM_COLORLIST);
    COLORREF c = ColorSchemeColor(&scheme, k);

    if (!ColorPickerChoose(GetDlgItem(dlg, IDC_TM_CHANGE), &c))
        return FALSE;
    ColorSchemeSetColor(&scheme, k, c);
    ColorSchemeFormat(&scheme, value);
    ThemeSet(name, "Colors", value);
    return TRUE;
}

/* A file for a part's key, chosen with Browse; FALSE: none. */
static BOOL Pick(HWND dlg, LPCSTR title, LPCSTR filter, LPSTR file)
{
    *file = '\0';
    return BrowseDialogShow(dlg, title, filter, file, TRUE);
}

static BOOL ChangeIcon(HWND dlg, LPCSTR name)
{
    char file[MAX_PATH + 8];
    int i = Selected(dlg, IDC_TM_ICONLIST), index;

    Value(name, "Icon", ThemeIconKey(i), file, sizeof file);
    if (*file) {
        index = ThemeManagerIconIndex(file);
    } else {                            /* Snowball's own: numbered in id order from 0 */
        IconsArtFile(file);
        index = stock[i] - 1;
    }
    if (!ChangeIconDialogShow(dlg, file, &index))
        return FALSE;
    wsprintf(file + lstrlen(file), ",%d", index);
    SetValue(name, "Icon", ThemeIconKey(i), file);
    return TRUE;
}

static BOOL ChangePointer(HWND dlg, LPCSTR name)
{
    char file[MAX_PATH], path[MAX_PATH];
    HCURSOR c;

    if (!Pick(dlg, "Browse", "Cursors (*.cur, *.ani)\0*.CUR;*.ANI\0", file))
        return FALSE;
    DosShortPath(file, path);
    if ((c = CursorsLoad(path)) == NULL) {
        MessageBox(dlg, "This file has no mouse pointer that Windows 3.1 can show.", "Pointers", MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }
    DestroyCursor(c);
    SetValue(name, "Pointer", ThemePointerKey(Selected(dlg, IDC_TM_POINTERLIST)), file);
    return TRUE;
}

/* TRUE: the theme changed. */
BOOL ThemePagesCommand(HWND dlg, LPCSTR name, UINT id, UINT code)
{
    char file[MAX_PATH];
    DWORD data;
    int i;

    for (i = 0; i < THEME_PARTS; i++)
        if (id == boxes[i]) {
            Part(name, 1 << i, IsDlgButtonChecked(dlg, id));
            ThemePagesLoad(dlg, name);
            return TRUE;
        }
    switch (id) {
    case IDC_TM_COLORLIST:
        if (code != LBN_DBLCLK)
            return FALSE;
        /* (a double click: Change) */
    case IDC_TM_CHANGE:
        if (!ChangeColor(dlg, name))
            return FALSE;
        InvalidateRect(GetDlgItem(dlg, IDC_TM_COLORLIST), NULL, FALSE);
        return TRUE;
    case IDC_TM_WALLBROWSE:
        if (!Pick(dlg, "Browse", "Pictures (*.bmp;*.jpg;*.jpeg;*.png)\0*.BMP;*.RLE;*.JPG;*.JPEG;*.PNG\0", file))
            return FALSE;
        ThemeSet(name, "Wallpaper", file);
        break;
    case IDC_TM_WALLNONE:
        ThemeSet(name, "Wallpaper", "");
        break;
    case IDC_TM_WALLSTYLE:
        if (code != CBN_SELCHANGE)
            return FALSE;
        ThemeSetWallpaperStyle(name, (int)SendDlgItemMessage(dlg, id, CB_GETCURSEL, 0, 0L));
        break;
    case IDC_TM_SAVER:
        if (code != CBN_SELCHANGE)
            return FALSE;
        data = SendDlgItemMessage(dlg, id, CB_GETITEMDATA, (int)SendDlgItemMessage(dlg, id, CB_GETCURSEL, 0, 0L), 0L);
        if (data == LIST_NONE || !PathListAt(&savers, (int)data, file))
            *file = '\0';
        ThemeSet(name, "ScreenSaver", file);
        break;
    case IDC_TM_ICONLIST:
        if (code != LBN_DBLCLK)
            return FALSE;
    case IDC_TM_ICONCHANGE:
        if (!ChangeIcon(dlg, name))
            return FALSE;
        break;
    case IDC_TM_ICONDEFAULT:
        SetValue(name, "Icon", ThemeIconKey(Selected(dlg, IDC_TM_ICONLIST)), "");
        break;
    case IDC_TM_SOUNDLIST:
        if (code != LBN_DBLCLK)
            return FALSE;
    case IDC_TM_SOUNDBROWSE:
        if (!Pick(dlg, "Browse", "Sounds (*.wav)\0*.WAV\0", file))
            return FALSE;
        SetValue(name, "Sound", ThemeSoundKey(Selected(dlg, IDC_TM_SOUNDLIST)), file);
        break;
    case IDC_TM_SOUNDPLAY:
        Value(name, "Sound", ThemeSoundKey(Selected(dlg, IDC_TM_SOUNDLIST)), file, sizeof file);
        if (*file) {
            DosShortPath(file, file);
            sndPlaySound(file, SND_ASYNC | SND_NODEFAULT);
        }
        return FALSE;
    case IDC_TM_SOUNDNONE:
        SetValue(name, "Sound", ThemeSoundKey(Selected(dlg, IDC_TM_SOUNDLIST)), "");
        break;
    case IDC_TM_POINTERLIST:
        if (code != LBN_DBLCLK)
            return FALSE;
    case IDC_TM_POINTERBROWSE:
        if (!ChangePointer(dlg, name))
            return FALSE;
        break;
    case IDC_TM_POINTERDEFAULT:
        SetValue(name, "Pointer", ThemePointerKey(Selected(dlg, IDC_TM_POINTERLIST)), "");
        break;
    default:
        return FALSE;
    }
    ThemePagesLoad(dlg, name);
    return TRUE;
}

BOOL ThemePagesMeasure(MEASUREITEMSTRUCT FAR *m)
{
    switch (m->CtlID) {
    case IDC_TM_COLORLIST:   m->itemHeight = ROW_COLOR; return TRUE;
    case IDC_TM_ICONLIST:    m->itemHeight = ROW_ICON; return TRUE;
    case IDC_TM_SOUNDLIST:   m->itemHeight = ROW_SOUND; return TRUE;
    case IDC_TM_POINTERLIST: m->itemHeight = ROW_POINTER; return TRUE;
    }
    return FALSE;
}

/* A row's background and text colours: the list's, lit when selected. */
static void RowColors(const DRAWITEMSTRUCT FAR *d, COLORREF *ink)
{
    BOOL sel = (d->itemState & ODS_SELECTED) && IsWindowEnabled(d->hwndItem);

    DrawSolidRect(d->hDC, &d->rcItem, GetSysColor(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    *ink = GetSysColor(!IsWindowEnabled(d->hwndItem) ? COLOR_GRAYTEXT : sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT);
}

static void Text(HDC dc, LPCSTR s, int left, int right, const RECT *row, COLORREF ink)
{
    RECT t;

    SetRect(&t, left, row->top, right, row->bottom);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ink);
    DrawTextEllipsis(dc, s, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* "(None)", "(Windows' own)" or the file's name. */
static void FileText(LPCSTR value, LPCSTR none, LPSTR out)
{
    lstrcpy(out, *value ? PathFileName((LPSTR)value) : none);
}

BOOL ThemePagesDraw(const DRAWITEMSTRUCT FAR *d, LPCSTR name)
{
    const RECT *r = &d->rcItem;
    char value[MAX_PATH + 8], text[MAX_PATH];
    int i = (int)d->itemID;
    HFONT old;
    COLORREF ink;
    RECT s;

    if (i < 0)
        return FALSE;
    RowColors(d, &ink);
    old = SelectObject(d->hDC, DrawFont(FALSE));
    switch (d->CtlID) {
    case IDC_TM_COLORLIST:              /* a swatch, the colour's name */
        SetRect(&s, r->left + 3, r->top + 2, r->left + 27, r->bottom - 2);
        DrawFrame(d->hDC, &s, RGB(0, 0, 0), RGB(0, 0, 0));
        InflateRect(&s, -1, -1);
        DrawSolidRect(d->hDC, &s, ColorSchemeColor(&scheme, i));
        lstrcpy(text, ColorSchemeName(i));
        if (!ColorSchemeChosen(&scheme, i))
            lstrcat(text, " (made from the others)");
        Text(d->hDC, text, r->left + 32, r->right - 2, r, ink);
        break;
    case IDC_TM_ICONLIST:               /* the icon, what it stands for */
        DrawIconImage(d->hDC, r->left + 3, r->top + 2, icons[i] ? icons[i] : IconStock(stock[i]), TRUE);
        Text(d->hDC, ThemeIconName(i), r->left + 24, r->right - 2, r, ink);
        break;
    case IDC_TM_SOUNDLIST:              /* the event, its sound */
        Text(d->hDC, ThemeSoundName(i), r->left + 3, r->left + NAME_W, r, ink);
        Value(name, "Sound", ThemeSoundKey(i), value, sizeof value);
        FileText(value, "(None)", text);
        Text(d->hDC, text, r->left + NAME_W + 4, r->right - 2, r, ink);
        break;
    case IDC_TM_POINTERLIST:            /* the pointer as it will look, its use, its file */
        DrawIcon(d->hDC, r->left + 2, r->top + 2, pointers[i] ? pointers[i] : LoadCursor(NULL, own[i]));
        Text(d->hDC, ThemePointerName(i), r->left + 40, r->left + 40 + NAME_W, r, ink);
        Value(name, "Pointer", ThemePointerKey(i), value, sizeof value);
        FileText(value, "(Windows' own)", text);
        Text(d->hDC, text, r->left + 44 + NAME_W, r->right - 2, r, ink);
        break;
    default:
        SelectObject(d->hDC, old);
        return FALSE;
    }
    SelectObject(d->hDC, old);
    return TRUE;
}
