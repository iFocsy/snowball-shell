<#
  Draws the network tray's icons into src\snshell\resources, 16 colours, at 16
  and 32 pixels as Windows 98's: two computers, their screens lit by traffic.
  -Preview: also a PNG of them enlarged, on the taskbar's grey.
#>
param([string]$Preview)
Add-Type -AssemblyName System.Drawing
$out = Join-Path $PSScriptRoot '..\src\snshell\resources'
$palette = 0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0,
           0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
$BLACK = 0; $TEAL = 6; $SILVER = 7; $GRAY = 8; $RED = 9; $YELLOW = 11; $CYAN = 14; $WHITE = 15

function New-Image($size) { $p = New-Object 'int[,]' $size, $size
  for ($y = 0; $y -lt $size; $y++) { for ($x = 0; $x -lt $size; $x++) { $p.SetValue(-1, $y, $x) } }
  , $p }

function Fill($p, $x0, $y0, $x1, $y1, $color) {
  for ($y = $y0; $y -le $y1; $y++) { for ($x = $x0; $x -le $x1; $x++) { $p.SetValue($color, $y, $x) } }
}

function Computer($p, $x, $y, $w, $h, $screen, $foot) {
  Fill $p $x $y ($x + $w - 1) ($y + $h - 1) $BLACK
  Fill $p ($x + 1) ($y + 1) ($x + $w - 2) ($y + $h - 2) $SILVER
  Fill $p ($x + 1) ($y + 1) ($x + $w - 3) ($y + 1) $WHITE
  Fill $p ($x + 1) ($y + 1) ($x + 1) ($y + $h - 3) $WHITE
  $edge = if ($w -gt 12) { 3 } else { 2 }
  Fill $p ($x + $edge) ($y + $edge) ($x + $w - 1 - $edge) ($y + $h - 1 - $edge) $screen
  $mid = $x + [int][Math]::Floor($w / 2)
  $stand = if ($w -gt 12) { 3 } else { 1 }
  Fill $p ($mid - $stand) ($y + $h) ($mid + $stand) ($y + $h + $foot - 2) $GRAY
  Fill $p ($x + 1) ($y + $h + $foot - 1) ($x + $w - 2) ($y + $h + $foot - 1) $BLACK
}

function Draw($size, $back, $front, $mark) {
  $p = New-Image $size
  if ($size -eq 16) {
    Computer $p 6 0 10 7 $back 2
    Computer $p 0 6 10 7 $front 2
  } else {
    Computer $p 12 1 19 13 $back 3
    Computer $p 1 13 19 13 $front 3
  }
  $n = if ($size -eq 16) { 7 } else { 13 }
  $x = $size - $n; $y = $size - $n
  if ($mark -eq 'x') {
    $thick = if ($size -eq 16) { 0 } else { 1 }
    for ($i = 0; $i -lt $n; $i++) {
      Fill $p ([Math]::Max($x, $x + $i - 1)) ($y + $i) ([Math]::Min($x + $n - 1, $x + $i + 1 + $thick)) ($y + $i) $RED
      Fill $p ([Math]::Max($x, $x + $n - 2 - $i - $thick)) ($y + $i) ([Math]::Min($x + $n - 1, $x + $n - $i)) ($y + $i) $RED
    }
  } elseif ($mark -eq '!') {
    for ($i = 0; $i -lt $n; $i++) {
      $half = [int][Math]::Floor(($i + 1) / 2)
      $mid = $x + [int][Math]::Floor($n / 2)
      Fill $p ($mid - $half) ($y + $i) ($mid + $half) ($y + $i) $YELLOW
    }
    $mid = $x + [int][Math]::Floor($n / 2)
    Fill $p $mid ($y + [int]($n * 0.3)) $mid ($y + $n - 3) $BLACK
    if ($size -gt 16) { Fill $p $mid ($y + $n - 2) $mid ($y + $n - 2) $YELLOW; Fill $p $mid ($y + $n - 1) $mid ($y + $n - 1) $BLACK }
  }
  , $p
}

