/*
 * SbIconView, its geometry and its painting: where each item goes in each
 * mode (its cell, the parts the mouse hits, the width of its name, the
 * scrolling) and how it is drawn: icons, labels, the Details columns and
 * header, My Computer's drives as tiles, BMP files as thumbnails.
 */
#include "internal.h"
#include "iconviewint.h"

static const char *KeyName[] = { "Name", "Type", "Size", "Modified", "In Folder" };   /* by IVS_* */

/* ---- the Details columns' text ---- */

/* The In Folder column (results of Find): where the item is. */
static void Folder(VIEW *v, int i, LPSTR out)
{
    LPSTR file = v->pool + v->it[i].file;
    lstrcpy(out, file[0] && file[1] == ':' ? file : v->dir);
    if (file[0] && file[1] == ':')
        PathToParent(out);
}

/* The Type column: Windows 98 names. */
static void ItemType(VIEW *v, int i, LPSTR out)
{
    ITEM far *it = v->it + i;

    switch (it->kind) {
    case IVK_FOLDER:   lstrcpy(out, "File Folder"); break;
    case IVK_COMPUTER:
    case IVK_BIN:      lstrcpy(out, "System Folder"); break;
    case IVK_DRIVE:    DriveTypeName(v->pool[it->file] - 'A', out); break;
    default:           FormatFileType(v->pool + it->file, out);
    }
}

/* ---- geometry ---- */

/* My Computer in Large Icons: its drives as tiles. */
static BOOL Tiled(HWND w)
{
    VIEW *v = View(w);
    return v->mode == IVM_LARGE && !v->dir[0] && !IconViewIsDesktop(w);
}

void IconViewLayout(HWND w)
{
    VIEW *v = View(w);
    RECT r;
    int extent = 0, page = 0, hextent = 0, hpage = 0, col;

    GetClientRect(w, &r);
    switch (v->mode) {
    case IVM_LARGE:
        if (IconViewIsDesktop(w)) {
            v->rows = max(1, r.bottom / CELL_H);
            v->cols = (v->count + v->rows - 1) / v->rows;
            return;
        }
        v->cols = max(1, r.right / (Tiled(w) ? TILE_W : CELL_W));
        v->rows = (v->count + v->cols - 1) / v->cols;
        extent = v->rows * (Tiled(w) ? TILE_H : CELL_H);
        break;
    case IVM_THUMBS:
        v->cols = max(1, r.right / THUMB_W);
        v->rows = (v->count + v->cols - 1) / v->cols;
        extent = v->rows * THUMB_H + 4;
        break;
    case IVM_SMALL:
        v->cols = max(1, (r.right - 2) / SMALL_W);
        v->rows = (v->count + v->cols - 1) / v->cols;
        extent = v->rows * ROW_H + 4;
        break;
    case IVM_LIST:
        v->rows = max(1, (r.bottom - 4) / ROW_H);
        v->cols = (v->count + v->rows - 1) / v->rows;
        hextent = v->cols * v->cellW + 4;
        hpage = r.right;
        break;
    case IVM_DETAILS:
        extent = HEAD_H + v->count * ROW_H;
        for (hextent = 0, col = 0; col < v->ncols; col++)
            hextent += v->colW[col];
        hpage = r.right;
        break;
    }
    page = r.bottom;
    v->scroll = max(0, min(v->scroll, extent - page));
    v->hscroll = max(0, min(v->hscroll, hextent - hpage));
    SetScrollRange(w, SB_VERT, 0, max(0, extent - page), FALSE);
    SetScrollPos(w, SB_VERT, v->scroll, TRUE);
    SetScrollRange(w, SB_HORZ, 0, max(0, hextent - hpage), FALSE);
    SetScrollPos(w, SB_HORZ, v->hscroll, TRUE);
}

