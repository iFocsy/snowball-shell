/*
 * Themes, as Windows 98's Desktop Themes, kept in Snowball's THEMES.INI:
 * [Themes] lists them (name=group); [Theme: name] holds what each sets, any
 * of its colours (Colors=, appearance.c's form), the wallpaper (Wallpaper=,
 * WallpaperStyle=, TileWallpaper= for what has no style), the screen saver (ScreenSaver=), sounds (Sound <event>=
 * a .WAV), mouse pointers (Pointer <name>= a .CUR or .ANI) and desktop
 * icons (Icon <name>= file,index); an empty value sets none (Windows' own
 * pointer or icon). A colour scheme is a theme with colours only: Display
 * Properties offers every theme's colours. The first start makes
 * Snowball's schemes (schemes.c) and those the user saved in CONTROL.INI
 * themes; Restore Defaults brings Snowball's back. Windows 98's .Theme
 * files come in and go out as they are. Applied, pointers and icons are
 * SNOWBALL.INI's [Pointers] and [Icons] (set again at each start), the rest
 * Windows' own settings.
 */
#include "internal.h"

#define INDEX       "Themes"
#define CUSTOM      "Custom"
#define MAX_THEMES  200
#define KEYS_SIZE   4096
#define MISSING     "<missing>"         /* a default no value can be (Windows trims control characters) */

static const struct { LPCSTR key, name; } sounds[THEME_SOUNDS_N] = {
    { "SystemDefault", "Default sound" },   { "SystemAsterisk", "Asterisk" },
    { "SystemExclamation", "Exclamation" }, { "SystemHand", "Critical Stop" },
    { "SystemQuestion", "Question" },       { "SystemStart", "Start Windows" },
    { "SystemExit", "Exit Windows" },       { "Open", "Open program" },
    { "Close", "Close program" },           { "Minimize", "Minimize" },
    { "Maximize", "Maximize" },             { "RestoreUp", "Restore Up" },
    { "RestoreDown", "Restore Down" },      { "MenuPopup", "Menu popup" },
    { "EmptyRecycleBin", "Empty Recycle Bin" },
};
static const struct { LPCSTR key, name; } pointers[CURSORS] = {   /* by CURSOR_* */
    { "Arrow", "Normal Select" },           { "IBeam", "Text Select" },
    { "Wait", "Busy" },                     { "Crosshair", "Precision Select" },
    { "UpArrow", "Alternate Select" },      { "SizeAll", "Move" },
    { "SizeNWSE", "Diagonal Resize 1" },    { "SizeNESW", "Diagonal Resize 2" },
    { "SizeWE", "Horizontal Resize" },      { "SizeNS", "Vertical Resize" },
};
static const struct { LPCSTR key, name, clsid, value; } icons[THEME_ICONS_N] = {   /* Windows 98's place for each */
    { "Computer", "My Computer", "{20D04FE0-3AEA-1069-A2D8-08002B30309D}", "DefaultValue" },
    { "Documents", "My Documents", "{450D8FBA-AD25-11D0-98A8-0800361B1103}", "DefaultValue" },
    { "BinEmpty", "Recycle Bin (empty)", "{645FF040-5081-101B-9F08-00AA002F954E}", "empty" },
    { "BinFull", "Recycle Bin (full)", "{645FF040-5081-101B-9F08-00AA002F954E}", "full" },
};
static const LPCSTR groupOrder[] = { "Windows", "Light", "Colorful", "Dark", "Windows 98 Classic", "High Contrast" };

LPCSTR ThemeSoundKey(int i)    { return sounds[i].key; }
LPCSTR ThemeSoundName(int i)   { return sounds[i].name; }
LPCSTR ThemePointerKey(int i)  { return pointers[i].key; }
LPCSTR ThemePointerName(int i) { return pointers[i].name; }
LPCSTR ThemeIconKey(int i)     { return icons[i].key; }
LPCSTR ThemeIconName(int i)    { return icons[i].name; }

