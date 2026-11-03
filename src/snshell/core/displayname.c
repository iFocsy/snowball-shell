/* Names as folders show them, after Folder Options: shortcuts without
   .SHC (Desktop as Shortcut without .DESKLINK), known types perhaps without extension, DOS names not all capitals. */
#include "internal.h"

/* A known file type: a program, or an extension WIN.INI associates. */
static BOOL KnownType(LPCSTR name)
{
    char program[8];
    LPSTR ext = PathExtension((LPSTR)name);

    return *ext && (PathIsProgram(name) || GetProfileString("extensions", ext + 1, "", program, sizeof program));
}

/* As Windows 98 shows names: a DOS name all in capitals with only its
   first letter so ("Readme.txt"), unless Folder Options allows them. */
static void Tame(LPSTR name)
{
    LPSTR p;

    if (g_viewOptions.allUpper || !PathIsShortName(name))
        return;
    for (p = name; *p; p++)
        if (IsCharLower(*p))
            return;
    AnsiLower(name + 1);
}

BOOL DisplayNameHidesExtension(LPCSTR fileName)
{
    return PathIsShortcut(fileName) || PathIsDeskLink(fileName) || (g_viewOptions.hideExt && KnownType(fileName));
}

/* Shortcuts without .SHC; known types without their extension if Folder
   Options hides them. */
void DisplayNameOfFile(LPCSTR fileName, LPSTR out)
{
    lstrcpy(out, fileName);
    Tame(out);
    if (DisplayNameHidesExtension(fileName))
        *PathExtension(out) = '\0';
}

void DisplayNameOfFolder(LPCSTR name, LPSTR out)
{
    lstrcpy(out, name);
    Tame(out);
}
