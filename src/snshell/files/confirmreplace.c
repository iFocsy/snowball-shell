/* Confirm File Replace, as Windows 98 asks it. */
#include "internal.h"

typedef struct {
    LPCSTR target;
    const DOSENTRY *existing, *with;
} REPLACE;

static void Describe(HWND w, int id, const DOSENTRY *e)
{
    char size[24], date[32], text[80];
    FormatSize(e->size, size);
    FormatDateTime(e->date, e->time, date);
    wsprintf(text, "%s\nmodified on %s", (LPSTR)size, (LPSTR)date);
    SetDlgItemText(w, id, text);
}

BOOL CALLBACK __export ConfirmReplaceProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    REPLACE FAR *r;
    char text[MAX_PATH + 60];
    HICON icon;

    switch (msg) {
    case WM_INITDIALOG:
        r = (REPLACE FAR *)lp;
        SkinDialog(w);
        SendDlgItemMessage(w, IDC_DLGICON, STM_SETICON, (WPARAM)IconStock(IDI_FOLDER), 0L);
        wsprintf(text, "This folder already contains a file named '%s'.", (LPSTR)PathFileName((LPSTR)r->target));
        SetDlgItemText(w, IDC_TEXT, text);
        Describe(w, IDC_OLDINFO, r->existing);
        Describe(w, IDC_NEWINFO, r->with);
        icon = IconForFile(r->target, FALSE);
        SendDlgItemMessage(w, IDC_OLDICON, STM_SETICON, (WPARAM)icon, 0L);
        SendDlgItemMessage(w, IDC_NEWICON, STM_SETICON, (WPARAM)icon, 0L);
        WindowCenter(w);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDYES || wp == IDNO || wp == IDCANCEL || wp == IDC_YESALL)
            EndDialog(w, wp);
        return TRUE;
    }
    return FALSE;
}

int ConfirmReplaceAsk(HWND owner, LPCSTR target, const DOSENTRY *existing, const DOSENTRY *with)
{
    REPLACE r;
    HWND progress = ProgressWindow();

    r.target = target;
    r.existing = existing;
    r.with = with;
    return DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_REPLACE), progress ? progress : owner,
                          ConfirmReplaceProc, (LPARAM)(REPLACE FAR *)&r);
}
