@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ==============================
echo   MeterBridge REAPER Bridge
echo   Install Script
echo ==============================
echo.

:: Check if REAPER Scripts directory exists
set "REAPER_SCRIPTS=%APPDATA%\REAPER\Scripts"
if not exist "%REAPER_SCRIPTS%" (
    echo [...]  Creating REAPER Scripts directory...
    mkdir "%REAPER_SCRIPTS%" 2>nul
    if errorlevel 1 (
        echo [ERROR] Could not create "%REAPER_SCRIPTS%"
        echo         Is REAPER installed?
        goto :end
    )
)

:: Copy the bridge script
:: Support both: installed layout ({app}\reaper\) and dev-repo layout (scripts\)
set "SRC="
if exist "%~dp0reaper\meterbridge_reaper_bridge.lua" (
    set "SRC=%~dp0reaper\meterbridge_reaper_bridge.lua"
) else if exist "%~dp0scripts\meterbridge_reaper_bridge.lua" (
    set "SRC=%~dp0scripts\meterbridge_reaper_bridge.lua"
)
set "DST=%REAPER_SCRIPTS%\meterbridge_reaper_bridge.lua"

if not defined SRC (
    echo [ERROR] Bridge script not found in reaper\ or scripts\
    echo         Expected location: %~dp0reaper\meterbridge_reaper_bridge.lua
    goto :end
)

copy /y "%SRC%" "%DST%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to copy bridge script.
    goto :end
)

echo [OK]   Bridge script installed to:
echo         %DST%

:: Copy the JSFX spectrum analyzer plugin
set "REAPER_EFFECTS=%APPDATA%\REAPER\Effects"
if not exist "%REAPER_EFFECTS%" (
    echo [...]  Creating REAPER Effects directory...
    mkdir "%REAPER_EFFECTS%" 2>nul
)

:: Copy JSFX — probe reaper\ then scripts\ same as the Lua file
set "JSFX_SRC="
if exist "%~dp0reaper\meterbridge_spectrum.jsfx" (
    set "JSFX_SRC=%~dp0reaper\meterbridge_spectrum.jsfx"
) else if exist "%~dp0scripts\meterbridge_spectrum.jsfx" (
    set "JSFX_SRC=%~dp0scripts\meterbridge_spectrum.jsfx"
)
set "JSFX_DST=%REAPER_EFFECTS%\meterbridge_spectrum.jsfx"

if exist "%JSFX_SRC%" (
    copy /y "%JSFX_SRC%" "%JSFX_DST%" >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Failed to copy JSFX spectrum plugin.
    ) else (
        echo [OK]   Spectrum JSFX installed to:
        echo         %JSFX_DST%
        echo         ^(Available in REAPER FX browser under JS effects^)
    )
) else (
    echo [SKIP] Spectrum JSFX not found: %JSFX_SRC%
)
echo.
echo ==============================
echo   Next Steps:
echo ==============================
echo.
echo   1. Open REAPER
echo   2. Go to Actions ^> Show action list
echo   3. Click "Load..." and select:
echo      %DST%
echo.
echo   OR: To auto-run on REAPER startup, name the file
echo       "__startup_meterbridge.lua" and it will run
echo       automatically every time REAPER opens.
echo.


:: Also install as auto-start script with a separate name (non-interactive)
:: Users can delete __startup_meterbridge.lua from REAPER\Scripts to disable auto-start
set "AUTO_DST=%REAPER_SCRIPTS%\__startup_meterbridge.lua"
copy /y "%SRC%" "%AUTO_DST%" >nul 2>&1
if errorlevel 1 (
    echo [SKIP] Auto-start script could not be installed.
) else (
    echo [OK]   Auto-start script installed:
    echo         %AUTO_DST%
    echo         (Delete this file from REAPER\Scripts to disable auto-start)
)

:end
echo.
echo  Installation complete! Start REAPER to activate MeterBridge.
echo.
pause
