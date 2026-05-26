$file = 'c:\Users\gerard\source\repos\D3D12Lecture\13_Sprite\ask.md'
$lines = Get-Content $file
$inBlock = $false
$openLine = 0
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    if ($line -match '^```') {
        if ($inBlock) {
            $inBlock = $false
        } else {
            $inBlock = $true
            $openLine = $i + 1
        }
    }
}
if ($inBlock) { Write-Host "UNCLOSED block starting at line $openLine" }
else { Write-Host "All code blocks balanced OK" }
Write-Host "Total lines: $($lines.Count)"
