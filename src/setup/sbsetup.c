/*
 * SBSETUP.EXE: what Snowball's Setup (Inno Setup's 16-bit edition) cannot do
 * itself. Setup runs it once the files are in place; its uninstaller, before
 * they go. It lives in Snowball's folder.
 *   /install    Snowball becomes the shell (SYSTEM.INI shell=, taskman.exe=
 *               for Ctrl+Esc); Windows 98's colours and its menu and dialog
 *               font as the system font (SBSANS8.FON, made from this
 *               Windows' own fonts); more time for Windows programs beside
 *               busy DOS boxes; LFN.DRV if wanted (asked). Each value
 *               replaced is kept in UNINSTAL.INI, as install.ps1 keeps them
 *               ([SYSTEM.INI] boot|shell=progman.exe).
 *   /uninstall  puts back each value still as Snowball set it.
 *   /remove     the Start menu's Uninstall Snowball: the uninstaller, or,
 *               while Snowball is the shell, Windows restarted with Program
 *               Manager and the uninstaller run then (WIN.INI run=).
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "snowball.h"
#include "dpmi.h"

#define TITLE       "Snowball Setup"
#define SAVED       "UNINSTAL.INI"
#define NOVALUE     "\001"              /* GetProfileString's default: no such key */
#define FONT_FILE   "SBSANS8.FON"

typedef struct { const char *file, *section, *key, *value; } CHANGE;

/* Windows 98's "Windows Standard" colours, in Control Panel's scheme order */
static const char *Colors[][2] = {
    { "Background", "0 128 128" },      { "AppWorkspace", "128 128 128" },  { "Window", "255 255 255" },
    { "WindowText", "0 0 0" },          { "Menu", "192 192 192" },          { "MenuText", "0 0 0" },
    { "ActiveTitle", "0 0 128" },       { "InactiveTitle", "128 128 128" }, { "TitleText", "255 255 255" },
    { "ActiveBorder", "192 192 192" },  { "InactiveBorder", "192 192 192" }, { "WindowFrame", "0 0 0" },
    { "Scrollbar", "192 192 192" },     { "ButtonFace", "192 192 192" },    { "ButtonShadow", "128 128 128" },
    { "ButtonText", "0 0 0" },          { "GrayText", "128 128 128" },      { "Hilight", "0 0 128" },
    { "HilightText", "255 255 255" },   { "InactiveTitleText", "192 192 192" }, { "ButtonHilight", "255 255 255" },
};
#define NCOLORS     (sizeof Colors / sizeof Colors[0])

static char app[MAX_PATH], saved[MAX_PATH], shell[MAX_PATH], scheme[200];
static CHANGE changes[8 + NCOLORS];
static int nChanges;

static void Add(const char *file, const char *section, const char *key, const char *value)
{
    changes[nChanges].file = file;
    changes[nChanges].section = section;
    changes[nChanges].key = key;
    changes[nChanges++].value = value;
}

/* The values Snowball sets; fonts.fon only when its font could be made. */
static void Changes(BOOL font)
{
    char rgb[16], *p;
    int i, r, g, b;

    nChanges = 0;
    Add("SYSTEM.INI", "boot", "shell", shell);
    Add("SYSTEM.INI", "boot", "taskman.exe", shell);
    if (font)
        Add("SYSTEM.INI", "boot", "fonts.fon", "sbsans8.fon");
    Add("SYSTEM.INI", "386Enh", "WinTimeSlice", "200,50");
    Add("CONTROL.INI", "color schemes", "Windows 98", scheme);
    Add("CONTROL.INI", "current", "color schemes", "Windows 98");
    scheme[0] = '\0';
    for (i = 0; i < NCOLORS; i++) {     /* CONTROL.INI keeps a scheme as BBGGRR hex */
        lstrcpy(rgb, Colors[i][1]);
        r = atoi(rgb);
        g = atoi(p = strchr(rgb, ' ') + 1);
        b = atoi(strchr(p, ' ') + 1);
        wsprintf(scheme + lstrlen(scheme), "%s%lX", (LPSTR)(i ? "," : ""), (DWORD)b * 65536L + (DWORD)g * 256 + r);
        Add("WIN.INI", "colors", Colors[i][0], Colors[i][1]);
    }
}

