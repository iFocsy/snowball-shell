/*
 * Folder Options' File Types page, as in Windows 98: the registered types
 * by description with their icons, the selected one's extensions and
 * program; New Type, Remove and Edit, whose dialog has the type's icon,
 * description, extension (a new type's), actions (open, print... the
 * default one bold) and Always show extension. Changes are kept at once,
 * as Windows 98 does, not by the sheet's OK.
 */
#include "internal.h"

#define MAX_TYPES   80
#define ROW_H       18

static FILETYPEID ids[MAX_TYPES];
static int nTypes;
static FILETYPE edit;                   /* the type in the Edit dialog */
static BOOL editNew;
static int editAction;                  /* its action in the action dialog, -1 new */

static HICON TypeIcon(LPCSTR exts)
{
    char ext[8], name[12];

    FileTypeFirstExt(exts, ext);
    wsprintf(name, "x.%s", (LPSTR)ext);
    return IconForFile(name, FALSE);
}

/* Whether ext is one of a type's extensions ("TXT INI"). */
static BOOL HasExt(LPCSTR exts, LPCSTR ext)
{
    char all[48];
    LPSTR e;

    lstrcpy(all, exts);
    for (e = _fstrtok(all, " "); e; e = _fstrtok(NULL, " "))
        if (!lstrcmpi(e, ext))
            return TRUE;
    return FALSE;
}

/* The types again, the one with extension ext selected (NULL: row). */
static void Fill(HWND page, LPCSTR ext, int row)
{
    HWND list = GetDlgItem(page, IDC_FILETYPES_LIST);
    char first[8], name[12], desc[64];
    int i, r;

    SendMessage(list, WM_SETREDRAW, FALSE, 0L);
    SendMessage(list, LB_RESETCONTENT, 0, 0L);
    nTypes = FileTypesList(ids, MAX_TYPES);
    for (i = 0; i < nTypes; i++) {
        FileTypeFirstExt(ids[i].exts, first);
        wsprintf(name, "x.%s", (LPSTR)first);
        FormatFileType(name, desc);
        r = (int)SendMessage(list, LB_ADDSTRING, 0, (LPARAM)(LPSTR)desc);
        SendMessage(list, LB_SETITEMDATA, r, (LPARAM)i);
    }
    for (r = 0; ext && r < nTypes && !HasExt(ids[(int)SendMessage(list, LB_GETITEMDATA, r, 0L)].exts, ext); r++)
        ;
    SendMessage(list, LB_SETCURSEL, ext ? (r < nTypes ? r : 0) : min(row, nTypes - 1), 0L);
    SendMessage(list, WM_SETREDRAW, TRUE, 0L);
    InvalidateRect(list, NULL, TRUE);
}

static int Selected(HWND page)
{
    HWND list = GetDlgItem(page, IDC_FILETYPES_LIST);
    int row = (int)SendMessage(list, LB_GETCURSEL, 0, 0L);

    return row < 0 ? -1 : (int)SendMessage(list, LB_GETITEMDATA, row, 0L);
}

/* The details of the type selected: its icon, extensions, program. */
static void Details(HWND page)
{
    int i = Selected(page);
    char ext[8], cmd[MAX_PATH], program[MAX_PATH];

    EnableWindow(GetDlgItem(page, IDC_FILETYPES_REMOVE), i >= 0);
    EnableWindow(GetDlgItem(page, IDC_FILETYPES_EDIT), i >= 0);
    if (i < 0) {
        SetDlgItemText(page, IDC_FILETYPES_EXT, "");
        SetDlgItemText(page, IDC_FILETYPES_OPENS, "");
        return;
    }
    SendDlgItemMessage(page, IDC_FILETYPES_ICON, STM_SETICON, (WPARAM)TypeIcon(ids[i].exts), 0L);
    SetDlgItemText(page, IDC_FILETYPES_EXT, ids[i].exts);
    FileTypeFirstExt(ids[i].exts, ext);
    program[0] = '\0';
    if (FileTypeOpenCommand(ext, cmd, sizeof cmd) && FileTypeProgram(cmd, program)) {
        lstrcpy(program, PathFileName(program));
        if (*PathExtension(program))
            *PathExtension(program) = '\0';
        AnsiUpper(program);
    }
    SetDlgItemText(page, IDC_FILETYPES_OPENS, program);
}

void FileTypesPageInit(HWND page)
{
    Fill(page, NULL, 0);
    Details(page);
}

