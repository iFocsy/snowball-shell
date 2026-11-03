<#
  Builds SBSANS8.FON, the Windows 98 menu and dialog font (MS Sans Serif 8)
  packaged as a Windows 3.1 system font, from the fonts of this installation.
  GDI only accepts a fonts.fon holding a single font, so the container is
  VGASYS.FON (module FONTS, aspect 96x96) with its font replaced by the
  8 point face of SSERIFE.FON, renamed System.

  -SystemDir  WINDOWS\SYSTEM of the installation; the result is written there
#>
param([Parameter(Mandatory)][string]$SystemDir)
$ErrorActionPreference = 'Stop'

function U16([byte[]]$d, [int]$i) { [int][BitConverter]::ToUInt16($d, $i) }

# Table positions of the resource entries of one type (12 bytes each:
# offset, length, flags, id) and the alignment shift.
function Get-Resources([byte[]]$d, [int]$type) {
  $ne = U16 $d 0x3C
  $p = $ne + (U16 $d ($ne + 0x24))
  $shift = U16 $d $p
  $p += 2
  while (($t = U16 $d $p) -ne 0) {
    $n = U16 $d ($p + 2)
    $p += 8
    if ($t -eq $type) { return @{ Shift = $shift; Entries = @(0..($n - 1) | ForEach-Object { $p + 12 * $_ }) } }
    $p += 12 * $n
  }
  throw "resource type $type not found"
}

$box = [IO.File]::ReadAllBytes((Join-Path $SystemDir 'VGASYS.FON'))
$sans = [IO.File]::ReadAllBytes((Join-Path $SystemDir 'SSERIFE.FON'))

$res = Get-Resources $sans 0x8008
$font = $null
foreach ($e in $res.Entries) {
  $at = (U16 $sans $e) -shl $res.Shift
  if ((U16 $sans ($at + 0x44)) -eq 8) {          # dfPoints
    $font = [byte[]]$sans[$at..($at + [BitConverter]::ToUInt32($sans, $at + 2) - 1)]
    break
  }
}
if (-not $font) { throw 'SSERIFE.FON has no 8 point font' }
$face = [BitConverter]::ToUInt32($font, 0x69)
[Text.Encoding]::ASCII.GetBytes("System`0").CopyTo($font, $face)

$dirEntry = (Get-Resources $box 0x8007).Entries[0]
$res = Get-Resources $box 0x8008
$fontEntry = $res.Entries[0]
$unit = 1 -shl $res.Shift
$start = [Math]::Min((U16 $box $dirEntry), (U16 $box $fontEntry)) * $unit
$ne = U16 $box 0x3C
if ([BitConverter]::ToUInt32($box, $ne + 0x2C) -ge $start) { throw 'unexpected VGASYS.FON layout' }

# Font directory: count, ordinal, the 113-byte header, device and face names.
$dir = [byte[]]((1, 0) + [BitConverter]::GetBytes([uint16]((U16 $box ($fontEntry + 6)) -band 0x7FFF)) +
       $font[0..112] + [Text.Encoding]::ASCII.GetBytes("`0System`0"))

# Header part of VGASYS.FON, then both resources, each padded to the alignment.
$at = $start / $unit
foreach ($part in @(@($dirEntry, $dir), @($fontEntry, $font))) {
  $units = [Math]::Ceiling($part[1].Length / $unit)
  [BitConverter]::GetBytes([uint16]$at).CopyTo($box, $part[0])
  [BitConverter]::GetBytes([uint16]$units).CopyTo($box, $part[0] + 2)
  $at += $units
}
$file = New-Object byte[] ($at * $unit)
[Array]::Copy($box, $file, $start)
$dir.CopyTo($file, $start)
$font.CopyTo($file, (U16 $box $fontEntry) * $unit)
[IO.File]::WriteAllBytes((Join-Path $SystemDir 'SBSANS8.FON'), $file)
