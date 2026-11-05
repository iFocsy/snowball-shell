/*
 * Disk Cleanup, as Windows 98's: for a drive, the files that can go, each
 * kind with its size and a check box (temporary files not changed for a
 * week, its Recycle Bin, old ScanDisk files); OK deletes the kinds checked.
 * From System Tools ("SNOWBALL.EXE /CLEANUP"), a drive's Properties and the
 * low disk space warning: the taskbar asks every five minutes whether a
 * fixed disk has less than a tenth free, and warns once for each.
 */
#include "internal.h"
#include <dos.h>

#define KINDS       3
#define K_TEMP      0
#define K_BIN       1
#define K_CHK       2
#define ROW_H       18
#define CHECK_SCANS 30                  /* TIMER_SCAN's (10 s): a check every 5 minutes */

static const struct { LPCSTR name, about; BOOL on; } kinds[KINDS] = {
    { "Temporary files", "Programs sometimes store temporary information in a TEMP folder. Before a program "
      "closes, it usually deletes this information. You can safely delete temporary files that have not been "
      "modified in over a week.", TRUE },
    { "Recycle Bin", "The Recycle Bin contains files you have deleted from your computer. These files are not "
      "permanently removed until you empty the Recycle Bin.", FALSE },
    { "Old ScanDisk files", "ScanDisk and CHKDSK save lost pieces of files as .CHK files, in the drive's root "
      "folder and its FOUND folders. Unless you need them, you can safely delete them.", TRUE },
};
static int drive;
static DWORD sizes[KINDS];
static BOOL checked[KINDS];
static WNDPROC listProc;

/* Days since 1980 of a DOS date (to 2099). */
static long Days(WORD d)
{
    static const int before[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int y = (d >> 9) + 1980, m = ((d >> 5) & 15) ? ((d >> 5) & 15) - 1 : 0;

    return (long)(y - 1980) * 365 + (y - 1977) / 4 + before[m % 12] + (d & 31) + (m > 1 && y % 4 == 0);
}

/* The bytes of dir's files of a kind (and folders' below, temporary
   files'); deleted with remove. */
static DWORD Walk(LPCSTR dir, int kind, BOOL remove, int depth)
{
    char path[MAX_PATH];
    long today = Days(HIWORD(DosNow()));
    DOSENTRY e;
    DOSFIND f;
    DWORD n = 0;
    LPSTR ext;

    lstrcpy(path, dir);
    PathAppend(path, "*.*");
    if (!DosFindFirst(path, &f, &e))
        return 0;
    do {
        lstrcpy(path, dir);
        PathAppend(path, e.name);
        if (e.attr & FA_DIR) {
            if (kind == K_TEMP && depth < 4)
                n += Walk(path, kind, remove, depth + 1);
            else if (kind == K_CHK && !depth && !_fstrnicmp(e.name, "FOUND.", 6))
                n += Walk(path, kind, remove, 1);
            continue;
        }
        ext = PathExtension(e.name);
        if (kind == K_TEMP ? today - Days(e.date) <= 7 : lstrcmpi(ext, ".CHK"))
            continue;
        n += e.size;
        if (remove && !(e.attr & FA_READONLY))
            DosDelete(path);
    } while (DosFindNext(&f, &e));
    DosFindClose(&f);
    if (remove && kind == K_CHK && depth)
        DosRemoveFolder(dir);           /* a FOUND folder, if empty now */
    return n;
}

/* The temporary folders on the drive: TEMP's, Windows'. */
static int TempFolders(char dirs[2][MAX_PATH])
{
    LPSTR env = GetDOSEnvironment();
    int n = 0;

    for (; env && *env; env += lstrlen(env) + 1)
        if (!_fstrnicmp(env, "TEMP=", 5) && (env[5] | 0x20) - 'a' == drive)
            lstrcpyn(dirs[n++], env + 5, MAX_PATH);
    GetWindowsDirectory(dirs[n], MAX_PATH);
    PathAppend(dirs[n], "TEMP");
    if ((dirs[n][0] | 0x20) - 'a' == drive && (!n || lstrcmpi(dirs[0], dirs[n])))
        n++;
    return n;
}

static DWORD Kind(HWND owner, int k, BOOL remove)
{
    char dirs[2][MAX_PATH], root[4];
    DWORD n = 0;
    int i;

    switch (k) {
    case K_TEMP:
        for (i = TempFolders(dirs) - 1; i >= 0; i--)
            n += Walk(dirs[i], K_TEMP, remove, 0);
        return n;
    case K_BIN:
        return RecycleBinDrive(owner, drive, remove);
    }
    wsprintf(root, "%c:\\", 'A' + drive);
    return Walk(root, K_CHK, remove, 0);
}

static void Totals(HWND dlg)
{
    char text[128], size[24], name[4];
    DWORD all = 0, gain = 0;
    int k;

    for (k = 0; k < KINDS; k++) {
        all += sizes[k];
        gain += checked[k] ? sizes[k] : 0;
    }
    wsprintf(name, "%c:", 'A' + drive);
    FormatSize(all, size);
    wsprintf(text, "You can use Disk Cleanup to free up to %s of disk space on (%s).", (LPSTR)size, (LPSTR)name);
    SetDlgItemText(dlg, IDC_CLEANUP_TEXT, text);
    FormatSize(gain, size);
    wsprintf(text, "Total amount of disk space you gain:  %s", (LPSTR)size);
    SetDlgItemText(dlg, IDC_CLEANUP_TOTAL, text);
}

static void Measure(HWND dlg)
{
    HCURSOR old = SetCursor(LoadCursor(NULL, IDC_WAIT));
    int k;

    for (k = 0; k < KINDS; k++)
        sizes[k] = Kind(dlg, k, FALSE);
    SetCursor(old);
    SendDlgItemMessage(dlg, IDC_CLEANUP_ICON, STM_SETICON, (WPARAM)IconForDrive(drive), 0L);
    InvalidateRect(GetDlgItem(dlg, IDC_CLEANUP_LIST), NULL, FALSE);
    Totals(dlg);
}

static void Toggle(HWND list, int k)
{
    if (k < 0 || k >= KINDS)
        return;
    checked[k] = !checked[k];
    InvalidateRect(list, NULL, FALSE);
    Totals(GetParent(list));
}

LRESULT CALLBACK __export DiskCleanupListProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res = CallWindowProc((FARPROC)listProc, w, msg, wp, lp);

    if (msg == WM_LBUTTONDOWN && LOWORD(lp) < 20)          /* on the check box */
        Toggle(w, (int)HIWORD(lp) / ROW_H);
    else if (msg == WM_KEYDOWN && wp == VK_SPACE)
        Toggle(w, (int)SendMessage(w, LB_GETCURSEL, 0, 0L));
    return res;
}

