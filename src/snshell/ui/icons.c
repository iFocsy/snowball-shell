/*
 * Icon cache. Extracted icons live until Snowball exits, so windows can keep
 * the handles. The shell's own icons bring Windows 98's 16x16 images (and
 * 256 colour ones); other icons get 16x16 copies made once on first use
 * (SNSYSTEM.DLL, draw.c).
 */
#include "internal.h"
#include <shellapi.h>

#define MAX_ICONS   160

static struct { DWORD hash; HICON icon; BOOL document; } icons[MAX_ICONS];
static int nIcons;

static DWORD Hash(LPCSTR s)
{
    DWORD h = 5381;
    for (; *s; s++)
        h = h * 33 + (BYTE)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)*s);
    return h;
}

static HICON Lookup(DWORD h)
{
    int i;
    for (i = 0; i < nIcons; i++)
        if (icons[i].hash == h)
            return icons[i].icon;
    return NULL;
}

static HICON Remember(DWORD h, HICON icon)
{
    if (nIcons < MAX_ICONS) {
        icons[nIcons].hash = h;
        icons[nIcons].document = FALSE;
        icons[nIcons++].icon = icon;
    }
    return icon;
}

/* A type's icon, kept by its extension until the file types change. */
static HICON RememberDocument(DWORD h, HICON icon)
{
    Remember(h, icon);
    if (icons[nIcons - 1].hash == h)
        icons[nIcons - 1].document = TRUE;
    return icon;
}

/* The handles stay valid (windows may hold them): only forgotten. */
void IconsForgetDocuments(void)
{
    int i = 0;

    while (i < nIcons)
        if (icons[i].document)
            icons[i] = icons[--nIcons];
        else
            i++;
}

/* Its icons are numbered in id order from 0, as SNOWBALL.EXE's were before
   they moved there. */
void IconsArtFile(LPSTR path)
{
    lstrcpy(path, g_home);
    lstrcat(path, "W98ART.DLL");
}

void IconsForget(LPCSTR key)
{
    DWORD h = Hash(key);
    int i;

    for (i = 0; i < nIcons; i++)
        if (icons[i].hash == h) {
            icons[i] = icons[--nIcons];
            return;
        }
}

/* The desktop's icons a theme gives (SNOWBALL.INI [Icons] key=file,index),
   read once; those of an earlier theme are kept: windows may show them yet. */
static const struct { int id; LPCSTR key; } themed[] = {
    { IDI_COMPUTER, "Computer" }, { IDI_MYDOCS, "Documents" }, { IDI_BIN, "BinEmpty" }, { IDI_BINFULL, "BinFull" },
};
#define NTHEMED     (sizeof themed / sizeof themed[0])
static HICON own[NTHEMED];
static BOOL ownRead;

void IconsThemeChanged(void)
{
    ownRead = FALSE;
}

static HICON Themed(int id)
{
    char value[MAX_PATH + 8];
    int i, index;

    if (!ownRead) {
        ownRead = TRUE;
        for (i = 0; i < NTHEMED; i++) {
            own[i] = NULL;
            SettingsGetString("Icons", themed[i].key, "", value, sizeof value);
            if (*value) {
                index = ThemeManagerIconIndex(value);
                DosShortPath(value, value);
                if ((UINT)(own[i] = ExtractIcon(g_hInst, value, index)) <= 1)
                    own[i] = NULL;
            }
        }
    }
    for (i = 0; i < NTHEMED; i++)
        if (themed[i].id == id)
            return own[i];
    return NULL;
}

HICON IconStock(int id)
{
    static BYTE imaged[IDI_ART_LAST / 8 + 1];    /* a bit each: its own images registered */
    HICON icon = Themed(id);

    if (icon)
        return icon;
    icon = LoadIcon(g_hArt, MAKEINTRESOURCE(id));

    if (icon && id <= IDI_ART_LAST && !(imaged[id / 8] & (1 << (id % 8))) &&
        DrawIconImages(icon, g_hArt, MAKEINTRESOURCE(id)))
        imaged[id / 8] |= (BYTE)(1 << (id % 8));
    return icon;
}

