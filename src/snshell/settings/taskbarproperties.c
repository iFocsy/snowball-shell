/*
 * Taskbar Properties, as in Windows 98: Taskbar Options (on top, auto hide,
 * small icons in the Start menu, the clock, with a picture of the result)
 * and Start Menu Programs, where the Start menu is edited: Add (the Create
 * Shortcut wizard), Remove (the Start menu's tree, to the Recycle Bin) and
 * Advanced (Explorer on the Start Menu folder); the Documents menu's Clear. The menu is built anew each
 * time it opens, so changes show at once. Tray (Calmira's): programs shown
 * as icons by the clock, and programs kept off the taskbar (SNOWBALL.INI
 * [Tray] and [Hidden], "1=path"...).
 */
#include "internal.h"

#define SECTION     "Taskbar"

static HWND sheet;
static int wizPage;

static void DlgRect(HWND dlg, int id, RECT *r)
{
    GetWindowRect(GetDlgItem(dlg, id), r);
    ScreenToClient(dlg, (POINT FAR *)&r->left);
    ScreenToClient(dlg, (POINT FAR *)&r->right);
}

/* ---- Taskbar Options: the picture ---- */

static void PreviewMenu(HDC dc, const RECT *d, int bottom, BOOL small)
{
    static const struct { LPCSTR text; int icon; } items[] = {
        { "Programs", IDI_SM_PROGRAMS }, { "Settings", IDI_SM_SETTINGS }, { "Find", IDI_SM_FIND },
        { "Run...", IDI_SM_RUN }, { NULL, 0 }, { "Shut Down...", IDI_SM_SHUTDOWN },
    };
    int rowH = small ? 16 : 22, n = sizeof items / sizeof items[0], i, y, left;
    RECT m, r;

    SetRect(&m, d->left + 2, bottom - 4, d->left + 120, bottom);
    for (i = 0; i < n; i++)
        m.top -= items[i].text ? rowH : 6;
    DrawSolidRect(dc, &m, GetSysColor(COLOR_MENU));
    DrawBevel(dc, &m, FALSE);
    left = m.left + 2;
    if (!small) {                       /* the banner */
        SetRect(&r, left, m.top + 2, left + 12, m.bottom - 2);
        DrawGradient(dc, &r, RGB(0, 0, 64), RGB(16, 132, 208), TRUE);
        left = r.right;
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_MENUTEXT));
    for (i = 0, y = m.top + 2; i < n; i++) {
        if (!items[i].text) {           /* a separator */
            SetRect(&r, left + 2, y + 2, m.right - 4, y + 3);
            DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNSHADOW));
            OffsetRect(&r, 0, 1);
            DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT));
            y += 6;
            continue;
        }
        DrawIconImage(dc, left + 3, y + (rowH - 16) / 2, IconStock(items[i].icon), TRUE);
        SetRect(&r, left + 22, y, m.right - 2, y + rowH);
        DrawText(dc, items[i].text, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        y += rowH;
    }
}

/* A small desktop with the Start menu open over the taskbar, as the
   options would make them. */
