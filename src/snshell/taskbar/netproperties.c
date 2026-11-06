/*
 * The network's settings, kept as Windows' own TCP/IP dialog keeps them: the
 * address, mask and gateway in SYSTEM.INI under the interface, DNS in [DNS], and
 * the card's interrupt and I/O address in PROTOCOL.INI. The network software
 * reads them when Windows starts, so a change takes a restart.
 *
 * Reached from the status window and the tray, and from Start's Settings: a
 * wrong I/O address only brings Windows' message at startup and no tray icon,
 * but a wrong interrupt hangs Windows at startup, and only WIN /N, no network,
 * gets past it. This must open in both cases, so it reads only the INI files.
 */
#include "internal.h"
#include "taskbarint.h"

#define SYSTEM_INI "SYSTEM.INI"
#define NO_ADDRESS "0.0.0.0"            /* Windows': the address comes from the network */
#define TITLE      "Network Properties"
#define PAGE_IP    0
#define PAGE_DNS   1
#define PAGE_CARD  2
#define IRQ_FIRST  2
#define IRQ_LAST   15
#define IO_FIRST   0x100
#define IO_LAST    0x3F0

static char section[48];                /* SYSTEM.INI's, of the interface */
static char cardSection[48];            /* PROTOCOL.INI's, of the card */
static char protocol[MAX_PATH];
static char moreDns[64];                /* the servers after the second: kept */
static BOOL changed;

/* The fields of a comma list: text keeps the first, the rest goes to rest. */
static void SplitList(LPSTR text, LPSTR rest, int size)
{
    LPSTR comma = _fstrchr(text, ',');

    *rest = '\0';
    if (comma) {
        *comma = '\0';
        lstrcpyn(rest, comma + 1, size);
    }
}

static BOOL FindSections(void)
{
    char line[96], rest[64];
    LPSTR field = line;
    int i;

    GetPrivateProfileString("MSTCP", "Interfaces", "", section, sizeof section, SYSTEM_INI);
    SplitList(section, rest, sizeof rest);
    GetWindowsDirectory(protocol, sizeof protocol);
    lstrcat(protocol, "\\PROTOCOL.INI");
    /* netcard=ms$ne2clone,1,MS$NE2CLONE,3: the third field names its section */
    GetPrivateProfileString("network.setup", "netcard", "", line, sizeof line, protocol);
    for (i = 0; i < 2 && field; i++) {
        field = _fstrchr(field, ',');
        if (field)
            field++;
    }
    cardSection[0] = '\0';
    if (field) {
        lstrcpyn(cardSection, field, sizeof cardSection);
        SplitList(cardSection, rest, sizeof rest);
    }
    return section[0] != '\0';
}

BOOL NetPropertiesAvailable(void)
{
    char driver[16];

    return GetPrivateProfileString("386Enh", "netcard", "", driver, sizeof driver, SYSTEM_INI) && FindSections();
}

static BOOL ParseAddress(LPCSTR s, DWORD *out)
{
    DWORD value = 0;
    int parts = 0, digits = 0;
    unsigned n = 0;

    for (;; s++) {
        if (*s >= '0' && *s <= '9') {
            n = n * 10 + (*s - '0');
            if (++digits > 3 || n > 255)
                return FALSE;
        } else if (*s == '.' || !*s) {
            if (!digits || ++parts > 4)
                return FALSE;
            value = value << 8 | n;
            n = digits = 0;
            if (!*s)
                break;
        } else {
            return FALSE;
        }
    }
    *out = value;
    return parts == 4;
}

static BOOL HostName(LPCSTR s, BOOL dots)
{
    int n = lstrlen(s), i;

    if (n > 63 || s[0] == '-' || s[0] == '.' || (n && (s[n - 1] == '-' || s[n - 1] == '.')))
        return FALSE;
    for (i = 0; i < n; i++)
        if (!(s[i] >= '0' && s[i] <= '9') && !(s[i] >= 'a' && s[i] <= 'z') && !(s[i] >= 'A' && s[i] <= 'Z') &&
            s[i] != '-' && !(dots && s[i] == '.'))
            return FALSE;
    return TRUE;
}

