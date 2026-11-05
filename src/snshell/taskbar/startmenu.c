/*
 * The Start menu: native popup menus with owner-drawn items. Root items are
 * large with the banner on their left; folders become submenus that are
 * filled when they first open. Or two columns (Taskbar Properties,
 * startmenucolumns.c): pinned and most used programs (frequent.c) and All
 * Programs, the shell's places and commands on the right. Everything for one opening lives in one
 * arena, reset the next time the menu opens. Items of the Start Menu
 * folder have their own menu on a right click, and are dragged to another
 * place or submenu. Windows 3.1's menus know neither: a message filter
 * takes the mouse, ends the menu, shows the item's menu or moves the item,
 * then opens the Start menu again where it was.
 */
#include "internal.h"
#include "startmenuint.h"

#define ROOT_H      36
#define SEP_H       8
#define ARENA_SIZE  32000
#define CMD_BASE    0x1000


static HGLOBAL arena;
static BYTE far *heap;
static WORD used;
ENTRY far *StartMenuEntries[MAX_ENTRIES];
int StartMenuCount;
static HDC bannerDC;
static HBITMAP bannerBmp, bannerOld;
HWND StartMenuOwner;
static BOOL smallRoot;                   /* Taskbar Properties: small icons in the Start menu, */
static BOOL twoColumns;                  /* ... or two columns */
WORD StartMenuPaints;
OUTCOME StartMenuOut;

static void far *Alloc(WORD n)
{
    void far *p;
    n = (n + 1) & ~1;
    if (used + n > ARENA_SIZE)
        return NULL;
    p = heap + used;
    used += n;
    return p;
}

static LPSTR Str(LPCSTR s)
{
    LPSTR p = Alloc(lstrlen(s) + 1);
    if (p)
        lstrcpy(p, s);
    return p;
}

ENTRY far *StartMenuAdd(HMENU m, LPCSTR label, LPCSTR path, BOOL root)
{
    ENTRY far *e;

    if (StartMenuCount == MAX_ENTRIES || (e = Alloc(sizeof(ENTRY))) == NULL)
        return NULL;
    _fmemset(e, 0, sizeof *e);
    e->label = Str(label);
    e->path = path ? Str(path) : NULL;
    e->parent = m;
    e->root = (BYTE)root;
    StartMenuEntries[StartMenuCount++] = e;
    return e;
}


/* How far down its column a submenu has got before e. */
static int ColumnHeight(ENTRY far *e)
{
    int y = 0, i;

    for (i = 0; i < StartMenuCount && StartMenuEntries[i] != e; i++)
        if (StartMenuEntries[i]->parent == e->parent)
            y = (StartMenuEntries[i]->column ? 0 : y) + StartMenuItemHeight(StartMenuEntries[i]);
    return y;
}

void StartMenuAppend(ENTRY far *e)
{
    UINT flags = MF_OWNERDRAW;
    UINT id = CMD_BASE;
    int i, y;
    RECT work;

    for (i = 0; i < StartMenuCount && StartMenuEntries[i] != e; i++)
        ;
    id += i;
    /* a submenu taller than the screen (less the taskbar) goes on in
       another column, as Windows 95's do (Windows 3.1's run off the screen) */
    y = ColumnHeight(e);
    TaskbarWorkArea(&work);
    if (!e->root && y && y + StartMenuItemHeight(e) > work.bottom - work.top - 8) {
        e->column = 1;
        flags |= MF_MENUBREAK;
    }
    if (e->bar)
        flags |= MF_MENUBARBREAK;
    else if (e->column)
        flags |= MF_MENUBREAK;
    if (e->sub) {
        flags |= MF_POPUP;
        id = (UINT)e->sub;
    }
    if (e->separator || e->disabled)
        flags |= MF_DISABLED;
    e->pos = (BYTE)GetMenuItemCount(e->parent);
    AppendMenu(e->parent, flags, id, (LPCSTR)e);
}

static void Separator(HMENU m, BOOL root)
{
    ENTRY far *e = StartMenuAdd(m, "", NULL, root);
    if (e) {
        e->separator = 1;
        StartMenuAppend(e);
    }
}

