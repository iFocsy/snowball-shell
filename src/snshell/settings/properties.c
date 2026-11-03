/*
 * Properties, as Windows 98 shows them. Each kind of item has one dialog
 * whose pages lie at the same place; a tab shows the controls of its page
 * by their id range (Windows 3.1 cannot tab through nested dialogs).
 *   files and folders, one or many: General; shortcuts also Shortcut
 *   a folder: also Customize (its desktop.ini: its own icon, its infotip)
 *   drives: General with the used/free pie; My Computer: System
 * Changes wait for OK or Apply: renaming, attributes, the shortcut.
 */
#include "internal.h"
#include <ver.h>

#define PAGES       2
#define PIE_DEPTH   10

typedef struct {
    PATHLIST items;
    int      kind, page, files, folders;
    BYTE     tabPage[3];                /* the page (IDC_PAGE1 + 50 * page) of each tab */
    BOOL     ready, shortcut;
    BOOL     customize, customChanged;  /* a folder's Customize page */
    char     iconFile[MAX_PATH];        /* its own icon ("": the folder's) */
    int      iconIndex;
    HICON    customIcon;
    char     path[MAX_PATH];            /* the first item */
    SHORTCUT shc;
    WORD     attrs[4];                  /* checkbox states shown at the start */
} SHEET;

typedef struct {
    char  file[MAX_PATH];
    int   index;
} ICONPICK;

static const UINT AttrIds[4] = { IDC_PROPS_READONLY, IDC_PROPS_HIDDEN, IDC_PROPS_ARCHIVE, IDC_PROPS_SYSTEM };
static const WORD AttrBits[4] = { FA_READONLY, FA_HIDDEN, FA_ARCHIVE, FA_SYSTEM };
static const int RunShow[3] = { SW_SHOWNORMAL, SW_SHOWMINNOACTIVE, SW_SHOWMAXIMIZED };

static SHEET sheet;                     /* large; one sheet at a time */
static ICONPICK pick;
static BOOL showing;               /* a sheet is up */

#define Sheet(w)    (&sheet)

static void SetIcon(HWND dlg, int id, HICON icon)
{
    SendDlgItemMessage(dlg, id, STM_SETICON, (WPARAM)icon, 0L);
}

/* Which controls a sheet uses: folders only have "Contains", one item has
   names and dates, and every page has hidden separators. */
static BOOL Wanted(SHEET *s, int id)
{
    if (id == IDC_PROPS_SEP || id == IDC_SHORTCUT_SEP || id == IDC_DRIVE_PIE || id == IDC_DRIVE_USEDBOX || id == IDC_DRIVE_FREEBOX)
        return FALSE;
    if ((id == IDC_PROPS_CONTAINSL || id == IDC_PROPS_CONTAINS) && !s->folders)
        return FALSE;
    if (id >= IDC_PROPS_DATESL && id <= IDC_PROPS_ACCESSED && s->items.count > 1)
        return FALSE;
    return TRUE;
}

static BOOL WantedHere(HWND dlg, int id)
{
    return Wanted(Sheet(dlg), id);
}

static void ShowPage(HWND dlg, int tab)
{
    SHEET *s = Sheet(dlg);

    s->page = s->tabPage[tab];
    TabsShowPage(dlg, s->page, WantedHere);
}

static void Changed(HWND dlg)
{
    if (Sheet(dlg)->ready)
        EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
}

/* ---- files and folders ---- */

