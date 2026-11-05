/* About Snowball: Windows 98's look, a banner over the text. */
#include "internal.h"
#include "commit.h"                     /* SB_COMMIT, written by build.ps1 */

#define LINK    "github.com/iFocsy/snowball-shell"

/* The banner: the gradient of an active caption, the big icon, the name
   large and bold, the version; and an etched line over the memory. */
static void Paint(HWND dlg, HDC dc)
{
    static HFONT big;
    RECT r, t;
    HFONT old;

    if (!big)
        big = CreateFont(-24, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, 0, VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
    WindowRectIn(GetDlgItem(dlg, IDC_ABOUT_BANNER), dlg, &r);
    DrawGradient(dc, &r, RGB(0, 0, 128), RGB(16, 132, 208), FALSE);
    DrawIcon(dc, r.left + 12, r.top + (r.bottom - r.top - 32) / 2, LoadIcon(g_hArt, MAKEINTRESOURCE(IDI_SNOWBALL)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    old = SelectObject(dc, big);
    t = r;
    t.left += 54;
    DrawText(dc, SB_NAME, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, DrawFont(FALSE));
    t.right -= 10;
    t.bottom -= 4;
    DrawText(dc, SB_VERSION, -1, &t, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    WindowRectIn(GetDlgItem(dlg, IDC_ABOUT_LINK), dlg, &t);     /* the link, blue and underlined */
    SetTextColor(dc, RGB(0, 0, 255));
    DrawText(dc, LINK, -1, &t, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    { TEXTMETRIC tm; GetTextMetrics(dc, &tm); t.top += tm.tmAscent + 1; }
    t.bottom = t.top + 1;
    t.right = t.left + LOWORD(GetTextExtent(dc, LINK, sizeof LINK - 1));
    DrawSolidRect(dc, &t, RGB(0, 0, 255));
    SelectObject(dc, old);
    GetWindowRect(GetDlgItem(dlg, IDC_INFO), &t);
    ScreenToClient(dlg, (POINT FAR *)&t.left);
    SetRect(&t, 12, t.top - 6, r.right - 12, t.top - 5);
    DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNSHADOW));
    OffsetRect(&t, 0, 1);
    DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNHIGHLIGHT));
}

BOOL CALLBACK __export AboutDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char text[160];
    PAINTSTRUCT ps;

    switch (msg) {
    case WM_INITDIALOG:
        ShowWindow(GetDlgItem(dlg, IDC_ABOUT_BANNER), SW_HIDE);    /* their places are painted */
        ShowWindow(GetDlgItem(dlg, IDC_ABOUT_LINK), SW_HIDE);
        SkinDialog(dlg);
        WindowCenter(dlg);
        wsprintf(text, "Version %s (16-bit, for Windows 3.1 and 3.11)\nCommit %s", (LPSTR)SB_VERSION, (LPSTR)SB_COMMIT);
        SetDlgItemText(dlg, IDC_VERSION, text);
        wsprintf(text, "Memory: %lu KB free\nSystem resources: %u%% free",
                 GetFreeSpace(0) / 1024, GetFreeSystemResources(GFSR_SYSTEMRESOURCES));
        SetDlgItemText(dlg, IDC_INFO, text);
        return TRUE;
    case WM_PAINT:
        BeginPaint(dlg, &ps);
        Paint(dlg, ps.hdc);
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK || wp == IDCANCEL) {
            EndDialog(dlg, wp);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void AboutDialogShow(HWND owner)
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), owner, (DLGPROC)AboutDialogProc);
}