static void Ini(LPSTR out)
{
    lstrcpy(out, g_home);
    lstrcat(out, "THEMES.INI");
}

static void Section(LPCSTR name, LPSTR out)
{
    wsprintf(out, "Theme: %s", name);
}

/* A part's key: "Sound Open", "Pointer Arrow", "Icon Computer". */
void ThemeKey(LPCSTR kind, LPCSTR key, LPSTR out)
{
    wsprintf(out, "%s %s", kind, key);
}

BOOL ThemeGet(LPCSTR name, LPCSTR key, LPSTR out, int size)
{
    char ini[MAX_PATH], sec[THEME_NAME + 8];

    Ini(ini);
    Section(name, sec);
    GetPrivateProfileString(sec, key, MISSING, out, size, ini);
    if (lstrcmp(out, MISSING))
        return TRUE;
    *out = '\0';
    return FALSE;
}

void ThemeSet(LPCSTR name, LPCSTR key, LPCSTR value)
{
    char ini[MAX_PATH], sec[THEME_NAME + 8];

    Ini(ini);
    Section(name, sec);
    WritePrivateProfileString(sec, key, value, ini);
}

BOOL ThemeGroup(LPCSTR name, LPSTR out)
{
    char ini[MAX_PATH];

    Ini(ini);
    GetPrivateProfileString(INDEX, name, MISSING, out, THEME_GROUP, ini);
    if (lstrcmp(out, MISSING))
        return TRUE;
    *out = '\0';
    return FALSE;
}

BOOL ThemeExists(LPCSTR name)
{
    char group[THEME_GROUP];

    return ThemeGroup(name, group);
}

void ThemeSetGroup(LPCSTR name, LPCSTR group)
{
    char ini[MAX_PATH];

    Ini(ini);
    WritePrivateProfileString(INDEX, name, *group ? group : CUSTOM, ini);
}

/* A new theme, empty; FALSE: one has that name. */
BOOL ThemeAdd(LPCSTR name, LPCSTR group)
{
    if (!*name || ThemeExists(name))
        return FALSE;
    ThemeSetGroup(name, group);
    return TRUE;
}

void ThemeDelete(LPCSTR name)
{
    char ini[MAX_PATH], sec[THEME_NAME + 8];

    Ini(ini);
    Section(name, sec);
    WritePrivateProfileString(sec, NULL, NULL, ini);
    WritePrivateProfileString(INDEX, name, NULL, ini);
}

/* A section's keys, each in turn ("" ends), in a block of the caller's. */
static LPSTR Keys(LPCSTR section, LPCSTR file, HGLOBAL *h)
{
    LPSTR keys;

    if ((*h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, KEYS_SIZE)) == NULL)
        return NULL;
    keys = GlobalLock(*h);
    GetPrivateProfileString(section, NULL, "", keys, KEYS_SIZE, file);
    return keys;
}

static void FreeKeys(HGLOBAL h)
{
    GlobalUnlock(h);
    GlobalFree(h);
}

/* The THEME_* parts a theme sets. */
UINT ThemeParts(LPCSTR name)
{
    char ini[MAX_PATH], sec[THEME_NAME + 8];
    HGLOBAL h;
    LPSTR k;
    UINT parts = 0;

    Ini(ini);
    Section(name, sec);
    if ((k = Keys(sec, ini, &h)) == NULL)
        return 0;
    for (; *k; k += lstrlen(k) + 1)
        parts |= !lstrcmpi(k, "Colors") ? THEME_COLORS : !lstrcmpi(k, "Wallpaper") ? THEME_WALLPAPER :
                 !lstrcmpi(k, "ScreenSaver") ? THEME_SAVER : !_fstrnicmp(k, "Sound ", 6) ? THEME_SOUNDS :
                 !_fstrnicmp(k, "Pointer ", 8) ? THEME_POINTERS : !_fstrnicmp(k, "Icon ", 5) ? THEME_ICONS : 0;
    FreeKeys(h);
    return parts;
}

