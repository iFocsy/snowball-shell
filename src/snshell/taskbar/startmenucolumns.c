/* The two-column Start menu (Taskbar Properties' choice), as Windows XP's
   in its classic look: a band in the active title bar's colours across the
   top (the banner's text) and the bottom (Shut Down); on the left, in the
   window colours, the programs pinned to it and the most used (frequent.c)
   and All Programs; on the right the shell's places and commands. How
   many most used it lists (its height) and how wide it is are the user's
   (Customize Start Menu). startmenu.c measures and draws the items, the
   bands' slices here. */
#include "internal.h"
#include "startmenuint.h"

#define MOST_USED   8                   /* most used programs shown, room allowing */
#define HEAD_H      42
#define FOOT_H      32
#define LEFT_W      190                 /* the panes' least widths, as they start */
#define RIGHT_W     150

static int paneW[2];                    /* the widest item of the left pane and the right, as measured */
static int leastW[2];                   /* the panes' least widths */

int StartMenuColumnsPrograms(void)
{
    return max(0, min(SettingsGetInt("Taskbar", "MostUsed", MOST_USED), FREQUENT_MAX));
}

int StartMenuColumnsPaneWidth(void)
{
    return max(PANE_W_MIN, min(SettingsGetInt("Taskbar", "MenuWidth", LEFT_W), PANE_W_MAX));
}

static ENTRY far *Left(HMENU root, LPCSTR label, LPCSTR path)
{
    ENTRY far *e = StartMenuAdd(root, label, path, TRUE);

    if (e)
        e->pane = 1;
    return e;
}

static ENTRY far *Line(HMENU root, BOOL left)
{
    ENTRY far *e = StartMenuAdd(root, "", NULL, TRUE);

    if (e) {
        e->separator = 1;
        e->pane = (BYTE)left;
    }
    return e;
}

static ENTRY far *Gap(HMENU root, BOOL left)
{
    ENTRY far *e = Line(root, left);

    if (e) {
        e->separator = 0;
        e->spacer = e->disabled = 1;
    }
    return e;
}

/* A pane's slice of the band at the top or the bottom: an item if labelled. */
static ENTRY far *Band(HMENU root, BOOL left, int band, LPCSTR label)
{
    ENTRY far *e = StartMenuAdd(root, label, NULL, TRUE);

    if (e) {
        e->pane = (BYTE)left;
        e->band = (BYTE)band;
        e->disabled = !*label;
        e->height = band == BAND_HEAD ? HEAD_H : FOOT_H;
    }
    return e;
}

/* Pinned or most used items, each once, while they fit in room (pixels);
   after a line if line. How many. */
static int Listed(HMENU root, PATHLIST *l, PATHLIST *shown, BOOL bold, BOOL line, int *room)
{
    char path[MAX_PATH], name[MAX_PATH], s[MAX_PATH];
    ENTRY far *e;
    int i, j, n = 0;

    for (i = 0; PathListAt(l, i, path); i++) {
        for (j = 0; PathListAt(shown, j, s) && lstrcmpi(s, path); j++)
            ;
        if (PathListAt(shown, j, s))
            continue;
        DisplayNameOfFile(PathFileName(path), name);
        if (*PathExtension(name))           /* "Notepad", as Windows XP shows it */
            *PathExtension(name) = '\0';
        if (line && !n)
            Line(root, TRUE);
        if ((e = Left(root, name, path)) == NULL)
            break;
        e->fixed = 1;
        e->bold = (BYTE)bold;
        if ((*room -= StartMenuItemHeight(e)) < 0) {
            StartMenuCount -= line && !n ? 2 : 1;   /* no room: left out */
            break;
        }
        PathListAdd(shown, path);
        n++;
    }
    return n;
}

/* Built whole, then put in: the shorter column gets the room that ends
   both together (All Programs and Shut Down at the bottom). */
