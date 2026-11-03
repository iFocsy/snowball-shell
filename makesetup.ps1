<#
  Makes setup\SNOWSET.EXE, Snowball's Setup, after build.ps1: puts
  src\setup\SNOWBALL.ISS and the files it names in build\setup\stage, then
  compiles it with Inno Setup 1.2.16's 16-bit edition (as Calmira's was;
  Inno Setup 1.3 dropped 16-bit setups). That compiler runs only inside
  Windows 3.1, or under winevdm. tools\buildtools.ps1 finds or sets up
  them in build-tools\ (-Auto: never asks). Where tools\compilesetup.ps1 cannot run it,
  copy the stage into a Windows 3.1 with Inno Setup 1.2.16, run
  COMPIL16 /cc SNOWBALL.ISS there, and take OUT\SNOWSET.EXE.
#>
param([switch]$Auto)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
. (Join-Path $root 'tools\buildtools.ps1')

$stage = Join-Path $root 'build\setup\stage'
Remove-Item $stage -Recurse -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $stage | Out-Null
foreach ($f in 'SNOWBALL.EXE', 'SNSHELL.DLL', 'SNSYSTEM.DLL', 'W98ART.DLL', 'SBSETUP.EXE', 'OPEN.COM', 'LFN.DRV') {
  if (-not (Test-Path (Join-Path $root "release\$f"))) { throw "release\$f is missing: run build.ps1 first." }
  Copy-Item (Join-Path $root "release\$f") $stage
}
Copy-Item (Join-Path $root 'LICENSE') "$stage\LICENSE.TXT"
Copy-Item (Join-Path $root 'redist\LFN.TXT'), "$root\src\setup\INFO.TXT", "$root\src\setup\SNOWBALL.ISS" $stage

# Inno Setup's compiler runs under winevdm, or else in Windows 3.1 under DOSBox-X
$inno = Get-BuildTool inno -Auto:$Auto
$made = if ($inno) {
  $vdm = Get-BuildTool winevdm -Auto:$Auto
  if ($vdm) {
    & (Join-Path $root 'tools\compilesetup.ps1') -Stage $stage -Inno $inno -Otvdm "$vdm\otvdmw.exe"
  } else {
    $dosbox = Get-BuildTool dosbox -Auto:$Auto
    & (Join-Path $root 'tools\compilesetup.ps1') -Stage $stage -Inno $inno -WinDrive (Get-BuildTool win31 -Auto:$Auto) `
      -Conf (Get-BuildTool dosboxconf -Auto:$Auto) -DosBoxX $(if ($dosbox) { "$dosbox\dosbox-x.exe" })
  }
}
if (-not $made) {
  Write-Output "Staged in ${stage}: compile SNOWBALL.ISS there in Windows 3.1 (COMPIL16 /cc)."
  return
}
New-Item -ItemType Directory -Force (Join-Path $root 'setup') | Out-Null
Copy-Item $made (Join-Path $root 'setup') -Force
Get-Item (Join-Path $root 'setup\SNOWSET.EXE') | Select-Object Name, Length