/* A part taken out of a theme (part THEME_*), every key of it. */
void ThemeDrop(LPCSTR name, UINT part)
{
    char key[40];
    int i;

    if (part == THEME_COLORS)
        ThemeSet(name, "Colors", NULL);
    if (part == THEME_WALLPAPER) {
        ThemeSet(name, "Wallpaper", NULL);
        ThemeSet(name, "WallpaperStyle", NULL);
        ThemeSet(name, "TileWallpaper", NULL);
    }
    if (part == THEME_SAVER)
        ThemeSet(name, "ScreenSaver", NULL);
    for (i = 0; part == THEME_SOUNDS && i < THEME_SOUNDS_N; i++) {
        ThemeKey("Sound", sounds[i].key, key);
        ThemeSet(name, key, NULL);
    }
    for (i = 0; part == THEME_POINTERS && i < CURSORS; i++) {
        ThemeKey("Pointer", pointers[i].key, key);
        ThemeSet(name, key, NULL);
    }
    for (i = 0; part == THEME_ICONS && i < THEME_ICONS_N; i++) {
        ThemeKey("Icon", icons[i].key, key);
        ThemeSet(name, key, NULL);
    }
}

/* Every key of one theme into another (new) one, its group too. */
BOOL ThemeCopy(LPCSTR from, LPCSTR to)
{
    char ini[MAX_PATH], sec[THEME_NAME + 8], group[THEME_GROUP], value[COLOR_TEXT + MAX_PATH];
    HGLOBAL h;
    LPSTR k;

    ThemeGroup(from, group);
    if (!ThemeAdd(to, group))
        return FALSE;
    Ini(ini);
    Section(from, sec);
    if ((k = Keys(sec, ini, &h)) == NULL)
        return FALSE;
    for (; *k; k += lstrlen(k) + 1) {
        ThemeGet(from, k, value, sizeof value);
        ThemeSet(to, k, value);
    }
    FreeKeys(h);
    return TRUE;
}

BOOL ThemeRename(LPCSTR from, LPCSTR to)
{
    if (!ThemeCopy(from, to))
        return FALSE;
    ThemeDelete(from);
    return TRUE;
}

/* ---- the list ---- */

static int Rank(LPCSTR group)
{
    int i;

    for (i = 0; i < sizeof groupOrder / sizeof groupOrder[0]; i++)
        if (!lstrcmpi(group, groupOrder[i]))
            return i;
    return 99;                          /* the user's groups after Snowball's */
}

typedef struct { LPSTR name; char group[THEME_GROUP]; } LISTED;

static int CompareListed(const void far *pa, const void far *pb)
{
    const LISTED far *a = pa, far *b = pb;
    int ra = Rank(a->group), rb = Rank(b->group), c;

    if (ra != rb)
        return ra < rb ? -1 : 1;
    if ((c = lstrcmpi(a->group, b->group)) != 0)
        return c;
    return lstrcmpi(a->name, b->name);
}

/* Every theme (names, and their groups at the same places): by group,
   Snowball's first, then by name. How many. */
int ThemesList(PATHLIST *names, PATHLIST *groups)
{
    char ini[MAX_PATH];
    HGLOBAL h, hl;
    LISTED far *l;
    LPSTR k;
    int n = 0, i;

    ThemesPrepare();
    Ini(ini);
    if ((k = Keys(INDEX, ini, &h)) == NULL)
        return 0;
    if ((hl = GlobalAlloc(GMEM_MOVEABLE, MAX_THEMES * (DWORD)sizeof(LISTED))) != NULL) {
        l = (LISTED far *)GlobalLock(hl);
        for (; *k && n < MAX_THEMES; k += lstrlen(k) + 1, n++) {
            l[n].name = k;
            GetPrivateProfileString(INDEX, k, CUSTOM, l[n].group, THEME_GROUP, ini);
        }
        SortRecords(l, n, sizeof(LISTED), CompareListed);
        for (i = 0; i < n; i++) {
            PathListAdd(names, l[i].name);
            PathListAdd(groups, l[i].group);
        }
        GlobalUnlock(hl);
        GlobalFree(hl);
    }
    FreeKeys(h);
    return n;
}