static void DrawKind(const DRAWITEMSTRUCT FAR *d)
{
    char size[24];
    RECT r = d->rcItem, t;
    int k = (int)d->itemID;

    if (k < 0 || k >= KINDS)
        return;
    DrawSolidRect(d->hDC, &r, GetSysColor(COLOR_WINDOW));
    SkinCheckBox(d->hDC, r.left + 3, r.top + (ROW_H - 13) / 2, FALSE, checked[k], FALSE);
    SetBkMode(d->hDC, TRANSPARENT);
    SetRect(&t, r.left + 22, r.top, r.right - 70, r.bottom);
    if (d->itemState & ODS_SELECTED) {
        DrawSolidRect(d->hDC, &t, GetSysColor(COLOR_HIGHLIGHT));
        SetTextColor(d->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
    } else {
        SetTextColor(d->hDC, GetSysColor(COLOR_WINDOWTEXT));
    }
    t.left += 2;
    DrawText(d->hDC, kinds[k].name, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SetTextColor(d->hDC, GetSysColor(COLOR_WINDOWTEXT));
    FormatSize(sizes[k], size);
    SetRect(&t, r.right - 68, r.top, r.right - 4, r.bottom);
    DrawText(d->hDC, size, -1, &t, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* OK: asked first, then the kinds checked deleted. */
static BOOL Clean(HWND dlg)
{
    HCURSOR old;
    int k;

    if (MessageBox(dlg, "Are you sure you want to delete files?", "Disk Cleanup", MB_ICONQUESTION | MB_YESNO) != IDYES)
        return FALSE;
    old = SetCursor(LoadCursor(NULL, IDC_WAIT));
    for (k = 0; k < KINDS; k++)
        if (checked[k])
            Kind(dlg, k, TRUE);
    SetCursor(old);
    FileCommandsFolderChanged(NULL);
    return TRUE;
}

BOOL CALLBACK __export DiskCleanupProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND list = GetDlgItem(dlg, IDC_CLEANUP_LIST);
    char name[8];
    DWORD total, free;
    int d, i;

    switch (msg) {
    case WM_INITDIALOG:
        for (d = 2; d < 26; d++)                        /* the fixed disks */
            if (GetDriveType(d) == DRIVE_FIXED && DosDiskSpace(d, &total, &free) && total) {
                wsprintf(name, "(%c:)", 'A' + d);
                i = (int)SendDlgItemMessage(dlg, IDC_CLEANUP_DRIVE, CB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
                SendDlgItemMessage(dlg, IDC_CLEANUP_DRIVE, CB_SETITEMDATA, i, d);
                if (d == drive)
                    SendDlgItemMessage(dlg, IDC_CLEANUP_DRIVE, CB_SETCURSEL, i, 0L);
            }
        for (i = 0; i < KINDS; i++) {
            checked[i] = kinds[i].on;
            SendMessage(list, LB_ADDSTRING, 0, (LPARAM)i);
        }
        SendMessage(list, LB_SETCURSEL, 0, 0L);
        SetDlgItemText(dlg, IDC_CLEANUP_ABOUT, kinds[0].about);
        listProc = (WNDPROC)SetWindowLong(list, GWL_WNDPROC, (LONG)(FARPROC)DiskCleanupListProc);
        SkinDialog(dlg);
        WindowCenter(dlg);
        Measure(dlg);
        return TRUE;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = ROW_H;
        return TRUE;
    case WM_DRAWITEM:
        DrawKind((const DRAWITEMSTRUCT FAR *)lp);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDC_CLEANUP_LIST && HIWORD(lp) == LBN_SELCHANGE) {
            i = (int)SendMessage(list, LB_GETCURSEL, 0, 0L);
            SetDlgItemText(dlg, IDC_CLEANUP_ABOUT, kinds[max(0, min(i, KINDS - 1))].about);
        } else if (wp == IDC_CLEANUP_DRIVE && HIWORD(lp) == CBN_SELCHANGE) {
            i = (int)SendDlgItemMessage(dlg, IDC_CLEANUP_DRIVE, CB_GETCURSEL, 0, 0L);
            drive = (int)SendDlgItemMessage(dlg, IDC_CLEANUP_DRIVE, CB_GETITEMDATA, i, 0L);
            Measure(dlg);
        } else if (wp == IDOK) {
            if (Clean(dlg))
                EndDialog(dlg, TRUE);
        } else if (wp == IDCANCEL) {
            EndDialog(dlg, FALSE);
        }
        return TRUE;
    case WM_DESTROY:
        SetWindowLong(list, GWL_WNDPROC, (LONG)(FARPROC)listProc);
        return TRUE;
    }
    return FALSE;
}

/* For drive d (0 = A:); -1: Windows' drive. */
void DiskCleanupShow(HWND owner, int d)
{
    char windows[MAX_PATH];

    GetWindowsDirectory(windows, sizeof windows);
    drive = d >= 0 ? d : (windows[0] | 0x20) - 'a';
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CLEANUP), owner, DiskCleanupProc);
}

/* ---- the low disk space warning ---- */

BOOL CALLBACK __export DiskCleanupWarningProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char text[200];

    switch (msg) {
    case WM_INITDIALOG:
        SendDlgItemMessage(dlg, IDC_LOWDISK_ICON, STM_SETICON, (WPARAM)LoadIcon(NULL, IDI_EXCLAMATION), 0L);
        wsprintf(text, "You are running out of disk space on drive %c.\n\nTo free space on this drive by deleting "
                 "old or unnecessary files, click Disk Cleanup.", 'A' + drive);
        SetDlgItemText(dlg, IDC_LOWDISK_TEXT, text);
        SkinDialog(dlg);
        WindowCenter(dlg);
        MessageBeep(MB_ICONEXCLAMATION);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK || wp == IDCANCEL)
            EndDialog(dlg, wp == IDOK);
        return TRUE;
    }
    return FALSE;
}