static int CompareEntries(const void far *pa, const void far *pb)
{
    ENTRY far *a = *(ENTRY far * far *)pa, far *b = *(ENTRY far * far *)pb;
    WORD placeA = a->place ? a->place : 0xFFFF, placeB = b->place ? b->place : 0xFFFF;

    if (placeA != placeB)               /* dragged into place first */
        return placeA < placeB ? -1 : 1;
    if ((a->sub != NULL) != (b->sub != NULL))
        return a->sub ? -1 : 1;
    return lstrcmpi(a->label, b->label);
}

/* Adds the contents of dir to m: as dragged, then folders as submenus,
   then everything else. */
static int AddFolder(HMENU m, LPCSTR dir, BOOL root, LPCSTR skip)
{
    char spec[MAX_PATH], path[MAX_PATH], name[MAX_PATH];
    DOSENTRY f;
    DOSFIND find;
    int first = StartMenuCount, i;

    lstrcpy(spec, dir);
    PathAppend(spec, "*.*");
    if (DosFindFirst(spec, &find, &f)) {
        do {
            ENTRY far *e;
            if ((f.attr & (FA_HIDDEN | FA_SYSTEM)) || (skip && !lstrcmpi(f.name, skip)))
                continue;
            lstrcpy(path, dir);
            PathAppend(path, f.name);
            DisplayNameOfFile(f.name, name);
            if ((e = StartMenuAdd(m, name, path, root)) == NULL)
                break;
            e->place = StartMenuOrderPlace(path);
            if (f.attr & FA_DIR)
                e->sub = CreatePopupMenu();
        } while (DosFindNext(&find, &f));
        DosFindClose(&find);
    }
    SortRecords(StartMenuEntries + first, StartMenuCount - first, sizeof(ENTRY far *), CompareEntries);
    for (i = first; i < StartMenuCount; i++)
        StartMenuAppend(StartMenuEntries[i]);
    return StartMenuCount - first;
}

static void FillSubmenu(ENTRY far *e)
{
    char recent[MAX_PATH];
    ENTRY far *docs;

    e->filled = 1;
    if (e->root && !lstrcmpi(e->path, RecentFolder(recent)) &&     /* Documents: My Documents first */
        (docs = StartMenuAdd(e->sub, "My Documents", DesktopMyDocuments(), FALSE)) != NULL) {
        docs->icon = IconStock(IDI_MYDOCS);
        StartMenuAppend(docs);
        Separator(e->sub, FALSE);
    }
    if (AddFolder(e->sub, e->path, FALSE, NULL) == 0) {
        ENTRY far *empty = StartMenuAdd(e->sub, "(Empty)", NULL, FALSE);
        if (empty) {
            empty->disabled = empty->empty = 1;
            StartMenuAppend(empty);
        }
    }
}

/* A file of that name in the Windows folder. */
static BOOL WindowsHas(LPCSTR file)
{
    char path[MAX_PATH];

    GetWindowsDirectory(path, sizeof path);
    PathAppend(path, file);
    return DosGetAttributes(path) != 0xFFFF;
}

ENTRY far *StartMenuProgram(HMENU m, LPCSTR label, LPCSTR exe)
{
    char path[MAX_PATH];
    ENTRY far *e;

    GetWindowsDirectory(path, sizeof path);
    PathAppend(path, exe);
    if ((e = StartMenuAdd(m, label, path, FALSE)) != NULL)
        e->mnemonic = 1;
    return e;
}

ENTRY far *StartMenuRootItem(HMENU m, LPCSTR label, LPCSTR path, int icon)
{
    ENTRY far *e = StartMenuAdd(m, label, path, TRUE);
    if (e) {
        e->icon = IconStock(icon);
        e->mnemonic = 1;
    }
    return e;
}

/* An item that opens one of the shell's own dialogs. */
static void Special(HMENU m, LPCSTR label, int icon, int special)
{
    ENTRY far *e = StartMenuAdd(m, label, NULL, FALSE);

    if (e) {
        e->icon = IconStock(icon);
        e->mnemonic = 1;
        e->special = (BYTE)special;
        StartMenuAppend(e);
    }
}