/* Snowball's themes as they came, and any it lost; the user's stay. */
void ThemesRestoreDefaults(void)
{
    char ini[MAX_PATH], sec[THEME_NAME + 8];
    int i;

    Ini(ini);
    for (i = 0; SchemesName(i); i++) {
        Section(SchemesName(i), sec);
        WritePrivateProfileString(sec, NULL, NULL, ini);
        WritePrivateProfileString(INDEX, SchemesName(i), SchemesGroup(i), ini);
        WritePrivateProfileString(sec, "Colors", SchemesColors(SchemesName(i)), ini);
    }
}

/* When first listed: Snowball's themes, and the schemes the user saved
   in CONTROL.INI (Windows 3.1's own left out). */
void ThemesPrepare(void)
{
    char ini[MAX_PATH], first[4], value[256];
    HGLOBAL h;
    LPSTR k;

    Ini(ini);
    if (GetPrivateProfileString(INDEX, NULL, "", first, sizeof first, ini))
        return;
    ThemesRestoreDefaults();
    if ((k = Keys("color schemes", "CONTROL.INI", &h)) == NULL)
        return;
    for (; *k; k += lstrlen(k) + 1)
        if (!SchemesOld(k) && !SchemesColors(k) && ThemeAdd(k, CUSTOM)) {
            GetPrivateProfileString("color schemes", k, "", value, sizeof value, "CONTROL.INI");
            ThemeSet(k, "Colors", value);
        }
    FreeKeys(h);
}

/* ---- the settings in use ---- */

/* The file a WIN.INI [sounds] value names ("" for none). */
static void SoundFile(LPCSTR key, LPSTR file)
{
    LPSTR comma;

    GetProfileString("sounds", key, "", file, MAX_PATH);
    if ((comma = _fstrchr(file, ',')) != NULL)
        *comma = '\0';
    if (!lstrcmpi(file, "none") || !lstrcmpi(file, "<none>"))
        *file = '\0';
}

/* A theme made of the settings in use now, every part. */
void ThemeFromCurrent(LPCSTR name)
{
    COLORSCHEME cs;
    char value[COLOR_TEXT + MAX_PATH], key[40];
    int i;

    ColorSchemeCurrent(&cs);
    ColorSchemeFormat(&cs, value);
    ThemeSet(name, "Colors", value);
    ThemeSetWallpaperStyle(name, WallpaperNow(value));
    ThemeSet(name, "Wallpaper", value);
    GetPrivateProfileString("boot", "SCRNSAVE.EXE", "", value, sizeof value, "SYSTEM.INI");
    ThemeSet(name, "ScreenSaver", GetProfileInt("windows", "ScreenSaveActive", 0) ? value : "");
    for (i = 0; i < THEME_SOUNDS_N; i++) {
        SoundFile(sounds[i].key, value);
        ThemeKey("Sound", sounds[i].key, key);
        ThemeSet(name, key, value);
    }
    for (i = 0; i < CURSORS; i++) {
        SettingsGetString("Pointers", pointers[i].key, "", value, sizeof value);
        ThemeKey("Pointer", pointers[i].key, key);
        ThemeSet(name, key, value);
    }
    for (i = 0; i < THEME_ICONS_N; i++) {
        SettingsGetString("Icons", icons[i].key, "", value, sizeof value);
        ThemeKey("Icon", icons[i].key, key);
        ThemeSet(name, key, value);
    }
}

/* Pointer i from file (a long path, "": Windows' own). */
static void SetPointer(int i, LPCSTR file)
{
    char path[MAX_PATH];

    if (*file)
        DosShortPath(file, path);
    CursorsSet(i, *file ? path : NULL);
}

/* The pointers chosen, at the shell's start. */
void ThemesStartup(void)
{
    char file[MAX_PATH];
    int i;

    for (i = 0; i < CURSORS; i++) {
        SettingsGetString("Pointers", pointers[i].key, "", file, sizeof file);
        if (*file)
            SetPointer(i, file);
    }
}