void IconViewCellRect(HWND w, int i, RECT *r)
{
    VIEW *v = View(w);
    int col, row;

    switch (v->mode) {
    case IVM_LARGE:
        if (IconViewIsDesktop(w)) { col = i / v->rows; row = i % v->rows; }
        else              { col = i % v->cols; row = i / v->cols; }
        if (Tiled(w)) {
            SetRect(r, col * TILE_W + 2, row * TILE_H - v->scroll + 2, 0, 0);
            r->right = r->left + TILE_W;
            r->bottom = r->top + TILE_H;
            return;
        }
        SetRect(r, col * CELL_W, row * CELL_H - v->scroll + 2, 0, 0);
        r->right = r->left + CELL_W;
        r->bottom = r->top + CELL_H;
        return;
    case IVM_THUMBS:
        SetRect(r, (i % v->cols) * THUMB_W + 2, (i / v->cols) * THUMB_H - v->scroll + 2, 0, 0);
        r->right = r->left + THUMB_W;
        r->bottom = r->top + THUMB_H;
        return;
    case IVM_SMALL:
        SetRect(r, (i % v->cols) * SMALL_W + 2, (i / v->cols) * ROW_H - v->scroll + 2, 0, 0);
        r->right = r->left + SMALL_W;
        break;
    case IVM_LIST:
        SetRect(r, (i / v->rows) * v->cellW - v->hscroll + 2, (i % v->rows) * ROW_H + 2, 0, 0);
        r->right = r->left + v->cellW;
        break;
    default:
        SetRect(r, -v->hscroll, HEAD_H + i * ROW_H - v->scroll, -v->hscroll, 0);
        for (col = 0; col < v->ncols; col++)
            r->right += v->colW[col];
    }
    r->bottom = r->top + ROW_H;
}

/* The two parts that react to the mouse: the icon and the label. */
void IconViewPartRects(HWND w, int i, RECT *icon, RECT *label)
{
    VIEW *v = View(w);
    RECT c;
    int room;

    IconViewCellRect(w, i, &c);
    if (Tiled(w)) {                     /* the icon at the left; the name, with what is under it */
        SetRect(icon, c.left + 4, c.top + (TILE_H - 36) / 2, c.left + 40, c.top + (TILE_H + 36) / 2);
        SetRect(label, c.left + 44, c.top + 4, c.left + 48 + min(v->it[i].textW, TILE_W - 56), c.top + 20);
        if (v->it[i].size)
            label->bottom = c.top + 52;
        return;
    }
    if (v->mode == IVM_THUMBS) {        /* the picture's box (its frame too), the name under it */
        SetRect(icon, c.left + (THUMB_W - THUMB) / 2 - 2, c.top + 2, c.left + (THUMB_W + THUMB) / 2 + 2, c.top + THUMB + 6);
        room = min(v->it[i].textW + 4, THUMB_W - 4);
        SetRect(label, c.left + (THUMB_W - room) / 2, icon->bottom + 3, c.left + (THUMB_W + room) / 2, icon->bottom + 19);
        return;
    }
    if (v->mode == IVM_LARGE) {
        SetRect(icon, c.left + (CELL_W - 36) / 2, c.top, c.left + (CELL_W + 36) / 2, c.top + 34);
        SetRect(label, c.left + 2, c.top + 35, c.right - 2, c.bottom - 4);
        return;
    }
    room = (v->mode == IVM_DETAILS ? v->colW[0] : c.right - c.left) - 24;
    SetRect(icon, c.left + 2, c.top + 1, c.left + 18, c.top + 17);
    SetRect(label, c.left + 20, c.top + 1, c.left + 20 + min(v->it[i].textW + 4, room), c.bottom - 1);
}

int IconViewHitTest(HWND w, int x, int y)
{
    VIEW *v = View(w);
    RECT icon, label;
    POINT p;
    int i;

    p.x = x;
    p.y = y;
    if (v->mode == IVM_DETAILS && y < HEAD_H)
        return -1;
    for (i = 0; i < v->count; i++) {
        IconViewPartRects(w, i, &icon, &label);
        if (Tiled(w)) {                 /* a tile is one block: icon, name, bar */
            IconViewCellRect(w, i, &label);
            InflateRect(&label, -2, -2);
        }
        if (PtInRect(&icon, p) || PtInRect(&label, p))
            return i;
    }
    return -1;
}

/* ---- painting ---- */

/* Large Icons label: at most two lines, ending in "..." when it does not
   fit; *box becomes the rectangle the text takes. */