static void InitFiles(HWND dlg, SHEET *s)
{
    static DOSENTRY e;
    char text[MAX_PATH + 40], parent[MAX_PATH], first[MAX_PATH], type[48];
    DWORD bytes = 0;
    WORD files = 0, folders = 0, attr, all = 0xFF, any = 0;
    BOOL sameFolder = TRUE, sameType = TRUE, dir;
    LPSTR p;
    int i;

    SetCursor(LoadCursor(NULL, IDC_WAIT));      /* folders are walked for their size */
    first[0] = parent[0] = '\0';
    for (p = PathListLock(&s->items); *p; p = PathListNext(p)) {
        attr = DosGetAttributes(p);
        dir = attr != 0xFFFF && (attr & FA_DIR);
        if (dir) {
            s->folders++;
            FileOperationTotals(p, &bytes, &files, &folders);
        } else {
            s->files++;
            if (DosGetEntry(p, &e))
                bytes += e.size;
        }
        all &= attr;
        any |= attr;
        if (dir)
            lstrcpy(type, "File Folder");
        else
            FormatFileType(p, type);
        lstrcpy(text, p);
        PathToParent(text);
        if (!*first) {
            lstrcpy(first, type);
            lstrcpy(parent, text);
        } else {
            sameType = sameType && !lstrcmp(first, type);
            sameFolder = sameFolder && !lstrcmpi(parent, text);
        }
    }
    PathListUnlock(&s->items);
    SetCursor(LoadCursor(NULL, IDC_ARROW));

    if (s->items.count == 1) {
        DosGetEntry(s->path, &e);
        DisplayNameOfFile(e.name, text);
        SetDlgItemText(dlg, IDC_PROPS_NAME, text);
        SetIcon(dlg, IDC_PROPS_ICON, IconForFile(s->path, s->folders));
        if (s->folders)
            lstrcpy(text, "File Folder");
        else
            FormatFileType(s->path, text);
        SetDlgItemText(dlg, IDC_PROPS_TYPE, text);
        DosShortPath(s->path, text);
        SetDlgItemText(dlg, IDC_PROPS_DOSNAME, PathFileName(text));
        FormatLongDateTime(e.cdate, e.ctime, TRUE, text);
        SetDlgItemText(dlg, IDC_PROPS_CREATED, text);
        FormatLongDateTime(e.date, e.time, TRUE, text);
        SetDlgItemText(dlg, IDC_PROPS_MODIFIED, text);
        FormatLongDateTime(e.adate, 0, FALSE, text);
        SetDlgItemText(dlg, IDC_PROPS_ACCESSED, text);
        lstrcpy(text, parent);
    } else {
        wsprintf(text, "%d Files, %d Folders", s->files, s->folders);
        SetDlgItemText(dlg, IDC_PROPS_NAME, text);
        SendDlgItemMessage(dlg, IDC_PROPS_NAME, EM_SETREADONLY, TRUE, 0L);
        SetIcon(dlg, IDC_PROPS_ICON, IconStock(IDI_FILE));
        if (!sameType)
            lstrcpy(text, "Multiple types");
        else
            wsprintf(text, "All of type %s", (LPSTR)first);
        SetDlgItemText(dlg, IDC_PROPS_TYPE, text);
        if (sameFolder)
            wsprintf(text, "All in %s", (LPSTR)parent);
        else
            lstrcpy(text, "Various folders");
    }
    SetDlgItemText(dlg, IDC_PROPS_LOCATION, text);
    FormatBytes(bytes, text);
    SetDlgItemText(dlg, IDC_PROPS_SIZE, text);
    wsprintf(text, "%u Files, %u Folders", files, folders);
    SetDlgItemText(dlg, IDC_PROPS_CONTAINS, text);
    for (i = 0; i < 4; i++) {                   /* set, clear or mixed */
        s->attrs[i] = (all & AttrBits[i]) ? 1 : (any & AttrBits[i]) ? 2 : 0;
        if (s->items.count == 1) {              /* one item: plain check boxes */
            HWND c = GetDlgItem(dlg, AttrIds[i]);
            SetWindowLong(c, GWL_STYLE, (GetWindowLong(c, GWL_STYLE) & ~0x0FL) | BS_AUTOCHECKBOX);
        }
        CheckDlgButton(dlg, AttrIds[i], s->attrs[i]);
    }
}

static void InitShortcut(HWND dlg, SHEET *s)
{
    static const char *Runs[3] = { "Normal window", "Minimized", "Maximized" };
    char text[MAX_PATH + 140], folder[MAX_PATH];
    int i;

    DisplayNameOfFile(PathFileName(s->path), text);
    SetDlgItemText(dlg, IDC_SHORTCUT_NAME, text);
    SetIcon(dlg, IDC_SHORTCUT_ICON, IconForShortcut(s->path));
    if (PathIsFolder(s->shc.target))
        lstrcpy(text, "File Folder");
    else
        FormatFileType(s->shc.target, text);
    SetDlgItemText(dlg, IDC_SHORTCUT_TYPE, text);
    lstrcpy(folder, s->shc.target);
    PathToParent(folder);
    SetDlgItemText(dlg, IDC_SHORTCUT_LOCATION, lstrlen(folder) > 3 ? PathFileName(folder) : folder);
    wsprintf(text, _fstrchr(s->shc.target, ' ') ? "\"%s\"" : "%s", (LPSTR)s->shc.target);
    if (s->shc.args[0])
        wsprintf(text + lstrlen(text), " %s", (LPSTR)s->shc.args);
    SetDlgItemText(dlg, IDC_SHORTCUT_TARGET, text);
    SetDlgItemText(dlg, IDC_SHORTCUT_STARTIN, s->shc.dir);
    ShortcutKeysField(GetDlgItem(dlg, IDC_SHORTCUT_KEY), s->shc.hotkey);
    for (i = 0; i < 3; i++)
        SendDlgItemMessage(dlg, IDC_SHORTCUT_RUN, CB_ADDSTRING, 0, (LPARAM)(LPSTR)Runs[i]);
    for (i = 2; i > 0 && RunShow[i] != s->shc.show && !(i == 1 && s->shc.show == SW_SHOWMINIMIZED); i--)
        ;
    SendDlgItemMessage(dlg, IDC_SHORTCUT_RUN, CB_SETCURSEL, i, 0L);
}

