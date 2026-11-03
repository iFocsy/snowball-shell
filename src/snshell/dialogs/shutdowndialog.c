/* Shut Down Windows, over a dimmed screen. */
#include "internal.h"

/* ---- Shut Down ---- */

/* Windows 98 style: darken the screen with a 50% black dither. */
static void DimScreen(void)
{
    static WORD pat[8] = { 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA };
    HBITMAP b = CreateBitmap(8, 8, 1, 1, pat);
    HBRUSH br = CreatePatternBrush(b), old;
    HDC dc = GetDC(NULL);

    old = SelectObject(dc, br);
    SetTextColor(dc, RGB(0, 0, 0));
    SetBkColor(dc, RGB(255, 255, 255));
    PatBlt(dc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 0x00A000C9L);  /* DPa */
    SelectObject(dc, old);
    ReleaseDC(NULL, dc);
    DeleteObject(br);
    DeleteObject(b);
}

BOOL CALLBACK __export ShutDownDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        SkinDialog(dlg);
        SendDlgItemMessage(dlg, IDC_DLGICON, STM_SETICON, (WPARAM)IconStock(IDI_SHUTDOWN), 0L);
        WindowCenter(dlg);
        CheckRadioButton(dlg, IDC_SHUTDOWN, IDC_TODOS, IDC_SHUTDOWN);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK) {
            EndDialog(dlg, IsDlgButtonChecked(dlg, IDC_RESTART) ? IDC_RESTART :
                           IsDlgButtonChecked(dlg, IDC_TODOS) ? IDC_TODOS : IDC_SHUTDOWN);
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

void ShutDownDialogShow(HWND owner)
{
    char flag[MAX_PATH];
    int choice;

    DimScreen();
    choice = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SHUTDOWN), owner, (DLGPROC)ShutDownDialogProc);
    InvalidateRect(NULL, NULL, TRUE);           /* undo the dimming */
    if (choice == IDCANCEL || choice == -1)
        return;
    /* "Shut down" leaves a flag for DOSBox-X's autoexec to close the emulator */
    lstrcpy(flag, g_home);
    lstrcat(flag, "POWEROFF.FLG");
    if (choice == IDC_SHUTDOWN)
        DosCreateTextFile(flag, "");
    if (!ExitWindows(choice == IDC_RESTART ? EW_RESTARTWINDOWS : 0, 0))
        DosDelete(flag);                         /* a program refused to close */
}
