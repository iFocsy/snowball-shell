/*
 * Folder Options as in Windows 98: General, how folders browse (each in
 * the same window, or each in its own as Windows 95 did); View, every
 * folder's view like the folder it was opened from or as at first, and the
 * advanced settings that Snowball follows, a list of groups with check
 * boxes and choices (a click or Space changes one). OK or Apply keep them
 * (SNOWBALL.INI [Folders]) and show every window again. File Types is its
 * own page (filetypespage.c).
 */
#include "internal.h"

#define ROW_H       18
#define INDENT      16

enum { FO_GROUP, FO_CHECK, FO_RADIO };

static const struct { LPCSTR text; BYTE kind, depth, value; } rows[] = {
    { "Files and Folders", FO_GROUP, 0, 0 },
    { "Display the full path in title bar", FO_CHECK, 1, 0 },
    { "Hide file extensions for known file types", FO_CHECK, 1, 1 },
    { "Allow all uppercase names", FO_CHECK, 1, 2 },
    { "Remember each folder's view settings", FO_CHECK, 1, 4 },
    { "Show pop-up description for folder and desktop items", FO_CHECK, 1, 3 },
    { "Hidden files", FO_GROUP, 1, 0 },
    { "Do not show hidden or system files", FO_RADIO, 2, VIEW_NO_HIDDEN_SYSTEM },
    { "Do not show hidden files", FO_RADIO, 2, VIEW_NO_HIDDEN },
    { "Show all files", FO_RADIO, 2, VIEW_ALL },
};
#define NROWS   (sizeof rows / sizeof rows[0])

static HWND sheet;
static HWND current;                       /* the folder's view it was opened from, or NULL */
static VIEWOPTIONS edited;                 /* what the dialog shows */
static WNDPROC listProc;

static BOOL *Check(int value)
{
    return value == 0 ? &edited.fullPath : value == 1 ? &edited.hideExt : value == 2 ? &edited.allUpper
         : value == 3 ? &edited.infoTips : &edited.rememberViews;
}

static BOOL On(int i)
{
    return rows[i].kind == FO_CHECK ? *Check(rows[i].value) : edited.hidden == rows[i].value;
}

static void Toggle(int i)
{
    if (i < 0 || i >= NROWS || rows[i].kind == FO_GROUP)
        return;
    if (rows[i].kind == FO_CHECK)
        *Check(rows[i].value) = !*Check(rows[i].value);
    else
        edited.hidden = rows[i].value;
    InvalidateRect(GetDlgItem(sheet, IDC_FOLDEROPTIONS_LIST), NULL, FALSE);
    EnableWindow(GetDlgItem(sheet, IDC_APPLY), TRUE);
}

/* The list's own clicks and Space change the row as well. */
LRESULT CALLBACK __export FolderOptionsListProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res = CallWindowProc((FARPROC)listProc, w, msg, wp, lp);

    if (msg == WM_LBUTTONDOWN)
        Toggle((int)SendMessage(w, LB_GETTOPINDEX, 0, 0L) + (int)HIWORD(lp) / ROW_H);
    else if (msg == WM_KEYDOWN && wp == VK_SPACE)
        Toggle((int)SendMessage(w, LB_GETCURSEL, 0, 0L));
    return res;
}

/* Tree-like: a folder for a group, else its check box or radio button;
   the selection colour on the text only. */
static void DrawRow(const DRAWITEMSTRUCT FAR *d)
{
    RECT r = d->rcItem, t;
    int i = (int)d->itemID, x;
    BOOL sel = (d->itemState & ODS_SELECTED) != 0;
    HFONT old;

    if (i < 0 || i >= NROWS)
        return;
    DrawSolidRect(d->hDC, &r, GetSysColor(COLOR_WINDOW));
    x = r.left + 2 + rows[i].depth * INDENT;
    if (rows[i].kind == FO_GROUP)
        DrawIconImage(d->hDC, x, r.top + (ROW_H - 16) / 2, IconStock(IDI_FOLDER), TRUE);
    else
        SkinCheckBox(d->hDC, x + 1, r.top + (ROW_H - 13) / 2, rows[i].kind == FO_RADIO, On(i), FALSE);
    old = SelectObject(d->hDC, DrawFont(FALSE));
    SetRect(&t, x + 19, r.top + 1, min(r.right, x + 23 + (int)LOWORD(GetTextExtent(d->hDC, rows[i].text,
            lstrlen(rows[i].text)))), r.bottom - 1);
    if (sel)
        DrawSolidRect(d->hDC, &t, GetSysColor(COLOR_HIGHLIGHT));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    InflateRect(&t, -2, 0);
    DrawTextEllipsis(d->hDC, rows[i].text, &t, DT_LEFT | DT_NOPREFIX);
    InflateRect(&t, 2, 0);
    if (d->itemState & ODS_FOCUS)
        DrawFocusRect(d->hDC, &t);
    SelectObject(d->hDC, old);
}

/* Like Current Folder or Reset All Folders, once the user agrees. */
static void Views(HWND dlg, BOOL like)
{
    if (MessageBox(dlg, like ? "Do you want all folders to match this folder's view settings the next time you open them?"
                             : "Do you want all folders to be reset to their original view settings the next time you open them?",
                   "Folder Views", MB_YESNO | MB_ICONQUESTION) == IDYES)
        FolderViewsDefault(like && IsWindow(current) ? current : NULL);
}

