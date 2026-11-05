/* Sizes, dates and type names written the way Windows 98 shows them. */
#include "internal.h"
#include <ver.h>

/* Three significant digits: "512 bytes", "1.23 KB", "45.6 MB", "789 KB". */
void FormatSize(DWORD bytes, LPSTR out)
{
    static const char *units[] = { "KB", "MB", "GB" };
    DWORD unit = 1024, whole, frac;
    int u = 0;

    if (bytes < 1024) {
        wsprintf(out, "%lu bytes", bytes);
        return;
    }
    while (u < 2 && bytes >= unit * 1024) {
        unit *= 1024;
        u++;
    }
    whole = bytes / unit;
    /* hundredths, cut as Windows 98 does ("1.99 GB" for 2,147,483,647 bytes);
       in kilobytes first, so that a gigabyte's part times 100 fits */
    frac = (bytes % unit) / (unit / 1024) * 100 / 1024;
    if (whole >= 100)
        wsprintf(out, "%lu %s", whole, (LPSTR)units[u]);
    else if (whole >= 10)
        wsprintf(out, "%lu.%lu %s", whole, frac / 10, (LPSTR)units[u]);
    else
        wsprintf(out, "%lu.%02lu %s", whole, frac, (LPSTR)units[u]);
}

static const char *Days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *Months[] = { "January", "February", "March", "April", "May", "June", "July",
                                "August", "September", "October", "November", "December" };

/* Day of the week of a date, 0 = Sunday. */
static int Weekday(int y, int m, int d)
{
    static const BYTE t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3)
        y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

/* A DOS date by a WIN.INI picture: d dd ddd dddd, M MM MMM MMMM, yy yyyy;
   text in quotes and other characters are copied. Returns the end. */
static LPSTR PutDate(LPSTR out, WORD date, LPCSTR picture)
{
    int day = date & 31, month = max(1, min(12, (date >> 5) & 15)), year = (date >> 9) + 1980, n;
    LPCSTR f, name;

    for (f = picture; *f; f += n) {
        for (n = 1; f[n] == f[0]; n++)
            ;
        if (*f == '\'') {                       /* 'literal text' */
            for (n = 1; f[n] && f[n] != '\''; n++)
                *out++ = f[n];
            if (f[n])
                n++;
            continue;
        }
        if ((*f == 'd' || *f == 'M') && n >= 3) {
            name = *f == 'd' ? Days[Weekday(year, month, day)] : Months[month - 1];
            lstrcpyn(out, name, n == 3 ? 4 : lstrlen(name) + 1);
            out += lstrlen(out);
            continue;
        }
        switch (*f) {
        case 'd': out += wsprintf(out, n > 1 ? "%02d" : "%d", day); break;
        case 'M': out += wsprintf(out, n > 1 ? "%02d" : "%d", month); break;
        case 'y': out += wsprintf(out, n > 2 ? "%d" : "%02d", n > 2 ? year : year % 100); break;
        default:  lstrcpyn(out, f, n + 1); out += n;
        }
    }
    *out = '\0';
    return out;
}

/* The time with seconds as WIN.INI [intl] says: 12 or 24 hours, separator,
   leading zero. */
static void PutTime(LPSTR out, WORD time)
{
    int hour = time >> 11, minute = (time >> 5) & 63;
    char sep[4], ampm[8];

    GetProfileString("intl", "sTime", ":", sep, sizeof sep);
    if (!GetProfileInt("intl", "iTime", 0))
        hour = hour % 12 ? hour % 12 : 12;
    out += wsprintf(out, GetProfileInt("intl", "iTLZero", 0) ? "%02d%s%02d" : "%d%s%02d", hour, (LPSTR)sep, minute);
    out += wsprintf(out, "%s%02d", (LPSTR)sep, (time & 31) * 2);
    hour = time >> 11;
    if (!GetProfileInt("intl", "iTime", 0)) {
        GetProfileString("intl", hour < 12 ? "s1159" : "s2359", hour < 12 ? "AM" : "PM", ampm, sizeof ampm);
        wsprintf(out, " %s", (LPSTR)ampm);
    }
}

/* A date and time by WIN.INI [intl]: sShortDate, or, in a file without it,
   iDate's order with sDate; then the time (the Details view). */
void FormatDateTime(WORD date, WORD time, LPSTR out)
{
    static const char *const orders[] = { "M%sd%syy", "d%sM%syy", "yy%sM%sd" };
    char picture[40], sep[4], text[128];        /* any picture that fits, with its names */
    LPSTR end;
    UINT order = GetProfileInt("intl", "iDate", 0);

    if (!GetProfileString("intl", "sShortDate", "", picture, sizeof picture)) {
        GetProfileString("intl", "sDate", "/", sep, sizeof sep);
        wsprintf(picture, orders[min(order, 2)], (LPSTR)sep, (LPSTR)sep);
    }
    end = PutDate(text, date, picture);
    *end++ = ' ';
    PutTime(end, time);
    lstrcpyn(out, text, DATETIME_SIZE);
}

/* "Monday, October 26, 2026 6:58:12 PM" by sLongDate (Properties). */
void FormatLongDateTime(WORD date, WORD time, BOOL withTime, LPSTR out)
{
    char picture[40];
    GetProfileString("intl", "sLongDate", "dddd, MMMM dd, yyyy", picture, sizeof picture);
    out = PutDate(out, date, picture);
    if (withTime) {
        *out++ = ' ';
        PutTime(out, time);
    }
}

