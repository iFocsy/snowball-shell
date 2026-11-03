/* Folder Options, kept in SNOWBALL.INI [Folders]. */
#include "internal.h"

VIEWOPTIONS g_viewOptions;

/* Folder Options, kept in SNOWBALL.INI [Folders]. */
void ViewOptionsLoad(void)
{
    g_viewOptions.fullPath = SettingsGetInt("Folders", "FullPath", 0);
    g_viewOptions.hideExt = SettingsGetInt("Folders", "HideExtensions", 0);
    g_viewOptions.allUpper = SettingsGetInt("Folders", "AllUppercase", 0);
    g_viewOptions.hidden = SettingsGetInt("Folders", "Hidden", 0);
    g_viewOptions.ownWindows = SettingsGetInt("Folders", "OwnWindows", 0);
    g_viewOptions.infoTips = SettingsGetInt("Folders", "InfoTips", 1);
}

void ViewOptionsSave(void)
{
    SettingsSetInt("Folders", "FullPath", g_viewOptions.fullPath);
    SettingsSetInt("Folders", "HideExtensions", g_viewOptions.hideExt);
    SettingsSetInt("Folders", "AllUppercase", g_viewOptions.allUpper);
    SettingsSetInt("Folders", "Hidden", g_viewOptions.hidden);
    SettingsSetInt("Folders", "OwnWindows", g_viewOptions.ownWindows);
    SettingsSetInt("Folders", "InfoTips", g_viewOptions.infoTips);
}

/* Whether Folder Options keeps a file with these attributes out of view. */
BOOL ViewOptionsHides(WORD attr)
{
    return g_viewOptions.hidden == VIEW_NO_HIDDEN_SYSTEM ? (attr & (FA_HIDDEN | FA_SYSTEM)) != 0
         : g_viewOptions.hidden == VIEW_NO_HIDDEN ? (attr & FA_HIDDEN) != 0 : FALSE;
}
