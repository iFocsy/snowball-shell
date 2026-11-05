/*
 * Copy, move and delete of files and whole folders as one operation with a
 * progress dialog. The work runs in the shell's own task: between pieces
 * Windows keeps running (the dialog pumps messages) and Cancel stops it.
 * One operation at a time. Paths grow and shrink in two shared buffers
 * while the trees are walked, so deep folders cost little stack.
 */
#include "internal.h"

#define CHUNK       32768U
#define ERR_TOOLONG 206                 /* DOS has no code for a path that does not fit */
#define ERR_FULL    39
#define ERR_INSIDE  0xFF01              /* destination inside the source */

static BOOL busy, yesAll, cancelled, weighBytes;
static int op;
static HWND owner;
static DWORD done, total;
static HGLOBAL hBuf;
static char huge *buf;
static char src[MAX_PATH], dst[MAX_PATH];
static DOSENTRY entry;                   /* scratch of the tree walks */

static const char *Verb[] = { "copy", "move", "delete", "delete" };

BOOL FileOperationBusy(void)
{
    return busy;
}

/* "Cannot copy Name: reason", worded and titled as in Windows 98. */
void FileOperationError(HWND w, LPCSTR verb, LPCSTR path, WORD code)
{
    char msg[MAX_PATH + 200], title[48];
    LPCSTR why;
    int n;

    switch (code) {
    case 2: case 3:     why = "Cannot find the specified file. Make sure you specify the correct path and filename."; break;
    case 5:             why = "Access is denied.\n\nMake sure the disk is not full or write-protected and that the file is not currently in use."; break;
    case 16:            why = "The folder is in use by a program."; break;
    case 19:            why = "The disk is write-protected."; break;
    case 21:            why = "The device is not ready."; break;
    case 32: case 33:   why = "There has been a sharing violation. The source or destination file may be in use."; break;
    case ERR_FULL:      why = "There is not enough free disk space.\n\nDelete one or more files to free disk space, and then try again."; break;
    case 80: case 183:  why = "A file with the name you specified already exists. Specify a different filename."; break;
    case ERR_TOOLONG:   why = "The filename or extension is too long."; break;
    case ERR_INSIDE:    why = "The destination folder is a subfolder of the source folder."; break;
    default:            why = "The file system reported an error."; break;
    }
    wsprintf(msg, "Cannot %s %s: %s", verb, (LPSTR)PathFileName((LPSTR)path), (LPSTR)why);
    wsprintf(title, "Error %s", verb);           /* "delete" -> "Error Deleting File or Folder" */
    title[6] = (char)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)title[6]);
    n = lstrlen(title);
    if (title[n - 1] == 'e')
        title[n - 1] = '\0';
    lstrcat(title, "ing File or Folder");
    MessageBox(w, msg, title, MB_ICONSTOP | MB_OK);
}

static BOOL Fail(LPCSTR path, WORD code)
{
    FileOperationError(owner, Verb[op], path, code);
    cancelled = TRUE;
    return FALSE;
}

/* path\name, unless it would not fit. */
static BOOL Append(LPSTR path, LPCSTR name)
{
    if (lstrlen(path) + lstrlen(name) + 2 > MAX_PATH)
        return Fail(name, ERR_TOOLONG);
    PathAppend(path, name);
    return TRUE;
}

static BOOL Advance(DWORD n)
{
    done += n;
    ProgressStep(done, total);
    if (ProgressCancelled())
        cancelled = TRUE;
    return !cancelled;
}

static DWORD Weight(DWORD size)
{
    return weighBytes ? size + 1 : 1;
}

/* Adds the weight of src (a file, or a folder and all it holds) to total. */
static void Count(WORD attr, DWORD size)
{
    DOSFIND f;
    int n = lstrlen(src);

    total += Weight(size);
    if (!(attr & FA_DIR) || !Append(src, "*.*"))
        return;
    if (DosFindFirst(src, &f, &entry)) {
        do {
            src[n] = '\0';
            if (Append(src, entry.name))
                Count(entry.attr, entry.size);
        } while (!cancelled && DosFindNext(&f, &entry));
        DosFindClose(&f);
    }
    src[n] = '\0';
}

/* Same file or folder kind already at dst: ask (files) or merge (folders).
   fresh: dst is in a folder this operation just made, so it is free. */
static BOOL MayWrite(const DOSENTRY *from, BOOL fresh)
{
    DOSENTRY there;
    int answer;

    if (fresh || !DosGetEntry(dst, &there))
        return TRUE;
    if ((there.attr & FA_DIR) != (from->attr & FA_DIR))
        return Fail(dst, 80);
    if (from->attr & FA_DIR)
        return TRUE;
    if (!yesAll) {
        answer = ConfirmReplaceAsk(owner, dst, &there, from);
        if (answer == IDCANCEL) {
            cancelled = TRUE;
            return FALSE;
        }
        if (answer == IDNO)
            return FALSE;
        yesAll = answer == IDC_YESALL;
    }
    if (there.attr & FA_READONLY)
        DosSetAttributes(dst, there.attr & ~FA_READONLY);
    return TRUE;
}

