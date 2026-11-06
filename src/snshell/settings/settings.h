/* settings\: property sheets and options dialogs. */
#ifndef SETTINGS_H
#define SETTINGS_H

/* ---- properties.c: Properties of files, folders, drives, My Computer ---- */
void PropertiesShow(HWND owner, PATHLIST *items, int kind);  /* IVK_FILE/FOLDER (mixed), DRIVE, COMPUTER */

/* ---- propertypages.c: its pages that read the system ---- */
#define VER_ITEMS       10              /* the Version page's list */
#define VER_TEXT        128
BOOL PropertyPagesVersion(HWND dlg, LPCSTR path, char (*ver)[VER_TEXT]);
BOOL PropertyPagesDrive(HWND dlg, int drive, DWORD *total, DWORD *free);
void PropertyPagesPie(HWND dlg, HDC dc, DWORD total, DWORD free);
void PropertyPagesSystem(HWND dlg);

/* ---- folderoptions.c ---- */
void FolderOptionsShow(HWND view);

/* ---- filetypespage.c: Folder Options' File Types page ---- */
void FileTypesPageInit(HWND page);
void FileTypesPageDone(void);                     /* the sheet closes */
void FileTypesPageDraw(const DRAWITEMSTRUCT FAR *d);
BOOL FileTypesPageCommand(HWND page, WPARAM wp, LPARAM lp);   /* its controls' */

/* ---- displayproperties.c: Display Properties ---- */
#define LIST_NONE       0xFFFFFFFFL     /* a list's item data: "(None)" */
void DisplayPropertiesShow(int page);             /* 0 Background, 1 Screen Saver, 2 Appearance, 3 Effects, 4 Settings */
/* ---- wallpaper.c ---- */
void WallpaperStyles(HWND combo);                 /* the styles, in the order of WP_ */
int  WallpaperNow(LPSTR path);                    /* the picture chosen ("": none) and its style */
BOOL WallpaperApply(HWND owner, LPCSTR path, int style);  /* FALSE: stopped, or it could not be done (said) */
int  WallpaperPreview(LPCSTR path, int style, int width, IMAGEPROGRESS progress, DIB *d);
void WallpaperShown(LPSTR out, int size);         /* the file shown, as WIN.INI would name it */
BOOL WallpaperTiled(void);
BOOL WallpaperMakeLow(LPCSTR source, LPCSTR target);  /* wallpaperlow.c: 256 colours */
DWORD WallpaperMemoryKB(void);
BOOL WallpaperYield(int percent);                 /* an IMAGEPROGRESS: other programs go on */
void WallpaperStart(void);                        /* at start: taken over, shown; made again if the screen changed */

void DisplayPropertiesDesktop(HDC dc, const RECT *s, COLORREF desktop, const DIB *wall, BOOL tile);  /* small */
void DisplayPropertiesSavers(HWND combo, PATHLIST *l);    /* "(None)" (LIST_NONE) and each, data: its place in l */

/* ---- colorscheme.c: a colour scheme, Windows 3.1's colours and Windows 98's ---- */
#define COLOR_KEYS_31   21              /* WIN.INI [colors]' keys, in its order */
#define COLOR_KEYS      (COLOR_KEYS_31 + SC_COUNT)  /* then Windows 98's (SC_*) */
#define COLOR_TEXT      (COLOR_KEYS * 9)    /* a scheme as text, at most */
typedef struct {
    COLORREF colors[COLOR_KEYS_31];     /* by COLOR_* */
    COLORREF extra[SC_COUNT];           /* by SC_*, */
    BOOL     chosen[SC_COUNT];          /* when not made from the others */
} COLORSCHEME;
LPCSTR   ColorSchemeKey(int k);                   /* k-th: WIN.INI [colors]' key */
LPCSTR   ColorSchemeName(int k);                  /* what it colours */
COLORREF ColorSchemeColor(const COLORSCHEME *s, int k);
void     ColorSchemeSetColor(COLORSCHEME *s, int k, COLORREF c);
BOOL     ColorSchemeChosen(const COLORSCHEME *s, int k);  /* the scheme's own, not made from the others */
BOOL     ColorSchemeParse(LPCSTR text, COLORSCHEME *s);
void     ColorSchemeFormat(const COLORSCHEME *s, LPSTR out);  /* COLOR_TEXT */
void     ColorSchemeCurrent(COLORSCHEME *s);      /* in use */
void     ColorSchemeApply(const COLORSCHEME *s, LPCSTR name);
void     ColorSchemePicture(HDC dc, const RECT *a, const COLORSCHEME *s, BOOL desktop);

