@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: MeterBridge — Flash ESP32 Firmware
:: Detects the ESP32 COM port and flashes the compiled firmware.
:: ============================================================================

set "SCRIPT_DIR=%~dp0"

echo.
echo  ===================================
echo   MeterBridge - Flash ESP32
echo  ===================================
echo.

:: Check PlatformIO
where pio >nul 2>&1
if errorlevel 1 (
    echo [ERROR] PlatformIO not found.
    echo         Run build_and_run.bat first to install it.
    goto :done
)

:: Check firmware was built
if not exist "%SCRIPT_DIR%esp32-firmware\.pio\build" (
    echo [ERROR] Firmware not built yet.
    echo         Run build_and_run.bat first.
    goto :done
)

echo [...]  Detecting ESP32 on USB...
echo.

:: List available serial ports
pio device list 2>nul

echo.
echo [...]  Uploading firmware to ESP32...
echo.

cd /d "%SCRIPT_DIR%esp32-firmware"
pio run --target upload
if errorlevel 1 (
    echo.
    echo [ERROR] Upload failed!
    echo         - Is the ESP32 connected via USB?
    echo         - Is the correct COM port selected?
    echo         - Try holding the BOOT button while uploading.
    goto :done
)

echo.
echo [OK]   Firmware uploaded successfully!
echo.
echo [...]  Opening serial monitor (press Ctrl+C to exit)...
echo.
pio device monitor --baud 115200

:done
echo.
echo Press any key to close...
pause >nul
cd /d "%SCRIPT_DIR%"
endlocal
