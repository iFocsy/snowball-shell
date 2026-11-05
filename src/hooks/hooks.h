/*
 * The tests' hooks: what the tests read of Snowball, in words (UIDRIVE's
 * facts, tests\run.ps1) instead of a screenshot's pixels. All of it is in
 * src\hooks and is compiled only with SNOWBALL_HOOKS: build.ps1 makes those
 * SNSYSTEM.DLL and SNSHELL.DLL in build\hooks, which the tests use; the
 * release has none of it. (Not Windows' hooks: src\snsystem\hooks.c.)
 *   "SnowballDescribe"      a control adds lines about what it shows
 *                           (wParam: the buffer's size, lParam: the buffer)
 *   "SnowballDescribeMenu"  the owner of an open menu writes the items it
 *                           draws itself (wParam: the menu, lParam: a buffer
 *                           of DESCRIBE_MENU_SIZE); 0: not one of those
 *   HookOpenMenus           SNSYSTEM.DLL's export: the popup menus open now
 * Where a window answers, a HOOK_* line; without the switch it is nothing.
 * Nothing that changes by itself (the time) is told.
 */
#ifndef HOOKS_H
#define HOOKS_H

#ifdef SNOWBALL_HOOKS

#define DESCRIBE_MENU_SIZE  2048        /* as UIDRIVE's facts.c */

/* ---- snshell\: the controls' words ---- */
UINT    DescribeMessage(void);
UINT    DescribeMenuMessage(void);
LRESULT DescribeLine(LPSTR buf, WORD size, LPCSTR fmt, ...);   /* added if it fits; the length after */
LRESULT IconViewDescribe(HWND w, LPSTR buf, WORD size);
LRESULT TreeViewDescribe(HWND w, LPSTR buf, WORD size);
LRESULT TabsDescribe(HWND w, LPSTR buf, WORD size);
LRESULT ToolbarDescribe(HWND w, LPSTR buf, WORD size);
LRESULT SliderDescribe(HWND w, LPSTR buf, WORD size);
LRESULT TooltipDescribe(LPSTR buf, WORD size);
LRESULT SwitcherDescribe(LPSTR buf, WORD size);
LRESULT TaskbarDescribe(HWND w, LPSTR buf, WORD size);
LRESULT StartMenuDescribeMenu(HMENU m, LPSTR buf);              /* 0: not one of its menus */
LRESULT SendToDescribeMenu(HMENU m, LPSTR buf);                 /* 0: not Send To */
LRESULT PopupMenuDescribeMenu(WPARAM wp, LPARAM lp);            /* its owner's answer */

/* ... what they read of the controls' own data (beside them, also under the switch) */
LPCSTR  TabsLabel(HWND w, int i, BOOL *selected);               /* NULL past the last */
BOOL    ToolbarButton(HWND w, int i, UINT *cmd, LPSTR label, BOOL *off, BOOL *checked);  /* FALSE past the last */
int     SliderCount(HWND w);
LPCSTR  TooltipText(void);
HWND    SwitcherWindow(int i, BOOL *selected);                  /* NULL past the last */
BOOL    SendToItemLabel(UINT cmd, LPSTR label);
HWND    PopupMenuOwner(void);
int     QuickLaunchCount(void);

/* ---- snsystem\: the menus open now ---- */
void    HookMenuMessage(HWND w, UINT msg, WPARAM wp, LPARAM lp);    /* from Windows' WH_CALLWNDPROC hook */
int     SNSYSTEM_API HookOpenMenus(HMENU FAR *menus, HWND FAR *owners, int max);

#define HOOK_DESCRIBE(msg, call)                if ((msg) == DescribeMessage()) return (call)
#define HOOK_DESCRIBE_MENU(msg, call)           if ((msg) == DescribeMenuMessage()) return (call)
#define HOOK_DESCRIBE_MENU_RES(msg, res, call)  if ((msg) == DescribeMenuMessage() && (*(res) = (call)) != 0) return TRUE
#define HOOK_MENU_MESSAGE(code, c)              if ((code) >= 0) HookMenuMessage((c)->hwnd, (c)->message, (c)->wParam, (c)->lParam)

#else

#define HOOK_DESCRIBE(msg, call)
#define HOOK_DESCRIBE_MENU(msg, call)
#define HOOK_DESCRIBE_MENU_RES(msg, res, call)
#define HOOK_MENU_MESSAGE(code, c)

#endif
#endif
