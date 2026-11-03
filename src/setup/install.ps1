<#
  Installs Snowball as the Windows 3.11 shell of this DOSBox-X setup, from the
  host (SNOWSET.EXE does it from inside any Windows 3.1):
    - SNOWBALL.EXE, SNSHELL.DLL, SNSYSTEM.DLL and W98ART.DLL go to
      drive-c\WINDOWS\SHELL (SNOWBALL.EXE loads the libraries from there);
      the copies older versions put in WINDOWS\SYSTEM, and SNOWSYS.DLL, go
    - OPEN.COM to WINDOWS (on the PATH): MS-DOS Prompt windows open things
      through Snowball (OPEN ., OPEN file)
    - SYSTEM.INI [boot] shell= and taskman.exe= (Ctrl+Esc opens the Start menu)
    - the Windows 98 colour scheme (WIN.INI [colors], CONTROL.INI scheme list)
    - MS Sans Serif 8 as the system font (SBSANS8.FON, built by sysfont.ps1)
    - [386Enh] WinTimeSlice=200,50: Windows programs get more time than DOS boxes
    - w311.conf: "Shut down" closes DOSBox-X (POWEROFF.FLG checked after win), and
      DOSBox-X's CLIP$ device, through which the clipboard's text is shared with
      the host
  Every value it replaces is first saved in SHELL\UNINSTAL.INI;
  -Uninstall puts them back and returns to Program Manager.

  -DriveC  the host folder DOSBox-X mounts as C: (Windows in its WINDOWS)
  -Conf    DOSBox-X's configuration file; without one, Shut Down and the
           clipboard stay as they are (INFO.TXT says what to add)
  -Root    a folder holding both as drive-c and w311.conf (the default,
           three levels up, is where the maintainer's setup has them)
  Inside Windows, without DOSBox-X's host, SNOWSET.EXE installs instead.
#>
param(
  [string]$DriveC,
  [string]$Conf,
  [string]$Root = (Join-Path $PSScriptRoot '..\..\..'),
  [switch]$Uninstall
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ini.ps1')
if (-not $DriveC) {
  $DriveC = Join-Path $Root 'drive-c'
  if (-not $Conf) { $Conf = Join-Path $Root 'w311.conf' }
}
if (-not (Test-Path (Join-Path $DriveC 'WINDOWS\WIN.COM'))) {
  throw "No Windows in $DriveC`: give -DriveC, the folder DOSBox-X mounts as C:."
}
$win = Join-Path (Resolve-Path $DriveC).Path 'WINDOWS'
$shellDir = Join-Path $win 'SHELL'
$saved = Join-Path $shellDir 'UNINSTAL.INI'
$conf = if ($Conf -and (Test-Path $Conf)) { (Resolve-Path $Conf).Path } else { $null }
$dlls = 'SNSHELL.DLL', 'SNSYSTEM.DLL', 'W98ART.DLL'

# Windows 98 "Windows Standard" colours: WIN.INI name = "R G B"
$colors = [ordered]@{
  Background = '0 128 128'; AppWorkspace = '128 128 128'; Window = '255 255 255'; WindowText = '0 0 0'
  Menu = '192 192 192'; MenuText = '0 0 0'; ActiveTitle = '0 0 128'; InactiveTitle = '128 128 128'
  TitleText = '255 255 255'; ActiveBorder = '192 192 192'; InactiveBorder = '192 192 192'
  WindowFrame = '0 0 0'; Scrollbar = '192 192 192'; ButtonFace = '192 192 192'
  ButtonShadow = '128 128 128'; ButtonText = '0 0 0'; GrayText = '128 128 128'; Hilight = '0 0 128'
  HilightText = '255 255 255'; InactiveTitleText = '192 192 192'; ButtonHilight = '255 255 255'
}
# CONTROL.INI keeps schemes as BBGGRR hex in the same order
$scheme = ($colors.Values | ForEach-Object {
  $r, $g, $b = $_ -split ' ' | ForEach-Object { [int]$_ }
  '{0:X}' -f ($b * 65536 + $g * 256 + $r)
}) -join ','

# Files and keys this installer changes: file, section, key, new value
$changes = @(
  @('SYSTEM.INI', 'boot', 'shell', 'C:\WINDOWS\SHELL\SNOWBALL.EXE'),
  @('SYSTEM.INI', 'boot', 'taskman.exe', 'C:\WINDOWS\SHELL\SNOWBALL.EXE'),
  @('SYSTEM.INI', 'boot', 'fonts.fon', 'sbsans8.fon'),
  @('SYSTEM.INI', '386Enh', 'WinTimeSlice', '200,50'),   # more time for Windows against busy DOS boxes
  @('CONTROL.INI', 'color schemes', 'Windows 98', $scheme),
  @('CONTROL.INI', 'current', 'color schemes', 'Windows 98')
) + ($colors.Keys | ForEach-Object { , @('WIN.INI', 'colors', $_, $colors[$_]) })

# w311.conf [dos]: DOSBox-X's CLIP$ device, through which the clipboard's
# text is shared with the host; $null takes the line out.
function Set-ClipboardDevice([AllowNull()][string]$value) {
  $c = [System.Collections.Generic.List[string]](Get-Content $conf)
  $dos = $c.IndexOf('[dos]')
  if ($dos -lt 0) { return }
  $end = $dos + 1
  while ($end -lt $c.Count -and $c[$end] -notmatch '^\[') { $end++ }
  $at = -1
  for ($i = $dos + 1; $i -lt $end; $i++) { if ($c[$i] -match '^\s*dos clipboard device enable\s*=') { $at = $i } }
  $comment = "# The clipboard's text shared with Windows: Snowball reads and writes CLIP$."
  if ($at -ge 0 -and $value) {
    $c[$at] = "dos clipboard device enable = $value"
  } elseif ($at -ge 0) {
    $c.RemoveAt($at)
    if ($c[$at - 1] -eq $comment) { $c.RemoveAt($at - 1) }
  } elseif ($value) {
    while ($end -gt $dos + 1 -and $c[$end - 1].Trim() -eq '') { $end-- }
    $c.Insert($end, "dos clipboard device enable = $value")
    $c.Insert($end, $comment)
  }
  Set-Content $conf $c -Encoding ascii
}

function Update-Ini([string]$file, [scriptblock]$edit) {
  $path = Join-Path $win $file
  Set-Content $path (& $edit (Get-Content $path)) -Encoding ascii
}

$autoexecOld = 'if exist C:\WINDOWS\WIN.COM win'
$autoexecNew = @($autoexecOld,
  'if exist C:\WINDOWS\SHELL\POWEROFF.FLG goto poweroff', 'goto end', ':poweroff',
  'del C:\WINDOWS\SHELL\POWEROFF.FLG', 'exit', ':end')

if ($Uninstall) {
  if (-not (Test-Path $saved)) { throw "Nothing to undo: $saved not found" }
  $old = Get-Content $saved
  foreach ($file in 'SYSTEM.INI', 'WIN.INI', 'CONTROL.INI') {
    Update-Ini $file {
      param($lines)
      foreach ($c in $changes | Where-Object { $_[0] -eq $file }) {
        $lines = Set-IniValue $lines $c[1] $c[2] (Get-IniValue $old $file "$($c[1])|$($c[2])")
      }
      , $lines
    }
  }
  if ($conf) {
    (Get-Content $conf -Raw).Replace(($autoexecNew -join "`r`n"), $autoexecOld) | Set-Content $conf -Encoding ascii -NoNewline
    Set-ClipboardDevice (Get-IniValue $old 'w311.conf' 'dos|dos clipboard device enable')
  }
  Remove-Item $shellDir -Recurse -Force
  Remove-Item (Join-Path $win 'OPEN.COM') -ErrorAction SilentlyContinue
  Remove-Item ($dlls + 'SNOWSYS.DLL', 'SBSANS8.FON' | ForEach-Object { Join-Path $win "SYSTEM\$_" }) -ErrorAction SilentlyContinue
  Write-Output 'Snowball removed; Program Manager is the shell again.'
  return
}

New-Item -ItemType Directory -Force $shellDir | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'SNOWBALL.EXE') $shellDir -Force
foreach ($d in $dlls) { Copy-Item (Join-Path $PSScriptRoot $d) $shellDir -Force }
Copy-Item (Join-Path $PSScriptRoot 'OPEN.COM') $win -Force
Remove-Item (@($dlls + 'SNOWSYS.DLL' | ForEach-Object { Join-Path $win "SYSTEM\$_" }) + (Join-Path $shellDir 'SNOWSYS.DLL')) -ErrorAction SilentlyContinue
& (Join-Path $PSScriptRoot 'sysfont.ps1') (Join-Path $win 'SYSTEM')

# Remember each original value once: a reinstall must not overwrite them, but
# keys added by a newer version are recorded before their first change.
$old = if (Test-Path $saved) { @(Get-Content $saved) } else { @('; values replaced by the Snowball installer, restored by install.ps1 -Uninstall') }
foreach ($file in 'SYSTEM.INI', 'WIN.INI', 'CONTROL.INI') {
  $lines = Get-Content (Join-Path $win $file)
  foreach ($c in $changes | Where-Object { $_[0] -eq $file }) {
    $v = Get-IniValue $lines $c[1] $c[2]
    if ($null -ne $v -and $v -ne $c[3] -and $null -eq (Get-IniValue $old $file "$($c[1])|$($c[2])")) {
      $old = Set-IniValue $old $file "$($c[1])|$($c[2])" $v
    }
  }
}
$v = if ($conf) { Get-IniValue (Get-Content $conf) 'dos' 'dos clipboard device enable' }
if ($null -ne $v -and $v -ne 'true' -and $null -eq (Get-IniValue $old 'w311.conf' 'dos|dos clipboard device enable')) {
  $old = Set-IniValue $old 'w311.conf' 'dos|dos clipboard device enable' $v
}
Set-Content $saved $old -Encoding ascii

foreach ($file in 'SYSTEM.INI', 'WIN.INI', 'CONTROL.INI') {
  Update-Ini $file {
    param($lines)
    foreach ($c in $changes | Where-Object { $_[0] -eq $file }) { $lines = Set-IniValue $lines $c[1] $c[2] $c[3] }
    , $lines
  }
}

if ($conf) {
  $text = Get-Content $conf -Raw
  if (-not $text.Contains(':poweroff')) {
    $text.Replace($autoexecOld, ($autoexecNew -join "`r`n")) | Set-Content $conf -Encoding ascii -NoNewline
  }
  Set-ClipboardDevice 'true'
}
Write-Output "Snowball installed in $shellDir (shell, Ctrl+Esc, Windows 98 colours and font)."
if (-not $conf) { Write-Output 'No DOSBox-X configuration given (-Conf): Shut Down and the clipboard as INFO.TXT says.' }
