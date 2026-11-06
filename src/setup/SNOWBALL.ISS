; -- SNOWBALL.ISS -- Snowball's Setup, for Inno Setup 1.2.16's 16-bit edition:
; SNOWSET.EXE runs in any Windows 3.1 or 3.11. Compiled inside Windows 3.1
; (compil16 /cc), from the folder ..\..\makesetup.ps1 stages: this script and the
; files it names (release\, LICENSE as LICENSE.TXT, NOTICE.TXT, INFO.TXT, LFN.TXT,
; RESEXT.TXT).
; The license page shows NOTICE.TXT: Inno Setup 1.2 takes no text over 32000 bytes.
;
; Setup cannot replace a library the running shell has loaded (Windows 3.1
; has no WININIT.INI): the libraries go to NEW\, and SNOWBALL.EXE moves them
; into place at its next start, before it loads them. SBSETUP.EXE does what
; Inno Setup cannot: the INI changes (kept for the uninstaller), the system
; font, and the components (Inno Setup 1.2 has none of its own): its dialog
; offers LFN.DRV, USERX.DRV and GDIX.DRV, copies the ones chosen to
; WINDOWS\SYSTEM and adds them to drivers=.

[Setup]
Bits=16
AppName=Snowball
AppVerName=Snowball v20261106
AppCopyright=Snowball: a rewrite of Calmira Reborn, under the GNU GPL 3
MinVersion=3.1,0
DefaultDirName={win}\SHELL
DefaultGroupName=Snowball
LicenseFile=NOTICE.TXT
InfoAfterFile=INFO.TXT
AlwaysRestart=1
OutputBaseFilename=SNOWSET
OutputDir=OUT

[Messages]
WelcomeLabel=Welcome to the [name] setup program. This will install [name/ver], a Windows 98 style shell, on your computer.%n%nIt is strongly recommended that you close all other applications you have running before continuing.

[Files]
Source: "SNOWBALL.EXE"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "SNSHELL.DLL"; DestDir: "{app}\NEW"; CopyMode: alwaysoverwrite
Source: "SNSYSTEM.DLL"; DestDir: "{app}\NEW"; CopyMode: alwaysoverwrite
Source: "W98ART.DLL"; DestDir: "{app}\NEW"; CopyMode: alwaysoverwrite
Source: "SBSETUP.EXE"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "OPEN.COM"; DestDir: "{win}"; CopyMode: alwaysoverwrite
Source: "LFN.DRV"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "USERX.DRV"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "GDIX.DRV"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "LICENSE.TXT"; DestDir: "{app}"
Source: "INFO.TXT"; DestDir: "{app}"
Source: "LFN.TXT"; DestDir: "{app}"
Source: "RESEXT.TXT"; DestDir: "{app}"

[Icons]
Name: "{group}\Uninstall Snowball"; Filename: "{app}\SBSETUP.EXE"; Parameters: "/remove"
Name: "{group}\Snowball Notes"; Filename: "{app}\INFO.TXT"

[Run]
Filename: "{app}\SBSETUP.EXE"; Parameters: "/install"

[UninstallRun]
Filename: "{app}\SBSETUP.EXE"; Parameters: "/uninstall"

[UninstallDelete]
Type: files; Name: "{app}\SNSHELL.DLL"
Type: files; Name: "{app}\SNSYSTEM.DLL"
Type: files; Name: "{app}\W98ART.DLL"
Type: files; Name: "{app}\UNINSTAL.INI"
Type: dirifempty; Name: "{app}\NEW"
