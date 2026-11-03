/* core\: no windows of its own. Settings, paths, files, shortcuts, launching. */
#ifndef CORE_H
#define CORE_H

/* ---- settings.c: SNOWBALL.INI ---- */
extern char g_home[MAX_PATH];           /* SNOWBALL.EXE's folder, ends in '\' */
extern char g_ini[MAX_PATH];            /* SNOWBALL.INI there */
int  SettingsGetInt(LPCSTR section, LPCSTR key, int def);
void SettingsSetInt(LPCSTR section, LPCSTR key, int value);
void SettingsGetString(LPCSTR section, LPCSTR key, LPCSTR def, LPSTR buf, int size);

/* ---- viewoptions.c: Folder Options, kept in SNOWBALL.INI [Folders] ---- */
typedef struct {
    BOOL fullPath;                      /* the whole path in a folder's title */
    BOOL hideExt;                       /* known file types shown without extension */
    BOOL allUpper;                      /* "README.TXT" as it is (else "Readme.txt") */
    int  hidden;                        /* VIEW_*: which files stay out of view */
    BOOL ownWindows;                    /* each folder opens in its own window (Windows 95) */
    BOOL infoTips;                      /* an item's description when the mouse rests on it */
} VIEWOPTIONS;
#define VIEW_NO_HIDDEN_SYSTEM   0
#define VIEW_NO_HIDDEN          1
#define VIEW_ALL                2
extern VIEWOPTIONS g_viewOptions;
void ViewOptionsLoad(void);
void ViewOptionsSave(void);
BOOL ViewOptionsHides(WORD attr);                 /* FA_* of a file: not listed */

/* ---- errorbox.c ---- */
void ErrorBox(HWND owner, LPCSTR text);           /* Snowball's message, with the exclamation mark */

/* ---- memory.c: blocks that windows keep by their 16-bit offset; growing ---- */
void *LocalHeapAlloc(UINT size);                  /* zeroed; NULL if there is no room */
void  LocalHeapFree(void *block);                 /* NULL is ignored */
WORD  LocalHeapOffset(void *block);               /* for a window word or property */
void *LocalHeapPointer(WORD offset);              /* and back; 0 gives NULL */
BOOL  GlobalGrow(HGLOBAL *h, void far **p, DWORD size);  /* a locked block bigger */

/* ---- path.c: path strings ---- */
LPSTR PathFileName(LPSTR path);                   /* file part */
LPSTR PathExtension(LPSTR path);                  /* ".ext" or "" */
void  PathAppend(LPSTR dir, LPCSTR name);         /* dir\name */
void  PathToParent(LPSTR path);                   /* "C:\A\B" -> "C:\A", "C:\A" -> "C:\" */
BOOL  PathIsInside(LPCSTR path, LPCSTR dir);      /* path is dir or below it */
BOOL  PathIsFolder(LPCSTR path);
BOOL  PathOnDisk(LPCSTR path);                    /* "X:..." - not My Computer or the bin */
BOOL  PathMatchesPattern(LPCSTR name, LPCSTR patterns);  /* "*.txt;*.doc": * and ?, case ignored */
BOOL  PathIsShortName(LPCSTR name);               /* a DOS name: 8 + 3, one dot, no spaces */
BOOL  PathMakeUnique(LPSTR out, LPCSTR dir, LPCSTR base, LPCSTR ext);  /* "base ext", "base (2)ext"... */
BOOL  PathIsShortcut(LPCSTR path);                /* .SHC */
#define DESKLINK_EXT    ".DESKLINK"
BOOL  PathIsDeskLink(LPCSTR path);                /* Send To's Desktop as Shortcut */
BOOL  PathIsProgram(LPCSTR path);                 /* .EXE, .COM, .BAT, .PIF */

/* ---- pathlist.c: paths one after the other, each ending in '\0' ---- */
typedef struct {
    HGLOBAL mem;
    WORD    used, cap;
    int     count;
} PATHLIST;
void  PathListInit(PATHLIST *l);
BOOL  PathListAdd(PATHLIST *l, LPCSTR path);
LPSTR PathListLock(PATHLIST *l);                  /* walk with PathListNext until "" */
LPSTR PathListNext(LPSTR p);
void  PathListUnlock(PATHLIST *l);
void  PathListFree(PATHLIST *l);

/* ---- sort.c ---- */
void  SortRecords(void far *base, int n, int size, int (*cmp)(const void far *, const void far *));

