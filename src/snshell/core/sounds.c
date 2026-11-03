/*
 * Windows 98's sound events (Open program, Minimize, Menu popup...), kept in
 * WIN.INI [sounds] the way Windows 3.1 keeps its own ("Open=x.wav,Open
 * program"): Control Panel's Sound lists them, and a sound is chosen
 * there. Snowball plays one only when a file is set for it.
 */
#include "internal.h"
#include <mmsystem.h>

static const char *Events[][2] = {      /* by SOUND_*: the key, Control Panel's name */
    { "Open", "Open program" },         { "Close", "Close program" },
    { "Minimize", "Minimize" },         { "Maximize", "Maximize" },
    { "RestoreUp", "Restore Up" },      { "RestoreDown", "Restore Down" },
    { "MenuPopup", "Menu popup" },      { "EmptyRecycleBin", "Empty Recycle Bin" },
};
#define NEVENTS (sizeof Events / sizeof Events[0])

/* The events Control Panel does not list yet, with no sound. */
void SoundsRegister(void)
{
    char value[80];
    int i;

    for (i = 0; i < NEVENTS; i++)
        if (!GetProfileString("sounds", Events[i][0], "", value, sizeof value)) {
            wsprintf(value, ",%s", (LPSTR)Events[i][1]);
            WriteProfileString("sounds", Events[i][0], value);
        }
}

void SoundPlay(int event)
{
    char value[MAX_PATH];

    if (event < 0 || event >= NEVENTS ||
        !GetProfileString("sounds", Events[event][0], "", value, sizeof value) ||
        value[0] == ',' || !_fstrnicmp(value, "none", 4) || !_fstrnicmp(value, "<none>", 6))
        return;
    sndPlaySound(Events[event][0], SND_ASYNC | SND_NODEFAULT);
}
