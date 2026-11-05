/*
 * Open and Save As as in Windows 98, for every program: SNSYSTEM hands the
 * shell COMMDLG's GetOpenFileName and GetSaveFileName (SNSYSTEM.DLL, filedialogs.c)
 * while the program waits. Long names are shown; the program gets the 8.3
 * alias of what is chosen, as under Windows 95, unless it asks for long
 * names (OFN_LONGNAMES). A new long name is created first, so it has one.
 */
#include "internal.h"

#define SECTION     "OpenSave"
#define FD_PROP     "SbFd"
#define FDM_LOOKIN  (WM_USER + 200)     /* wParam: the place chosen in Look in */
#define HELPMSG     "commdlg_help"      /* COMMDLG's HELPMSGSTRING */

typedef struct {
    FILEREQUEST FAR *req;
    OPENFILENAME FAR *ofn;
    char path[MAX_PATH];                /* shown: a folder, "" My Computer, PLACE_DESKTOP */
    char only[128];                     /* the patterns of the files listed */
    char put[MAX_PATH];                 /* what choosing in the view put in File name */
    HWND view, tools;
    SHORTCUT shc;
} FD;

static const char allFiles[] = "*.*";

#define Fd(dlg)     ((FD *)LocalHeapPointer((WORD)GetProp(dlg, FD_PROP)))

static int Say(HWND dlg, LPCSTR format, LPCSTR name, UINT type)
{
    char text[MAX_PATH + 96], title[80];

    wsprintf(text, format, name);
    GetWindowText(dlg, title, sizeof title);
    return MessageBox(dlg, text, title, type);
}

/* ---- Files of type ---- */

static void AddType(HWND cb, LPCSTR text, LPCSTR patterns)
{
    int i = (int)SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)text);
    SendMessage(cb, CB_SETITEMDATA, i, (LPARAM)patterns);
}

/* The program's own filter (if it kept one), then its list; the patterns
   stay in its memory. */
static void FillTypes(HWND dlg, FD *fd)
{
    OPENFILENAME FAR *o = fd->ofn;
    HWND cb = GetDlgItem(dlg, IDC_OPENSAVE_TYPE);
    BOOL custom = o->lpstrCustomFilter && *o->lpstrCustomFilter;
    LPCSTR p;

    if (custom)
        AddType(cb, o->lpstrCustomFilter, o->lpstrCustomFilter + lstrlen(o->lpstrCustomFilter) + 1);
    for (p = o->lpstrFilter; p && *p; p += lstrlen(p) + 1) {
        AddType(cb, p, p + lstrlen(p) + 1);
        p += lstrlen(p) + 1;
    }
    if (!SendMessage(cb, CB_GETCOUNT, 0, 0L))
        AddType(cb, "All Files (*.*)", allFiles);
    SendMessage(cb, CB_SETCURSEL, o->nFilterIndex ? (int)o->nFilterIndex - 1 + custom : 0, 0L);
    if (SendMessage(cb, CB_GETCURSEL, 0, 0L) < 0)
        SendMessage(cb, CB_SETCURSEL, 0, 0L);
}

static LPCSTR Patterns(HWND dlg)
{
    HWND cb = GetDlgItem(dlg, IDC_OPENSAVE_TYPE);
    int i = (int)SendMessage(cb, CB_GETCURSEL, 0, 0L);

    return i < 0 ? allFiles : (LPCSTR)SendMessage(cb, CB_GETITEMDATA, i, 0L);
}

/* ---- the folder shown ---- */

static void EnableTools(HWND dlg, FD *fd)
{
    ToolbarEnable(fd->tools, IDC_OPENSAVE_UP, lstrcmp(fd->path, PLACE_DESKTOP) != 0);
    ToolbarEnable(fd->tools, IDC_OPENSAVE_NEWFOLDER, *fd->path != '\0');
}

