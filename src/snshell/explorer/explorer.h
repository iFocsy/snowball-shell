/* explorer\: the desktop, folder and Explorer windows, and what they do with files. */
#ifndef EXPLORER_H
#define EXPLORER_H

/* ---- desktop.c ---- */
#define ID_VIEW         1               /* the icon view inside the desktop and folder windows */
BOOL DesktopCreate(void);
void DesktopSetArea(const RECT *r);              /* where its icons go: beside the taskbar */
void DesktopFolder(LPSTR out);                    /* the Desktop folder */
LPCSTR DesktopMyDocuments(void);                  /* My Documents (made if missing) */
BOOL DesktopIsMyDocuments(HWND iv, int item);     /* the desktop's My Documents item */
void DesktopRepaint(void);                        /* the wallpaper changed */

/* ---- folderwindow.c: folder and Explorer windows ---- */
void FolderWindowOpen(LPCSTR path);               /* "" = My Computer, BIN_PATH */
void FolderWindowExplore(LPCSTR path);            /* the same with the folder tree */
void FolderWindowSelect(LPCSTR path);             /* its folder's window, with it selected */
BOOL FolderWindowRegister(void);

/* ---- filecommands.c: file commands, menus, keys and drops of both ---- */
#define CMD_OPEN        100
#define CMD_CUT         101
#define CMD_COPY        102
#define CMD_PASTE       103
#define CMD_PASTELINK   104
#define CMD_LINK        105             /* Create Shortcut */
#define CMD_DELETE      106
#define CMD_RENAME      107
#define CMD_NEWFOLDER   108
#define CMD_NEWTEXT     109
#define CMD_SELECTALL   110
#define CMD_INVERT      111
#define CMD_REFRESH     112
#define CMD_RESTORE     113             /* Recycle Bin */
#define CMD_EMPTYBIN    114
#define CMD_PROPERTIES  115
#define CMD_UP          116
#define CMD_CLOSE       117
#define CMD_LARGE       118             /* + IVM_*: View > Large Icons ... Details */
#define CMD_DETAILS     121
#define CMD_BYNAME      122             /* + IVSORT_*: Arrange Icons > by Name ... by Date */
#define CMD_BYDATE      125
#define CMD_EXPLORE     126
#define CMD_GOTO        127
#define CMD_FIND        128
#define CMD_FOLDEROPTS  129
#define CMD_BACK        130             /* folder windows' own */
#define CMD_FORWARD     131
#define CMD_VIEWS       132             /* the next view; from its arrow, the list */
#define CMD_SHOWTOOLS   133             /* View > Toolbars */
#define CMD_SHOWADDRESS 134
#define CMD_SHOWLABELS  135
#define CMD_FOLDERS     136             /* View > Explorer Bar > Folders: the tree */
#define CMD_NOBAR       137             /* ... > None */
#define CMD_OPENWITH    138
#define CMD_SELECTFILES 139             /* Edit > Select Files: by a pattern (File Manager's) */
#define CMD_FILTER      140             /* View > Filter: only the files of a pattern (Calmira's) */
#define CMD_UNDO        141             /* Edit > Undo: the last file operation */
#define CMD_QUICKVIEW   142             /* a text or BMP file looked at */
#define CMD_THUMBS      143             /* View > Thumbnails (CMD_LARGE + IVM_THUMBS is taken) */
#define CMD_COMPACT     144             /* View > Toolbars > Compact */
#define CMD_ABOUT       145             /* Help > About Snowball */
#define CMD_ACTION      0x0F00          /* + n: a document type's action n (Print...) */
void  FileCommandsContextMenu(HWND owner, HWND iv);
UINT  FileCommandsItemMenu(HWND owner, LPCSTR path, BOOL sortable, LPSTR renamed);
UINT  FileCommandsKindMenu(HWND owner, LPCSTR path, int kind, LPSTR renamed);  /* of that IVK_* */
                                                  /* one item outside the views (Start menu): the command */