/* OK and Apply: FALSE keeps the sheet open (a name to correct). */
static BOOL ApplyFiles(HWND dlg, SHEET *s)
{
    char name[MAX_PATH], to[MAX_PATH], cmd[MAX_PATH + 140], parent[MAX_PATH];
    WORD set = 0, clear = 0, attr;
    LPSTR p;
    int i, state;

    if (s->items.count == 1) {
        GetDlgItemText(dlg, IDC_PROPS_NAME, name, sizeof name);
        DisplayNameOfFile(PathFileName(s->path), to);
        if (lstrcmp(name, to)) {
            if (FileCommandsRename(dlg, s->path, name, to) == 0)
                return FALSE;
            if (lstrcmp(s->path, to)) {         /* the rest applies to the new name */
                PathListFree(&s->items);
                PathListAdd(&s->items, to);
                lstrcpy(s->path, to);
            }
        }
    }
    for (i = 0; i < 4; i++) {
        state = IsDlgButtonChecked(dlg, AttrIds[i]);
        if (state != s->attrs[i] && state != 2) {
            if (state)
                set |= AttrBits[i];
            else
                clear |= AttrBits[i];
            s->attrs[i] = state;
        }
    }
    for (p = PathListLock(&s->items); *p && (set | clear); p = PathListNext(p))
        if ((attr = DosGetAttributes(p)) != 0xFFFF)
            DosSetAttributes(p, ((attr & ~clear) | set) & (FA_READONLY | FA_HIDDEN | FA_SYSTEM | FA_ARCHIVE));
    PathListUnlock(&s->items);
    if (s->shortcut) {
        GetDlgItemText(dlg, IDC_SHORTCUT_TARGET, cmd, sizeof cmd);
        LaunchSplitCommand(cmd, s->shc.target, s->shc.args, sizeof s->shc.args);
        GetDlgItemText(dlg, IDC_SHORTCUT_STARTIN, s->shc.dir, sizeof s->shc.dir);
        s->shc.show = RunShow[(int)SendDlgItemMessage(dlg, IDC_SHORTCUT_RUN, CB_GETCURSEL, 0, 0L) % 3];
        s->shc.hotkey = ShortcutKeysFieldKey(GetDlgItem(dlg, IDC_SHORTCUT_KEY));
        if (!ShortcutWrite(s->path, &s->shc)) {
            FileOperationError(dlg, "save", s->path, DosLastError());
            return FALSE;
        }
        ShortcutKeysScan();
    }
    if (s->customChanged) {
        GetDlgItemText(dlg, IDC_CUSTOM_TIP, cmd, 256);
        for (p = cmd; *p; p++)          /* one line, as desktop.ini keeps it */
            if (*p == '\r' || *p == '\n')
                *p = ' ';
        if (!FolderIniWrite(s->path, s->iconFile, s->iconIndex, cmd)) {
            ErrorBox(dlg, "This folder's icon and infotip cannot be saved.");
            return FALSE;
        }
        IconsForget(s->path);
        SetIcon(dlg, IDC_PROPS_ICON, IconForFile(s->path, TRUE));
        s->customChanged = FALSE;
    }
    lstrcpy(parent, s->path);
    PathToParent(parent);
    FileCommandsFolderChanged(parent);
    return TRUE;
}

/* ---- Change Icon ---- */

static void FillIcons(HWND dlg, LPCSTR file)
{
    char shortPath[MAX_PATH];
    HWND list = GetDlgItem(dlg, IDC_CHANGEICON_LIST);
    int i, n;

    for (i = (int)SendMessage(list, LB_GETCOUNT, 0, 0L) - 1; i >= 0; i--)
        DestroyIcon((HICON)SendMessage(list, LB_GETITEMDATA, i, 0L));
    SendMessage(list, LB_RESETCONTENT, 0, 0L);
    DosShortPath(file, shortPath);
    n = (int)ExtractIcon(g_hInst, shortPath, (UINT)-1);
    for (i = 0; i < n; i++)
        SendMessage(list, LB_ADDSTRING, 0, (LPARAM)(DWORD)(WORD)ExtractIcon(g_hInst, shortPath, i));
}

