/*
 * File types. Windows 3.1 keeps them in the registration database (REG.DAT:
 * ".ext" = class, class = its description, class\shell\<action>\command =
 * the program line, %1 standing for the file) and the open command also in
 * WIN.INI [Extensions] ("ext=program ^.ext"), which older programs read;
 * File Manager's Associate writes both, and so does Snowball. Windows 95's
 * keys are kept as well: class\shell = the default action, class\DefaultIcon
 * = "file,index", class\AlwaysShowExt. Extensions are given without the dot.
 */
#include "internal.h"
#include <stdlib.h>

#define HKCR    HKEY_CLASSES_ROOT

static BOOL Query(LPCSTR key, LPSTR out, int size)
{
    LONG n = size;

    out[0] = '\0';
    return RegQueryValue(HKCR, key, out, &n) == ERROR_SUCCESS && out[0];
}

static BOOL Exists(LPCSTR key)
{
    HKEY h;

    if (RegOpenKey(HKCR, key, &h) != ERROR_SUCCESS)
        return FALSE;
    RegCloseKey(h);
    return TRUE;
}

static void Set(LPCSTR key, LPCSTR value)
{
    RegSetValue(HKCR, key, REG_SZ, value, lstrlen(value));
}

static void ExtKey(LPCSTR ext, LPSTR key)
{
    wsprintf(key, ".%s", ext);
    AnsiLower(key);
}

BOOL FileTypeClass(LPCSTR ext, LPSTR cls, int size)
{
    char key[8];

    ExtKey(ext, key);
    return *ext && Query(key, cls, size);
}

BOOL FileTypeCommand(LPCSTR cls, LPCSTR action, LPSTR cmd, int size)
{
    char key[80];

    wsprintf(key, "%s\\shell\\%s\\command", cls, action);
    return Query(key, cmd, size);
}

void FileTypeDefaultAction(LPCSTR cls, LPSTR action, int size)
{
    char key[48], cmd[MAX_PATH];

    wsprintf(key, "%s\\shell", cls);
    if (!*cls || !Query(key, action, size) || !FileTypeCommand(cls, action, cmd, sizeof cmd))
        lstrcpyn(action, "open", size);
}

/* WIN.INI's "program ^.txt" as a registration line, "program %1". */
static void FromWinIni(LPCSTR line, LPSTR cmd, int size)
{
    LPCSTR caret = _fstrchr(line, '^');

    if (!caret) {
        lstrcpyn(cmd, line, size - 3);
        lstrcat(cmd, " %1");
        return;
    }
    lstrcpyn(cmd, line, min((int)(caret - line), size - 3) + 1);
    lstrcat(cmd, "%1");
    while (*caret && *caret != ' ')     /* "^.txt": the file */
        caret++;
    if (lstrlen(cmd) + lstrlen(caret) < size)
        lstrcat(cmd, caret);
}

/* And back: "%1" becomes "^.ext". */
static void ToWinIni(LPCSTR cmd, LPCSTR ext, LPSTR line, int size)
{
    LPCSTR p = _fstrstr(cmd, "%1");
    char file[10];

    wsprintf(file, "^.%s", ext);
    AnsiLower(file);
    lstrcpyn(line, cmd, min(p ? (int)(p - cmd) : lstrlen(cmd), size - 12) + 1);
    if (!p)
        lstrcat(line, " ");
    lstrcat(line, file);
    if (p && lstrlen(line) + lstrlen(p + 2) < size)
        lstrcat(line, p + 2);
}

BOOL FileTypeOpenCommand(LPCSTR ext, LPSTR cmd, int size)
{
    char cls[40], action[16], line[MAX_PATH], key[8];

    if (FileTypeClass(ext, cls, sizeof cls)) {
        FileTypeDefaultAction(cls, action, sizeof action);
        if (FileTypeCommand(cls, action, cmd, size))
            return TRUE;
    }
    lstrcpyn(key, ext, sizeof key);
    AnsiLower(key);
    if (!*ext || !GetProfileString("Extensions", key, "", line, sizeof line))
        return FALSE;
    FromWinIni(line, cmd, size);
    return TRUE;
}

int FileTypeActions(LPCSTR ext, char FAR names[][16], int max)
{
    char cls[40], key[60], cmd[MAX_PATH];
    HKEY h;
    DWORD i;
    int n = 0;

    if (!FileTypeClass(ext, cls, sizeof cls)) {     /* WIN.INI's: open only */
        if (!FileTypeOpenCommand(ext, cmd, sizeof cmd))
            return 0;
        lstrcpy(names[0], "open");
        return 1;
    }
    FileTypeDefaultAction(cls, names[0], sizeof names[0]);
    n = FileTypeCommand(cls, names[0], cmd, sizeof cmd) ? 1 : 0;
    wsprintf(key, "%s\\shell", (LPSTR)cls);
    if (RegOpenKey(HKCR, key, &h) != ERROR_SUCCESS)
        return n;
    for (i = 0; n < max && RegEnumKey(h, i, names[n], sizeof names[0]) == ERROR_SUCCESS; i++)
        if ((!n || lstrcmpi(names[n], names[0])) && FileTypeCommand(cls, names[n], cmd, sizeof cmd))
            n++;
    RegCloseKey(h);
    return n;
}

