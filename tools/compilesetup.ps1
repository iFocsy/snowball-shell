<#
  Compiles a staged SNOWBALL.ISS with Inno Setup 1.2.16's 16-bit COMPIL16:
  under winevdm (otvdmw.exe, which runs 16-bit Windows programs on 64-bit
  Windows) when it is given, else in Windows 3.1 under DOSBox-X (runwin.ps1:
  the stage D:, Inno Setup E:). Returns SNOWSET.EXE's path; nothing when
  neither can run it. makesetup.ps1 passes what tools\buildtools.ps1 found.
  -Stage  the folder makesetup.ps1 made
  -Inno   Inno Setup 1.2.16 (16-bit)'s folder
  -Otvdm  otvdmw.exe
  Others go to runwin.ps1 (-WinDrive, -Conf, -DosBoxX).
#>
param([Parameter(Mandatory)][string]$Stage, [string]$Inno, [string]$Otvdm, [string]$WinDrive, [string]$Conf,
      [string]$DosBoxX)
$ErrorActionPreference = 'Stop'
if (-not $Inno -or -not (Test-Path "$Inno\COMPIL16.EXE")) { Write-Host 'compilesetup: no Inno Setup 1.2.16.'; return }
$out = Join-Path $Stage 'OUT\SNOWSET.EXE'
Remove-Item $out -ErrorAction SilentlyContinue

if ($Otvdm -and (Test-Path $Otvdm)) {
  # A 16-bit program sees DOS paths, at most 64 characters: the stage becomes a
  # drive of its own for a while (subst), Inno Setup's files in it.
  Copy-Item $Inno (Join-Path $Stage 'ISETUP') -Recurse -Force
  $d = ([char[]](90..68) | Where-Object { -not (Test-Path "${_}:\") } | Select-Object -First 1) + ':'
  subst $d $Stage
  try {
    # an error leaves its message up, so it gets 3 minutes
    $p = Start-Process $Otvdm -ArgumentList "$d\ISETUP\COMPIL16.EXE", '/cc', "$d\SNOWBALL.ISS" -WindowStyle Minimized -PassThru
    if (-not $p.WaitForExit(180000)) {
      Stop-Process -Id $p.Id -Force
      throw 'COMPIL16 did not finish under winevdm (its error message stayed up).'
    }
  } finally {
    subst $d /d
  }
  if ($p.ExitCode -or -not (Test-Path $out)) { throw "COMPIL16 failed under winevdm (exit $($p.ExitCode))." }
  return (Resolve-Path $out).Path
}

# The compiler closes itself when done; an error leaves its message up.
$steps = 'wait 2000', 'run E:\COMPIL16.EXE /cc D:\SNOWBALL.ISS', 'wait 45000', 'windows all', 'snap t:\SETUP.BMP',
         'wait 3000', 'exitwindows'
$t = & (Join-Path $PSScriptRoot 'runwin.ps1') -Steps $steps -Mount "D=$Stage", "E=$Inno" -WinDrive $WinDrive -Conf $Conf -DosBoxX $DosBoxX
if (-not $t) { return }
if (-not (Test-Path $out)) {
  Get-Content "$t\UIDRIVE.LOG" -ErrorAction SilentlyContinue | Select-String "'.+'" | ForEach-Object { Write-Host $_.Line }
  throw "SNOWSET.EXE was not made: see $t\SETUP.BMP"
}
$out