/* An INI value: WIN.INI through Windows' copy of it; FALSE when missing. */
static BOOL Get(const char *file, const char *section, const char *key, LPSTR out, int size)
{
    if (!lstrcmpi(file, "WIN.INI"))
        GetProfileString(section, key, NOVALUE, out, size);
    else
        GetPrivateProfileString(section, key, NOVALUE, out, size, file);
    return out[0] != NOVALUE[0];
}

/* value NULL: the key goes */
static void Set(const char *file, const char *section, const char *key, LPCSTR value)
{
    if (!lstrcmpi(file, "WIN.INI"))
        WriteProfileString(section, key, value);
    else
        WritePrivateProfileString(section, key, value, file);
}

static BOOL Exists(LPCSTR path)
{
    OFSTRUCT of;

    return OpenFile(path, &of, OF_EXIST) != HFILE_ERROR;
}

static void PathAppend(LPSTR dir, LPCSTR name)
{
    int n = lstrlen(dir);

    if (n && dir[n - 1] != '\\')
        lstrcat(dir, "\\");
    lstrcat(dir, name);
}

/* ---- the system font: Windows 98's menu and dialog font ---- */

static WORD U16(BYTE FAR *d, WORD i)
{
    return *(WORD FAR *)(d + i);
}

/* The entries (12 bytes each: offset, length, flags, id) of one resource
   type of a .FON (a resource-only module), and the alignment shift. */
static BOOL Resources(BYTE FAR *d, WORD type, WORD *first, WORD *count, WORD *shift)
{
    WORD ne = U16(d, 0x3C), p = ne + U16(d, ne + 0x24), t, n;

    *shift = U16(d, p);
    for (p += 2; (t = U16(d, p)) != 0; p += 12 * n) {
        n = U16(d, p + 2);
        p += 8;
        if (t == type) {
            *first = p;
            *count = n;
            return TRUE;
        }
    }
    return FALSE;
}

static void FreeAll(BYTE FAR *d)
{
    if (d) {
        HGLOBAL h = (HGLOBAL)LOWORD(GlobalHandle(SELECTOROF(d)));
        GlobalUnlock(h);
        GlobalFree(h);
    }
}

static BYTE FAR *ReadAll(LPCSTR dir, LPCSTR name, WORD *size)
{
    char path[MAX_PATH];
    HFILE h;
    LONG n;
    BYTE FAR *d = NULL;

    lstrcpy(path, dir);
    PathAppend(path, name);
    if ((h = _lopen(path, OF_READ | OF_SHARE_DENY_NONE)) == HFILE_ERROR)
        return NULL;
    n = _llseek(h, 0, 2);
    _llseek(h, 0, 0);
    if (n > 0 && n < 65000L && (d = (BYTE FAR *)GlobalLock(GlobalAlloc(GMEM_MOVEABLE, n))) != NULL &&
        _lread(h, d, (UINT)n) != (UINT)n) {
        FreeAll(d);
        d = NULL;
    }
    _lclose(h);
    *size = (WORD)n;
    return d;
}

/* SBSANS8.FON: GDI takes as fonts.fon only a file holding one font, so the
   container is VGASYS.FON (module FONTS, aspect 96x96) with its font replaced
   by the 8 point face of SSERIFE.FON (MS Sans Serif), named System. */
