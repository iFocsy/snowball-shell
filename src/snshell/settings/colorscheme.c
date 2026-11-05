/*
 * A colour scheme: Windows 3.1's 21 colours (WIN.INI [colors], in its key
 * order) and Windows 98's six that Windows 3.1 has no index for (title
 * bars' Color 2, tooltips, the dark shadow, hot-tracking), which follow the
 * others until one is chosen. Kept as text (a theme's Colors=, CONTROL.INI's
 * schemes): the 21 as BBGGRR hexadecimal, comma separated, then the six,
 * each empty when made from the others. Applied, the colours are Windows'
 * (SetSysColors) and WIN.INI's, under Windows 98's keys for the six. And a
 * picture of windows in a scheme's colours (Display Properties, the Theme
 * Manager).
 */
#include "internal.h"

static const struct { LPCSTR key, name; int index; } keys[COLOR_KEYS] = {
    { "Background", "Desktop", COLOR_BACKGROUND },
    { "AppWorkspace", "Application background", COLOR_APPWORKSPACE },
    { "Window", "Window", COLOR_WINDOW },
    { "WindowText", "Window text", COLOR_WINDOWTEXT },
    { "Menu", "Menu", COLOR_MENU },
    { "MenuText", "Menu text", COLOR_MENUTEXT },
    { "ActiveTitle", "Active title bar", COLOR_ACTIVECAPTION },
    { "InactiveTitle", "Inactive title bar", COLOR_INACTIVECAPTION },
    { "TitleText", "Active title text", COLOR_CAPTIONTEXT },
    { "ActiveBorder", "Active window border", COLOR_ACTIVEBORDER },
    { "InactiveBorder", "Inactive window border", COLOR_INACTIVEBORDER },
    { "WindowFrame", "Window frame", COLOR_WINDOWFRAME },
    { "Scrollbar", "Scroll bar", COLOR_SCROLLBAR },
    { "ButtonFace", "3D objects", COLOR_BTNFACE },
    { "ButtonShadow", "3D shadow", COLOR_BTNSHADOW },
    { "ButtonText", "Button text", COLOR_BTNTEXT },
    { "GrayText", "Disabled text", COLOR_GRAYTEXT },
    { "Hilight", "Selected items", COLOR_HIGHLIGHT },
    { "HilightText", "Selected text", COLOR_HIGHLIGHTTEXT },
    { "InactiveTitleText", "Inactive title text", COLOR_INACTIVECAPTIONTEXT },
    { "ButtonHilight", "3D highlight", COLOR_BTNHIGHLIGHT },
    { NULL, "Active title bar, Color 2", SC_GRADIENT_ACTIVE },      /* Windows 98's: keys SkinColorKey's */
    { NULL, "Inactive title bar, Color 2", SC_GRADIENT_INACTIVE },
    { NULL, "ToolTip", SC_INFO },
    { NULL, "ToolTip text", SC_INFOTEXT },
    { NULL, "3D dark shadow", SC_DKSHADOW },
    { NULL, "Hot-tracked item", SC_HOTTRACK },
};

LPCSTR ColorSchemeKey(int k)
{
    return keys[k].key ? keys[k].key : SkinColorKey(keys[k].index);
}

LPCSTR ColorSchemeName(int k)
{
    return keys[k].name;
}

/* Colour k: Windows 98's made from its base's if not chosen. */
COLORREF ColorSchemeColor(const COLORSCHEME *s, int k)
{
    int sc = keys[k].index;

    if (k < COLOR_KEYS_31)
        return s->colors[sc];
    return s->chosen[sc] ? s->extra[sc] : SkinColorMade(sc, s->colors[SkinColorBase(sc)]);
}

void ColorSchemeSetColor(COLORSCHEME *s, int k, COLORREF c)
{
    if (k < COLOR_KEYS_31) {
        s->colors[keys[k].index] = c;
    } else {
        s->extra[keys[k].index] = c;
        s->chosen[keys[k].index] = TRUE;
    }
}

/* Whether colour k is the scheme's own (Windows 98's: chosen). */
BOOL ColorSchemeChosen(const COLORSCHEME *s, int k)
{
    return k < COLOR_KEYS_31 || s->chosen[keys[k].index];
}

BOOL ColorSchemeParse(LPCSTR p, COLORSCHEME *s)
{
    DWORD c;
    BOOL any;
    int k;

    if (!p || !*p)
        return FALSE;
    for (k = 0; k < COLOR_KEYS; k++) {
        any = *p && *p != ',';
        for (c = 0; *p && *p != ','; p++)
            c = c * 16 + (*p <= '9' ? *p - '0' : (*p | 0x20) - 'a' + 10);
        c &= 0xFFFFFFL;                             /* the high byte marks palette colours */
        if (k < COLOR_KEYS_31)
            s->colors[keys[k].index] = c;
        else if ((s->chosen[keys[k].index] = any) != FALSE)
            s->extra[keys[k].index] = c;
        if (*p == ',')
            p++;
    }
    return TRUE;
}

