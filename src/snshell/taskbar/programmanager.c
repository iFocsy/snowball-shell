/*
 * Program Manager's part, played by the Start menu: setup programs that still
 * talk to "PROGMAN" through DDE (CreateGroup, AddItem...) get their groups as
 * Start Menu\Programs folders with one shortcut per item, as programgroups.c
 * makes them of the .GRP files.
 */
#include "internal.h"
#include <dde.h>
#include <stdlib.h>

#define DDE_NAME    "PROGMAN"   /* the service and its topic */
#define DATA_MAX    16384       /* the longest answer to a request */
#define MAX_ARGS    9           /* AddItem's */

void ProgramManagerFolder(LPSTR out)
{
    StartMenuFolder(out);
    PathAppend(out, "Programs");
}

/* dir\title+ext as a file name: long names keep the title (minus characters
   DOS forbids), 8.3 names use its first letters. nth 1, 2...: the title's
   name, then "Title (2)"..., taken or not, so the same title and nth always
   give the same name and Program Manager's names find their groups and items
   again; nth 0: the first of those not taken. Long or 8.3 as asked. */
void ProgramManagerItemNameAs(LPCSTR dir, LPCSTR title, LPCSTR ext, int nth, BOOL longNames, LPSTR out)
{
    char base[64], name[80];
    int i, n = 0, tries;

    for (i = 0; title[i] && n < (longNames ? 60 : 8); i++) {
        char c = title[i];
        if (longNames ? (c >= ' ' && !_fstrchr("\\/:*?\"<>|", c) && (n || c != ' '))
                      : IsCharAlphaNumeric(c))
            base[n++] = c;
    }
    while (n && (base[n - 1] == ' ' || base[n - 1] == '.'))
        n--;
    base[n] = '\0';
    if (!n)
        lstrcpy(base, "ITEM");
    for (tries = nth ? min(nth, 99) : 1; tries < 100; tries++) {
        if (tries == 1)
            wsprintf(name, "%s%s", (LPSTR)base, ext);
        else if (longNames)
            wsprintf(name, "%s (%d)%s", (LPSTR)base, tries, ext);
        else
            wsprintf(name, "%.6s%02d%s", (LPSTR)base, tries, ext);
        lstrcpy(out, dir);
        PathAppend(out, name);
        if (nth || DosGetAttributes(out) == 0xFFFF)
            return;
    }
}

/* ... as names are made now. */
void ProgramManagerItemName(LPCSTR dir, LPCSTR title, LPCSTR ext, int nth, LPSTR out)
{
    ProgramManagerItemNameAs(dir, title, ext, nth, DosHasLongNames(), out);
}

/* A Program Manager item as a shortcut in dir, named as ProgramManagerItemName's
   nth; no title: the program's name. */
BOOL ProgramManagerWriteItem(LPCSTR dir, LPCSTR title, LPCSTR cmd, LPCSTR workDir,
                             LPCSTR icon, int iconIndex, BOOL minimized, int nth)
{
    char shc[MAX_PATH], prog[MAX_PATH];
    OFSTRUCT of;
    SHORTCUT s;

    _fmemset(&s, 0, sizeof s);
    LaunchSplitCommand(cmd, prog, s.args, sizeof s.args);
    if (!*prog)
        return FALSE;
    /* programs without a path: find where Windows would run them from */
    if (!_fstrchr(prog, '\\') && OpenFile(prog, &of, OF_EXIST) != HFILE_ERROR)
        OemToAnsi(of.szPathName, s.target);
    else
        lstrcpy(s.target, prog);
    lstrcpyn(s.dir, workDir, sizeof s.dir);
    lstrcpyn(s.icon, icon, sizeof s.icon);
    s.iconIndex = *icon ? iconIndex : 0;
    s.show = minimized ? SW_SHOWMINNOACTIVE : SW_SHOWNORMAL;
    if (!*title) {
        title = PathFileName(prog);
        *PathExtension(prog) = '\0';
    }
    ProgramManagerItemName(dir, title, SHC_EXT, nth, shc);
    return ShortcutWrite(shc, &s);
}

/* A shortcut to cmd in dir, for Taskbar Properties' Add: a name of its
   own if title is taken. */
BOOL ProgramManagerAddItem(LPCSTR dir, LPCSTR title, LPCSTR cmd)
{
    return ProgramManagerWriteItem(dir, title, cmd, "", "", 0, FALSE, 0);
}

/* ---- DDE: what setup programs ask of Program Manager ---- */

static char group[MAX_PATH];            /* chosen by CreateGroup or ShowGroup */

typedef struct {                        /* one "[Name(arg,"arg",...)]" */
    char  name[16];
    int   argc;
    char *argv[MAX_ARGS];
    char  text[600];
} DDECMD;

