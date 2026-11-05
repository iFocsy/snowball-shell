/*
 * The tray's own icons, after the programs' and before the time, as
 * Windows 98's: Resource Meter (the free System, User and GDI resources as
 * three bars; opened from System Tools, closed from its menu) and Volume
 * (when a sound card's driver can set it: a click shows its slider and
 * Mute), with Windows 98's icons (RSRCMTR.EXE's and SNDVOL32.EXE's, in W98ART.DLL).
 */
#include "internal.h"
#include "taskbarint.h"
#include <mmsystem.h>

#define METER_MS    5000                /* the meter read again */
#define STEPS       16                  /* the volume slider's */
#define TTC_DETAILS 1                   /* the icons' menus */
#define TTC_EXIT    2
#define TTC_MUTE    3
#define TTC_VOLUME  4

static BOOL meter, volume;
static BYTE level[3];                   /* System, User, GDI: % free */
static HWND details, slider;            /* their windows, while up */

static BOOL Muted(void)
{
    return SettingsGetInt("Volume", "Muted", 0);
}

int TrayToolsCount(void)
{
    return meter + volume;
}

/* The tool at the tray's icon i (after the programs'): 0 the meter, 1 Volume. */
static int Tool(int i)
{
    return i == 0 && meter ? 0 : 1;
}

/* TRUE when a figure changed. */
static BOOL Read(void)
{
    BYTE now[3];

    now[0] = (BYTE)GetFreeSystemResources(GFSR_SYSTEMRESOURCES);
    now[1] = (BYTE)GetFreeSystemResources(GFSR_USERRESOURCES);
    now[2] = (BYTE)GetFreeSystemResources(GFSR_GDIRESOURCES);
    if (!_fmemcmp(now, level, sizeof now))
        return FALSE;
    _fmemcpy(level, now, sizeof now);
    return TRUE;
}

/* Which are shown: [Taskbar] ResourceMeter; a wave device that sets its volume. */
void TrayToolsLoad(void)
{
    WAVEOUTCAPS caps;

    meter = SettingsGetInt("Taskbar", "ResourceMeter", 0);
    volume = waveOutGetNumDevs() && !waveOutGetDevCaps(0, &caps, sizeof caps) && (caps.dwSupport & WAVECAPS_VOLUME);
    if (meter) {
        Read();
        SetTimer(g_taskbar, TIMER_METER, METER_MS, NULL);
    } else {
        KillTimer(g_taskbar, TIMER_METER);
    }
}

/* System Tools' Resource Meter ("SNOWBALL.EXE /RESMETER"), or its Exit. */
void TrayToolsShowMeter(BOOL on)
{
    SettingsSetInt("Taskbar", "ResourceMeter", on);
    TrayToolsLoad();
    if (!on && details)
        DestroyWindow(details);
    InvalidateRect(g_taskbar, NULL, FALSE);
}

void TrayToolsTimer(void)
{
    if (Read()) {
        InvalidateRect(g_taskbar, NULL, FALSE);
        if (details)
            InvalidateRect(details, NULL, FALSE);
    }
}

/* Windows 98's own icons: the meter's by the lowest of the three (13 steps, empty last). */
void TrayToolsPaint(HDC dc, int i, const RECT *r)
{
    int id = Tool(i) == 0 ? IDI_METER + (100 - min(level[0], 100)) * 12 / 100 : Muted() ? IDI_MUTED : IDI_SPEAKER;

    DrawIconImage(dc, r->left, r->top, IconStock(id), TRUE);
}

void TrayToolsTip(int i, LPSTR out)
{
    if (Tool(i) == 0)
        wsprintf(out, "System %u%%, User %u%%, GDI %u%% free", level[0], level[1], level[2]);
    else
        lstrcpy(out, Muted() ? "Volume (muted)" : "Volume");
}

/* ---- Volume ---- */

static void SetVolume(int pos)
{
    WORD v = (WORD)((DWORD)0xFFFF * pos / (STEPS - 1));

    waveOutSetVolume(0, MAKELONG(v, v));
}

static void Mute(BOOL on)
{
    DWORD v;

    SettingsSetInt("Volume", "Muted", on);
    if (on) {
        waveOutGetVolume(0, &v);
        SettingsSetInt("Volume", "Level", (int)(LOWORD(v) / (0xFFFF / (STEPS - 1))));
        waveOutSetVolume(0, 0L);
    } else {
        SetVolume(SettingsGetInt("Volume", "Level", STEPS - 1));
    }
    if (slider)
        CheckDlgButton(slider, IDC_VOLUME_MUTE, on);
    InvalidateRect(g_taskbar, NULL, FALSE);
}

