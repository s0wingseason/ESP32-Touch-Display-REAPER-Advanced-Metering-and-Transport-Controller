@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

title MeterBridge RC2 Package Builder

echo.
echo  ============================================================
echo    MeterBridge v2.0 RC2  ^|  Package Builder
echo    FalconEYE Software Dev  ^|  Data Sanitized Release
echo  ============================================================
echo.

set "LOG=%~dp0package_rc2_log.txt"
echo RC2 Package started: %DATE% %TIME% > "%LOG%"
set "RC2_DIR=%~dp0release\rc2_23-June-2026"
set "OUT_DIR=%RC2_DIR%"

:: ============================================================
:: Step 1: Check Inno Setup
:: ============================================================
echo [...]  Checking for Inno Setup...
set "ISCC="
for %%p in (
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) do (
    if exist %%p (
        if not defined ISCC set "ISCC=%%~p"
    )
)
if not defined ISCC (
    if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" (
        set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
    )
)
if not defined ISCC (
    echo [ERROR] Inno Setup 6 not found! Get it from: https://jrsoftware.org/isdl.php
    goto :DONE
)
echo [OK]   Inno Setup: %ISCC%

:: ============================================================
:: Step 2: Update ISS version string to RC2
:: ============================================================
echo [...]  Patching version to RC2 in installer script...
set "ISS_SRC=%~dp0installer\meterbridge.iss"
set "ISS_RC2=%~dp0installer\meterbridge_rc2.iss"

powershell -NoProfile -Command ^
    "(Get-Content '%ISS_SRC%') -replace 'RC1','RC2' | Set-Content '%ISS_RC2%'"

if not exist "%ISS_RC2%" (
    echo [ERROR] Failed to create RC2 ISS patch!
    goto :DONE
)
echo [OK]   RC2 ISS script ready.

:: ============================================================
:: Step 3: Compile RC2 installer
:: ============================================================
echo.
echo [OK]   Directory stubs not needed (ISS uses skipifsourcedoesntexist).
echo.
echo [...]  Compiling RC2 installer...
"%ISCC%" /O"%OUT_DIR%" "%ISS_RC2%" >> "%LOG%" 2>&1
set "ISS_ERR=!errorlevel!"

:: Clean up temp patched ISS
if exist "%ISS_RC2%" del /q "%ISS_RC2%"

if "!ISS_ERR!" NEQ "0" (
    echo [ERROR] Installer compile FAILED! See package_rc2_log.txt
    powershell -NoProfile -Command "Get-Content '%LOG%' -Tail 25"
    goto :DONE
)
echo [OK]   Installer compiled.

:: Rename compiled exe to RC2
if exist "%OUT_DIR%\MeterBridge_v2.0-RC1_Setup.exe" (
    if exist "%OUT_DIR%\MeterBridge_v2.0-RC2_Setup.exe" del /q "%OUT_DIR%\MeterBridge_v2.0-RC2_Setup.exe"
    rename "%OUT_DIR%\MeterBridge_v2.0-RC1_Setup.exe" "MeterBridge_v2.0-RC2_Setup.exe"
    echo [OK]   Renamed installer to RC2.
)

:: ============================================================
:: Step 4: Copy firmware (sanitized — binaries only, no build artifacts)
:: ============================================================
echo.
echo [...]  Copying firmware artifacts...
set "FW_ROOT=%~dp0esp32-firmware\.pio\build\crowpanel_7inch"
set "FW_DEST=%OUT_DIR%\firmware"
if not exist "%FW_DEST%" mkdir "%FW_DEST%"

set "FW_MISSING=0"
for %%f in (firmware.bin bootloader.bin partitions.bin) do (
    if exist "%FW_ROOT%\%%f" (
        copy /y "%FW_ROOT%\%%f" "%FW_DEST%\%%f" >nul
        echo [OK]   Copied: %%f
    ) else (
        echo [WARN] Missing firmware artifact: %%f
        set "FW_MISSING=1"
    )
)
if exist "%FW_DEST%\firmware.bin" (
    copy /y "%FW_DEST%\firmware.bin" "%FW_DEST%\meterbridge_firmware_rc2.bin" >nul
)

:: ============================================================
:: Step 5: Copy REAPER scripts
:: ============================================================
echo [...]  Copying REAPER scripts...
set "SCRIPTS_DEST=%OUT_DIR%\reaper"
if not exist "%SCRIPTS_DEST%" mkdir "%SCRIPTS_DEST%"
copy /y "%~dp0scripts\meterbridge_reaper_bridge.lua" "%SCRIPTS_DEST%\" >nul 2>&1
copy /y "%~dp0scripts\meterbridge_spectrum.jsfx"     "%SCRIPTS_DEST%\" >nul 2>&1
echo [OK]   REAPER scripts copied.

