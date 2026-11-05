<#
  Sets Snowball's version (vYYYYMMDD) everywhere it is written: the code's
  SB_VERSION, the binaries' version resource, Setup's name and the README's
  badge (and the maintainer's notes, when there). Run it before a commit that
  makes a new version, then build.

    .\bumpversion.ps1                today's date: v20261104
    .\bumpversion.ps1 v20261231      that version
    .\bumpversion.ps1 -Show          the version now, changing nothing
#>
param(
  [string]$Version = ('v' + (Get-Date -Format 'yyyyMMdd')),
  [switch]$Show
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$header = Join-Path $root 'src\include\snowball.h'
$now = [regex]::Match((Get-Content $header -Raw), '#define SB_VERSION\s+"(v\d{8})"').Groups[1].Value
if ($Show) { return $now }
if ($Version -notmatch '^v(\d{4})(\d{2})(\d{2})$') { throw "A version is v and a date, such as v20261104: not $Version" }
$numbers = '{0}, {1}, {2}, 0' -f [int]$Matches[1], [int]$Matches[2], [int]$Matches[3]

# Each file and its replacements; every byte else stays (read and written as
# Latin-1, which keeps any byte as it is), line endings too.
$bytes = [Text.Encoding]::GetEncoding(28591)
$edits = [ordered]@{
  'src\include\snowball.h'   = @(, @('(#define SB_VERSION\s+)"v\d{8}"', ('$1"' + $Version + '"')))
  'src\include\version.rc'   = @(@('v\d{8}', $Version), @('((?:FILE|PRODUCT)VERSION\s+)\d+, \d+, \d+, \d+', ('${1}' + $numbers)))
  'src\setup\SNOWBALL.ISS'   = @(, @('(AppVerName=Snowball )v\d{8}', ('${1}' + $Version)))
  'README.md'                = @(, @('v\d{8}', $Version))
  'technical\DEVELOPMENT.md' = @(, @('(version \*\*)v\d{8}', ('${1}' + $Version)))
}
foreach ($file in $edits.Keys) {
  $path = Join-Path $root $file
  if (-not (Test-Path $path)) { continue }               # (the notes are the maintainer's only)
  $text = [IO.File]::ReadAllText($path, $bytes)
  $new = $text
  foreach ($e in $edits[$file]) { $new = [regex]::Replace($new, $e[0], $e[1]) }
  if ($new -ne $text) {
    [IO.File]::WriteAllText($path, $new, $bytes)
    "$file"
  }
}
"$now -> $Version"
