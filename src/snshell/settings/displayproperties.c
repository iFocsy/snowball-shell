/*
 * Display Properties, as in Windows 98, on Windows 3.1's own settings:
 * Background (the wallpaper, centred or tiled: WIN.INI [Desktop]), Screen
 * Saver (SYSTEM.INI SCRNSAVE.EXE, WIN.INI ScreenSaveActive and
 * ScreenSaveTimeOut), Appearance (appearance.c) and Settings (the VBESVGA
 * driver's mode in SYSTEM.INI [vbesvga.drv], used once Windows restarts).
 * Background, Screen Saver and Settings show the screen on Windows 98's
 * monitor. A page is written only if something on it changed.
 */
#include "internal.h"

#define PAGE_BACKGROUND 0
#define PAGE_SAVER      1
#define PAGE_APPEARANCE 2
#define PAGE_SETTINGS   3
#define MONITOR_W       184             /* Windows 98's monitor (IDB_MONITOR), */
#define SCREEN_X        16              /* its screen */
#define SCREEN_Y        17
#define SCREEN_W        152
#define SCREEN_H        112

typedef struct {
    HGLOBAL mem;                        /* the BMP file after its header */
    int w, h;
    DWORD bits;                         /* where the pixels start in mem */
} PICTURE;

static const struct { int w, h; } areas[] = { { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1280, 1024 } };
static const struct { int depth; LPCSTR name, ini; } depths[] = {
    { 8, "256 Colors", "256" }, { 15, "High Color (15 bit)", "32k" },
    { 16, "High Color (16 bit)", "65k" }, { 24, "True Color (24 bit)", "16M" },
};

static HWND sheet;
static BOOL changed[4];
static PATHLIST walls, savers;          /* the lists' paths, by item data */
static PICTURE wall;                    /* the wallpaper chosen, for the monitor */
static char wallShown[MAX_PATH];

/* ---- the monitor ---- */

/* The monitor's screen, on the page under the tabs, centred. */
static void ScreenRect(HWND dlg, RECT *r)
{
    SetRect(r, 6, 21, 246, 21);
    MapDialogRect(dlg, r);
    r->left = (r->left + r->right - MONITOR_W) / 2 + SCREEN_X;
    r->top += SCREEN_Y;
    r->right = r->left + SCREEN_W;
    r->bottom = r->top + SCREEN_H;
}

static void PictureFree(PICTURE *p)
{
    if (p->mem)
        GlobalFree(p->mem);
    _fmemset(p, 0, sizeof *p);
}

/* A Windows bitmap file, whole, in memory. */
static BOOL PictureLoad(LPCSTR path, PICTURE *p)
{
    BITMAPFILEHEADER fh;
    LPBITMAPINFOHEADER bi;
    HFILE f;
    LONG size;

    PictureFree(p);
    if ((f = DosOpenRead(path)) == HFILE_ERROR)
        return FALSE;
    size = _llseek(f, 0, 2) - sizeof fh;
    _llseek(f, 0, 0);
    if (_lread(f, &fh, sizeof fh) != sizeof fh || fh.bfType != 0x4D42 || size <= 0 ||
        (p->mem = GlobalAlloc(GMEM_MOVEABLE, size)) == NULL) {
        _lclose(f);
        return FALSE;
    }
    bi = (LPBITMAPINFOHEADER)GlobalLock(p->mem);
    _hread(f, (void __huge *)bi, size);
    _lclose(f);
    if (bi->biSize != sizeof(BITMAPINFOHEADER)) {   /* OS/2 bitmaps: not shown */
        GlobalUnlock(p->mem);
        PictureFree(p);
        return FALSE;
    }
    p->w = (int)bi->biWidth;
    p->h = (int)bi->biHeight;
    p->bits = fh.bfOffBits - sizeof fh;
    GlobalUnlock(p->mem);
    return TRUE;
}

static void PictureDraw(HDC dc, PICTURE *p, int x, int y, int w, int h)
{
    LPBITMAPINFO bi = (LPBITMAPINFO)GlobalLock(p->mem);

    StretchDIBits(dc, x, y, w, h, 0, 0, p->w, p->h, (char __huge *)bi + p->bits, bi, DIB_RGB_COLORS, SRCCOPY);
    GlobalUnlock(p->mem);
}

