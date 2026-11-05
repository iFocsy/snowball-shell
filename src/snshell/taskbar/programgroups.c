/*
 * Program Manager's groups (its binary .GRP files) as Start Menu\Programs
 * folders, one shortcut per item: on the first run, every group PROGMAN.INI
 * lists; on Taskbar Properties' Sync, the ones new there or whose file
 * changed (a setup that wrote to Program Manager itself, Program Manager run
 * beside Snowball), as Windows 95's GRPCONV did at each start; and any .GRP
 * opened, after asking. A group goes into the folder of its name and each item over the
 * shortcut of its title, so converting again updates and never duplicates;
 * a group converted again writes only its items new or changed since the
 * last time (a copy of each group file as converted is kept), so changes
 * made in the Start menu stay. Nothing is ever deleted.
 */
#include "internal.h"

#define GRP_ITEMS   34          /* offset of the item table in the header */
#define TAG_DIR     0x8101
#define TAG_MIN     0x8103
#define STAMPS      "Program Groups"    /* SNOWBALL.INI: each group file's size and time when converted */

typedef struct {
    HGLOBAL mem;
    BYTE far *grp;              /* the file, and a 0 after it: every string ends inside */
    WORD size;
} GROUP;

static WORD Word(BYTE far *p) { return *(WORD far *)p; }

/* A string the file points to; past its end, the "" there (a damaged file). */
static LPSTR Str(GROUP *g, WORD at)
{
    return (LPSTR)(g->grp + (at < g->size ? at : g->size));
}

static WORD ItemCount(GROUP *g)         /* slots in the item table */
{
    return min(Word(g->grp + 32), (g->size - GRP_ITEMS) / 2);
}

/* Slot i's item, its fields (24 bytes) inside the file; 0: an empty slot. */
static WORD Item(GROUP *g, WORD i)
{
    WORD at = Word(g->grp + GRP_ITEMS + 2 * i);
    return at <= g->size - 24 ? at : 0;
}

static LPSTR Title(GROUP *g, WORD i) { return Str(g, Word(g->grp + Item(g, i) + 18)); }

static void GroupFree(GROUP *g)
{
    if (g->mem) {
        GlobalUnlock(g->mem);
        GlobalFree(g->mem);
    }
    _fmemset(g, 0, sizeof *g);
}

static BOOL GroupLoad(LPCSTR path, GROUP *g)
{
    LONG len;
    HFILE h;

    _fmemset(g, 0, sizeof *g);
    if ((h = DosOpenRead(path)) == HFILE_ERROR)
        return FALSE;
    len = _llseek(h, 0, 2);
    _llseek(h, 0, 0);
    if (len > GRP_ITEMS && len < 65000L && (g->mem = GlobalAlloc(GMEM_MOVEABLE, (WORD)len + 1)) != NULL) {
        g->grp = GlobalLock(g->mem);
        g->size = (WORD)len;
        if (_lread(h, g->grp, g->size) == g->size && !_fmemcmp(g->grp, "PMCC", 4))
            g->grp[g->size] = 0;
        else
            GroupFree(g);
    }
    _lclose(h);
    return g->grp != NULL;
}

/* PROGMAN.INI's name alone is in the Windows folder, as Program Manager has it. */
static void GroupPath(LPCSTR file, LPSTR out)
{
    out[0] = '\0';
    if (!_fstrchr(file, '\\') && !_fstrchr(file, ':'))
        GetWindowsDirectory(out, MAX_PATH);
    PathAppend(out, file);
}

/* Which of the group's items whose titles make the same name slot i holds:
   1, 2... (in 8.3, "Calculator" and "Calculator (32-Bit)" both make CALCULAT). */
static int Nth(GROUP *g, WORD i, BOOL longNames)
{
    char name[MAX_PATH], other[MAX_PATH];
    WORD j;
    int n = 1;

    ProgramManagerItemNameAs("", Title(g, i), "", 1, longNames, name);
    for (j = 0; j < i; j++) {
        if (!Item(g, j))
            continue;
        ProgramManagerItemNameAs("", Title(g, j), "", 1, longNames, other);
        if (!lstrcmpi(name, other))
            n++;
    }
    return n;
}

/* Slot i's working directory, from the tagged data after the group; TRUE:
   it runs minimized. */