void ColorSchemeFormat(const COLORSCHEME *s, LPSTR out)
{
    int k;

    *out = '\0';
    for (k = 0; k < COLOR_KEYS; k++) {
        if (k)
            lstrcat(out, ",");
        if (ColorSchemeChosen(s, k))
            wsprintf(out + lstrlen(out), "%lX", ColorSchemeColor(s, k));
    }
}

/* The colours in use. */
void ColorSchemeCurrent(COLORSCHEME *s)
{
    char v[16];
    int i;

    for (i = 0; i < COLOR_KEYS_31; i++)
        s->colors[keys[i].index] = GetSysColor(keys[i].index);
    for (i = 0; i < SC_COUNT; i++) {
        s->extra[i] = SkinColor(i);
        s->chosen[i] = GetProfileString("colors", SkinColorKey(i), "", v, sizeof v) != 0;
    }
}

/* Windows' colours from now on (every window repaints); name: the scheme's,
   CONTROL.INI's current one. */
void ColorSchemeApply(const COLORSCHEME *s, LPCSTR name)
{
    char value[16];
    int indices[COLOR_KEYS_31], k;
    COLORREF values[COLOR_KEYS_31], c;

    for (k = 0; k < COLOR_KEYS; k++) {
        c = ColorSchemeColor(s, k);
        wsprintf(value, "%d %d %d", GetRValue(c), GetGValue(c), GetBValue(c));
        WriteProfileString("colors", ColorSchemeKey(k), ColorSchemeChosen(s, k) ? value : NULL);
        if (k < COLOR_KEYS_31) {
            indices[k] = keys[k].index;
            values[k] = c;
        }
    }
    WritePrivateProfileString("current", "color schemes", name, "CONTROL.INI");
    SkinColorsChanged();
    SetSysColors(COLOR_KEYS_31, indices, values);
}

/* ---- the picture ---- */

static void Text(HDC dc, LPCSTR s, RECT *r, COLORREF c, BOOL bold, UINT align)
{
    HFONT old = SelectObject(dc, DrawFont(bold));

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    DrawText(dc, s, -1, r, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, old);
}

/* A window's frame in the scheme's colours; r becomes its inside. */
static void Frame(HDC dc, RECT *r, const COLORREF *c, const COLORREF *sc, BOOL active)
{
    COLORREF border = c[active ? COLOR_ACTIVEBORDER : COLOR_INACTIVEBORDER];

    DrawFrame(dc, r, c[COLOR_BTNFACE], sc[SC_DKSHADOW]);
    InflateRect(r, -1, -1);
    DrawFrame(dc, r, c[COLOR_BTNHIGHLIGHT], c[COLOR_BTNSHADOW]);
    InflateRect(r, -1, -1);
    DrawFrame(dc, r, border, border);
    InflateRect(r, -1, -1);
    DrawFrame(dc, r, border, border);
    InflateRect(r, -1, -1);
}

/* A caption across the top of r, which then starts below it. */
static void Caption(HDC dc, RECT *r, LPCSTR title, const COLORREF *c, const COLORREF *sc, BOOL active, BOOL buttons)
{
    int h = GetSystemMetrics(SM_CYCAPTION) - 1, b = h - 4;
    RECT bar, t;

    SetRect(&bar, r->left, r->top, r->right, r->top + h);
    DrawGradient(dc, &bar, c[active ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION],
                 sc[active ? SC_GRADIENT_ACTIVE : SC_GRADIENT_INACTIVE], FALSE);
    t = bar;
    t.left += 4;
    Text(dc, title, &t, c[active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT], TRUE, DT_LEFT);
    SetRect(&t, bar.right - 2 - b - 2, bar.top + 2, bar.right - 2, bar.top + 2 + b);
    SkinCaptionButton(dc, &t, SKIN_CLOSE);
    if (buttons) {
        OffsetRect(&t, -(b + 4), 0);
        SkinCaptionButton(dc, &t, SKIN_MAX);
        OffsetRect(&t, -(b + 2), 0);
        SkinCaptionButton(dc, &t, SKIN_MIN);
    }
    r->top = bar.bottom + 1;
}

/* An arrow of 4, 3, 2 and 1 pixels, up or down, centred in r. */
static void Arrow(HDC dc, const RECT *r, COLORREF ink, BOOL up)
{
    int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2 - 2, i;
    RECT t;

    for (i = 0; i < 4; i++) {
        SetRect(&t, x - 3 + i, y + (up ? 3 - i : i), x + 4 - i, y + (up ? 4 - i : i + 1));
        DrawSolidRect(dc, &t, ink);
    }
}

