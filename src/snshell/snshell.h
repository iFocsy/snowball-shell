/* SNSHELL.DLL as SNOWBALL.EXE sees it. */
#ifndef SNSHELL_H
#define SNSHELL_H

#ifdef SNSHELL_DLL
#define SNSHELL_API FAR PASCAL __export
#else
#define SNSHELL_API FAR PASCAL
#endif

/* Runs the shell in the calling task until Windows ends; exe is
   SNOWBALL.EXE, whose folder holds SNOWBALL.INI and the Start Menu. */
int SNSHELL_API ShellMain(HINSTANCE exe, LPSTR cmdLine);

#endif
