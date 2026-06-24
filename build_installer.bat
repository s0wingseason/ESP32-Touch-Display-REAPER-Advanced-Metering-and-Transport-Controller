@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

title MeterBridge Build Installer

echo.
echo  ============================================================
echo    MeterBridge v2.0 RC1  ^|  Installer Builder
echo    FalconEYE Software Dev
echo  ============================================================
echo.

set "LOG=%~dp0build_installer_log.txt"
echo Build started: %DATE% %TIME% > "%LOG%"

:: ============================================================
:: Step 1: Check Inno Setup
:: ============================================================
echo [...]  Checking for Inno Setup...
set "ISCC="
for %%p in (
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    "C:\Program Files\Inno Setup 6\ISCC.exe"
    "C:\Program Files (x86)\Inno Setup 5\ISCC.exe"
) do (
    if exist %%p (
        if not defined ISCC set "ISCC=%%~p"
    )
)
:: Also check LOCALAPPDATA (user install without admin)
if not defined ISCC (
    if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" (
        set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
    )
)

if not defined ISCC (
    echo.
    echo [ERROR] Inno Setup 6 not found!
    echo.
    echo   Download from: https://jrsoftware.org/isdl.php
    echo   Install it, then re-run this script.
    echo.
    goto :DONE
)
echo [OK]   Inno Setup: %ISCC%
echo Inno Setup: %ISCC% >> "%LOG%"

:: ============================================================
:: Step 2: Check PlatformIO
:: ============================================================
echo [...]  Checking PlatformIO...
set "SKIP_FW_BUILD=0"
where pio >nul 2>&1
if errorlevel 1 (
    echo [WARN] PlatformIO not in PATH. Using existing firmware build output.
    set "SKIP_FW_BUILD=1"
) else (
    echo [OK]   PlatformIO found.
)

:: ============================================================
:: Step 3: Build firmware (if PlatformIO available)
:: ============================================================
if "!SKIP_FW_BUILD!"=="0" (
    echo.
    echo [...]  Building ESP32 firmware ^(60-120 seconds^)...
    echo.
    cd /d "%~dp0esp32-firmware"
    pio run -e crowpanel_7inch >> "%LOG%" 2>&1
    set "FW_ERR=!errorlevel!"
    cd /d "%~dp0"
    if "!FW_ERR!" NEQ "0" (
        echo [ERROR] Firmware build FAILED! See build_installer_log.txt
        echo.
        findstr /i "error" "%LOG%"
        goto :DONE
    )
    echo [OK]   Firmware built successfully.
) else (
    echo [SKIP] Firmware build skipped.
)

:: ============================================================
:: Step 4: Verify firmware binaries exist
:: ============================================================
echo.
echo [...]  Verifying firmware binaries...
set "FW_ROOT=%~dp0esp32-firmware\.pio\build\crowpanel_7inch"
set "MISSING=0"
for %%f in (firmware.bin bootloader.bin partitions.bin) do (
    if not exist "%FW_ROOT%\%%f" (
        echo [ERROR] Missing: %FW_ROOT%\%%f
        set "MISSING=1"
    ) else (
        echo [OK]   Found: %%f
    )
)
if "!MISSING!"=="1" (
    echo.
    echo [ERROR] Firmware binaries missing. Build firmware first.
    goto :DONE
)

:: ============================================================
:: Step 5: Bundle esptool
:: ============================================================
echo.
echo [...]  Preparing esptool...
set "TOOLS_DIR=%~dp0tools\esptool"
if not exist "%TOOLS_DIR%\esptool.exe" (
    echo [...]  esptool.exe not found. Searching PlatformIO cache...
    set "PIO_ESPTOOL="
    for /r "%USERPROFILE%\.platformio" %%f in (esptool.exe) do (
        if not defined PIO_ESPTOOL set "PIO_ESPTOOL=%%f"
    )
    if defined PIO_ESPTOOL (
        if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%"
        copy /y "!PIO_ESPTOOL!" "%TOOLS_DIR%\esptool.exe" >nul
        for %%d in ("!PIO_ESPTOOL!\..\*.dll") do (
            copy /y "%%d" "%TOOLS_DIR%\" >nul 2>&1
        )
        for %%d in ("!PIO_ESPTOOL!\..\*.pyd") do (
            copy /y "%%d" "%TOOLS_DIR%\" >nul 2>&1
        )
        echo [OK]   Copied esptool from PlatformIO.
    ) else (
        echo [WARN] esptool not found in PlatformIO cache.
        echo        End-users can install it with: pip install esptool
    )
) else (
    echo [OK]   esptool bundle ready.
)

