/*
 * The desktop's wallpaper. Windows 3.1 shows a BMP file, centred or tiled.
 * Anything else, another format or another way of showing it, is made once
 * into a BMP of the screen's size and colours (WALLPAPR.BMP, with Snowball's
 * settings), which Windows then shows as its own: nothing of this stays in
 * memory or runs again until the screen changes. Which picture and style it
 * was made from is kept in SNOWBALL.INI [Wallpaper], for Display Properties
 * and for making it again. If WIN.INI names another file, that one rules.
 *
 * Unless Setup was told otherwise ([Wallpaper] Managed=0) Snowball shows it, not
 * Windows, which loads any wallpaper whole before the shell starts and keeps it:
 * too much on a small machine, where the network then fails to start. So WIN.INI
 * names none, [Wallpaper] Shown does, and a picture larger than its share of the
 * memory is painted from a 256-colour file instead of being kept (draw.c's
 * neighbour wallpaper.c in SNSYSTEM). A file another program put in WIN.INI is
 * taken over at the next start.
 */
#include "internal.h"

#define SECTION     "Wallpaper"
#define WALL_SHARE  6                   /* of the memory, at most: the rest is for the user's programs */

static const char *const Names[WP_STYLES] = { "Fill", "Fit", "Stretch", "Tile", "Center" };
static LPCSTR making;

void WallpaperStyles(HWND combo)
{
    int i;

    for (i = 0; i < WP_STYLES; i++)
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)(LPSTR)Names[i]);
}

static BOOL Managed(void)
{
    return SettingsGetInt(SECTION, "Managed", 1);
}

/* The file shown: Snowball's, or what WIN.INI names. */
void WallpaperShown(LPSTR out, int size)
{
    *out = '\0';
    if (Managed())
        SettingsGetString(SECTION, "Shown", "", out, size);
    if (!*out)
        GetProfileString("Desktop", "Wallpaper", "", out, size);
}

BOOL WallpaperTiled(void)
{
    char shown[MAX_PATH];

    if (Managed() && (SettingsGetString(SECTION, "Shown", "", shown, sizeof shown), *shown))
        return SettingsGetInt(SECTION, "Tile", 0);
    return GetProfileInt("Desktop", "TileWallpaper", 0);
}

static void MadePath(LPSTR out)
{
    lstrcpy(out, g_home);
    PathAppend(out, "WALLPAPR.BMP");
}

static int ScreenDepth(void)
{
    HDC dc = GetDC(NULL);
    int depth = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);

    ReleaseDC(NULL, dc);
    return depth;
}

static void MadeFor(LPSTR out)
{
    wsprintf(out, "%dx%dx%d", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), ScreenDepth());
}

/* WIN.INI's wallpaper is the one made here. */
static BOOL IsMade(LPCSTR shown)
{
    char made[MAX_PATH], brief[MAX_PATH];

    MadePath(made);
    return !lstrcmpi(shown, made) || (DosShortPath(made, brief) && !lstrcmpi(shown, brief));
}

int WallpaperNow(LPSTR path)
{
    char shown[MAX_PATH];

    WallpaperShown(shown, sizeof shown);
    *path = '\0';
    if (IsMade(shown)) {
        SettingsGetString(SECTION, "Source", "", path, MAX_PATH);
        if (*path)
            return SettingsGetInt(SECTION, "Style", WP_CENTER);
    }
    if (*shown && lstrcmpi(shown, "(None)")) {
        if (!_fstrchr(shown, '\\')) {   /* a name: in the Windows folder */
            GetWindowsDirectory(path, MAX_PATH);
            PathAppend(path, shown);
        } else {
            lstrcpy(path, shown);
        }
    }
    return WallpaperTiled() ? WP_TILE : WP_CENTER;
}

/* A BMP within the screen, centred or tiled, is Windows' own to show. One
   that cannot be read here (compressed, or of OS/2) goes to Windows as ever. */
static BOOL AsItIs(LPCSTR path, int style)
{
    int w, h;

    if ((style != WP_TILE && style != WP_CENTER) || lstrcmpi(PathExtension((LPSTR)path), ".BMP"))
        return FALSE;
    return ImageSize(path, &w, &h) != IMAGE_OK ||
           (w <= GetSystemMetrics(SM_CXSCREEN) && h <= GetSystemMetrics(SM_CYSCREEN));
}

static void Job(IMAGEJOB *job, int style, IMAGEPROGRESS progress)
{
    job->style = style;
    job->w = GetSystemMetrics(SM_CXSCREEN);
    job->h = GetSystemMetrics(SM_CYSCREEN);
    job->back = GetSysColor(COLOR_BACKGROUND);
    job->progress = progress;
}

/* The wallpaper in use is this picture in this style: what was made is at hand. */
static BOOL IsInUse(LPCSTR path, int style)
{
    char now[MAX_PATH];

    return WallpaperNow(now) == style && !lstrcmpi(now, path);
}

