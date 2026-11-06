/*
 * What Windows for Workgroups' own network software says of the network card,
 * for any card with a Windows driver, through WINSOCK.DLL's WSCONTROL (its
 * export 1001): the TDI queries of Windows 95's WsControl, which grew from this
 * stack. It has no documentation here; what is used below was tried on
 * Microsoft TCP/IP-32 3.11b.
 *
 * No card in SYSTEM.INI: Winsock is not even loaded. A card that did not start,
 * or no TCP/IP, leaves the stack without an interface: no network either.
 */
#include "internal.h"
#include "taskbarint.h"

#define ORD_WSASTARTUP 115
#define ORD_WSACLEANUP 116
#define ORD_WSCONTROL  1001
#define WINSOCK_11     0x0101

#define TDI_TCP        6                /* WSCONTROL's first argument */
#define WSC_QUERY      0
#define WSC_ECHO       2

#define ENTITY_GENERIC 0
#define ENTITY_IF      0x200
#define ENTITY_IP      0x301
#define CLASS_GENERIC  0x100
#define CLASS_PROTOCOL 0x200
#define TYPE_PROVIDER  0x100
#define ID_ENTITIES    0
#define ID_STATISTICS  1                /* of an interface */
#define ID_ROUTES      0x101            /* of IP */
#define ID_ADDRESSES   0x102

#define IF_LOOPBACK    24
#define MAX_ENTITIES   32
#define ROW_ADDRESS    24               /* bytes of a row of IP's tables */
#define ROW_ROUTE      48

/* An interface's record, in DWORDs; its 8-byte address is at 5 */
#define IF_INDEX       0
#define IF_TYPE        1
#define IF_SPEED       3
#define IF_MACSIZE     4
#define IF_IN_BYTES    10
#define IF_IN_UNICAST  11
#define IF_IN_OTHER    12
#define IF_OUT_BYTES   16
#define IF_OUT_UNICAST 17
#define IF_OUT_OTHER   18
#define IF_NAME_SIZE   22
#define IF_NAME        23

typedef struct {
    DWORD entity, instance;
} ENTITY;

typedef struct {
    ENTITY who;
    DWORD  klass, type, id;
    BYTE   context[16];
} QUERY;

typedef DWORD (FAR PASCAL *WSCONTROLPROC)(DWORD, DWORD, void FAR *, DWORD FAR *, void FAR *, DWORD FAR *);

static HINSTANCE winsock;
static WSCONTROLPROC control;
static ENTITY card, ip;
static DWORD cardIndex;
static BOOL dhcp;
static BYTE buffer[2048];               /* the answers; routes are the longest */

static DWORD Query(const ENTITY *who, DWORD klass, DWORD id, void FAR *out, DWORD *size)
{
    static QUERY q;
    DWORD in = sizeof q;

    _fmemset(&q, 0, sizeof q);
    q.who = *who;
    q.klass = klass;
    q.type = TYPE_PROVIDER;
    q.id = id;
    return control(TDI_TCP, WSC_QUERY, &q, &in, out, size);
}

/* The first interface with a hardware address, and IP. */
static BOOL FindCard(void)
{
    static ENTITY list[MAX_ENTITIES];
    ENTITY generic = { ENTITY_GENERIC, 0 };
    DWORD size = sizeof list, n, i;
    DWORD FAR *record = (DWORD FAR *)buffer;
    BOOL found = FALSE, hasIp = FALSE;

    if (Query(&generic, CLASS_GENERIC, ID_ENTITIES, list, &size))
        return FALSE;
    n = size / sizeof list[0];
    for (i = 0; i < n; i++) {
        if (list[i].entity == ENTITY_IP && !hasIp) {
            ip = list[i];
            hasIp = TRUE;
        } else if (list[i].entity == ENTITY_IF && !found) {
            size = sizeof buffer;
            if (!Query(&list[i], CLASS_PROTOCOL, ID_STATISTICS, buffer, &size) &&
                record[IF_TYPE] != IF_LOOPBACK && record[IF_MACSIZE]) {
                card = list[i];
                cardIndex = record[IF_INDEX];
                found = TRUE;
            }
        }
    }
    return found && hasIp;
}

/* Windows keeps 0.0.0.0 for an address the network gives. */
static void ReadDhcp(void)
{
    char name[48], value[20], *comma;

    GetPrivateProfileString("MSTCP", "Interfaces", "", name, sizeof name, "SYSTEM.INI");
    comma = strchr(name, ',');
    if (comma)
        *comma = '\0';
    GetPrivateProfileString(name, "IPAddress", "", value, sizeof value, "SYSTEM.INI");
    dhcp = !value[0] || !lstrcmp(value, "0.0.0.0");
}

