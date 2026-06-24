@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: MeterBridge — Run (launch REAPER with plugin already installed)
:: ============================================================================

set "SCRIPT_DIR=%~dp0"
set "REAPER_EXE=C:\Program Files\REAPER (x64)\reaper.exe"

echo.
echo  ===================================
echo   MeterBridge - Run
echo  ===================================
echo.

:: Check if VST3 is installed
set "REAPER_PLUGINS=%APPDATA%\REAPER\UserPlugins"
if not exist "%REAPER_PLUGINS%\MeterBridge.vst3" (
    echo [ERROR] MeterBridge VST3 plugin not found.
    echo         Run build_and_run.bat first.
    goto :done
)

echo [OK]   MeterBridge VST3 plugin found.

if exist "%REAPER_EXE%" (
    echo [...]  Launching REAPER...
    start "" "%REAPER_EXE%"
    echo [OK]   REAPER launched.
    echo.
    echo  Remember to:
    echo    1. Add MeterBridge to your master bus (FX chain)
    echo    2. Enter ESP32 IP and click Connect
) else (
    echo [WARN]  REAPER not found at: %REAPER_EXE%
    echo         Launch REAPER manually.
)

:done
echo.
echo Press any key to close...
pause >nul
endlocal