/* Settings and Find, with their items. */
ENTRY far *StartMenuSettingsItem(HMENU root)
{
    ENTRY far *e = StartMenuRootItem(root, "&Settings", NULL, IDI_SM_SETTINGS), far *i;
    HMENU m;

    if (!e)
        return NULL;
    e->sub = m = CreatePopupMenu();
    e->filled = 1;
    if ((i = StartMenuProgram(m, "&Control Panel", "CONTROL.EXE")) != NULL) StartMenuAppend(i);
    if ((i = StartMenuProgram(m, "&Printers", "PRINTMAN.EXE")) != NULL) StartMenuAppend(i);
    /* WINSETUP.EXE: Setup inside Windows (SETUP.EXE is the DOS one: no use with Windows running) */
    if (WindowsHas("WINSETUP.EXE") && (i = StartMenuProgram(m, "Windows &Setup", "WINSETUP.EXE")) != NULL) StartMenuAppend(i);
    Special(m, "&Taskbar && Start Menu...", IDI_SNOWBALL, SPECIAL_TASKBAR);
    Special(m, "&Folder Options...", IDI_FOLDER, SPECIAL_FOLDEROPTS);
    Special(m, "T&heme Manager...", IDI_DISPLAY, SPECIAL_THEMES);
    return e;
}

ENTRY far *StartMenuFindItem(HMENU root)
{
    ENTRY far *e = StartMenuRootItem(root, "&Find", NULL, IDI_SM_FIND);

    if (e) {
        e->sub = CreatePopupMenu();
        e->filled = 1;
        Special(e->sub, "&Files or Folders...", IDI_SM_FIND, SPECIAL_FIND);
    }
    return e;
}

static HMENU BuildRoot(void)
{
    HMENU root = CreatePopupMenu();
    char dir[MAX_PATH], programs[MAX_PATH];
    ENTRY far *e;

    StartMenuFolder(dir);
    if (AddFolder(root, dir, TRUE, "Programs"))
        Separator(root, TRUE);

    lstrcpy(programs, dir);
    PathAppend(programs, "Programs");
    if ((e = StartMenuRootItem(root, "&Programs", programs, IDI_SM_PROGRAMS)) != NULL) {
        e->sub = CreatePopupMenu();
        StartMenuAppend(e);
    }
    if ((e = StartMenuRootItem(root, "&Documents", RecentFolder(dir), IDI_SM_DOCS)) != NULL) {
        e->sub = CreatePopupMenu();     /* filled when it opens: "(Empty)" if nothing */
        StartMenuAppend(e);
    }
    if ((e = StartMenuSettingsItem(root)) != NULL)
        StartMenuAppend(e);
    if ((e = StartMenuFindItem(root)) != NULL)
        StartMenuAppend(e);
    if ((e = StartMenuRootItem(root, "&Run...", NULL, IDI_SM_RUN)) != NULL) {
        e->special = SPECIAL_RUN;
        StartMenuAppend(e);
    }
    Separator(root, TRUE);
    if ((e = StartMenuRootItem(root, "Sh&ut Down...", NULL, IDI_SM_SHUTDOWN)) != NULL) {
        e->special = SPECIAL_SHUTDOWN;
        StartMenuAppend(e);
    }
    return root;
}

/* Root items are large, with the banner, unless Taskbar Properties says
   small icons; with two columns the pinned and most used, no banner. */
BOOL StartMenuBig(ENTRY far *e)
{
    return !smallRoot && (twoColumns ? e->fixed : e->root);
}

BOOL StartMenuBannered(ENTRY far *e)
{
    return !twoColumns && StartMenuBig(e);
}

int StartMenuItemHeight(ENTRY far *e)
{
    return e->spacer || e->band ? e->height : e->separator ? SEP_H : StartMenuBig(e) ? ROOT_H : MENUITEM_H;
}

/* [Start] key ("1084D0"), else def: the scheme's. */
static COLORREF IniColor(LPCSTR key, COLORREF def)
{
    char hex[8];
    DWORD v = 0;
    LPSTR p;

    SettingsGetString("Start", key, "", hex, sizeof hex);
    if (!*hex)
        return def;
    for (p = hex; *p; p++)
        v = v * 16 + (*p <= '9' ? *p - '0' : ((*p | 0x20) - 'a' + 10));
    return RGB((BYTE)(v >> 16), (BYTE)(v >> 8), (BYTE)v);
}

static int Lightness(COLORREF c)        /* 0 to 255 */
{
    return (GetRValue(c) * 30 + GetGValue(c) * 59 + GetBValue(c) * 11) / 100;
}

/* The banner's gradient, top to bottom, and its text's colour: the active
   title bar's. Light text: the title's colour half as light at the top,
   Color 2 at the bottom (as Windows 98's), or the title's colour if the
   text would not show on Color 2 (with 256 colours or fewer, the title's
   alone). Dark text: the title's colours as they are. */
