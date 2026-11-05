/*
 * Owner-drawn menu items with an icon, as Windows 98 draws the Start menu
 * and Send To: the icon, large or small, then the label; the selected item
 * in the highlight colour. In a pane (the two-column Start menu's left)
 * in the window colours, as Windows XP's.
 */
#include "internal.h"

int MenuItemWidth(LPCSTR label, UINT flags)
{
    HDC dc = GetDC(NULL);
    HFONT old = SelectObject(dc, DrawFont((flags & MIP_BOLD) != 0));
    int text = LOWORD(GetTextExtent(dc, label, lstrlen(label)));

    if (flags & MIP_MNEMONIC)           /* the & is not drawn */
        text -= LOWORD(GetTextExtent(dc, "&", 1));
    SelectObject(dc, old);
    ReleaseDC(NULL, dc);
    return flags & MIP_LARGE ? 44 + text + 24 : min(25 + text + 24, 300);
}

void MenuItemPaint(HDC dc, const RECT *r, HICON icon, LPCSTR label, UINT flags)
{
    BOOL sel = (flags & (MIP_SELECTED | MIP_DISABLED)) == MIP_SELECTED, large = (flags & MIP_LARGE) != 0;
    RECT t = *r;
    HFONT old;

    DrawSolidRect(dc, r, GetSysColor(sel ? COLOR_HIGHLIGHT : flags & MIP_PANE ? COLOR_WINDOW : COLOR_MENU));
    if (icon && !(flags & MIP_CENTER)) {
        if (large)
            DrawIconImage(dc, r->left + 4, r->top + (r->bottom - r->top - 32) / 2, icon, FALSE);
        else
            DrawIconImage(dc, r->left + 3, r->top + (r->bottom - r->top - 16) / 2, icon, TRUE);
    }
    t.left += flags & MIP_CENTER ? 12 : large ? 44 : 25;
    t.right -= 12;
    old = SelectObject(dc, DrawFont((flags & MIP_BOLD) != 0));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(flags & MIP_DISABLED ? COLOR_GRAYTEXT : sel ? COLOR_HIGHLIGHTTEXT :
                                 flags & MIP_PANE ? COLOR_WINDOWTEXT : COLOR_MENUTEXT));
    DrawTextEllipsis(dc, label, &t, (flags & MIP_CENTER ? DT_CENTER : DT_LEFT) | (flags & MIP_MNEMONIC ? 0 : DT_NOPREFIX));
    SelectObject(dc, old);
}

/* The key typed picks the item: its &-letter, else its first letter. */
BOOL MenuItemMatchesKey(LPCSTR label, BOOL mnemonic, WPARAM key)
{
    LPCSTR k = label;

    if (mnemonic && _fstrchr(label, '&'))
        k = _fstrchr(label, '&') + 1;
    return (BYTE)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)*k) == (BYTE)(DWORD)AnsiUpper((LPSTR)(DWORD)(BYTE)key);
}