/* The desktop as it would be, small: its colour and the wallpaper chosen. */
static void PaintScreen(HDC dc, const RECT *s, BOOL tile)
{
    int scale = GetSystemMetrics(SM_CXSCREEN), w, h, x, y;

    DrawSolidRect(dc, s, GetSysColor(COLOR_BACKGROUND));
    if (!wall.mem)
        return;
    w = max(1, (int)((long)wall.w * (s->right - s->left) / scale));
    h = max(1, (int)((long)wall.h * (s->right - s->left) / scale));
    SaveDC(dc);
    IntersectClipRect(dc, s->left, s->top, s->right, s->bottom);
    SetStretchBltMode(dc, COLORONCOLOR);
    if (tile) {
        /* the picture scaled once, then copied across: the first row by
           doubling what is filled, then the rows the same way (a tile per
           StretchDIBits took seconds) */
        int sw = s->right - s->left, sh = s->bottom - s->top;
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = mem ? CreateCompatibleBitmap(dc, sw, sh) : NULL, old;
        if (bmp) {
            old = SelectObject(mem, bmp);
            SetStretchBltMode(mem, COLORONCOLOR);
            PictureDraw(mem, &wall, 0, 0, w, h);
            for (x = w; x < sw; x *= 2)
                BitBlt(mem, x, 0, min(x, sw - x), h, mem, 0, 0, SRCCOPY);
            for (y = h; y < sh; y *= 2)
                BitBlt(mem, 0, y, sw, min(y, sh - y), mem, 0, 0, SRCCOPY);
            BitBlt(dc, s->left, s->top, sw, sh, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
        } else {
            for (y = s->top; y < s->bottom; y += h)
                for (x = s->left; x < s->right; x += w)
                    PictureDraw(dc, &wall, x, y, w, h);
        }
        if (mem)
            DeleteDC(mem);
    } else {
        PictureDraw(dc, &wall, (s->left + s->right - w) / 2, (s->top + s->bottom - h) / 2, w, h);
    }
    RestoreDC(dc, -1);
}

static void PaintMonitor(HWND dlg, HDC dc)
{
    RECT s;

    ScreenRect(dlg, &s);
    PaintBitmap(dc, s.left - SCREEN_X, s.top - SCREEN_Y, IDB_MONITOR);
    PaintScreen(dc, &s, SendDlgItemMessage(dlg, IDC_BG_STYLE, CB_GETCURSEL, 0, 0L) == 1);
}

static void RepaintMonitor(HWND dlg)
{
    RECT s;

    ScreenRect(dlg, &s);
    InvalidateRect(dlg, &s, FALSE);
}

/* A list's item data is the place of its path in a PATHLIST, or NONE
   ("(None)": a path list cannot hold an empty path). */
#define NONE        0xFFFFFFFFL

static void ItemPath(PATHLIST *l, DWORD i, LPSTR out)
{
    LPSTR p = PathListLock(l);

    for (; *p && i > 0 && i != NONE; i--)
        p = PathListNext(p);
    lstrcpy(out, i == NONE ? "" : p);
    PathListUnlock(l);
}

static void Changed(HWND dlg, int page)
{
    changed[page] = TRUE;
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
}

/* ---- Background ---- */

/* "Arcade" for ARCADE.BMP; a path list's index in the item data. */
static int AddWallpaper(HWND dlg, LPCSTR path)
{
    char name[MAX_PATH];
    int i;

    DisplayNameOfFile(PathFileName((LPSTR)path), name);
    *PathExtension(name) = '\0';
    i = (int)SendDlgItemMessage(dlg, IDC_BG_LIST, LB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
    SendDlgItemMessage(dlg, IDC_BG_LIST, LB_SETITEMDATA, i, walls.count);
    PathListAdd(&walls, path);
    return i;
}

static void ShowWallpaper(HWND dlg)
{
    char path[MAX_PATH];
    int i = (int)SendDlgItemMessage(dlg, IDC_BG_LIST, LB_GETCURSEL, 0, 0L);

    path[0] = '\0';
    if (i >= 0)
        ItemPath(&walls, SendDlgItemMessage(dlg, IDC_BG_LIST, LB_GETITEMDATA, i, 0L), path);
    if (lstrcmpi(path, wallShown)) {
        lstrcpy(wallShown, path);
        if (!*path || !PictureLoad(path, &wall))
            PictureFree(&wall);
    }
    RepaintMonitor(dlg);
}

static void SelectWallpaper(HWND dlg, LPCSTR path)
{
    char item[MAX_PATH];
    int i, n = (int)SendDlgItemMessage(dlg, IDC_BG_LIST, LB_GETCOUNT, 0, 0L);

    for (i = 0; i < n; i++) {
        ItemPath(&walls, SendDlgItemMessage(dlg, IDC_BG_LIST, LB_GETITEMDATA, i, 0L), item);
        if (!lstrcmpi(PathFileName(item), PathFileName((LPSTR)path)))
            break;
    }
    if (i == n)
        i = AddWallpaper(dlg, path);
    SendDlgItemMessage(dlg, IDC_BG_LIST, LB_SETCURSEL, i, 0L);
}

static void InitBackground(HWND dlg)
{
    char dir[MAX_PATH], spec[MAX_PATH], path[MAX_PATH], now[MAX_PATH];
    static DOSENTRY e;
    DOSFIND f;
    int i;

    PathListInit(&walls);
    i = (int)SendDlgItemMessage(dlg, IDC_BG_LIST, LB_ADDSTRING, 0, (LPARAM)(LPSTR)"(None)");
    SendDlgItemMessage(dlg, IDC_BG_LIST, LB_SETITEMDATA, i, NONE);
    GetWindowsDirectory(dir, sizeof dir);
    lstrcpy(spec, dir);
    PathAppend(spec, "*.BMP");
    if (DosFindFirst(spec, &f, &e)) {
        do {
            lstrcpy(path, dir);
            PathAppend(path, e.name);
            AddWallpaper(dlg, path);
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
    GetProfileString("Desktop", "Wallpaper", "(None)", now, sizeof now);
    if (!lstrcmpi(now, "(None)") || !*now) {
        SendDlgItemMessage(dlg, IDC_BG_LIST, LB_SETCURSEL, 0, 0L);
    } else {
        if (!_fstrchr(now, '\\')) {     /* a name: in the Windows folder */
            lstrcpy(path, dir);
            PathAppend(path, now);
            lstrcpy(now, path);
        }
        SelectWallpaper(dlg, now);
    }
    SendDlgItemMessage(dlg, IDC_BG_STYLE, CB_ADDSTRING, 0, (LPARAM)(LPSTR)"Center");
    SendDlgItemMessage(dlg, IDC_BG_STYLE, CB_ADDSTRING, 0, (LPARAM)(LPSTR)"Tile");
    SendDlgItemMessage(dlg, IDC_BG_STYLE, CB_SETCURSEL, GetProfileInt("Desktop", "TileWallpaper", 0) ? 1 : 0, 0L);
    ShowWallpaper(dlg);
}

static void ApplyBackground(HWND dlg)
{
    char path[MAX_PATH];
    int i = (int)SendDlgItemMessage(dlg, IDC_BG_LIST, LB_GETCURSEL, 0, 0L);

    path[0] = '\0';
    if (i >= 0)
        ItemPath(&walls, SendDlgItemMessage(dlg, IDC_BG_LIST, LB_GETITEMDATA, i, 0L), path);
    if (*path)
        DosShortPath(path, path);
    WriteProfileString("Desktop", "TileWallpaper",
                       SendDlgItemMessage(dlg, IDC_BG_STYLE, CB_GETCURSEL, 0, 0L) == 1 ? "1" : "0");
    SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, *path ? (LPSTR)path : (LPSTR)"(None)", SPIF_UPDATEINIFILE);
    DesktopRepaint();
}

/* ---- Screen Saver ---- */

/* Its module description after "SCRNSAVE :", as Windows 3.1 names them;
   else the file name. */
static void SaverName(LPCSTR path, LPSTR out)
{
    BYTE head[64], len;
    char text[128];
    LPSTR p;
    HFILE f;
    DWORD names;

    DisplayNameOfFile(PathFileName((LPSTR)path), out);
    *PathExtension(out) = '\0';
    if ((f = DosOpenRead(path)) == HFILE_ERROR)
        return;
    if (_lread(f, head, 64) == 64 && head[0] == 'M' && head[1] == 'Z' &&
        _llseek(f, *(WORD *)(head + 0x3C), 0) != -1 && _lread(f, head, 64) == 64 && head[0] == 'N' && head[1] == 'E') {
        names = *(DWORD *)(head + 0x2C);        /* the non-resident names: the description first */
        if (_llseek(f, names, 0) != -1 && _lread(f, &len, 1) == 1 && _lread(f, text, len) == len) {
            text[len] = '\0';
            if (!_fstrnicmp(text, "SCRNSAVE", 8) && (p = _fstrchr(text, ':')) != NULL) {
                for (p++; *p == ' '; p++)
                    ;
                if (*p)
                    lstrcpy(out, p);
            }
        }
    }
    _lclose(f);
}

static void AddSavers(HWND dlg, LPCSTR dir)
{
    char spec[MAX_PATH], path[MAX_PATH], name[128];
    static DOSENTRY e;
    DOSFIND f;
    int i;

    lstrcpy(spec, dir);
    PathAppend(spec, "*.SCR");
    if (DosFindFirst(spec, &f, &e)) {
        do {
            lstrcpy(path, dir);
            PathAppend(path, e.name);
            SaverName(path, name);
            i = (int)SendDlgItemMessage(dlg, IDC_SS_LIST, CB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
            SendDlgItemMessage(dlg, IDC_SS_LIST, CB_SETITEMDATA, i, savers.count);
            PathListAdd(&savers, path);
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
}

static BOOL SaverChosen(HWND dlg, LPSTR path)
{
    int i = (int)SendDlgItemMessage(dlg, IDC_SS_LIST, CB_GETCURSEL, 0, 0L);

    path[0] = '\0';
    if (i >= 0)
        ItemPath(&savers, SendDlgItemMessage(dlg, IDC_SS_LIST, CB_GETITEMDATA, i, 0L), path);
    return *path != '\0';
}

static void SaverButtons(HWND dlg)
{
    char path[MAX_PATH];
    BOOL on = SaverChosen(dlg, path);

    EnableWindow(GetDlgItem(dlg, IDC_SS_SETTINGS), on);
    EnableWindow(GetDlgItem(dlg, IDC_SS_PREVIEW), on);
    EnableWindow(GetDlgItem(dlg, IDC_SS_WAIT), on);
}

static void InitSaver(HWND dlg)
{
    char dir[MAX_PATH], now[MAX_PATH], item[MAX_PATH];
    int i, n, sel;

    PathListInit(&savers);
    i = (int)SendDlgItemMessage(dlg, IDC_SS_LIST, CB_ADDSTRING, 0, (LPARAM)(LPSTR)"(None)");
    SendDlgItemMessage(dlg, IDC_SS_LIST, CB_SETITEMDATA, i, NONE);
    GetWindowsDirectory(dir, sizeof dir);
    AddSavers(dlg, dir);
    GetSystemDirectory(dir, sizeof dir);
    AddSavers(dlg, dir);
    GetPrivateProfileString("boot", "SCRNSAVE.EXE", "", now, sizeof now, "SYSTEM.INI");
    n = (int)SendDlgItemMessage(dlg, IDC_SS_LIST, CB_GETCOUNT, 0, 0L);
    for (i = 1, sel = 0; i < n && *now && GetProfileInt("windows", "ScreenSaveActive", 0); i++) {
        ItemPath(&savers, SendDlgItemMessage(dlg, IDC_SS_LIST, CB_GETITEMDATA, i, 0L), item);
        if (!lstrcmpi(PathFileName(item), PathFileName(now)))
            sel = i;
    }
    SendDlgItemMessage(dlg, IDC_SS_LIST, CB_SETCURSEL, sel, 0L);
    SendDlgItemMessage(dlg, IDC_SS_WAIT, EM_LIMITTEXT, 2, 0L);
    SetDlgItemInt(dlg, IDC_SS_WAIT, (GetProfileInt("windows", "ScreenSaveTimeOut", 120) + 59) / 60, FALSE);
    SaverButtons(dlg);
}

/* The saver with "/c" (its settings) or "/s" (running). */
static void RunSaver(HWND dlg, LPCSTR how)
{
    char path[MAX_PATH], cmd[MAX_PATH + 4];

    if (!SaverChosen(dlg, path))
        return;
    DosShortPath(path, path);
    wsprintf(cmd, "%s %s", (LPSTR)path, how);
    WinExec(cmd, SW_SHOWNORMAL);
}

static void ApplySaver(HWND dlg)
{
    char path[MAX_PATH];
    BOOL on = SaverChosen(dlg, path), ok;
    UINT minutes = GetDlgItemInt(dlg, IDC_SS_WAIT, &ok, FALSE);

    if (on)
        DosShortPath(path, path);
    WritePrivateProfileString("boot", "SCRNSAVE.EXE", path, "SYSTEM.INI");
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, on, NULL, SPIF_UPDATEINIFILE);
    if (ok && minutes)
        SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, minutes * 60, NULL, SPIF_UPDATEINIFILE);
}

/* ---- Settings ---- */

static BOOL Vbe(void)
{
    char drv[16];

    GetPrivateProfileString("boot", "display.drv", "", drv, sizeof drv, "SYSTEM.INI");
    return !lstrcmpi(drv, "vbesvga.drv");
}

static void ShowArea(HWND dlg)
{
    char text[40];
    int i = SliderPos(GetDlgItem(dlg, IDC_ST_AREA));

    if (Vbe())
        wsprintf(text, "%d by %d pixels", areas[i].w, areas[i].h);
    else
        wsprintf(text, "%d by %d pixels", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    SetDlgItemText(dlg, IDC_ST_AREATEXT, text);
}

static void InitSettings(HWND dlg)
{
    char text[128];
    HWND colors = GetDlgItem(dlg, IDC_ST_COLORS), area = GetDlgItem(dlg, IDC_ST_AREA);
    int depth = GetPrivateProfileInt("vbesvga.drv", "Depth", 8, "SYSTEM.INI"), i, sel = 0;
    int w = GetPrivateProfileInt("vbesvga.drv", "Width", 640, "SYSTEM.INI");
    HDC dc;

    GetPrivateProfileString("boot.description", "display.drv", "", text, sizeof text, "SYSTEM.INI");
    SetDlgItemText(dlg, IDC_ST_DISPLAY, text);
    if (!Vbe()) {                       /* another driver: what it shows, unchangeable */
        dc = GetDC(NULL);
        depth = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
        ReleaseDC(NULL, dc);
        wsprintf(text, depth <= 4 ? "16 Colors" : depth == 8 ? "256 Colors" : "%d bit color", depth);
        SendMessage(colors, CB_ADDSTRING, 0, (LPARAM)(LPSTR)text);
        SendMessage(colors, CB_SETCURSEL, 0, 0L);
        EnableWindow(colors, FALSE);
        SliderSetCount(area, 1);
        EnableWindow(area, FALSE);
        ShowArea(dlg);
        return;
    }
    for (i = 0; i < sizeof depths / sizeof depths[0]; i++) {
        SendMessage(colors, CB_ADDSTRING, 0, (LPARAM)depths[i].name);
        if (depths[i].depth == depth)
            sel = i;
    }
    SendMessage(colors, CB_SETCURSEL, sel, 0L);
    SliderSetCount(area, sizeof areas / sizeof areas[0]);
    for (i = 0, sel = 0; i < sizeof areas / sizeof areas[0]; i++)
        if (areas[i].w == w)
            sel = i;
    SliderSetPos(area, sel);
    ShowArea(dlg);
}

/* Written for the driver; it takes them when Windows starts again. */
static void ApplySettings(HWND dlg)
{
    char text[64];
    int a = SliderPos(GetDlgItem(dlg, IDC_ST_AREA));
    int d = (int)SendDlgItemMessage(dlg, IDC_ST_COLORS, CB_GETCURSEL, 0, 0L);

    if (!Vbe() || d < 0)
        return;
    wsprintf(text, "%d", areas[a].w);
    WritePrivateProfileString("vbesvga.drv", "Width", text, "SYSTEM.INI");
    wsprintf(text, "%d", areas[a].h);
    WritePrivateProfileString("vbesvga.drv", "Height", text, "SYSTEM.INI");
    wsprintf(text, "%d", depths[d].depth);
    WritePrivateProfileString("vbesvga.drv", "Depth", text, "SYSTEM.INI");
    wsprintf(text, "Modern SVGA %dx%d %s Small", areas[a].w, areas[a].h, (LPSTR)depths[d].ini);
    WritePrivateProfileString("boot.description", "display.drv", text, "SYSTEM.INI");
    SetDlgItemText(dlg, IDC_ST_DISPLAY, text);
    if (MessageBox(dlg, "You must restart Windows before the new settings will take effect.\n\n"
                        "Do you want to restart Windows now?", "System Settings Change",
                   MB_ICONQUESTION | MB_YESNO) == IDYES)
        ExitWindows(EW_RESTARTWINDOWS, 0);
}

/* ---- the sheet ---- */

static int Page(HWND dlg)
{
    return TabsSelected(GetDlgItem(dlg, IDC_TABS));
}

static void Apply(HWND dlg)
{
    if (changed[PAGE_BACKGROUND])
        ApplyBackground(dlg);
    if (changed[PAGE_SAVER])
        ApplySaver(dlg);
    if (changed[PAGE_APPEARANCE])
        AppearanceApply(dlg);
    if (changed[PAGE_SETTINGS])
        ApplySettings(dlg);
    _fmemset(changed, 0, sizeof changed);
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
}

BOOL CALLBACK __export DisplayPropertiesProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;

    switch (msg) {
    case WM_INITDIALOG:
        sheet = dlg;
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Background");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Screen Saver");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Appearance");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Settings");
        InitBackground(dlg);
        InitSaver(dlg);
        AppearanceInit(dlg);
        InitSettings(dlg);
        _fmemset(changed, 0, sizeof changed);
        EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
        SkinDialog(dlg);
        WindowCenter(dlg);
        TabsSelect(GetDlgItem(dlg, IDC_TABS), (int)lp);
        TabsShowPage(dlg, (int)lp, NULL);
        return TRUE;
    case WM_ACTIVATE:                   /* modeless: the main loop hands it its keys */
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        break;
    case WM_PAINT:
        BeginPaint(dlg, &ps);
        if (Page(dlg) == PAGE_APPEARANCE)
            AppearancePaint(dlg, ps.hdc);
        else
            PaintMonitor(dlg, ps.hdc);
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_DRAWITEM:
        return AppearanceDrawItem(dlg, (const DRAWITEMSTRUCT FAR *)lp);
    case WM_COMMAND:
        switch (wp) {
        case IDC_TABS:
            if (HIWORD(lp) == TCN_SELECT)
                TabsShowPage(dlg, Page(dlg), NULL);
            break;
        case IDC_BG_LIST:
        case IDC_BG_STYLE:
            if (HIWORD(lp) == LBN_SELCHANGE || HIWORD(lp) == CBN_SELCHANGE) {
                ShowWallpaper(dlg);
                Changed(dlg, PAGE_BACKGROUND);
            }
            break;
        case IDC_BG_BROWSE:
            {
                char file[MAX_PATH] = "";
                if (BrowseDialogShow(dlg, "Browse", "Bitmap Files (*.bmp)\0*.bmp\0All Files\0*.*\0", file, TRUE)) {
                    SelectWallpaper(dlg, file);
                    ShowWallpaper(dlg);
                    Changed(dlg, PAGE_BACKGROUND);
                }
            }
            break;
        case IDC_SS_LIST:
            if (HIWORD(lp) == CBN_SELCHANGE) {
                SaverButtons(dlg);
                Changed(dlg, PAGE_SAVER);
            }
            break;
        case IDC_SS_WAIT:
            if (HIWORD(lp) == EN_CHANGE && GetFocus() == (HWND)LOWORD(lp))
                Changed(dlg, PAGE_SAVER);
            break;
        case IDC_SS_SETTINGS:
            RunSaver(dlg, "/c");
            break;
        case IDC_SS_PREVIEW:
            RunSaver(dlg, "/s");
            break;
        case IDC_ST_COLORS:
            if (HIWORD(lp) == CBN_SELCHANGE)
                Changed(dlg, PAGE_SETTINGS);
            break;
        case IDC_ST_AREA:
            ShowArea(dlg);
            Changed(dlg, PAGE_SETTINGS);
            break;
        default:
            if (!SheetButton(dlg, wp, Apply) && AppearanceCommand(dlg, wp, HIWORD(lp)))
                Changed(dlg, PAGE_APPEARANCE);
        }
        return TRUE;
    case WM_DESTROY:
        MessageLoopTrackDialog(dlg, FALSE);
        PathListFree(&walls);
        PathListFree(&savers);
        PictureFree(&wall);
        wallShown[0] = '\0';
        sheet = NULL;
        return TRUE;
    }
    return FALSE;
}

void DisplayPropertiesShow(int page)
{
    if (sheet)
        SetActiveWindow(sheet);
    else if (CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DISPLAY), NULL, DisplayPropertiesProc, page) && sheet)
        ShowWindow(sheet, SW_SHOWNORMAL);
}