static BOOL ItemTags(GROUP *g, WORD i, LPSTR workDir, int size)
{
    BOOL minimized = FALSE;
    WORD tag, cb;

    *workDir = '\0';
    for (tag = Word(g->grp + 4 + 2); tag <= g->size - 6; tag += cb) {   /* cbGroup */
        WORD id = Word(g->grp + tag), forItem = Word(g->grp + tag + 2);
        cb = Word(g->grp + tag + 4);
        if (id == 0xFFFF || cb < 6 || cb > g->size - tag)
            break;
        if (forItem == i && id == TAG_DIR)
            lstrcpyn(workDir, (LPSTR)(g->grp + tag + 6), size);
        if (forItem == i && id == TAG_MIN)
            minimized = TRUE;
    }
    return minimized;
}

static BOOL ConvertItem(LPCSTR dir, GROUP *g, WORD i)
{
    BYTE far *it = g->grp + Item(g, i);
    char workDir[MAX_PATH];
    BOOL minimized = ItemTags(g, i, workDir, sizeof workDir);

    return ProgramManagerWriteItem(dir, Title(g, i), Str(g, Word(it + 20)), workDir,
                                   Str(g, Word(it + 22)), Word(it + 4), minimized, Nth(g, i, DosHasLongNames()));
}

/* Slot i of g is an item of last (the group as last converted) as it was:
   title, command, icon, working directory and minimized all the same. */
static BOOL Unchanged(GROUP *g, WORD i, GROUP *last)
{
    BYTE far *a = g->grp + Item(g, i), far *b;
    char dirA[MAX_PATH], dirB[MAX_PATH];
    BOOL minA = ItemTags(g, i, dirA, sizeof dirA);
    WORD j, count = last ? ItemCount(last) : 0;

    for (j = 0; j < count; j++) {
        if (!Item(last, j))
            continue;
        b = last->grp + Item(last, j);
        if (!lstrcmp(Title(g, i), Title(last, j)) && !lstrcmp(Str(g, Word(a + 20)), Str(last, Word(b + 20)))
            && !lstrcmp(Str(g, Word(a + 22)), Str(last, Word(b + 22))) && Word(a + 4) == Word(b + 4)
            && ItemTags(last, j, dirB, sizeof dirB) == minA && !lstrcmp(dirA, dirB))
            return TRUE;
    }
    return FALSE;
}

/* The group into the folder of its name under programs (dir gets it): with
   last, only its items new or changed since, so what was done in the Start
   menu since (an item deleted, renamed, edited; the group deleted) stays as
   it is; nothing is deleted. The folder is made once an item needs it (and
   without last, always). Its number of items written; -1: no folder. */
static int GroupConvert(LPCSTR programs, GROUP *g, GROUP *last, LPSTR dir)
{
    WORD i, count = ItemCount(g);
    int n = 0;

    ProgramManagerItemName(programs, Str(g, Word(g->grp + 22)), "", 1, dir);
    if (!last && !PathIsFolder(dir) && !DosMakeFolder(dir))
        return -1;
    for (i = 0; i < count; i++) {
        if (!Item(g, i) || Unchanged(g, i, last))
            continue;
        if (!PathIsFolder(dir) && !DosMakeFolder(dir))
            return -1;
        if (ConvertItem(dir, g, i))
            n++;
    }
    return n;
}

/* Where the copy of a group file as last converted is kept: SHELL\GROUPS. */
static void CopyPath(LPCSTR path, LPSTR out)
{
    lstrcpy(out, g_home);
    PathAppend(out, "GROUPS");
    DosMakeFolder(out);
    PathAppend(out, PathFileName((LPSTR)path));
}

static void GroupSave(GROUP *g, LPCSTR copy)
{
    HFILE h = DosCreate(copy);

    if (h != HFILE_ERROR) {
        _lwrite(h, (LPCSTR)g->grp, g->size);
        _lclose(h);
    }
}

/* A group file's size and time of writing: changed, it is converted again. */
static BOOL Stamp(LPCSTR path, LPSTR out)
{
    DOSENTRY e;

    if (!DosGetEntry(path, &e))
        return FALSE;
    wsprintf(out, "%lu %u %u", e.size, e.date, e.time);
    return TRUE;
}

