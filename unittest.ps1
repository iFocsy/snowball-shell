<#
  Runs build\unittest\UNITTEST.EXE (build.ps1 makes it): checks of SNSHELL's
  core\ functions, under winevdm on this machine (no Windows 3.1 needed).
  Prints each failed check; exits 1 if any failed.
    -Auto     never asks for winevdm (tools\buildtools.ps1)
    -Seconds  how long it may take (60)
#>
param([switch]$Auto, [int]$Seconds = 60)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'tools\buildtools.ps1')
$dir = Join-Path $PSScriptRoot 'build\unittest'
if (-not (Test-Path "$dir\UNITTEST.EXE")) { throw 'build\unittest\UNITTEST.EXE is missing: run build.ps1 first.' }
$vdm = Get-BuildTool winevdm -Auto:$Auto
if (-not $vdm) { throw 'winevdm is needed: run unittest.ps1 in a console to give its folder or download it.' }

# It loads SNSYSTEM.DLL from its folder, writes UNITTEST.LOG there and its
# files in WORK.
Copy-Item (Join-Path $PSScriptRoot 'build\snsystem\SNSYSTEM.DLL') $dir -Force
Remove-Item "$dir\UNITTEST.LOG", "$dir\WORK" -Recurse -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$dir\WORK" | Out-Null
$p = Start-Process "$vdm\otvdmw.exe" -ArgumentList "$dir\UNITTEST.EXE" -WorkingDirectory $dir -WindowStyle Minimized -PassThru
if (-not $p.WaitForExit($Seconds * 1000)) {
  Stop-Process -Id $p.Id -Force
  throw 'UNITTEST.EXE did not end.'
}
# (winevdm does not pass a program's exit code on: its log's last line counts)
$log = @(Get-Content "$dir\UNITTEST.LOG" -ErrorAction SilentlyContinue)
$log | ForEach-Object { Write-Output $_ }
if (-not $log -or $log[-1] -notmatch '^\d+ checks, 0 failed$') { exit 1 }
