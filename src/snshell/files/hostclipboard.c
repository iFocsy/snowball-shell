/*
 * Text on the clipboard shared with the computer DOSBox-X runs on, through
 * DOSBox-X's CLIP$ device (w311.conf [dos] "dos clipboard device enable =
 * true"): reading the device gives the host's text, writing it sets the
 * host's. Windows' clipboard is watched as a clipboard viewer; the host's
 * is read every second. Without the device (another emulator, a real
 * computer) or with SNOWBALL.INI [Clipboard] ShareWithHost=0 nothing is
 * started. The device speaks the DOS code page: text crosses as OEM text.
 */
#include "internal.h"

#define DEVICE      "CLIP$"
#define CLASS       "SbHostClipboard"
#define MAX_TEXT    32000               /* more is cut there */
#define TIMER_POLL  1
#define POLL_MS     1000

static HWND watcher, nextViewer;
static DWORD shared;                    /* the sum of the text both sides have */
static BOOL setting;                    /* we are filling Windows' clipboard */

static DWORD Sum(LPCSTR text, int n)
{
    DWORD s = 0;

    while (n-- > 0)
        s = (s << 5 | s >> 27) ^ (BYTE)*text++;
    return s;
}

/* The device if this is DOSBox-X with it on: a device, not a file so named. */
static HFILE Device(int mode)
{
    HFILE f = _lopen(DEVICE, mode);

    if (f != HFILE_ERROR && !DosIsDevice(f)) {
        _lclose(f);
        f = HFILE_ERROR;
    }
    return f;
}

/* The host's text, OEM, in a new block of at most MAX_TEXT + 1 bytes. */
static HGLOBAL ReadHost(int *n)
{
    HGLOBAL h;
    LPSTR text;
    HFILE f;

    *n = 0;
    if ((f = Device(OF_READ)) == HFILE_ERROR)
        return NULL;
    if ((h = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, MAX_TEXT + 1)) != NULL) {
        text = GlobalLock(h);
        *n = _lread(f, text, MAX_TEXT);
        if (*n == (int)HFILE_ERROR)
            *n = 0;
        text[*n] = '\0';
        GlobalUnlock(h);
    }
    _lclose(f);
    return h;
}

/* New text on the host: Windows' clipboard gets it. */
static void FromHost(void)
{
    HGLOBAL h;
    LPSTR text;
    DWORD sum;
    int n;

    if ((h = ReadHost(&n)) == NULL)
        return;
    text = GlobalLock(h);
    sum = Sum(text, n);
    if (n == 0 || sum == shared) {      /* nothing (or not text), or nothing new */
        GlobalUnlock(h);
        GlobalFree(h);
        return;
    }
    shared = sum;
    OemToAnsi(text, text);
    GlobalUnlock(h);
    h = GlobalReAlloc(h, n + 1, GMEM_MOVEABLE | GMEM_DDESHARE);
    if (h && OpenClipboard(watcher)) {
        setting = TRUE;
        EmptyClipboard();
        SetClipboardData(CF_TEXT, h);
        CloseClipboard();
        setting = FALSE;
    } else if (h) {
        GlobalFree(h);
    }
}

/* New text on Windows' clipboard: the host gets it. What the host keeps
   (it may change the line ends) is read back, so it is not taken for new. */
static void ToHost(void)
{
    HGLOBAL h, copy;
    LPSTR text;
    HFILE f;
    int n;

    if (setting || !IsClipboardFormatAvailable(CF_TEXT) || !OpenClipboard(watcher))
        return;
    copy = NULL;
    if ((h = GetClipboardData(CF_TEXT)) != NULL && (text = GlobalLock(h)) != NULL) {
        n = (int)min((DWORD)lstrlen(text), (DWORD)MAX_TEXT);
        if ((copy = GlobalAlloc(GMEM_MOVEABLE, n + 1)) != NULL) {
            LPSTR oem = GlobalLock(copy);
            _fmemcpy(oem, text, n);
            oem[n] = '\0';
            AnsiToOem(oem, oem);
            if ((f = Device(OF_WRITE)) != HFILE_ERROR) {
                _lwrite(f, oem, n);
                _lclose(f);
            }
            GlobalUnlock(copy);
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
    if (copy)
        GlobalFree(copy);
    if ((h = ReadHost(&n)) != NULL) {
        shared = Sum(GlobalLock(h), n);
        GlobalUnlock(h);
        GlobalFree(h);
    }
}

LRESULT CALLBACK __export HostClipboardProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        FromHost();
        return 0;
    case WM_DRAWCLIPBOARD:
        ToHost();
        if (nextViewer)
            SendMessage(nextViewer, msg, wp, lp);
        return 0;
    case WM_CHANGECBCHAIN:
        if ((HWND)wp == nextViewer)
            nextViewer = (HWND)LOWORD(lp);
        else if (nextViewer)
            SendMessage(nextViewer, msg, wp, lp);
        return 0;
    case WM_DESTROY:
        ChangeClipboardChain(w, nextViewer);
        KillTimer(w, TIMER_POLL);
        watcher = NULL;
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

/* At the start the host's text comes over (at the first second); Windows'
   clipboard, empty then, does not go over it. */
void HostClipboardStart(void)
{
    WNDCLASS wc;
    HFILE f;

    if (watcher || !SettingsGetInt("Clipboard", "ShareWithHost", 1) || (f = Device(OF_READ)) == HFILE_ERROR)
        return;
    _lclose(f);
    _fmemset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = HostClipboardProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = CLASS;
    RegisterClass(&wc);
    if ((watcher = CreateWindow(CLASS, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL)) == NULL)
        return;
    setting = TRUE;                     /* a new viewer is told of the clipboard at once */
    nextViewer = SetClipboardViewer(watcher);
    setting = FALSE;
    SetTimer(watcher, TIMER_POLL, POLL_MS, NULL);
}
