/* The pages of Properties that read the system: a program's Version, a
   drive's General with its pie, My Computer's System. properties.c holds
   the sheet and the pages of files. */
#include "internal.h"
#include "commit.h"                     /* SB_COMMIT, written by build.ps1 */
#include <ver.h>

#define PIE_DEPTH   10

/* ---- Version: a program's or library's version resource ---- */

static const char *VerKeys[][2] = {     /* resource name, as Windows 98 lists it */
    { "Comments", "Comments" }, { "CompanyName", "Company Name" }, { "InternalName", "Internal Name" },
    { "", "Language" }, { "LegalTrademarks", "Legal Trademarks" }, { "OriginalFilename", "Original File name" },
    { "PrivateBuild", "Private Build" }, { "ProductName", "Product Name" },
    { "ProductVersion", "Product Version" }, { "SpecialBuild", "Special Build" },
};

/* A string of the version resource, "" if it has none. */
static void VerString(void FAR *data, DWORD lang, LPCSTR name, LPSTR out, int size)
{
    char key[64];
    LPSTR value;
    UINT n;

    wsprintf(key, "\\StringFileInfo\\%04X%04X\\%s", LOWORD(lang), HIWORD(lang), name);
    if (VerQueryValue(data, key, (void FAR * FAR *)&value, &n) && n)
        lstrcpyn(out, value, size);
    else
        *out = '\0';
}

/* Fills the Version page and ver, its list's values; FALSE when the file
   has no version resource. */
BOOL PropertyPagesVersion(HWND dlg, LPCSTR path, char (*ver)[VER_TEXT])
{
    char file[MAX_PATH], text[128];
    DWORD handle, size, lang = MAKELONG(0x0409, 0x04E4), FAR *trans;
    VS_FIXEDFILEINFO FAR *fixed;
    HGLOBAL h;
    void FAR *data;
    UINT n;
    int i, row;

    DosShortPath(path, file);
    if ((size = GetFileVersionInfoSize(file, &handle)) == 0 || (h = GlobalAlloc(GMEM_MOVEABLE, size)) == NULL)
        return FALSE;
    data = GlobalLock(h);
    if (!GetFileVersionInfo(file, handle, size, data)) {
        GlobalUnlock(h);
        GlobalFree(h);
        return FALSE;
    }
    if (VerQueryValue(data, "\\VarFileInfo\\Translation", (void FAR * FAR *)&trans, &n) && n >= 4)
        lang = *trans;
    if (VerQueryValue(data, "\\", (void FAR * FAR *)&fixed, &n) && n) {
        wsprintf(text, "%u.%u.%u.%u", HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS),
                 HIWORD(fixed->dwFileVersionLS), LOWORD(fixed->dwFileVersionLS));
        SetDlgItemText(dlg, IDC_VERSION_FILE, text);
    }
    VerString(data, lang, "FileDescription", text, sizeof text);
    SetDlgItemText(dlg, IDC_VERSION_DESC, text);
    VerString(data, lang, "LegalCopyright", text, sizeof text);
    SetDlgItemText(dlg, IDC_VERSION_COPY, text);
    for (i = 0; i < VER_ITEMS; i++) {
        if (*VerKeys[i][0])
            VerString(data, lang, VerKeys[i][0], ver[i], VER_TEXT);
        else
            VerLanguageName(LOWORD(lang), ver[i], VER_TEXT);
        if (ver[i][0]) {
            row = (int)SendDlgItemMessage(dlg, IDC_VERSION_ITEMS, LB_ADDSTRING, 0, (LPARAM)(LPSTR)VerKeys[i][1]);
            SendDlgItemMessage(dlg, IDC_VERSION_ITEMS, LB_SETITEMDATA, row, i);
        }
    }
    SendDlgItemMessage(dlg, IDC_VERSION_ITEMS, LB_SETCURSEL, 0, 0L);
    SendMessage(dlg, WM_COMMAND, IDC_VERSION_ITEMS, MAKELPARAM(GetDlgItem(dlg, IDC_VERSION_ITEMS), LBN_SELCHANGE));
    GlobalUnlock(h);
    GlobalFree(h);
    return TRUE;
}

/* ---- drives ---- */

/* sin of a degrees, in thousandths (every 10 degrees, then straight lines). */
static int Sin(int a)
{
    static const int t[] = { 0, 174, 342, 500, 643, 766, 866, 940, 985, 1000 };
    int sign = 1, i;

    a %= 360;
    if (a >= 180) {
        a -= 180;
        sign = -1;
    }
    if (a > 90)
        a = 180 - a;
    i = a / 10;
    return sign * (i < 9 ? t[i] + (t[i + 1] - t[i]) * (a % 10) / 10 : 1000);
}

static void Slice(HDC dc, RECT *r, int degrees, COLORREF used, COLORREF free)
{
    HBRUSH b = CreateSolidBrush(free), old = SelectObject(dc, b);
    int cx = (r->left + r->right) / 2, cy = (r->top + r->bottom) / 2;

    Ellipse(dc, r->left, r->top, r->right, r->bottom);
    SelectObject(dc, old);
    DeleteObject(b);
    if (degrees <= 0)
        return;
    b = CreateSolidBrush(used);
    old = SelectObject(dc, b);
    if (degrees >= 360)
        Ellipse(dc, r->left, r->top, r->right, r->bottom);
    else                                /* from 3 o'clock, counterclockwise */
        Pie(dc, r->left, r->top, r->right, r->bottom, r->right, cy,
            cx + Sin(degrees + 90) / 10, cy - Sin(degrees) / 10);
    SelectObject(dc, old);
    DeleteObject(b);
}

