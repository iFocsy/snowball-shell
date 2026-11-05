<#
  Assert-NoEarlySegmentLoads: fails the build when a function with an LPARAM
  parameter loads a segment register before its first conditional jump.
  Watcom (-os) may load lParam as a far pointer (les bx,...) before the
  switch on the message, when some cases use it as one; for a message whose
  lParam is no selector (WM_COMMAND's EN_CHANGE...) that is a GP fault in
  protected mode, on real machines as in DOSBox-X. The cure: dereference
  lParam in a helper function, which gets it in registers.
  Dot-sourced by build.ps1; wdis must be on PATH.
#>

function Assert-NoEarlySegmentLoads([string]$sources, [string]$objects) {
  $names = @{}
  Get-ChildItem $sources -Filter *.c -Recurse |
    Select-String -Pattern '^(?:static\s+)?[A-Za-z_][\w \*]*?\b(\w+)\s*\(([^)]*\bLPARAM\b[^)]*)\)\s*$' |
    ForEach-Object { $names[$_.Matches[0].Groups[1].Value.ToUpper()] = $_.Filename }
  $found = foreach ($obj in Get-ChildItem $objects -Filter *.obj) {
    $func = $null
    foreach ($line in (wdis $obj.FullName)) {
      if ($line -match '^[0-9A-F]{4}\s+([A-Za-z_]\w*):\s*$') {
        # register calling convention: Name_; pascal (exported): NAME
        $func = if ($names.ContainsKey($Matches[1].TrimEnd('_').ToUpper())) { $Matches[1] } else { $null }
        continue
      }
      if (-not $func) { continue }
      $parts = @($line -split "`t" | Where-Object { $_.Trim() })
      if ($parts.Count -lt 2) { continue }
      $op = $parts[1].Trim()
      $operands = if ($parts.Count -gt 2) { $parts[2].Trim() } else { '' }
      if ($op -in 'les', 'lds' -or ($op -eq 'mov' -and $operands -like 'es,*')) {
        "$func ($($obj.Name)): $op $operands"
        $func = $null
      } elseif ($op -like 'j*' -or $op -like 'call*' -or $op -like 'ret*') {
        $func = $null                   # past the entry: the message is known
      }
    }
  }
  if ($found) {
    throw ("A segment register is loaded before the switch on the message (a GP fault when lParam " +
           "is no pointer; dereference it in a helper):`n  " + ($found -join "`n  "))
  }
}
