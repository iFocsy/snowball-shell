/*
 * Open With, as in Windows 98: the programs the file types use (and those
 * added with Other..., kept in SNOWBALL.INI [Open With]), each with its
 * icon; the file opens with the one chosen, and "Always use this program"
 * makes it the type's (a type made for a new extension).
 */
#include "internal.h"

#define MAX_PROGRAMS    48
#define MAX_OTHERS      16
#define ROW_H           18

static char programs[MAX_PROGRAMS][MAX_PATH];
static int nPrograms;
static LPCSTR openFile;
static char ext[8], typeDesc[64];
static BOOL opened;

static void AddProgram(LPCSTR path)
{
    int i;

    for (i = 0; i < nPrograms; i++)
        if (!lstrcmpi(programs[i], path))
            return;
    if (nPrograms < MAX_PROGRAMS)
        lstrcpy(programs[nPrograms++], path);
}

static void AddCommand(LPCSTR cmd)
{
    char path[MAX_PATH];

    if (FileTypeProgram(cmd, path))
        AddProgram(path);
}

/* Every action of every type, the programs of WIN.INI's lines, the others. */
static void Collect(void)
{
    char key[64], action[16], cmd[MAX_PATH], keys[512], line[MAX_PATH];
    HKEY h;
    DWORD i, j;
    LPSTR k;
    int n;

    nPrograms = 0;
    for (i = 0; RegEnumKey(HKEY_CLASSES_ROOT, i, key, sizeof key) == ERROR_SUCCESS; i++) {
        if (key[0] == '.' || lstrlen(key) > sizeof key - 8)
            continue;
        lstrcat(key, "\\shell");
        if (RegOpenKey(HKEY_CLASSES_ROOT, key, &h) != ERROR_SUCCESS)
            continue;
        key[lstrlen(key) - 6] = '\0';
        for (j = 0; RegEnumKey(h, j, action, sizeof action) == ERROR_SUCCESS; j++)
            if (FileTypeCommand(key, action, cmd, sizeof cmd))
                AddCommand(cmd);
        RegCloseKey(h);
    }
    GetProfileString("Extensions", NULL, "", keys, sizeof keys);
    for (k = keys; *k; k += lstrlen(k) + 1)
        if (GetProfileString("Extensions", k, "", line, sizeof line))
            AddCommand(line);
    for (n = 1; n <= MAX_OTHERS; n++) {
        wsprintf(key, "%d", n);
        SettingsGetString("Open With", key, "", line, sizeof line);
        if (line[0] && DosGetAttributes(line) != 0xFFFF)
            AddProgram(line);
    }
}

/* "NOTEPAD", as Windows 98 names a program in the list. */
static void ProgramName(LPCSTR path, LPSTR out)
{
    lstrcpy(out, PathFileName((LPSTR)path));
    if (*PathExtension(out))
        *PathExtension(out) = '\0';
    AnsiUpper(out);
}