/* "Windows Explorer" in dir: Snowball opening C:\ with the folder tree. */
BOOL ProgramManagerAddExplorer(LPCSTR dir)
{
    char path[MAX_PATH];
    SHORTCUT s;

    _fmemset(&s, 0, sizeof s);
    lstrcpy(s.target, g_home);
    lstrcat(s.target, "SNOWBALL.EXE");
    lstrcpy(s.args, "/E,C:\\");
    IconsArtFile(s.icon);
    s.iconIndex = IDI_EXPLORER - 1;     /* icons are numbered in id order from 0 */
    s.show = SW_SHOWNORMAL;
    lstrcpy(path, dir);
    PathAppend(path, DosHasLongNames() ? "Windows Explorer" SHC_EXT : "EXPLORER" SHC_EXT);
    return ShortcutWrite(path, &s);
}

static void AddExplorer(LPCSTR programs)
{
    if (!SettingsGetInt("Start", "Explorer", 0) && PathIsFolder(programs) && ProgramManagerAddExplorer(programs))
        SettingsSetInt("Start", "Explorer", 1);
}

/* Snowball's own tools, in Accessories' System Tools (or Programs'): each
   made once ([Start] SystemTools counts them), so one deleted stays so. */
static const struct { LPCSTR name, dosName, args; int icon; } tools[] = {
    { "Resource Meter", "RESMETER", "/RESMETER", IDI_RESMETER },
    { "Disk Cleanup", "CLEANUP", "/CLEANUP", IDI_HARDDISK },
};

static void AddSystemTools(LPCSTR programs)
{
    char dir[MAX_PATH], path[MAX_PATH];
    SHORTCUT s;
    int i = SettingsGetInt("Start", "SystemTools", 0);

    if (i >= sizeof tools / sizeof tools[0] || !PathIsFolder(programs))
        return;
    ProgramManagerItemName(programs, "Accessories", "", 1, dir);     /* ACCESSOR in 8.3 */
    if (!PathIsFolder(dir))
        lstrcpy(dir, programs);
    PathAppend(dir, DosHasLongNames() ? "System Tools" : "SYSTOOLS");
    DosMakeFolder(dir);
    for (; i < sizeof tools / sizeof tools[0]; i++) {
        _fmemset(&s, 0, sizeof s);
        lstrcpy(s.target, g_home);
        lstrcat(s.target, "SNOWBALL.EXE");
        lstrcpy(s.args, tools[i].args);
        IconsArtFile(s.icon);
        s.iconIndex = tools[i].icon - 1;
        s.show = SW_SHOWNORMAL;
        lstrcpy(path, dir);
        PathAppend(path, DosHasLongNames() ? tools[i].name : tools[i].dosName);
        lstrcat(path, SHC_EXT);
        ShortcutWrite(path, &s);
    }
    SettingsSetInt("Start", "SystemTools", i);
}

/* PROGMAN.INI's groups whose stamp is not the one kept in [Program Groups]:
   with convert, into the Start menu against their copies as last converted
   (no copy: the whole group); always, their stamps and copies kept. The
   number of items written. */
static int Sync(LPCSTR programs, BOOL convert)
{
    char dir[MAX_PATH], ini[MAX_PATH], path[MAX_PATH], copy[MAX_PATH], stamp[40], kept[40];
    HGLOBAL mem;
    LPSTR keys, k;
    GROUP g, last;
    BOOL had;
    int n = 0;

    GetWindowsDirectory(ini, sizeof ini);
    PathAppend(ini, "PROGMAN.INI");
    if ((mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 1024)) == NULL)
        return 0;
    keys = GlobalLock(mem);
    GetPrivateProfileString("Groups", NULL, "", keys, 1024, ini);
    for (k = keys; *k; k += lstrlen(k) + 1) {
        GetPrivateProfileString("Groups", k, "", dir, sizeof dir, ini);
        GroupPath(dir, path);
        if (!*dir || !Stamp(path, stamp))
            continue;
        SettingsGetString(STAMPS, path, "", kept, sizeof kept);
        CopyPath(path, copy);
        if ((!lstrcmp(stamp, kept) && DosGetAttributes(copy) != 0xFFFF) || !GroupLoad(path, &g))
            continue;
        if (convert && lstrcmp(stamp, kept)) {
            had = GroupLoad(copy, &last);
            n += max(0, GroupConvert(programs, &g, had ? &last : NULL, dir));
            if (had)
                GroupFree(&last);
        }
        GroupSave(&g, copy);
        GroupFree(&g);
        SettingsSetString(STAMPS, path, stamp);
    }
    GlobalUnlock(mem);
    GlobalFree(mem);
    SettingsSetInt("Start", "GroupStamps", 1);
    return n;
}