BOOL CALLBACK __export PropertiesChangeIconProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    DRAWITEMSTRUCT FAR *d;
    char file[MAX_PATH];
    int i;

    switch (msg) {
    case WM_INITDIALOG:
        SkinDialog(dlg);
        SendDlgItemMessage(dlg, IDC_CHANGEICON_LIST, LB_SETCOLUMNWIDTH, 40, 0L);
        SetDlgItemText(dlg, IDC_CHANGEICON_FILE, pick.file);
        FillIcons(dlg, pick.file);
        SendDlgItemMessage(dlg, IDC_CHANGEICON_LIST, LB_SETCURSEL, pick.index, 0L);
        WindowCenter(dlg);
        return TRUE;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = 36;
        return TRUE;
    case WM_DRAWITEM:
        d = (DRAWITEMSTRUCT FAR *)lp;
        if (d->itemID != (UINT)-1) {
            DrawSolidRect(d->hDC, &d->rcItem, GetSysColor((d->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHT : COLOR_WINDOW));
            DrawIconImage(d->hDC, d->rcItem.left + 4, d->rcItem.top + 2, (HICON)(WORD)d->itemData, FALSE);
        }
        if (d->itemState & ODS_FOCUS)
            DrawFocusRect(d->hDC, &d->rcItem);
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDC_CHANGEICON_BROWSE:
            GetDlgItemText(dlg, IDC_CHANGEICON_FILE, file, sizeof file);
            if (BrowseDialogShow(dlg, "Change Icon", "Icon files\0*.ico;*.exe;*.dll\0All files\0*.*\0", file, FALSE)) {
                SetDlgItemText(dlg, IDC_CHANGEICON_FILE, file);
                FillIcons(dlg, file);
                SendDlgItemMessage(dlg, IDC_CHANGEICON_LIST, LB_SETCURSEL, 0, 0L);
            }
            return TRUE;
        case IDOK:
            GetDlgItemText(dlg, IDC_CHANGEICON_FILE, pick.file, sizeof pick.file);
            i = (int)SendDlgItemMessage(dlg, IDC_CHANGEICON_LIST, LB_GETCURSEL, 0, 0L);
            pick.index = max(0, i);
            /* fall through */
        case IDCANCEL:
            FillIcons(dlg, "");                 /* frees the icons */
            EndDialog(dlg, wp == IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* Change Icon: file and index in, the icon chosen out. */
BOOL PropertiesPickIcon(HWND owner, LPSTR file, int *index)
{
    lstrcpy(pick.file, file);
    pick.index = *index;
    if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CHANGEICON), owner, PropertiesChangeIconProc) != 1)
        return FALSE;
    lstrcpy(file, pick.file);
    *index = pick.index;
    return TRUE;
}

static void ChangeIcon(HWND dlg, SHEET *s)
{
    char file[MAX_PATH];
    int index = s->shc.icon[0] ? s->shc.iconIndex : 0;

    lstrcpy(file, s->shc.icon[0] ? s->shc.icon : s->shc.target);
    if (PropertiesPickIcon(dlg, file, &index)) {
        lstrcpy(s->shc.icon, file);
        s->shc.iconIndex = index;
        Changed(dlg);
    }
}

/* ---- Customize: a folder's own icon and infotip (desktop.ini) ---- */

static void ShowFolderIcon(HWND dlg, SHEET *s)
{
    char shortPath[MAX_PATH];
    HICON old = s->customIcon;

    s->customIcon = NULL;
    if (s->iconFile[0] && DosShortPath(s->iconFile, shortPath) &&
        (UINT)(s->customIcon = ExtractIcon(g_hInst, shortPath, s->iconIndex)) <= 1)
        s->customIcon = NULL;
    SetIcon(dlg, IDC_CUSTOM_ICON, s->customIcon ? s->customIcon : IconStock(IDI_FOLDER));
    if (old)
        DestroyIcon(old);
    EnableWindow(GetDlgItem(dlg, IDC_CUSTOM_DEFAULT), s->iconFile[0] != '\0');
}

static void InitCustomize(HWND dlg, SHEET *s)
{
    char tip[256];

    s->customize = TRUE;
    if (!FolderIniIcon(s->path, s->iconFile, &s->iconIndex))
        s->iconFile[0] = '\0';
    ShowFolderIcon(dlg, s);
    FolderIniRead(s->path, "InfoTip", tip, sizeof tip);
    SendDlgItemMessage(dlg, IDC_CUSTOM_TIP, EM_LIMITTEXT, sizeof tip - 1, 0L);
    SetDlgItemText(dlg, IDC_CUSTOM_TIP, tip);
}

/* Change Icon starts on Snowball's icons (W98ART.DLL), as Windows 98's on SHELL32.DLL. */
static void ChangeFolderIcon(HWND dlg, SHEET *s)
{
    char file[MAX_PATH];
    int index = s->iconFile[0] ? s->iconIndex : 0;

    if (s->iconFile[0])
        lstrcpy(file, s->iconFile);
    else
        IconsArtFile(file);
    if (PropertiesPickIcon(dlg, file, &index)) {
        lstrcpy(s->iconFile, file);
        s->iconIndex = index;
        ShowFolderIcon(dlg, s);
        s->customChanged = TRUE;
        Changed(dlg);
    }
}

