/* Go To Folder. */
#include "internal.h"

static char goPath[MAX_PATH];

BOOL CALLBACK __export GoToDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char text[MAX_PATH + 80];

    switch (msg) {
    case WM_INITDIALOG:
        SkinDialog(dlg);
        WindowCenter(dlg);
        SendDlgItemMessage(dlg, IDC_GOTO_PATH, EM_LIMITTEXT, MAX_PATH - 1, 0L);
        AutoCompleteAttach(GetDlgItem(dlg, IDC_GOTO_PATH));
        SetDlgItemText(dlg, IDC_GOTO_PATH, goPath);
        SendDlgItemMessage(dlg, IDC_GOTO_PATH, EM_SETSEL, 0, MAKELPARAM(0, -1));
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK) {
            GetDlgItemText(dlg, IDC_GOTO_PATH, goPath, sizeof goPath);
            if (lstrlen(goPath) == 2 && goPath[1] == ':')
                lstrcat(goPath, "\\");                /* "C:" means its root */
            else if (lstrlen(goPath) > 3 && goPath[lstrlen(goPath) - 1] == '\\')
                goPath[lstrlen(goPath) - 1] = '\0';
            if (PathIsFolder(goPath)) {
                EndDialog(dlg, IDOK);
            } else {
                wsprintf(text, "Cannot find the folder '%s'.\n\nMake sure the path is correct.", (LPSTR)goPath);
                MessageBox(dlg, text, "Go To Folder", MB_ICONSTOP | MB_OK);
            }
            return TRUE;
        }
        if (wp == IDCANCEL) {
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* path: the folder shown, then the one chosen. */
BOOL GoToDialogShow(HWND owner, LPSTR path)
{
    lstrcpy(goPath, path);
    if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_GOTO), owner, (DLGPROC)GoToDialogProc) != IDOK)
        return FALSE;
    lstrcpy(path, goPath);
    return TRUE;
}