/* Icon number index of a program or icon file; NULL if it has none. */
static HICON Extract(LPCSTR file, int index)
{
    char shortPath[MAX_PATH];
    HICON h;

    if (nIcons == MAX_ICONS)            /* full: an icon not kept would never be freed (a stock one stands in) */
        return NULL;
    DosShortPath(file, shortPath);
    h = ExtractIcon(g_hInst, shortPath, index);
    return (UINT)h > 1 ? h : NULL;
}

HICON IconForFile(LPCSTR path, BOOL isDir)
{
    char ext[10], key[MAX_PATH + 2], shortPath[MAX_PATH], exe[MAX_PATH];
    HICON h;
    DWORD hash;
    int index;

    if (isDir) {                        /* its desktop.ini may give its own */
        if ((h = Lookup(Hash(path))) != NULL)
            return h;
        if (FolderIniIcon(path, exe, &index) && (h = Extract(exe, index)) != NULL)
            return Remember(Hash(path), h);
        return IconStock(lstrcmpi(path, DesktopMyDocuments()) ? IDI_FOLDER : IDI_MYDOCS);
    }
    lstrcpyn(ext, PathExtension((LPSTR)path), sizeof ext);
    AnsiUpper(ext);
    if (!lstrcmp(ext, SHC_EXT))
        return IconForShortcut(path);
    if (!lstrcmp(ext, DESKLINK_EXT))
        return IconStock(IDI_SHOWDESKTOP);
    if (!lstrcmp(ext, ".COM") || !lstrcmp(ext, ".BAT") || !lstrcmp(ext, ".PIF"))
        return IconStock(IDI_PROGRAM);

    /* programs and icon files are keyed by path, documents by extension */
    if (!lstrcmp(ext, ".EXE") || !lstrcmp(ext, ".ICO"))
        lstrcpy(key, path);
    else
        wsprintf(key, "*%s", (LPSTR)ext);
    hash = Hash(key);
    if ((h = Lookup(hash)) != NULL)
        return h;
    if (key[0] != '*') {
        h = Extract(path, 0);
        return h ? Remember(hash, h) : IconStock(IDI_PROGRAM);
    }
    if (*ext && FileTypeIcon(ext + 1, exe, &index) && (h = Extract(exe, index)) != NULL)   /* (no extension: none) */
        return RememberDocument(hash, h);
    DosShortPath(path, shortPath);
    if ((UINT)FindExecutable(shortPath, NULL, exe) > 32 && (h = Extract(exe, 0)) != NULL)
        return RememberDocument(hash, h);
    return RememberDocument(hash, IconStock(IDI_FILE));
}

HICON IconForShortcut(LPCSTR shcPath)
{
    SHORTCUT s;
    DWORD hash = Hash(shcPath);
    HICON h;

    if ((h = Lookup(hash)) != NULL)
        return h;
    if (!ShortcutRead(shcPath, &s))
        return IconStock(IDI_FILE);
    if (s.icon[0] && (h = Extract(s.icon, s.iconIndex)) != NULL)
        return Remember(hash, h);
    if (lstrlen(s.target) == 3 && s.target[1] == ':')
        return Remember(hash, IconForDrive((s.target[0] | 0x20) - 'a'));
    return Remember(hash, IconForFile(s.target, PathIsFolder(s.target)));
}

static BOOL IsCdRom(int drive)
{
    WORD found = 0, cd = 0;
    _asm {
        mov ax, 1500h
        xor bx, bx
        int 2Fh
        mov found, bx
    }
    if (!found)
        return FALSE;
    _asm {
        mov ax, 150Bh
        mov cx, drive
        int 2Fh
        mov cd, ax
    }
    return cd != 0;
}

HICON IconForDrive(int drive)
{
    switch (GetDriveType(drive)) {
    case DRIVE_REMOVABLE: return IconStock(IDI_FLOPPY);
    case DRIVE_REMOTE:    return IconStock(IsCdRom(drive) ? IDI_CDROM : IDI_NETWORK);
    }
    return IconStock(IsCdRom(drive) ? IDI_CDROM : IDI_HARDDISK);
}
