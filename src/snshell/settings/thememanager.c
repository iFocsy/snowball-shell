/*
 * The Theme Manager, as Windows 98's Desktop Themes with more to edit: every
 * theme (themes.c) under its group on the left; on the right a picture of
 * the chosen one (its desktop, icons and windows) and a page to edit each
 * part it may set (themepages.c): colours, the desktop's wallpaper, screen
 * saver and icons, sounds, mouse pointers. Themes are made from the
 * settings in use (New), copied, renamed, put in a group, deleted, brought
 * in from a Windows 98 .Theme file or written out as one; Restore Defaults
 * brings Snowball's back as they came. OK and Apply put the parts ticked
 * below into use. Edits are kept at once.
 */
#include "internal.h"

#define PREVIEW_W   240                 /* the wallpaper read for a screen this wide */
#define HEADING     0xFFFFL             /* a list item that is a group's name */
#define ICONS_SHOWN 3                   /* in the picture: My Computer, My Documents, Recycle Bin */

static HWND tm;                         /* the window, while open */
static char chosen[THEME_NAME];         /* the theme chosen in the list */
static DIB wall;                        /* its wallpaper, for the picture */
static char wallShown[MAX_PATH];
static int  wallStyle = -1;
static HICON shown[ICONS_SHOWN];        /* its own desktop icons (NULL: Snowball's) */
static int listAt;                      /* the list's choice, to pass group names the way it moves */

/* "file,index" (index 0 if none): the file left in value. */
int ThemeManagerIconIndex(LPSTR value)
{
    LPSTR comma = _fstrrchr(value, ',');
    int n = 0;

    if (!comma)
        return 0;
    *comma++ = '\0';
    while (*comma >= '0' && *comma <= '9')
        n = n * 10 + *comma++ - '0';
    return n;
}

static void PictureRect(HWND dlg, RECT *r)
{
    GetWindowRect(GetDlgItem(dlg, IDC_TM_PICTURE), r);
    MapWindowPoints(NULL, dlg, (POINT FAR *)r, 2);
}

static void RepaintPicture(HWND dlg)
{
    RECT r;

    PictureRect(dlg, &r);
    InvalidateRect(dlg, &r, FALSE);
}

static BOOL NotPicture(HWND dlg, int id)
{
    return id != IDC_TM_PICTURE;
}

/* The theme's wallpaper and icons, read again for the picture. */
static void LoadPicture(void)
{
    char key[40], value[MAX_PATH + 8];
    int i, index, style;

    if (!ThemeGet(chosen, "Wallpaper", value, MAX_PATH)) {
        GetProfileString("Desktop", "Wallpaper", "", value, MAX_PATH);  /* (not set: as now) */
        if (!lstrcmpi(value, "(None)"))
            *value = '\0';
    }
    style = ThemeWallpaperStyle(chosen);
    if (lstrcmpi(value, wallShown) || style != wallStyle) {
        lstrcpy(wallShown, value);
        wallStyle = style;
        DibFree(&wall);
        if (*value)
            WallpaperPreview(value, style, PREVIEW_W, WallpaperYield, &wall);
    }
    for (i = 0; i < ICONS_SHOWN; i++) {
        if (shown[i])
            DestroyIcon(shown[i]);
        shown[i] = NULL;
        ThemeKey("Icon", ThemeIconKey(i), key);      /* (the first three: those the picture shows) */
        if (ThemeGet(chosen, key, value, sizeof value) && *value) {
            index = ThemeManagerIconIndex(value);
            DosShortPath(value, value);
            if ((UINT)(shown[i] = ExtractIcon(g_hInst, value, index)) <= 1)
                shown[i] = NULL;
        }
    }
}