static void Windows(HDC dc, RECT a, const COLORREF *c, const COLORREF *sc)
{
    RECT r, t, s;
    int h = GetSystemMetrics(SM_CYCAPTION) - 1, sb = GetSystemMetrics(SM_CXVSCROLL), x;

    SetRect(&r, a.left + 8, a.top + 8, a.right - 40, a.top + 8 + h + 8);           /* inactive */
    DrawSolidRect(dc, &r, c[COLOR_BTNFACE]);
    Frame(dc, &r, c, sc, FALSE);
    Caption(dc, &r, "Inactive Window", c, sc, FALSE, TRUE);

    SetRect(&r, a.left + 16, a.top + h + 12, a.right - 24, a.bottom - 16);         /* active */
    DrawSolidRect(dc, &r, c[COLOR_BTNFACE]);
    Frame(dc, &r, c, sc, TRUE);
    Caption(dc, &r, "Active Window", c, sc, TRUE, TRUE);
    SetRect(&t, r.left, r.top, r.right, r.top + h);                                 /* its menu */
    DrawSolidRect(dc, &t, c[COLOR_MENU]);
    x = t.left + 6;
    SetRect(&s, x, t.top, x + 44, t.bottom);
    Text(dc, "Normal", &s, c[COLOR_MENUTEXT], FALSE, DT_LEFT);
    OffsetRect(&s, 44, 0);
    Text(dc, "Disabled", &s, c[COLOR_GRAYTEXT], FALSE, DT_LEFT);
    SetRect(&s, s.right + 2, t.top + 1, s.right + 50, t.bottom - 1);
    DrawSolidRect(dc, &s, c[COLOR_HIGHLIGHT]);
    s.left += 4;
    Text(dc, "Selected", &s, c[COLOR_HIGHLIGHTTEXT], FALSE, DT_LEFT);
    SetRect(&t, r.left + 2, t.bottom + 2, r.right - 2, r.bottom - 2);               /* its contents */
    DrawBevel(dc, &t, TRUE);
    InflateRect(&t, -2, -2);
    DrawSolidRect(dc, &t, c[COLOR_WINDOW]);
    SetRect(&s, t.right - sb, t.top, t.right, t.bottom);                            /* scroll bar */
    DrawSolidRect(dc, &s, c[COLOR_SCROLLBAR]);
    SetRect(&s, t.right - sb, t.top, t.right, t.top + sb);
    DrawSolidRect(dc, &s, c[COLOR_BTNFACE]);
    DrawBevel(dc, &s, FALSE);
    Arrow(dc, &s, c[COLOR_BTNTEXT], TRUE);
    SetRect(&s, t.right - sb, t.bottom - sb, t.right, t.bottom);
    DrawSolidRect(dc, &s, c[COLOR_BTNFACE]);
    DrawBevel(dc, &s, FALSE);
    Arrow(dc, &s, c[COLOR_BTNTEXT], FALSE);
    SetRect(&s, t.left + 4, t.top + 2, t.right - sb - 4, t.top + 2 + h);
    Text(dc, "Window Text", &s, c[COLOR_WINDOWTEXT], FALSE, DT_LEFT);
    SetRect(&s, t.right - sb - 52, t.top + 4, t.right - sb - 4, t.top + 4 + h - 2);   /* a tooltip */
    DrawSolidRect(dc, &s, sc[SC_INFO]);
    DrawFrame(dc, &s, sc[SC_INFOTEXT], sc[SC_INFOTEXT]);
    Text(dc, "ToolTip", &s, sc[SC_INFOTEXT], FALSE, DT_CENTER);

    x = (a.left + a.right) / 2;                                                     /* message box */
    SetRect(&r, x - 70, a.bottom - 2 * h - 34, x + 50, a.bottom - 6);
    DrawSolidRect(dc, &r, c[COLOR_BTNFACE]);
    Frame(dc, &r, c, sc, TRUE);
    Caption(dc, &r, "Message Box", c, sc, TRUE, FALSE);
    SetRect(&s, r.left + 6, r.top, r.right, r.top + h);
    Text(dc, "Message Text", &s, c[COLOR_WINDOWTEXT], FALSE, DT_LEFT);
    SetRect(&s, x - 30, r.bottom - 22, x + 10, r.bottom - 4);
    DrawBevel(dc, &s, FALSE);
    Text(dc, "OK", &s, c[COLOR_BTNTEXT], FALSE, DT_CENTER);
}

/* An inactive and an active window and a message box in a's place, all
   in the scheme's colours (edges and caption buttons too); over the
   desktop's colour if desktop, else over what a already shows. */
void ColorSchemePicture(HDC dc, const RECT *a, const COLORSCHEME *s, BOOL desktop)
{
    COLORREF sc[SC_COUNT];
    int i;

    for (i = 0; i < SC_COUNT; i++)
        sc[i] = ColorSchemeColor(s, COLOR_KEYS_31 + i);
    if (desktop)
        DrawSolidRect(dc, a, s->colors[COLOR_BACKGROUND]);
    DrawPreviewColors(s->colors, sc);
    Windows(dc, *a, s->colors, sc);
    DrawPreviewColors(NULL, NULL);
}