/* A file can be made on it: not a disk that is always full (DOSBox-X's Z:,
   a read-only volume). */
static BOOL Writable(int d)
{
    char path[16];
    HFILE f;

    wsprintf(path, "%c:\\~SBFREE.TMP", 'A' + d);
    if ((f = _lcreat(path, 0)) == HFILE_ERROR)
        return FALSE;
    _lclose(f);
    DosDelete(path);
    return TRUE;
}

/* TIMER_SCAN: every 5 minutes, a fixed disk with less than a tenth free
   (and at least 10 MB in all) that takes files is warned of, once. */
void DiskCleanupCheck(HWND owner)
{
    static int scans = CHECK_SCANS - 6;     /* the first check a minute after the start */
    static DWORD warned;                    /* a bit for each drive */
    DWORD total, free;
    int d;

    if (++scans < CHECK_SCANS || !SettingsGetInt("Disk Cleanup", "Warn", 1))
        return;
    scans = 0;
    for (d = 2; d < 26; d++)
        if (!(warned & (1L << d)) && GetDriveType(d) == DRIVE_FIXED && DosDiskSpace(d, &total, &free) &&
            total >= 10L * 1024 * 1024 && free < total / 10) {
            warned |= 1L << d;
            if (!Writable(d))
                continue;
            drive = d;
            if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_LOWDISK), owner, DiskCleanupWarningProc) == TRUE)
                DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CLEANUP), owner, DiskCleanupProc);
            return;
        }
}
