/*
 * The Effects page's switch for the USER and GDI Resource Extensions
 * (USERX.DRV, GDIX.DRV), Setup's components. They read their switches when Windows starts. The
 * page also tells when one of them refused to run here.
 */
#include "internal.h"

/* A driver's status: 0xFFFF and why when off; past 2 it failed to start. */
static BOOL Refused(LPCSTR module, LPCSTR function)
{
    HMODULE m = GetModuleHandle(module);
    DWORD (FAR PASCAL *status)(void);
    DWORD s;

    status = m ? (DWORD (FAR PASCAL *)(void))GetProcAddress(m, function) : NULL;
    s = status ? status() : 0;
    return LOWORD(s) == 0xFFFF && HIWORD(s) > 2;
}

static BOOL On(void)
{
    return GetPrivateProfileInt("userx.drv", "Enabled", 1, "SYSTEM.INI") &&
           GetPrivateProfileInt("gdix.drv", "Enabled", 1, "SYSTEM.INI");
}

void ExtendersInit(HWND dlg)
{
    char list[256];
    BOOL there;

    GetPrivateProfileString("boot", "drivers", "", list, sizeof list, "SYSTEM.INI");
    AnsiLower(list);
    there = strstr(list, "userx.drv") || strstr(list, "gdix.drv");
    CheckDlgButton(dlg, IDC_EF_RESOURCES, there && On());
    EnableWindow(GetDlgItem(dlg, IDC_EF_RESOURCES), there);
    SetDlgItemText(dlg, IDC_EF_RESNOTE,
                   !there ? "Not installed. Run Snowball Setup to add them." :
                   Refused("GDIX", "GDIXSTATUS") ?
                       "GDI Resource Extension disabled: the display driver failed its compatibility test." :
                   Refused("USERX", "USERXSTATUS") ?
                       "USER Resource Extension disabled: this version of USER.EXE is not supported." :
                   "Moves closed menus and display driver graphics data out of USER and GDI "
                   "resources. Takes effect when Windows restarts.");
}

/* TRUE when the switch changed: it takes effect when Windows restarts. */
BOOL ExtendersApply(HWND dlg)
{
    BOOL on = IsDlgButtonChecked(dlg, IDC_EF_RESOURCES);

    if (!IsWindowEnabled(GetDlgItem(dlg, IDC_EF_RESOURCES)) || on == On())
        return FALSE;
    WritePrivateProfileString("userx.drv", "Enabled", on ? NULL : "0", "SYSTEM.INI");
    WritePrivateProfileString("gdix.drv", "Enabled", on ? NULL : "0", "SYSTEM.INI");
    return TRUE;
}
