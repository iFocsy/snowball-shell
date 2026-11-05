/* The skin's items of a maximized document window (MDI) in its program's
   menu bar: its icon, Minimize, Restore and Close, as in Windows 98. */
#include "snsystem.h"
#include "skinint.h"

#define MDI_ITEM 0x53420000L    /* item data of those owner-drawn items, + MI_* */

enum { MI_ICON = 1, MI_MIN, MI_RESTORE, MI_CLOSE };

static HWND MdiClient(HWND frame)
{
    char cls[12];
    HWND c;

    for (c = GetWindow(frame, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        GetClassName(c, cls, sizeof cls);
        if (!lstrcmpi(cls, "MDIClient"))
            return c;
    }
    return NULL;
}

/* With a document window maximized, MDI puts Windows 3.1's two bitmaps in
   the menu bar: the window menu box first, the restore button last. Here
   they become, as in Windows 98, the document's icon and three buttons:
   Minimize and Restore are put in before the last item, which becomes
   Close (DefFrameProc hands the document these commands, as it did
   Restore). MDI no longer knows them as its own, so what it does when it
   takes its items out is done here: they go once nothing is maximized, or
   when MDI puts in fresh ones for another document, and the document they
   were for gets its window menu style back (MDI takes it while maximized).
   Nothing changes when nothing has to (this runs on repaints). */
static void MdiDone(HWND frame)
{
    HWND child = (HWND)GetProp(frame, MDIBARS);

    if (IsWindow(child) && !IsZoomed(child) && !(GetWindowLong(child, GWL_STYLE) & WS_SYSMENU)) {
        SetWindowLong(child, GWL_STYLE, GetWindowLong(child, GWL_STYLE) | WS_SYSMENU);
        SetWindowPos(child, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

void SkinMdiMenu(HWND frame)
{
    HMENU m = GetMenu(frame);
    HWND client;
    BOOL ours = GetProp(frame, MDIBARS) != NULL, changed = FALSE;
    DWORD act;
    int last;

    if (!m || (client = MdiClient(frame)) == NULL)
        return;
    act = SendMessage(client, WM_MDIGETACTIVE, 0, 0L);
    if (LOWORD(act) && HIWORD(act)) {
        /* A menu's item that opens a menu has its flags in the low byte only
           (MF_BITMAP is there, MF_OWNERDRAW is not): ours is known by its menu. */
        if ((GetMenuState(m, 0, MF_BYPOSITION) & MF_BITMAP) && GetSubMenu(m, 0)) {
            if (GetSubMenu(m, 1) && GetSubMenu(m, 1) == (HMENU)GetProp(frame, MDIMENU))
                RemoveMenu(m, 1, MF_BYPOSITION);                    /* the icon of the one before */
            SetProp(frame, MDIMENU, (HANDLE)GetSubMenu(m, 0));
            if (ours && (HWND)GetProp(frame, MDIBARS) != (HWND)LOWORD(act)) {
                MdiDone(frame);
                SetProp(frame, MDIBARS, (HANDLE)LOWORD(act));
            }
            ModifyMenu(m, 0, MF_BYPOSITION | MF_POPUP | MF_OWNERDRAW, (UINT)GetSubMenu(m, 0),
                       (LPCSTR)(MDI_ITEM | MI_ICON));
            changed = TRUE;
        }
        last = GetMenuItemCount(m) - 1;
        if (last > 0 && GetMenuItemID(m, last) == SC_RESTORE && (GetMenuState(m, last, MF_BYPOSITION) & MF_BITMAP)) {
            if (ours)
                DeleteMenu(m, SC_CLOSE, MF_BYCOMMAND);      /* the Close of the one before */
            else {
                InsertMenu(m, last, MF_BYPOSITION | MF_OWNERDRAW | MF_HELP, SC_MINIMIZE, (LPCSTR)(MDI_ITEM | MI_MIN));
                InsertMenu(m, last + 1, MF_BYPOSITION | MF_OWNERDRAW, SC_RESTORE, (LPCSTR)(MDI_ITEM | MI_RESTORE));
                SetProp(frame, MDIBARS, (HANDLE)LOWORD(act));
            }
            last = GetMenuItemCount(m) - 1;
            ModifyMenu(m, last, MF_BYPOSITION | MF_OWNERDRAW, SC_CLOSE, (LPCSTR)(MDI_ITEM | MI_CLOSE));
            changed = TRUE;
        }
    } else if (ours) {
        if (GetSubMenu(m, 0) && GetSubMenu(m, 0) == (HMENU)GetProp(frame, MDIMENU))
            RemoveMenu(m, 0, MF_BYPOSITION);            /* the document's window menu: not ours to destroy */
        RemoveProp(frame, MDIMENU);
        DeleteMenu(m, SC_MINIMIZE, MF_BYCOMMAND);
        DeleteMenu(m, SC_RESTORE, MF_BYCOMMAND);
        DeleteMenu(m, SC_CLOSE, MF_BYCOMMAND);
        MdiDone(frame);
        RemoveProp(frame, MDIBARS);
        changed = TRUE;
    }
    if (changed)
        DrawMenuBar(frame);
}

/* The four items: the document's icon, or a button of caption buttons' look. */
BOOL SkinMdiItem(HWND frame, UINT msg, LPARAM lp)
{
    MEASUREITEMSTRUCT FAR *mi = (MEASUREITEMSTRUCT FAR *)lp;
    const DRAWITEMSTRUCT FAR *d = (const DRAWITEMSTRUCT FAR *)lp;
    HWND child;
    HICON icon;
    RECT r;
    int which;

    if (msg == WM_MEASUREITEM) {
        if (mi->CtlType != ODT_MENU || HIWORD(mi->itemData) != HIWORD(MDI_ITEM))
            return FALSE;
        mi->itemWidth = LOWORD(mi->itemData) == MI_ICON ? 16 : 14;
        mi->itemHeight = GetSystemMetrics(SM_CYMENU) - 2;
        return TRUE;
    }
    if (d->CtlType != ODT_MENU || HIWORD(d->itemData) != HIWORD(MDI_ITEM))
        return FALSE;
    r = d->rcItem;
    DrawSolidRect(d->hDC, &r, GetSysColor(COLOR_MENU));
    which = LOWORD(d->itemData);
    if (which == MI_ICON) {
        child = (HWND)LOWORD(SendMessage(MdiClient(frame), WM_MDIGETACTIVE, 0, 0L));
        icon = child ? (HICON)GetClassWord(child, GCW_HICON) : NULL;
        if (!icon)                      /* as Windows 98 shows such documents */
            icon = LoadIcon(NULL, IDI_APPLICATION);
        DrawIconImage(d->hDC, r.left + (r.right - r.left - 16) / 2, r.top + (r.bottom - r.top - 16) / 2, icon, TRUE);
    } else {
        SetRect(&r, r.left, r.top + (r.bottom - r.top - 14) / 2, r.left + 16, r.top + (r.bottom - r.top - 14) / 2 + 14);
        SkinDrawButton(d->hDC, &r, which == MI_MIN ? SKIN_MIN : which == MI_CLOSE ? SKIN_CLOSE : SKIN_RESTORE,
                      (d->itemState & ODS_SELECTED) != 0, TRUE);
    }
    return TRUE;
}

