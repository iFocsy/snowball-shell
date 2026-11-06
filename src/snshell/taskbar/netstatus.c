/*
 * The network's status window, as Windows XP's: General, how the connection
 * is and what went through it; Support, its addresses and the card.
 */
#include "internal.h"
#include "taskbarint.h"

static HWND status;

void NetAddressText(DWORD address, LPSTR out)
{
    const BYTE FAR *b = (const BYTE FAR *)&address;
    wsprintf(out, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
}

/* The text of a control, only if it changed: no flicker. */
static void Put(HWND dlg, int id, LPCSTR text)
{
    char now[64];

    GetDlgItemText(dlg, id, now, sizeof now);
    if (lstrcmp(now, text))
        SetDlgItemText(dlg, id, text);
}

static void PutAddress(HWND dlg, int id, DWORD address)
{
    char text[16];

    NetAddressText(address, text);
    Put(dlg, id, address ? text : "");
}

/* With thousands separated. */
static void PutCount(HWND dlg, int id, DWORD value)
{
    char digits[12], text[16];
    int n, i, k = 0;

    n = wsprintf(digits, "%lu", value);
    for (i = 0; i < n; i++) {
        text[k++] = digits[i];
        if (i < n - 1 && (n - 1 - i) % 3 == 0)
            text[k++] = ',';
    }
    text[k] = '\0';
    Put(dlg, id, text);
}

static void Refresh(HWND dlg)
{
    const NETINFO *n = TrayNetInfo();
    BOOL connected = TrayNetState() == NETS_CONNECTED;
    DWORD seconds = (GetTickCount() - TrayNetSince()) / 1000;
    char text[48];

    Put(dlg, IDC_NET_STATUS, TrayNetStateText());
    if (connected)
        wsprintf(text, "%02lu:%02lu:%02lu", seconds / 3600, seconds / 60 % 60, seconds % 60);
    else
        text[0] = '\0';
    Put(dlg, IDC_NET_DURATION, text);
    wsprintf(text, "%lu.%lu Mbps", n->speed / 1000000L, n->speed / 100000L % 10);
    Put(dlg, IDC_NET_SPEED, text);
    PutCount(dlg, IDC_NET_SENT_PACKETS, n->outPackets);
    PutCount(dlg, IDC_NET_IN_PACKETS, n->inPackets);
    PutCount(dlg, IDC_NET_SENT_BYTES, n->outBytes);
    PutCount(dlg, IDC_NET_IN_BYTES, n->inBytes);
    SendDlgItemMessage(dlg, IDC_NET_PICTURE, STM_SETICON, (WPARAM)TrayNetIcon(), 0L);

    Put(dlg, IDC_NET_TYPE, n->dhcp ? "Assigned by DHCP" : "Manually Configured");
    PutAddress(dlg, IDC_NET_ADDRESS, n->address);
    PutAddress(dlg, IDC_NET_MASK, n->mask);
    PutAddress(dlg, IDC_NET_GATEWAY, n->gateway);
    Put(dlg, IDC_NET_CARD, n->card);
    wsprintf(text, "%02X-%02X-%02X-%02X-%02X-%02X", n->mac[0], n->mac[1], n->mac[2], n->mac[3], n->mac[4], n->mac[5]);
    Put(dlg, IDC_NET_MAC, text);
}

BOOL CALLBACK __export NetStatusProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "General");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "Support");
        TabsShowPage(dlg, 0, NULL);
        SkinDialog(dlg);
        WindowCenter(dlg);
        Refresh(dlg);
        return TRUE;
    case WM_ACTIVATE:
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        return FALSE;
    case WM_COMMAND:
        if (wp == IDC_TABS && HIWORD(lp) == TCN_SELECT)
            TabsShowPage(dlg, TabsSelected(GetDlgItem(dlg, IDC_TABS)), NULL);
        else if (wp == IDC_NET_PROPERTIES)
            NetPropertiesShow(dlg);
        else if (wp == IDOK || wp == IDCANCEL)
            DestroyWindow(dlg);
        return TRUE;
    case WM_DESTROY:
        MessageLoopTrackDialog(dlg, FALSE);
        status = NULL;
        return TRUE;
    }
    return FALSE;
}

void NetStatusShow(void)
{
    if (status)
        SetActiveWindow(status);
    else if ((status = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_NETSTATUS), NULL, NetStatusProc)) != NULL)
        ShowWindow(status, SW_SHOWNORMAL);
}

void NetStatusRefresh(void)
{
    if (status)
        Refresh(status);
}