static BOOL MakeFont(LPCSTR sys)
{
    static const char System[] = "System";
    BYTE FAR *box, FAR *sans, FAR *font = NULL, FAR *out;
    WORD boxLen, sansLen, e, n, shift, dirEntry, fontEntry, unit, start, at, i, ne, total;
    DWORD fontLen = 0;
    BYTE dir[125];
    char path[MAX_PATH];
    HFILE h;
    BOOL ok = FALSE;

    box = ReadAll(sys, "VGASYS.FON", &boxLen);
    sans = ReadAll(sys, "SSERIFE.FON", &sansLen);
    if (!box || !sans || !Resources(sans, 0x8008, &e, &n, &shift))
        goto done;
    for (i = 0; i < n && !font; i++, e += 12) {
        at = U16(sans, e) << shift;
        if (U16(sans, at + 0x44) == 8) {                    /* dfPoints */
            font = sans + at;
            fontLen = *(DWORD FAR *)(font + 2);
        }
    }
    if (!font || fontLen > 32000)
        goto done;
    _fmemcpy(font + (WORD)*(DWORD FAR *)(font + 0x69), System, sizeof System);   /* its face name */
    if (!Resources(box, 0x8007, &dirEntry, &n, &shift) || !Resources(box, 0x8008, &fontEntry, &n, &shift))
        goto done;
    unit = 1 << shift;
    start = min(U16(box, dirEntry), U16(box, fontEntry)) * unit;
    ne = U16(box, 0x3C);
    if (*(DWORD FAR *)(box + ne + 0x2C) >= start)        /* the names after the resources: not as expected */
        goto done;

    /* the font directory: count, ordinal, the 113-byte header, device and face names */
    dir[0] = 1;
    dir[1] = 0;
    *(WORD *)(dir + 2) = U16(box, fontEntry + 6) & 0x7FFF;
    _fmemcpy(dir + 4, font, 113);
    dir[117] = '\0';
    _fmemcpy(dir + 118, System, sizeof System);

    /* VGASYS.FON's header part, then both resources, each padded to the alignment */
    at = start / unit;
    *(WORD FAR *)(box + dirEntry) = at;
    *(WORD FAR *)(box + dirEntry + 2) = (sizeof dir + unit - 1) / unit;
    at += (sizeof dir + unit - 1) / unit;
    *(WORD FAR *)(box + fontEntry) = at;
    *(WORD FAR *)(box + fontEntry + 2) = (WORD)((fontLen + unit - 1) / unit);
    at += (WORD)((fontLen + unit - 1) / unit);
    total = at * unit;
    if ((out = (BYTE FAR *)GlobalLock(GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total))) == NULL)
        goto done;
    _fmemcpy(out, box, start);
    _fmemcpy(out + start, dir, sizeof dir);
    _fmemcpy(out + U16(box, fontEntry) * unit, font, (WORD)fontLen);
    lstrcpy(path, sys);
    PathAppend(path, FONT_FILE);
    if ((h = _lcreat(path, 0)) != HFILE_ERROR) {
        ok = _lwrite(h, (LPCSTR)out, total) == total;
        _lclose(h);
    }
    FreeAll(out);
done:
    FreeAll(box);
    FreeAll(sans);
    return ok;
}

/* ---- long file names ---- */

/* Whether DOS has the long file name API on C: (INT 21h AX=71A0h, run in
   DOS through DPMI: Windows would hand DOS this call's pointers as they are). */
static BOOL DosHasLongNames(void)
{
    static RMCALL rm;
    DWORD block = GlobalDosAlloc(64);
    LPSTR mem = MAKELP(LOWORD(block), 0);
    BOOL ok;

    if (!block)
        return FALSE;
    lstrcpy(mem, "C:\\");
    _fmemset(&rm, 0, sizeof rm);
    rm.eax = 0x71A0;
    rm.ds = rm.es = HIWORD(block);
    rm.edx = 0;
    rm.edi = 16;                        /* the file system's name */
    rm.ecx = 32;
    ok = DpmiRealInt(0x21, &rm);
    GlobalDosFree(LOWORD(block));
    return ok && !(rm.flags & 1) && LOWORD(rm.eax) != 0x7100;     /* 7100h: no such call */
}

/* drivers= without (or with) one name */
static BOOL DriversHave(LPCSTR list, LPCSTR name)
{
    char copy[256], *t;                 /* near: a far pointer made from a NULL one is not NULL */

    lstrcpy(copy, list);
    AnsiLower(copy);
    for (t = strtok(copy, " ,"); t; t = strtok(NULL, " ,"))
        if (!lstrcmp(t, name))
            return TRUE;
    return FALSE;
}

static void DriversRemove(LPSTR list, LPCSTR name)
{
    char copy[256], out[256], *t;

    lstrcpy(copy, list);
    out[0] = '\0';
    for (t = strtok(copy, " "); t; t = strtok(NULL, " "))
        if (lstrcmpi(t, name)) {
            if (out[0])
                lstrcat(out, " ");
            lstrcat(out, t);
        }
    lstrcpy(list, out);
}

/* ---- install, uninstall ---- */

/* Windows keeps INI files' changes in memory until it ends: written out now,
   so a reset or a crash before then loses none. */