static BOOL CopyFile(const DOSENTRY *e)
{
    HFILE in, out = HFILE_ERROR;
    UINT n;
    WORD date, time;

    if ((in = DosOpenRead(src)) == HFILE_ERROR)
        return Fail(src, DosLastError());
    if ((out = DosCreate(dst)) == HFILE_ERROR) {
        _lclose(in);
        return Fail(dst, DosLastError());
    }
    do {
        n = _lread(in, buf, CHUNK);
        if (n == (UINT)HFILE_ERROR) {
            Fail(src, 5);
            break;
        }
        if (n && _lwrite(out, buf, n) != n) {
            Fail(dst, ERR_FULL);
            break;
        }
    } while (n && Advance(n));
    if (!cancelled && DosGetFileTime(in, &date, &time))
        DosSetFileTime(out, date, time);
    _lclose(in);
    _lclose(out);
    if (cancelled) {
        DosDelete(dst);                  /* no half files */
        return FALSE;
    }
    if (e->attr & (FA_READONLY | FA_HIDDEN | FA_SYSTEM))
        DosSetAttributes(dst, e->attr | FA_ARCHIVE);
    return Advance(1);
}

/* src -> dst: a file, or a folder with everything in it. */
static BOOL CopyTree(const DOSENTRY *e, BOOL fresh)
{
    char dir[MAX_PATH];
    DOSFIND f;
    int ns = lstrlen(src), nd = lstrlen(dst);

    if (!MayWrite(e, fresh))
        return !cancelled && Advance(Weight(e->size));
    lstrcpy(dir, dst);
    PathToParent(dir);
    ProgressItem(src, dir);
    if (!(e->attr & FA_DIR))
        return CopyFile(e);
    fresh = !PathIsFolder(dst);
    if (fresh && !DosMakeFolder(dst))
        return Fail(dst, DosLastError());
    if (!Advance(1) || !Append(src, "*.*"))
        return FALSE;
    if (DosFindFirst(src, &f, &entry)) {
        do {
            static DOSENTRY child;           /* valid until the next level down */
            child = entry;
            src[ns] = dst[nd] = '\0';
            if (!Append(src, child.name) || !Append(dst, child.name) || !CopyTree(&child, fresh)) {
                if (cancelled)
                    break;
            }
        } while (DosFindNext(&f, &entry));
        DosFindClose(&f);
    }
    src[ns] = dst[nd] = '\0';
    return !cancelled;
}

/* src, and everything in it when it is a folder. */
static BOOL DeleteTree(WORD attr)
{
    DOSFIND f;
    char root[4];
    int n = lstrlen(src);

    if (attr & FA_DIR) {
        if (!Append(src, "*.*"))
            return FALSE;
        if (DosFindFirst(src, &f, &entry)) {
            do {
                WORD a = entry.attr;
                src[n] = '\0';
                if (!Append(src, entry.name) || !DeleteTree(a))
                    break;
            } while (DosFindNext(&f, &entry));
            DosFindClose(&f);
        }
        src[n] = '\0';
        if (cancelled)
            return FALSE;
        wsprintf(root, "%c:\\", src[0]);
        DosChangeFolder(root);                  /* DOS cannot remove the current folder of a drive */
        if (!DosRemoveFolder(src))
            return Fail(src, DosLastError());
    } else {
        ProgressItem(src, NULL);
        if (attr & FA_READONLY)
            DosSetAttributes(src, attr & ~FA_READONLY);
        if (!DosDelete(src))
            return Fail(src, DosLastError());
    }
    return Advance(1);
}

/* src -> dst; a rename when both are on one drive, else copy and delete. */
static BOOL MoveTree(const DOSENTRY *e)
{
    if ((src[0] | 0x20) == (dst[0] | 0x20)) {
        if (DosGetAttributes(dst) == 0xFFFF && DosRename(src, dst)) {
            ProgressItem(src, NULL);
            return Advance(Weight(e->size));
        }
        if (!(e->attr & FA_DIR) && DosGetAttributes(dst) != 0xFFFF) {     /* replace a file */
            if (!MayWrite(e, FALSE))
                return !cancelled && Advance(Weight(e->size));
            if (DosDelete(dst) && DosRename(src, dst))
                return Advance(Weight(e->size));
            return Fail(src, DosLastError());
        }
    }
    if (!CopyTree(e, FALSE))            /* merging folders, or another drive */
        return FALSE;
    return DeleteTree(e->attr);
}