int WallpaperPreview(LPCSTR path, int style, int width, IMAGEPROGRESS progress, DIB *d)
{
    IMAGEJOB job;
    char     made[MAX_PATH], shown[MAX_PATH];

    WallpaperShown(shown, sizeof shown);
    if (IsMade(shown) && IsInUse(path, style)) {
        MadePath(made);
        if (DibLoad(made, d))
            return IMAGE_OK;
    }
    if (AsItIs(path, style))
        return DibLoad(path, d) ? IMAGE_OK : IMAGE_DAMAGED;
    Job(&job, style, progress);
    return ImageMakeDib(path, &job, width, d);
}

/* For a reading that holds its window up: other programs go on. */
BOOL WallpaperYield(int percent)
{
    MSG m;

    (void)percent;
    PeekMessage(&m, NULL, 0, 0, PM_NOREMOVE);
    return TRUE;
}

static BOOL Working(int percent)
{
    ProgressItem(making, NULL);
    ProgressStep(percent, 100);
    return !ProgressCancelled();
}

static int Make(HWND owner, LPCSTR path, int style, LPCSTR made)
{
    IMAGEJOB job;
    DIB  d;
    char temp[MAX_PATH];
    int  r;

    making = path;
    Job(&job, style, Working);
    ProgressOpen(owner, FOP_WALLPAPER);
    r = ImageMakeDib(path, &job, 0, &d);
    if (r == IMAGE_OK) {
        lstrcpy(temp, made);
        lstrcpy(PathExtension(temp), ".TMP");
        r = ImageWriteBitmap(&d, temp, ScreenDepth());
        DibFree(&d);
        if (r == IMAGE_OK) {
            DosDelete(made);
            if (!DosRename(temp, made))
                r = IMAGE_NOWRITE;
        } else {
            DosDelete(temp);
        }
    }
    ProgressClose();
    return r;
}

/* Would this picture, as a bitmap of the screen, stay within its share of the memory? */
static BOOL Fits(int w, int h)
{
    return (DWORD)w * h * ScreenDepth() / 8 / 1024 <= WallpaperMemoryKB() / WALL_SHARE;
}

/* The 256-colour copy of a picture, made once. */
static BOOL LowCopy(LPCSTR shown, LPSTR low)
{
    char made[MAX_PATH];
    OFSTRUCT of;
    HCURSOR cursor;
    BOOL done;

    lstrcpy(low, g_home);
    PathAppend(low, "WALLLOW.BMP");
    SettingsGetString(SECTION, "Low", "", made, sizeof made);
    if (!lstrcmpi(made, shown) && OpenFile(low, &of, OF_EXIST) != HFILE_ERROR)
        return TRUE;
    cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    done = WallpaperMakeLow(shown, low);
    SetCursor(cursor);
    if (done)
        SettingsSetString(SECTION, "Low", shown);
    return done;
}

/* SNSYSTEM told how to show what [Wallpaper] Shown names. */
static void Display(void)
{
    char shown[MAX_PATH], low[MAX_PATH];
    BOOL tile = WallpaperTiled();
    int w, h, depth;

    SettingsGetString(SECTION, "Shown", "", shown, sizeof shown);
    if (!*shown || !DrawWallpaperInfo(shown, &w, &h, &depth)) {
        DrawWallpaperUse(NULL, FALSE, FALSE);
        return;
    }
    if (Fits(w, h) && DrawWallpaperUse(shown, TRUE, tile))
        return;
    if (depth > 8 && LowCopy(shown, low))
        DrawWallpaperUse(low, FALSE, tile);
    else
        DrawWallpaperUse(shown, FALSE, tile);
}

static void Show(LPCSTR path, BOOL tile)
{
    char brief[MAX_PATH];
    int w, h, depth;

    if (*path)
        DosShortPath(path, brief);
    if (Managed() && (!*path || DrawWallpaperInfo(brief, &w, &h, &depth))) {
        SettingsSetString(SECTION, "Shown", *path ? (LPCSTR)brief : NULL);
        SettingsSetInt(SECTION, "Tile", tile);
        SettingsSetString(SECTION, "Low", NULL);
        WriteProfileString("Desktop", "TileWallpaper", "0");
        SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (LPSTR)"(None)", SPIF_UPDATEINIFILE);
        Display();
    } else {                            /* compressed, or of OS/2: Windows' to show */
        SettingsSetString(SECTION, "Shown", NULL);
        DrawWallpaperUse(NULL, FALSE, FALSE);
        WriteProfileString("Desktop", "TileWallpaper", tile ? "1" : "0");
        SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, *path ? (LPSTR)brief : (LPSTR)"(None)", SPIF_UPDATEINIFILE);
    }
    DesktopRepaint();
}