/* The parts of a theme (THEME_*) it has, made Windows' settings. */
void ThemeApply(LPCSTR name, UINT parts)
{
    COLORSCHEME cs;
    char value[COLOR_TEXT + MAX_PATH], key[40], desc[64], *comma;
    int i;

    parts &= ThemeParts(name);
    if ((parts & THEME_COLORS) && ThemeGet(name, "Colors", value, sizeof value) && ColorSchemeParse(value, &cs))
        ColorSchemeApply(&cs, name);
    if (parts & THEME_WALLPAPER) {
        ThemeGet(name, "Wallpaper", value, sizeof value);
        WallpaperApply(NULL, value, ThemeWallpaperStyle(name));
    }
    if (parts & THEME_SAVER) {
        ThemeGet(name, "ScreenSaver", value, sizeof value);
        if (*value)
            DosShortPath(value, value);
        WritePrivateProfileString("boot", "SCRNSAVE.EXE", value, "SYSTEM.INI");
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, *value != '\0', NULL, SPIF_UPDATEINIFILE);
    }
    for (i = 0; (parts & THEME_SOUNDS) && i < THEME_SOUNDS_N; i++) {
        GetProfileString("sounds", sounds[i].key, "", value, sizeof value);    /* its description stays */
        comma = _fstrchr(value, ',');
        lstrcpyn(desc, comma ? comma + 1 : sounds[i].name, sizeof desc);
        ThemeKey("Sound", sounds[i].key, key);
        ThemeGet(name, key, value, MAX_PATH);
        lstrcat(value, ",");
        lstrcat(value, desc);
        WriteProfileString("sounds", sounds[i].key, value);
    }
    for (i = 0; (parts & THEME_POINTERS) && i < CURSORS; i++) {
        ThemeKey("Pointer", pointers[i].key, key);
        ThemeGet(name, key, value, sizeof value);
        SettingsSetString("Pointers", pointers[i].key, *value ? (LPSTR)value : NULL);
        SetPointer(i, value);
    }
    if (parts & THEME_ICONS) {
        for (i = 0; i < THEME_ICONS_N; i++) {
            ThemeKey("Icon", icons[i].key, key);
            ThemeGet(name, key, value, sizeof value);
            SettingsSetString("Icons", icons[i].key, *value ? (LPSTR)value : NULL);
        }
        IconsThemeChanged();
        FileCommandsFolderChanged(NULL);
    }
}

/* ---- Windows 98's .Theme files ---- */

#define COLORS_98   "Control Panel\\Colors"
#define DESKTOP_98  "Control Panel\\Desktop"
#define CURSORS_98  "Control Panel\\Cursors"

/* "r g b" as a colour; FALSE: not one. */
static BOOL Rgb(LPCSTR p, COLORREF *c)
{
    int v[3], i;

    for (i = 0; i < 3; i++) {
        while (*p == ' ')
            p++;
        if (*p < '0' || *p > '9')
            return FALSE;
        for (v[i] = 0; *p >= '0' && *p <= '9'; p++)
            v[i] = v[i] * 10 + *p - '0';
        if (v[i] > 255)
            return FALSE;
    }
    *c = RGB(v[0], v[1], v[2]);
    return TRUE;
}

/* "%ThemeDir%x.wav" and "%WinDir%x" made whole paths. */
static void Expand(LPCSTR value, LPCSTR themeDir, LPSTR out)
{
    char win[MAX_PATH];

    if (!_fstrnicmp(value, "%ThemeDir%", 10)) {
        lstrcpy(out, themeDir);
        PathAppend(out, value + 10);
    } else if (!_fstrnicmp(value, "%WinDir%", 8)) {
        GetWindowsDirectory(win, sizeof win);
        lstrcpy(out, win);
        PathAppend(out, value + 8);
    } else {
        lstrcpy(out, value);
    }
}

