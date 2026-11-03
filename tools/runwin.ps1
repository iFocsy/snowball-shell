<#
  Runs Windows 3.1 in DOSBox-X with a UIDRIVE script (build\uidrive, made by
  build.ps1) and waits for Windows to end. C: is a copy of the Windows drive
  (build\runwin\c), so the installation itself never changes. Returns the
  folder that was T: (UIDRIVE.LOG, screenshots); nothing when DOSBox-X or
  Windows is not found.
  -Steps     UIDRIVE's script, one command a line
  -Mount     more drives: "X=host folder"
  -WinDrive  the folder holding Windows 3.1 (its WINDOWS\WIN.COM)
  -Conf      a DOSBox-X configuration that runs it; its [autoexec] is replaced
  -DosBoxX   dosbox-x.exe (default: on the PATH)
  -Wait      seconds before DOSBox-X is stopped (default 900)
#>
param([Parameter(Mandatory)][string[]]$Steps, [string[]]$Mount = @(), [string]$WinDrive, [string]$Conf,
      [string]$DosBoxX, [int]$Wait = 900)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path
if (-not $DosBoxX) { $DosBoxX = (Get-Command dosbox-x.exe -ErrorAction SilentlyContinue).Source }
$uidrive = Join-Path $root 'build\uidrive\UIDRIVE.EXE'
foreach ($need in @(@([string]$DosBoxX, 'DOSBox-X'), @("$WinDrive\WINDOWS\WIN.COM", 'Windows 3.1 (-WinDrive)'),
                    @($Conf, 'a DOSBox-X configuration (-Conf)'), @($uidrive, 'UIDRIVE.EXE (build.ps1)'))) {
  if (-not $need[0] -or $need[0] -like '\*' -or -not (Test-Path $need[0])) { Write-Host "runwin: $($need[1]) not found ($($need[0]))."; return }
}

$work = Join-Path $root 'build\runwin'
$c = Join-Path $work 'c'
$t = Join-Path $work 't'
New-Item -ItemType Directory -Force $c, $t | Out-Null
robocopy $WinDrive $c /MIR /NFL /NDL /NJH /NJS /NP /R:0 /W:0 | Out-Null
if ($LASTEXITCODE -ge 8) { throw "copying $WinDrive failed ($LASTEXITCODE)" }
# 800x600 (a small window) whatever mode the VBE SVGA driver is set to
. (Join-Path $root 'src\setup\ini.ps1')
$sysIni = Join-Path $c 'WINDOWS\SYSTEM.INI'
$ini = Get-Content $sysIni
if (Get-IniValue $ini 'vbesvga.drv' 'Width') {
  $ini = Set-IniValue $ini 'vbesvga.drv' 'Width' '800'
  Set-Content $sysIni (Set-IniValue $ini 'vbesvga.drv' 'Height' '600') -Encoding ascii
}
Remove-Item "$t\*" -Recurse -ErrorAction SilentlyContinue
Copy-Item $uidrive $t
$Steps | Set-Content "$t\UIDRIVE.TXT" -Encoding Default       # ANSI, as Windows reads it

# The configuration up to its [autoexec], then ours: the drives, Windows with UIDRIVE.
$lines = @(Get-Content $Conf)
$cut = [Array]::IndexOf($lines, '[autoexec]')
if ($cut -ge 0) { $lines = $lines[0..($cut - 1)] }
$lines += '[autoexec]', '@echo off', "mount c `"$c`"", "mount t `"$t`""
foreach ($m in $Mount) {
  $d, $path = $m -split '=', 2
  $lines += "mount $d `"$((Resolve-Path $path).Path)`""
}
$lines += 'set PATH=%PATH%;C:\WINDOWS', 'set TEMP=C:\WINDOWS\TEMP', 'c:', 'win t:\uidrive.exe', 'exit'
Set-Content "$work\run.conf" $lines -Encoding ascii

# Minimized at 1x, the mouse never grabbed, the host's clipboard never touched.
$p = Start-Process $DosBoxX -ArgumentList "-conf `"$work\run.conf`"", '-set "dosbox title=Snowball - closes by itself"',
       '-set "sdl windowresolution=original"', '-set "sdl autolock=false"', '-set "dos clipboard device enable=false"' `
       -WorkingDirectory (Split-Path $DosBoxX) -WindowStyle Minimized -PassThru
if (-not $p.WaitForExit($Wait * 1000)) {
  Stop-Process -Id $p.Id -Force
  Write-Host "runwin: stopped after $Wait s."
}
$t