/* ---- long names for what Snowball named in 8.3 (it ran before they worked) ---- */

/* A group's folder and items in 8.3: the long names their titles give. */
static void LengthenGroup(LPCSTR programs, GROUP *g)
{
    char from[MAX_PATH], dir[MAX_PATH], to[MAX_PATH];
    LPSTR title = Str(g, Word(g->grp + 22));
    WORD i, count = ItemCount(g);

    ProgramManagerItemNameAs(programs, title, "", 1, FALSE, from);
    ProgramManagerItemNameAs(programs, title, "", 1, TRUE, dir);
    DosRenameFree(from, dir);
    if (!PathIsFolder(dir))
        return;
    for (i = 0; i < count; i++) {
        if (!Item(g, i))
            continue;
        ProgramManagerItemNameAs(dir, Title(g, i), SHC_EXT, Nth(g, i, FALSE), FALSE, from);
        ProgramManagerItemNameAs(dir, Title(g, i), SHC_EXT, Nth(g, i, TRUE), TRUE, to);
        DosRenameFree(from, to);
    }
}

static void RenameIn(LPCSTR dir, LPCSTR from, LPCSTR to)
{
    char a[MAX_PATH], b[MAX_PATH];

    lstrcpy(a, dir);
    PathAppend(a, from);
    lstrcpy(b, dir);
    PathAppend(b, to);
    DosRenameFree(a, b);
}

/* Snowball's own 8.3 folders and shortcuts, made while long names did not
   work, get their long names now that they do: the Start menu, its groups
   (named by the copies kept, else by the group files) and items, Windows
   Explorer, System Tools, Quick Launch, Send To's floppy. A long name that
   is already taken stays so: both are kept. */
static void LongNames(void)
{
    char programs[MAX_PATH], dir[MAX_PATH], ini[MAX_PATH], path[MAX_PATH], copy[MAX_PATH];
    HGLOBAL mem;
    LPSTR keys, k;
    GROUP g;
    int i, j;

    RenameIn(g_home, "STARTMNU", "Start Menu");
    DosLengthen(g_home, "Quick Launch");
    lstrcpy(dir, g_home);
    PathAppend(dir, "Quick Launch");
    RenameIn(dir, "EXPLORER" SHC_EXT, "Windows Explorer" SHC_EXT);
    lstrcpy(dir, g_home);
    PathAppend(dir, "SendTo");
    RenameIn(dir, "FLOPPY" SHC_EXT, "3\xBD Floppy (A)" SHC_EXT);
    ProgramManagerFolder(programs);
    if (!PathIsFolder(programs))
        return;
    RenameIn(programs, "EXPLORER" SHC_EXT, "Windows Explorer" SHC_EXT);
    GetWindowsDirectory(ini, sizeof ini);
    PathAppend(ini, "PROGMAN.INI");
    if ((mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 1024)) != NULL) {
        keys = GlobalLock(mem);
        GetPrivateProfileString("Groups", NULL, "", keys, 1024, ini);
        for (k = keys; *k; k += lstrlen(k) + 1) {
            GetPrivateProfileString("Groups", k, "", dir, sizeof dir, ini);
            GroupPath(dir, path);
            CopyPath(path, copy);
            if (*dir && (GroupLoad(copy, &g) || GroupLoad(path, &g))) {
                LengthenGroup(programs, &g);
                GroupFree(&g);
            }
        }
        GlobalUnlock(mem);
        GlobalFree(mem);
    }
    for (i = 0; i < 2; i++) {               /* System Tools: in Accessories, or Programs */
        if (i)
            ProgramManagerItemName(programs, "Accessories", "", 1, dir);
        else
            lstrcpy(dir, programs);
        RenameIn(dir, "SYSTOOLS", "System Tools");
        PathAppend(dir, "System Tools");
        for (j = 0; j < sizeof tools / sizeof tools[0]; j++) {
            wsprintf(path, "%s" SHC_EXT, tools[j].dosName);
            wsprintf(copy, "%s" SHC_EXT, tools[j].name);
            RenameIn(dir, path, copy);
        }
    }
}

