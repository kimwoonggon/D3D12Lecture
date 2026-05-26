$file = 'c:\Users\gerard\source\repos\D3D12Lecture\13_Sprite\ask.md'
$lines = Get-Content $file
$inBlock = $false
$openLine = 0
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    if ($line -match '^```') {
        if ($inBlock) {
            $inBlock = $false
            # Write-Host "CLOSE line $($i+1)"
        } else {
            $inBlock = $true
            $openLine = $i + 1
            # Write-Host "OPEN  line $($i+1): $line"
        }
    }
}
if ($inBlock) { Write-Host "UNCLOSED block opening at line $openLine" }
else { Write-Host "All code blocks balanced OK" }

# Also count total opens
$count = 0
foreach ($l in $lines) { if ($l -match '^```') { $count++ } }
Write-Host "Total fence lines: $count (should be even)"

# Show fence lines near the end
Write-Host "`n--- Fence lines from line 2990 onwards ---"
for ($i = 2989; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^```') {
        Write-Host "Line $($i+1): $($lines[$i])"
    }
}