static void Apply(HWND dlg)
{
    g_viewOptions = edited;
    ViewOptionsSave();
    FileCommandsFolderChanged(NULL);                 /* every window, as Folder Options now says */
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
}

BOOL CALLBACK __export FolderOptionsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND list;
    int i, k;

    switch (msg) {
    case WM_INITDIALOG:
        sheet = dlg;
        edited = g_viewOptions;
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "General");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "View");
        TabsAdd(GetDlgItem(dlg, IDC_TABS), "File Types");
        FileTypesPageInit(dlg);
        CheckRadioButton(dlg, IDC_FOLDEROPTIONS_SAME, IDC_FOLDEROPTIONS_OWN,
                         edited.ownWindows ? IDC_FOLDEROPTIONS_OWN : IDC_FOLDEROPTIONS_SAME);
        CheckRadioButton(dlg, IDC_FOLDEROPTIONS_SINGLE, IDC_FOLDEROPTIONS_DOUBLE,
                         edited.singleClick ? IDC_FOLDEROPTIONS_SINGLE : IDC_FOLDEROPTIONS_DOUBLE);
        list = GetDlgItem(dlg, IDC_FOLDEROPTIONS_LIST);
        for (i = 0; i < NROWS; i++)
            SendMessage(list, LB_ADDSTRING, 0, (LPARAM)i);
        listProc = (WNDPROC)SetWindowLong(list, GWL_WNDPROC, (LONG)(FARPROC)FolderOptionsListProc);
        SendDlgItemMessage(dlg, IDC_FOLDEROPTIONS_VIEWICON, STM_SETICON, (WPARAM)IconStock(IDI_FOLDER), 0L);
        EnableWindow(GetDlgItem(dlg, IDC_FOLDEROPTIONS_LIKE), current != NULL);
        EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
        SkinDialog(dlg);
        WindowCenter(dlg);
        TabsShowPage(dlg, 0, NULL);
        return TRUE;
    case WM_ACTIVATE:                   /* modeless: the main loop hands it its keys */
        MessageLoopTrackDialog(dlg, wp != WA_INACTIVE);
        break;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT FAR *)lp)->itemHeight = ROW_H;
        return TRUE;
    case WM_DRAWITEM:
        if (((const DRAWITEMSTRUCT FAR *)lp)->CtlID == IDC_FILETYPES_LIST)
            FileTypesPageDraw((const DRAWITEMSTRUCT FAR *)lp);
        else
            DrawRow((const DRAWITEMSTRUCT FAR *)lp);
        return TRUE;
    case WM_COMMAND:
        if (FileTypesPageCommand(dlg, wp, lp))
            return TRUE;
        switch (wp) {
        case IDC_TABS:
            if (HIWORD(lp) == TCN_SELECT)
                TabsShowPage(dlg, TabsSelected(GetDlgItem(dlg, IDC_TABS)), NULL);
            break;
        case IDC_FOLDEROPTIONS_SINGLE:
        case IDC_FOLDEROPTIONS_DOUBLE:
            edited.singleClick = wp == IDC_FOLDEROPTIONS_SINGLE;
            EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
            break;
        case IDC_FOLDEROPTIONS_SAME:
        case IDC_FOLDEROPTIONS_OWN:
            edited.ownWindows = wp == IDC_FOLDEROPTIONS_OWN;
            EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
            break;
        case IDC_FOLDEROPTIONS_LIKE:
        case IDC_FOLDEROPTIONS_RESET:
            Views(dlg, wp == IDC_FOLDEROPTIONS_LIKE);
            break;
        case IDC_FOLDEROPTIONS_DEFAULTS:            /* the View tab's */
            i = edited.ownWindows;
            k = edited.singleClick;
            _fmemset(&edited, 0, sizeof edited);
            edited.ownWindows = i;          /* (the General tab's) */
            edited.singleClick = k;
            edited.infoTips = edited.rememberViews = TRUE;
            InvalidateRect(GetDlgItem(dlg, IDC_FOLDEROPTIONS_LIST), NULL, FALSE);
            EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
            break;
        default:
            SheetButton(dlg, wp, Apply);
        }
        return TRUE;
    case WM_DESTROY:
        SetWindowLong(GetDlgItem(dlg, IDC_FOLDEROPTIONS_LIST), GWL_WNDPROC, (LONG)(FARPROC)listProc);
        MessageLoopTrackDialog(dlg, FALSE);
        FileTypesPageDone();
        sheet = NULL;
        return TRUE;
    }
    return FALSE;
}

/* view: the folder window's it was opened from (Like Current Folder), or NULL. */
void FolderOptionsShow(HWND view)
{
    current = view && !(GetWindowLong(view, GWL_STYLE) & IVS_DESKTOP) ? view : NULL;
    if (sheet)
        SetActiveWindow(sheet);
    else if (CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_FOLDEROPTS), NULL, FolderOptionsProc) && sheet)
        ShowWindow(sheet, SW_SHOWNORMAL);
}
