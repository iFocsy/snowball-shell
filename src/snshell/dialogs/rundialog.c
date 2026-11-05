/* Run, with the commands typed before. */
#include "internal.h"

#define HISTORY 10

static void LoadHistory(HWND combo)
{
    char key[4], cmd[MAX_PATH];
    int i;

    for (i = 1; i <= HISTORY; i++) {
        wsprintf(key, "%d", i);
        SettingsGetString("RunHistory", key, "", cmd, sizeof cmd);
        if (*cmd)
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)(LPSTR)cmd);
    }
    if (SendMessage(combo, CB_GETCOUNT, 0, 0L))
        SendMessage(combo, CB_SETCURSEL, 0, 0L);
}

static void SaveHistory(LPCSTR newest, HWND combo)
{
    char key[4], cmd[MAX_PATH];
    int i, n = 1, count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0L);

    WritePrivateProfileString("RunHistory", "1", newest, g_ini);
    for (i = 0; i < count && n < HISTORY; i++) {
        SendMessage(combo, CB_GETLBTEXT, i, (LPARAM)(LPSTR)cmd);
        if (lstrcmpi(cmd, newest)) {
            wsprintf(key, "%d", ++n);
            WritePrivateProfileString("RunHistory", key, cmd, g_ini);
        }
    }
}

static void Browse(HWND dlg)
{
    char file[MAX_PATH] = "";

    if (BrowseDialogShow(dlg, "Browse", "Programs\0*.exe;*.com;*.bat;*.pif\0All Files\0*.*\0", file, FALSE))
        SetDlgItemText(dlg, IDC_COMMAND, file);
}

BOOL CALLBACK __export RunDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char cmd[MAX_PATH];
    switch (msg) {
    case WM_INITDIALOG:
        SkinDialog(dlg);
        SendDlgItemMessage(dlg, IDC_DLGICON, STM_SETICON, (WPARAM)IconStock(IDI_RUN), 0L);
        WindowCenter(dlg);
        LoadHistory(GetDlgItem(dlg, IDC_COMMAND));
        SendDlgItemMessage(dlg, IDC_COMMAND, CB_LIMITTEXT, MAX_PATH - 1, 0L);
        AutoCompleteAttach(GetWindow(GetDlgItem(dlg, IDC_COMMAND), GW_CHILD));
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDOK:
            GetDlgItemText(dlg, IDC_COMMAND, cmd, sizeof cmd);
            if (*cmd && LaunchCommand(dlg, cmd, NULL, SW_SHOWNORMAL)) {
                SaveHistory(cmd, GetDlgItem(dlg, IDC_COMMAND));
                EndDialog(dlg, IDOK);
            }
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_BROWSE:
            Browse(dlg);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void RunDialogShow(HWND owner)
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_RUN), owner, (DLGPROC)RunDialogProc);
}