/* Restore Default: the folder's own icon dropped. */
static void DefaultFolderIcon(HWND dlg, SHEET *s)
{
    s->iconFile[0] = '\0';
    ShowFolderIcon(dlg, s);
    SetFocus(GetDlgItem(dlg, IDC_CUSTOM_CHANGE));       /* not on a greyed button */
    s->customChanged = TRUE;
    Changed(dlg);
}

/* ---- Version: a program's or library's version resource ---- */

#define MAX_VERITEMS    10

static const char *VerKeys[][2] = {     /* resource name, as Windows 98 lists it */
    { "Comments", "Comments" }, { "CompanyName", "Company Name" }, { "InternalName", "Internal Name" },
    { "", "Language" }, { "LegalTrademarks", "Legal Trademarks" }, { "OriginalFilename", "Original File name" },
    { "PrivateBuild", "Private Build" }, { "ProductName", "Product Name" },
    { "ProductVersion", "Product Version" }, { "SpecialBuild", "Special Build" },
};
static char verValues[MAX_VERITEMS][128];

/* A string of the version resource, "" if it has none. */
static void VerString(void FAR *data, DWORD lang, LPCSTR name, LPSTR out, int size)
{
    char key[64];
    LPSTR value;
    UINT n;

    wsprintf(key, "\\StringFileInfo\\%04X%04X\\%s", LOWORD(lang), HIWORD(lang), name);
    if (VerQueryValue(data, key, (void FAR * FAR *)&value, &n) && n)
        lstrcpyn(out, value, size);
    else
        *out = '\0';
}

/* Fills the Version page; FALSE when the file has no version resource. */
static BOOL InitVersion(HWND dlg, SHEET *s)
{
    char file[MAX_PATH], text[128];
    DWORD handle, size, lang = MAKELONG(0x0409, 0x04E4), FAR *trans;
    VS_FIXEDFILEINFO FAR *fixed;
    HGLOBAL h;
    void FAR *data;
    UINT n;
    int i, row;

    if (s->items.count != 1 || s->folders || s->shortcut)
        return FALSE;
    DosShortPath(s->path, file);
    if ((size = GetFileVersionInfoSize(file, &handle)) == 0 || (h = GlobalAlloc(GMEM_MOVEABLE, size)) == NULL)
        return FALSE;
    data = GlobalLock(h);
    if (!GetFileVersionInfo(file, handle, size, data)) {
        GlobalUnlock(h);
        GlobalFree(h);
        return FALSE;
    }
    if (VerQueryValue(data, "\\VarFileInfo\\Translation", (void FAR * FAR *)&trans, &n) && n >= 4)
        lang = *trans;
    if (VerQueryValue(data, "\\", (void FAR * FAR *)&fixed, &n) && n) {
        wsprintf(text, "%u.%u.%u.%u", HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS),
                 HIWORD(fixed->dwFileVersionLS), LOWORD(fixed->dwFileVersionLS));
        SetDlgItemText(dlg, IDC_VERSION_FILE, text);
    }
    VerString(data, lang, "FileDescription", text, sizeof text);
    SetDlgItemText(dlg, IDC_VERSION_DESC, text);
    VerString(data, lang, "LegalCopyright", text, sizeof text);
    SetDlgItemText(dlg, IDC_VERSION_COPY, text);
    for (i = 0; i < MAX_VERITEMS; i++) {
        if (*VerKeys[i][0])
            VerString(data, lang, VerKeys[i][0], verValues[i], sizeof verValues[0]);
        else
            VerLanguageName(LOWORD(lang), verValues[i], sizeof verValues[0]);
        if (verValues[i][0]) {
            row = (int)SendDlgItemMessage(dlg, IDC_VERSION_ITEMS, LB_ADDSTRING, 0, (LPARAM)(LPSTR)VerKeys[i][1]);
            SendDlgItemMessage(dlg, IDC_VERSION_ITEMS, LB_SETITEMDATA, row, i);
        }
    }
    SendDlgItemMessage(dlg, IDC_VERSION_ITEMS, LB_SETCURSEL, 0, 0L);
    SendMessage(dlg, WM_COMMAND, IDC_VERSION_ITEMS, MAKELPARAM(GetDlgItem(dlg, IDC_VERSION_ITEMS), LBN_SELCHANGE));
    GlobalUnlock(h);
    GlobalFree(h);
    return TRUE;
}

/* ---- drives ---- */

/* sin of a degrees, in thousandths (every 10 degrees, then straight lines). */
static int Sin(int a)
{
    static const int t[] = { 0, 174, 342, 500, 643, 766, 866, 940, 985, 1000 };
    int sign = 1, i;

    a %= 360;
    if (a >= 180) {
        a -= 180;
        sign = -1;
    }
    if (a > 90)
        a = 180 - a;
    i = a / 10;
    return sign * (i < 9 ? t[i] + (t[i + 1] - t[i]) * (a % 10) / 10 : 1000);
}

