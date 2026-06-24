param([string]$Tag = "verify")
$ip = "192.168.1.180"
$out = "C:\dev\mb\screen_${Tag}_$(Get-Date -Format 'HHmmss').bmp"
$png = $out -replace '\.bmp$','.png'
try {
    $r = Invoke-WebRequest -Uri "http://${ip}:8099/screenshot" -TimeoutSec 10
    [System.IO.File]::WriteAllBytes($out, $r.Content)
    Add-Type -AssemblyName System.Drawing
    $bmp = [System.Drawing.Bitmap]::FromFile($out)
    $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "[OK] Screenshot saved: $png ($($r.Content.Length) bytes, $($bmp.Width)x$($bmp.Height))"
    Remove-Item $out -ErrorAction SilentlyContinue
} catch { Write-Host "[ERROR] $_" }