/* The Windows 98 disk pie: blue used, magenta free, standing 10 pixels high. */
void PropertyPagesPie(HWND dlg, HDC dc, DWORD total, DWORD free)
{
    RECT r, box;
    int degrees = total ? (int)((total - free) / (total / 360 + 1)) : 0, d;
    HPEN pen = SelectObject(dc, GetStockObject(NULL_PEN));

    WindowRectIn(GetDlgItem(dlg, IDC_DRIVE_PIE), dlg, &r);
    r.bottom -= PIE_DEPTH;
    for (d = PIE_DEPTH; d > 0; d--) {           /* the side, bottom up */
        OffsetRect(&r, 0, d);
        Slice(dc, &r, degrees, RGB(0, 0, 128), RGB(128, 0, 128));
        OffsetRect(&r, 0, -d);
    }
    SelectObject(dc, GetStockObject(BLACK_PEN));
    Slice(dc, &r, degrees, RGB(0, 0, 255), RGB(255, 0, 255));
    SelectObject(dc, pen);
    WindowRectIn(GetDlgItem(dlg, IDC_DRIVE_USEDBOX), dlg, &box);
    DrawSolidRect(dc, &box, RGB(0, 0, 255));
    DrawFrame(dc, &box, RGB(0, 0, 0), RGB(0, 0, 0));
    WindowRectIn(GetDlgItem(dlg, IDC_DRIVE_FREEBOX), dlg, &box);
    DrawSolidRect(dc, &box, RGB(255, 0, 255));
    DrawFrame(dc, &box, RGB(0, 0, 0), RGB(0, 0, 0));
}

static void SpaceLine(HWND dlg, int id, DWORD bytes)
{
    char text[32];
    FormatNumber(bytes, text);
    lstrcat(text, " bytes");
    SetDlgItemText(dlg, id, text);
    FormatSize(bytes, text);
    SetDlgItemText(dlg, id + 1, text);
}

/* A drive's General page; FALSE when it has no disk (total and free unread). */
BOOL PropertyPagesDrive(HWND dlg, int drive, DWORD *total, DWORD *free)
{
    char text[40];
    BOOL space;

    SendDlgItemMessage(dlg, IDC_PROPS_ICON, STM_SETICON, (WPARAM)IconForDrive(drive), 0L);
    DosVolumeLabel(drive, text);
    SendDlgItemMessage(dlg, IDC_DRIVE_LABEL, EM_LIMITTEXT, 11, 0L);
    SetDlgItemText(dlg, IDC_DRIVE_LABEL, text);
    if ((space = DosDiskSpace(drive, total, free)) == FALSE)    /* no disk in it: nothing to label */
        SendDlgItemMessage(dlg, IDC_DRIVE_LABEL, EM_SETREADONLY, TRUE, 0L);
    DriveTypeName(drive, text);
    SetDlgItemText(dlg, IDC_PROPS_TYPE, text);
    SetDlgItemText(dlg, IDC_DRIVE_FS, "FAT");
    SpaceLine(dlg, IDC_DRIVE_USED, *total - *free);
    SpaceLine(dlg, IDC_DRIVE_FREE, *free);
    SpaceLine(dlg, IDC_DRIVE_CAP, *total);
    wsprintf(text, "Drive %c", 'A' + drive);
    SetDlgItemText(dlg, IDC_DRIVE_DRIVE, text);
    return space;
}

/* ---- My Computer ---- */

void PropertyPagesSystem(HWND dlg)
{
    char text[64];
    DWORD flags = GetWinFlags(), version = GetVersion();

    SendDlgItemMessage(dlg, IDC_PROPS_ICON, STM_SETICON, (WPARAM)IconStock(IDI_COMPUTER), 0L);
    SetDlgItemText(dlg, IDC_COMPUTER_SYSTEM, GetModuleHandle("WFWNET") ? "Microsoft Windows for Workgroups"
                                                               : "Microsoft Windows");
    wsprintf(text, "Version %d.%02d", LOBYTE(LOWORD(version)), HIBYTE(LOWORD(version)));
    SetDlgItemText(dlg, IDC_COMPUTER_VERSION, text);
    SetDlgItemText(dlg, IDC_COMPUTER_SHELL, SB_NAME " " SB_VERSION "\nCommit " SB_COMMIT);
    SetDlgItemText(dlg, IDC_COMPUTER_CPU, (flags & WF_CPU486) ? "80486 or better" :
                                   (flags & WF_CPU386) ? "80386" : "80286");
    SetDlgItemText(dlg, IDC_COMPUTER_MODE, (flags & WF_ENHANCED) ? "386 Enhanced Mode" : "Standard Mode");
    FormatSize(GetFreeSpace(0), text);
    lstrcat(text, " free memory");
    SetDlgItemText(dlg, IDC_COMPUTER_MEMORY, text);
    wsprintf(text, "%u%% system resources free", GetFreeSystemResources(GFSR_SYSTEMRESOURCES));
    SetDlgItemText(dlg, IDC_COMPUTER_RESOURCES, text);
}

