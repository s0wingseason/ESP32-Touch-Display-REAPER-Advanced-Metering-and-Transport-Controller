@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: MeterBridge — Uninstall / Clean
:: Removes all build artifacts. Does NOT delete source code.
:: ============================================================================

set "SCRIPT_DIR=%~dp0"

echo.
echo  ===================================
echo   MeterBridge - Uninstall / Clean
echo  ===================================
echo.

:: ESP32 build artifacts
if exist "%SCRIPT_DIR%esp32-firmware\.pio" (
    echo [...]  Removing ESP32 build artifacts...
    rmdir /s /q "%SCRIPT_DIR%esp32-firmware\.pio"
    echo [OK]   ESP32 build cleaned.
) else (
    echo [SKIP]  No ESP32 build artifacts found.
)

:: VST3 build directory
if exist "%SCRIPT_DIR%vst-plugin\build" (
    echo [...]  Removing VST3 build directory...
    rmdir /s /q "%SCRIPT_DIR%vst-plugin\build"
    echo [OK]   VST3 build cleaned.
) else (
    echo [SKIP]  No VST3 build directory found.
)

:: Installed VST3 plugin
set "REAPER_PLUGINS=%APPDATA%\REAPER\UserPlugins"
if exist "%REAPER_PLUGINS%\MeterBridge.vst3" (
    echo [...]  Removing installed VST3 plugin...
    rmdir /s /q "%REAPER_PLUGINS%\MeterBridge.vst3"
    echo [OK]   VST3 plugin removed from REAPER.
)

:: Build log
if exist "%SCRIPT_DIR%build_log.txt" (
    del /q "%SCRIPT_DIR%build_log.txt"
    echo [OK]   Build log removed.
)

echo.
echo  ===================================
echo   Clean Complete
echo  ===================================
echo.
echo  Source code and configuration files are preserved.
echo  Run build_and_run.bat to rebuild from scratch.
echo.
echo Press any key to close...
pause >nul
endlocal
