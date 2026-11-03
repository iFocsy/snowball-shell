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

    /* DOS date: (year - 1980) << 9 | month << 5 | day; time: h << 11 | m << 5 | s / 2 */
    FormatDateTime((WORD)((2026 - 1980) << 9 | 10 << 5 | 26), (WORD)(18 << 11 | 58 << 5 | 12 / 2), s);
    CHECK_STR(s, "2026/10/26 18:58:12");
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