/* "300" and "0x300" alike; 0 if it is neither. */
static unsigned ParseHex(LPCSTR s)
{
    unsigned value = 0;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    if (!*s)
        return 0;
    for (; *s; s++) {
        int digit = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 :
                    *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
        if (digit < 0 || value > 0xFFF)
            return 0;
        value = value * 16 + digit;
    }
    return value;
}

/* Shows what is wrong where it is. Always FALSE. */
static BOOL Complain(HWND dlg, int page, int id, LPCSTR text)
{
    TabsSelect(GetDlgItem(dlg, IDC_TABS), page);
    TabsShowPage(dlg, page, NULL);
    MessageBox(dlg, text, TITLE, MB_ICONEXCLAMATION | MB_OK);
    SetFocus(GetDlgItem(dlg, id));
    SendDlgItemMessage(dlg, id, EM_SETSEL, 0, MAKELONG(0, -1));
    return FALSE;
}

static BOOL CheckAddresses(HWND dlg)
{
    char text[32];
    DWORD ip, mask, gateway, inverse;

    GetDlgItemText(dlg, IDC_NP_IP, text, sizeof text);
    if (!ParseAddress(text, &ip) || !(ip >> 24) || ip >> 24 == 127 || ip >> 24 >= 224)
        return Complain(dlg, PAGE_IP, IDC_NP_IP, "Type an IP address such as 192.168.0.50.");
    GetDlgItemText(dlg, IDC_NP_MASK, text, sizeof text);
    mask = 0;
    ParseAddress(text, &mask);
    inverse = ~mask;
    if (!mask || (inverse & (inverse + 1)))     /* ones, then zeros */
        return Complain(dlg, PAGE_IP, IDC_NP_MASK, "Type a subnet mask such as 255.255.255.0.");
    if (!(ip & inverse) || (ip & inverse) == inverse)
        return Complain(dlg, PAGE_IP, IDC_NP_IP, "That address is the network's own: it cannot be a computer's.");
    GetDlgItemText(dlg, IDC_NP_GATEWAY, text, sizeof text);
    if (text[0]) {
        if (!ParseAddress(text, &gateway) || gateway == ip)
            return Complain(dlg, PAGE_IP, IDC_NP_GATEWAY, "Type the address of the router, or leave this empty.");
        if ((gateway & mask) != (ip & mask))
            return Complain(dlg, PAGE_IP, IDC_NP_GATEWAY, "The default gateway must be on the same network as the IP address.");
    }
    return TRUE;
}

static BOOL CheckDns(HWND dlg)
{
    char text[64];
    DWORD value;
    int id;

    GetDlgItemText(dlg, IDC_NP_HOST, text, sizeof text);
    if (!HostName(text, FALSE))
        return Complain(dlg, PAGE_DNS, IDC_NP_HOST, "A host name has only letters, digits and hyphens.");
    GetDlgItemText(dlg, IDC_NP_DOMAIN, text, sizeof text);
    if (!HostName(text, TRUE))
        return Complain(dlg, PAGE_DNS, IDC_NP_DOMAIN, "A domain has only letters, digits, hyphens and dots.");
    for (id = IDC_NP_DNS1; id <= IDC_NP_DNS2; id += IDC_NP_DNS2 - IDC_NP_DNS1) {
        GetDlgItemText(dlg, id, text, sizeof text);
        if (text[0] && !ParseAddress(text, &value))
            return Complain(dlg, PAGE_DNS, id, "Type the DNS server's address, or leave this empty.");
    }
    return TRUE;
}

static BOOL CheckCard(HWND dlg)
{
    char text[16];
    unsigned value;

    if (!IsWindowEnabled(GetDlgItem(dlg, IDC_NP_IRQ)))
        return TRUE;
    value = GetDlgItemInt(dlg, IDC_NP_IRQ, NULL, FALSE);
    if (value < IRQ_FIRST || value > IRQ_LAST)
        return Complain(dlg, PAGE_CARD, IDC_NP_IRQ, "The interrupt is a number from 2 to 15.");
    GetDlgItemText(dlg, IDC_NP_IO, text, sizeof text);
    value = ParseHex(text);
    if (value < IO_FIRST || value > IO_LAST)
        return Complain(dlg, PAGE_CARD, IDC_NP_IO, "The I/O address is in hexadecimal, such as 300.");
    return TRUE;
}

