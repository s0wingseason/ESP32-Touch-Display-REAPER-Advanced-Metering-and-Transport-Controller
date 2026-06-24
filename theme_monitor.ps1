
# MeterBridge Theme Monitor
# Polls every 6s. Saves a new screenshot only if the display changed (hash diff).

$esp = "http://192.168.1.180:8099/screenshot"
$outDir = "C:\dev\mb\theme_captures"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

$tmpBmp  = "C:\dev\mb\_monitor_tmp.bmp"
$tmpPng  = "C:\dev\mb\_monitor_tmp.png"
$lastHash = ""
$saved   = 0
$polls   = 0
$MaxPolls = 60  # ~6 minutes maximum

Write-Host "Monitoring started. Polling every 6s. Save only on change. Ctrl+C to stop."
Write-Host "Output: $outDir"
Write-Host ""

while ($polls -lt $MaxPolls) {
    try {
        Invoke-WebRequest -Uri $esp -OutFile $tmpBmp -TimeoutSec 10 -ErrorAction Stop

        $hash = (Get-FileHash $tmpBmp -Algorithm MD5).Hash

        if ($hash -ne $lastHash) {
            $ts  = Get-Date -Format "HHmmss"
            $idx = ($saved + 1).ToString("00")
            $bmp = "$outDir\theme_${idx}_${ts}.bmp"
            $png = "$outDir\theme_${idx}_${ts}.png"

            Copy-Item $tmpBmp $bmp -Force
            powershell -File "C:\dev\mb\tmp_convert.ps1" -In $bmp -Out $png

            $lastHash = $hash
            $saved++
            Write-Host "[SAVED #$saved] $png  (hash: $($hash.Substring(0,8))...)"
        } else {
            Write-Host "[no change] poll $($polls+1)"
        }
    } catch {
        Write-Host "[ERROR] $_"
    }

    $polls++
    Start-Sleep -Seconds 6
}

Write-Host ""
Write-Host "Done. $saved unique frames saved to $outDir"