static void Slice(HDC dc, RECT *r, int degrees, COLORREF used, COLORREF free)
{
    HBRUSH b = CreateSolidBrush(free), old = SelectObject(dc, b);
    int cx = (r->left + r->right) / 2, cy = (r->top + r->bottom) / 2;

    Ellipse(dc, r->left, r->top, r->right, r->bottom);
    SelectObject(dc, old);
    DeleteObject(b);
    if (degrees <= 0)
        return;
    b = CreateSolidBrush(used);
    old = SelectObject(dc, b);
    if (degrees >= 360)
        Ellipse(dc, r->left, r->top, r->right, r->bottom);
    else                                /* from 3 o'clock, counterclockwise */
        Pie(dc, r->left, r->top, r->right, r->bottom, r->right, cy,
            cx + Sin(degrees + 90) / 10, cy - Sin(degrees) / 10);
    SelectObject(dc, old);
    DeleteObject(b);
}

/* The Windows 98 disk pie: blue used, magenta free, standing 10 pixels high. */
static void PaintPie(HWND dlg, HDC dc, DWORD total, DWORD free)
{
    RECT r, box;
    int degrees = total ? (int)((total - free) / (total / 360 + 1)) : 0, d;
    HPEN pen = SelectObject(dc, GetStockObject(NULL_PEN));

    GetWindowRect(GetDlgItem(dlg, IDC_DRIVE_PIE), &r);
    ScreenToClient(dlg, (POINT FAR *)&r.left);
    ScreenToClient(dlg, (POINT FAR *)&r.right);
    r.bottom -= PIE_DEPTH;
    for (d = PIE_DEPTH; d > 0; d--) {           /* the side, bottom up */
        OffsetRect(&r, 0, d);
        Slice(dc, &r, degrees, RGB(0, 0, 128), RGB(128, 0, 128));
        OffsetRect(&r, 0, -d);
    }
    SelectObject(dc, GetStockObject(BLACK_PEN));
    Slice(dc, &r, degrees, RGB(0, 0, 255), RGB(255, 0, 255));
    SelectObject(dc, pen);
    GetWindowRect(GetDlgItem(dlg, IDC_DRIVE_USEDBOX), &box);
    ScreenToClient(dlg, (POINT FAR *)&box.left);
    ScreenToClient(dlg, (POINT FAR *)&box.right);
    DrawSolidRect(dc, &box, RGB(0, 0, 255));
    DrawFrame(dc, &box, RGB(0, 0, 0), RGB(0, 0, 0));
    GetWindowRect(GetDlgItem(dlg, IDC_DRIVE_FREEBOX), &box);
    ScreenToClient(dlg, (POINT FAR *)&box.left);
    ScreenToClient(dlg, (POINT FAR *)&box.right);
    DrawSolidRect(dc, &box, RGB(255, 0, 255));
    DrawFrame(dc, &box, RGB(0, 0, 0), RGB(0, 0, 0));
}

static void SpaceLine(HWND dlg, int id, DWORD bytes)
{
    char text[32];
    FormatNumber(bytes, text);
    lstrcat(text, " bytes");
    SetDlgItemText(dlg, id, text);
    FormatSize(bytes, text);
    SetDlgItemText(dlg, id + 1, text);
}

static void InitDrive(HWND dlg, SHEET *s)
{
    char text[40];
    int drive = (s->path[0] | 0x20) - 'a';
    DWORD total = 0, free = 0;

    SetIcon(dlg, IDC_PROPS_ICON, IconForDrive(drive));
    DosVolumeLabel(drive, text);
    SendDlgItemMessage(dlg, IDC_DRIVE_LABEL, EM_LIMITTEXT, 11, 0L);
    SetDlgItemText(dlg, IDC_DRIVE_LABEL, text);
    if (!DosDiskSpace(drive, &total, &free))    /* no disk in it: nothing to label */
        SendDlgItemMessage(dlg, IDC_DRIVE_LABEL, EM_SETREADONLY, TRUE, 0L);
    DriveTypeName(drive, text);
    SetDlgItemText(dlg, IDC_PROPS_TYPE, text);
    SetDlgItemText(dlg, IDC_DRIVE_FS, "FAT");
    DosDiskSpace(drive, &total, &free);
    SpaceLine(dlg, IDC_DRIVE_USED, total - free);
    SpaceLine(dlg, IDC_DRIVE_FREE, free);
    SpaceLine(dlg, IDC_DRIVE_CAP, total);
    wsprintf(text, "Drive %c", 'A' + drive);
    SetDlgItemText(dlg, IDC_DRIVE_DRIVE, text);
}

/* ---- My Computer ---- */