/* A row: the type's small icon, its description (selected: highlighted). */
static void DrawRow(const DRAWITEMSTRUCT FAR *d, HICON icon, BOOL bold)
{
    RECT r = d->rcItem, t;
    char text[64];
    BOOL sel = (d->itemState & ODS_SELECTED) != 0;
    HFONT old;
    int x = r.left + 2;

    if (d->itemID == (UINT)-1)
        return;
    DrawSolidRect(d->hDC, &r, GetSysColor(COLOR_WINDOW));
    if (icon) {
        DrawIconImage(d->hDC, x, r.top + (ROW_H - 16) / 2, icon, TRUE);
        x += 19;
    }
    SendMessage(d->hwndItem, LB_GETTEXT, d->itemID, (LPARAM)(LPSTR)text);
    old = SelectObject(d->hDC, DrawFont(bold));
    SetRect(&t, x, r.top + 1, min(r.right, x + 4 + (int)LOWORD(GetTextExtent(d->hDC, text, lstrlen(text)))),
            r.bottom - 1);
    if (sel)
        DrawSolidRect(d->hDC, &t, GetSysColor(COLOR_HIGHLIGHT));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    InflateRect(&t, -2, 0);
    DrawTextEllipsis(d->hDC, text, &t, DT_LEFT | DT_NOPREFIX);
    InflateRect(&t, 2, 0);
    if (d->itemState & ODS_FOCUS)
        DrawFocusRect(d->hDC, &t);
    SelectObject(d->hDC, old);
}

void FileTypesPageDraw(const DRAWITEMSTRUCT FAR *d)
{
    if (d->itemID != (UINT)-1)
        DrawRow(d, TypeIcon(ids[(int)d->itemData].exts), FALSE);
}

/* ---- an action: its name and program line ---- */

