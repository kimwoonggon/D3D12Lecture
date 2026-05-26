$file = 'c:\Users\gerard\source\repos\D3D12Lecture\13_Sprite\ask.md'
$lines = Get-Content $file
$inBlock = $false
# Print state at every 500 lines
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^```') {
        $inBlock = -not $inBlock
        $state = if ($inBlock) { "OPEN " } else { "CLOSE" }
        Write-Host "$state line $($i+1): $($lines[$i])"
    }
}