/* ---- format.c: sizes, dates and types as Windows 98 shows them ---- */
void FormatSize(DWORD bytes, LPSTR out);          /* "512 bytes", "1.23 KB", "4.5 MB" */
void FormatDateTime(WORD date, WORD time, LPSTR out);  /* DOS date/time: yyyy/MM/dd HH:mm:ss */
void FormatLongDateTime(WORD date, WORD time, BOOL withTime, LPSTR out);
                                                  /* "Monday, October 26, 2026 6:58:12 PM" */
void FormatBytes(DWORD bytes, LPSTR out);         /* "1.23 KB (1,264 bytes)" */
void FormatNumber(DWORD n, LPSTR out);            /* "1,234,567" */
void FormatKB(DWORD bytes, LPSTR out);            /* "1KB", "1,234KB": the Details column */
void FormatFileType(LPCSTR name, LPSTR out);      /* "Text Document", "BAK File"... */
BOOL FormatDescription(LPCSTR path, LPSTR out, int size);  /* a program's or library's FileDescription */
void FormatForgetTypes(void);                     /* the file types changed */

/* ---- filetypes.c: file types (REG.DAT and WIN.INI [Extensions]); ext without the dot ---- */
#define FT_ACTIONS  8
typedef struct {                /* a type in the File Types list */
    char cls[40];               /* its class, "" if only WIN.INI knows it */
    char exts[40];              /* its extensions, "TXT INI" */
} FILETYPEID;
typedef struct {                /* a type being edited */
    char cls[40];
    char exts[40];
    char desc[64], origDesc[64];
    char icon[MAX_PATH];        /* DefaultIcon, "" for the program's */
    int  iconIndex;
    char defAction[16];
    int  nActions;
    struct { char name[16]; char cmd[128]; } actions[FT_ACTIONS];
    BOOL showExt;               /* AlwaysShowExt */
} FILETYPE;
BOOL FileTypeClass(LPCSTR ext, LPSTR cls, int size);
BOOL FileTypeCommand(LPCSTR cls, LPCSTR action, LPSTR cmd, int size);  /* "notepad.exe %1" */
void FileTypeDefaultAction(LPCSTR cls, LPSTR action, int size);       /* "open" unless set */
BOOL FileTypeOpenCommand(LPCSTR ext, LPSTR cmd, int size);  /* its default action's, or WIN.INI's */
int  FileTypeActions(LPCSTR ext, char FAR names[][16], int max);  /* the default first; count */
BOOL FileTypeProgram(LPCSTR cmd, LPSTR program);  /* a command's program, found: its path */
BOOL FileTypeIcon(LPCSTR ext, LPSTR file, int *index);  /* DefaultIcon, else the program's */
BOOL FileTypeAlwaysShowExt(LPCSTR ext);
void FileTypeAssociate(LPCSTR ext, LPCSTR program, LPCSTR desc);  /* Open With's "Always use" */
int  FileTypesList(FILETYPEID FAR *ids, int max);
void FileTypeFirstExt(LPCSTR exts, LPSTR ext);
void FileTypeLoad(const FILETYPEID FAR *id, FILETYPE FAR *t);
void FileTypeSave(FILETYPE FAR *t);               /* a new type (cls "") gets its class */
void FileTypeRemove(const FILETYPE FAR *t);