/* "Copy of Name", "Copy (2) of Name"... in the folder of dst. */
static BOOL CopyName(LPCSTR name)
{
    char tail[MAX_PATH];
    int n, len = lstrlen(dst);

    for (n = 1; n < 100; n++) {
        if (n == 1)
            wsprintf(tail, "Copy of %s", name);
        else
            wsprintf(tail, "Copy (%d) of %s", n, name);
        dst[len] = '\0';
        if (!Append(dst, tail))
            return FALSE;
        if (DosGetAttributes(dst) == 0xFFFF)
            return TRUE;
    }
    return Fail(name, 80);
}

static DWORD sumBytes;
static WORD sumFiles, sumFolders;

/* Adds the contents of the folder in dir (a buffer it may extend). */
static void Sum(LPSTR dir)
{
    static DOSENTRY e;                   /* its own: a copy may be running meanwhile */
    DOSFIND f;
    int n = lstrlen(dir);

    if (n + 5 > MAX_PATH)
        return;
    PathAppend(dir, "*.*");
    if (DosFindFirst(dir, &f, &e)) {
        do {
            if (e.attr & FA_DIR) {
                sumFolders++;
                dir[n] = '\0';
                if (n + lstrlen(e.name) + 2 <= MAX_PATH) {
                    PathAppend(dir, e.name);
                    Sum(dir);
                }
            } else {
                sumFiles++;
                sumBytes += e.size;
            }
        } while (DosFindNext(&f, &e));
        DosFindClose(&f);
    }
    dir[n] = '\0';
}

void FileOperationTotals(LPCSTR path, DWORD *bytes, WORD *files, WORD *folders)
{
    char dir[MAX_PATH];

    sumBytes = 0;
    sumFiles = sumFolders = 0;
    lstrcpy(dir, path);
    Sum(dir);
    *bytes += sumBytes;
    *files += sumFiles;
    *folders += sumFolders;
}

static void OneItem(LPCSTR path, LPCSTR destDir)
{
    DOSENTRY e;
    char parent[MAX_PATH];

    lstrcpy(src, path);
    if (!DosGetEntry(src, &e)) {
        Fail(src, DosLastError());
        return;
    }
    if (op == FOP_PURGE || (op == FOP_DELETE && !RecycleBinCovers(src))) {
        DeleteTree(e.attr);
        return;
    }
    if (op == FOP_DELETE) {
        if (RecycleBinReserve(src, dst)) {
            ProgressItem(src, NULL);
            if (MoveTree(&e)) {
                RecycleBinRecord(dst, src);
                UndoPair(src, dst);
            }
        } else {
            Fail(src, 5);
        }
        return;
    }
    lstrcpy(parent, src);
    PathToParent(parent);
    lstrcpy(dst, destDir);
    if (!lstrcmpi(parent, destDir)) {
        if (op == FOP_COPY && CopyName(e.name) && CopyTree(&e, TRUE))
            UndoPair(src, dst);
        return;                         /* moving into its own folder: nothing to do */
    }
    if ((e.attr & FA_DIR) && PathIsInside(destDir, src)) {
        Fail(src, ERR_INSIDE);
        return;
    }
    if (!Append(dst, e.name))
        return;
    if (op == FOP_COPY ? CopyTree(&e, FALSE) : MoveTree(&e))
        UndoPair(src, dst);
}

BOOL FileOperationRun(HWND w, int what, PATHLIST *files, LPCSTR destDir)
{
    LPSTR list, p;
    DOSENTRY e;

    if (busy || !files->count) {
        MessageBeep(0);
        return FALSE;
    }
    busy = TRUE;
    op = what;
    owner = w;
    cancelled = yesAll = FALSE;
    done = total = 0;
    list = PathListLock(files);
    /* bytes measure copies; renames and deletes go by the item */
    weighBytes = op == FOP_COPY || (op == FOP_MOVE && (*list | 0x20) != (*destDir | 0x20));
    /* (destDir is only NULL for deletes) */
    hBuf = GlobalAlloc(GMEM_MOVEABLE, CHUNK);
    buf = (char huge *)GlobalLock(hBuf);
    if (!buf) {
        Fail(list, 8);
    } else {
        ProgressOpen(w, op);
        for (p = list; *p && !cancelled; p = PathListNext(p)) {
            lstrcpy(src, p);
            if (op == FOP_PURGE || weighBytes || (op == FOP_DELETE && !RecycleBinCovers(src))) {
                if (DosGetEntry(src, &e))
                    Count(e.attr, e.size);
            } else {
                total++;
            }
        }
        UndoBegin(op == FOP_COPY ? UNDO_COPY : op == FOP_MOVE ? UNDO_MOVE : op == FOP_DELETE ? UNDO_DELETE : 0);
        for (p = list; *p && !cancelled; p = PathListNext(p))
            OneItem(p, destDir);
        UndoEnd();
        ProgressClose();
        GlobalUnlock(hBuf);
    }
    if (hBuf)
        GlobalFree(hBuf);
    PathListUnlock(files);
    busy = FALSE;
    return !cancelled;
}