/* A wrong interrupt hangs Windows as it starts: the one change here that can, so it is asked for twice. */
static BOOL ConfirmCard(HWND dlg)
{
    char now[16], text[16];

    if (!IsWindowEnabled(GetDlgItem(dlg, IDC_NP_IRQ)))
        return TRUE;
    GetPrivateProfileString(cardSection, "INTERRUPT", "", now, sizeof now, protocol);
    wsprintf(text, "%u", GetDlgItemInt(dlg, IDC_NP_IRQ, NULL, FALSE));
    if (!lstrcmp(text, now)) {
        GetPrivateProfileString(cardSection, "IOBASE", "", now, sizeof now, protocol);
        GetDlgItemText(dlg, IDC_NP_IO, text, sizeof text);
        if (ParseHex(text) == ParseHex(now))
            return TRUE;
    }
    if (MessageBox(dlg, "If these do not match the card, Windows may stop while it starts.\n\n"
                        "Should that happen, type WIN /N at the DOS prompt to start Windows without "
                        "the network, and correct them from Start, Settings, Network.\n\n"
                        "Do you want to change the card's settings?", TITLE,
                   MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2) == IDYES)
        return TRUE;
    TabsSelect(GetDlgItem(dlg, IDC_TABS), PAGE_CARD);
    TabsShowPage(dlg, PAGE_CARD, NULL);
    return FALSE;
}

static void Put(LPCSTR file, LPCSTR where, LPCSTR key, LPCSTR value)
{
    char now[96];

    GetPrivateProfileString(where, key, "", now, sizeof now, file);
    if (lstrcmp(now, value ? value : "")) {
        WritePrivateProfileString(where, key, value, file);
        changed = TRUE;
    }
}

static void Save(HWND dlg)
{
    char text[64], list[160];
    unsigned value;

    if (IsDlgButtonChecked(dlg, IDC_NP_AUTO)) {
        Put(SYSTEM_INI, section, "IPAddress", NO_ADDRESS);
        Put(SYSTEM_INI, section, "IPMask", NO_ADDRESS);
        Put(SYSTEM_INI, section, "DefaultGateway", NULL);
    } else {
        GetDlgItemText(dlg, IDC_NP_IP, text, sizeof text);
        Put(SYSTEM_INI, section, "IPAddress", text);
        GetDlgItemText(dlg, IDC_NP_MASK, text, sizeof text);
        Put(SYSTEM_INI, section, "IPMask", text);
        GetDlgItemText(dlg, IDC_NP_GATEWAY, text, sizeof text);
        Put(SYSTEM_INI, section, "DefaultGateway", text[0] ? text : NULL);
    }
    GetDlgItemText(dlg, IDC_NP_HOST, text, sizeof text);
    Put(SYSTEM_INI, "DNS", "HostName", text);
    GetDlgItemText(dlg, IDC_NP_DOMAIN, text, sizeof text);
    Put(SYSTEM_INI, "DNS", "DomainName", text);
    GetDlgItemText(dlg, IDC_NP_DNS1, list, sizeof list);
    GetDlgItemText(dlg, IDC_NP_DNS2, text, sizeof text);
    if (text[0]) {
        if (list[0])
            lstrcat(list, ",");
        lstrcat(list, text);
    }
    if (moreDns[0] && list[0]) {
        lstrcat(list, ",");
        lstrcat(list, moreDns);
    }
    Put(SYSTEM_INI, "DNS", "DNSServers", list);
    if (IsWindowEnabled(GetDlgItem(dlg, IDC_NP_IRQ))) {
        wsprintf(text, "%u", GetDlgItemInt(dlg, IDC_NP_IRQ, NULL, FALSE));
        Put(protocol, cardSection, "INTERRUPT", text);
        GetDlgItemText(dlg, IDC_NP_IO, text, sizeof text);
        value = ParseHex(text);
        wsprintf(text, "0x%03X", value);
        Put(protocol, cardSection, "IOBASE", text);
    }
    WritePrivateProfileString(NULL, NULL, NULL, SYSTEM_INI);    /* to the disk now */
    WritePrivateProfileString(NULL, NULL, NULL, protocol);
}

static void EnableFixed(HWND dlg)
{
    BOOL fixed = IsDlgButtonChecked(dlg, IDC_NP_FIXED);
    int id;

    for (id = IDC_NP_IP - 1; id <= IDC_NP_GATEWAY; id++)
        EnableWindow(GetDlgItem(dlg, id), fixed);
}

