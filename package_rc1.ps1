# MeterBridge v2.0 RC1 - Release Packaging (PowerShell)
# FalconEYE Software Dev - June 2026
# Called by package_rc1.bat

$ErrorActionPreference = 'Continue'
$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$RcTag      = "rc1_22-June-2026"
$Version    = "2.0-RC1"
$AppName    = "MeterBridge"
$Staging    = Join-Path $ScriptDir "staging_release"
$RcOut      = Join-Path $ScriptDir "release\$RcTag"
$LogFile    = Join-Path $ScriptDir "build_log.txt"
$Iscc       = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
$IssFile    = Join-Path $ScriptDir "installer\meterbridge.iss"

function Log($msg) { Add-Content $LogFile $msg }
function Ok($msg)  { Write-Host "[OK]   $msg"   -ForegroundColor Green }
function Warn($msg){ Write-Host "[WARN] $msg"   -ForegroundColor Yellow }
function Info($msg){ Write-Host "[...] $msg" }
function Err($msg) { Write-Host "[ERROR] $msg"  -ForegroundColor Red }

"MeterBridge $Version Release Log - $(Get-Date)" | Out-File $LogFile

# ─── 1. Verify firmware binary ────────────────────────────────────────────────
Info "Checking firmware binary..."
$FwSrc = Join-Path $ScriptDir "esp32-firmware\.pio\build\crowpanel_7inch\firmware.bin"
$BlSrc = Join-Path $ScriptDir "esp32-firmware\.pio\build\crowpanel_7inch\bootloader.bin"
$PtSrc = Join-Path $ScriptDir "esp32-firmware\.pio\build\crowpanel_7inch\partitions.bin"

if (-not (Test-Path $FwSrc)) {
    Err "Firmware binary not found: $FwSrc"
    Err "Run 'pio run -e crowpanel_7inch' or build_and_run.bat first."
    exit 1
}
Ok "Firmware binary found ($('{0:N0}' -f (Get-Item $FwSrc).Length) bytes)"

# ─── 2. Clean staging area ────────────────────────────────────────────────────
Info "Creating staging area..."
if (Test-Path $Staging) { Remove-Item $Staging -Recurse -Force }
@("$Staging\firmware","$Staging\relay","$Staging\reaper",
  "$Staging\scripts","$Staging\docs","$Staging\installer_output") |
  ForEach-Object { New-Item $_ -ItemType Directory -Force | Out-Null }
Ok "Staging area ready."

# ─── 3. Stage firmware ────────────────────────────────────────────────────────
Info "Staging firmware..."
Copy-Item $FwSrc "$Staging\firmware\meterbridge_firmware_rc1.bin"
if (Test-Path $BlSrc) { Copy-Item $BlSrc "$Staging\firmware\bootloader.bin" }
if (Test-Path $PtSrc) { Copy-Item $PtSrc "$Staging\firmware\partitions.bin" }
Ok "Firmware staged."