BOOL CALLBACK __export FileTypesActionProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char title[100], name[16], cmd[128];
    int i;

    switch (msg) {
    case WM_INITDIALOG:
        if (editAction >= 0) {
            wsprintf(title, "Editing action for type: %s", (LPSTR)edit.desc);
            SetWindowText(dlg, title);
            SetDlgItemText(dlg, IDC_FILEACTION_NAME, edit.actions[editAction].name);
            SetDlgItemText(dlg, IDC_FILEACTION_CMD, edit.actions[editAction].cmd);
        }
        SendDlgItemMessage(dlg, IDC_FILEACTION_NAME, EM_LIMITTEXT, sizeof name - 1, 0L);
        SendDlgItemMessage(dlg, IDC_FILEACTION_CMD, EM_LIMITTEXT, sizeof cmd - 1, 0L);
        SkinDialog(dlg);
        WindowCenter(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDC_FILEACTION_BROWSE:
            cmd[0] = '\0';
            if (BrowseDialogShow(dlg, "Open With", "Programs\0*.exe;*.com;*.pif;*.bat\0All Files\0*.*\0", cmd, FALSE)) {
                lstrcat(cmd, " %1");
                SetDlgItemText(dlg, IDC_FILEACTION_CMD, cmd);
            }
            return TRUE;
        case IDOK:
            GetDlgItemText(dlg, IDC_FILEACTION_NAME, name, sizeof name);
            GetDlgItemText(dlg, IDC_FILEACTION_CMD, cmd, sizeof cmd);
            if (!name[0] || !cmd[0]) {
                ErrorBox(dlg, !name[0] ? "You must type the name of the action." :
                                         "You must type the program used to perform the action.");
                return TRUE;
            }
            for (i = 0; i < edit.nActions && (i == editAction || lstrcmpi(edit.actions[i].name, name)); i++)
                ;
            if (i < edit.nActions) {
                ErrorBox(dlg, "This type already has an action with that name.");
                return TRUE;
            }
            if (editAction < 0)
                editAction = edit.nActions++;
            if (!lstrcmpi(edit.defAction, edit.actions[editAction].name))
                lstrcpy(edit.defAction, name);
            lstrcpy(edit.actions[editAction].name, name);
            lstrcpy(edit.actions[editAction].cmd, cmd);
            EndDialog(dlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* ---- Edit File Type / Add New File Type ---- */

static void FillActions(HWND dlg, int select)
{
    HWND list = GetDlgItem(dlg, IDC_FILETYPE_ACTIONS);
    int i;

    SendMessage(list, LB_RESETCONTENT, 0, 0L);
    for (i = 0; i < edit.nActions; i++)
        SendMessage(list, LB_ADDSTRING, 0, (LPARAM)(LPSTR)edit.actions[i].name);
    SendMessage(list, LB_SETCURSEL, min(select, edit.nActions - 1), 0L);
    EnableWindow(GetDlgItem(dlg, IDC_FILETYPE_EDIT), edit.nActions > 0);
    EnableWindow(GetDlgItem(dlg, IDC_FILETYPE_REMOVE), edit.nActions > 0);
    EnableWindow(GetDlgItem(dlg, IDC_FILETYPE_DEFAULT), edit.nActions > 0);
}

/* The type's icon: its own, else its open program's, else a blank file's. */
static void ShowIcon(HWND dlg)
{
    static HICON shown;
    char program[MAX_PATH], shortPath[MAX_PATH];
    HICON h = NULL;
    int i;

    if (edit.icon[0]) {
        DosShortPath(edit.icon, shortPath);
        h = ExtractIcon(g_hInst, shortPath, edit.iconIndex);
    } else {
        for (i = 0; i < edit.nActions && lstrcmpi(edit.actions[i].name, edit.defAction); i++)
            ;
        if (i < edit.nActions && FileTypeProgram(edit.actions[i].cmd, program)) {
            DosShortPath(program, shortPath);
            h = ExtractIcon(g_hInst, shortPath, 0);
        }
    }
    if ((UINT)h <= 1)
        h = NULL;
    SendDlgItemMessage(dlg, IDC_FILETYPE_ICON, STM_SETICON, (WPARAM)(h ? h : IconStock(IDI_FILE)), 0L);
    if (shown)
        DestroyIcon(shown);
    shown = h;
}

static BOOL ReadNewExt(HWND dlg)
{
    char ext[16], cls[40], msg[120];
    LPSTR e = ext;

    GetDlgItemText(dlg, IDC_FILETYPE_EXT, ext, sizeof ext);
    while (*e == '.' || *e == ' ')
        e++;
    if (!*e || lstrlen(e) > 3 || _fstrpbrk(e, " .\\/*?") != NULL) {
        ErrorBox(dlg, "You must type an extension of up to three letters, such as TXT.");
        return FALSE;
    }
    AnsiUpper(e);
    if (FileTypeClass(e, cls, sizeof cls) || GetProfileString("Extensions", e, "", msg, sizeof msg)) {
        wsprintf(msg, "Extension .%s is already in use by another file type. Choose another extension.", e);
        ErrorBox(dlg, msg);
        return FALSE;
    }
    lstrcpy(edit.exts, e);
    return TRUE;
}

BOOL CALLBACK __export FileTypesEditProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    const DRAWITEMSTRUCT FAR *d;
    char file[MAX_PATH], program[MAX_PATH];
    int i, index;

    switch (msg) {
    case WM_INITDIALOG:
        if (editNew)
            SetWindowText(dlg, "Add New File Type");
        SendDlgItemMessage(dlg, IDC_FILETYPE_DESC, EM_LIMITTEXT, sizeof edit.desc - 1, 0L);
        SetDlgItemText(dlg, IDC_FILETYPE_DESC, edit.desc);
        SetDlgItemText(dlg, IDC_FILETYPE_EXT, edit.exts);
        EnableWindow(GetDlgItem(dlg, IDC_FILETYPE_EXTL), editNew);
        EnableWindow(GetDlgItem(dlg, IDC_FILETYPE_EXT), editNew);
        CheckDlgButton(dlg, IDC_FILETYPE_SHOWEXT, edit.showExt);
        FillActions(dlg, 0);
        ShowIcon(dlg);
        SkinDialog(dlg);
        WindowCenter(dlg);
        return TRUE;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = ROW_H - 2;
        return TRUE;
    case WM_DRAWITEM:                   /* the default action bold */
        d = (const DRAWITEMSTRUCT FAR *)lp;
        if (d->itemID != (UINT)-1 && (int)d->itemID < edit.nActions)
            DrawRow(d, NULL, !lstrcmpi(edit.actions[d->itemID].name, edit.defAction));
        return TRUE;
    case WM_COMMAND:
        i = (int)SendDlgItemMessage(dlg, IDC_FILETYPE_ACTIONS, LB_GETCURSEL, 0, 0L);
        switch (wp) {
        case IDC_FILETYPE_CHANGEICON:
            index = edit.iconIndex;
            lstrcpy(file, edit.icon);
            if (!file[0]) {             /* the program's, to start from */
                for (i = 0; i < edit.nActions && lstrcmpi(edit.actions[i].name, edit.defAction); i++)
                    ;
                if (i < edit.nActions && FileTypeProgram(edit.actions[i].cmd, program))
                    lstrcpy(file, program);
            }
            if (PropertiesPickIcon(dlg, file, &index)) {
                lstrcpy(edit.icon, file);
                edit.iconIndex = index;
                ShowIcon(dlg);
            }
            return TRUE;
        case IDC_FILETYPE_NEW:
        case IDC_FILETYPE_EDIT:
            if (wp == IDC_FILETYPE_NEW && edit.nActions == FT_ACTIONS)
                return TRUE;
            if (wp == IDC_FILETYPE_EDIT && i < 0)
                return TRUE;
            editAction = wp == IDC_FILETYPE_NEW ? -1 : i;
            if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_FILEACTION), dlg, (DLGPROC)FileTypesActionProc) == IDOK) {
                FillActions(dlg, editAction);
                ShowIcon(dlg);
            }
            return TRUE;
        case IDC_FILETYPE_ACTIONS:
            if (HIWORD(lp) == LBN_DBLCLK)
                SendMessage(dlg, WM_COMMAND, IDC_FILETYPE_EDIT, 0L);
            return TRUE;
        case IDC_FILETYPE_REMOVE:
            if (i < 0)
                return TRUE;
            if (!lstrcmpi(edit.actions[i].name, edit.defAction))
                lstrcpy(edit.defAction, "open");
            _fmemmove(&edit.actions[i], &edit.actions[i + 1], (edit.nActions - i - 1) * sizeof edit.actions[0]);
            edit.nActions--;
            FillActions(dlg, i);
            ShowIcon(dlg);
            return TRUE;
        case IDC_FILETYPE_DEFAULT:
            if (i >= 0) {
                lstrcpy(edit.defAction, edit.actions[i].name);
                InvalidateRect(GetDlgItem(dlg, IDC_FILETYPE_ACTIONS), NULL, TRUE);
                ShowIcon(dlg);
            }
            return TRUE;
        case IDOK:
            if (editNew && !ReadNewExt(dlg))
                return TRUE;
            GetDlgItemText(dlg, IDC_FILETYPE_DESC, edit.desc, sizeof edit.desc);
            if (!edit.desc[0])
                wsprintf(edit.desc, "%s File", (LPSTR)edit.exts);
            edit.showExt = IsDlgButtonChecked(dlg, IDC_FILETYPE_SHOWEXT);
            FileTypeSave(&edit);
            EndDialog(dlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        SendDlgItemMessage(dlg, IDC_FILETYPE_ICON, STM_SETICON, (WPARAM)IconStock(IDI_FILE), 0L);
        break;
    }
    return FALSE;
}

/* Edit (or New Type, i < 0); the list and every window show the change. */
static void EditType(HWND page, int i)
{
    char ext[8];

    editNew = i < 0;
    if (editNew) {
        _fmemset(&edit, 0, sizeof edit);
        lstrcpy(edit.defAction, "open");
    } else {
        FileTypeLoad(&ids[i], &edit);
    }
    if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_FILETYPE), page, (DLGPROC)FileTypesEditProc) != IDOK)
        return;
    FileTypeFirstExt(edit.exts, ext);
    Fill(page, ext, 0);
    Details(page);
    FileCommandsFolderChanged(NULL);
}