/* ---- appearance.c: its Appearance page ---- */
void AppearanceInit(HWND dlg);
BOOL AppearanceCommand(HWND dlg, UINT id, UINT code);     /* TRUE: the colours changed */
BOOL AppearanceDrawItem(HWND dlg, const DRAWITEMSTRUCT FAR *d);
void AppearancePaint(HWND dlg, HDC dc);
void ExtendersInit(HWND dlg);
BOOL ExtendersApply(HWND dlg);
void AppearanceApply(HWND dlg);

/* ---- schemes.c: Snowball's colour schemes ---- */
LPCSTR SchemesName(int i);                        /* NULL past the last */
LPCSTR SchemesGroup(int i);
LPCSTR SchemesColors(LPCSTR name);                /* colorscheme.c's form; NULL: not one */
BOOL   SchemesOld(LPCSTR name);                   /* one of Windows 3.1's, not offered */

/* ---- themes.c: themes, THEMES.INI ---- */
#define THEME_COLORS    0x01            /* the parts a theme may set */
#define THEME_WALLPAPER 0x02
#define THEME_SAVER     0x04
#define THEME_SOUNDS    0x08
#define THEME_POINTERS  0x10
#define THEME_ICONS     0x20
#define THEME_PARTS     6
#define THEME_NAME      64
#define THEME_GROUP     32
#define THEME_SOUNDS_N  15              /* sound events */
#define THEME_ICONS_N   4               /* desktop icons: My Computer, My Documents, Recycle Bin empty, full */
LPCSTR ThemeSoundKey(int i);                      /* WIN.INI [sounds]' */
LPCSTR ThemeSoundName(int i);
LPCSTR ThemePointerKey(int i);                    /* by CURSOR_* */
LPCSTR ThemePointerName(int i);
LPCSTR ThemeIconKey(int i);
LPCSTR ThemeIconName(int i);
void   ThemeKey(LPCSTR kind, LPCSTR key, LPSTR out);   /* "Sound Open": a part's key in a theme */
BOOL   ThemeGet(LPCSTR name, LPCSTR key, LPSTR out, int size);    /* FALSE: not set */
void   ThemeSet(LPCSTR name, LPCSTR key, LPCSTR value);           /* NULL: taken out */
int    ThemeWallpaperStyle(LPCSTR name);          /* WP_ */
void   ThemeSetWallpaperStyle(LPCSTR name, int style);
BOOL   ThemeGroup(LPCSTR name, LPSTR out);        /* THEME_GROUP; FALSE: no such theme */
BOOL   ThemeExists(LPCSTR name);
void   ThemeSetGroup(LPCSTR name, LPCSTR group);  /* "": Custom */
BOOL   ThemeAdd(LPCSTR name, LPCSTR group);       /* empty; FALSE: one has that name */
void   ThemeDelete(LPCSTR name);
UINT   ThemeParts(LPCSTR name);                   /* THEME_* */
void   ThemeDrop(LPCSTR name, UINT part);         /* one THEME_* taken out */
BOOL   ThemeCopy(LPCSTR from, LPCSTR to);
BOOL   ThemeRename(LPCSTR from, LPCSTR to);
int    ThemesList(PATHLIST *names, PATHLIST *groups);  /* by group, then name */
void   ThemesRestoreDefaults(void);               /* Snowball's as they came */
void   ThemesPrepare(void);                       /* Snowball's, the first time */
void   ThemesStartup(void);                       /* the pointers chosen */
void   ThemeFromCurrent(LPCSTR name);             /* every part as in use now */
void   ThemeApply(LPCSTR name, UINT parts);
BOOL   ThemeImport(LPCSTR file, LPSTR name);      /* a Windows 98 .Theme; name: THEME_NAME out */
BOOL   ThemeExport(LPCSTR name, LPCSTR file);

/* ---- thememanager.c, themepages.c: the Theme Manager ---- */
void   ThemeManagerShow(void);
int    ThemeManagerIconIndex(LPSTR value);       /* "file,index": index, the file left */
void   ThemePagesInit(HWND dlg);
void   ThemePagesLoad(HWND dlg, LPCSTR name);     /* the theme chosen */
BOOL   ThemePagesCommand(HWND dlg, LPCSTR name, UINT id, UINT code);  /* TRUE: the theme changed */
BOOL   ThemePagesMeasure(MEASUREITEMSTRUCT FAR *m);
BOOL   ThemePagesDraw(const DRAWITEMSTRUCT FAR *d, LPCSTR name);
void   ThemePagesDone(void);

/* ---- taskbarproperties.c: Taskbar Properties, the Start menu's editing ---- */
void TaskbarPropertiesShow(void);

/* ---- startmenusize.c: Customize Start Menu (the two-column one's size) ---- */
void StartMenuSizeShow(HWND owner);

/* ---- datetime.c: Date/Time Properties ---- */
void DateTimeShow(void);

/* ---- binproperties.c: Recycle Bin Properties ---- */
void RecycleBinPropertiesShow(HWND owner);

#endif