BOOL NetInfoOpen(void)
{
    static BYTE data[400];              /* a WSADATA */
    int (FAR PASCAL *startup)(WORD, void FAR *);
    char driver[16];
    UINT mode;

    if (winsock)
        return TRUE;
    if (!GetPrivateProfileString("386Enh", "netcard", "", driver, sizeof driver, "SYSTEM.INI"))
        return FALSE;
    mode = SetErrorMode(SEM_NOOPENFILEERRORBOX);
    winsock = LoadLibrary("WINSOCK.DLL");
    SetErrorMode(mode);
    if ((UINT)winsock < 32) {
        winsock = NULL;
        return FALSE;
    }
    *(FARPROC *)&startup = GetProcAddress(winsock, MAKEINTRESOURCE(ORD_WSASTARTUP));
    control = (WSCONTROLPROC)GetProcAddress(winsock, MAKEINTRESOURCE(ORD_WSCONTROL));
    if (startup && control && !startup(WINSOCK_11, data)) {
        if (FindCard()) {
            ReadDhcp();
            return TRUE;
        }
        NetInfoClose();
        return FALSE;
    }
    FreeLibrary(winsock);
    winsock = NULL;
    return FALSE;
}

void NetInfoClose(void)
{
    if (winsock) {
        int (FAR PASCAL *cleanup)(void);
        *(FARPROC *)&cleanup = GetProcAddress(winsock, MAKEINTRESOURCE(ORD_WSACLEANUP));
        if (cleanup)
            cleanup();
        FreeLibrary(winsock);
        winsock = NULL;
    }
}

static void ReadAddress(NETINFO *n)
{
    DWORD size = sizeof buffer, i;
    DWORD FAR *row;

    n->address = n->mask = n->gateway = 0;
    if (Query(&ip, CLASS_PROTOCOL, ID_ADDRESSES, buffer, &size))
        return;
    for (i = 0; i + ROW_ADDRESS <= size; i += ROW_ADDRESS) {
        row = (DWORD FAR *)(buffer + i);
        if (row[1] == cardIndex) {
            n->address = row[0];
            n->mask = row[2];
        }
    }
    size = sizeof buffer;
    if (Query(&ip, CLASS_PROTOCOL, ID_ROUTES, buffer, &size))
        return;
    for (i = 0; i + ROW_ROUTE <= size; i += ROW_ROUTE) {
        row = (DWORD FAR *)(buffer + i);
        if (!row[0] && !row[10])        /* to anywhere: the way out */
            n->gateway = row[6];
    }
}

BOOL NetInfoRead(NETINFO *n)
{
    DWORD size = sizeof buffer;
    DWORD FAR *record = (DWORD FAR *)buffer;

    if (!winsock || Query(&card, CLASS_PROTOCOL, ID_STATISTICS, buffer, &size))
        return FALSE;
    n->speed = record[IF_SPEED];
    n->inBytes = record[IF_IN_BYTES];
    n->outBytes = record[IF_OUT_BYTES];
    n->inPackets = record[IF_IN_UNICAST] + record[IF_IN_OTHER];
    n->outPackets = record[IF_OUT_UNICAST] + record[IF_OUT_OTHER];
    _fmemcpy(n->mac, &record[5], sizeof n->mac);
    if (size > IF_NAME * sizeof(DWORD) && record[IF_NAME_SIZE])
        lstrcpyn(n->card, (LPCSTR)&record[IF_NAME], sizeof n->card);
    n->dhcp = dhcp;
    ReadAddress(n);
    return TRUE;
}

/*
 * A ping through the stack, TRUE when answered. Windows stands still until
 * it returns: at once when answered, and for some 0.2 to 0.4 seconds when not,
 * whatever ms says, so it is asked rarely and with a short ms.
 */
BOOL NetInfoPing(DWORD address, WORD ms)
{
    static BYTE in[52], out[128];
    DWORD inSize = sizeof in, outSize = sizeof out, timeout = ms;

    _fmemset(in, 0, sizeof in);
    _fmemcpy(in, &address, 4);
    _fmemcpy(in + 4, &timeout, 4);
    in[8] = 20;                         /* where the data starts */
    in[10] = 32;                        /* its size */
    in[13] = 32;                        /* time to live */
    _fmemset(in + 20, 'a', 32);
    if (control(TDI_TCP, WSC_ECHO, in, &inSize, out, &outSize) || outSize < 8)
        return FALSE;
    return !((DWORD FAR *)out)[1];      /* the status: 0, or why not */
}