static void RemoveType(HWND page, int i)
{
    int row = (int)SendDlgItemMessage(page, IDC_FILETYPES_LIST, LB_GETCURSEL, 0, 0L);

    if (i < 0 || MessageBox(page, "Removing a file type will make it impossible to open files of that type "
                                  "by double-clicking their icons.\n\nAre you sure you want to remove this file type?",
                            "File Types", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    FileTypeLoad(&ids[i], &edit);
    FileTypeRemove(&edit);
    Fill(page, NULL, row);
    Details(page);
    FileCommandsFolderChanged(NULL);
}

BOOL FileTypesPageCommand(HWND page, WPARAM wp, LPARAM lp)
{
    switch (wp) {
    case IDC_FILETYPES_LIST:
        if (HIWORD(lp) == LBN_SELCHANGE)
            Details(page);
        else if (HIWORD(lp) == LBN_DBLCLK)
            EditType(page, Selected(page));
        return TRUE;
    case IDC_FILETYPES_NEW:
        EditType(page, -1);
        return TRUE;
    case IDC_FILETYPES_EDIT:
        if (Selected(page) >= 0)
            EditType(page, Selected(page));
        return TRUE;
    case IDC_FILETYPES_REMOVE:
        RemoveType(page, Selected(page));
        return TRUE;
    }
    return FALSE;
}