BOOL CALLBACK __export TrayToolsVolumeProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    DWORD v;

    switch (msg) {
    case WM_INITDIALOG:
        SkinDialog(dlg);
        SliderSetCount(GetDlgItem(dlg, IDC_VOLUME_SLIDER), STEPS);
        if (Muted())
            SliderSetPos(GetDlgItem(dlg, IDC_VOLUME_SLIDER), SettingsGetInt("Volume", "Level", STEPS - 1));
        else if (!waveOutGetVolume(0, &v))
            SliderSetPos(GetDlgItem(dlg, IDC_VOLUME_SLIDER), (int)(LOWORD(v) / (0xFFFF / (STEPS - 1))));
        CheckDlgButton(dlg, IDC_VOLUME_MUTE, Muted());
        return TRUE;
    case WM_ACTIVATE:                   /* goes when anything else is clicked */
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        if (wp == WA_INACTIVE)
            PostMessage(dlg, WM_CLOSE, 0, 0L);
        return FALSE;
    case WM_COMMAND:
        if (wp == IDC_VOLUME_SLIDER && HIWORD(lp) == SLN_CHANGE) {
            if (Muted()) {              /* moved: heard again */
                SettingsSetInt("Volume", "Muted", FALSE);
                CheckDlgButton(dlg, IDC_VOLUME_MUTE, FALSE);
                InvalidateRect(g_taskbar, NULL, FALSE);
            }
            SetVolume(SliderPos(GetDlgItem(dlg, IDC_VOLUME_SLIDER)));
        } else if (wp == IDC_VOLUME_MUTE) {
            Mute(IsDlgButtonChecked(dlg, IDC_VOLUME_MUTE));
        } else if (wp == IDCANCEL) {
            PostMessage(dlg, WM_CLOSE, 0, 0L);
        }
        return TRUE;
    case WM_CLOSE:
        DestroyWindow(dlg);
        return TRUE;
    case WM_DESTROY:
        MessageLoopTrackDialog(dlg, FALSE);
        slider = NULL;
        return TRUE;
    }
    return FALSE;
}

/* The slider by the icon, on the taskbar's inner side. */
static void ShowSlider(int i)
{
    RECT icon, r;

    if (slider || (slider = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_VOLUME), g_taskbar, TrayToolsVolumeProc)) == NULL)
        return;
    TaskbarTrayIconRect(TaskbarNTray + i, &icon);
    MapWindowPoints(g_taskbar, NULL, (POINT FAR *)&icon, 2);
    GetWindowRect(slider, &r);
    OffsetRect(&r, (icon.left + icon.right) / 2 - (r.left + r.right) / 2,
               TaskbarSide == TBE_TOP ? icon.bottom + 8 - r.top : icon.top - 8 - r.bottom);
    if (TaskbarVertical())
        OffsetRect(&r, TaskbarSide == TBE_LEFT ? icon.right + 8 - r.left : icon.left - 8 - r.right,
                   icon.top - r.top);
    OffsetRect(&r, max(0, -r.left) - max(0, r.right - GetSystemMetrics(SM_CXSCREEN)), 0);
    SetWindowPos(slider, HWND_TOPMOST, r.left, r.top, 0, 0, SWP_NOSIZE);
    ShowWindow(slider, SW_SHOW);
    SetActiveWindow(slider);
}

/* ---- Resource Meter ---- */

BOOL CALLBACK __export TrayToolsMeterProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    char text[16];
    RECT r;
    int k;

    switch (msg) {
    case WM_INITDIALOG:
        for (k = 0; k < 3; k++)         /* the bars' places, painted over */
            ShowWindow(GetDlgItem(dlg, IDC_RESMETER_BAR + k), SW_HIDE);
        SkinDialog(dlg);
        WindowCenter(dlg);
        return TRUE;
    case WM_ACTIVATE:
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        return FALSE;
    case WM_PAINT:                      /* each bar on its hidden placeholder */
        BeginPaint(dlg, &ps);
        for (k = 0; k < 3; k++) {
            WindowRectIn(GetDlgItem(dlg, IDC_RESMETER_BAR + k), dlg, &r);
            PaintProgressBar(ps.hdc, &r, level[k] * 10, GetSysColor(COLOR_HIGHLIGHT));
            wsprintf(text, "%u%% free", level[k]);
            SetDlgItemText(dlg, IDC_RESMETER_FREE + k, text);
        }
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK || wp == IDCANCEL)
            DestroyWindow(dlg);
        return TRUE;
    case WM_DESTROY:
        MessageLoopTrackDialog(dlg, FALSE);
        details = NULL;
        return TRUE;
    }
    return FALSE;
}

static void ShowDetails(void)
{
    if (details)
        SetActiveWindow(details);
    else if ((details = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_RESMETER), NULL, TrayToolsMeterProc)) != NULL)
        ShowWindow(details, SW_SHOWNORMAL);
}

/* ---- clicks ---- */

void TrayToolsClick(int i)
{
    if (Tool(i) == 0)
        ShowDetails();
    else
        ShowSlider(i);
}

void TrayToolsMenu(int i)
{
    HMENU m = CreatePopupMenu();
    POINT p;
    UINT cmd;

    if (Tool(i) == 0) {
        AppendMenu(m, MF_STRING, TTC_DETAILS, "&Details...");
        AppendMenu(m, MF_STRING, TTC_EXIT, "E&xit");
    } else {
        AppendMenu(m, MF_STRING, TTC_VOLUME, "&Volume...");
        AppendMenu(m, MF_STRING | (Muted() ? MF_CHECKED : 0), TTC_MUTE, "&Mute");
    }
    GetCursorPos(&p);
    TaskbarMenuUp = TRUE;
    cmd = PopupMenuTrack(m, g_taskbar);
    TaskbarMenuUp = FALSE;
    DestroyMenu(m);
    switch (cmd) {
    case TTC_DETAILS: ShowDetails(); break;
    case TTC_EXIT:    TrayToolsShowMeter(FALSE); break;
    case TTC_VOLUME:  ShowSlider(i); break;
    case TTC_MUTE:    Mute(!Muted()); break;
    }
}

void TaskbarResourceMeter(void)
{
    TrayToolsShowMeter(TRUE);
}
