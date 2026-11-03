/*
 * Recycle Bin Properties, Windows 98's Global page: deleting for good
 * (NoMove), the bins' size (MaxPercent of each drive) and the delete
 * confirmation, kept in SNOWBALL.INI [Recycle Bin].
 */
#include "internal.h"

BOOL CALLBACK __export RecycleBinPropertiesProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND slider = GetDlgItem(dlg, IDC_BIN_SLIDER);
    char text[8];

    switch (msg) {
    case WM_INITDIALOG:
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Global");
        SliderSetCount(slider, 101);
        SliderSetPos(slider, SettingsGetInt("Recycle Bin", "MaxPercent", 10));
        CheckDlgButton(dlg, IDC_BIN_NOMOVE, SettingsGetInt("Recycle Bin", "NoMove", 0));
        CheckDlgButton(dlg, IDC_BIN_CONFIRM, SettingsGetInt("Recycle Bin", "Confirm", 1));
        SendMessage(dlg, WM_COMMAND, IDC_BIN_SLIDER, MAKELPARAM(slider, SLN_CHANGE));
        EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
        SkinDialog(dlg);
        WindowCenter(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDC_BIN_SLIDER:
            wsprintf(text, "%d%%", SliderPos(slider));
            SetDlgItemText(dlg, IDC_BIN_PERCENT, text);
            /* fall through */
        case IDC_BIN_NOMOVE:
        case IDC_BIN_CONFIRM:
            EnableWindow(slider, !IsDlgButtonChecked(dlg, IDC_BIN_NOMOVE));
            EnableWindow(GetDlgItem(dlg, IDC_BIN_PERCENT), !IsDlgButtonChecked(dlg, IDC_BIN_NOMOVE));
            EnableWindow(GetDlgItem(dlg, IDC_BIN_SIZELABEL), !IsDlgButtonChecked(dlg, IDC_BIN_NOMOVE));
            EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
            return TRUE;
        case IDOK:
        case IDC_APPLY:
            SettingsSetInt("Recycle Bin", "NoMove", IsDlgButtonChecked(dlg, IDC_BIN_NOMOVE));
            SettingsSetInt("Recycle Bin", "MaxPercent", SliderPos(slider));
            SettingsSetInt("Recycle Bin", "Confirm", IsDlgButtonChecked(dlg, IDC_BIN_CONFIRM));
            EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
            RecycleBinTrim(dlg);
            if (wp == IDOK)
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

void RecycleBinPropertiesShow(HWND owner)
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_BINPROPS), owner, (DLGPROC)RecycleBinPropertiesProc);
}