static void DrawLabel(HDC dc, LPCSTR text, RECT *box, BOOL selected, BOOL desktop)
{
    char buf[MAX_PATH + 4];
    RECT calc;
    int n = lstrlen(text), lineH;
    TEXTMETRIC tm;

    int lo = 0, hi, mid;

    GetTextMetrics(dc, &tm);
    lineH = tm.tmHeight;
    lstrcpy(buf, text);
    calc = *box;
    DrawText(dc, buf, -1, &calc, DT_CALCRECT | DT_WORDBREAK | DT_CENTER | DT_NOPREFIX);
    if (calc.bottom - calc.top > 2 * lineH) {   /* the longest start, with "...", in two lines */
        for (hi = n - 1; lo < hi; ) {
            mid = (lo + hi + 1) / 2;
            lstrcpyn(buf, text, mid + 1);
            lstrcat(buf, "...");
            calc = *box;
            DrawText(dc, buf, -1, &calc, DT_CALCRECT | DT_WORDBREAK | DT_CENTER | DT_NOPREFIX);
            if (calc.bottom - calc.top <= 2 * lineH)
                lo = mid;
            else
                hi = mid - 1;
        }
        lstrcpyn(buf, text, lo + 1);
        lstrcat(buf, "...");
        calc = *box;
        DrawText(dc, buf, -1, &calc, DT_CALCRECT | DT_WORDBREAK | DT_CENTER | DT_NOPREFIX);
    }
    n = calc.right - calc.left + 2;             /* width of the widest line */
    calc.left = box->left + (box->right - box->left - n) / 2;
    calc.right = calc.left + n;
    if (selected) {
        DrawSolidRect(dc, &calc, GetSysColor(COLOR_HIGHLIGHT));
        SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
    } else if (desktop) {               /* white over a black shadow (as Windows XP): read on any wallpaper */
        RECT shadow = calc;
        OffsetRect(&shadow, 1, 1);
        SetTextColor(dc, RGB(0, 0, 0));
        DrawText(dc, buf, -1, &shadow, DT_WORDBREAK | DT_CENTER | DT_NOPREFIX);
        SetTextColor(dc, RGB(255, 255, 255));
    } else {
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
    }
    DrawText(dc, buf, -1, &calc, DT_WORDBREAK | DT_CENTER | DT_NOPREFIX);
    *box = calc;
}

static HICON ResolveIcon(HWND w, int i)
{
    ITEM far *it = View(w)->it + i;
    char path[MAX_PATH];

    if (!it->icon) {
        IconViewItemPath(w, i, path);
        switch (it->kind) {
        case IVK_COMPUTER: it->icon = IconStock(IDI_COMPUTER); break;
        case IVK_BIN:      it->icon = IconStock(RecycleBinIsEmpty() ? IDI_BIN : IDI_BINFULL); break;
        case IVK_DRIVE:    it->icon = IconForDrive(path[0] - 'A'); break;
        default:           it->icon = IconForFile(path, it->kind == IVK_FOLDER);
        }
    }
    return it->icon;
}

/* The Size, Type and Modified cells of a Details row. */
static void PaintColumns(HDC dc, VIEW *v, int i, RECT *row)
{
    ITEM far *it = v->it + i;
    char text[64];
    RECT cell;
    int col, x = row->left + v->colW[0];

    SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
    for (col = 1; col < v->ncols; x += v->colW[col++]) {
        text[0] = '\0';
        if (v->colKey[col] == IVSORT_SIZE && it->kind == IVK_FILE)
            FormatKB(it->size, text);
        else if (v->colKey[col] == IVSORT_TYPE)
            ItemType(v, i, text);
        else if (v->colKey[col] == IVSORT_DATE && it->date)
            FormatDateTime(it->date, it->time, text);
        else if (v->colKey[col] == IVSORT_FOLDER)
            Folder(v, i, text);
        SetRect(&cell, x + 6, row->top, x + v->colW[col] - 6, row->bottom);
        DrawTextEllipsis(dc, text, &cell, (v->colKey[col] == IVSORT_SIZE ? DT_RIGHT : DT_LEFT) | DT_NOPREFIX);
    }
}