static void Reload(FD *fd, BOOL keep)
{
    char desk[MAX_PATH];

    IconViewClear(fd->view, keep);
    if (!lstrcmp(fd->path, PLACE_DESKTOP)) {
        DesktopFolder(desk);
        IconViewAdd(fd->view, "My Computer", "", IVK_COMPUTER, NULL);
        IconViewLoadFolder(fd->view, desk, fd->only);
    } else if (!*fd->path) {
        IconViewLoadDrives(fd->view);
    } else {
        IconViewLoadFolder(fd->view, fd->path, fd->only);
    }
    IconViewFinish(fd->view);
}

/* A drive with removable disks answers only with one in it. */
static BOOL Reachable(LPCSTR path)
{
    char spec[8];
    DOSENTRY e;
    DOSFIND f;
    WORD error;

    if (!PathIsFolder(path))
        return FALSE;
    if (GetDriveType((path[0] | 0x20) - 'a') != DRIVE_REMOVABLE)
        return TRUE;
    wsprintf(spec, "%c:\\*.*", path[0]);
    if (DosFindFirst(spec, &f, &e)) {
        DosFindClose(&f);
        return TRUE;
    }
    error = DosLastError();
    return error == 2 || error == 18;  /* an empty disk */
}

static BOOL Go(HWND dlg, FD *fd, LPCSTR path)
{
    if (PathOnDisk(path) && !Reachable(path)) {
        Say(dlg, "%s is not accessible.\n\nThe device is not ready, or the folder does not exist.",
            path, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    lstrcpy(fd->path, path);
    Reload(fd, FALSE);
    PlacesFill(GetDlgItem(dlg, IDC_OPENSAVE_LOOKIN), fd->path);
    EnableTools(dlg, fd);
    return TRUE;
}

/* The folder above, the one come from selected. */
static void Up(HWND dlg, FD *fd)
{
    char dir[MAX_PATH], from[MAX_PATH];
    int i;

    if (!lstrcmp(fd->path, PLACE_DESKTOP))
        return;
    DesktopFolder(dir);
    lstrcpy(from, fd->path);
    if (!*fd->path || !lstrcmpi(fd->path, dir))
        lstrcpy(dir, PLACE_DESKTOP);
    else if (lstrlen(fd->path) <= 3)
        *dir = '\0';
    else {
        lstrcpy(dir, fd->path);
        PathToParent(dir);
    }
    if (Go(dlg, fd, dir) && lstrlen(from) > 3 && (i = IconViewFind(fd->view, PathFileName(from))) >= 0)
        IconViewSelect(fd->view, i);
}

/* The folder relative names are in: the one shown, the Desktop folder's
   for the Desktop; none for My Computer. */
static BOOL Base(FD *fd, LPSTR out)
{
    if (!lstrcmp(fd->path, PLACE_DESKTOP))
        DesktopFolder(out);
    else
        lstrcpy(out, fd->path);
    return *out != '\0';
}

/* ".", ".." and doubled backslashes out; no backslash at the end but a root's. */
static void Tidy(LPSTR path)
{
    char out[MAX_PATH];
    LPSTR p = path + 3, q = out + 3, start;

    lstrcpyn(out, path, 4);
    while (*p) {
        for (start = p; *p && *p != '\\'; p++)
            ;
        if (p - start == 2 && start[0] == '.' && start[1] == '.') {
            while (q > out + 3 && *--q != '\\')
                ;
        } else if (p > start && !(p - start == 1 && *start == '.')) {
            if (q > out + 3)
                *q++ = '\\';
            while (start < p)
                *q++ = *start++;
        }
        if (*p)
            p++;
    }
    *q = '\0';
    lstrcpy(path, out);
}

/* What was typed, as a full path: absolute, from the root of the drive
   shown, or in the folder shown. */
static BOOL Resolve(FD *fd, LPCSTR text, LPSTR out)
{
    char base[MAX_PATH];

    if (text[0] && text[1] == ':')
        lstrcpy(out, text);
    else if (!Base(fd, base))
        return FALSE;
    else if (text[0] == '\\')
        wsprintf(out, "%c:%s", base[0], (LPSTR)text);
    else {
        lstrcpy(out, base);
        PathAppend(out, text);
    }
    if (lstrlen(out) < 3)
        lstrcat(out, "\\");
    Tidy(out);
    return TRUE;
}

/* What the program gets for path: its 8.3 alias, unless it wants long
   names. A new file with a long name is made, to have one. */
static BOOL ForProgram(FD *fd, LPCSTR path, LPSTR out)
{
    char dir[MAX_PATH];
    HFILE h;

    if (fd->ofn->Flags & OFN_LONGNAMES) {
        lstrcpy(out, path);
        return TRUE;
    }
    if (DosShortPath(path, out))
        return TRUE;
    lstrcpy(dir, path);
    PathToParent(dir);
    if (PathIsShortName(PathFileName((LPSTR)path)) && DosShortPath(dir, out)) {
        PathAppend(out, PathFileName((LPSTR)path));
        AnsiUpper(out);
        return TRUE;
    }
    h = DosCreate(path);
    if (h == HFILE_ERROR)
        return FALSE;
    _lclose(h);
    return DosShortPath(path, out);
}

/* Writes the answer into the program's OPENFILENAME. Too small a buffer
   gets the size it needs, as COMMDLG does, and the dialog fails. */
static void Give(HWND dlg, FD *fd, LPCSTR result)
{
    OPENFILENAME FAR *o = fd->ofn;
    LPCSTR def = o->lpstrDefExt;
    LPSTR name, ext;
    int type = (int)SendDlgItemMessage(dlg, IDC_OPENSAVE_TYPE, CB_GETCURSEL, 0, 0L);

    if (lstrlen(result) >= (int)o->nMaxFile) {
        if (o->nMaxFile >= 2)
            *(WORD FAR *)o->lpstrFile = lstrlen(result) + 1;
        EndDialog(dlg, IDCANCEL);
        return;
    }
    lstrcpy(o->lpstrFile, result);
    name = PathFileName(o->lpstrFile);
    ext = PathExtension(name);
    o->nFileOffset = (UINT)(name - o->lpstrFile);
    o->nFileExtension = (UINT)((*ext ? ext + 1 : ext) - o->lpstrFile);
    if (o->lpstrFileTitle && o->nMaxFileTitle)
        lstrcpyn(o->lpstrFileTitle, name, (int)o->nMaxFileTitle);
    o->Flags &= ~(OFN_READONLY | OFN_EXTENSIONDIFFERENT);
    if (IsDlgButtonChecked(dlg, IDC_OPENSAVE_READONLY))
        o->Flags |= OFN_READONLY;
    if (def && *def && lstrcmpi(*ext ? ext + 1 : ext, def))
        o->Flags |= OFN_EXTENSIONDIFFERENT;
    if (type >= 0)
        o->nFilterIndex = type + 1 - (o->lpstrCustomFilter && *o->lpstrCustomFilter);
    EndDialog(dlg, IDOK);
}

/* Several files chosen: "C:\DIR FILE1.TXT FILE2.TXT", in 8.3 as in Windows 3.1. */
static void GiveMany(HWND dlg, FD *fd)
{
    char result[512], path[MAX_PATH], alias[MAX_PATH];
    int i = -1;

    DosShortPath(IconViewFolder(fd->view), result);
    while ((i = IconViewNextSelected(fd->view, i)) >= 0)
        if (IconViewItemKind(fd->view, i) == IVK_FILE && IconViewItemPath(fd->view, i, path) && DosShortPath(path, alias) &&
            lstrlen(result) + lstrlen(PathFileName(alias)) + 2 < (int)sizeof result) {
            lstrcat(result, " ");
            lstrcat(result, PathFileName(alias));
        }
    Give(dlg, fd, result);
}

/* The followed target of a shortcut, as Windows 98's dialog does. */
static void Follow(FD *fd, LPSTR path)
{
    if (!(fd->ofn->Flags & OFN_NODEREFERENCELINKS) && !lstrcmpi(PathExtension(path), ".SHC") &&
        ShortcutRead(path, &fd->shc) && *fd->shc.target)
        lstrcpy(path, fd->shc.target);
}

static void Opened(HWND dlg, FD *fd);

/* What File name says, its quotes off; FALSE when that was all: nothing
   typed (the item chosen in the view opens), several files chosen, or
   patterns to list by. */
static BOOL Typed(HWND dlg, FD *fd, LPSTR text)
{
    LPSTR p;

    GetDlgItemText(dlg, IDC_OPENSAVE_NAME, text, MAX_PATH);
    if (!*text) {
        Opened(dlg, fd);
        return FALSE;
    }
    if ((fd->ofn->Flags & OFN_ALLOWMULTISELECT) && !lstrcmp(text, fd->put) && IconViewSelectedCount(fd->view) > 1) {
        GiveMany(dlg, fd);
        return FALSE;
    }
    if (*text == '"') {
        _fmemmove(text, text + 1, lstrlen(text));
        if ((p = _fstrchr(text, '"')) != NULL)
            *p = '\0';
    }
    if (!_fstrpbrk(text, "*?"))
        return TRUE;
    lstrcpyn(fd->only, text, sizeof fd->only);
    Reload(fd, FALSE);
    return FALSE;
}

/* A new name gets the default extension; then what the program asked to
   be checked or asked. FALSE: not this file. */
static BOOL Allowed(HWND dlg, FD *fd, LPCSTR text, LPSTR path)
{
    OPENFILENAME FAR *o = fd->ofn;
    char dir[MAX_PATH];
    BOOL exists = DosGetAttributes(path) != 0xFFFF;

    if (!exists && o->lpstrDefExt && *o->lpstrDefExt && !*PathExtension(PathFileName(path)) &&
        lstrlen(path) + lstrlen(o->lpstrDefExt) + 1 < MAX_PATH) {
        lstrcat(path, ".");
        lstrcat(path, o->lpstrDefExt);
        exists = DosGetAttributes(path) != 0xFFFF;
    }
    lstrcpy(dir, path);
    PathToParent(dir);
    if (!PathIsFolder(dir)) {
        Say(dlg, "%s\nPath does not exist.\nPlease verify the correct path was given.", text, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    if (!exists && !fd->req->save && (o->Flags & OFN_FILEMUSTEXIST)) {
        Say(dlg, "%s\nFile not found.\nPlease verify the correct file name was given.", text, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    if (exists && fd->req->save && (o->Flags & OFN_OVERWRITEPROMPT))
        return Say(dlg, "%s already exists.\nDo you want to replace it?", path, MB_YESNO | MB_ICONEXCLAMATION) == IDYES;
    if (!exists && (o->Flags & OFN_CREATEPROMPT))
        return Say(dlg, "%s\nThis file does not exist.\nDo you want to create the file?", path, MB_YESNO | MB_ICONEXCLAMATION) == IDYES;
    return TRUE;
}

/* Open or Save: a name, a folder to go into, or patterns to list by. */
static void Ok(HWND dlg, FD *fd)
{
    char text[MAX_PATH], path[MAX_PATH], result[MAX_PATH];

    if (!Typed(dlg, fd, text) || !Resolve(fd, text, path))
        return;
    Follow(fd, path);
    if (PathIsFolder(path)) {
        if (Go(dlg, fd, path))
            SetDlgItemText(dlg, IDC_OPENSAVE_NAME, "");
        return;
    }
    if (!Allowed(dlg, fd, text, path))
        return;
    if (!ForProgram(fd, path, result)) {
        Say(dlg, "%s\nThe file name is not valid.", text, MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    RecentAdd(path);                    /* Start > Documents, whichever program */
    Give(dlg, fd, result);
}

/* The file (or files) chosen in the view go into File name. */
static void Picked(HWND dlg, FD *fd)
{
    char path[MAX_PATH], text[MAX_PATH];
    int i = -1, n = 0;

    *text = '\0';
    while ((i = IconViewNextSelected(fd->view, i)) >= 0)
        if (IconViewItemKind(fd->view, i) == IVK_FILE && IconViewItemPath(fd->view, i, path)) {
            if (n++ && !(fd->ofn->Flags & OFN_ALLOWMULTISELECT))
                return;
            if (lstrlen(text) + lstrlen(PathFileName(path)) + 4 >= (int)sizeof text)
                break;
            if (n > 1 || IconViewSelectedCount(fd->view) > 1)
                wsprintf(text + lstrlen(text), "%s\"%s\"", (LPSTR)(n > 1 ? " " : ""), (LPSTR)PathFileName(path));
            else
                lstrcpy(text, PathFileName(path));
        }
    if (n) {
        SetDlgItemText(dlg, IDC_OPENSAVE_NAME, text);
        lstrcpy(fd->put, text);
    }
}

static void Opened(HWND dlg, FD *fd)
{
    char path[MAX_PATH];
    int i = IconViewFocused(fd->view), k;

    if (i < 0)
        return;
    k = IconViewItemKind(fd->view, i);
    if (k == IVK_COMPUTER)
        Go(dlg, fd, "");
    else if (IconViewItemPath(fd->view, i, path)) {
        Follow(fd, path);
        if (k == IVK_DRIVE || PathIsFolder(path))
            Go(dlg, fd, path);
        else if (k == IVK_FILE) {
            Picked(dlg, fd);
            Ok(dlg, fd);
        }
    }
}

static void SetMode(HWND dlg, FD *fd, int mode)
{
    IconViewSetMode(fd->view, mode);
    SettingsSetInt(SECTION, "View", mode);
    ToolbarCheck(fd->tools, IDC_OPENSAVE_LIST, mode == IVM_LIST);
    ToolbarCheck(fd->tools, IDC_OPENSAVE_DETAILS, mode == IVM_DETAILS);
}

/* ---- the dialog ---- */

/* Where to start, and the name proposed: the program's file name may
   carry a folder or patterns; else its initial folder, else its own. */
static void Start(FD *fd, LPSTR dir, LPSTR name)
{
    OPENFILENAME FAR *o = fd->ofn;
    char file[MAX_PATH];

    *dir = *name = '\0';
    lstrcpyn(file, o->lpstrFile && o->nMaxFile ? o->lpstrFile : (LPCSTR)"", sizeof file);
    if (_fstrpbrk(file, "*?"))
        lstrcpyn(fd->only, PathFileName(file), sizeof fd->only);
    else
        lstrcpy(name, PathFileName(file));
    if (PathFileName(file) != file) {
        *PathFileName(file) = '\0';
        lstrcpy(dir, file);
    } else if (o->lpstrInitialDir && *o->lpstrInitialDir) {
        lstrcpyn(dir, o->lpstrInitialDir, MAX_PATH);
    }
    if (!*dir || !PathIsFolder(dir))
        lstrcpy(dir, fd->req->dir);
    if (lstrlen(dir) > 3 && dir[lstrlen(dir) - 1] == '\\')
        dir[lstrlen(dir) - 1] = '\0';
    DosLongPath(dir, dir);
    lstrcpy(file, dir);                 /* a file there: its long name, not the alias given */
    PathAppend(file, name);
    if (*name && DosGetAttributes(file) != 0xFFFF && DosLongPath(file, file))
        lstrcpy(name, PathFileName(file));
}

static BOOL Init(HWND dlg, FILEREQUEST FAR *req)
{
    FD *fd = LocalHeapAlloc(sizeof(FD));
    OPENFILENAME FAR *o = req->ofn;
    char dir[MAX_PATH], name[MAX_PATH];
    RECT r, w;

    if (!fd)
        return FALSE;
    SetProp(dlg, FD_PROP, (HANDLE)LocalHeapOffset(fd));
    fd->req = req;
    fd->ofn = o;
    fd->view = GetDlgItem(dlg, IDC_OPENSAVE_VIEW);
    fd->tools = GetDlgItem(dlg, IDC_OPENSAVE_TOOLBAR);
    ToolbarSetImages(fd->tools, IDB_TOOLBAR, 0, IDB_TOOLBAROFF);
    ToolbarAdd(fd->tools, TBI_UP, IDC_OPENSAVE_UP, NULL, TBS_BUTTON);
    ToolbarAdd(fd->tools, TBI_NEWFOLDER, IDC_OPENSAVE_NEWFOLDER, NULL, TBS_BUTTON);
    ToolbarAdd(fd->tools, 0, 0, NULL, TBS_SEPARATOR);
    ToolbarAdd(fd->tools, TBI_LIST, IDC_OPENSAVE_LIST, NULL, TBS_BUTTON);
    ToolbarAdd(fd->tools, TBI_DETAILS, IDC_OPENSAVE_DETAILS, NULL, TBS_BUTTON);
    SetWindowText(dlg, o->lpstrTitle && *o->lpstrTitle ? o->lpstrTitle : req->save ? "Save As" : "Open");
    if (req->save) {
        SetDlgItemText(dlg, IDC_OPENSAVE_LOOKINL, "Save &in:");
        SetDlgItemText(dlg, IDC_OPENSAVE_TYPEL, "Save as &type:");
        SetDlgItemText(dlg, IDOK, "&Save");
    }
    FillTypes(dlg, fd);
    lstrcpyn(fd->only, Patterns(dlg), sizeof fd->only);
    CheckDlgButton(dlg, IDC_OPENSAVE_READONLY, (o->Flags & OFN_READONLY) != 0);
    if (o->Flags & OFN_HIDEREADONLY)
        ShowWindow(GetDlgItem(dlg, IDC_OPENSAVE_READONLY), SW_HIDE);
    if (!(o->Flags & OFN_SHOWHELP))
        ShowWindow(GetDlgItem(dlg, IDC_OPENSAVE_HELP), SW_HIDE);
    if ((o->Flags & OFN_HIDEREADONLY) && !(o->Flags & OFN_SHOWHELP)) {
        SetRect(&r, 0, 0, 0, 14);       /* the line of both goes */
        MapDialogRect(dlg, &r);
        GetWindowRect(dlg, &w);
        SetWindowPos(dlg, NULL, 0, 0, w.right - w.left, w.bottom - w.top - r.bottom, SWP_NOMOVE | SWP_NOZORDER);
    }
    SetMode(dlg, fd, SettingsGetInt(SECTION, "View", IVM_LIST) == IVM_DETAILS ? IVM_DETAILS : IVM_LIST);
    Start(fd, dir, name);
    if (!Go(dlg, fd, dir))
        Go(dlg, fd, PLACE_DESKTOP);
    SetDlgItemText(dlg, IDC_OPENSAVE_NAME, name);
    SendDlgItemMessage(dlg, IDC_OPENSAVE_NAME, EM_SETSEL, 0, MAKELPARAM(0, -1));
    SkinDialog(dlg);
    WindowCenter(dlg);
    SetFocus(GetDlgItem(dlg, IDC_OPENSAVE_NAME));
    return TRUE;
}

static void Command(HWND dlg, FD *fd, UINT id, HWND ctl, UINT code)
{
    switch (id) {
    case IDOK:
        Ok(dlg, fd);
        break;
    case IDCANCEL:
        EndDialog(dlg, IDCANCEL);
        break;
    case IDC_OPENSAVE_HELP:                    /* COMMDLG's way: the owner is told */
        if (IsWindow(fd->ofn->hwndOwner))
            SendMessage(fd->ofn->hwndOwner, RegisterWindowMessage(HELPMSG), (WPARAM)dlg, (LPARAM)fd->ofn);
        break;
    case IDC_OPENSAVE_UP:
        Up(dlg, fd);
        SetFocus(fd->view);
        break;
    case IDC_OPENSAVE_NEWFOLDER:
        SetFocus(fd->view);
        FileCommandsRun(dlg, fd->view, CMD_NEWFOLDER);
        break;
    case IDC_OPENSAVE_LIST:
    case IDC_OPENSAVE_DETAILS:
        SetMode(dlg, fd, id == IDC_OPENSAVE_LIST ? IVM_LIST : IVM_DETAILS);
        SetFocus(fd->view);
        break;
    case IDC_OPENSAVE_LOOKIN:                  /* chosen: the list closed, or a key while closed */
        if (code == CBN_CLOSEUP || (code == CBN_SELCHANGE && !SendMessage(ctl, CB_GETDROPPEDSTATE, 0, 0L))) {
            char path[MAX_PATH];
            int i = (int)SendMessage(ctl, CB_GETCURSEL, 0, 0L);
            if (i >= 0) {
                SendMessage(ctl, CB_GETLBTEXT, i, (LPARAM)(LPSTR)path);
                if (lstrcmpi(path, fd->path))
                    PostMessage(dlg, FDM_LOOKIN, i, 0L);       /* once the list has closed */
            }
        }
        break;
    case IDC_OPENSAVE_TYPE:
        if (code == CBN_SELCHANGE) {
            lstrcpyn(fd->only, Patterns(dlg), sizeof fd->only);
            Reload(fd, FALSE);
        }
        break;
    case IDC_OPENSAVE_VIEW:
        switch (code) {
        case IVN_OPEN:    Opened(dlg, fd); break;
        case IVN_SELECT:  Picked(dlg, fd); break;
        case IVN_CONTEXT: FileMenusContext(dlg, fd->view); break;
        case IVN_DRAG:
        case IVN_RDRAG:   DragDropRun(fd->view, code == IVN_RDRAG); break;
        case IVN_RENAME:  SetWindowLong(dlg, DWL_MSGRESULT, FileCommandsRenamed(dlg, fd->view)); break;
        }
        break;
    }
}

BOOL CALLBACK __export OpenSaveDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    FD *fd = Fd(dlg);
    char path[MAX_PATH];

    switch (msg) {
    case WM_INITDIALOG:
        if (!Init(dlg, (FILEREQUEST FAR *)lp))
            EndDialog(dlg, IDCANCEL);
        return FALSE;                   /* the focus is set */
    case WM_MEASUREITEM:
        if (((MEASUREITEMSTRUCT FAR *)lp)->CtlID == IDC_OPENSAVE_LOOKIN)
            ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = PLACE_H;
        return TRUE;
    case WM_DRAWITEM:
        if (!fd)
            return FALSE;
        PlacesDraw((const DRAWITEMSTRUCT FAR *)lp);     /* Look in */
        return TRUE;
    case WM_KEYDOWN:                    /* keys the view did not take */
        if (fd && wp == VK_BACK)
            Up(dlg, fd);
        else if (fd)
            FileCommandsKey(dlg, fd->view, wp);
        return TRUE;
    case FDM_LOOKIN:
        if (fd && SendDlgItemMessage(dlg, IDC_OPENSAVE_LOOKIN, CB_GETLBTEXT, wp, (LPARAM)(LPSTR)path) != CB_ERR)
            Go(dlg, fd, path);
        return TRUE;
    case SBM_CHANGED:                   /* a folder's contents changed (NULL: all) */
        if (fd && (!lp || !lstrcmpi((LPCSTR)lp, IconViewFolder(fd->view))))
            Reload(fd, TRUE);
        return TRUE;
    case WM_COMMAND:
        if (fd)
            Command(dlg, fd, wp, (HWND)LOWORD(lp), HIWORD(lp));
        else if (wp == IDCANCEL)
            EndDialog(dlg, IDCANCEL);
        return TRUE;
    case WM_DESTROY:
        if (fd) {
            RemoveProp(dlg, FD_PROP);
            LocalHeapFree(fd);
        }
        return TRUE;
    }
    return FALSE;
}

/* SBM_FILEDLG: shown here, the answer handed back to the waiting program. */
void OpenSaveDialogRequest(FILEREQUEST FAR *req)
{
    HWND owner = req->ofn->hwndOwner;
    int r = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_OPENSAVE), IsWindow(owner) ? owner : NULL,
                           OpenSaveDialogProc, (LPARAM)req);

    if (IsTask(req->task)) {
        req->ok = r == IDOK;
        req->done = TRUE;
        PostAppMessage(req->task, WM_NULL, 0, 0L);
    }
}
