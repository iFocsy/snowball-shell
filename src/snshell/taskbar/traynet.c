/*
 * The network's tray icon, as on Windows XP: shown only with a network card
 * that started, its two computers lit by traffic, crossed out when not connected.
 *
 * A card says nothing of its cable, so "connected" is found out. While traffic
 * comes in nothing need be asked. A line gone quiet makes it ask the gateway, by
 * ping: answered at once when connected, but a ping nobody answers holds all
 * of Windows (netinfo.c), so when not connected it asks seldom.
 */
#include "internal.h"
#include "taskbarint.h"

#define CHECK_MS    2000
#define QUIET_TICKS 10                  /* of silence before the gateway is asked: 20 s */
#define RETRY_TICKS 15                  /* not connected: asked every 30 s */
#define HEARD_TICKS 5                   /* ... but in 10 s if something was received meanwhile */
#define AGAIN_TICKS 3                   /* a first ping unanswered: asked again in 6 s */
#define PING_MS     100
#define MISSES      2                   /* pings unanswered in a row that mean not connected */
#define LIGHT_OUT   1
#define LIGHT_IN    2
#define TTC_STATUS  1
#define TTC_SETUP   2
#define TTC_PROPERTIES 3

static BOOL present, heardDown;
static NETINFO info;
static int state = NETS_ACQUIRING, lights, quiet, misses;
static DWORD lastIn, lastOut, connectedAt;
static HICON icons[IDI_NETWARN - IDI_NETIDLE + 1];

BOOL TrayNetShown(void)
{
    return present;
}

const NETINFO *TrayNetInfo(void)
{
    return &info;
}

int TrayNetState(void)
{
    return state;
}

DWORD TrayNetSince(void)
{
    return connectedAt;
}

LPCSTR TrayNetStateText(void)
{
    switch (state) {
    case NETS_CONNECTED: return "Connected";
    case NETS_DOWN:      return "Not connected";
    case NETS_LIMITED:   return "Limited or no connectivity";
    default:             return "Acquiring network address";
    }
}

HICON TrayNetIcon(void)
{
    int id = state == NETS_DOWN ? IDI_NETDOWN : state != NETS_CONNECTED ? IDI_NETWARN : IDI_NETIDLE + lights;
    int k = id - IDI_NETIDLE;

    if (!icons[k]) {
        icons[k] = LoadIcon(g_hInst, MAKEINTRESOURCE(id));
        if (icons[k])
            DrawIconImages(icons[k], g_hInst, MAKEINTRESOURCE(id));
    }
    return icons[k];
}

/*
 * With an address: what the line shows. Traffic only keeps a connection that
 * was proven: being connected takes the gateway's answer, as a card can
 * receive a server's broadcasts, or its driver's own echoes, on a dead line.
 */
static int Judge(BOOL heard)
{
    int wait = QUIET_TICKS;

    if (!info.gateway) {
        quiet = misses = 0;
        return NETS_CONNECTED;
    }
    if (state == NETS_CONNECTED && heard) {
        quiet = misses = 0;
        return NETS_CONNECTED;
    }
    if (state == NETS_DOWN) {
        heardDown |= heard;
        wait = heardDown ? HEARD_TICKS : RETRY_TICKS;
    }
    if (++quiet < wait)
        return state;
    quiet = 0;
    heardDown = FALSE;
    if (NetInfoPing(info.gateway, PING_MS)) {
        misses = 0;
        return NETS_CONNECTED;
    }
    if (state != NETS_DOWN && ++misses < MISSES) {
        quiet = QUIET_TICKS - AGAIN_TICKS;
        return state;
    }
    misses = 0;
    return NETS_DOWN;
}

void TrayNetTimer(void)
{
    int oldState = state, oldLights = lights;
    BOOL heard;

    if (!NetInfoRead(&info))
        return;
    heard = info.inBytes != lastIn;
    lights = (heard ? LIGHT_IN : 0) | (info.outBytes != lastOut ? LIGHT_OUT : 0);
    lastIn = info.inBytes;
    lastOut = info.outBytes;

    if (!info.address) {
        state = NETS_ACQUIRING;
        quiet = QUIET_TICKS;            /* with an address, asked at once */
    } else if ((info.address & 0xFFFF) == 0xFEA9) {
        state = NETS_LIMITED;           /* 169.254.x.x: nobody gave it one */
    } else {
        state = Judge(heard);
    }
    if (state == NETS_CONNECTED && oldState != NETS_CONNECTED)
        connectedAt = GetTickCount();
    if (state != oldState || lights != oldLights)
        TrayToolsRedrawNet();
    NetStatusRefresh();
}

void TrayNetLoad(void)
{
    if (present || !NetInfoOpen())
        return;
    present = TRUE;
    quiet = QUIET_TICKS;
    SetTimer(g_taskbar, TIMER_NET, CHECK_MS, NULL);
    TrayNetTimer();
}

void TrayNetClose(void)
{
    if (present) {
        KillTimer(g_taskbar, TIMER_NET);
        NetInfoClose();
        present = FALSE;
    }
}

void TrayNetTip(LPSTR out)
{
    char address[16];

    if (info.address) {
        NetAddressText(info.address, address);
        wsprintf(out, "Network: %s, %s", (LPCSTR)TrayNetStateText(), (LPSTR)address);
    } else {
        wsprintf(out, "Network: %s", (LPCSTR)TrayNetStateText());
    }
}

void TrayNetMenu(void)
{
    HMENU m = CreatePopupMenu();
    UINT cmd;

    AppendMenu(m, MF_STRING, TTC_STATUS, "&Status");
    AppendMenu(m, MF_STRING, TTC_PROPERTIES, "&Properties");
    AppendMenu(m, MF_STRING, TTC_SETUP, "Windows Set&up...");
    TaskbarMenuUp = TRUE;
    cmd = PopupMenuTrack(m, g_taskbar);
    TaskbarMenuUp = FALSE;
    DestroyMenu(m);
    if (cmd == TTC_STATUS)
        NetStatusShow();
    else if (cmd == TTC_PROPERTIES)
        NetPropertiesShow(g_taskbar);
    else if (cmd == TTC_SETUP)
        WinExec("WINSETUP.EXE", SW_SHOWNORMAL);
}