BOOL FileTypeProgram(LPCSTR cmd, LPSTR program)
{
    char name[MAX_PATH];
    OFSTRUCT of;
    int n = 0;

    while (*cmd == ' ')
        cmd++;
    while (cmd[n] && cmd[n] != ' ' && n < MAX_PATH - 1)
        n++;
    lstrcpyn(name, cmd, n + 1);
    if (!*name)
        return FALSE;
    if (!*PathExtension(name))
        lstrcat(name, ".EXE");
    if (OpenFile(name, &of, OF_EXIST) == HFILE_ERROR)
        return FALSE;
    OemToAnsi(of.szPathName, program);
    return TRUE;
}

BOOL FileTypeIcon(LPCSTR ext, LPSTR file, int *index)
{
    char cls[40], key[60], cmd[MAX_PATH], *comma;

    *index = 0;
    if (FileTypeClass(ext, cls, sizeof cls)) {
        wsprintf(key, "%s\\DefaultIcon", (LPSTR)cls);
        if (Query(key, cmd, sizeof cmd)) {
            if ((comma = _fstrrchr(cmd, ',')) != NULL) {
                *comma = '\0';
                *index = atoi(comma + 1);
            }
            lstrcpy(file, cmd);
            return TRUE;
        }
    }
    return FileTypeOpenCommand(ext, cmd, sizeof cmd) && FileTypeProgram(cmd, file);
}

BOOL FileTypeAlwaysShowExt(LPCSTR ext)
{
    char cls[40], key[60];

    if (!FileTypeClass(ext, cls, sizeof cls))
        return FALSE;
    wsprintf(key, "%s\\AlwaysShowExt", (LPSTR)cls);
    return Exists(key);
}

/* A class name for a new type, Windows 98's "txtfile", not yet taken. */
static void NewClass(LPCSTR ext, LPSTR cls)
{
    int n = 1;

    wsprintf(cls, "%sfile", ext);
    AnsiLower(cls);
    while (Exists(cls) && n < 100)
        wsprintf(cls, "%sfile%d", ext, ++n);
    AnsiLower(cls);
}

/* Programs re-read [Extensions]; names and icons of documents are found again. */
static void Changed(void)
{
    SendMessage(HWND_BROADCAST, WM_WININICHANGE, 0, (LPARAM)(LPCSTR)"Extensions");
    IconsForgetDocuments();
    FormatForgetTypes();
}

void FileTypeAssociate(LPCSTR ext, LPCSTR program, LPCSTR desc)
{
    char cls[40], key[80], shortProgram[MAX_PATH], cmd[MAX_PATH + 4], line[MAX_PATH + 12], lower[8];

    if (!FileTypeClass(ext, cls, sizeof cls)) {
        NewClass(ext, cls);
        ExtKey(ext, key);
        Set(key, cls);
    }
    if (desc && *desc)
        Set(cls, desc);
    DosShortPath(program, shortProgram);
    wsprintf(cmd, "%s %%1", (LPSTR)shortProgram);
    wsprintf(key, "%s\\shell\\open\\command", (LPSTR)cls);
    Set(key, cmd);
    wsprintf(key, "%s\\shell", (LPSTR)cls);   /* opening is the default again */
    Set(key, "");
    ToWinIni(cmd, ext, line, sizeof line);
    lstrcpyn(lower, ext, sizeof lower);
    AnsiLower(lower);
    WriteProfileString("Extensions", lower, line);
    Changed();
}

/* ---- the File Types list and its editing ---- */

/* Adds ext to the type of class cls in ids (a new one if none). */
static int AddExt(FILETYPEID FAR *ids, int n, int max, LPCSTR cls, LPCSTR ext)
{
    int i;

    for (i = 0; i < n && (!*cls || lstrcmpi(ids[i].cls, cls)); i++)
        ;
    if (i == n) {
        if (n == max)
            return n;
        lstrcpyn(ids[n].cls, cls, sizeof ids[0].cls);
        ids[n].exts[0] = '\0';
        n++;
    }
    if (lstrlen(ids[i].exts) + lstrlen(ext) + 2 < sizeof ids[0].exts) {
        if (ids[i].exts[0])
            lstrcat(ids[i].exts, " ");
        lstrcat(ids[i].exts, ext);
        AnsiUpper(ids[i].exts);
    }
    return n;
}

int FileTypesList(FILETYPEID FAR *ids, int max)
{
    char key[64], cls[40], keys[512];
    LPSTR k;
    DWORD i;
    int n = 0;

    for (i = 0; RegEnumKey(HKCR, i, key, sizeof key) == ERROR_SUCCESS; i++)
        if (key[0] == '.' && key[1] && Query(key, cls, sizeof cls))
            n = AddExt(ids, n, max, cls, key + 1);
    /* extensions only WIN.INI knows: a type each */
    GetProfileString("Extensions", NULL, "", keys, sizeof keys);
    for (k = keys; *k; k += lstrlen(k) + 1)
        if (!FileTypeClass(k, cls, sizeof cls))
            n = AddExt(ids, n, max, "", k);
    return n;
}