BOOL WallpaperApply(HWND owner, LPCSTR path, int style)
{
    char made[MAX_PATH], now[32];
    int  r;

    if (!*path || AsItIs(path, style)) {
        SettingsSetString(SECTION, "Source", NULL);
        SettingsSetString(SECTION, "Style", NULL);
        SettingsSetString(SECTION, "Made", NULL);
        Show(path, style == WP_TILE);
        return TRUE;
    }
    MadePath(made);
    r = Make(owner, path, style, made);
    if (r == IMAGE_OK) {
        MadeFor(now);
        SettingsSetString(SECTION, "Source", path);
        SettingsSetInt(SECTION, "Style", style);
        SettingsSetString(SECTION, "Made", now);
        Show(made, style == WP_TILE);
        return TRUE;
    }
    if (r != IMAGE_STOPPED)
        ErrorBox(owner, r == IMAGE_MEMORY ? "There is not enough memory to prepare this picture." :
                        r == IMAGE_NOWRITE ? "The wallpaper could not be written. The disk may be full." :
                        "This picture cannot be used as wallpaper. It may be damaged, or in a form Snowball "
                        "cannot read.");
    return FALSE;
}

/* The desktop as it would be, small (the screen in s): its colour and a
   wallpaper (none: wall NULL or empty), centred or tiled. */
void DisplayPropertiesDesktop(HDC dc, const RECT *s, COLORREF desktop, const DIB *wall, BOOL tile)
{
    int scale = GetSystemMetrics(SM_CXSCREEN), w, h, x, y;

    DrawSolidRect(dc, s, desktop);
    if (!wall || !wall->mem)
        return;
    w = max(1, (int)((long)(wall->shownW ? wall->shownW : wall->w) * (s->right - s->left) / scale));
    h = max(1, (int)((long)(wall->shownH ? wall->shownH : wall->h) * (s->right - s->left) / scale));
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
            DibDraw(mem, wall, 0, 0, w, h);
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
                    DibDraw(dc, wall, x, y, w, h);
        }
        if (mem)
            DeleteDC(mem);
    } else {
        DibDraw(dc, wall, (s->left + s->right - w) / 2, (s->top + s->bottom - h) / 2, w, h);
    }
    RestoreDC(dc, -1);
}

/* After the screen changed: made again for it, if its picture is still there. */
static void Check(void)
{
    char path[MAX_PATH], shown[MAX_PATH], was[32], now[32];
    int  style, w, h;

    WallpaperShown(shown, sizeof shown);
    if (!IsMade(shown))
        return;
    SettingsGetString(SECTION, "Made", "", was, sizeof was);
    MadeFor(now);
    if (!lstrcmp(was, now))
        return;
    style = WallpaperNow(path);
    if (*path && ImageSize(path, &w, &h) == IMAGE_OK)
        WallpaperApply(NULL, path, style);
}

/* Windows has loaded what WIN.INI names by now: Snowball takes it, and frees Windows' copy. */
static void TakeOver(void)
{
    char named[MAX_PATH], path[MAX_PATH];
    int w, h, depth;

    GetProfileString("Desktop", "Wallpaper", "", named, sizeof named);
    if (!*named || !lstrcmpi(named, "(None)"))
        return;
    if (_fstrchr(named, '\\')) {
        lstrcpy(path, named);
    } else {
        GetWindowsDirectory(path, sizeof path);
        PathAppend(path, named);
    }
    if (!DrawWallpaperInfo(path, &w, &h, &depth)) {
        SettingsSetString(SECTION, "Shown", NULL);
        return;
    }
    SettingsSetString(SECTION, "Shown", path);
    SettingsSetInt(SECTION, "Tile", GetProfileInt("Desktop", "TileWallpaper", 0));
    SettingsSetString(SECTION, "Low", NULL);
    WriteProfileString("Desktop", "TileWallpaper", "0");
    SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (LPSTR)"(None)", SPIF_UPDATEINIFILE);
}

/* Setup was told Snowball no longer shows it: WIN.INI names it again. */
static void GiveBack(void)
{
    char shown[MAX_PATH];

    SettingsGetString(SECTION, "Shown", "", shown, sizeof shown);
    if (!*shown)
        return;
    WriteProfileString("Desktop", "TileWallpaper", SettingsGetInt(SECTION, "Tile", 0) ? "1" : "0");
    SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, shown, SPIF_UPDATEINIFILE);
    SettingsSetString(SECTION, "Shown", NULL);
    SettingsSetString(SECTION, "Tile", NULL);
    SettingsSetString(SECTION, "Low", NULL);
}

void WallpaperStart(void)
{
    if (!Managed()) {
        GiveBack();
        Check();
        return;
    }
    TakeOver();
    Check();
    Display();
    DesktopRepaint();
}
