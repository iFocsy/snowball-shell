/*
 * Snowball - a Windows 98 style shell for Windows 3.11.
 * Copyright (C) 2026 Snowball contributors. GPL 3.
 * Based on the design of Calmira Reborn (GPL 2+).
 *
 * What Snowball's three binaries share (include after windows.h):
 *   SNOWBALL.EXE  src\snowball\   the shell's task: starts it, passes later starts on
 *   SNSHELL.DLL   src\snshell\    the shell: desktop, folders, taskbar, Start menu,
 *                                 dialogs, Task Manager
 *   SNSYSTEM.DLL  src\snsystem\   system-wide: hooks, the Windows 98 skin of every
 *                                 program, drawing, processor time, Open and Save As
 */
#ifndef SNOWBALL_H
#define SNOWBALL_H

#define SB_NAME     "Snowball"
#define SB_VERSION  "v20261105"

#define MAX_PATH    260

/* Window classes looked for by name */
#define DESKTOP_CLASS       "SbDesktop"
#define TASKBAR_CLASS       "SbTaskbar"
#define FOLDERWINDOW_CLASS  "SbFolder"

/* Messages to the taskbar and the desktop */
#define SBM_SHELL     (WM_USER + 100)   /* wParam HSHELL_* code, lParam window */
#define SBM_ACTIVATE  (WM_USER + 101)   /* wParam window being activated */
#define SBM_MINMAX    (WM_USER + 102)   /* wParam window, lParam SW_* command */
#define SBM_TITLE     (WM_USER + 103)   /* wParam window whose title changed */
#define SBM_STARTMENU (WM_USER + 104)   /* open the Start menu */
#define SBM_CHANGED   (WM_USER + 105)   /* lParam LPCSTR folder whose contents changed */
#define SBM_OPEN      (WM_USER + 106)   /* lParam global atom: path or command to open */
#define SBM_FILEDLG   (WM_USER + 107)   /* lParam FILEREQUEST FAR *: a program's Open or Save As */
#define SBM_SERVE     (WM_USER + 108)   /* the shell is up: take over Open and Save As */
#define SBM_SWITCH    (WM_USER + 109)   /* Alt+Tab, wParam SWITCH_*: from SNSYSTEM */
#define SWITCH_START  0
#define SWITCH_NEXT   1
#define SWITCH_PREV   2
#define SWITCH_DONE   3                 /* Alt let go: to the one chosen */
#define SWITCH_CANCEL 4
#define SBM_TASKMAN   (WM_USER + 110)   /* Ctrl+Shift+Esc, from a second start */
#define SBM_HOTKEY    (WM_USER + 112)   /* wParam: the shortcut key pressed (its index), from SNSYSTEM */
#define SK_SHIFT      0x01              /* a shortcut key: virtual key, these in the high byte */
#define SK_CONTROL    0x02
#define SK_ALT        0x04
#define SBM_WINKEY    (WM_USER + 114)   /* the Windows key: wParam 0 alone, else the key with it;
                                           lParam Shift was down */
#define SBM_SNAP      (WM_USER + 115)   /* wParam window dropped at the screen's edge, lParam VK_LEFT/RIGHT/UP */
#define SBM_ARROWS    (WM_USER + 116)   /* wParam window whose scroll bar arrows Windows may have drawn:
                                           SkinScrollArrows, from SNSYSTEM */
#define SBM_SOUND     (WM_USER + 113)   /* wParam SOUND_*: a sound event (a menu popped up) */
#define SOUND_OPEN        0             /* the events (WIN.INI [sounds], sounds.c) */
#define SOUND_CLOSE       1
#define SOUND_MINIMIZE    2
#define SOUND_MAXIMIZE    3
#define SOUND_RESTOREUP   4
#define SOUND_RESTOREDOWN 5
#define SOUND_MENUPOPUP   6
#define SOUND_EMPTYBIN    7
#define SBM_FLASH     (WM_USER + 111)   /* wParam window, lParam its caption looks active:
                                           FlashWindow, or (de)activation, from SNSYSTEM */

#endif