/* ---- dos.c: DOS files and folders, with long names when available ---- */
typedef struct {
    char  name[MAX_PATH];       /* long name (ANSI) */
    DWORD size;
    WORD  attr, date, time;     /* date and time last written */
    WORD  cdate, ctime, adate;  /* created, accessed (long names only, else = written) */
} DOSENTRY;
typedef struct {
    WORD handle;
    BOOL lfn;
    BYTE dta[43];
} DOSFIND;
#define FA_READONLY 0x01
#define FA_HIDDEN   0x02
#define FA_SYSTEM   0x04
#define FA_LABEL    0x08
#define FA_DIR      0x10
#define FA_ARCHIVE  0x20
#define BIN_FOLDER  "RECYCLED"          /* the Recycle Bin's folder in each disk's root */
BOOL  DosInit(void);                              /* TRUE if long names work */
BOOL  DosHasLongNames(void);
BOOL  DosFindFirst(LPCSTR spec, DOSFIND *f, DOSENTRY *e);  /* skips "." and ".." */
BOOL  DosFindNext(DOSFIND *f, DOSENTRY *e);
void  DosFindClose(DOSFIND *f);
BOOL  DosHasSubfolders(LPCSTR dir);               /* any (visible) folder inside */
BOOL  DosGetEntry(LPCSTR path, DOSENTRY *e);      /* one file or folder */
BOOL  DosShortPath(LPCSTR path, LPSTR out);       /* 8.3 path for old APIs */
BOOL  DosLongPath(LPCSTR path, LPSTR out);        /* the long names of an 8.3 path */
BOOL  DosMakeFolder(LPCSTR path);
BOOL  DosRemoveFolder(LPCSTR path);
BOOL  DosChangeFolder(LPCSTR dir);                /* drive and folder */
BOOL  DosRename(LPCSTR from, LPCSTR to);          /* also moves within a drive */
BOOL  DosDelete(LPCSTR path);
WORD  DosGetAttributes(LPCSTR path);              /* 0xFFFF if missing */
BOOL  DosSetAttributes(LPCSTR path, WORD attr);
HFILE DosOpenRead(LPCSTR path);                   /* HFILE_ERROR on failure */
BOOL  DosIsDevice(HFILE h);                       /* CON, CLIP$..., not a file */
HFILE DosCreate(LPCSTR path);                     /* new or truncated, write only */
BOOL  DosGetFileTime(HFILE h, WORD *date, WORD *time);
BOOL  DosSetFileTime(HFILE h, WORD date, WORD time);
BOOL  DosCreateTextFile(LPCSTR path, LPCSTR text);    /* new file with this content */
WORD  DosLastError(void);                         /* DOS error of the last failed call */
DWORD DosNow(void);                               /* MAKELONG(time, date) */
BOOL  DosDiskSpace(int drive, DWORD *total, DWORD *free);  /* bytes; 0 = A: */
BOOL  DosVolumeLabel(int drive, LPSTR out);       /* "" when the disk has none */
BOOL  DosSetVolumeLabel(int drive, LPCSTR label);  /* "" removes it */

/* ---- shortcut.c: .SHC shortcuts, small INI-style files ---- */
typedef struct {
    char target[MAX_PATH];
    char args[128];
    char dir[MAX_PATH];
    char icon[MAX_PATH];
    int  iconIndex;
    int  show;
    WORD hotkey;                        /* shortcut key: virtual key | SK_* << 8, 0 none */
} SHORTCUT;
#define SHC_EXT ".SHC"
BOOL ShortcutRead(LPCSTR path, SHORTCUT *s);
BOOL ShortcutWrite(LPCSTR path, const SHORTCUT *s);
BOOL ShortcutCreate(LPCSTR target, LPCSTR dir, BOOL named);   /* "Shortcut to X" unless named */

/* ---- displayname.c: names as folders show them (Folder Options) ---- */
void DisplayNameOfFile(LPCSTR fileName, LPSTR out);
void DisplayNameOfFolder(LPCSTR name, LPSTR out);
BOOL DisplayNameHidesExtension(LPCSTR fileName);

/* ---- launch.c: opening folders, shortcuts, programs and documents ---- */
BOOL LaunchItem(HWND owner, LPCSTR path);
BOOL LaunchWith(HWND owner, LPCSTR program, LPCSTR args);     /* program or shortcut to one */
BOOL LaunchAction(HWND owner, LPCSTR file, LPCSTR action);    /* a file type's action: "print" */
void LaunchOpenRequest(HWND owner);               /* OPEN.COM's, from a DOS box, if there is one */
BOOL LaunchCommand(HWND owner, LPCSTR cmdline, LPCSTR dir, int show);
void LaunchSplitCommand(LPCSTR cmdline, LPSTR prog, LPSTR args, int argsSize);  /* "prog" args */
BOOL LaunchIsProgram(LPCSTR path);                /* a program, or a shortcut to one */

/* ---- sounds.c: Windows 98's sound events, in WIN.INI [sounds] ---- */
void SoundsRegister(void);                        /* Control Panel's Sound lists them */
void SoundPlay(int event);                        /* SOUND_*, if a sound is set for it */

/* folderini.c: a folder's desktop.ini */
int  FolderIniRead(LPCSTR folder, LPCSTR key, LPSTR out, int size);  /* "InfoTip"...: its length */
BOOL FolderIniIcon(LPCSTR folder, LPSTR file, int *index);  /* its own icon, if set */
BOOL FolderIniWrite(LPCSTR folder, LPCSTR iconFile, int iconIndex, LPCSTR tip);  /* "": none */

/* ---- recent.c: Start > Documents, the documents used last ---- */
LPSTR RecentFolder(LPSTR out);                    /* ...\Recent; returns out */
void  RecentAdd(LPCSTR document);
void  RecentClear(void);
BOOL  RecentIsEmpty(void);

#endif