static void PaintPreview(HWND dlg, HDC dc)
{
    BOOL hide = IsDlgButtonChecked(dlg, IDC_TASKBAR_AUTOHIDE), clock = IsDlgButtonChecked(dlg, IDC_TASKBAR_CLOCK);
    HFONT old = SelectObject(dc, DrawFont(FALSE));
    RECT d, bar, b;

    DlgRect(dlg, IDC_TASKBAR_PREVIEW, &d);
    DrawBevel(dc, &d, TRUE);
    InflateRect(&d, -2, -2);
    DrawSolidRect(dc, &d, GetSysColor(COLOR_BACKGROUND));
    bar = d;
    bar.top = bar.bottom - (hide ? 2 : 24);
    DrawSolidRect(dc, &bar, GetSysColor(COLOR_BTNFACE));
    SetRect(&b, bar.left, bar.top + 1, bar.right, bar.top + 2);
    DrawSolidRect(dc, &b, GetSysColor(COLOR_BTNHIGHLIGHT));
    PreviewMenu(dc, &d, bar.top, IsDlgButtonChecked(dlg, IDC_TASKBAR_SMALL));
    if (!hide) {
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        SetRect(&b, bar.left + 2, bar.top + 4, bar.left + 56, bar.bottom - 2);
        DrawBevel(dc, &b, TRUE);            /* Start, pressed: its menu is open */
        PaintStripImage(dc, b.left + 4, b.top + (b.bottom - b.top - 16) / 2, IDB_STARTFLAG, 0);
        b.left += 22;
        SelectObject(dc, DrawFont(TRUE));
        DrawText(dc, "Start", -1, &b, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        SelectObject(dc, DrawFont(FALSE));
        SetRect(&b, bar.left + 60, bar.top + 4, bar.left + 170, bar.bottom - 2);
        DrawBevel(dc, &b, FALSE);
        b.left += 6;
        DrawText(dc, "Untitled - Notepad", -1, &b, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        if (clock) {
            SetRect(&b, bar.right - 62, bar.top + 4, bar.right - 2, bar.bottom - 2);
            DrawBevel(dc, &b, TRUE);
            DrawText(dc, "12:45 PM", -1, &b, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        }
    }
    SelectObject(dc, old);
}

/* ---- Start Menu Programs: Add, the Create Shortcut wizard ---- */

static BOOL Exists(LPCSTR prog)
{
    OFSTRUCT of;

    if (_fstrchr(prog, '\\') || _fstrchr(prog, ':'))
        return DosGetAttributes(prog) != 0xFFFF;
    return OpenFile(prog, &of, OF_EXIST) != HFILE_ERROR;
}

static void WizPage(HWND dlg, int page)
{
    static const char *titles[] = { "Create Shortcut", "Select Program Folder", "Select a Title for the Program" };
    static const int focus[] = { IDC_WIZARD_CMD, IDC_WIZARD_TREE, IDC_WIZARD_NAME };

    wizPage = page;
    TabsShowPage(dlg, page, NULL);
    SetWindowText(dlg, titles[page]);
    EnableWindow(GetDlgItem(dlg, IDC_WIZARD_BACK), page > 0);
    SetDlgItemText(dlg, IDC_WIZARD_NEXT, page == 2 ? "Finish" : "&Next >");
    SetFocus(GetDlgItem(dlg, focus[page]));
    if (page != 1)
        SendDlgItemMessage(dlg, focus[page], EM_SETSEL, 0, MAKELPARAM(0, -1));
}

/* Next, or Finish on the last page; FALSE: stay (something to correct). */
static BOOL WizNext(HWND dlg)
{
    char cmd[MAX_PATH], prog[MAX_PATH], args[128], dir[MAX_PATH], name[80];
    HWND tree = GetDlgItem(dlg, IDC_WIZARD_TREE);

    GetDlgItemText(dlg, IDC_WIZARD_CMD, cmd, sizeof cmd);
    LaunchSplitCommand(cmd, prog, args, sizeof args);
    switch (wizPage) {
    case 0:
        if (!*prog || !Exists(prog)) {
            MessageBox(dlg, "The command line is not valid. Make sure the path and file name are correct.",
                       "Create Shortcut", MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        lstrcpy(name, PathFileName(prog));  /* the program's name, as a title */
        *PathExtension(name) = '\0';
        AnsiUpperBuff(name, 1);
        SetDlgItemText(dlg, IDC_WIZARD_NAME, name);
        return TRUE;
    case 1:
        return TreeViewSelectedPath(tree, dir);
    default:
        GetDlgItemText(dlg, IDC_WIZARD_NAME, name, sizeof name);
        if (!*name || !TreeViewSelectedPath(tree, dir))
            return FALSE;
        if (!ProgramManagerAddItem(dir, name, cmd)) {
            MessageBox(dlg, "The shortcut could not be created.", "Create Shortcut", MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        EndDialog(dlg, IDOK);
        return FALSE;
    }
}

static void Browse(HWND dlg)
{
    char file[MAX_PATH] = "", quoted[MAX_PATH + 2];

    if (!BrowseDialogShow(dlg, "Browse", "Programs\0*.exe;*.com;*.bat;*.pif\0All Files\0*.*\0", file, TRUE))
        return;
    if (_fstrchr(file, ' ')) {          /* a long name with spaces: quoted, as a command */
        wsprintf(quoted, "\"%s\"", (LPSTR)file);
        SetDlgItemText(dlg, IDC_WIZARD_CMD, quoted);
    } else {
        SetDlgItemText(dlg, IDC_WIZARD_CMD, file);
    }
}

static void NewFolder(HWND dlg)
{
    HWND tree = GetDlgItem(dlg, IDC_WIZARD_TREE);
    char dir[MAX_PATH], made[MAX_PATH];

    if (TreeViewSelectedPath(tree, dir) && PathMakeUnique(made, dir, "New Folder", "") && DosMakeFolder(made)) {
        TreeViewRefresh(tree, dir);
        TreeViewShowPath(tree, made);
    }
}

/* Windows 98's picture: a program with the shortcut arrow, on the desktop's colour. */
static void PaintPicture(HWND dlg, HDC dc)
{
    RECT r, l;
    int x, y;

    DlgRect(dlg, IDC_WIZARD_PICTURE, &r);
    DrawBevel(dc, &r, TRUE);
    InflateRect(&r, -2, -2);
    DrawSolidRect(dc, &r, GetSysColor(COLOR_BACKGROUND));
    x = (r.left + r.right) / 2 - 16;
    y = (r.top + r.bottom) / 2 - 16;
    DrawIconImage(dc, x, y, IconStock(IDI_PROGRAM), FALSE);
    DrawIconImage(dc, x, y, IconStock(IDI_ARROW), FALSE);
    DlgRect(dlg, IDC_WIZARD_BACK, &l);       /* the etched line above the buttons */
    GetClientRect(dlg, &r);
    SetRect(&l, 7, l.top - 9, r.right - 7, l.top - 8);
    DrawSolidRect(dc, &l, GetSysColor(COLOR_BTNSHADOW));
    OffsetRect(&l, 0, 1);
    DrawSolidRect(dc, &l, GetSysColor(COLOR_BTNHIGHLIGHT));
}

BOOL CALLBACK __export TaskbarPropertiesWizardProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char dir[MAX_PATH];
    PAINTSTRUCT ps;

    switch (msg) {
    case WM_INITDIALOG:
        StartMenuFolder(dir);
        TreeViewResetFolder(GetDlgItem(dlg, IDC_WIZARD_TREE), dir, "Start Menu", FALSE);
        PathAppend(dir, "Programs");
        TreeViewShowPath(GetDlgItem(dlg, IDC_WIZARD_TREE), dir);
        ShowWindow(GetDlgItem(dlg, IDC_WIZARD_PICTURE), SW_HIDE);
        SkinDialog(dlg);
        WindowCenter(dlg);
        WizPage(dlg, 0);
        return FALSE;
    case WM_PAINT:
        BeginPaint(dlg, &ps);
        PaintPicture(dlg, ps.hdc);
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDC_WIZARD_BROWSE:
            Browse(dlg);
            break;
        case IDC_WIZARD_NEWFOLDER:
            NewFolder(dlg);
            break;
        case IDC_WIZARD_BACK:
            if (wizPage > 0)
                WizPage(dlg, wizPage - 1);
            break;
        case IDOK:
        case IDC_WIZARD_NEXT:
            if (WizNext(dlg) && wizPage < 2)
                WizPage(dlg, wizPage + 1);
            break;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

/* ---- Start Menu Programs: Remove ---- */

static void RemoveSelected(HWND dlg)
{
    HWND tree = GetDlgItem(dlg, IDC_REMOVE_TREE);
    char path[MAX_PATH], dir[MAX_PATH];
    PATHLIST l;

    StartMenuFolder(dir);
    if (!TreeViewSelectedPath(tree, path) || !lstrcmpi(path, dir))
        return;
    PathListInit(&l);
    PathListAdd(&l, path);
    FileCommandsDelete(dlg, &l, FALSE);
    PathListFree(&l);
    PathToParent(path);
    TreeViewRefresh(tree, path);
}

BOOL CALLBACK __export TaskbarPropertiesRemoveProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char dir[MAX_PATH];

    switch (msg) {
    case WM_INITDIALOG:
        StartMenuFolder(dir);
        TreeViewResetFolder(GetDlgItem(dlg, IDC_REMOVE_TREE), dir, "Start Menu", TRUE);
        SkinDialog(dlg);
        WindowCenter(dlg);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDC_REMOVE_REMOVE)
            RemoveSelected(dlg);
        else if (wp == IDCANCEL || wp == IDOK)
            EndDialog(dlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

/* ---- the sheet ---- */

static BOOL NotPicture(HWND dlg, int id)
{
    return id != IDC_TASKBAR_PREVIEW;
}

/* The 24-hour time is Windows' own setting (Control Panel's International
   changes it too); programs are told, as Control Panel tells them. */
static void Apply24Hour(BOOL on)
{
    static const char intl[] = "intl";

    if (on == (GetProfileInt(intl, "iTime", 0) != 0))
        return;
    WriteProfileString(intl, "iTime", on ? "1" : "0");
    PostMessage(HWND_BROADCAST, WM_WININICHANGE, 0, (LPARAM)(LPCSTR)intl);
}

/* ---- Tray ---- */

#define ROW_H   18
#define MAX_TRAY    6
#define MAX_HIDDEN  16

static void LoadList(HWND dlg, int id, LPCSTR section, int max)
{
    char key[4], path[MAX_PATH];
    int i;

    for (i = 1; i <= max; i++) {
        wsprintf(key, "%d", i);
        SettingsGetString(section, key, "", path, sizeof path);
        if (path[0])
            SendDlgItemMessage(dlg, id, LB_ADDSTRING, 0, (LPARAM)(LPSTR)path);
    }
}

static void SaveList(HWND dlg, int id, LPCSTR section)
{
    char key[4], path[MAX_PATH];
    int i, n = (int)SendDlgItemMessage(dlg, id, LB_GETCOUNT, 0, 0L);

    WritePrivateProfileString(section, NULL, NULL, g_ini);
    for (i = 0; i < n; i++) {
        SendDlgItemMessage(dlg, id, LB_GETTEXT, i, (LPARAM)(LPSTR)path);
        wsprintf(key, "%d", i + 1);
        WritePrivateProfileString(section, key, path, g_ini);
    }
}

static void Buttons(HWND dlg)
{
    EnableWindow(GetDlgItem(dlg, IDC_TRAY_ADD),
                 SendDlgItemMessage(dlg, IDC_TRAY_LIST, LB_GETCOUNT, 0, 0L) < MAX_TRAY);
    EnableWindow(GetDlgItem(dlg, IDC_TRAY_REMOVE),
                 SendDlgItemMessage(dlg, IDC_TRAY_LIST, LB_GETCURSEL, 0, 0L) >= 0);
    EnableWindow(GetDlgItem(dlg, IDC_HIDE_ADD),
                 SendDlgItemMessage(dlg, IDC_HIDE_LIST, LB_GETCOUNT, 0, 0L) < MAX_HIDDEN);
    EnableWindow(GetDlgItem(dlg, IDC_HIDE_REMOVE),
                 SendDlgItemMessage(dlg, IDC_HIDE_LIST, LB_GETCURSEL, 0, 0L) >= 0);
}

static void AddProgram(HWND dlg, int id)
{
    char path[MAX_PATH], had[MAX_PATH];
    int i, n = (int)SendDlgItemMessage(dlg, id, LB_GETCOUNT, 0, 0L);

    path[0] = '\0';
    if (!BrowseDialogShow(dlg, "Add", "Programs\0*.exe;*.com;*.pif;*.bat\0All Files\0*.*\0", path, TRUE))
        return;
    for (i = 0; i < n; i++) {
        SendDlgItemMessage(dlg, id, LB_GETTEXT, i, (LPARAM)(LPSTR)had);
        if (!lstrcmpi(had, path))
            break;
    }
    if (i == n)
        i = (int)SendDlgItemMessage(dlg, id, LB_ADDSTRING, 0, (LPARAM)(LPSTR)path);
    SendDlgItemMessage(dlg, id, LB_SETCURSEL, i, 0L);
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
    Buttons(dlg);
}

static void RemoveProgram(HWND dlg, int id)
{
    int i = (int)SendDlgItemMessage(dlg, id, LB_GETCURSEL, 0, 0L);

    if (i < 0)
        return;
    SendDlgItemMessage(dlg, id, LB_DELETESTRING, i, 0L);
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
    Buttons(dlg);
}

/* A program: its icon, its name. */
static void DrawProgram(const DRAWITEMSTRUCT FAR *d)
{
    char path[MAX_PATH], name[MAX_PATH];

    if (d->itemID == (UINT)-1)
        return;
    SendMessage(d->hwndItem, LB_GETTEXT, d->itemID, (LPARAM)(LPSTR)path);
    DisplayNameOfFile(PathFileName(path), name);
    if (*PathExtension(name))
        *PathExtension(name) = '\0';
    PaintListItem(d, d->rcItem.left + 2, IconForFile(path, FALSE), name);
}

static void Apply(HWND dlg)
{
    SettingsSetInt(SECTION, "OnTop", IsDlgButtonChecked(dlg, IDC_TASKBAR_ONTOP));
    SettingsSetInt(SECTION, "AutoHide", IsDlgButtonChecked(dlg, IDC_TASKBAR_AUTOHIDE));
    SettingsSetInt(SECTION, "SmallIcons", IsDlgButtonChecked(dlg, IDC_TASKBAR_SMALL));
    SettingsSetInt(SECTION, "Clock", IsDlgButtonChecked(dlg, IDC_TASKBAR_CLOCK));
    SettingsSetInt(SECTION, "IconsOnly", IsDlgButtonChecked(dlg, IDC_TASKBAR_ICONSONLY));
    SettingsSetInt(SECTION, "Group", IsDlgButtonChecked(dlg, IDC_TASKBAR_GROUPED));
    Apply24Hour(IsDlgButtonChecked(dlg, IDC_TASKBAR_24HOUR));
    SaveList(dlg, IDC_TRAY_LIST, "Tray");
    SaveList(dlg, IDC_HIDE_LIST, "Hidden");
    TaskbarApply();
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
}

BOOL CALLBACK __export TaskbarPropertiesProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char dir[MAX_PATH];
    PAINTSTRUCT ps;
    RECT r;

    switch (msg) {
    case WM_INITDIALOG:
        sheet = dlg;
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Taskbar Options");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Start Menu Programs");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Tray");
        LoadList(dlg, IDC_TRAY_LIST, "Tray", MAX_TRAY);
        LoadList(dlg, IDC_HIDE_LIST, "Hidden", MAX_HIDDEN);
        Buttons(dlg);
        CheckDlgButton(dlg, IDC_TASKBAR_ONTOP, SettingsGetInt(SECTION, "OnTop", 1));
        CheckDlgButton(dlg, IDC_TASKBAR_AUTOHIDE, SettingsGetInt(SECTION, "AutoHide", 0));
        CheckDlgButton(dlg, IDC_TASKBAR_SMALL, SettingsGetInt(SECTION, "SmallIcons", 0));
        CheckDlgButton(dlg, IDC_TASKBAR_CLOCK, SettingsGetInt(SECTION, "Clock", 1));
        CheckDlgButton(dlg, IDC_TASKBAR_ICONSONLY, SettingsGetInt(SECTION, "IconsOnly", 0));
        CheckDlgButton(dlg, IDC_TASKBAR_GROUPED, SettingsGetInt(SECTION, "Group", 0));
        CheckDlgButton(dlg, IDC_TASKBAR_24HOUR, GetProfileInt("intl", "iTime", 0) != 0);
        EnableWindow(GetDlgItem(dlg, IDC_TASKBAR_24HOUR), IsDlgButtonChecked(dlg, IDC_TASKBAR_CLOCK));
        EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
        EnableWindow(GetDlgItem(dlg, IDC_TASKBAR_CLEAR), !RecentIsEmpty());
        SendDlgItemMessage(dlg, IDC_TASKBAR_ICON, STM_SETICON, (WPARAM)IconStock(IDI_SM_PROGRAMS), 0L);
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
            PaintPreview(dlg, ps.hdc);
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = ROW_H;
        return TRUE;
    case WM_DRAWITEM:
        DrawProgram((const DRAWITEMSTRUCT FAR *)lp);
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDC_TABS:
            if (HIWORD(lp) == TCN_SELECT)
                TabsShowPage(dlg, TabsSelected(GetDlgItem(dlg, IDC_TABS)), NotPicture);
            break;
        case IDC_TASKBAR_ONTOP:
        case IDC_TASKBAR_AUTOHIDE:
        case IDC_TASKBAR_SMALL:
        case IDC_TASKBAR_CLOCK:
        case IDC_TASKBAR_24HOUR:
        case IDC_TASKBAR_ICONSONLY:
        case IDC_TASKBAR_GROUPED:
            EnableWindow(GetDlgItem(dlg, IDC_TASKBAR_24HOUR), IsDlgButtonChecked(dlg, IDC_TASKBAR_CLOCK));
            EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
            DlgRect(dlg, IDC_TASKBAR_PREVIEW, &r);
            InvalidateRect(dlg, &r, FALSE);
            break;
        case IDC_TASKBAR_ADD:
            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_NEWSHORTCUT), dlg, TaskbarPropertiesWizardProc);
            break;
        case IDC_TASKBAR_REMOVE:
            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_REMOVESC), dlg, TaskbarPropertiesRemoveProc);
            break;
        case IDC_TASKBAR_ADVANCED:
            StartMenuFolder(dir);
            FolderWindowExplore(dir);
            break;
        case IDC_TRAY_ADD:    AddProgram(dlg, IDC_TRAY_LIST); break;
        case IDC_TRAY_REMOVE: RemoveProgram(dlg, IDC_TRAY_LIST); break;
        case IDC_HIDE_ADD:    AddProgram(dlg, IDC_HIDE_LIST); break;
        case IDC_HIDE_REMOVE: RemoveProgram(dlg, IDC_HIDE_LIST); break;
        case IDC_TRAY_LIST:
        case IDC_HIDE_LIST:
            if (HIWORD(lp) == LBN_SELCHANGE)
                Buttons(dlg);
            break;
        case IDC_TASKBAR_CLEAR:
            RecentClear();
            SetFocus(GetDlgItem(dlg, IDC_TASKBAR_ADD));
            EnableWindow(GetDlgItem(dlg, IDC_TASKBAR_CLEAR), FALSE);
            break;
        default:
            SheetButton(dlg, wp, Apply);
        }
        return TRUE;
    case WM_DESTROY:
        MessageLoopTrackDialog(dlg, FALSE);
        sheet = NULL;
        return TRUE;
    }
    return FALSE;
}

void TaskbarPropertiesShow(void)
{
    if (sheet)
        SetActiveWindow(sheet);
    else if (CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_TBPROPS), NULL, TaskbarPropertiesProc) && sheet)
        ShowWindow(sheet, SW_SHOWNORMAL);
}
