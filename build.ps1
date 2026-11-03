# Builds Snowball natively with Open Watcom, each binary from its folder of
# src\ (every .c file in it and below is compiled):
#   SNSYSTEM.DLL  src\snsystem  large model, 386: in every program
#   SNSHELL.DLL   src\snshell   large model: its 64 KB data segment holds only
#                               small variables and the local heap
#   SNOWBALL.EXE  src\snowball  medium model: the task, its stack; loads the rest
#   SBSETUP.EXE   src\setup     what Setup cannot do itself: INI files,
#                               the system font, long file names, uninstalling
#   OPEN.COM      src\open      a DOS program: DOS boxes open things through Snowball
#   UIDRIVE.EXE   tools\uidrive drives Windows from a script (making the setup,
#                               the tests); stays in build\uidrive, not shipped
#   UNITTEST.EXE  src\unittest  checks of SNSHELL's core\, with SNSHELL's sources
#                               but snshell.c; unittest.ps1 runs it; not shipped
# Intermediates: build\<binary>. Deliverables: release\, with redist\'s
# W98ART.DLL (Windows 98's pictures; ..\w98icon builds it) and LFN.DRV.
# Needs Open Watcom 2.0 with its 16-bit Windows target, which
# tools\buildtools.ps1 finds or sets up in build-tools\watcom (-Auto: never
# asks); its tools are put on PATH here.
param([switch]$Auto)
$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'tools\buildtools.ps1')
$env:WATCOM = Get-BuildTool watcom -Auto:$Auto
if (-not $env:WATCOM) {
  throw ('Open Watcom 2.0 is needed (https://github.com/open-watcom/open-watcom-v2/releases): run build.ps1 ' +
         'in a console to give its folder or download it.')
}
$env:PATH = "$env:WATCOM\binnt64;$env:WATCOM\binnt;$env:PATH"
$env:INCLUDE = "$env:WATCOM\h;$env:WATCOM\h\win"

$src = Join-Path $PSScriptRoot 'src'
$redist = Join-Path $PSScriptRoot 'redist'
$build = Join-Path $PSScriptRoot 'build'
$release = Join-Path $PSScriptRoot 'release'
# All warnings are errors (a missing prototype means a wrong calling convention);
# W303 unused parameter is off (fixed Windows callback signatures).
$common = @('-q', '-bt=windows', '-wx', '-we', '-wcd=303', '-s', '-os', "-i=$src\include")
# Libraries: far data, SS is the caller's stack (-zu), Windows entry code (-zw).
$dll = @('-ml', '-bd', '-zu', '-zw')

# Sources: src\<name>'s, unless given.
function Compile([string]$name, [string[]]$flags, [IO.FileInfo[]]$sources) {
  $out = Join-Path $build $name
  New-Item -ItemType Directory -Force $out | Out-Null
  Remove-Item "$out\*.obj" -ErrorAction SilentlyContinue
  if (-not $sources) { $sources = Get-ChildItem (Join-Path $src $name) -Filter *.c -Recurse }
  # wcc looks in the current folder before the include paths: not a source folder
  Push-Location $out
  try {
    $objs = foreach ($c in $sources) {
      wcc @common @flags "-fo=$out\$($c.BaseName).obj" $c.FullName
      if ($LASTEXITCODE) { throw "wcc $($c.Name) failed" }
      "$out\$($c.BaseName).obj"
    }
  } finally {
    Pop-Location
  }
  $objs -join ','
}

# The shell's resources folder is in the include path of both scripts.
function Resources([string]$dir, [string]$rc, [string]$res) {
  Push-Location $dir
  wrc -q -r -bt=windows "-i=$src\snshell\resources" $rc "-fo=$res"
  $rcode = $LASTEXITCODE
  Pop-Location
  if ($rcode) { throw "wrc $rc failed" }
}

function Link([string]$name, [string]$objs, [string]$binary) {
  Push-Location (Join-Path $build $name)
  wlink '@' (Join-Path $src "$name\$name.lnk") file $objs libpath (Join-Path $build 'lib')
  $rcode = $LASTEXITCODE
  Pop-Location
  if ($rcode) { throw "wlink $binary failed" }
}