void StartMenuBannerColors(COLORREF *top, COLORREF *bottom, COLORREF *text)
{
    COLORREF c = GetSysColor(COLOR_ACTIVECAPTION), c2 = SkinColor(SC_GRADIENT_ACTIVE);
    int light;

    *text = GetSysColor(COLOR_CAPTIONTEXT);
    light = Lightness(*text);
    if (light >= 128 && DrawScreenDepth() <= 8) {   /* one colour (no gradients): the title's */
        *top = *bottom = c;
    } else if (light >= 128) {
        *top = RGB(GetRValue(c) / 2, GetGValue(c) / 2, GetBValue(c) / 2);
        *bottom = light - Lightness(c2) >= 112 ? c2 : c;
    } else {
        *top = c;
        *bottom = c2;
    }
    *top = IniColor("BannerTop", *top);
    *bottom = IniColor("BannerBottom", *bottom);
}

/* The banner's text from x, y, running upwards (up) or along: the first
   word bold ("Windows 3.11"), the rest a little fainter, in the title's ink. */
void StartMenuBannerText(HDC dc, int x, int y, BOOL up)
{
    char text[40];
    LPSTR rest;
    LOGFONT lf;
    HFONT bold, light, old;
    COLORREF ink = GetSysColor(COLOR_CAPTIONTEXT);
    int w;

    SettingsGetString("Start", "Banner", "Windows 3.11", text, sizeof text);
    for (rest = text; *rest && *rest != ' '; rest++)
        ;
    if (*rest)
        *rest++ = '\0';
    _fmemset(&lf, 0, sizeof lf);
    lf.lfHeight = -17;
    lf.lfEscapement = lf.lfOrientation = up ? 900 : 0;
    lf.lfWeight = FW_BOLD;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lstrcpy(lf.lfFaceName, "Arial");
    bold = CreateFontIndirect(&lf);
    lf.lfWeight = FW_NORMAL;
    light = CreateFontIndirect(&lf);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ink);
    old = SelectObject(dc, bold);
    TextOut(dc, x, y, text, lstrlen(text));
    w = LOWORD(GetTextExtent(dc, text, lstrlen(text))) + 5;
    SelectObject(dc, light);
    SetTextColor(dc, RGB(GetRValue(ink) + (128 - GetRValue(ink)) / 4, GetGValue(ink) + (128 - GetGValue(ink)) / 4,
                         GetBValue(ink) + (128 - GetBValue(ink)) / 4));       /* white: 224 */
    TextOut(dc, up ? x : x + w, up ? y - w : y, rest, lstrlen(rest));
    SelectObject(dc, old);
    DeleteObject(bold);
    DeleteObject(light);
}

/* The banner: a vertical gradient with the text running upwards. */
static void MakeBanner(int height)
{
    HDC screen = GetDC(NULL);
    COLORREF top, bottom, ink;
    RECT r;

    bannerDC = CreateCompatibleDC(screen);
    bannerBmp = CreateCompatibleBitmap(screen, BANNER_W, height);
    ReleaseDC(NULL, screen);
    bannerOld = SelectObject(bannerDC, bannerBmp);
    SetRect(&r, 0, 0, BANNER_W, height);
    StartMenuBannerColors(&top, &bottom, &ink);
    DrawGradient(bannerDC, &r, top, bottom, TRUE);
    StartMenuBannerText(bannerDC, 1, height - 6, TRUE);
}