static void PaintHeader(HWND w, HDC dc)
{
    VIEW *v = View(w);
    RECT r, c;
    int col, x = -v->hscroll;

    GetClientRect(w, &c);
    for (col = 0; col < v->ncols; col++) {
        SetRect(&r, x, 0, x + v->colW[col], HEAD_H);
        PaintHeaderCell(dc, &r, KeyName[v->colKey[col]], v->colKey[col] == IVSORT_SIZE ? DT_RIGHT : DT_LEFT,
                   col == v->headDown);
        x = r.right;
    }
    SetRect(&r, x, 0, max(x, c.right) + 2, HEAD_H);    /* the rest of the row */
    PaintHeaderCell(dc, &r, "", DT_LEFT, FALSE);
}

/* A BMP file's picture shrunk into a thumbnail's box (its shape kept; a
   small one as it is), on the window's colour. */
static HBITMAP MakeThumb(HDC dc, LPCSTR path)
{
    char shortPath[MAX_PATH];
    BITMAPINFOHEADER FAR *bi;
    BYTE huge *d;
    HGLOBAL mem;
    HBITMAP bm = NULL, old;
    HFILE h;
    HDC mem_dc;
    RECT box;
    LONG size;
    int w, ht, cw, ch;

    DosShortPath(path, shortPath);
    if ((h = _lopen(shortPath, OF_READ | OF_SHARE_DENY_NONE)) == HFILE_ERROR)
        return NULL;
    size = _llseek(h, 0, 2);
    _llseek(h, 0, 0);
    if (size < 54 || size > 0x400000L || (mem = GlobalAlloc(GMEM_MOVEABLE, size)) == NULL) {
        _lclose(h);
        return NULL;
    }
    d = (BYTE huge *)GlobalLock(mem);
    bi = (BITMAPINFOHEADER FAR *)(d + 14);
    if (_hread(h, (void huge *)d, size) == size && d[0] == 'B' && d[1] == 'M' && bi->biSize == 40 &&
        bi->biWidth > 0 && bi->biHeight > 0) {
        w = (int)bi->biWidth;
        ht = (int)bi->biHeight;
        cw = w <= THUMB && ht <= THUMB ? w : w >= ht ? THUMB : max(1, (int)((long)w * THUMB / ht));
        ch = w <= THUMB && ht <= THUMB ? ht : w >= ht ? max(1, (int)((long)ht * THUMB / w)) : THUMB;
        mem_dc = CreateCompatibleDC(dc);
        bm = CreateCompatibleBitmap(dc, THUMB, THUMB);
        old = SelectObject(mem_dc, bm);
        SetRect(&box, 0, 0, THUMB, THUMB);
        DrawSolidRect(mem_dc, &box, GetSysColor(COLOR_WINDOW));
        SetStretchBltMode(mem_dc, COLORONCOLOR);
        StretchDIBits(mem_dc, (THUMB - cw) / 2, (THUMB - ch) / 2, cw, ch, 0, 0, w, ht,
                      (LPVOID)(d + *(DWORD FAR *)(d + 10)), (BITMAPINFO FAR *)bi, DIB_RGB_COLORS, SRCCOPY);
        SelectObject(mem_dc, old);
        DeleteDC(mem_dc);
    }
    _lclose(h);
    GlobalUnlock(mem);
    GlobalFree(mem);
    return bm;
}

/* A thumbnail: its framed box (the selection's colour round it when
   selected), the picture or else the large icon in the middle. */
static void PaintThumb(HWND w, HDC dc, int i, const RECT *box, BOOL lit)
{
    ITEM far *it = &View(w)->it[i];
    char path[MAX_PATH];
    RECT r = *box;
    HDC mem_dc;
    HBITMAP old;

    if (lit) {
        DrawFrame(dc, &r, GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHT));
        InflateRect(&r, -1, -1);
        DrawFrame(dc, &r, GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHT));
    } else {
        InflateRect(&r, -1, -1);
        DrawFrame(dc, &r, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNSHADOW));
    }
    if (!it->thumb && it->kind == IVK_FILE && IconViewItemPath(w, i, path) &&
        !lstrcmpi(PathExtension(path), ".bmp"))
        it->thumb = MakeThumb(dc, path);
    if (it->thumb) {
        mem_dc = CreateCompatibleDC(dc);
        old = SelectObject(mem_dc, it->thumb);
        BitBlt(dc, box->left + 2, box->top + 2, THUMB, THUMB, mem_dc, 0, 0, SRCCOPY);
        SelectObject(mem_dc, old);
        DeleteDC(mem_dc);
    } else {
        DrawIconImage(dc, box->left + 2 + (THUMB - 32) / 2, box->top + 2 + (THUMB - 32) / 2, ResolveIcon(w, i), FALSE);
        if (it->flags & IF_SHORTCUT)
            DrawIconImage(dc, box->left + 2 + (THUMB - 32) / 2, box->top + 2 + (THUMB - 32) / 2, IconStock(IDI_ARROW), FALSE);
    }
}

