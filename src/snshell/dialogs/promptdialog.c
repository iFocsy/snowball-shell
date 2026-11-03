/* A name asked for in a small dialog: Rename (items that cannot be renamed
   in place, the Start menu's), Save Scheme. */
#include "internal.h"

static LPCSTR promptTitle, promptLabel;
static LPSTR promptText;
static int promptSize;

BOOL CALLBACK __export PromptDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowText(dlg, promptTitle);
        SetDlgItemText(dlg, IDC_PROMPT_LABEL, promptLabel);
        SkinDialog(dlg);
        WindowCenter(dlg);
        SendDlgItemMessage(dlg, IDC_PROMPT_TEXT, EM_LIMITTEXT, promptSize - 1, 0L);
        SetDlgItemText(dlg, IDC_PROMPT_TEXT, promptText);
        SendDlgItemMessage(dlg, IDC_PROMPT_TEXT, EM_SETSEL, 0, MAKELPARAM(0, -1));
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK)
            GetDlgItemText(dlg, IDC_PROMPT_TEXT, promptText, promptSize);
        if (wp == IDOK || wp == IDCANCEL) {
            EndDialog(dlg, wp);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL PromptDialogShow(HWND owner, LPCSTR title, LPCSTR label, LPSTR text, int size)
{
    promptTitle = title;
    promptLabel = label;
    promptText = text;
    promptSize = size;
    return DialogBox(g_hInst, MAKEINTRESOURCE(IDD_PROMPT), owner, (DLGPROC)PromptDialogProc) == IDOK;
}