static void Flush(void)
{
    WriteProfileString(NULL, NULL, NULL);
    WritePrivateProfileString(NULL, NULL, NULL, "SYSTEM.INI");
    WritePrivateProfileString(NULL, NULL, NULL, "CONTROL.INI");
    WritePrivateProfileString(NULL, NULL, NULL, saved);
}

static BOOL CopyFile(LPCSTR from, LPCSTR to)
{
    static char buf[2048];
    HFILE in, out;
    UINT n;
    BOOL ok = TRUE;

    if ((in = _lopen(from, OF_READ | OF_SHARE_DENY_NONE)) == HFILE_ERROR)
        return FALSE;
    if ((out = _lcreat(to, 0)) == HFILE_ERROR) {
        _lclose(in);
        return FALSE;
    }
    while (ok && (n = _lread(in, buf, sizeof buf)) != 0 && n != (UINT)HFILE_ERROR)
        ok = _lwrite(out, buf, n) == n;
    _lclose(out);
    _lclose(in);
    return ok && n != (UINT)HFILE_ERROR;
}

/* LFN.DRV (Setup leaves it in Snowball's folder), if wanted: in WINDOWS\SYSTEM,
   loaded at startup. Loaded already (an earlier install): only its new
   version. Otherwise asked, Yes first when DOS has long file names. Its
   code stays in memory, so the file may be replaced while it runs. */
static void InstallLfn(LPCSTR sys)
{
    char value[256], from[MAX_PATH], to[MAX_PATH], question[600];
    BOOL dosHas;

    lstrcpy(from, app);
    lstrcat(from, "LFN.DRV");
    lstrcpy(to, sys);
    PathAppend(to, "LFN.DRV");
    if (!Exists(from))
        return;
    if (!Get("SYSTEM.INI", "boot", "drivers", value, sizeof value))
        value[0] = '\0';
    if (DriversHave(value, "lfn.drv")) {
        CopyFile(from, to);
        return;
    }
    dosHas = DosHasLongNames();
    lstrcpy(question, "Install LFN.DRV, long file names for Windows programs?\n\n"
            "With it, Windows programs can use the long file names DOS has (DOSBox-X with lfn=true, or a "
            "DOS long file name driver), and older programs can open files in long-named folders. It loads "
            "when Windows starts.");
    if (!dosHas)
        lstrcat(question, "\n\nThis DOS has no long file names now: LFN.DRV does nothing until it has them.");
    if (MessageBox(NULL, question, TITLE, MB_YESNO | MB_ICONQUESTION | (dosHas ? MB_DEFBUTTON1 : MB_DEFBUTTON2)) != IDYES)
        return;
    if (!CopyFile(from, to)) {
        MessageBox(NULL, "LFN.DRV could not be copied to Windows' SYSTEM folder.", TITLE, MB_OK | MB_ICONSTOP);
        return;
    }
    if (value[0])
        lstrcat(value, " ");
    lstrcat(value, "lfn.drv");
    Set("SYSTEM.INI", "boot", "drivers", value);
    WritePrivateProfileString("Snowball", "AddedLfn", "1", saved);
    if (!Get("SYSTEM.INI", "lfn.drv", "LongPaths", value, sizeof value)) {
        Set("SYSTEM.INI", "lfn.drv", "LongPaths", "1");     /* long paths for old programs too */
        WritePrivateProfileString("Snowball", "AddedLongPaths", "1", saved);
    }
}

static void Install(void)
{
    char sys[MAX_PATH], value[256], key[64], path[MAX_PATH];
    OFSTRUCT of;
    int i;

    GetSystemDirectory(sys, sizeof sys);
    Changes(MakeFont(sys));

    /* each original kept once: a reinstall must not overwrite them */
    for (i = 0; i < nChanges; i++) {
        CHANGE *c = changes + i;
        wsprintf(key, "%s|%s", (LPSTR)c->section, (LPSTR)c->key);
        if (Get(c->file, c->section, c->key, value, sizeof value) && lstrcmpi(value, c->value) &&
            !GetPrivateProfileString(c->file, key, "", path, sizeof path, saved))
            WritePrivateProfileString(c->file, key, value, saved);
    }
    for (i = 0; i < nChanges; i++)
        Set(changes[i].file, changes[i].section, changes[i].key, changes[i].value);

    InstallLfn(sys);
    lstrcpy(path, sys);                 /* an old version's library */
    PathAppend(path, "SNOWSYS.DLL");
    OpenFile(path, &of, OF_DELETE);
    Flush();
}