HMENU StartMenuColumns(void)
{
    HMENU root = CreatePopupMenu();
    char dir[MAX_PATH], path[MAX_PATH];
    PATHLIST pins, used, shown;
    ENTRY far *e, far *gapLeft, far *gapRight;
    int first = StartMenuCount, left = 0, right = 0, room, i;
    RECT work;

    paneW[0] = leastW[0] = StartMenuColumnsPaneWidth();
    paneW[1] = leastW[1] = MulDiv(leastW[0], RIGHT_W, LEFT_W);
    PathListInit(&pins);
    PathListInit(&used);
    PathListInit(&shown);
    FrequentPinned(&pins);
    FrequentMostUsed(&used, StartMenuColumnsPrograms());
    TaskbarWorkArea(&work);
    room = work.bottom - work.top - 8 - HEAD_H - FOOT_H - 3 * MENUITEM_H;   /* All Programs, the lines */
    Band(root, TRUE, BAND_HEAD, "");
    i = Listed(root, &pins, &shown, TRUE, FALSE, &room);
    GetWindowsDirectory(path, sizeof path);         /* MS-DOS Prompt is on the right */
    PathAppend(path, "DOSPRMPT.PIF");
    PathListAdd(&shown, path);
    Listed(root, &used, &shown, FALSE, i > 0, &room);
    gapLeft = Gap(root, TRUE);
    Line(root, TRUE);
    StartMenuFolder(dir);
    lstrcpy(path, dir);
    PathAppend(path, "Programs");
    if ((e = Left(root, "All &Programs", path)) != NULL) {
        e->sub = CreatePopupMenu();
        e->mnemonic = e->bold = e->center = 1;
    }
    Band(root, TRUE, BAND_FOOT, "");
    if ((e = Band(root, FALSE, BAND_HEAD, "")) != NULL)
        e->column = 1;
    if ((e = StartMenuRootItem(root, "My &Documents", DesktopMyDocuments(), IDI_MYDOCS)) != NULL)
        e->bold = 1;
    if ((e = StartMenuRootItem(root, "My &Computer", NULL, IDI_COMPUTER)) != NULL) {
        e->special = SPECIAL_COMPUTER;
        e->bold = 1;
    }
    Line(root, FALSE);
    if ((e = StartMenuRootItem(root, "R&ecent Documents", RecentFolder(dir), IDI_SM_DOCS)) != NULL)
        e->sub = CreatePopupMenu();
    StartMenuSettingsItem(root);
    StartMenuFindItem(root);
    Line(root, FALSE);
    if ((e = StartMenuProgram(root, "MS-DOS P&rompt", "DOSPRMPT.PIF")) != NULL)
        e->root = 1;
    if ((e = StartMenuRootItem(root, "&Run...", NULL, IDI_SM_RUN)) != NULL)
        e->special = SPECIAL_RUN;
    gapRight = Gap(root, FALSE);
    if ((e = Band(root, FALSE, BAND_FOOT, "Sh&ut Down...")) != NULL) {
        e->icon = IconStock(IDI_SM_SHUTDOWN);
        e->mnemonic = 1;
        e->special = SPECIAL_SHUTDOWN;
    }
    for (i = first; i < StartMenuCount; i++)
        if (StartMenuEntries[i]->parent == root) {
            if (StartMenuEntries[i]->pane)
                left += StartMenuItemHeight(StartMenuEntries[i]);
            else
                right += StartMenuItemHeight(StartMenuEntries[i]);
        }
    if (gapLeft && gapRight) {
        gapLeft->height = max(0, right - left);
        gapRight->height = max(0, left - right);
    }
    for (i = first; i < StartMenuCount; i++)
        if (StartMenuEntries[i]->parent == root && !(StartMenuEntries[i]->spacer && !StartMenuEntries[i]->height))
            StartMenuAppend(StartMenuEntries[i]);
    PathListFree(&pins);
    PathListFree(&used);
    PathListFree(&shown);
    return root;
}

/* An item's width, the widest of each pane kept (the bands' slices meet). */
int StartMenuColumnsWidth(ENTRY far *e, int w)
{
    int pane = e->pane ? 0 : 1;

    w = max(w, leastW[pane]);
    paneW[pane] = max(paneW[pane], w);
    return w;
}

/* The band's colour at x of a band w wide: the active title bar's, Color 2 at the right. */
static COLORREF Along(int x, int w)
{
    COLORREF a = GetSysColor(COLOR_ACTIVECAPTION), b = SkinColor(SC_GRADIENT_ACTIVE);

    x = max(0, min(x, w));
    return RGB(GetRValue(a) + (int)(((long)GetRValue(b) - GetRValue(a)) * x / w),
               GetGValue(a) + (int)(((long)GetGValue(b) - GetGValue(a)) * x / w),
               GetBValue(a) + (int)(((long)GetBValue(b) - GetBValue(a)) * x / w));
}

/* Windows leaves a pixel between the columns: the bands go on over it. */
void StartMenuColumnsGap(HDC dc, ENTRY far *e, const RECT *r)
{
    RECT g;

    if (!e->band || e->pane)
        return;
    SetRect(&g, r->left - 1, r->top, r->left, r->bottom);
    DrawSolidRect(dc, &g, DrawScreenDepth() <= 8 ? GetSysColor(COLOR_ACTIVECAPTION) : Along(g.left, r->right));
}

/* A slice of a band, r in the menu's client area: the left one of the top
   band has the banner's text, the right one of the bottom band Shut Down,
   raised while selected. Windows lays each pane out a little wider than
   measured, the same for both: the left one guesses the right one's. */
void StartMenuColumnsBand(HDC dc, ENTRY far *e, const RECT *r, BOOL sel)
{
    int w = e->pane ? r->right + paneW[1] + (r->right - r->left - paneW[0]) : r->right, x;
    HFONT old;
    RECT b;

    if (DrawScreenDepth() <= 8)         /* no gradients: the title's colour */
        DrawSolidRect(dc, r, GetSysColor(COLOR_ACTIVECAPTION));
    else
        DrawGradient(dc, r, Along(r->left, w), Along(r->right, w), FALSE);
    if (e->band == BAND_HEAD && e->pane) {
        DrawIconImage(dc, r->left + 6, r->top + (r->bottom - r->top - 32) / 2, IconStock(IDI_SNOWBALL), FALSE);
        StartMenuBannerText(dc, r->left + 46, r->top + (r->bottom - r->top - 17) / 2, FALSE);
    }
    if (e->special != SPECIAL_SHUTDOWN)
        return;
    old = SelectObject(dc, DrawFont(FALSE));
    x = 25 + LOWORD(GetTextExtent(dc, e->label, lstrlen(e->label))) - LOWORD(GetTextExtent(dc, "&", 1)) + 8;
    SetRect(&b, r->right - 8 - x, r->top + 4, r->right - 8, r->bottom - 4);
    if (sel)
        DrawFrame(dc, &b, GetSysColor(COLOR_BTNHIGHLIGHT), SkinColor(SC_DKSHADOW));
    DrawIconImage(dc, b.left + 4, b.top + (b.bottom - b.top - 16) / 2, e->icon, TRUE);
    b.left += 25;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_CAPTIONTEXT));
    DrawText(dc, e->label, -1, &b, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(dc, old);
}