/* At start: a new Start menu gets all of Program Manager's groups; one made
   before stamps were kept ([Start] GroupStamps) gets only the stamps and
   copies, so that a Sync brings what changed after, not what was removed
   before. Nothing else: Sync is asked for. */
void ProgramManagerConvertGroups(void)
{
    char programs[MAX_PATH], dir[MAX_PATH];
    BOOL first;

    if (DosHasLongNames())
        LongNames();
    lstrcpy(dir, g_home);
    PathAppend(dir, "Desktop");
    DosMakeFolder(dir);
    ProgramManagerFolder(programs);
    first = !PathIsFolder(programs);
    StartMenuFolder(dir);
    DosMakeFolder(dir);
    DosMakeFolder(programs);
    if (first || !SettingsGetInt("Start", "GroupStamps", 0))
        Sync(programs, first);
    AddExplorer(programs);
    AddSystemTools(programs);
}

/* Taskbar Properties' Sync: what Program Manager's groups gained or changed
   since the last time, into the Start menu; what was done in the Start menu
   since stays. */
void ProgramManagerSync(HWND owner)
{
    char programs[MAX_PATH], text[120];
    HCURSOR cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    int n;

    ProgramManagerFolder(programs);
    n = Sync(programs, TRUE);
    SetCursor(cursor);
    if (n)
        FileCommandsFolderChanged(programs);
    if (!n)
        lstrcpy(text, "The Start menu already has all the programs from Program Manager.");
    else
        wsprintf(text, n == 1 ? "1 item from Program Manager was added to the Start menu or updated."
                              : "%d items from Program Manager were added to the Start menu or updated.", n);
    MessageBox(owner, text, "Sync", MB_ICONINFORMATION | MB_OK);
}

/* ---- a .GRP opened ---- */

BOOL ProgramManagerIsGroupFile(LPCSTR path)
{
    char head[4];
    HFILE h;
    BOOL is;

    if (lstrcmpi(PathExtension((LPSTR)path), ".GRP") || (h = DosOpenRead(path)) == HFILE_ERROR)
        return FALSE;
    is = _lread(h, head, 4) == 4 && !_fmemcmp(head, "PMCC", 4);
    _lclose(h);
    return is;
}

/* Into the Start menu once the user agrees; its folder then opens, as a
   new group shows. FALSE: not added. */
BOOL ProgramManagerImportGroup(HWND owner, LPCSTR path)
{
    char programs[MAX_PATH], dir[MAX_PATH], text[MAX_PATH + 160], count[24];
    LPSTR name;
    GROUP g;
    WORD i, items = 0;
    int n;

    if (!GroupLoad(path, &g)) {
        wsprintf(text, "Cannot read the program group '%s'.", PathFileName((LPSTR)path));
        ErrorBox(owner, text);
        return FALSE;
    }
    for (i = 0; i < ItemCount(&g); i++)
        if (Item(&g, i))
            items++;
    wsprintf(count, items == 1 ? "%u item" : "%u items", items);
    name = Str(&g, Word(g.grp + 22));
    ProgramManagerFolder(programs);
    ProgramManagerItemName(programs, name, "", 1, dir);
    if (PathIsFolder(dir))
        wsprintf(text, "The Start menu already has a program group named '%.60s'.\n"
                       "Do you want to add this group's %s to it? Items with the same names are replaced.",
                 name, (LPSTR)count);
    else            /* (the name, from the file, cut: a damaged one could be any length) */
        wsprintf(text, "Do you want to add the program group '%.60s' and its %s to the Start menu?", name, (LPSTR)count);
    if (MessageBox(owner, text, "Import Program Group", MB_ICONQUESTION | MB_YESNO) != IDYES) {
        GroupFree(&g);
        return FALSE;
    }
    n = GroupConvert(programs, &g, NULL, dir);
    GroupFree(&g);
    if (n < 0) {
        wsprintf(text, "Cannot create the folder '%s'.", (LPSTR)dir);
        ErrorBox(owner, text);
        return FALSE;
    }
    FileCommandsFolderChanged(programs);
    FolderWindowOpen(dir);
    return TRUE;
}