static void Load(HWND dlg)
{
    char text[96], rest[64];
    BOOL fixed;

    GetPrivateProfileString(section, "IPAddress", "", text, sizeof text, SYSTEM_INI);
    fixed = text[0] && lstrcmp(text, NO_ADDRESS);
    CheckRadioButton(dlg, IDC_NP_AUTO, IDC_NP_FIXED, fixed ? IDC_NP_FIXED : IDC_NP_AUTO);
    if (fixed) {
        SetDlgItemText(dlg, IDC_NP_IP, text);
        GetPrivateProfileString(section, "IPMask", "", text, sizeof text, SYSTEM_INI);
        SetDlgItemText(dlg, IDC_NP_MASK, text);
        GetPrivateProfileString(section, "DefaultGateway", "", text, sizeof text, SYSTEM_INI);
        SplitList(text, rest, sizeof rest);
        SetDlgItemText(dlg, IDC_NP_GATEWAY, text);
    }
    EnableFixed(dlg);

    GetPrivateProfileString("DNS", "HostName", "", text, sizeof text, SYSTEM_INI);
    SetDlgItemText(dlg, IDC_NP_HOST, text);
    GetPrivateProfileString("DNS", "DomainName", "", text, sizeof text, SYSTEM_INI);
    SetDlgItemText(dlg, IDC_NP_DOMAIN, text);
    GetPrivateProfileString("DNS", "DNSServers", "", text, sizeof text, SYSTEM_INI);
    SplitList(text, rest, sizeof rest);
    SetDlgItemText(dlg, IDC_NP_DNS1, text);
    SplitList(rest, moreDns, sizeof moreDns);
    SetDlgItemText(dlg, IDC_NP_DNS2, rest);

    GetPrivateProfileString(section, "Description", cardSection, text, sizeof text, SYSTEM_INI);
    SetDlgItemText(dlg, IDC_NP_CARD, text);
    text[0] = rest[0] = '\0';
    if (cardSection[0]) {
        GetPrivateProfileString(cardSection, "INTERRUPT", "", text, sizeof text, protocol);
        GetPrivateProfileString(cardSection, "IOBASE", "", rest, sizeof rest, protocol);
    }
    SetDlgItemText(dlg, IDC_NP_IRQ, text);
    SetDlgItemText(dlg, IDC_NP_IO, rest);
    /* A card that finds its own has neither: nothing to set */
    EnableWindow(GetDlgItem(dlg, IDC_NP_IRQ), text[0] != '\0');
    EnableWindow(GetDlgItem(dlg, IDC_NP_IO), rest[0] != '\0');
}

BOOL CALLBACK __export NetPropertiesProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "IP Address");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "DNS");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Card");
        TabsShowPage(dlg, PAGE_IP, NULL);
        SkinDialog(dlg);
        WindowCenter(dlg);
        Load(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDC_TABS:
            if (HIWORD(lp) == TCN_SELECT)
                TabsShowPage(dlg, TabsSelected(GetDlgItem(dlg, IDC_TABS)), NULL);
            return TRUE;
        case IDC_NP_AUTO:
        case IDC_NP_FIXED:
            EnableFixed(dlg);
            return TRUE;
        case IDOK:
            if ((IsDlgButtonChecked(dlg, IDC_NP_AUTO) || CheckAddresses(dlg)) && CheckDns(dlg) && CheckCard(dlg) &&
                ConfirmCard(dlg)) {
                Save(dlg);
                EndDialog(dlg, IDOK);
            }
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

void NetPropertiesShow(HWND owner)
{
    if (!NetPropertiesAvailable()) {
        MessageBox(owner, "TCP/IP is not set up on this computer. Use Windows Setup to add a network card and TCP/IP.",
                   TITLE, MB_ICONINFORMATION | MB_OK);
        return;
    }
    changed = FALSE;
    if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_NETPROPS), owner, (DLGPROC)NetPropertiesProc) == IDOK && changed &&
        MessageBox(owner, "You must restart Windows before the new settings will take effect.\n\n"
                          "Do you want to restart Windows now?", "System Settings Change",
                   MB_ICONQUESTION | MB_YESNO) == IDYES)
        ExitWindows(EW_RESTARTWINDOWS, 0);
}