BOOL  FileCommandsRun(HWND owner, HWND iv, UINT cmd);
BOOL  FileCommandsKey(HWND owner, HWND iv, WPARAM key);   /* Delete, F2, Ctrl+C... */
void  FileCommandsInitMenu(HMENU menu, HWND iv);  /* enables the items that apply */
BOOL  FileCommandsAvailable(HWND iv, UINT cmd);   /* it applies now (menus, toolbars) */
HMENU FileCommandsFileMenu(BOOL bin);
HMENU FileCommandsEditMenu(void);
HMENU FileCommandsViewMenu(void);
HMENU FileCommandsModesMenu(HMENU m, int checked);  /* Large Icons... Thumbnails; -1 none checked */
UINT  FileCommandsModeCommand(int mode);          /* IVM_* -> its View command */
void  FileCommandsLoadView(HWND iv);              /* the view and order last chosen */
void  FileCommandsSaveView(HWND iv);
BOOL  FileCommandsRenamed(HWND owner, HWND iv);   /* IVN_RENAME */
int   FileCommandsRename(HWND owner, LPCSTR from, LPCSTR name, LPSTR to);  /* 1 done, 0 error, -1 kept */
void  FileCommandsDelete(HWND owner, PATHLIST *l, BOOL purge);  /* asks, then to the bin (or for good) */
void  FileCommandsFolderChanged(LPCSTR dir);      /* refresh every window showing dir; NULL: all */
/* Drops: on an item (folder, drive, program, Recycle Bin) or the view (-1) */
#define DE_NONE         0
#define DE_MOVE         1
#define DE_COPY         2
#define DE_LINK         3
#define DE_RUN          4               /* open the program with the files */
int   FileCommandsDropItem(HWND iv, int x, int y);    /* documents are not targets: -1 */
int   FileCommandsDropTarget(HWND iv, int item, LPSTR target);   /* -> path and IVK_* kind */
int   FileCommandsDropEffect(LPCSTR target, int kind, PATHLIST *files, BOOL ctrl, BOOL shift);
void  FileCommandsDrop(HWND owner, LPCSTR target, int kind, PATHLIST *files, int effect);
void  FileCommandsDropFiles(HWND iv, HDROP drop); /* WM_DROPFILES from other programs */

/* ---- sendto.c: Send To, the items of the SendTo folder ---- */
#define CMD_SENDTO      0x0D00          /* + item: the menu's commands */
#define MAX_SENDTO      32
HMENU SendToMenu(void);                           /* filled as it opens (SendToMessage) */
BOOL  SendToIsMenu(HMENU m);
BOOL  SendToMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT *res);  /* its owner's: fill, draw, keys */
void  SendToRun(HWND owner, PATHLIST *files, UINT cmd);

/* ---- dragdrop.c ---- */
void DragDropRun(HWND iv, BOOL right);            /* drags the selection until the drop */

/* ---- shortcutkeys.c: shortcut keys (Ctrl+Alt+N) of the desktop's and Start menu's shortcuts ---- */
void ShortcutKeysScan(void);                      /* (again) all of them, to SNSYSTEM's hook */
void ShortcutKeysChanged(LPCSTR folder);          /* files changed there (NULL: anywhere) */
void ShortcutKeysRun(int i);                      /* SBM_HOTKEY */
void ShortcutKeyText(WORD key, LPSTR out);        /* "Ctrl + Alt + N", "None" */
void ShortcutKeysField(HWND edit, WORD key);      /* an edit becomes the key field */
WORD ShortcutKeysFieldKey(HWND edit);

/* ---- quickview.c: Quick View ---- */
BOOL QuickViewCan(LPCSTR path);                   /* a text file or a BMP picture */
void QuickViewShow(LPCSTR path);

/* ---- find.c: Find: Files or Folders ---- */
void FindShow(LPCSTR lookIn);

#endif