static int AddRow(HWND list, int i)
{
    char name[MAX_PATH];
    int row;

    ProgramName(programs[i], name);
    row = (int)SendMessage(list, LB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
    SendMessage(list, LB_SETITEMDATA, row, (LPARAM)i);
    return row;
}

/* A program picked with Other... is offered from then on. */
static void KeepOther(LPCSTR path)
{
    char key[4], line[MAX_PATH];
    int n, free = 0;

    for (n = 1; n <= MAX_OTHERS; n++) {
        wsprintf(key, "%d", n);
        SettingsGetString("Open With", key, "", line, sizeof line);
        if (!lstrcmpi(line, path))
            return;
        if (!line[0] && !free)
            free = n;
    }
    wsprintf(key, "%d", free ? free : MAX_OTHERS);
    WritePrivateProfileString("Open With", key, path, g_ini);
}

static void DrawRow(const DRAWITEMSTRUCT FAR *d)
{
    char name[MAX_PATH];

    if (d->itemID == (UINT)-1)
        return;
    SendMessage(d->hwndItem, LB_GETTEXT, d->itemID, (LPARAM)(LPSTR)name);
    PaintListItem(d, d->rcItem.left + 2, IconForFile(programs[(int)d->itemData], FALSE), name);
}

static void Choose(HWND dlg)
{
    HWND list = GetDlgItem(dlg, IDC_OPENWITH_LIST);
    int row = (int)SendMessage(list, LB_GETCURSEL, 0, 0L);
    char shortFile[MAX_PATH], desc[64];
    LPCSTR program;

    if (row < 0)
        return;
    program = programs[(int)SendMessage(list, LB_GETITEMDATA, row, 0L)];
    if (IsDlgButtonChecked(dlg, IDC_OPENWITH_ALWAYS)) {
        GetDlgItemText(dlg, IDC_OPENWITH_DESC, desc, sizeof desc);
        FileTypeAssociate(ext, program, lstrcmp(desc, typeDesc) ? desc : NULL);
        FileCommandsFolderChanged(NULL);        /* the type's icon and name, everywhere */
    }
    DosShortPath(openFile, shortFile);
    EndDialog(dlg, IDOK);
    if ((opened = LaunchWith(GetParent(dlg), program, shortFile)) != FALSE)
        RecentAdd(openFile);
}

BOOL CALLBACK __export OpenWithDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char text[MAX_PATH + 160], file[MAX_PATH], name[MAX_PATH], cls[40], cmd[MAX_PATH];
    HWND list;
    int i, row;

    switch (msg) {
    case WM_INITDIALOG:
        lstrcpy(file, openFile);
        DisplayNameOfFile(PathFileName(file), name);
        wsprintf(text, "Click the program you want to use to open the file '%s'.\n"
                       "If the program you want is not in the list, click Other.", (LPSTR)name);
        SetDlgItemText(dlg, IDC_OPENWITH_TEXT, text);
        if (ext[0]) {
            wsprintf(text, "Description of '.%s' files:", (LPSTR)ext);
            AnsiLower(text + 17);
            SetDlgItemText(dlg, IDC_OPENWITH_DESCL, text);
        }
        typeDesc[0] = '\0';
        if (ext[0] && FileTypeClass(ext, cls, sizeof cls)) {
            wsprintf(file, "x.%s", (LPSTR)ext);
            FormatFileType(file, typeDesc);
        }
        SendDlgItemMessage(dlg, IDC_OPENWITH_DESC, EM_LIMITTEXT, sizeof typeDesc - 1, 0L);
        SetDlgItemText(dlg, IDC_OPENWITH_DESC, typeDesc);
        EnableWindow(GetDlgItem(dlg, IDC_OPENWITH_DESCL), ext[0] != '\0');
        EnableWindow(GetDlgItem(dlg, IDC_OPENWITH_DESC), ext[0] != '\0');
        EnableWindow(GetDlgItem(dlg, IDC_OPENWITH_ALWAYS), ext[0] != '\0');
        CheckDlgButton(dlg, IDC_OPENWITH_ALWAYS, ext[0] && !FileTypeOpenCommand(ext, cmd, sizeof cmd));
        Collect();
        list = GetDlgItem(dlg, IDC_OPENWITH_LIST);
        for (i = 0; i < nPrograms; i++)
            AddRow(list, i);
        SendMessage(list, LB_SETCURSEL, 0, 0L);
        SkinDialog(dlg);
        WindowCenter(dlg);
        return TRUE;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = ROW_H;
        return TRUE;
    case WM_DRAWITEM:
        DrawRow((const DRAWITEMSTRUCT FAR *)lp);
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDC_OPENWITH_LIST:
            if (HIWORD(lp) == LBN_DBLCLK)
                Choose(dlg);
            return TRUE;
        case IDC_OPENWITH_OTHER:
            file[0] = '\0';
            if (BrowseDialogShow(dlg, "Open With", "Programs\0*.exe;*.com;*.pif;*.bat\0All Files\0*.*\0",
                                 file, FALSE) && nPrograms < MAX_PROGRAMS) {
                AddProgram(file);
                KeepOther(file);
                list = GetDlgItem(dlg, IDC_OPENWITH_LIST);
                for (i = 0; i < nPrograms && lstrcmpi(programs[i], file); i++)
                    ;
                for (row = (int)SendMessage(list, LB_GETCOUNT, 0, 0L) - 1;
                     row >= 0 && (int)SendMessage(list, LB_GETITEMDATA, row, 0L) != i; row--)
                    ;
                SendMessage(list, LB_SETCURSEL, row >= 0 ? row : AddRow(list, i), 0L);
                SetFocus(list);
            }
            return TRUE;
        case IDOK:
            Choose(dlg);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL OpenWithDialogShow(HWND owner, LPCSTR file)
{
    LPSTR dot = PathExtension((LPSTR)file);

    openFile = file;
    lstrcpyn(ext, *dot ? dot + 1 : "", sizeof ext);
    AnsiUpper(ext);
    opened = FALSE;
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_OPENWITH), owner, (DLGPROC)OpenWithDialogProc);
    return opened;
}
