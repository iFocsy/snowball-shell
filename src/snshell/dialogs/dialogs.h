/* dialogs\: the shell's own dialogs, and every program's Open and Save As. */
#ifndef DIALOGS_H
#define DIALOGS_H

void RunDialogShow(HWND owner);
BOOL GoToDialogShow(HWND owner, LPSTR path);      /* path: in the shown folder, out the chosen */
BOOL PromptDialogShow(HWND owner, LPCSTR title, LPCSTR label, LPSTR text, int size);
                                                  /* a name asked for; text: in shown, out typed */
BOOL BrowseDialogShow(HWND owner, LPCSTR title, LPCSTR filter, LPSTR file, BOOL longNames);
                                                  /* Open to pick a file: filter "Name\0*.x\0" */
BOOL BrowseDialogSave(HWND owner, LPCSTR title, LPCSTR filter, LPCSTR ext, LPSTR file);   /* Save As */
BOOL OpenWithDialogShow(HWND owner, LPCSTR file); /* the program to open it with; it opens */
void ShutDownDialogShow(HWND owner);
void AboutDialogShow(HWND owner);
BOOL ChangeIconDialogShow(HWND owner, LPSTR file, int *index);  /* file and index: in shown, out chosen */
void OpenSaveDialogRequest(FILEREQUEST FAR *req); /* SBM_FILEDLG */
void FormatDialogShow(HWND owner, int drive);     /* a floppy: 0 A:, 1 B: */
void FormatDialogCopyDisk(HWND owner, int drive); /* DISKCOPY, both disks in it in turn */

#endif