/* Each value still as Snowball set it goes back to what it was (or goes);
   one changed since stays. The uninstaller's run= entry goes too. */
static void Uninstall(void)
{
    char value[256], before[256], key[64], *t;
    OFSTRUCT of;
    int i;

    Changes(TRUE);
    for (i = 0; i < nChanges; i++) {
        CHANGE *c = changes + i;
        wsprintf(key, "%s|%s", (LPSTR)c->section, (LPSTR)c->key);
        if (!Get(c->file, c->section, c->key, value, sizeof value) || lstrcmpi(value, c->value))
            continue;
        Set(c->file, c->section, c->key,
            GetPrivateProfileString(c->file, key, "", before, sizeof before, saved) ? (LPCSTR)before : NULL);
        WritePrivateProfileString(c->file, key, NULL, saved);
    }
    if (GetPrivateProfileInt("Snowball", "AddedLfn", 0, saved)) {
        if (Get("SYSTEM.INI", "boot", "drivers", value, sizeof value)) {
            DriversRemove(value, "lfn.drv");
            Set("SYSTEM.INI", "boot", "drivers", value[0] ? (LPCSTR)value : NULL);
        }
        GetSystemDirectory(before, sizeof before);     /* the copy Install made; its code is in memory */
        PathAppend(before, "LFN.DRV");
        OpenFile(before, &of, OF_DELETE);
    }
    if (GetPrivateProfileInt("Snowball", "AddedLongPaths", 0, saved))
        WritePrivateProfileString("lfn.drv", "LongPaths", NULL, "SYSTEM.INI");
    WritePrivateProfileString("Snowball", NULL, NULL, saved);

    GetProfileString("windows", "run", "", value, sizeof value);
    lstrcpy(before, value);
    value[0] = '\0';
    for (t = strtok(before, " "); t; t = strtok(NULL, " "))
        if (_fstrnicmp(t, app, lstrlen(app)) || _fstrnicmp(t + lstrlen(app), "UNINS", 5)) {
            if (value[0])
                lstrcat(value, " ");
            lstrcat(value, t);
        }
    WriteProfileString("windows", "run", value);
    Flush();
}

static void Remove(void)
{
    char uninstaller[MAX_PATH], run[256], value[MAX_PATH];

    lstrcpy(uninstaller, app);
    lstrcat(uninstaller, "UNINS000.EXE");
    if (!Exists(uninstaller)) {
        MessageBox(NULL, "Snowball's uninstaller (UNINS000.EXE) is missing.", TITLE, MB_OK | MB_ICONSTOP);
        return;
    }
    Get("SYSTEM.INI", "boot", "shell", value, sizeof value);
    if (!FindWindow(TASKBAR_CLASS, NULL) || lstrcmpi(value, shell)) {
        WinExec(uninstaller, SW_SHOWNORMAL);
        return;
    }
    if (MessageBox(NULL, "Snowball is the shell now. To remove it, Windows restarts with Program Manager "
                   "and removes Snowball then.\n\nContinue?", TITLE, MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
        return;
    Uninstall();
    GetProfileString("windows", "run", "", run, sizeof run);
    if (run[0])
        lstrcat(run, " ");
    lstrcat(run, uninstaller);
    WriteProfileString("windows", "run", run);
    ExitWindows(MAKELONG(EW_RESTARTWINDOWS, 0), 0);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int show)
{
    GetModuleFileName(hInst, app, sizeof app);
    *(strrchr(app, '\\') + 1) = '\0';   /* Snowball's folder, with its '\' */
    lstrcpy(saved, app);
    lstrcat(saved, SAVED);
    lstrcpy(shell, app);
    lstrcat(shell, "SNOWBALL.EXE");
    AnsiLower(cmdLine);
    if (!lstrcmp(cmdLine, "/install"))
        Install();
    else if (!lstrcmp(cmdLine, "/uninstall"))
        Uninstall();
    else if (!lstrcmp(cmdLine, "/remove"))
        Remove();
    else
        MessageBox(NULL, "SBSETUP /install, /uninstall or /remove: run by Snowball's Setup and its uninstaller.",
                   TITLE, MB_OK | MB_ICONINFORMATION);
    return 0;
}