function Image-Bytes($p, $size) {
  $s = New-Object IO.MemoryStream
  $w = New-Object IO.BinaryWriter $s
  $w.Write([UInt32]40); $w.Write([Int32]$size); $w.Write([Int32]($size * 2)); $w.Write([UInt16]1); $w.Write([UInt16]4)
  $w.Write([UInt32]0); $w.Write([UInt32]0); $w.Write([Int32]0); $w.Write([Int32]0); $w.Write([UInt32]16); $w.Write([UInt32]0)
  foreach ($c in $palette) { $w.Write([byte]($c -band 255)); $w.Write([byte](($c -shr 8) -band 255)); $w.Write([byte](($c -shr 16) -band 255)); $w.Write([byte]0) }
  for ($y = $size - 1; $y -ge 0; $y--) {
    for ($x = 0; $x -lt $size; $x += 2) {
      $a = $p.GetValue($y, $x); $b = $p.GetValue($y, $x + 1)
      if ($a -lt 0) { $a = 0 }
      if ($b -lt 0) { $b = 0 }
      $w.Write([byte]($a * 16 + $b))
    }
  }
  $row = [int](([Math]::Ceiling($size / 32.0)) * 4)
  for ($y = $size - 1; $y -ge 0; $y--) {
    $bytes = New-Object byte[] $row
    for ($x = 0; $x -lt $size; $x++) { if ($p.GetValue($y, $x) -lt 0) { $bytes[$x -shr 3] = $bytes[$x -shr 3] -bor (0x80 -shr ($x -band 7)) } }
    $w.Write($bytes)
  }
  , $s.ToArray()
}

$icons = [ordered]@{
  NETIDLE = $TEAL, $TEAL, ''; NETSEND = $TEAL, $CYAN, ''; NETRECV = $CYAN, $TEAL, ''; NETBOTH = $CYAN, $CYAN, ''
  NETDOWN = $GRAY, $GRAY, 'x'; NETWARN = $TEAL, $TEAL, '!'
}
$sheet = New-Object Drawing.Bitmap 760, 170
$g = [Drawing.Graphics]::FromImage($sheet)
$g.Clear([Drawing.Color]::FromArgb(192, 192, 192))
$left = 8
foreach ($name in $icons.Keys) {
  $spec = $icons[$name]
  $images = foreach ($size in 32, 16) { , (Image-Bytes (Draw $size $spec[0] $spec[1] $spec[2]) $size) }
  $file = New-Object IO.MemoryStream
  $w = New-Object IO.BinaryWriter $file
  $w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]2)
  $offset = 6 + 32
  foreach ($i in 0, 1) {
    $size = if ($i -eq 0) { 32 } else { 16 }
    $w.Write([byte]$size); $w.Write([byte]$size); $w.Write([byte]16); $w.Write([byte]0)
    $w.Write([UInt16]1); $w.Write([UInt16]4); $w.Write([UInt32]$images[$i].Length); $w.Write([UInt32]$offset)
    $offset += $images[$i].Length
  }
  foreach ($image in $images) { $w.Write([byte[]]$image) }
  [IO.File]::WriteAllBytes((Join-Path $out "$name.ICO"), $file.ToArray())
  if ($Preview) {
    foreach ($size in 16, 32) {
      $p = Draw $size $spec[0] $spec[1] $spec[2]
      $k = if ($size -eq 16) { 4 } else { 3 }
      $top = if ($size -eq 16) { 8 } else { 76 }
      for ($y = 0; $y -lt $size; $y++) { for ($x = 0; $x -lt $size; $x++) {
        $v = $p.GetValue($y, $x)
        if ($v -ge 0) {
          $c = $palette[$v]
          $brush = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(($c -shr 16) -band 255, ($c -shr 8) -band 255, $c -band 255))
          $g.FillRectangle($brush, $left + $x * $k, $top + $y * $k, $k, $k)
          $brush.Dispose()
        }
      } }
    }
    $left += 124
  }
}
if ($Preview) { $sheet.Save($Preview, [Drawing.Imaging.ImageFormat]::Png) }
