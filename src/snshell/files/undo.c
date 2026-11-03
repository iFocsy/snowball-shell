/*
 * Undo, as Windows 98's Edit menu has it: the last file operations (up to
 * ten) - a rename, a move, a copy, a delete to the Recycle Bin - each kept
 * as the paths it made, before and after, and undone the other way: names
 * back, moves back, copies deleted (asked first), deleted items restored.
 * Undoing records nothing itself.
 */
#include "internal.h"

#define UNDO_MAX    10

typedef struct {
    int      what;                      /* UNDO_* */
    PATHLIST pairs;                     /* before, after, before, after... */
} UNDO;

static UNDO stack[UNDO_MAX];
static int depth;
static UNDO building;
static BOOL undoing;

void UndoBegin(int what)
{
    if (undoing)
        return;
    PathListFree(&building.pairs);
    PathListInit(&building.pairs);
    building.what = what;
}

void UndoPair(LPCSTR before, LPCSTR after)
{
    if (!undoing && building.what && PathListAdd(&building.pairs, before))
        PathListAdd(&building.pairs, after);
}

/* The operation is over: it is the one undone next (the oldest of ten goes). */
void UndoEnd(void)
{
    if (undoing || !building.what)
        return;
    if (building.pairs.count) {
        if (depth == UNDO_MAX) {
            PathListFree(&stack[0].pairs);
            _fmemmove(stack, stack + 1, (UNDO_MAX - 1) * sizeof stack[0]);
            depth--;
        }
        stack[depth++] = building;
        PathListInit(&building.pairs);
    } else {
        PathListFree(&building.pairs);
    }
    building.what = 0;
}

/* "&Undo Delete" for the Edit menu; "&Undo" with nothing to undo. */
BOOL UndoMenuText(LPSTR out)
{
    static const char *Names[] = { "", "Rename", "Move", "Copy", "Delete" };

    wsprintf(out, depth ? "&Undo %s\tCtrl+Z" : "&Undo\tCtrl+Z", (LPSTR)Names[depth ? stack[depth - 1].what : 0]);
    return depth > 0;
}

void UndoLast(HWND owner)
{
    char folder[MAX_PATH], parent[MAX_PATH];
    UNDO u;
    PATHLIST afters, back;
    LPSTR p, before;
    int rounds;

    if (!depth || FileOperationBusy()) {
        MessageBeep(0);
        return;
    }
    u = stack[--depth];
    undoing = TRUE;
    PathListInit(&afters);
    for (p = PathListLock(&u.pairs); *p; p = PathListNext(PathListNext(p)))
        PathListAdd(&afters, PathListNext(p));
    PathListUnlock(&u.pairs);
    switch (u.what) {
    case UNDO_RENAME:
        p = PathListLock(&u.pairs);
        if (!DosRename(PathListNext(p), p))
            FileOperationError(owner, "rename", PathListNext(p), DosLastError());
        lstrcpy(folder, p);
        PathListUnlock(&u.pairs);
        PathToParent(folder);
        FileCommandsFolderChanged(folder);
        break;
    case UNDO_MOVE:                     /* back to each one's folder, a folder at a time */
        folder[0] = '\0';
        for (rounds = 0; rounds < u.pairs.count / 2; rounds++) {    /* those that cannot go back stay */
            PathListInit(&back);
            for (p = PathListLock(&u.pairs); *p; p = PathListNext(PathListNext(p))) {
                before = p;
                lstrcpy(parent, before);
                PathToParent(parent);
                if (DosGetAttributes(PathListNext(p)) != 0xFFFF && (!back.count || !lstrcmpi(parent, folder))) {
                    lstrcpy(folder, parent);
                    PathListAdd(&back, PathListNext(p));
                }
            }
            PathListUnlock(&u.pairs);
            if (back.count) {
                FileOperationRun(owner, FOP_MOVE, &back, folder);
                FileCommandsFolderChanged(folder);
                p = PathListLock(&back);
                lstrcpy(parent, p);
                PathListUnlock(&back);
                PathToParent(parent);
                FileCommandsFolderChanged(parent);
            }
            rounds = back.count ? rounds : u.pairs.count;   /* none left to go back: done */
            PathListFree(&back);
        }
        break;
    case UNDO_COPY:                     /* the copies, to the Recycle Bin (asked) */
        FileCommandsDelete(owner, &afters, FALSE);
        break;
    case UNDO_DELETE:
        RecycleBinRestore(owner, &afters);
        break;
    }
    PathListFree(&afters);
    PathListFree(&u.pairs);
    undoing = FALSE;
}
