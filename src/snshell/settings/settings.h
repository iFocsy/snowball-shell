/* settings\: property sheets and options dialogs. */
#ifndef SETTINGS_H
#define SETTINGS_H

/* ---- properties.c: Properties of files, folders, drives, My Computer ---- */
void PropertiesShow(HWND owner, PATHLIST *items, int kind);  /* IVK_FILE/FOLDER (mixed), DRIVE, COMPUTER */
BOOL PropertiesPickIcon(HWND owner, LPSTR file, int *index);  /* Change Icon: in shown, out chosen */

/* ---- folderoptions.c ---- */
void FolderOptionsShow(void);

/* ---- filetypespage.c: Folder Options' File Types page ---- */
void FileTypesPageInit(HWND page);
void FileTypesPageDraw(const DRAWITEMSTRUCT FAR *d);
BOOL FileTypesPageCommand(HWND page, WPARAM wp, LPARAM lp);   /* its controls' */

/* ---- displayproperties.c: Display Properties ---- */
void DisplayPropertiesShow(int page);             /* 0 Background, 1 Screen Saver, 2 Appearance, 3 Settings */

/* ---- appearance.c: its Appearance page ---- */
void AppearanceInit(HWND dlg);
BOOL AppearanceCommand(HWND dlg, UINT id, UINT code);     /* TRUE: the colours changed */
BOOL AppearanceDrawItem(HWND dlg, const DRAWITEMSTRUCT FAR *d);
void AppearancePaint(HWND dlg, HDC dc);
void AppearanceApply(HWND dlg);

/* ---- taskbarproperties.c: Taskbar Properties, the Start menu's editing ---- */
void TaskbarPropertiesShow(void);

/* ---- binproperties.c: Recycle Bin Properties ---- */
void RecycleBinPropertiesShow(HWND owner);

#endif
