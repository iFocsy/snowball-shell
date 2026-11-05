/* Format, Windows 98's dialog for a floppy disk: the options make the
   command line of DOS's FORMAT (or SYS, for system files only), which runs
   in an MS-DOS window (dosprompt.c) and asks there for the disk. */
#include "internal.h"

static const struct { LPCSTR name, size; } capacities[] = {
    { "1.44 Mb (3.5\")", "1440" }, { "720 Kb (3.5\")", "720" }, { "2.88 Mb (3.5\")", "2880" },
    { "1.2 Mb (5.25\")", "1200" }, { "360 Kb (5.25\")", "360" },
};
static int drive;

/* "FORMAT A: /F:1440 /Q /V:DISK /S", from what is chosen. */
static void Command(HWND dlg, LPSTR out)
{
    char label[12];
    int c = (int)SendDlgItemMessage(dlg, IDC_FORMAT_CAPACITY, CB_GETCURSEL, 0, 0L);

    if (IsDlgButtonChecked(dlg, IDC_FORMAT_SYSONLY)) {
        wsprintf(out, "SYS %c:", 'A' + drive);
        return;
    }
    wsprintf(out, "FORMAT %c: /F:%s", 'A' + drive, (LPSTR)capacities[max(0, c)].size);
    if (IsDlgButtonChecked(dlg, IDC_FORMAT_QUICK))
        lstrcat(out, " /Q");
    GetDlgItemText(dlg, IDC_FORMAT_LABEL, label, sizeof label);
    if (!IsDlgButtonChecked(dlg, IDC_FORMAT_NOLABEL) && *label)
        wsprintf(out + lstrlen(out), " /V:%s", (LPSTR)label);
    if (IsDlgButtonChecked(dlg, IDC_FORMAT_SYS))
        lstrcat(out, " /S");
}

BOOL CALLBACK __export FormatDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char text[64];
    int i;

    switch (msg) {
    case WM_INITDIALOG:
        SkinDialog(dlg);
        wsprintf(text, "Format - %c:", 'A' + drive);
        SetWindowText(dlg, text);
        for (i = 0; i < sizeof capacities / sizeof capacities[0]; i++)
            SendDlgItemMessage(dlg, IDC_FORMAT_CAPACITY, CB_ADDSTRING, 0, (LPARAM)capacities[i].name);
        SendDlgItemMessage(dlg, IDC_FORMAT_CAPACITY, CB_SETCURSEL, 0, 0L);
        CheckRadioButton(dlg, IDC_FORMAT_QUICK, IDC_FORMAT_SYSONLY, IDC_FORMAT_QUICK);
        SendDlgItemMessage(dlg, IDC_FORMAT_LABEL, EM_LIMITTEXT, 11, 0L);
        WindowCenter(dlg);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK) {
            Command(dlg, text);
            EndDialog(dlg, LaunchDosPrompt(dlg, NULL, text, "Format"));
        } else if (wp == IDCANCEL) {
            EndDialog(dlg, FALSE);
        } else if (wp == IDC_FORMAT_NOLABEL || wp == IDC_FORMAT_SYSONLY || wp == IDC_FORMAT_QUICK ||
                   wp == IDC_FORMAT_FULL) {
            BOOL format = !IsDlgButtonChecked(dlg, IDC_FORMAT_SYSONLY);
            EnableWindow(GetDlgItem(dlg, IDC_FORMAT_LABEL), format && !IsDlgButtonChecked(dlg, IDC_FORMAT_NOLABEL));
            EnableWindow(GetDlgItem(dlg, IDC_FORMAT_NOLABEL), format);
            EnableWindow(GetDlgItem(dlg, IDC_FORMAT_SYS), format);
            EnableWindow(GetDlgItem(dlg, IDC_FORMAT_CAPACITY), format);
        }
        return TRUE;
    }
    return FALSE;
}

/* The disk in drive d (0 = A:) formatted. */
void FormatDialogShow(HWND owner, int d)
{
    drive = d;
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_FORMAT), owner, FormatDialogProc);
}

void FormatDialogCopyDisk(HWND owner, int d)
{
    char cmd[24];

    wsprintf(cmd, "DISKCOPY %c: %c:", 'A' + d, 'A' + d);
    LaunchDosPrompt(owner, NULL, cmd, "Copy Disk");
}