static void InitSystem(HWND dlg)
{
    char text[64];
    DWORD flags = GetWinFlags(), version = GetVersion();

    SetIcon(dlg, IDC_PROPS_ICON, IconStock(IDI_COMPUTER));
    SetDlgItemText(dlg, IDC_COMPUTER_SYSTEM, GetModuleHandle("WFWNET") ? "Microsoft Windows for Workgroups"
                                                               : "Microsoft Windows");
    wsprintf(text, "Version %d.%02d", LOBYTE(LOWORD(version)), HIBYTE(LOWORD(version)));
    SetDlgItemText(dlg, IDC_COMPUTER_VERSION, text);
    SetDlgItemText(dlg, IDC_COMPUTER_SHELL, SB_NAME " " SB_VERSION);
    SetDlgItemText(dlg, IDC_COMPUTER_CPU, (flags & WF_CPU486) ? "80486 or better" :
                                   (flags & WF_CPU386) ? "80386" : "80286");
    SetDlgItemText(dlg, IDC_COMPUTER_MODE, (flags & WF_ENHANCED) ? "386 Enhanced Mode" : "Standard Mode");
    FormatSize(GetFreeSpace(0), text);
    lstrcat(text, " free memory");
    SetDlgItemText(dlg, IDC_COMPUTER_MEMORY, text);
    wsprintf(text, "%u%% system resources free", GetFreeSystemResources(GFSR_SYSTEMRESOURCES));
    SetDlgItemText(dlg, IDC_COMPUTER_RESOURCES, text);
}

/* ---- the sheet ---- */

/* Separators of the shown page, etched as in Windows 98. */
static void PaintSeparators(HWND dlg, HDC dc)
{
    SHEET *s = Sheet(dlg);
    RECT r;
    HWND c;
    int want = s->page == 1 ? IDC_SHORTCUT_SEP : s->page == 0 ? IDC_PROPS_SEP : -2;

    for (c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        if (GetDlgCtrlID(c) == want) {
            GetWindowRect(c, &r);
            ScreenToClient(dlg, (POINT FAR *)&r.left);
            ScreenToClient(dlg, (POINT FAR *)&r.right);
            r.bottom = r.top + 1;
            DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNSHADOW));
            OffsetRect(&r, 0, 1);
            DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT));
        }
}

/* The pages of the item's kind, filled, each with its tab. */
static void InitPages(HWND dlg, SHEET *s)
{
    HWND tabs = GetDlgItem(dlg, IDC_TABS);

    TabsAdd(tabs, "General");
    s->tabPage[0] = 0;
    if (s->shortcut) {
        TabsAdd(tabs, "Shortcut");
        s->tabPage[1] = 1;
    }
    switch (s->kind) {
    case IVK_DRIVE:    InitDrive(dlg, s); break;
    case IVK_COMPUTER: InitSystem(dlg); break;
    default:
        InitFiles(dlg, s);
        if (s->shortcut)
            InitShortcut(dlg, s);
        else if (s->items.count == 1 && s->folders == 1 && lstrlen(s->path) > 3) {
            InitCustomize(dlg, s);
            TabsAdd(tabs, "Customize");
            s->tabPage[1] = 3;
        } else if (InitVersion(dlg, s)) {
            TabsAdd(tabs, "Version");
            s->tabPage[1] = 2;
        }
    }
}

/* The title: the item's name, a drive's letter, or none for many items. */
static void SetTitle(HWND dlg, SHEET *s)
{
    char title[MAX_PATH + 16];

    if (s->kind == IVK_DRIVE)
        wsprintf(title, "(%c:)", s->path[0]);
    else if (s->items.count == 1)
        DisplayNameOfFile(PathFileName(s->path), title);
    else
        title[0] = '\0';
    lstrcat(title, *title ? " Properties" : "Properties");
    SetWindowText(dlg, title);
}

/* The sheet filled and placed, nothing changed yet. */
static void Init(HWND dlg, SHEET *s)
{
    SkinDialog(dlg);
    InitPages(dlg, s);
    if (s->kind != IVK_COMPUTER)            /* "Notepad Properties" */
        SetTitle(dlg, s);
    if (s->shortcut)                        /* as Windows 98: a shortcut's own page first */
        TabsSelect(GetDlgItem(dlg, IDC_TABS), 1);
    ShowPage(dlg, s->shortcut ? 1 : 0);
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
    WindowCenter(dlg);
    s->ready = TRUE;
}

/* The shown page's separators, and a drive's pie. */
static void Paint(HWND dlg, SHEET *s)
{
    PAINTSTRUCT ps;
    DWORD total, free;

    BeginPaint(dlg, &ps);
    PaintSeparators(dlg, ps.hdc);
    if (s->kind == IVK_DRIVE && DosDiskSpace((s->path[0] | 0x20) - 'a', &total, &free))
        PaintPie(dlg, ps.hdc, total, free);
    EndPaint(dlg, &ps);
}