static BOOL Ext(LPCSTR path, LPCSTR a, LPCSTR b)
{
    LPCSTR e = PathExtension((LPSTR)path);
    return !lstrcmpi(e, a) || (b && !lstrcmpi(e, b));
}

int ThemeWallpaperStyle(LPCSTR name)
{
    char value[8];

    if (ThemeGet(name, "WallpaperStyle", value, sizeof value) && *value >= '0' && *value < '0' + WP_STYLES)
        return *value - '0';
    return ThemeGet(name, "TileWallpaper", value, sizeof value) && *value == '1' ? WP_TILE : WP_CENTER;
}

void ThemeSetWallpaperStyle(LPCSTR name, int style)
{
    char value[8];

    wsprintf(value, "%d", style);
    ThemeSet(name, "WallpaperStyle", value);
    ThemeSet(name, "TileWallpaper", style == WP_TILE ? "1" : "0");
}

/* A .Theme file as a new theme in Custom, named after the file. What
   Windows 3.1 cannot show is left out: a wallpaper that is no picture read
   here, pointers that are not .CUR or .ANI. */
BOOL ThemeImport(LPCSTR file, LPSTR name)
{
    COLORSCHEME cs;
    BOOL any = FALSE;
    char f[MAX_PATH], dir[MAX_PATH], sec[96], value[COLOR_TEXT + 8], path[MAX_PATH], key[40];
    COLORREF c;
    int i, k;

    DosShortPath(file, f);
    lstrcpy(dir, file);
    PathToParent(dir);
    DisplayNameOfFile(PathFileName((LPSTR)file), name);
    *PathExtension(name) = '\0';
    if (!ThemeAdd(name, CUSTOM))
        return FALSE;
    ColorSchemeCurrent(&cs);
    for (k = 0; k < SC_COUNT; k++)          /* Windows 98's: made from the others unless given */
        cs.chosen[k] = FALSE;
    for (k = 0; k < COLOR_KEYS; k++) {
        GetPrivateProfileString(COLORS_98, ColorSchemeKey(k), "", value, sizeof value, f);
        if (Rgb(value, &c)) {
            any = TRUE;
            ColorSchemeSetColor(&cs, k, c);
        }
    }
    if (any) {
        ColorSchemeFormat(&cs, value);
        ThemeSet(name, "Colors", value);
    }
    GetPrivateProfileString(DESKTOP_98, "Wallpaper", MISSING, value, sizeof value, f);
    if (lstrcmp(value, MISSING)) {
        Expand(value, dir, path);
        if (!*value || !lstrcmpi(value, "(None)") || Ext(path, ".BMP", ".RLE") || Ext(path, ".JPG", ".JPEG") ||
            Ext(path, ".PNG", NULL)) {
            ThemeSet(name, "Wallpaper", *value && lstrcmpi(value, "(None)") ? path : "");
            GetPrivateProfileString(DESKTOP_98, "TileWallpaper", "0", value, sizeof value, f);
            ThemeSet(name, "TileWallpaper", value);
        }
    }
    GetPrivateProfileString("boot", "SCRNSAVE.EXE", MISSING, value, sizeof value, f);
    if (lstrcmp(value, MISSING)) {
        Expand(value, dir, path);
        ThemeSet(name, "ScreenSaver", path);
    }
    for (i = 0; i < THEME_SOUNDS_N; i++) {
        wsprintf(sec, "AppEvents\\Schemes\\Apps\\%s\\%s\\.Current", (LPSTR)(i == THEME_SOUNDS_N - 1 ? "Explorer" : ".Default"),
                 (LPSTR)(i ? sounds[i].key : ".Default"));
        GetPrivateProfileString(sec, "DefaultValue", MISSING, value, sizeof value, f);
        if (lstrcmp(value, MISSING)) {
            Expand(value, dir, path);
            ThemeKey("Sound", sounds[i].key, key);
            ThemeSet(name, key, path);
        }
    }
    for (i = 0; i < CURSORS; i++) {
        GetPrivateProfileString(CURSORS_98, pointers[i].key, MISSING, value, sizeof value, f);
        Expand(value, dir, path);
        if (lstrcmp(value, MISSING) && (!*value || Ext(path, ".CUR", ".ANI"))) {
            ThemeKey("Pointer", pointers[i].key, key);
            ThemeSet(name, key, path);
        }
    }
    for (i = 0; i < THEME_ICONS_N; i++) {
        wsprintf(sec, "CLSID\\%s\\DefaultIcon", (LPSTR)icons[i].clsid);
        GetPrivateProfileString(sec, icons[i].value, MISSING, value, sizeof value, f);
        if (lstrcmp(value, MISSING)) {
            Expand(value, dir, path);
            ThemeKey("Icon", icons[i].key, key);
            ThemeSet(name, key, path);
        }
    }
    return TRUE;
}