/* n with the thousands separator of WIN.INI [intl]. */
void FormatNumber(DWORD n, LPSTR out)
{
    char digits[12], sep[4];
    int len, i, o = 0;

    GetProfileString("intl", "sThousand", ",", sep, sizeof sep);
    wsprintf(digits, "%lu", n);
    len = lstrlen(digits);
    for (i = 0; i < len; i++) {
        if (i && (len - i) % 3 == 0)
            out[o++] = sep[0];
        out[o++] = digits[i];
    }
    out[o] = '\0';
}

/* "1.23 KB (1,264 bytes)", as Properties shows a size. */
void FormatBytes(DWORD bytes, LPSTR out)
{
    FormatSize(bytes, out);
    lstrcat(out, " (");
    FormatNumber(bytes, out + lstrlen(out));
    lstrcat(out, " bytes)");
}

/* Whole kilobytes, rounded up, as the Size column of Windows 98. */
void FormatKB(DWORD bytes, LPSTR out)
{
    FormatNumber(bytes / 1024 + (bytes % 1024 != 0), out);
    lstrcat(out, "KB");
}

/* ---- type names: the registration database's (REG.DAT: ".ext" -> class
   -> description), Windows 98's for common files where it has Windows 3.1's
   own or none, else "EXT File" ---- */

#define TYPE_CACHE  24

static const char *Known[][2] = {
    { "EXE", "Application" },           { "COM", "MS-DOS Application" },
    { "BAT", "MS-DOS Batch File" },     { "PIF", "Shortcut to MS-DOS Program" },
    { "SHC", "Shortcut" },              { "TXT", "Text Document" },
    { "INI", "Configuration Settings" },{ "LOG", "Text Document" },
    { "BMP", "Bitmap Image" },          { "WRI", "Write Document" },
    { "HLP", "Help File" },             { "DLL", "Application Extension" },
    { "DRV", "Device driver" },         { "SYS", "System file" },
    { "386", "Virtual device driver" }, { "FON", "Font file" },
    { "TTF", "TrueType Font file" },    { "CPL", "Control Panel extension" },
    { "GRP", "Program Group" },         { "WAV", "Wave Sound" },
    { "MID", "MIDI Sequence" },         { "RTF", "Rich Text Format" },
    { "SCR", "Screen Saver" },          { "ICO", "Icon" },
};

/* What Windows 3.1's Setup registers: Windows 98's names go before these. */
static const char *Win31Names[] = {
    "Text File", "Card File", "Terminal Settings", "Recorder Macro", "Microsoft Mail",
    "Write Document", "Registration Entries", "Paintbrush Picture", "Sound", "Media Player",
};

static struct { char ext[8]; char name[48]; } cache[TYPE_CACHE];
static int nextSlot;

void FormatForgetTypes(void)
{
    int i;

    for (i = 0; i < TYPE_CACHE; i++)
        cache[i].ext[0] = '\0';
}

static BOOL Win31Name(LPCSTR name)
{
    int i;

    for (i = 0; i < sizeof Win31Names / sizeof Win31Names[0]; i++)
        if (!lstrcmp(name, Win31Names[i]))
            return TRUE;
    return FALSE;
}

static BOOL FromRegistry(LPCSTR ext, LPSTR out, int size)
{
    char cls[64];
    LONG n = size;

    return FileTypeClass(ext, cls, sizeof cls) && RegQueryValue(HKEY_CLASSES_ROOT, cls, out, &n) == ERROR_SUCCESS &&
           out[0];
}

void FormatFileType(LPCSTR name, LPSTR out)
{
    LPSTR dot = PathExtension((LPSTR)name);
    char ext[8];
    BOOL registered;
    int i;

    if (!*dot) {
        lstrcpy(out, "File");
        return;
    }
    lstrcpyn(ext, dot + 1, sizeof ext);
    AnsiUpper(ext);
    for (i = 0; i < TYPE_CACHE; i++)
        if (!lstrcmp(cache[i].ext, ext)) {
            lstrcpy(out, cache[i].name);
            return;
        }
    for (i = 0; i < sizeof Known / sizeof Known[0] && lstrcmp(Known[i][0], ext); i++)
        ;
    registered = FromRegistry(ext, out, sizeof cache[0].name);
    if (i < sizeof Known / sizeof Known[0] && (!registered || Win31Name(out)))
        lstrcpy(out, Known[i][1]);
    else if (!registered)
        wsprintf(out, "%s File", (LPSTR)ext);
    lstrcpy(cache[nextSlot].ext, ext);
    lstrcpyn(cache[nextSlot].name, out, sizeof cache[0].name);
    nextSlot = (nextSlot + 1) % TYPE_CACHE;
}

/* The FileDescription of a program's or library's version resource
   ("Notepad"); FALSE when it has none. */
BOOL FormatDescription(LPCSTR path, LPSTR out, int size)
{
    char shortPath[MAX_PATH], name[48];
    DWORD handle, len;
    HGLOBAL h;
    void FAR *data;
    LPSTR value;
    DWORD FAR *lang;
    UINT n;
    BOOL ok = FALSE;

    if (!DosShortPath(path, shortPath) || (len = GetFileVersionInfoSize(shortPath, &handle)) == 0 ||
        len > 32000 || (h = GlobalAlloc(GMEM_MOVEABLE, len)) == NULL)
        return FALSE;
    data = GlobalLock(h);
    if (GetFileVersionInfo(shortPath, handle, len, data) &&
        VerQueryValue(data, "\\VarFileInfo\\Translation", (void FAR * FAR *)&lang, &n) && n >= 4) {
        wsprintf(name, "\\StringFileInfo\\%04X%04X\\FileDescription", LOWORD(*lang), HIWORD(*lang));
        if (VerQueryValue(data, name, (void FAR * FAR *)&value, &n) && n > 1 && *value) {
            lstrcpyn(out, value, size);
            ok = TRUE;
        }
    }
    GlobalUnlock(h);
    GlobalFree(h);
    return ok;
}
