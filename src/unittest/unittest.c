/*
 * UNITTEST.EXE: checks of SNSHELL's core\ functions (paths, formats, lists,
 * sorting, shortcuts, desktop.ini) outside the shell, linked with all of
 * SNSHELL's sources but snshell.c. unittest.ps1 runs it under winevdm on
 * the host; it writes UNITTEST.LOG in the current folder, one line a
 * failed check and a count at the end, and returns the count of failures.
 */
#include "internal.h"

HINSTANCE g_hInst;                      /* (snshell.c's, which is not linked) */
HINSTANCE g_hArt;
BOOL g_isShell;

static HFILE logFile;
static int checks, failures;
static char work[MAX_PATH];             /* a folder for files: WORK beside UNITTEST.EXE */

static void Log(LPCSTR text)
{
    _lwrite(logFile, text, lstrlen(text));
    _lwrite(logFile, "\r\n", 2);
}

static void Check(BOOL ok, int line, LPCSTR what)
{
    char text[200];

    checks++;
    if (ok)
        return;
    failures++;
    wsprintf(text, "FAIL line %d: %s", line, what);
    Log(text);
}

static void CheckStr(LPCSTR got, LPCSTR want, int line, LPCSTR what)
{
    char text[300];

    checks++;
    if (!lstrcmp(got, want))
        return;
    failures++;
    wsprintf(text, "FAIL line %d: %s is \"%s\", not \"%s\"", line, what, got, want);
    Log(text);
}

#define CHECK(e)            Check((e) != 0, __LINE__, #e)
#define CHECK_STR(got, want) CheckStr(got, want, __LINE__, #got)

/* want with WIN.INI's thousands separator for its commas (under winevdm,
   the host's own; Windows 3.1 in DOSBox-X has ","). */
static LPSTR Thousands(LPCSTR want)
{
    static char s[80];
    char sep[4];
    LPSTR p;

    GetProfileString("intl", "sThousand", ",", sep, sizeof sep);
    lstrcpy(s, want);
    for (p = s; *p; p++)
        if (*p == ',')
            *p = sep[0];
    return s;
}

static void TestPaths(void)
{
    char p[MAX_PATH];
    static char file[] = "C:\\WINDOWS\\NOTEPAD.EXE";

    CHECK_STR(PathFileName(file), "NOTEPAD.EXE");
    CHECK_STR(PathExtension(file), ".EXE");
    lstrcpy(p, "C:\\A.B\\README");
    CHECK_STR(PathExtension(p), "");

    lstrcpy(p, "C:\\WINDOWS");
    PathAppend(p, "SYSTEM");
    CHECK_STR(p, "C:\\WINDOWS\\SYSTEM");
    lstrcpy(p, "C:\\");
    PathAppend(p, "TEMP");
    CHECK_STR(p, "C:\\TEMP");

    lstrcpy(p, "C:\\A\\B");
    PathToParent(p);
    CHECK_STR(p, "C:\\A");
    PathToParent(p);
    CHECK_STR(p, "C:\\");

    CHECK(PathIsInside("C:\\A\\B", "C:\\A"));
    CHECK(PathIsInside("C:\\A", "C:\\A"));
    CHECK(PathIsInside("c:\\a\\b", "C:\\A"));
    CHECK(!PathIsInside("C:\\AB", "C:\\A"));
    CHECK(!PathIsInside("C:\\A", "C:\\A\\B"));

    CHECK(PathMatchesPattern("readme.txt", "*.TXT;*.doc"));
    CHECK(PathMatchesPattern("letter.doc", "*.txt;*.doc"));
    CHECK(!PathMatchesPattern("notes.wri", "*.txt;*.doc"));
    CHECK(PathMatchesPattern("a1.txt", "a?.txt"));
    CHECK(!PathMatchesPattern("a12.txt", "a?.txt"));
    CHECK(PathMatchesPattern("anything", "*"));

    CHECK(PathIsShortName("README.TXT"));
    CHECK(PathIsShortName("A"));
    CHECK(!PathIsShortName("Long name.txt"));
    CHECK(!PathIsShortName("ABCDEFGHI.TXT"));
    CHECK(!PathIsShortName("A.TEXT"));
    CHECK(!PathIsShortName("A.B.C"));

    CHECK(PathOnDisk("C:\\WINDOWS"));
    CHECK(PathIsShortcut("C:\\DESKTOP\\NOTEPAD.SHC"));
    CHECK(!PathIsShortcut("C:\\NOTEPAD.EXE"));
    CHECK(PathIsProgram("C:\\NOTEPAD.EXE"));
    CHECK(PathIsProgram("C:\\AUTOEXEC.BAT"));
    CHECK(PathIsProgram("C:\\DOSPRMPT.PIF"));
    CHECK(!PathIsProgram("C:\\README.TXT"));
}

/* DOS dates and times by regional settings, whatever this Windows has: date
   picture, 24 hours, leading zero; a day in the afternoon and one in the morning. */