:: ============================================================
:: Step 6: Create output directory
:: ============================================================
set "OUT_DIR=%~dp0release\installer"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

:: ============================================================
:: Step 7: Compile installer with Inno Setup
:: ============================================================
echo.
echo [...]  Compiling installer with Inno Setup...
echo.

"%ISCC%" /O"%OUT_DIR%" "%~dp0installer\meterbridge.iss" >> "%LOG%" 2>&1
set "ISS_ERR=!errorlevel!"
if "!ISS_ERR!" NEQ "0" (
    echo [ERROR] Inno Setup compilation FAILED!
    echo.
    echo   Last 25 lines of log:
    powershell -NoProfile -Command "Get-Content '%LOG%' -Tail 25"
    goto :DONE
)
echo [OK]   Installer compiled.

:: ============================================================
:: Step 8: Build Standalone ZIP
:: ============================================================
echo.
echo [...]  Building standalone ZIP bundle...

set "STAGING=%~dp0release\_zip_staging"
if exist "%STAGING%" rmdir /s /q "%STAGING%"
mkdir "%STAGING%"
mkdir "%STAGING%\firmware"
mkdir "%STAGING%\reaper"
mkdir "%STAGING%\docs"

:: Core relay files
for %%f in (
    meterbridge_relay.py
    relay_config.txt
    run_relay.bat
    flash_firmware.bat
    install_reaper_bridge.bat
    requirements.txt
    RELEASE_NOTES.md
) do (
    if exist "%~dp0%%f" copy /y "%~dp0%%f" "%STAGING%\" >nul
)

:: Firmware binaries
copy /y "%FW_ROOT%\firmware.bin"   "%STAGING%\firmware\meterbridge_firmware_rc1.bin" >nul 2>&1
copy /y "%FW_ROOT%\bootloader.bin" "%STAGING%\firmware\" >nul 2>&1
copy /y "%FW_ROOT%\partitions.bin" "%STAGING%\firmware\" >nul 2>&1

:: REAPER scripts
copy /y "%~dp0scripts\meterbridge_reaper_bridge.lua" "%STAGING%\reaper\" >nul 2>&1
copy /y "%~dp0scripts\meterbridge_spectrum.jsfx"     "%STAGING%\reaper\" >nul 2>&1

:: Docs
if exist "%~dp0docs" xcopy /s /q /y "%~dp0docs\*" "%STAGING%\docs\" >nul 2>&1

:: Quick-start readme
(
    echo MeterBridge v2.0 RC1 - Standalone Bundle
    echo FalconEYE Software Dev - June 2026
    echo.
    echo QUICK START:
    echo   1. Double-click install_reaper_bridge.bat  - copies scripts to REAPER
    echo   2. Double-click run_relay.bat              - starts the relay server
    echo   3. Open docs\index.html                    - full manual
    echo.
    echo TO FLASH FIRMWARE:
    echo   Double-click flash_firmware.bat
    echo   Requires: pip install esptool
    echo.
    echo REQUIREMENTS:
    echo   Python 3.8+  from python.org
    echo   REAPER       from reaper.fm
    echo   CrowPanel 7" ESP32-S3 on your WiFi
) > "%STAGING%\QUICK_START.txt"

:: Create ZIP
set "ZIP_PATH=%OUT_DIR%\MeterBridge_v2.0-RC1_Standalone.zip"
if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGING%\*' -DestinationPath '%ZIP_PATH%' -Force"
if exist "%ZIP_PATH%" (
    echo [OK]   Standalone ZIP created.
) else (
    echo [WARN] ZIP creation failed - check PowerShell output above.
)
rmdir /s /q "%STAGING%"

:: ============================================================
:: Step 9: Report results
:: ============================================================
echo.
echo  ============================================================
echo   BUILD COMPLETE
echo.
echo   Output folder: %OUT_DIR%
echo.
for %%f in ("%OUT_DIR%\*.exe" "%OUT_DIR%\*.zip") do (
    set "SZ=%%~zf"
    set /a "SZMBK=!SZ! / 1024"
    echo     %%~nxf   !SZMBK! KB
)
echo.
echo   .exe  = full guided installer  ^(share with end users^)
echo   .zip  = standalone bundle      ^(advanced / offline users^)
echo  ============================================================

:DONE
echo.
echo Full build log: %LOG%
echo.
echo Press any key to close...
pause >nul
endlocal