/* Each item off the screen, then onto it at once. */
static void DrawEntry(const DRAWITEMSTRUCT FAR *d)
{
    ENTRY far *e = (ENTRY far *)d->itemData;
    DWORD org = GetDCOrg(d->hDC);
    RECT r = d->rcItem, t;
    OFFSCREEN o;
    HDC dc = DrawOffscreenBegin(&o, d->hDC, &d->rcItem);

    e->rc = r;
    OffsetRect(&e->rc, LOWORD(org), HIWORD(org));
    e->painted = ++StartMenuPaints;
    if (StartMenuBannered(e)) {
        BitBlt(dc, r.left, r.top, BANNER_W, r.bottom - r.top, bannerDC, 0, e->top, SRCCOPY);
        r.left += BANNER_W;
    }
    if (e->band) {
        StartMenuColumnsBand(dc, e, &r, (d->itemState & ODS_SELECTED) && !e->disabled);
        DrawOffscreenEnd(&o);
        StartMenuColumnsGap(d->hDC, e, &d->rcItem);
        return;
    }
    if (e->separator || e->spacer) {
        DrawSolidRect(dc, &r, GetSysColor(e->pane ? COLOR_WINDOW : COLOR_MENU));
        if (e->spacer) {
            DrawOffscreenEnd(&o);
            return;
        }
        SetRect(&t, r.left + (e->pane ? 8 : 2), r.top + SEP_H / 2 - 1, r.right - (e->pane ? 8 : 2), r.top + SEP_H / 2);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNSHADOW));
        OffsetRect(&t, 0, 1);
        DrawSolidRect(dc, &t, GetSysColor(COLOR_BTNHIGHLIGHT));
        DrawOffscreenEnd(&o);
        return;
    }
    if (!e->icon && e->path)
        e->icon = e->sub ? IconStock(IDI_FOLDER) : IconForFile(e->path, FALSE);
    MenuItemPaint(dc, &r, e->icon, e->label, (d->itemState & ODS_SELECTED ? MIP_SELECTED : 0) |
                  (e->disabled ? MIP_DISABLED : 0) | (e->mnemonic ? MIP_MNEMONIC : 0) | (StartMenuBig(e) ? MIP_LARGE : 0) |
                  (e->pane ? MIP_PANE : 0) | (e->bold ? MIP_BOLD : 0) | (e->center ? MIP_CENTER : 0));
    DrawOffscreenEnd(&o);
    StartMenuDragPaintMark(d->hDC, e, org);
}

static void MeasureEntry(MEASUREITEMSTRUCT FAR *m)
{
    ENTRY far *e = (ENTRY far *)m->itemData;
    int w = MenuItemWidth(e->label, (e->mnemonic ? MIP_MNEMONIC : 0) | (StartMenuBig(e) ? MIP_LARGE : 0) |
                          (e->bold ? MIP_BOLD : 0));

    m->itemHeight = StartMenuItemHeight(e);
    if (twoColumns && e->root)
        m->itemWidth = StartMenuColumnsWidth(e, w);
    else
        m->itemWidth = StartMenuBig(e) ? max(BANNER_W + w, 150) : w;
}

/* One opening; out says how it ended. Letting go of the mouse over the
   Start button that opened it keeps it open. */
static void Track(const RECT *button)
{
    RECT bar;
    HMENU root;
    int i, y = 0;

    heap = GlobalLock(arena);
    used = 0;
    StartMenuCount = 0;
    smallRoot = SettingsGetInt("Taskbar", "SmallIcons", 0);
    twoColumns = SettingsGetInt("Taskbar", "TwoColumns", 0);
    root = twoColumns ? StartMenuColumns() : BuildRoot();
    for (i = 0; i < StartMenuCount; i++)        /* (two columns: the left one's, as high) */
        if (StartMenuEntries[i]->parent == root && (!twoColumns || StartMenuEntries[i]->pane)) {
            StartMenuEntries[i]->top = y;
            y += StartMenuItemHeight(StartMenuEntries[i]);
        }
    if (!twoColumns)
        MakeBanner(y);
    StartMenuPaints = 0;
    StartMenuDragBegin(button);
    GetWindowRect(StartMenuOwner, &bar);
    /* the items and the menu's edges (3 pixels each side) against the
       taskbar: just above it, below it at the top of the screen, beside it
       from the Start button's top at a side (its edge grows 3 pixels right) */
    switch (TaskbarEdge()) {
    case TBE_TOP:   TrackPopupMenu(root, TPM_LEFTALIGN | TPM_LEFTBUTTON, bar.left, bar.bottom, 0, StartMenuOwner, button); break;
    case TBE_LEFT:  TrackPopupMenu(root, TPM_LEFTALIGN | TPM_LEFTBUTTON, bar.right, button->top, 0, StartMenuOwner, button); break;
    case TBE_RIGHT: TrackPopupMenu(root, TPM_RIGHTALIGN | TPM_LEFTBUTTON, bar.left - 3, button->top, 0, StartMenuOwner, button); break;
    default:        TrackPopupMenu(root, TPM_LEFTALIGN | TPM_LEFTBUTTON, bar.left, bar.top - y - 6, 0, StartMenuOwner, button);
    }
    StartMenuDragEnd();
    DestroyMenu(root);
    if (!twoColumns) {
        SelectObject(bannerDC, bannerOld);
        DeleteObject(bannerBmp);
        DeleteDC(bannerDC);
    }
    GlobalUnlock(arena);        /* StartMenuEntries stay valid until the next opening */
}