static void TestRegion(LPCSTR picture, LPCSTR hours24, LPCSTR zero, LPCSTR pm, LPCSTR am)
{
    static const char *const keys[] = { "sShortDate", "iTime", "iTLZero", "sTime", "s1159", "s2359" };
    char saved[6][40], s[64];
    int k;

    for (k = 0; k < 6; k++)
        GetProfileString("intl", keys[k], "", saved[k], sizeof saved[k]);
    WriteProfileString("intl", "sShortDate", picture);
    WriteProfileString("intl", "iTime", hours24);
    WriteProfileString("intl", "iTLZero", zero);
    WriteProfileString("intl", "sTime", ":");
    WriteProfileString("intl", "s1159", "AM");
    WriteProfileString("intl", "s2359", "PM");
    /* DOS date: (year - 1980) << 9 | month << 5 | day; time: h << 11 | m << 5 | s / 2 */
    FormatDateTime((WORD)((2026 - 1980) << 9 | 10 << 5 | 26), (WORD)(18 << 11 | 58 << 5 | 12 / 2), s);
    CHECK_STR(s, pm);
    FormatDateTime((WORD)((2026 - 1980) << 9 | 2 << 5 | 4), (WORD)(7 << 11 | 5 << 5 | 6 / 2), s);
    CHECK_STR(s, am);
    for (k = 0; k < 6; k++)
        WriteProfileString("intl", keys[k], saved[k][0] ? saved[k] : NULL);
}

static void TestFormats(void)
{
    char s[80];

    FormatNumber(0, s);
    CHECK_STR(s, "0");
    FormatNumber(999, s);
    CHECK_STR(s, "999");
    FormatNumber(1234567L, s);
    CHECK_STR(s, Thousands("1,234,567"));

    FormatSize(512, s);
    CHECK_STR(s, "512 bytes");
    FormatSize(1264, s);
    CHECK_STR(s, "1.23 KB");
    FormatSize(4718592L, s);
    CHECK_STR(s, "4.50 MB");

    FormatBytes(1264, s);
    CHECK_STR(s, Thousands("1.23 KB (1,264 bytes)"));

    FormatKB(1, s);
    CHECK_STR(s, "1KB");
    FormatKB(1263616L, s);
    CHECK_STR(s, Thousands("1,234KB"));

    TestRegion("dd-MM-yy", "1", "1", "26-10-26 18:58:12", "04-02-26 07:05:06");
    TestRegion("M/d/yyyy", "0", "0", "10/26/2026 6:58:12 PM", "2/4/2026 7:05:06 AM");
    TestRegion("yyyy.MM.dd", "1", "0", "2026.10.26 18:58:12", "2026.02.04 7:05:06");
}

/* Long names' extensions are cut to fit the registry keys built from them. */
static void TestTypes(void)
{
    char s[80];

    FormatFileType("notes.markdown", s);
    CHECK_STR(s, "MARKDOW File");
    CHECK(!FileTypeClass("averyveryverylongextension", s, sizeof s));
}

/* A BMP is drawn only when its header and bits are where it says. */
static void TestDib(void)
{
    static BYTE bmp[58] = { 'B', 'M', 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,     /* the bits at 54 */
                            40, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 24 };   /* 1x1, 24 bits */
    char path[MAX_PATH];
    HFILE h;
    DIB d;

    lstrcpy(path, work);
    PathAppend(path, "PIC.BMP");
    h = DosCreate(path);
    _lwrite(h, bmp, sizeof bmp);
    _lclose(h);
    CHECK(DibLoad(path, &d) && d.w == 1 && d.h == 1 && d.bits == 40);
    DibFree(&d);
    bmp[10] = 200;                      /* bits past the end */
    h = DosCreate(path);
    _lwrite(h, bmp, sizeof bmp);
    _lclose(h);
    CHECK(!DibLoad(path, &d));
    CHECK(DosCreateTextFile(path, "BMW notes, not a picture") && !DibLoad(path, &d));
}

/* The pictures in PICTURES beside the program, if any (tests\pictures.py puts
   them there and compares what comes out): each made into a BMP per style,
   for a screen of 320 x 240, in WORK. */
static void TestPictures(void)
{
    char dir[MAX_PATH], spec[MAX_PATH], from[MAX_PATH], to[MAX_PATH], text[80];
    DOSENTRY e;
    DOSFIND f;
    IMAGEJOB job;
    DIB d;
    int r;

    lstrcpy(dir, work);
    PathToParent(dir);
    PathAppend(dir, "PICTURES");
    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (!DosFindFirst(spec, &f, &e))
        return;
    job.w = 320;
    job.h = 240;
    job.back = RGB(0, 128, 128);
    job.progress = NULL;
    do {
        if (e.name[0] == '.')
            continue;
        lstrcpy(from, dir);
        PathAppend(from, e.name);
        for (job.style = 0; job.style < WP_STYLES; job.style++) {
            r = ImageMakeDib(from, &job, 0, &d);
            if (r == IMAGE_OK) {
                lstrcpy(to, work);
                PathAppend(to, e.name);
                wsprintf(PathExtension(to), "_%d.BMP", job.style);
                r = ImageWriteBitmap(&d, to, 24);
                DibFree(&d);
            }
            if (r != IMAGE_OK) {
                wsprintf(text, "picture %s style %d: error %d", (LPSTR)e.name, job.style, r);
                Log(text);
            }
        }
    } while (DosFindNext(&f, &e));
    DosFindClose(&f);
}

