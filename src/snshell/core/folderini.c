/*
 * A folder's desktop.ini, as Windows 98 keeps it: [.ShellClassInfo] IconFile
 * and IconIndex (the folder's own icon), InfoTip (the text shown when the
 * mouse rests on it). The file is hidden, as Windows 98 makes it.
 */
#include "internal.h"

#define FOLDER_INI      "DESKTOP.INI"
#define FOLDER_SECTION  ".ShellClassInfo"

static void IniPath(LPCSTR folder, LPSTR ini)
{
    if (!DosShortPath(folder, ini))
        lstrcpy(ini, folder);
    PathAppend(ini, FOLDER_INI);
}

int FolderIniRead(LPCSTR folder, LPCSTR key, LPSTR out, int size)
{
    char ini[MAX_PATH];

    IniPath(folder, ini);
    return GetPrivateProfileString(FOLDER_SECTION, key, "", out, size, ini);
}

/* The folder's own icon: its file (made whole against the folder) and number. */
BOOL FolderIniIcon(LPCSTR folder, LPSTR file, int *index)
{
    char ini[MAX_PATH], name[MAX_PATH];

    if (!FolderIniRead(folder, "IconFile", name, sizeof name))
        return FALSE;
    IniPath(folder, ini);
    *index = GetPrivateProfileInt(FOLDER_SECTION, "IconIndex", 0, ini);
    if (name[0] && name[1] != ':' && name[0] != '\\') {     /* "folder.ico": in the folder */
        lstrcpy(file, folder);
        PathAppend(file, name);
    } else {
        lstrcpy(file, name);
    }
    return TRUE;
}

/* Sets the icon (file "": none) and the infotip ("": none). */
BOOL FolderIniWrite(LPCSTR folder, LPCSTR iconFile, int iconIndex, LPCSTR tip)
{
    char ini[MAX_PATH], number[8];
    WORD attr;
    BOOL ok;

    IniPath(folder, ini);
    wsprintf(number, "%d", iconIndex);
    ok = WritePrivateProfileString(FOLDER_SECTION, "IconFile", *iconFile ? iconFile : NULL, ini) &&
         WritePrivateProfileString(FOLDER_SECTION, "IconIndex", *iconFile ? (LPCSTR)number : NULL, ini) &&
         WritePrivateProfileString(FOLDER_SECTION, "InfoTip", *tip ? tip : NULL, ini);
    if ((attr = DosGetAttributes(ini)) != 0xFFFF && !(attr & FA_HIDDEN))
        DosSetAttributes(ini, (attr | FA_HIDDEN) & (FA_READONLY | FA_HIDDEN | FA_SYSTEM | FA_ARCHIVE));
    return ok;
}