function ImportLibrary([string]$name, [string]$binary) {
  $lib = Join-Path $build 'lib'
  New-Item -ItemType Directory -Force $lib | Out-Null
  Remove-Item "$lib\$name.lib" -ErrorAction SilentlyContinue
  wlib -q -n "$lib\$name.lib" "+$(Join-Path $build "$name\$binary")"
  if ($LASTEXITCODE) { throw "wlib $name.lib failed" }
}

New-Item -ItemType Directory -Force $build, $release | Out-Null

$objs = Compile 'snsystem' ($dll + @('-3', '-dSNSYSTEM_DLL', "-i=$src\snsystem"))
Link 'snsystem' $objs 'SNSYSTEM.DLL'
ImportLibrary 'snsystem' 'SNSYSTEM.DLL'

# Strings in the code segments (-zc), objects over 256 bytes in their own (-zt).
$shell = @('-zc', '-zt256', '-dSNSHELL_DLL', "-i=$src\snsystem", "-i=$src\snshell")
$objs = Compile 'snshell' ($dll + $shell)
Resources (Join-Path $src 'snshell\resources') 'snshell.rc' (Join-Path $build 'snshell\snshell.res')
Link 'snshell' $objs 'SNSHELL.DLL'
ImportLibrary 'snshell' 'SNSHELL.DLL'

# The same sources as a program (no -bd), its entry unittest.c's instead of snshell.c's
$objs = Compile 'unittest' (@('-ml', '-zu', '-zw') + $shell) `
  (@(Get-ChildItem (Join-Path $src 'snshell') -Filter *.c -Recurse | Where-Object Name -ne 'snshell.c') +
   @(Get-ChildItem (Join-Path $src 'unittest') -Filter *.c))
Link 'unittest' $objs 'UNITTEST.EXE'

$objs = Compile 'snowball' @('-mm', '-zws', "-i=$src\snshell")
Resources (Join-Path $src 'snowball') 'snowball.rc' (Join-Path $build 'snowball\snowball.res')
Link 'snowball' $objs 'SNOWBALL.EXE'

$objs = Compile 'setup' @('-ms', '-zws')
Resources (Join-Path $src 'setup') 'sbsetup.rc' (Join-Path $build 'setup\sbsetup.res')
Link 'setup' $objs 'SBSETUP.EXE'

# UIDRIVE.EXE (tools\uidrive): drives Windows from a script, for
# tools\runwin.ps1 (making the setup) and the tests; not shipped.
New-Item -ItemType Directory -Force "$build\uidrive" | Out-Null
Push-Location "$build\uidrive"
wcl -q -bt=windows -l=windows "$PSScriptRoot\tools\uidrive\uidrive.c" ddeml.lib toolhelp.lib mmsystem.lib '-fe=UIDRIVE.EXE'
$rcode = $LASTEXITCODE
Pop-Location
if ($rcode) { throw 'wcl UIDRIVE.EXE failed' }

# OPEN.COM: tiny model, DOS's own headers
New-Item -ItemType Directory -Force "$build\open" | Out-Null
Push-Location "$build\open"
$include = $env:INCLUDE
$env:INCLUDE = "$env:WATCOM\h"
wcl -q -bt=dos -mt -bcl=com -wx -we -s -os "$src\open\open.c" '-fe=OPEN.COM'
$rcode = $LASTEXITCODE
$env:INCLUDE = $include
Pop-Location
if ($rcode) { throw 'wcl OPEN.COM failed' }

Remove-Item "$release\SNOWSYS.DLL" -ErrorAction SilentlyContinue
Copy-Item "$build\snowball\SNOWBALL.EXE", "$build\snshell\SNSHELL.DLL", "$build\snsystem\SNSYSTEM.DLL", "$build\open\OPEN.COM",
  "$build\setup\SBSETUP.EXE", "$redist\W98ART.DLL", "$redist\LFN.DRV" $release -Force
Copy-Item "$src\setup\install.ps1", "$src\setup\sysfont.ps1", "$src\setup\ini.ps1" $release -Force
Get-Item "$release\SNOWBALL.EXE", "$release\SNSHELL.DLL", "$release\SNSYSTEM.DLL", "$release\W98ART.DLL", "$release\SBSETUP.EXE" |
  Select-Object Name, Length