static void TestLists(void)
{
    PATHLIST l;
    LPSTR p;
    int n = 0;

    PathListInit(&l);
    CHECK(PathListAdd(&l, "C:\\A"));
    CHECK(PathListAdd(&l, "C:\\B\\LONG NAME.TXT"));
    CHECK(l.count == 2);
    for (p = PathListLock(&l); *p; p = PathListNext(p))
        n++;
    PathListUnlock(&l);
    CHECK(n == 2);
    p = PathListLock(&l);
    CHECK_STR(p, "C:\\A");
    CHECK_STR(PathListNext(p), "C:\\B\\LONG NAME.TXT");
    PathListUnlock(&l);
    PathListFree(&l);
}

static int CompareInts(const void far *a, const void far *b)
{
    return *(const int far *)a - *(const int far *)b;
}

static void TestSort(void)
{
    int n[] = { 5, 3, 9, 1, 7, 3 };

    SortRecords(n, 6, sizeof n[0], CompareInts);
    CHECK(n[0] == 1 && n[1] == 3 && n[2] == 3 && n[3] == 5 && n[4] == 7 && n[5] == 9);
    SortRecords(n, 0, sizeof n[0], CompareInts);
    CHECK(n[0] == 1);
}

static void TestLaunch(void)
{
    char prog[MAX_PATH], args[128];

    LaunchSplitCommand("notepad.exe readme.txt", prog, args, sizeof args);
    CHECK_STR(prog, "notepad.exe");
    CHECK_STR(args, "readme.txt");
    LaunchSplitCommand("\"C:\\Program Files\\A.EXE\" /x y", prog, args, sizeof args);
    CHECK_STR(prog, "C:\\Program Files\\A.EXE");
    CHECK_STR(args, "/x y");
    LaunchSplitCommand("calc.exe", prog, args, sizeof args);
    CHECK_STR(prog, "calc.exe");
    CHECK_STR(args, "");
}

static void TestShortcuts(void)
{
    SHORTCUT s, r;
    char path[MAX_PATH];

    _fmemset(&s, 0, sizeof s);
    lstrcpy(s.target, "C:\\WINDOWS\\NOTEPAD.EXE");
    lstrcpy(s.args, "README.TXT");
    lstrcpy(s.dir, "C:\\WINDOWS");
    lstrcpy(s.icon, "C:\\WINDOWS\\MORICONS.DLL");
    s.iconIndex = 3;
    s.show = SW_SHOWMAXIMIZED;
    s.hotkey = 'N' | (SK_CONTROL | SK_ALT) << 8;
    lstrcpy(path, work);
    PathAppend(path, "NOTEPAD.SHC");
    CHECK(ShortcutWrite(path, &s));
    _fmemset(&r, 0, sizeof r);
    CHECK(ShortcutRead(path, &r));
    CHECK_STR(r.target, s.target);
    CHECK_STR(r.args, s.args);
    CHECK_STR(r.dir, s.dir);
    CHECK_STR(r.icon, s.icon);
    CHECK(r.iconIndex == 3);
    CHECK(r.show == SW_SHOWMAXIMIZED);
    CHECK(r.hotkey == s.hotkey);
}

static void TestFolderIni(void)
{
    char tip[80], file[MAX_PATH];
    int index;

    CHECK(FolderIniWrite(work, "C:\\WINDOWS\\MORICONS.DLL", 5, "Papers to keep"));
    CHECK(FolderIniRead(work, "InfoTip", tip, sizeof tip) == 14);
    CHECK_STR(tip, "Papers to keep");
    CHECK(FolderIniIcon(work, file, &index));
    CHECK_STR(file, "C:\\WINDOWS\\MORICONS.DLL");
    CHECK(index == 5);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int show)
{
    char text[80];

    g_hInst = g_hArt = hInst;
    GetModuleFileName(hInst, work, sizeof work);
    *PathFileName(work) = '\0';
    lstrcpy(g_home, work);
    lstrcpy(g_ini, work);
    lstrcat(g_ini, "SNOWBALL.INI");
    lstrcat(work, "WORK");
    logFile = _lcreat("UNITTEST.LOG", 0);
    if (logFile == HFILE_ERROR)
        return 255;

    TestPaths();
    TestFormats();
    TestTypes();
    TestDib();
    TestPictures();
    TestLists();
    TestSort();
    TestLaunch();
    TestShortcuts();
    TestFolderIni();

    wsprintf(text, "%d checks, %d failed", checks, failures);
    Log(text);
    _lclose(logFile);
    return failures;
}
