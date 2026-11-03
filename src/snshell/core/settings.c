/* Settings, kept in SNOWBALL.INI next to SNOWBALL.EXE. */
#include "internal.h"

char g_home[MAX_PATH];

char g_ini[MAX_PATH];

int SettingsGetInt(LPCSTR section, LPCSTR key, int def)
{
    return GetPrivateProfileInt(section, key, def, g_ini);
}

void SettingsSetInt(LPCSTR section, LPCSTR key, int value)
{
    char text[8];
    wsprintf(text, "%d", value);
    WritePrivateProfileString(section, key, text, g_ini);
}

void SettingsGetString(LPCSTR section, LPCSTR key, LPCSTR def, LPSTR buf, int size)
{
    GetPrivateProfileString(section, key, def, buf, size, g_ini);
}
