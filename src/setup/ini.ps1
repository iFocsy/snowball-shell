# INI helpers for the install scripts. Dot-source, then:
#   $lines = Get-Content SYSTEM.INI
#   Get-IniValue $lines boot drivers
#   $lines = Set-IniValue $lines boot display.drv vbesvga.drv   # $null removes the key
# Sections and keys match case-insensitively; a missing section is appended.

function Find-IniSection([string[]]$Lines, [string]$Section) {
  $start = -1
  for ($i = 0; $i -lt $Lines.Count; $i++) {
    if ($Lines[$i] -match '^\s*\[(.+?)\]') {
      if ($start -ge 0) { return $start, $i }
      if ($Matches[1] -eq $Section) { $start = $i }
    }
  }
  return $start, $Lines.Count
}

function Get-IniValue([string[]]$Lines, [string]$Section, [string]$Key) {
  $start, $end = Find-IniSection $Lines $Section
  if ($start -lt 0) { return $null }
  for ($i = $start + 1; $i -lt $end; $i++) {
    if ($Lines[$i] -match "^\s*$([regex]::Escape($Key))\s*=(.*)$") { return $Matches[1].Trim() }
  }
  return $null
}

function Set-IniValue([string[]]$Lines, [string]$Section, [string]$Key, [AllowNull()]$Value) {
  $out = [System.Collections.Generic.List[string]]::new([string[]]$Lines)
  $start, $end = Find-IniSection $out.ToArray() $Section
  if ($start -lt 0) {
    if ($null -ne $Value) { $out.AddRange([string[]]@('', "[$Section]", "$Key=$Value")) }
    return , $out.ToArray()
  }
  for ($i = $start + 1; $i -lt $end; $i++) {
    if ($out[$i] -match "^\s*$([regex]::Escape($Key))\s*=") {
      if ($null -eq $Value) { $out.RemoveAt($i) } else { $out[$i] = "$Key=$Value" }
      return , $out.ToArray()
    }
  }
  if ($null -ne $Value) {
    while ($end -gt $start + 1 -and $out[$end - 1].Trim() -eq '') { $end-- }
    $out.Insert($end, "$Key=$Value")
  }
  return , $out.ToArray()
}
