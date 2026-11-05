/*
 * Customize Start Menu (Taskbar Properties' Customize..., for the
 * two-column Start menu), as Windows XP's: how many of the programs used
 * most it lists, which makes it taller or shorter, and how wide it is
 * (SNOWBALL.INI [Taskbar] MostUsed, MenuWidth), both seen at its next
 * opening. Clear List forgets the programs counted at once.
 */
#include "internal.h"

BOOL CALLBACK __export StartMenuSizeProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND width = GetDlgItem(dlg, IDC_SMC_WIDTH);
    char n[4];
    int i;

    switch (msg) {
    case WM_INITDIALOG:
        for (i = 0; i <= FREQUENT_MAX; i++) {
            wsprintf(n, "%d", i);
            SendDlgItemMessage(dlg, IDC_SMC_COUNT, CB_ADDSTRING, 0, (LPARAM)(LPSTR)n);
        }
        SendDlgItemMessage(dlg, IDC_SMC_COUNT, CB_SETCURSEL, StartMenuColumnsPrograms(), 0L);
        SliderSetCount(width, (PANE_W_MAX - PANE_W_MIN) / PANE_W_STEP + 1);
        SliderSetPos(width, (StartMenuColumnsPaneWidth() - PANE_W_MIN + PANE_W_STEP / 2) / PANE_W_STEP);
        SendDlgItemMessage(dlg, IDC_SMC_ICON, STM_SETICON, (WPARAM)IconStock(IDI_SM_PROGRAMS), 0L);
        EnableWindow(GetDlgItem(dlg, IDC_SMC_CLEAR), FrequentAnyUsed());
        SkinDialog(dlg);
        WindowCenter(dlg);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDC_SMC_CLEAR) {
            FrequentClear();
            SetFocus(GetDlgItem(dlg, IDOK));
            EnableWindow(GetDlgItem(dlg, IDC_SMC_CLEAR), FALSE);
        } else if (wp == IDOK) {
            SettingsSetInt("Taskbar", "MostUsed", (int)SendDlgItemMessage(dlg, IDC_SMC_COUNT, CB_GETCURSEL, 0, 0L));
            SettingsSetInt("Taskbar", "MenuWidth", PANE_W_MIN + SliderPos(width) * PANE_W_STEP);
            EndDialog(dlg, TRUE);
        } else if (wp == IDCANCEL) {
            EndDialog(dlg, FALSE);
        }
        return TRUE;
    }
    return FALSE;
}

void StartMenuSizeShow(HWND owner)
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_STARTSIZE), owner, StartMenuSizeProc);
}