/* The first extension of a type's list ("TXT INI" -> "TXT"). */
void FileTypeFirstExt(LPCSTR exts, LPSTR ext)
{
    int n = 0;

    while (exts[n] && exts[n] != ' ' && n < 7)
        n++;
    lstrcpyn(ext, exts, n + 1);
}

void FileTypeLoad(const FILETYPEID FAR *id, FILETYPE FAR *t)
{
    char key[80], name[MAX_PATH], ext[8];
    HKEY h;
    DWORD i;

    _fmemset(t, 0, sizeof *t);
    lstrcpy(t->cls, id->cls);
    lstrcpy(t->exts, id->exts);
    FileTypeFirstExt(t->exts, ext);
    wsprintf(name, "x.%s", (LPSTR)ext);
    FormatFileType(name, t->desc);
    if (!*t->cls) {                     /* WIN.INI's open command only */
        if (FileTypeOpenCommand(ext, t->actions[0].cmd, sizeof t->actions[0].cmd)) {
            lstrcpy(t->actions[0].name, "open");
            t->nActions = 1;
        }
    } else {
        wsprintf(key, "%s\\shell", (LPSTR)t->cls);
        if (RegOpenKey(HKCR, key, &h) == ERROR_SUCCESS) {
            for (i = 0; t->nActions < FT_ACTIONS &&
                        RegEnumKey(h, i, t->actions[t->nActions].name, sizeof t->actions[0].name) == ERROR_SUCCESS; i++)
                if (FileTypeCommand(t->cls, t->actions[t->nActions].name, t->actions[t->nActions].cmd,
                                    sizeof t->actions[0].cmd))
                    t->nActions++;
            RegCloseKey(h);
        }
        wsprintf(key, "%s\\DefaultIcon", (LPSTR)t->cls);
        if (Query(key, name, sizeof name) && _fstrrchr(name, ',')) {
            t->iconIndex = atoi(_fstrrchr(name, ',') + 1);
            *_fstrrchr(name, ',') = '\0';
            lstrcpy(t->icon, name);
        }
        t->showExt = FileTypeAlwaysShowExt(ext);
    }
    FileTypeDefaultAction(t->cls, t->defAction, sizeof t->defAction);
    lstrcpy(t->origDesc, t->desc);
}

/* Writes the type: its class (made for a new type), description if it was
   changed, actions, icon, and each extension's WIN.INI line. */
void FileTypeSave(FILETYPE FAR *t)
{
    char key[100], ext[8], line[MAX_PATH], keys[48], lower[8];
    LPSTR e;
    int i;

    FileTypeFirstExt(t->exts, ext);
    if (!*t->cls)
        NewClass(ext, t->cls);
    if (!Exists(t->cls) || lstrcmp(t->desc, t->origDesc))
        Set(t->cls, t->desc);
    wsprintf(key, "%s\\shell", (LPSTR)t->cls);
    RegDeleteKey(HKCR, key);
    for (i = 0; i < t->nActions; i++) {
        wsprintf(key, "%s\\shell\\%s\\command", (LPSTR)t->cls, (LPSTR)t->actions[i].name);
        Set(key, t->actions[i].cmd);
    }
    wsprintf(key, "%s\\shell", (LPSTR)t->cls);
    Set(key, lstrcmpi(t->defAction, "open") ? t->defAction : "");
    wsprintf(key, "%s\\DefaultIcon", (LPSTR)t->cls);
    if (t->icon[0]) {
        wsprintf(line, "%s,%d", (LPSTR)t->icon, t->iconIndex);
        Set(key, line);
    } else {
        RegDeleteKey(HKCR, key);
    }
    wsprintf(key, "%s\\AlwaysShowExt", (LPSTR)t->cls);
    if (t->showExt)
        Set(key, "");
    else
        RegDeleteKey(HKCR, key);
    lstrcpy(keys, t->exts);
    for (e = _fstrtok(keys, " "); e; e = _fstrtok(NULL, " ")) {
        ExtKey(e, key);
        Set(key, t->cls);
        lstrcpyn(lower, e, sizeof lower);
        AnsiLower(lower);
        for (i = 0; i < t->nActions && lstrcmpi(t->actions[i].name, "open"); i++)
            ;
        if (i < t->nActions)
            ToWinIni(t->actions[i].cmd, e, line, sizeof line);
        WriteProfileString("Extensions", lower, i < t->nActions ? line : NULL);
    }
    lstrcpy(t->origDesc, t->desc);
    Changed();
}

void FileTypeRemove(const FILETYPE FAR *t)
{
    char key[8], keys[48];
    LPSTR e;

    lstrcpy(keys, t->exts);
    for (e = _fstrtok(keys, " "); e; e = _fstrtok(NULL, " ")) {
        ExtKey(e, key);
        RegDeleteKey(HKCR, key);
        AnsiLower(key);
        WriteProfileString("Extensions", key + 1, NULL);
    }
    if (*t->cls)
        RegDeleteKey(HKCR, t->cls);
    Changed();
}