# ─── 4. Stage relay ──────────────────────────────────────────────────────────
Info "Staging relay..."
$relayFiles = @("meterbridge_relay.py","relay_config.txt")
foreach ($f in $relayFiles) {
    $src = Join-Path $ScriptDir $f
    if (Test-Path $src) { Copy-Item $src "$Staging\relay\" }
}
Ok "Relay staged."

# ─── 5. Stage REAPER scripts ──────────────────────────────────────────────────
Info "Staging REAPER scripts..."
$reaperFiles = @("scripts\meterbridge_reaper_bridge.lua","scripts\meterbridge_spectrum.jsfx")
foreach ($f in $reaperFiles) {
    $src = Join-Path $ScriptDir $f
    if (Test-Path $src) { Copy-Item $src "$Staging\reaper\" }
}
Ok "REAPER scripts staged."

# ─── 6. Stage batch scripts ───────────────────────────────────────────────────
Info "Staging helper scripts..."
$batFiles = @("run_relay.bat","flash_esp32.bat","install_reaper_bridge.bat")
foreach ($f in $batFiles) {
    $src = Join-Path $ScriptDir $f
    if (Test-Path $src) { Copy-Item $src "$Staging\scripts\" }
}
Ok "Helper scripts staged."

# ─── 7. Stage docs ────────────────────────────────────────────────────────────
$docsDir = Join-Path $ScriptDir "docs"
if (Test-Path $docsDir) {
    Info "Staging documentation..."
    Copy-Item "$docsDir\*" "$Staging\docs\" -Recurse -Force
    Ok "Documentation staged."
}

# ─── 8. Stage release notes ───────────────────────────────────────────────────
$rnSrc = Join-Path $ScriptDir "RELEASE_NOTES.md"
if (Test-Path $rnSrc) {
    Copy-Item $rnSrc "$Staging\"
    Ok "Release notes staged."
}

# ─── 9. Create RC1 output folder ─────────────────────────────────────────────
Info "Creating RC1 output folder: $RcOut"
if (Test-Path $RcOut) { Remove-Item $RcOut -Recurse -Force }
New-Item $RcOut -ItemType Directory -Force | Out-Null
New-Item "$RcOut\firmware" -ItemType Directory -Force | Out-Null

# Copy firmware + release notes to top-level output
Copy-Item "$Staging\firmware\*" "$RcOut\firmware\" -Force
if (Test-Path $rnSrc) { Copy-Item $rnSrc "$RcOut\" }
Ok "Output folder created."

# ─── 10. Build Windows Installer .exe ────────────────────────────────────────
Write-Host ""
Info "Building Windows installer (.exe)..."
$installerBuilt = $false

if (Test-Path $Iscc) {
    Ok "Inno Setup found: $Iscc"

    # Update the .iss OutputDir to point at our staging folder
    & $Iscc $IssFile "/O$Staging\installer_output" 2>&1 | Tee-Object -Append $LogFile | Out-Null

    $exeFiles = Get-ChildItem "$Staging\installer_output\*.exe" -ErrorAction SilentlyContinue
    if ($exeFiles) {
        foreach ($exe in $exeFiles) {
            Copy-Item $exe.FullName "$RcOut\" -Force
            Ok "Installer built and copied: $($exe.Name)"
        }
        $installerBuilt = $true
    } else {
        Warn "Inno Setup ran but no .exe was found. Check build_log.txt."
    }
} else {
    Warn "Inno Setup not found at: $Iscc"
    Warn "Installer .exe WILL NOT be built."
    Write-Host "       Download from: https://jrsoftware.org/isdl.php" -ForegroundColor Cyan
}

# ─── 11. Assemble standalone portable zip ────────────────────────────────────
Write-Host ""
Info "Building standalone portable zip..."
$SaDir = "$Staging\standalone_${AppName}_v${Version}"
@("$SaDir","$SaDir\reaper","$SaDir\firmware","$SaDir\docs") |
  ForEach-Object { New-Item $_ -ItemType Directory -Force | Out-Null }

# Root files
@("meterbridge_relay.py","relay_config.txt","run_relay.bat",
  "flash_esp32.bat","install_reaper_bridge.bat") | ForEach-Object {
    $src = Join-Path $ScriptDir $_
    if (Test-Path $src) { Copy-Item $src "$SaDir\" }
}

# Reaper scripts
if (Test-Path "$Staging\reaper") {
    Copy-Item "$Staging\reaper\*" "$SaDir\reaper\" -Force -ErrorAction SilentlyContinue
}

# Firmware
Copy-Item "$Staging\firmware\*" "$SaDir\firmware\" -Force

# Docs
if (Test-Path "$Staging\docs") {
    Copy-Item "$Staging\docs\*" "$SaDir\docs\" -Recurse -Force
}

# Release notes
if (Test-Path $rnSrc) { Copy-Item $rnSrc "$SaDir\" }

# README.txt for standalone
@"
MeterBridge v$Version - Standalone Portable
============================================================

No installation required. Extract and run from any folder.

QUICK START:
  1. run_relay.bat              -- Start the relay bridge
  2. install_reaper_bridge.bat  -- Copy REAPER Lua + JSFX scripts
  3. flash_esp32.bat            -- Flash ESP32 firmware via USB

REQUIREMENTS:
  - Python 3.8+ in PATH  (https://python.org)
  - REAPER DAW            (https://reaper.fm)
  - Elecrow CrowPanel 7" ESP32-S3 (or compatible ESP32+display)

Hardware:
  https://www.amazon.com/ELECROW-ESP32-Display-Compatible-MicroPython/dp/B0G1RYBZNL/

Full documentation: docs\index.html and RELEASE_NOTES.md
"@ | Out-File "$SaDir\README.txt" -Encoding utf8

# Zip it
$ZipName = "${AppName}_v${Version}_Standalone.zip"
$ZipOut  = "$RcOut\$ZipName"
Info "Compressing standalone to $ZipName..."
Compress-Archive -Path "$SaDir\*" -DestinationPath $ZipOut -Force

if (Test-Path $ZipOut) {
    $szMb = [math]::Round((Get-Item $ZipOut).Length / 1MB, 2)
    Ok "Standalone ZIP created: $ZipName ($szMb MB)"
} else {
    Warn "ZIP creation failed. Copying folder to output instead."
    Copy-Item "$SaDir\*" "$RcOut\standalone\" -Recurse -Force
}

# ─── 12. Print manifest ───────────────────────────────────────────────────────
Write-Host ""
Write-Host "  ===================================================" -ForegroundColor Cyan
Write-Host "   RC1 Release Package Complete!" -ForegroundColor Cyan
Write-Host "  ===================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Output: $RcOut" -ForegroundColor White
Write-Host ""
Write-Host "  Files:" -ForegroundColor White
Get-ChildItem $RcOut -File | ForEach-Object {
    $szMb = [math]::Round($_.Length / 1MB, 2)
    Write-Host ("    {0,-50} {1,8} MB" -f $_.Name, $szMb) -ForegroundColor Gray
}
if (Test-Path "$RcOut\firmware") {
    Write-Host ""
    Write-Host "  Firmware:" -ForegroundColor White
    Get-ChildItem "$RcOut\firmware" | ForEach-Object {
        $szKb = [math]::Round($_.Length / 1KB, 1)
        Write-Host ("    firmware\{0,-44} {1,8} KB" -f $_.Name, $szKb) -ForegroundColor Gray
    }
}
Write-Host ""

# ─── 13. Cleanup staging ─────────────────────────────────────────────────────
Info "Cleaning staging area..."
Remove-Item $Staging -Recurse -Force -ErrorAction SilentlyContinue
Ok "All done."
