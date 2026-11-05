/* Change Icon: the icons of a file, one picked (shortcuts, folders, file types). */
#include "internal.h"

static struct {
    char file[MAX_PATH];
    int  index;
} pick;

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

BOOL CALLBACK __export ChangeIconDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
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

BOOL ChangeIconDialogShow(HWND owner, LPSTR file, int *index)
{
    lstrcpy(pick.file, file);
    pick.index = *index;
    if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CHANGEICON), owner, ChangeIconDialogProc) != 1)
        return FALSE;
    lstrcpy(file, pick.file);
    *index = pick.index;
    return TRUE;
}