/* A drive's tile: its icon, its name, a bar of how full it is (Windows 98's
   progress blocks, dark red past 90%) and "1.2 GB free of 2 GB"; a disk that
   cannot be read (no floppy in) has an empty bar, "0 bytes free of 0 bytes".
   Selected, the whole tile is, as in newer Windows. */
static void PaintTile(HWND w, HDC dc, int i, const RECT *cell, BOOL lit)
{
    VIEW *v = View(w);
    ITEM far *it = &v->it[i];
    char room[24], total[24], text[64];
    RECT name, bar, block, all;
    int used, x, step;

    if (lit) {
        all = *cell;
        InflateRect(&all, -2, -2);
        DrawSolidRect(dc, &all, GetSysColor(COLOR_HIGHLIGHT));
    }
    DrawIconImage(dc, cell->left + 6, cell->top + (TILE_H - 32) / 2, ResolveIcon(w, i), FALSE);
    SetRect(&name, cell->left + 44, cell->top + 4, cell->left + 48 + min(it->textW, TILE_W - 56), cell->top + 20);
    SetTextColor(dc, GetSysColor(lit ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    InflateRect(&name, -2, 0);
    DrawTextEllipsis(dc, v->pool + it->name, &name, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
    SetRect(&bar, cell->left + 46, cell->top + 23, cell->left + 46 + BAR_W, cell->top + 37);
    DrawFrame(dc, &bar, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
    InflateRect(&bar, -1, -1);
    DrawSolidRect(dc, &bar, GetSysColor(COLOR_BTNFACE));
    used = (int)((it->size - it->free) / (it->size / 1000 + 1) * (bar.right - bar.left - 2) / 1000);
    step = (bar.bottom - bar.top) * 2 / 3 + 2;
    for (x = 0; x < used; x += step) {
        SetRect(&block, bar.left + 1 + x, bar.top + 1, bar.left + 1 + min(x + step - 2, used), bar.bottom - 1);
        DrawSolidRect(dc, &block, it->size - it->free >= it->size / 10 * 9 ? RGB(128, 0, 0) : GetSysColor(COLOR_HIGHLIGHT));
    }
    FormatSize(it->free, room);
    FormatSize(it->size, total);
    wsprintf(text, "%s free of %s", (LPSTR)room, (LPSTR)total);
    SetTextColor(dc, GetSysColor(lit ? COLOR_HIGHLIGHTTEXT : COLOR_GRAYTEXT));
    TextOut(dc, cell->left + 46, cell->top + 39, text, lstrlen(text));
}

/* Drawn off the screen first, then put there with one BitBlt: the screen
   never shows half a picture (the background without its items). */
void IconViewPaint(HWND w)
{
    VIEW *v = View(w);
    PAINTSTRUCT ps;
    RECT cell, icon, label, clip;
    HFONT oldFont;
    OFFSCREEN o;
    HDC dc;
    BOOL desktop = IconViewIsDesktop(w), focused = GetFocus() == w, lit;
    int i, x;

    BeginPaint(w, &ps);
    dc = DrawOffscreenBegin(&o, ps.hdc, &ps.rcPaint);
    if (desktop)
        DrawWallpaper(dc);
    else
        DrawSolidRect(dc, &ps.rcPaint, GetSysColor(COLOR_WINDOW));
    oldFont = SelectObject(dc, DrawFont(FALSE));
    SetBkMode(dc, TRANSPARENT);
    for (i = 0; i < v->count; i++) {
        IconViewCellRect(w, i, &cell);
        if (!IntersectRect(&clip, &cell, &ps.rcPaint))
            continue;
        lit = (Sel(v, i) && (focused || !desktop)) || i == v->drop;
        if (Tiled(w)) {
            PaintTile(w, dc, i, &cell, lit);
            IconViewPartRects(w, i, &icon, &label);
            label.bottom = label.top + 16;
        } else if (v->mode == IVM_THUMBS) {
            IconViewPartRects(w, i, &icon, &label);
            PaintThumb(w, dc, i, &icon, lit);
            if (lit)
                DrawSolidRect(dc, &label, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(dc, GetSysColor(lit ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
            InflateRect(&label, -2, 0);
            DrawTextEllipsis(dc, v->pool + v->it[i].name, &label, DT_CENTER | DT_NOPREFIX);
            InflateRect(&label, 2, 0);
        } else if (v->mode == IVM_LARGE) {
            x = cell.left + (CELL_W - 32) / 2;
            DrawIconImage(dc, x, cell.top, ResolveIcon(w, i), FALSE);
            if (v->it[i].flags & IF_SHORTCUT)
                DrawIconImage(dc, x, cell.top, IconStock(IDI_ARROW), FALSE);
            SetRect(&label, cell.left + (CELL_W - LABEL_W) / 2, cell.top + 35,
                    cell.left + (CELL_W + LABEL_W) / 2, cell.bottom);
            DrawLabel(dc, v->pool + v->it[i].name, &label, lit, desktop);
        } else {
            IconViewPartRects(w, i, &icon, &label);
            DrawIconImage(dc, icon.left, icon.top, ResolveIcon(w, i), TRUE);
            if (v->it[i].flags & IF_SHORTCUT)
                DrawIconImage(dc, icon.left, icon.top, IconStock(IDI_ARROW), TRUE);
            if (lit)
                DrawSolidRect(dc, &label, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(dc, GetSysColor(lit ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
            InflateRect(&label, -2, 0);
            DrawTextEllipsis(dc, v->pool + v->it[i].name, &label, DT_LEFT | DT_NOPREFIX);
            InflateRect(&label, 2, 0);
            if (v->mode == IVM_DETAILS)
                PaintColumns(dc, v, i, &cell);
        }
        if (i == v->focus && focused && !lit)   /* (over the selection, its XOR is a yellow line) */
            DrawFocusRect(dc, &label);
    }
    if (v->mode == IVM_DETAILS)
        PaintHeader(w, dc);
    SelectObject(dc, oldFont);
    DrawOffscreenEnd(&o);
    EndPaint(w, &ps);
}

/* Name widths for the small modes, and the List column width. */
void IconViewMeasure(HWND w)
{
    VIEW *v = View(w);
    HDC dc = GetDC(w);
    HFONT old = SelectObject(dc, DrawFont(FALSE));
    LPSTR name;
    int i, widest = 60;

    for (i = 0; i < v->count; i++) {
        name = v->pool + v->it[i].name;
        v->it[i].textW = LOWORD(GetTextExtent(dc, name, lstrlen(name)));
        widest = max(widest, v->it[i].textW);
    }
    SelectObject(dc, old);
    ReleaseDC(w, dc);
    v->cellW = min(LIST_MAX, widest + 28);
}

/* ---- scrolling ---- */

void IconViewScroll(HWND w, BOOL horizontal, int code, int pos)
{
    VIEW *v = View(w);
    RECT r;
    int *at = horizontal ? &v->hscroll : &v->scroll, old = *at;
    int line = !horizontal ? (v->mode == IVM_LARGE ? CELL_H / 2 : v->mode == IVM_THUMBS ? THUMB_H / 2 : ROW_H) : v->mode == IVM_LIST ? v->cellW : 20;

    GetClientRect(w, &r);
    switch (code) {
    case SB_LINEUP:        *at -= line; break;
    case SB_LINEDOWN:      *at += line; break;
    case SB_PAGEUP:        *at -= horizontal ? r.right : r.bottom; break;
    case SB_PAGEDOWN:      *at += horizontal ? r.right : r.bottom; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: *at = pos; break;
    default:               return;
    }
    IconViewLayout(w);
    if (*at == old)
        return;
    PostMessage(w, MSG_ENDEDIT, TRUE, 0L);
    if (horizontal) {
        ScrollWindow(w, old - *at, 0, NULL, NULL);
    } else {
        if (v->mode == IVM_DETAILS)             /* the header stays */
            r.top = HEAD_H;
        ScrollWindow(w, 0, old - *at, &r, &r);
    }
}
