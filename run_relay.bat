@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ==============================
echo   MeterBridge Relay
echo ==============================
echo.

:: Check for Python
where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found in PATH.
    echo         Install Python from https://python.org
    echo         Or ensure PlatformIO's Python is in PATH.
    goto :end
)

:: Check if relay script exists
if not exist "meterbridge_relay.py" (
    echo [ERROR] meterbridge_relay.py not found!
    echo         Run this from the MeterBridge project root.
    goto :end
)

:: Read esp32_ip from config, skip comment lines, treat 0.0.0.0 as unset
set "ESP32_IP="
if exist "relay_config.txt" (
    for /f "usebackq tokens=1,* delims==" %%a in (`findstr /v "^#" relay_config.txt`) do (
        if /i "%%a"=="esp32_ip" (
            :: Strip leading/trailing whitespace by passing through a SET round-trip
            set "_RAW=%%b"
            for /f "tokens=* delims= " %%x in ("%%b") do set "_RAW=%%x"
            if not "!_RAW!"=="0.0.0.0" if defined _RAW set "ESP32_IP=!_RAW!"
        )
    )
)

:: If we have a real saved IP, use it; otherwise auto-discover
if defined ESP32_IP (
    echo [OK]   Using saved ESP32 IP: !ESP32_IP!
    echo         (Edit relay_config.txt to change it)
    echo.
    python meterbridge_relay.py --esp32-ip !ESP32_IP!
) else (
    echo [...]  Auto-discovering ESP32 on network...
    echo         (To skip: edit relay_config.txt with esp32_ip=YOUR.ESP32.IP)
    echo.
    python meterbridge_relay.py
)

:end
echo.
pause