:: ============================================================
:: Step 6: Build Standalone ZIP (sanitized)
:: ============================================================
echo.
echo [...]  Building sanitized standalone ZIP...
set "STAGING=%~dp0release\_rc2_staging"
if exist "%STAGING%" rmdir /s /q "%STAGING%"
mkdir "%STAGING%"
mkdir "%STAGING%\firmware"
mkdir "%STAGING%\reaper"

:: Production files only — no logs, no scratch PNGs, no debug scripts
for %%f in (
    meterbridge_relay.py
    relay_config.txt
    run_relay.bat
    flash_firmware.bat
    install_reaper_bridge.bat
    requirements.txt
    build_and_run.bat
    run.bat
) do (
    if exist "%~dp0%%f" (
        copy /y "%~dp0%%f" "%STAGING%\" >nul
        echo [OK]   Staged: %%f
    )
)

:: README and release notes
copy /y "%OUT_DIR%\RELEASE_NOTES.md" "%STAGING%\" >nul
copy /y "%~dp0README.md" "%STAGING%\" >nul 2>&1

:: Firmware
copy /y "%FW_DEST%\meterbridge_firmware_rc2.bin" "%STAGING%\firmware\" >nul 2>&1
copy /y "%FW_DEST%\bootloader.bin" "%STAGING%\firmware\" >nul 2>&1
copy /y "%FW_DEST%\partitions.bin" "%STAGING%\firmware\" >nul 2>&1

:: REAPER scripts
copy /y "%SCRIPTS_DEST%\meterbridge_reaper_bridge.lua" "%STAGING%\reaper\" >nul 2>&1
copy /y "%SCRIPTS_DEST%\meterbridge_spectrum.jsfx"     "%STAGING%\reaper\" >nul 2>&1

:: Quick-start guide
(
    echo MeterBridge v2.0 RC2 - Standalone Bundle
    echo FalconEYE Software Dev - June 2026
    echo.
    echo QUICK START:
    echo   1. Double-click install_reaper_bridge.bat  - copies scripts to REAPER
    echo   2. Double-click run_relay.bat              - starts relay server
    echo   3. For firmware: double-click flash_firmware.bat
    echo.
    echo REQUIREMENTS:
    echo   Python 3.8+  from python.org
    echo   REAPER       from reaper.fm
    echo   CrowPanel 7" ESP32-S3 on your WiFi network
    echo.
    echo See RELEASE_NOTES.md for full upgrade notes from RC1.
) > "%STAGING%\QUICK_START.txt"

:: Create ZIP
set "ZIP_PATH=%OUT_DIR%\MeterBridge_v2.0-RC2_Standalone.zip"
if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGING%\*' -DestinationPath '%ZIP_PATH%' -Force"
if exist "%ZIP_PATH%" (
    echo [OK]   Standalone ZIP created.
) else (
    echo [WARN] ZIP creation failed.
)
rmdir /s /q "%STAGING%"

:: ============================================================
:: Step 7: Verify RC2 folder is clean (no scratch files)
:: ============================================================
echo.
echo [...]  Verifying data sanitization...
set "DIRTY=0"
for %%f in ("%OUT_DIR%\*.bmp" "%OUT_DIR%\*.log" "%OUT_DIR%\hs_err*" "%OUT_DIR%\replay_pid*") do (
    if exist "%%f" (
        echo [WARN] Scratch file found: %%f
        set "DIRTY=1"
    )
)
if "!DIRTY!"=="0" (
    echo [OK]   RC2 folder is clean. No debug/scratch artifacts present.
)

:: ============================================================
:: Step 8: Final report
:: ============================================================
echo.
echo  ============================================================
echo   RC2 PACKAGE COMPLETE
echo.
echo   Output: %OUT_DIR%
echo.
for %%f in ("%OUT_DIR%\*.exe" "%OUT_DIR%\*.zip" "%OUT_DIR%\*.md") do (
    set "SZ=%%~zf"
    set /a "SK=!SZ! / 1024"
    echo     %%~nxf   !SK! KB
)
echo.
echo   Share .exe for guided install, .zip for advanced/offline users.
echo  ============================================================

:DONE
echo.
echo Log: %LOG%
echo.
echo Press any key to close...
pause >nul
endlocal
