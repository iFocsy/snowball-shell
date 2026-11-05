/*
 * Shortcut keys, as in Windows 98: a shortcut on the desktop or in the
 * Start menu may have one (its HotKey line: the key, then SK_* in the high
 * byte), set on its Properties' Shortcut page, where the key field takes
 * the key pressed (a letter alone gets Ctrl+Alt, Backspace clears it).
 * SNSYSTEM's keyboard hook watches for them in every program and posts
 * SBM_HOTKEY; the shortcut then opens.
 */
#include "internal.h"

#define MAX_KEYS    32
#define MAX_DEPTH   4

static PATHLIST paths;                  /* of the keys, in their order */
static WORD keys[MAX_KEYS];
static int nKeys;
static WNDPROC editProc;

static void Scan(LPCSTR dir, int depth)
{
    static DOSENTRY e;                  /* large: the recursion keeps only the search */
    char spec[MAX_PATH], path[MAX_PATH];
    DOSFIND f;
    WORD key;

    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (!DosFindFirst(spec, &f, &e))
        return;
    do {
        lstrcpy(path, dir);
        PathAppend(path, e.name);
        if (e.attr & FA_DIR) {
            if (depth < MAX_DEPTH)
                Scan(path, depth + 1);
        } else if (PathIsShortcut(path) && nKeys < MAX_KEYS && (key = ShortcutHotKey(path)) != 0) {
            if (PathListAdd(&paths, path))
                keys[nKeys++] = key;
        }
    } while (DosFindNext(&f, &e));
    DosFindClose(&f);
}

void ShortcutKeysScan(void)
{
    char dir[MAX_PATH];

    nKeys = 0;
    PathListFree(&paths);
    DesktopFolder(dir);
    Scan(dir, MAX_DEPTH);
    StartMenuFolder(dir);
    Scan(dir, 0);
    HookHotKeys(keys, nKeys);
}

/* After changes in a folder (NULL: anywhere): the keys again, if it holds shortcuts that count. */
void ShortcutKeysChanged(LPCSTR folder)
{
    char dir[MAX_PATH];

    if (folder) {
        DesktopFolder(dir);
        if (!PathIsInside(folder, dir)) {
            StartMenuFolder(dir);
            if (!PathIsInside(folder, dir))
                return;
        }
    }
    ShortcutKeysScan();
}

void ShortcutKeysRun(int i)
{
    char path[MAX_PATH];

    if (i >= 0 && i < nKeys && PathListAt(&paths, i, path))
        LaunchItem(g_taskbar, path);
}

/* "Ctrl + Alt + N", as Windows 98 writes a key; "None". */
void ShortcutKeyText(WORD key, LPSTR out)
{
    char name[16];
    BYTE vk = LOBYTE(key);

    if (!key) {
        lstrcpy(out, "None");
        return;
    }
    *out = '\0';
    if (HIBYTE(key) & SK_CONTROL)
        lstrcat(out, "Ctrl + ");
    if (HIBYTE(key) & SK_SHIFT)
        lstrcat(out, "Shift + ");
    if (HIBYTE(key) & SK_ALT)
        lstrcat(out, "Alt + ");
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
        wsprintf(name, "%c", vk);
    else if (vk >= VK_F1 && vk <= VK_F12)
        wsprintf(name, "F%d", vk - VK_F1 + 1);
    else if (!GetKeyNameText(MAKELONG(0, MapVirtualKey(vk, 0)), name, sizeof name))
        wsprintf(name, "#%d", vk);
    lstrcat(out, name);
}

/* The key field: the key pressed (without Ctrl or Alt: with both), shown
   as text and kept as the field's property "SbKey". */
LRESULT CALLBACK __export ShortcutKeysEditProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    char text[40];
    WORD key, mods = 0;

    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp == VK_SHIFT || wp == VK_CONTROL || wp == VK_MENU)
            return 0;
        if (wp == VK_TAB || ((wp == VK_RETURN || wp == VK_ESCAPE) && msg == WM_KEYDOWN))
            break;                      /* the dialog's */
        if (GetKeyState(VK_SHIFT) < 0)   mods |= SK_SHIFT;
        if (GetKeyState(VK_CONTROL) < 0) mods |= SK_CONTROL;
        if (GetKeyState(VK_MENU) < 0)    mods |= SK_ALT;
        if (!(mods & (SK_CONTROL | SK_ALT)) && (wp == VK_BACK || wp == VK_DELETE || wp == VK_SPACE))
            key = 0;
        else
            key = (WORD)wp | ((mods & (SK_CONTROL | SK_ALT) ? mods : mods | SK_CONTROL | SK_ALT) << 8);
        SetProp(w, "SbKey", (HANDLE)key);
        ShortcutKeyText(key, text);
        SetWindowText(w, text);
        SendMessage(GetParent(w), WM_COMMAND, GetDlgCtrlID(w), MAKELPARAM(w, EN_CHANGE));
        return 0;
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        return 0;
    case WM_DESTROY:
        RemoveProp(w, "SbKey");
        RemoveProp(w, "SbKeyField");
        break;
    }
    return CallWindowProc((FARPROC)editProc, w, msg, wp, lp);
}

void ShortcutKeysField(HWND edit, WORD key)
{
    char text[40];

    editProc = (WNDPROC)SetWindowLong(edit, GWL_WNDPROC, (LONG)(FARPROC)ShortcutKeysEditProc);
    SetProp(edit, "SbKey", (HANDLE)key);
    SetProp(edit, "SbKeyField", (HANDLE)1);     /* SNSYSTEM's hook lets it have the shortcut keys */
    ShortcutKeyText(key, text);
    SetWindowText(edit, text);
}

WORD ShortcutKeysFieldKey(HWND edit)
{
    return (WORD)GetProp(edit, "SbKey");
}