/* The theme's desktop (colour, wallpaper, icons) with its windows. */
static void PaintPicture(HWND dlg, HDC dc)
{
    static const int stock[ICONS_SHOWN] = { IDI_COMPUTER, IDI_MYDOCS, IDI_BIN };
    static const LPCSTR labels[ICONS_SHOWN] = { "My Computer", "My Documents", "Recycle Bin" };
    COLORSCHEME cs;
    char value[COLOR_TEXT];
    RECT a, w, t;
    HFONT old;
    int i;

    PictureRect(dlg, &a);
    DrawBevel(dc, &a, TRUE);
    InflateRect(&a, -2, -2);
    if (!ThemeGet(chosen, "Colors", value, sizeof value) || !ColorSchemeParse(value, &cs))
        ColorSchemeCurrent(&cs);
    if (!ThemeGet(chosen, "TileWallpaper", value, sizeof value))
        wsprintf(value, "%d", GetProfileInt("Desktop", "TileWallpaper", 0));
    SaveDC(dc);
    IntersectClipRect(dc, a.left, a.top, a.right, a.bottom);
    DisplayPropertiesDesktop(dc, &a, cs.colors[COLOR_BACKGROUND], &wall, *value == '1');
    old = SelectObject(dc, DrawFont(FALSE));
    SetBkMode(dc, TRANSPARENT);
    for (i = 0; i < ICONS_SHOWN; i++) {
        DrawIconImage(dc, a.left + 18, a.top + 6 + i * 44, shown[i] ? shown[i] : IconStock(stock[i]), FALSE);
        SetRect(&t, a.left + 1, a.top + 40 + i * 44, a.left + 69, a.top + 52 + i * 44);
        SetTextColor(dc, RGB(0, 0, 0));             /* white with a shadow, as the desktop's */
        DrawText(dc, labels[i], -1, &t, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        OffsetRect(&t, -1, -1);
        SetTextColor(dc, RGB(255, 255, 255));
        DrawText(dc, labels[i], -1, &t, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    SelectObject(dc, old);
    SetRect(&w, a.left + 70, a.top, a.right, a.bottom);
    ColorSchemePicture(dc, &w, &cs, FALSE);
    RestoreDC(dc, -1);
}

/* What the theme sets, said under its picture; the parts to apply. */
static void ShowParts(HWND dlg)
{
    static const LPCSTR names[THEME_PARTS] = { "colors", "wallpaper", "screen saver", "sounds", "mouse pointers", "desktop icons" };
    char text[160];
    UINT parts = *chosen ? ThemeParts(chosen) : 0;
    int i, n = 0;

    lstrcpy(text, "This theme sets its ");
    for (i = 0; i < THEME_PARTS; i++) {
        EnableWindow(GetDlgItem(dlg, IDC_TM_PART + i), (parts >> i) & 1);
        CheckDlgButton(dlg, IDC_TM_PART + i, (parts >> i) & 1);
        if ((parts >> i) & 1) {
            if (n)
                lstrcat(text, ", ");
            lstrcat(text, names[i]);
            n++;
        }
    }
    lstrcat(text, ".");
    SetDlgItemText(dlg, IDC_TM_SUMMARY, n ? text : "This theme sets nothing yet: on a page, tick the box to add that part.");
}

static void Buttons(HWND dlg)
{
    static const int ids[] = { IDC_TM_COPY, IDC_TM_RENAME, IDC_TM_DELETE, IDC_TM_GROUP, IDC_TM_EXPORT, IDOK, IDC_APPLY };
    int i;

    for (i = 0; i < sizeof ids / sizeof ids[0]; i++)
        EnableWindow(GetDlgItem(dlg, ids[i]), *chosen != '\0');
}

/* The list's choice, a theme (a group's name passes it on). */
static void Choose(HWND dlg, BOOL down)
{
    HWND l = GetDlgItem(dlg, IDC_TM_LIST);
    int i = (int)SendMessage(l, LB_GETCURSEL, 0, 0L), n = (int)SendMessage(l, LB_GETCOUNT, 0, 0L);

    while (i >= 0 && i < n && SendMessage(l, LB_GETITEMDATA, i, 0L) == HEADING)
        i += down ? 1 : -1;
    if (i < 0 || i >= n)                /* (a group's name at an end: the other way) */
        for (i = 0; i < n && SendMessage(l, LB_GETITEMDATA, i, 0L) == HEADING; i++)
            ;
    *chosen = '\0';
    if (i < n) {
        SendMessage(l, LB_SETCURSEL, i, 0L);
        SendMessage(l, LB_GETTEXT, i, (LPARAM)(LPSTR)chosen);
    }
    listAt = i;
    LoadPicture();
    ThemePagesLoad(dlg, chosen);
    ShowParts(dlg);
    Buttons(dlg);
    RepaintPicture(dlg);
}

/* Each group's name, then its themes; select chosen (else the first). */
static void Fill(HWND dlg, LPCSTR select)
{
    HWND l = GetDlgItem(dlg, IDC_TM_LIST);
    char name[THEME_NAME], group[THEME_GROUP], last[THEME_GROUP];
    PATHLIST names, groups;
    int i, at, sel = -1;

    SendMessage(l, WM_SETREDRAW, FALSE, 0L);
    SendMessage(l, LB_RESETCONTENT, 0, 0L);
    PathListInit(&names);
    PathListInit(&groups);
    ThemesList(&names, &groups);
    *last = '\0';
    for (i = 0; PathListAt(&names, i, name) && PathListAt(&groups, i, group); i++) {
        if (lstrcmpi(group, last)) {
            at = (int)SendMessage(l, LB_ADDSTRING, 0, (LPARAM)(LPSTR)group);
            SendMessage(l, LB_SETITEMDATA, at, HEADING);
            lstrcpy(last, group);
        }
        at = (int)SendMessage(l, LB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
        if (!lstrcmpi(name, select))
            sel = at;
    }
    PathListFree(&names);
    PathListFree(&groups);
    SendMessage(l, LB_SETCURSEL, max(sel, 0), 0L);
    SendMessage(l, WM_SETREDRAW, TRUE, 0L);
    InvalidateRect(l, NULL, TRUE);
    Choose(dlg, TRUE);
}

static void DrawListItem(const DRAWITEMSTRUCT FAR *d)
{
    char text[THEME_NAME];
    RECT r = d->rcItem;
    HFONT old;

    if (d->itemID == (UINT)-1)
        return;
    SendMessage(d->hwndItem, LB_GETTEXT, d->itemID, (LPARAM)(LPSTR)text);
    if (d->itemData != HEADING) {
        PaintListItem(d, r.left + 12, NULL, text, FALSE);
        return;
    }
    DrawSolidRect(d->hDC, &r, GetSysColor(COLOR_WINDOW));     /* a group: its name in bold, not chosen */
    old = SelectObject(d->hDC, DrawFont(TRUE));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, GetSysColor(COLOR_WINDOWTEXT));
    r.left += 2;
    DrawText(d->hDC, text, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(d->hDC, old);
}

/* A name asked for (shown first in name); FALSE: none, or one in use. */
static BOOL AskName(HWND dlg, LPCSTR title, LPCSTR label, LPSTR name)
{
    char text[THEME_NAME + 48];

    if (!PromptDialogShow(dlg, title, label, name, THEME_NAME) || !*name)
        return FALSE;
    if (_fstrchr(name, '[') || _fstrchr(name, ']') || _fstrchr(name, '=')) {
        MessageBox(dlg, "A theme's name cannot have [, ] or =.", title, MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }
    if (!ThemeExists(name))
        return TRUE;
    wsprintf(text, "There is already a theme named '%s'.", (LPSTR)name);
    MessageBox(dlg, text, title, MB_ICONEXCLAMATION | MB_OK);
    return FALSE;
}

static void Apply(HWND dlg)
{
    UINT parts = 0;
    int i;

    for (i = 0; i < THEME_PARTS; i++)
        if (IsDlgButtonChecked(dlg, IDC_TM_PART + i))
            parts |= 1 << i;
    if (*chosen && parts) {
        ThemeApply(chosen, parts);
        SettingsSetString("Theme Manager", "Current", chosen);
    }
}

/* The list's buttons. */
static void Manage(HWND dlg, UINT id)
{
    static const char filter[] = "Themes (*.Theme)\0*.THEME;*.THE\0All Files (*.*)\0*.*\0";
    char name[THEME_NAME], group[THEME_GROUP], file[MAX_PATH], text[THEME_NAME + 64];

    switch (id) {
    case IDC_TM_NEW:
        lstrcpy(name, "My Theme");
        if (AskName(dlg, "New Theme", "&Save the settings in use now as the theme:", name) && ThemeAdd(name, "")) {
            ThemeFromCurrent(name);
            Fill(dlg, name);
        }
        break;
    case IDC_TM_COPY:
        wsprintf(name, "Copy of %s", (LPSTR)chosen);
        name[THEME_NAME - 1] = '\0';
        if (AskName(dlg, "Copy Theme", "&Name of the copy:", name) && ThemeCopy(chosen, name))
            Fill(dlg, name);
        break;
    case IDC_TM_RENAME:
        lstrcpy(name, chosen);
        if (AskName(dlg, "Rename Theme", "&New name:", name) && ThemeRename(chosen, name))
            Fill(dlg, name);
        break;
    case IDC_TM_DELETE:
        wsprintf(text, "Are you sure you want to delete the theme '%s'?", (LPSTR)chosen);
        if (MessageBox(dlg, text, "Delete Theme", MB_ICONQUESTION | MB_YESNO) == IDYES) {
            ThemeDelete(chosen);
            Fill(dlg, "");
        }
        break;
    case IDC_TM_GROUP:
        ThemeGroup(chosen, group);
        if (PromptDialogShow(dlg, "Group", "&Put the theme in the group:", group, sizeof group)) {
            ThemeSetGroup(chosen, group);
            Fill(dlg, chosen);
        }
        break;
    case IDC_TM_IMPORT:
        *file = '\0';
        if (BrowseDialogShow(dlg, "Import Theme", filter, file, TRUE)) {
            if (ThemeImport(file, name)) {
                Fill(dlg, name);
            } else {
                wsprintf(text, "There is already a theme named '%s'.", (LPSTR)name);
                MessageBox(dlg, text, "Import Theme", MB_ICONEXCLAMATION | MB_OK);
            }
        }
        break;
    case IDC_TM_EXPORT:
        wsprintf(file, "%s.Theme", (LPSTR)chosen);
        if (BrowseDialogSave(dlg, "Export Theme", filter, "Theme", file) && !ThemeExport(chosen, file))
            ErrorBox(dlg, "The theme cannot be written there.");
        break;
    case IDC_TM_DEFAULTS:
        if (MessageBox(dlg, "Snowball's own themes will be as they came. Your themes are kept.\n\nDo you want to continue?",
                       "Restore Defaults", MB_ICONQUESTION | MB_YESNO) == IDYES) {
            ThemesRestoreDefaults();
            Fill(dlg, chosen);
        }
        break;
    }
}

static void Command(HWND dlg, WPARAM wp, LPARAM lp)
{
    UINT code = HIWORD(lp);

    if (wp == IDC_TABS && code == TCN_SELECT) {
        TabsShowPage(dlg, TabsSelected(GetDlgItem(dlg, IDC_TABS)), NotPicture);
        ThemePagesLoad(dlg, chosen);    /* (its controls usable as the theme has them) */
    } else if (wp == IDC_TM_LIST) {
        if (code == LBN_SELCHANGE)
            Choose(dlg, (int)SendMessage((HWND)LOWORD(lp), LB_GETCURSEL, 0, 0L) >= listAt);
    } else if (wp >= IDC_TM_NEW && wp <= IDC_TM_DEFAULTS) {
        Manage(dlg, wp);
    } else if (*chosen && ThemePagesCommand(dlg, chosen, wp, code)) {
        LoadPicture();                  /* the theme changed */
        ShowParts(dlg);
        RepaintPicture(dlg);
    } else {
        SheetButton(dlg, wp, Apply);
    }
}

BOOL CALLBACK __export ThemeManagerProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    char now[THEME_NAME];
    int i;

    switch (msg) {
    case WM_INITDIALOG:
        tm = dlg;
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Preview");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Colors");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Desktop");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Sounds");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Pointers");
        ThemesPrepare();
        ThemePagesInit(dlg);
        SettingsGetString("Theme Manager", "Current", "", now, sizeof now);
        if (!*now)
            GetPrivateProfileString("current", "color schemes", "", now, sizeof now, "CONTROL.INI");
        Fill(dlg, now);
        SkinDialog(dlg);
        WindowCenter(dlg);
        TabsShowPage(dlg, 0, NotPicture);
        return TRUE;
    case WM_ACTIVATE:                   /* modeless: the main loop hands it its keys */
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        break;
    case WM_PAINT:
        BeginPaint(dlg, &ps);
        if (TabsSelected(GetDlgItem(dlg, IDC_TABS)) == 0)
            PaintPicture(dlg, ps.hdc);
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_MEASUREITEM:
        if (!ThemePagesMeasure((MEASUREITEMSTRUCT FAR *)lp))
            ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = 16;
        return TRUE;
    case WM_DRAWITEM:
        if (((const DRAWITEMSTRUCT FAR *)lp)->CtlID == IDC_TM_LIST)
            DrawListItem((const DRAWITEMSTRUCT FAR *)lp);
        else
            ThemePagesDraw((const DRAWITEMSTRUCT FAR *)lp, chosen);
        return TRUE;
    case WM_COMMAND:
        Command(dlg, wp, lp);
        return TRUE;
    case WM_DESTROY:
        MessageLoopTrackDialog(dlg, FALSE);
        ThemePagesDone();
        DibFree(&wall);
        *wallShown = '\0';
        for (i = 0; i < ICONS_SHOWN; i++) {
            if (shown[i])
                DestroyIcon(shown[i]);
            shown[i] = NULL;
        }
        tm = NULL;
        return TRUE;
    }
    return FALSE;
}

void ThemeManagerShow(void)
{
    if (tm)
        SetActiveWindow(tm);
    else if (CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_THEMES), NULL, ThemeManagerProc) && tm)
        ShowWindow(tm, SW_SHOWNORMAL);
}