/* A theme as a Windows 98 .Theme file (its parts only). */
BOOL ThemeExport(LPCSTR name, LPCSTR file)
{
    COLORSCHEME cs;
    COLORREF c;
    char f[MAX_PATH], sec[96], value[COLOR_TEXT + MAX_PATH], key[40];
    UINT parts = ThemeParts(name);
    HFILE h;
    int i, k;

    if ((h = DosCreate(file)) == HFILE_ERROR)   /* (made first: a long name has its alias then) */
        return FALSE;
    _lclose(h);
    DosShortPath(file, f);
    WritePrivateProfileString("MasterThemeSelector", "MTSM", "DABJDKT", f);
    if ((parts & THEME_COLORS) && ThemeGet(name, "Colors", value, sizeof value) && ColorSchemeParse(value, &cs))
        for (k = 0; k < COLOR_KEYS; k++)
            if (ColorSchemeChosen(&cs, k)) {
                c = ColorSchemeColor(&cs, k);
                wsprintf(value, "%d %d %d", GetRValue(c), GetGValue(c), GetBValue(c));
                WritePrivateProfileString(COLORS_98, ColorSchemeKey(k), value, f);
            }
    if (parts & THEME_WALLPAPER) {
        ThemeGet(name, "Wallpaper", value, sizeof value);
        WritePrivateProfileString(DESKTOP_98, "Wallpaper", *value ? value : "(None)", f);
        ThemeGet(name, "TileWallpaper", value, sizeof value);
        WritePrivateProfileString(DESKTOP_98, "TileWallpaper", value, f);
    }
    if (parts & THEME_SAVER) {
        ThemeGet(name, "ScreenSaver", value, sizeof value);
        WritePrivateProfileString("boot", "SCRNSAVE.EXE", value, f);
    }
    for (i = 0; (parts & THEME_SOUNDS) && i < THEME_SOUNDS_N; i++) {
        ThemeKey("Sound", sounds[i].key, key);
        if (!ThemeGet(name, key, value, sizeof value))
            continue;
        wsprintf(sec, "AppEvents\\Schemes\\Apps\\%s\\%s\\.Current", (LPSTR)(i == THEME_SOUNDS_N - 1 ? "Explorer" : ".Default"),
                 (LPSTR)(i ? sounds[i].key : ".Default"));
        WritePrivateProfileString(sec, "DefaultValue", value, f);
    }
    for (i = 0; (parts & THEME_POINTERS) && i < CURSORS; i++) {
        ThemeKey("Pointer", pointers[i].key, key);
        if (ThemeGet(name, key, value, sizeof value))
            WritePrivateProfileString(CURSORS_98, pointers[i].key, value, f);
    }
    for (i = 0; (parts & THEME_ICONS) && i < THEME_ICONS_N; i++) {
        ThemeKey("Icon", icons[i].key, key);
        if (!ThemeGet(name, key, value, sizeof value))
            continue;
        wsprintf(sec, "CLSID\\%s\\DefaultIcon", (LPSTR)icons[i].clsid);
        WritePrivateProfileString(sec, icons[i].value, value, f);
    }
    WritePrivateProfileString(NULL, NULL, NULL, f);     /* (Windows 3.1 writes its cache at exit) */
    return TRUE;
}
