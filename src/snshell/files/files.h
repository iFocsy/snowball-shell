/* files\: what happens to files - operations, the Recycle Bin, the clipboard. */
#ifndef FILES_H
#define FILES_H

/* ---- fileoperation.c: copy, move and delete whole trees, with progress ---- */
#define FOP_COPY        0
#define FOP_MOVE        1
#define FOP_DELETE      2               /* to the Recycle Bin where the drive has one */
#define FOP_PURGE       3               /* for good */
BOOL FileOperationRun(HWND owner, int op, PATHLIST *files, LPCSTR destDir);
BOOL FileOperationBusy(void);
void FileOperationError(HWND owner, LPCSTR verb, LPCSTR path, WORD code);  /* "Cannot copy X: ..." */
void FileOperationTotals(LPCSTR path, DWORD *bytes, WORD *files, WORD *folders);  /* adds a whole tree */

/* ---- progress.c: the progress window ---- */
void ProgressOpen(HWND owner, int op);            /* shown once the work takes a while */
void ProgressItem(LPCSTR path, LPCSTR destDir);
void ProgressStep(DWORD done, DWORD total);
BOOL ProgressCancelled(void);                     /* keeps Windows running meanwhile */
void ProgressClose(void);
HWND ProgressWindow(void);                        /* NULL while not shown */

/* ---- confirmreplace.c: Confirm File Replace ---- */
int  ConfirmReplaceAsk(HWND owner, LPCSTR target, const DOSENTRY *existing, const DOSENTRY *with);
                                                  /* IDYES, IDC_YESALL, IDNO or IDCANCEL */

/* ---- recyclebin.c: a RECYCLED folder per fixed disk, listed in INFO.INI ---- */
#define BIN_PATH        "::Bin"         /* folder windows show the bin for this path */
BOOL RecycleBinCovers(LPCSTR path);               /* deleting it goes to a bin */
BOOL RecycleBinReserve(LPCSTR path, LPSTR stored);    /* the name it gets in the bin */
void RecycleBinRecord(LPCSTR stored, LPCSTR original);   /* after it was moved there */
BOOL RecycleBinIsEmpty(void);
void RecycleBinLoad(HWND iv);                     /* items with their original names */
void RecycleBinRestore(HWND owner, PATHLIST *stored);
void RecycleBinPurge(HWND owner, PATHLIST *stored);   /* delete for good, with confirmation */
void RecycleBinEmptyAll(HWND owner);
void RecycleBinTrim(HWND owner);

/* ---- undo.c: Edit > Undo, the last file operations ---- */
#define UNDO_RENAME     1
#define UNDO_MOVE       2
#define UNDO_COPY       3
#define UNDO_DELETE     4               /* to the Recycle Bin */
void UndoBegin(int what);
void UndoPair(LPCSTR before, LPCSTR after);       /* one item's paths */
void UndoEnd(void);
BOOL UndoMenuText(LPSTR out);                     /* "&Undo Delete\tCtrl+Z"; FALSE: nothing */
void UndoLast(HWND owner);                  /* the oldest out while a bin is past its size */

/* ---- hostclipboard.c: text on the clipboard shared with DOSBox-X's host ---- */
void HostClipboardStart(void);                    /* nothing without DOSBox-X's CLIP$ */

/* ---- clipboard.c: Cut, Copy and Paste of files ---- */
void ClipboardPut(HWND owner, PATHLIST *files, BOOL cut);
BOOL ClipboardGet(HWND owner, PATHLIST *files, BOOL *cut);  /* FALSE if no files */
BOOL ClipboardHasFiles(void);
void ClipboardClear(HWND owner);

#endif
