/*
 * The colours of the Windows 98 look that Windows 3.1 has no index for:
 * where caption gradients end, tooltips, the dark outer shadow of edges,
 * names under the pointer. WIN.INI [colors] keeps them under Windows 98's
 * own keys (Windows 3.1 ignores those); a colour without its key is made
 * from the scheme's, so any scheme gets a look that goes with it. They are
 * read again only after the colours change (hooks.c sees it).
 */
#include "snsystem.h"

static const char *keys[SC_COUNT] = {
    "GradientActiveTitle", "GradientInactiveTitle", "InfoWindow", "InfoText", "ButtonDkShadow",
    "HotTrackingColor",
};
static const int bases[SC_COUNT] = {     /* what each is made from */
    COLOR_ACTIVECAPTION, COLOR_INACTIVECAPTION, COLOR_WINDOW, COLOR_WINDOWTEXT, COLOR_BTNSHADOW,
    COLOR_HIGHLIGHT,
};
static COLORREF now[SC_COUNT];
static BOOL fresh;

/* pct percent of the way from a to b */
static COLORREF Mix(COLORREF a, COLORREF b, int pct)
{
    return RGB(GetRValue(a) + ((GetRValue(b) - GetRValue(a)) * pct + 50) / 100,
               GetGValue(a) + ((GetGValue(b) - GetGValue(a)) * pct + 50) / 100,
               GetBValue(a) + ((GetBValue(b) - GetBValue(a)) * pct + 50) / 100);
}

/* "16 132 208" */
static BOOL Parse(LPCSTR s, COLORREF *c)
{
    int v[3], i;

    for (i = 0; i < 3; i++) {
        while (*s == ' ')
            s++;
        if (*s < '0' || *s > '9')
            return FALSE;
        for (v[i] = 0; *s >= '0' && *s <= '9'; s++)
            v[i] = v[i] * 10 + *s - '0';
    }
    *c = RGB(v[0] & 255, v[1] & 255, v[2] & 255);
    return TRUE;
}

/* The colour sc is without its key, made from base (the colour of the
   COLOR_* index it goes with): gentle, lighter ends for the gradients,
   Windows 98's own for the standard scheme. */
COLORREF SNSYSTEM_API SkinColorMade(int sc, COLORREF base)
{
    switch (sc) {
    case SC_GRADIENT_ACTIVE:
        return base == RGB(0, 0, 128) ? RGB(16, 132, 208) : Mix(base, RGB(255, 255, 255), 45);
    case SC_GRADIENT_INACTIVE:
        return Mix(base, RGB(255, 255, 255), 50);
    case SC_INFO:
        return RGB(255, 255, 225);
    case SC_INFOTEXT:
    case SC_DKSHADOW:
        return RGB(0, 0, 0);
    }
    return base;                        /* SC_HOTTRACK: the selection's */
}

COLORREF SNSYSTEM_API SkinColor(int sc)
{
    char value[16];
    int i;

    if (!fresh) {
        for (i = 0; i < SC_COUNT; i++)
            if (!GetProfileString("colors", keys[i], "", value, sizeof value) || !Parse(value, &now[i]))
                now[i] = SkinColorMade(i, GetSysColor(bases[i]));
        fresh = TRUE;
    }
    return now[sc];
}

LPCSTR SNSYSTEM_API SkinColorKey(int sc)
{
    return keys[sc];
}

int SNSYSTEM_API SkinColorBase(int sc)
{
    return bases[sc];
}

void SNSYSTEM_API SkinColorsChanged(void)
{
    fresh = FALSE;
}
