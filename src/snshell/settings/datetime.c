/*
 * Date/Time Properties, as Windows 98's Date & Time page (Windows 3.1 has
 * no time zones): the month and year, a calendar to pick the day, a clock
 * face ticking with the time typed below it. Apply sets DOS's date and
 * time and tells every window (WM_TIMECHANGE). Opened by a double click on
 * the taskbar's clock, or Adjust Date/Time on its menu.
 */
#include "internal.h"
#include <dos.h>

#define TIMER_TICK  1
#define FIRST_YEAR  1980                /* DOS's dates */
#define YEARS       120

static const LPCSTR months[12] = { "January", "February", "March", "April", "May", "June", "July", "August",
                                   "September", "October", "November", "December" };
static const int sin6[16] = { 0, 105, 208, 309, 407, 500, 588, 669, 743, 809, 866, 914, 951, 978, 995, 1000 };
static HWND sheet;
static int day, month, year;            /* the date shown (month 1..12) */
static long seconds;                    /* the time shown, of the day */
static BOOL typing;                     /* the time field changed by hand: read at Apply */
static BOOL showing;                    /* ... not by hand: ShowTime's */

static int MonthDays(int m, int y)
{
    static const BYTE n[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    return n[m - 1] + (m == 2 && y % 4 == 0);
}

/* 0 Sunday .. 6 Saturday (1 Jan 1980 was a Tuesday). */
static int WeekDay(int d, int m, int y)
{
    long n = (long)(y - 1980) * 365 + (y - 1977) / 4 + d - 1;
    int i;

    for (i = 1; i < m; i++)
        n += MonthDays(i, y);
    return (int)((n + 2) % 7);
}

/* sin and cos of a 60th of a turn from 12 o'clock, in thousandths. */
static int Sin60(int i)
{
    i %= 60;
    return i <= 15 ? sin6[i] : i <= 30 ? sin6[30 - i] : i <= 45 ? -sin6[i - 30] : -sin6[60 - i];
}

static void ShowTime(HWND dlg)
{
    char text[20], sep[4], am[8], pm[8];
    int h = (int)(seconds / 3600), m = (int)(seconds / 60 % 60), s = (int)(seconds % 60);

    GetProfileString("intl", "sTime", ":", sep, sizeof sep);
    if (GetProfileInt("intl", "iTime", 0)) {
        wsprintf(text, "%d%s%02d%s%02d", h, (LPSTR)sep, m, (LPSTR)sep, s);
    } else {
        GetProfileString("intl", "s1159", "AM", am, sizeof am);
        GetProfileString("intl", "s2359", "PM", pm, sizeof pm);
        wsprintf(text, "%d%s%02d%s%02d %s", h % 12 ? h % 12 : 12, (LPSTR)sep, m, (LPSTR)sep, s, (LPSTR)(h < 12 ? am : pm));
    }
    showing = TRUE;
    SetDlgItemText(dlg, IDC_DT_TIME, text);
    showing = FALSE;
    typing = FALSE;
}

/* "10:05:30 PM", "22:05" -> seconds; FALSE if it is no time. */
static BOOL ReadTime(HWND dlg, long *out)
{
    char text[24];
    LPSTR p = text;
    int part[3] = { 0, 0, 0 }, n;

    GetDlgItemText(dlg, IDC_DT_TIME, text, sizeof text);
    for (n = 0; n < 3 && *p; n++) {
        while (*p == ' ')
            p++;
        if (*p < '0' || *p > '9')
            break;
        for (; *p >= '0' && *p <= '9'; p++)
            part[n] = part[n] * 10 + *p - '0';
        while (*p && *p != ' ' && (*p < '0' || *p > '9') && (*p | 0x20) != 'a' && (*p | 0x20) != 'p')
            p++;                            /* the separator */
    }
    while (*p == ' ')
        p++;
    if ((*p | 0x20) == 'p' && part[0] < 12)
        part[0] += 12;
    else if ((*p | 0x20) == 'a' && part[0] == 12)
        part[0] = 0;
    if (!n || part[0] > 23 || part[1] > 59 || part[2] > 59)
        return FALSE;
    *out = part[0] * 3600L + part[1] * 60 + part[2];
    return TRUE;
}

static void Place(HWND dlg, int id, RECT *r)
{
    WindowRectIn(GetDlgItem(dlg, id), dlg, r);
}

/* A cell of the calendar: row 0 the days' letters, then the weeks. */
static void Cell(const RECT *cal, int row, int col, RECT *r)
{
    int w = (cal->right - cal->left - 4) / 7, h = (cal->bottom - cal->top - 4) / 7;

    SetRect(r, cal->left + 2 + col * w, cal->top + 2 + row * h, cal->left + 2 + (col + 1) * w, cal->top + 2 + (row + 1) * h);
}

static void PaintCalendar(HWND dlg, HDC dc)
{
    static const char letters[] = "SMTWTFS";
    RECT cal, r;
    char text[4];
    int first = WeekDay(1, month, year), d, cell;

    Place(dlg, IDC_DT_CALENDAR, &cal);
    DrawBevel(dc, &cal, TRUE);
    InflateRect(&cal, -2, -2);
    DrawSolidRect(dc, &cal, GetSysColor(COLOR_WINDOW));
    InflateRect(&cal, 2, 2);
    SetBkMode(dc, TRANSPARENT);
    for (d = 0; d < 7; d++) {
        Cell(&cal, 0, d, &r);
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNSHADOW));
        SetTextColor(dc, GetSysColor(COLOR_BTNHIGHLIGHT));
        DrawText(dc, &letters[d], 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (d = 1; d <= MonthDays(month, year); d++) {
        cell = first + d - 1;
        Cell(&cal, 1 + cell / 7, cell % 7, &r);
        if (d == day)
            DrawSolidRect(dc, &r, GetSysColor(COLOR_HIGHLIGHT));
        SetTextColor(dc, GetSysColor(d == day ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        wsprintf(text, "%d", d);
        DrawText(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void Hand(HDC dc, int cx, int cy, int at, int length, int width, COLORREF c)
{
    HPEN pen = CreatePen(PS_SOLID, width, c), old = SelectObject(dc, pen);

    MoveTo(dc, cx, cy);
    LineTo(dc, cx + (int)((long)length * Sin60(at) / 1000), cy - (int)((long)length * Sin60(at + 15) / 1000));
    SelectObject(dc, old);
    DeleteObject(pen);
}

/* Windows 98's: twelve raised dots, the hours' and minutes' hands, the seconds'. */
static void PaintClock(HWND dlg, HDC dc)
{
    RECT box, r;
    int cx, cy, rad, i, x, y;

    Place(dlg, IDC_DT_CLOCK, &box);
    DrawSolidRect(dc, &box, GetSysColor(COLOR_BTNFACE));
    cx = (box.left + box.right) / 2;
    cy = (box.top + box.bottom) / 2;
    rad = min(box.right - box.left, box.bottom - box.top) / 2 - 3;
    for (i = 0; i < 60; i += 5) {
        x = cx + (int)((long)rad * Sin60(i) / 1000);
        y = cy - (int)((long)rad * Sin60(i + 15) / 1000);
        SetRect(&r, x - 2, y - 2, x + 2, y + 2);
        DrawSolidRect(dc, &r, GetSysColor(COLOR_BTNFACE));
        DrawFrame(dc, &r, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
    }
    Hand(dc, cx, cy, (int)(seconds / 720 % 60), rad * 5 / 10, 3, GetSysColor(COLOR_BTNTEXT));   /* hours */
    Hand(dc, cx, cy, (int)(seconds / 60 % 60), rad * 8 / 10, 2, GetSysColor(COLOR_BTNTEXT));
    Hand(dc, cx, cy, (int)(seconds % 60), rad * 9 / 10, 1, GetSysColor(COLOR_BTNSHADOW));
}

static void Repaint(HWND dlg, int id)
{
    RECT r;

    Place(dlg, id, &r);
    InvalidateRect(dlg, &r, FALSE);
}

static void Changed(HWND dlg)
{
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
}

static void Apply(HWND dlg)
{
    struct dosdate_t d;
    struct dostime_t t;

    if (typing && !ReadTime(dlg, &seconds)) {
        ErrorBox(dlg, "The time is not valid. Type it as the field shows it, such as 10:30:00.");
        return;
    }
    d.day = (unsigned char)day;
    d.month = (unsigned char)month;
    d.year = year;
    t.hour = (unsigned char)(seconds / 3600);
    t.minute = (unsigned char)(seconds / 60 % 60);
    t.second = (unsigned char)(seconds % 60);
    t.hsecond = 0;
    _dos_setdate(&d);
    _dos_settime(&t);
    SendMessage((HWND)0xFFFF, WM_TIMECHANGE, 0, 0L);    /* the clocks, the taskbar's */
    ShowTime(dlg);
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
}

static void Init(HWND dlg)
{
    struct dosdate_t d;
    struct dostime_t t;
    char text[8];
    int i;

    _dos_getdate(&d);
    _dos_gettime(&t);
    day = d.day;
    month = d.month;
    year = d.year;
    seconds = t.hour * 3600L + t.minute * 60 + t.second;
    TabsAdd(GetDlgItem(dlg, IDC_TABS), "Date & Time");
    for (i = 0; i < 12; i++)
        SendDlgItemMessage(dlg, IDC_DT_MONTH, CB_ADDSTRING, 0, (LPARAM)months[i]);
    SendDlgItemMessage(dlg, IDC_DT_MONTH, CB_SETCURSEL, month - 1, 0L);
    for (i = 0; i < YEARS; i++) {
        wsprintf(text, "%d", FIRST_YEAR + i);
        SendDlgItemMessage(dlg, IDC_DT_YEAR, CB_ADDSTRING, 0, (LPARAM)(LPSTR)text);
    }
    SendDlgItemMessage(dlg, IDC_DT_YEAR, CB_SETCURSEL, year - FIRST_YEAR, 0L);
    ShowWindow(GetDlgItem(dlg, IDC_DT_CALENDAR), SW_HIDE);   /* their places, painted over */
    ShowWindow(GetDlgItem(dlg, IDC_DT_CLOCK), SW_HIDE);
    ShowTime(dlg);
    SetTimer(dlg, TIMER_TICK, 1000, NULL);
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
    SkinDialog(dlg);
    WindowCenter(dlg);
}

/* A click on the calendar: that day. */
static void Click(HWND dlg, int x, int y)
{
    POINT p;
    RECT cal, r;
    int cell, d;

    p.x = x;
    p.y = y;
    Place(dlg, IDC_DT_CALENDAR, &cal);
    for (cell = 0; cell < 42; cell++) {
        Cell(&cal, 1 + cell / 7, cell % 7, &r);
        d = cell - WeekDay(1, month, year) + 1;
        if (PtInRect(&r, p) && d >= 1 && d <= MonthDays(month, year) && d != day) {
            day = d;
            Repaint(dlg, IDC_DT_CALENDAR);
            Changed(dlg);
        }
    }
}

BOOL CALLBACK __export DateTimeProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;

    switch (msg) {
    case WM_INITDIALOG:
        sheet = dlg;
        Init(dlg);
        return TRUE;
    case WM_ACTIVATE:
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        break;
    case WM_PAINT:
        BeginPaint(dlg, &ps);
        PaintCalendar(dlg, ps.hdc);
        PaintClock(dlg, ps.hdc);
        EndPaint(dlg, &ps);
        return TRUE;
    case WM_LBUTTONDOWN:
        Click(dlg, (int)LOWORD(lp), (int)HIWORD(lp));
        return TRUE;
    case WM_TIMER:                      /* the time shown goes on */
        seconds = (seconds + 1) % 86400L;
        if (!typing)
            ShowTime(dlg);
        Repaint(dlg, IDC_DT_CLOCK);
        return TRUE;
    case WM_COMMAND:
        if ((wp == IDC_DT_MONTH || wp == IDC_DT_YEAR) && HIWORD(lp) == CBN_SELCHANGE) {
            month = (int)SendDlgItemMessage(dlg, IDC_DT_MONTH, CB_GETCURSEL, 0, 0L) + 1;
            year = FIRST_YEAR + (int)SendDlgItemMessage(dlg, IDC_DT_YEAR, CB_GETCURSEL, 0, 0L);
            day = min(day, MonthDays(month, year));
            Repaint(dlg, IDC_DT_CALENDAR);
            Changed(dlg);
        } else if (wp == IDC_DT_TIME && HIWORD(lp) == EN_CHANGE && !showing) {
            typing = TRUE;
            Changed(dlg);
        } else if (wp == IDC_DT_TIME && HIWORD(lp) == EN_KILLFOCUS && typing && ReadTime(dlg, &seconds)) {
            ShowTime(dlg);              /* typed: the clock shows it, and goes on from it */
            Changed(dlg);
            Repaint(dlg, IDC_DT_CLOCK);
        } else {
            SheetButton(dlg, wp, Apply);
        }
        return TRUE;
    case WM_DESTROY:
        KillTimer(dlg, TIMER_TICK);
        MessageLoopTrackDialog(dlg, FALSE);
        sheet = NULL;
        return TRUE;
    }
    return FALSE;
}

void DateTimeShow(void)
{
    if (sheet)
        SetActiveWindow(sheet);
    else if (CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DATETIME), NULL, DateTimeProc) && sheet)
        ShowWindow(sheet, SW_SHOWNORMAL);
}
