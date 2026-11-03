<#
  The tools the build uses, kept in build-tools\<name> (not in the
  repository), or where build-tools\paths.ini says this machine has them.
  Dot-sourced by build.ps1 and makesetup.ps1:
    watcom      Open Watcom 2.0, with its 16-bit Windows target   build.ps1
    inno        Inno Setup 1.2.16, its 16-bit edition               makesetup.ps1
    winevdm     runs Inno Setup's 16-bit compiler on 64-bit Windows makesetup.ps1
    dosbox      DOSBox-X, where there is no winevdm                 makesetup.ps1
    win31       a Windows 3.1 installation for DOSBox-X (the folder holding WINDOWS)
    dosboxconf  a DOSBox-X configuration that runs it
  Each is looked for in paths.ini, then build-tools\<name>, then where it
  usually is (kept in paths.ini when found there). A missing one is asked for:
  its folder (kept in paths.ini), or (the first four) a download into
  build-tools\<name>. With -Auto, or without a console, nothing is asked.
#>

$ToolsDir = Join-Path (Split-Path $PSScriptRoot) 'build-tools'
$ToolsIni = Join-Path $ToolsDir 'paths.ini'

$BuildTools = [ordered]@{
  watcom = @{
    What  = 'Open Watcom 2.0, with its 16-bit Windows target'
    File  = 'h\win\windows.h'
    Usual = { $env:WATCOM; 'C:\WATCOM' }
    Size  = 'about 150 MB to download'
    Get   = {
      param($dir)
      $xz = Join-Path $ToolsDir 'downloads\ow-snapshot.tar.xz'
      Save-Download 'https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/ow-snapshot.tar.xz' $xz
      New-Item -ItemType Directory -Force $dir | Out-Null
      # only what a 16-bit Windows build on Windows uses: tools, headers, 16-bit libraries
      tar -xf $xz -C $dir --include 'binnt64/*' --include 'binnt/*' --include 'h/*' --include 'lib286/*' `
          --include './binnt64/*' --include './binnt/*' --include './h/*' --include './lib286/*'
    }
  }
  inno = @{
    What  = 'Inno Setup 1.2.16, its 16-bit edition'
    File  = 'COMPIL16.EXE'
    Usual = { }
    Size  = 'about 1.2 MB, and innoextract to unpack it'
    Get   = {
      param($dir)
      # its own setup is a 16-bit program: innoextract unpacks it instead
      $exe = Join-Path $ToolsDir 'downloads\isetup16-1.2.16.exe'
      Save-Download 'https://files.jrsoftware.org/is/1.2/isetup16-1.2.16.exe' $exe
      $zip = Join-Path $ToolsDir 'downloads\innoextract-1.9-windows.zip'
      Save-Download 'https://github.com/dscharrer/innoextract/releases/download/1.9/innoextract-1.9-windows.zip' $zip
      $ie = Join-Path $ToolsDir 'downloads\innoextract'
      Expand-Archive $zip $ie -Force
      $tmp = Join-Path $ToolsDir 'downloads\inno'
      Remove-Item $tmp -Recurse -ErrorAction SilentlyContinue
      & (Join-Path $ie 'innoextract.exe') -s -e -d $tmp $exe
      Move-Item (Join-Path $tmp 'app') $dir
      Remove-Item $tmp -Recurse
    }
  }
  winevdm = @{
    What  = 'winevdm (runs 16-bit Windows programs on 64-bit Windows)'
    File  = 'otvdmw.exe'
    Usual = { $c = Get-Command otvdmw.exe -ErrorAction SilentlyContinue; if ($c) { Split-Path $c.Source } }
    Size  = 'about 2 MB'
    Get   = {
      param($dir)
      $zip = Join-Path $ToolsDir 'downloads\otvdm-v0.9.0.zip'
      Save-Download 'https://github.com/otya128/winevdm/releases/download/v0.9.0/otvdm-v0.9.0.zip' $zip
      Expand-Folder $zip 'otvdmw.exe' $dir
    }
  }
  dosbox = @{
    What  = 'DOSBox-X'
    File  = 'dosbox-x.exe'
    Usual = { $c = Get-Command dosbox-x.exe -ErrorAction SilentlyContinue; if ($c) { Split-Path $c.Source } }
    Size  = 'about 45 MB'
    Get   = {
      param($dir)
      $zip = Join-Path $ToolsDir 'downloads\dosbox-x-vsbuild-win64-2026.10.01-portable.zip'
      Save-Download 'https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v2026.10.01/dosbox-x-vsbuild-win64-2026.10.01-portable.zip' $zip
      Expand-Folder $zip 'dosbox-x.exe' $dir
    }
  }
  win31 = @{
    What  = 'a Windows 3.1 installation for DOSBox-X (the folder holding WINDOWS)'
    File  = 'WINDOWS\WIN.COM'
    Usual = { }
  }
  dosboxconf = @{
    What  = 'a DOSBox-X configuration file that runs that Windows'
    IsFile = $true
    Usual = { }
  }
}

function Save-Download([string]$url, [string]$file) {
  if (Test-Path $file) { return }
  New-Item -ItemType Directory -Force (Split-Path $file) | Out-Null
  Write-Host "Downloading $url"
  $ProgressPreference = 'SilentlyContinue'
  Invoke-WebRequest $url -OutFile "$file.part" -UseBasicParsing
  Move-Item "$file.part" $file
}

# A zip's folder that holds $exe (wherever it is inside) becomes $dir.
function Expand-Folder([string]$zip, [string]$exe, [string]$dir) {
  $tmp = "$dir.unzip"
  Remove-Item $tmp -Recurse -ErrorAction SilentlyContinue
  Expand-Archive $zip $tmp -Force
  $found = Get-ChildItem $tmp -Recurse -Filter $exe | Select-Object -First 1
  Move-Item $found.DirectoryName $dir
  Remove-Item $tmp -Recurse -ErrorAction SilentlyContinue
}

function Read-ToolPaths {
  $paths = @{}
  if (Test-Path $ToolsIni) {
    foreach ($l in Get-Content $ToolsIni) { if ($l -match '^\s*(\w+)\s*=\s*(.+?)\s*$') { $paths[$Matches[1]] = $Matches[2] } }
  }
  $paths
}

function Save-ToolPath([string]$name, [string]$dir) {
  $paths = Read-ToolPaths
  $paths[$name] = $dir
  New-Item -ItemType Directory -Force $ToolsDir | Out-Null
  $lines = @('; Where this machine keeps the tools Snowball''s build uses (tools\buildtools.ps1)')
  foreach ($k in $paths.Keys | Sort-Object) { $lines += "$k=$($paths[$k])" }
  Set-Content $ToolsIni $lines -Encoding ascii
}

function Test-Tool([string]$name, [string]$dir) {
  $t = $BuildTools[$name]
  try {                                 # a mistyped path: not there
    if ($t.IsFile) { $dir -and (Test-Path $dir -PathType Leaf) } else { $dir -and (Test-Path (Join-Path $dir $t.File)) }
  } catch { $false }
}

# The tool's folder (dosboxconf: the file); asked for when missing (unless
# -Auto); $null when not there.
function Get-BuildTool([string]$name, [switch]$Auto) {
  $t = $BuildTools[$name]
  $dir = (Read-ToolPaths)[$name]
  if (Test-Tool $name $dir) { return $dir }
  $mine = Join-Path $ToolsDir $name
  if (Test-Tool $name $mine) { return $mine }
  foreach ($d in & $t.Usual) {
    if (Test-Tool $name $d) { Save-ToolPath $name $d; return $d }
  }
  if ($Auto -or [Console]::IsInputRedirected -or -not [Environment]::UserInteractive) { return $null }

  Write-Host ''
  Write-Host "$($t.What) was not found."
  Write-Host "  1  give its $(if ($t.IsFile) { 'file' } else { 'folder' })"
  if ($t.Get) { Write-Host "  2  download it into build-tools\$name ($($t.Size))" }
  Write-Host "  3  go on without it"
  $choice = Read-Host 'Choose'
  if ($choice -eq '1') {
    $d = (Read-Host 'Where').Trim('"', ' ')
    if (-not (Test-Tool $name $d)) { Write-Host "Not found there: $d"; return $null }
    Save-ToolPath $name $d
    return $d
  }
  if ($choice -eq '2' -and $t.Get) {
    & $t.Get $mine
    if (-not (Test-Tool $name $mine)) { Write-Host "$name was not set up in $mine."; return $null }
    return $mine
  }
  $null
}
