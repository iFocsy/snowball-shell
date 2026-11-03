/*
 * The one header every source of SNSHELL.DLL includes. The shell's parts,
 * each a folder with its header, from the bottom up:
 *   core\         no windows: settings, paths, DOS files with long names,
 *                 shortcuts, launching, the Documents list
 *   ui\           windows that know nothing of the shell: the icon view,
 *                 folder tree, tabs, tooltips, icons; painting, message loop
 *   files\        file operations with progress, Recycle Bin, file clipboard
 *   explorer\     the desktop, folder and Explorer windows and what they do
 *                 with files: commands, drag and drop, Send To, Find, Quick
 *                 View, shortcut keys
 *   taskbar\      taskbar, Start menu, Quick Launch, Alt+Tab, Program
 *                 Manager's groups and DDE
 *   dialogs\      Run, Go To Folder, Shut Down, About, Open and Save As,
 *                 Open With
 *   settings\     property sheets and options: Properties, Folder Options,
 *                 File Types, Display, Taskbar, Recycle Bin
 *   taskmanager\  Task Manager
 *   snshell.c     the library's entry and ShellMain; startup.c, what starts with it
 *
 * The headers are included in that order; each uses only the types of those
 * before it. Any part may call any other.
 *
 * SNSHELL.DLL is built in the large model: pointers are far, and strings and
 * static data beyond small variables live outside its 64 KB data segment,
 * which is left to small variables and the local heap.
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include <windows.h>
#include <string.h>
#include <shellapi.h>
#include <commdlg.h>
#include "snowball.h"
#include "snsystem.h"
#include "snshell.h"
#include "resources\resource.h"

extern HINSTANCE g_hInst;               /* SNSHELL.DLL: resources, window classes */
extern HINSTANCE g_hArt;                /* W98ART.DLL: Windows 98's icons, cursors, bitmaps */
extern BOOL g_isShell;                  /* SYSTEM.INI's shell (else run as a program) */

#include "core\core.h"
#include "ui\ui.h"
#include "files\files.h"
#include "explorer\explorer.h"
#include "taskbar\taskbar.h"
#include "dialogs\dialogs.h"
#include "settings\settings.h"
#include "taskmanager\taskmanager.h"

/* ---- startup.c ---- */
BOOL StartupIsShell(void);
void StartupRun(LPCSTR cmdLine);                  /* "win program", load=, run=, StartUp */

#endif