void StartMenuShow(HWND w, const RECT *button)
{
    char renamed[MAX_PATH];

    StartMenuOwner = w;
    if (!arena && (arena = GlobalAlloc(GMEM_MOVEABLE, ARENA_SIZE)) == NULL)
        return;
    for (;;) {
        Track(button);
        if (StartMenuOut.what == TRACK_ITEM && StartMenuOut.fixed) {
            FrequentMenu(w, StartMenuOut.path);
        } else if (StartMenuOut.what == TRACK_ITEM) {
            if (FileMenusItem(w, StartMenuOut.path, TRUE, renamed) == CMD_BYNAME + IVSORT_NAME) {
                PathToParent(StartMenuOut.path);
                StartMenuOrderForget(StartMenuOut.path);
            } else if (*renamed) {
                StartMenuOrderRename(StartMenuOut.path, renamed);
            }
        } else if (StartMenuOut.what == TRACK_DROP) {
            StartMenuDragMove(w);
            PathListFree(&StartMenuOut.order);
        } else {
            break;
        }
        StartMenuDragReopen();
    }
    StartMenuPicturesGone();
}

static void Command(int i)
{
    ENTRY far *e;

    if (i < 0 || i >= StartMenuCount)
        return;
    heap = GlobalLock(arena);
    e = StartMenuEntries[i];
    switch (e->special) {
    case SPECIAL_RUN:      RunDialogShow(StartMenuOwner); break;
    case SPECIAL_SHUTDOWN: ShutDownDialogShow(StartMenuOwner); break;
    case SPECIAL_FIND:     FindShow(""); break;
    case SPECIAL_TASKBAR:  TaskbarPropertiesShow(); break;
    case SPECIAL_FOLDEROPTS: FolderOptionsShow(NULL); break;
    case SPECIAL_THEMES:   ThemeManagerShow(); break;
    case SPECIAL_COMPUTER: FolderWindowOpen(""); break;
    default:               if (e->path) LaunchItem(StartMenuOwner, e->path);
    }
    GlobalUnlock(arena);
}

BOOL StartMenuMessage(HWND w, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res)
{
    int i;

    HOOK_DESCRIBE_MENU_RES(msg, res, StartMenuDescribeMenu((HMENU)wp, (LPSTR)lp));

    switch (msg) {
    case WM_MEASUREITEM:
        if (((MEASUREITEMSTRUCT FAR *)lp)->CtlType != ODT_MENU)
            return FALSE;
        MeasureEntry((MEASUREITEMSTRUCT FAR *)lp);
        *res = TRUE;
        return TRUE;
    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT FAR *)lp)->CtlType != ODT_MENU)
            return FALSE;
        DrawEntry((DRAWITEMSTRUCT FAR *)lp);
        *res = TRUE;
        return TRUE;
    case SMM_MARK:
        StartMenuDragMark();
        *res = 0;
        return TRUE;
    case WM_INITMENUPOPUP:
        for (i = 0; i < StartMenuCount; i++)
            if (StartMenuEntries[i]->sub == (HMENU)wp && !StartMenuEntries[i]->filled)
                FillSubmenu(StartMenuEntries[i]);
        *res = 0;
        return TRUE;
    case WM_MENUCHAR:
        for (i = 0; i < StartMenuCount; i++) {
            ENTRY far *e = StartMenuEntries[i];
            if (e->parent == (HMENU)HIWORD(lp) && !e->separator && !e->disabled &&
                MenuItemMatchesKey(e->label, e->mnemonic, wp)) {
                *res = MAKELONG(e->pos, 2);         /* execute */
                return TRUE;
            }
        }
        return FALSE;
    case WM_COMMAND:
        if (wp < CMD_BASE || wp >= CMD_BASE + MAX_ENTRIES || LOWORD(lp))
            return FALSE;
        Command(wp - CMD_BASE);
        *res = 0;
        return TRUE;
    }
    return FALSE;
}

void StartMenuFolder(LPSTR out)
{
    lstrcpy(out, g_home);
    lstrcat(out, DosHasLongNames() ? "Start Menu" : "STARTMNU");
}