/* OK and Apply, when something changed: FALSE keeps the sheet as it is. */
static BOOL Apply(HWND dlg, SHEET *s)
{
    char label[12];

    if (!IsWindowEnabled(GetDlgItem(dlg, IDC_APPLY)) || s->kind == IVK_COMPUTER)
        return TRUE;
    if (s->kind != IVK_DRIVE)
        return ApplyFiles(dlg, s);
    GetDlgItemText(dlg, IDC_DRIVE_LABEL, label, sizeof label);
    if (!DosSetVolumeLabel((s->path[0] | 0x20) - 'a', label)) {
        ErrorBox(dlg, "The label of this drive cannot be changed.");
        return FALSE;
    }
    FileCommandsFolderChanged(NULL);        /* My Computer shows it */
    return TRUE;
}

/* A button or a control's notification; FALSE when not handled. */
static BOOL Command(HWND dlg, SHEET *s, UINT id, UINT code)
{
    int i;

    switch (id) {
    case IDC_TABS:
        if (code == TCN_SELECT)
            ShowPage(dlg, TabsSelected(GetDlgItem(dlg, IDC_TABS)));
        return TRUE;
    case IDOK:
    case IDC_APPLY:
        if (Apply(dlg, s)) {
            EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
            if (id == IDOK)
                EndDialog(dlg, TRUE);
        }
        return TRUE;
    case IDCANCEL:
        EndDialog(dlg, FALSE);
        return TRUE;
    case IDC_VERSION_ITEMS:
        if (code == LBN_SELCHANGE && (i = (int)SendDlgItemMessage(dlg, IDC_VERSION_ITEMS, LB_GETCURSEL, 0, 0L)) >= 0)
            SetDlgItemText(dlg, IDC_VERSION_VALUE,
                           verValues[(int)SendDlgItemMessage(dlg, IDC_VERSION_ITEMS, LB_GETITEMDATA, i, 0L)]);
        return TRUE;
    case IDC_SHORTCUT_FIND:
        FolderWindowSelect(s->shc.target);
        return TRUE;
    case IDC_SHORTCUT_ICONBTN:
        ChangeIcon(dlg, s);
        return TRUE;
    case IDC_CUSTOM_CHANGE:
        ChangeFolderIcon(dlg, s);
        return TRUE;
    case IDC_CUSTOM_DEFAULT:
        DefaultFolderIcon(dlg, s);
        return TRUE;
    case IDC_CUSTOM_TIP:
        if (code == EN_CHANGE && s->ready) {
            s->customChanged = TRUE;
            Changed(dlg);
        }
        return TRUE;
    case IDC_DRIVE_LABEL:
    case IDC_PROPS_NAME:
    case IDC_SHORTCUT_TARGET:
    case IDC_SHORTCUT_STARTIN:
    case IDC_SHORTCUT_KEY:
        if (code == EN_CHANGE)
            Changed(dlg);
        return TRUE;
    case IDC_SHORTCUT_RUN:
        if (code == CBN_SELCHANGE)
            Changed(dlg);
        return TRUE;
    default:
        if (id >= IDC_PROPS_READONLY && id <= IDC_PROPS_SYSTEM)
            Changed(dlg);
    }
    return FALSE;
}

BOOL CALLBACK __export PropertiesProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    SHEET *s = Sheet(dlg);

    switch (msg) {
    case WM_INITDIALOG:
        Init(dlg, s);
        return TRUE;
    case WM_PAINT:
        Paint(dlg, s);
        return TRUE;
    case WM_COMMAND:
        return Command(dlg, s, wp, HIWORD(lp));
    }
    return FALSE;
}

void PropertiesShow(HWND owner, PATHLIST *items, int kind)
{
    LPSTR p;

    if (!items->count || showing) {
        MessageBeep(0);
        return;
    }
    showing = TRUE;
    _fmemset(&sheet, 0, sizeof sheet);
    p = PathListLock(items);
    lstrcpy(sheet.path, p);
    for (; *p; p = PathListNext(p))
        PathListAdd(&sheet.items, p);
    PathListUnlock(items);
    sheet.kind = kind;
    sheet.shortcut = items->count == 1 && PathIsShortcut(sheet.path) && ShortcutRead(sheet.path, &sheet.shc);
    DialogBox(g_hInst, MAKEINTRESOURCE(kind == IVK_DRIVE ? IDD_PROPDRIVE :
                                       kind == IVK_COMPUTER ? IDD_PROPSYSTEM : IDD_PROPFILE),
              owner, PropertiesProc);
    if (sheet.customIcon)
        DestroyIcon(sheet.customIcon);
    PathListFree(&sheet.items);
    showing = FALSE;
}