static LPCSTR Blanks(LPCSTR p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

static char *Arg(DDECMD *c, int i) { return i < c->argc ? c->argv[i] : ""; }

/* Reads one command into c; returns what follows it, NULL if malformed. */
static LPCSTR ParseCommand(LPCSTR p, DDECMD *c)
{
    char *out = c->text, *end = c->text + sizeof c->text - 1, *start;
    int n = 0;

    c->argc = 0;
    if (*(p = Blanks(p)) != '[')
        return NULL;
    for (p = Blanks(p + 1); IsCharAlpha(*p); p++)
        if (n < sizeof c->name - 1)
            c->name[n++] = *p;
    c->name[n] = '\0';
    if (*(p = Blanks(p)) == '(') {
        do {
            p = Blanks(p + 1);
            if (c->argc < MAX_ARGS)
                c->argv[c->argc++] = out;
            start = out;
            if (*p == '"') {
                for (p++; *p && *p != '"'; p++)
                    if (out < end)
                        *out++ = *p;
                if (*p == '"')
                    p++;
            } else {
                for (; *p && *p != ',' && *p != ')'; p++)
                    if (out < end)
                        *out++ = *p;
                while (out > start && out[-1] == ' ')
                    out--;
            }
            *out = '\0';
            if (out < end)
                out++;
            p = Blanks(p);
        } while (*p == ',');
        if (*p++ != ')')
            return NULL;
    }
    p = Blanks(p);
    return *p == ']' ? p + 1 : NULL;
}

static const char *Commands[] = { "CreateGroup", "ShowGroup", "DeleteGroup", "AddItem",
                                  "DeleteItem", "ReplaceItem", "Reload", "ExitProgman" };

static BOOL Command(DDECMD *c)
{
    char programs[MAX_PATH], path[MAX_PATH], *dir;
    PATHLIST l;
    int i, show;
    BOOL ok;

    for (i = 0; i < 8 && lstrcmpi(c->name, Commands[i]); i++)
        ;
    ProgramManagerFolder(programs);
    dir = *group ? group : programs;
    switch (i) {
    case 0:                             /* CreateGroup(name[, file]): made or found */
    case 1:                             /* ShowGroup(name, show) */
        if (!*Arg(c, 0))
            return FALSE;
        ProgramManagerItemName(programs, Arg(c, 0), "", 1, path);
        if (!PathIsFolder(path)) {
            if (i == 1 || !DosMakeFolder(path))
                return FALSE;
            FileCommandsFolderChanged(programs);
        }
        lstrcpy(group, path);
        show = i == 1 && c->argc > 1 ? atoi(Arg(c, 1)) : SW_SHOWNORMAL;
        if (show != SW_HIDE && show != SW_MINIMIZE && show != SW_SHOWMINIMIZED && show != SW_SHOWMINNOACTIVE)
            FolderWindowOpen(path);           /* as Windows 95 shows a new group */
        return TRUE;
    case 2:                             /* DeleteGroup(name): to the Recycle Bin */
        ProgramManagerItemName(programs, Arg(c, 0), "", 1, path);
        if (!*Arg(c, 0) || !PathIsFolder(path))
            return FALSE;
        PathListInit(&l);
        ok = PathListAdd(&l, path) && FileOperationRun(NULL, FOP_DELETE, &l, NULL);
        PathListFree(&l);
        FileCommandsFolderChanged(programs);
        if (!lstrcmpi(group, path))
            *group = '\0';
        return ok;
    case 3:                             /* AddItem(command[, name, icon, index, x, y, dir, hotkey, minimized]) */
        if (!ProgramManagerWriteItem(dir, Arg(c, 1), Arg(c, 0), Arg(c, 6), Arg(c, 2), atoi(Arg(c, 3)), atoi(Arg(c, 8)), 1))
            return FALSE;
        FileCommandsFolderChanged(dir);
        return TRUE;
    case 4:                             /* DeleteItem(name) */
    case 5:                             /* ReplaceItem(name): an AddItem follows */
        ProgramManagerItemName(dir, Arg(c, 0), SHC_EXT, 1, path);
        ok = DosDelete(path);
        FileCommandsFolderChanged(dir);
        return ok || i == 5;
    case 6:                             /* Reload, ExitProgman: nothing to do */
    case 7:
        return TRUE;
    }
    return FALSE;
}

/* The commands of one WM_DDE_EXECUTE; the first failure stops them. */
static BOOL Execute(LPCSTR p)
{
    DDECMD c;

    while (*(p = Blanks(p)))
        if ((p = ParseCommand(p, &c)) == NULL || !Command(&c))
            return FALSE;
    return TRUE;
}

static void Put(LPSTR data, WORD *used, LPCSTR text)
{
    WORD n = lstrlen(text);

    if (*used + n < DATA_MAX) {
        _fmemcpy(data + *used, text, n);
        *used += n;
    }
}

/* A request's answer as DDE data: the groups ("Groups", or "PROGMAN" as in
   Windows 3.1), one per line; or for a group's name, the group and then
   its items, in Program Manager's format. NULL if there is no such group. */
static HGLOBAL Answer(LPCSTR item)
{
    char dir[MAX_PATH], spec[MAX_PATH], title[MAX_PATH], line[1024];
    DOSENTRY e;
    DOSFIND f;
    SHORTCUT s;
    HGLOBAL h, small;
    LPSTR data;
    WORD used = 4, count = 0;
    BOOL list = !lstrcmpi(item, "Groups") || !lstrcmpi(item, DDE_NAME);

    ProgramManagerFolder(dir);
    if (!list) {
        ProgramManagerItemName(dir, item, "", 1, spec);
        if (!PathIsFolder(spec))
            return NULL;
        lstrcpy(dir, spec);
    }
    if ((h = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, DATA_MAX + 1)) == NULL)
        return NULL;
    data = GlobalLock(h);
    ((WORD far *)data)[0] = 0x3000;     /* DDEDATA: fResponse, fRelease */
    ((WORD far *)data)[1] = CF_TEXT;
    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (!list) {
        if (DosFindFirst(spec, &f, &e)) {
            do
                count += PathIsShortcut(e.name);
            while (DosFindNext(&f, &e));
            DosFindClose(&f);
        }
        wsprintf(line, "\"%s\",%s,%u\r\n", (LPSTR)item, (LPSTR)dir, count);
        Put(data, &used, line);
    }
    if (DosFindFirst(spec, &f, &e)) {
        do {
            if (list && (e.attr & FA_DIR) && !(e.attr & FA_HIDDEN)) {
                Put(data, &used, e.name);
                Put(data, &used, "\r\n");
            } else if (!list && PathIsShortcut(e.name)) {
                lstrcpy(title, dir);
                PathAppend(title, e.name);
                if (ShortcutRead(title, &s)) {
                    DisplayNameOfFile(e.name, title);
                    wsprintf(line, "\"%s\",\"%s%s%s\",%s,%s,0,0,%d,0,%d\r\n", (LPSTR)title,
                             (LPSTR)s.target, (LPSTR)(*s.args ? " " : ""), (LPSTR)s.args,
                             (LPSTR)s.dir, (LPSTR)s.icon, s.iconIndex, s.show == SW_SHOWMINNOACTIVE);
                    Put(data, &used, line);
                }
            }
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
    data[used] = '\0';
    GlobalUnlock(h);
    small = GlobalReAlloc(h, used + 1, GMEM_MOVEABLE | GMEM_DDESHARE);
    return small ? small : h;
}

static BOOL IsName(ATOM a)              /* no atom: any service or topic */
{
    char name[16];
    return !a || (GlobalGetAtomName(a, name, sizeof name) && !lstrcmpi(name, DDE_NAME));
}

/* The raw DDE protocol (DDEML clients speak it too); the client's window
   is in wp, and each conversation is that window and this one. */
LRESULT CALLBACK __export ProgramManagerProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND client = (HWND)wp;
    char item[MAX_PATH];
    HGLOBAL h;
    LPCSTR text;
    BOOL ok;

    switch (msg) {
    case WM_DDE_INITIATE:
        if (IsName(LOWORD(lp)) && IsName(HIWORD(lp)))
            SendMessage(client, WM_DDE_ACK, (WPARAM)w,
                        MAKELPARAM(GlobalAddAtom(DDE_NAME), GlobalAddAtom(DDE_NAME)));
        return 0;
    case WM_DDE_EXECUTE:                /* the client frees the commands after the ack */
        h = (HGLOBAL)HIWORD(lp);
        ok = (text = GlobalLock(h)) != NULL && Execute(text);
        GlobalUnlock(h);
        PostMessage(client, WM_DDE_ACK, (WPARAM)w, MAKELPARAM(ok ? 0x8000 : 0, h));
        return 0;
    case WM_DDE_REQUEST:                /* the item's atom goes back with the answer */
        GlobalGetAtomName(HIWORD(lp), item, sizeof item);
        h = LOWORD(lp) == CF_TEXT ? Answer(item) : NULL;
        if (!h || !PostMessage(client, WM_DDE_DATA, (WPARAM)w, MAKELPARAM(h, HIWORD(lp)))) {
            if (h)
                GlobalFree(h);
            PostMessage(client, WM_DDE_ACK, (WPARAM)w, MAKELPARAM(0, HIWORD(lp)));
        }
        return 0;
    case WM_DDE_ADVISE:                 /* nothing to watch or set */
    case WM_DDE_UNADVISE:
    case WM_DDE_POKE:
        PostMessage(client, WM_DDE_ACK, (WPARAM)w, MAKELPARAM(0, HIWORD(lp)));
        return 0;
    case WM_DDE_TERMINATE:
        PostMessage(client, WM_DDE_TERMINATE, (WPARAM)w, 0L);
        return 0;
    case WM_CLOSE:                      /* closing "Program Manager" does not end the shell */
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

/* The window setup programs find as Program Manager: hidden, of class
   "Progman" as in Windows 95, and without size should one show it. */
BOOL ProgramManagerServe(void)
{

    return WindowRegister("Progman", ProgramManagerProc, 0, 0, NULL, NULL) &&
           CreateWindow("Progman", "Program Manager", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
}
