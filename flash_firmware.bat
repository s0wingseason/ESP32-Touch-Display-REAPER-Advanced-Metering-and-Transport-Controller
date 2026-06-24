@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

title MeterBridge — Flash ESP32 Firmware

echo.
echo  ============================================================
echo    MeterBridge v2.0 RC1  ^|  ESP32 Firmware Flasher
echo    FalconEYE Software Dev
echo  ============================================================
echo.

:: ── Step 1: Locate esptool ───────────────────────────────────────────
set "ESPTOOL="

:: Prefer bundled esptool in installed layout
if exist "%~dp0tools\esptool\esptool.exe" (
    set "ESPTOOL=%~dp0tools\esptool\esptool.exe"
    echo [OK]   Using bundled esptool: !ESPTOOL!
    goto :FIND_FIRMWARE
)

:: Fall back to PlatformIO's esptool (dev environment)
set "PIO_ESPTOOL="
for /r "%USERPROFILE%\.platformio" %%f in (esptool.exe) do (
    if not defined PIO_ESPTOOL set "PIO_ESPTOOL=%%f"
)
if defined PIO_ESPTOOL (
    set "ESPTOOL=!PIO_ESPTOOL!"
    echo [OK]   Using PlatformIO esptool: !ESPTOOL!
    goto :FIND_FIRMWARE
)

:: Fall back to Python-based esptool
python -m esptool version >nul 2>&1
if not errorlevel 1 (
    set "ESPTOOL=python -m esptool"
    echo [OK]   Using Python esptool (python -m esptool)
    goto :FIND_FIRMWARE
)

:: Nothing found
echo [ERROR] esptool not found!
echo.
echo   Option A: Re-run the MeterBridge installer and tick the
echo             "ESPTool Flash Utility" component.
echo.
echo   Option B: Install via pip:
echo             pip install esptool
echo.
goto :DONE

:FIND_FIRMWARE
:: ── Step 2: Locate firmware binaries ────────────────────────────────
set "FW_DIR=%~dp0firmware"
set "FW_BIN=%FW_DIR%\meterbridge_firmware_rc1.bin"
set "BOOT_BIN=%FW_DIR%\bootloader.bin"
set "PART_BIN=%FW_DIR%\partitions.bin"

:: Dev layout fallback
if not exist "%FW_BIN%" (
    set "FW_DIR=%~dp0esp32-firmware\.pio\build\crowpanel_7inch"
    set "FW_BIN=!FW_DIR!\firmware.bin"
    set "BOOT_BIN=!FW_DIR!\bootloader.bin"
    set "PART_BIN=!FW_DIR!\partitions.bin"
)

if not exist "%FW_BIN%" (
    echo [ERROR] Firmware binary not found!
    echo.
    echo   Expected: %FW_BIN%
    echo.
    echo   If you're in the dev environment, run build_and_run.bat first.
    echo   If you're using the installer, re-run it and tick "Firmware Binaries".
    goto :DONE
)

echo [OK]   Firmware:   %FW_BIN%
if exist "%BOOT_BIN%" (echo [OK]   Bootloader: %BOOT_BIN%) else (echo [WARN] Bootloader not found - skipping)
if exist "%PART_BIN%" (echo [OK]   Partitions: %PART_BIN%) else (echo [WARN] Partitions not found - skipping)

:: ── Step 3: Detect COM port ──────────────────────────────────────────
echo.
echo [...]  Scanning for ESP32 on USB...
echo.

set "COM_PORT="
set "COM_LIST="

:: Use PowerShell to enumerate COM ports with description
for /f "usebackq tokens=*" %%a in (`powershell -NoProfile -Command "Get-WmiObject Win32_PnPEntity | Where-Object {$_.Name -match 'COM\d+'} | ForEach-Object { ($_.Name -replace '.*\((.+)\).*','$1') + '|' + $_.Name } | Sort-Object" 2^>nul`) do (
    set "COM_LIST=!COM_LIST!%%a|"
    :: Auto-select if it looks like an ESP32/Silicon Labs/CH340
    echo %%a | findstr /i "CP210\|CH340\|ESP32\|USB Serial\|Silicon Labs" >nul 2>&1
    if not errorlevel 1 (
        if not defined COM_PORT (
            for /f "tokens=1 delims=|" %%c in ("%%a") do set "COM_PORT=%%c"
        )
    )
)

if defined COM_PORT (
    echo [OK]   ESP32 auto-detected on: !COM_PORT!
    goto :CONFIRM_FLASH
)

:: Manual selection
echo [INFO] No ESP32 automatically detected.
echo.
echo   Available ports:
for /f "usebackq tokens=*" %%a in (`powershell -NoProfile -Command "Get-WmiObject Win32_PnPEntity | Where-Object {$_.Name -match 'COM\d+'} | Select-Object -ExpandProperty Name" 2^>nul`) do (
    echo     %%a
)
echo.
set /p COM_PORT="  Enter the COM port for your ESP32 (e.g. COM7): "
if not defined COM_PORT goto :DONE
set "COM_PORT=%COM_PORT: =%"

:CONFIRM_FLASH
echo.
echo  ──────────────────────────────────────────────────────────
echo   Ready to flash:
echo     Port    : %COM_PORT%
echo     Firmware: %FW_BIN%
echo  ──────────────────────────────────────────────────────────
echo.
echo   Ensure your CrowPanel ESP32 is connected via USB.
echo   If the upload fails, hold the BOOT button on the device
echo   while clicking OK, then release it after 2 seconds.
echo.
set /p CONFIRM="  Type YES and press Enter to flash, or press Enter to cancel: "
if /i not "%CONFIRM%"=="YES" (
    echo [SKIP] Flash cancelled.
    goto :DONE
)

:: ── Step 4: Flash ────────────────────────────────────────────────────
echo.
echo [...]  Flashing — this takes 30-60 seconds. Do not disconnect.
echo.

set "FLASH_OK=0"

if exist "%BOOT_BIN%" if exist "%PART_BIN%" (
    :: Full 3-binary flash (bootloader + partitions + app)
    %ESPTOOL% --port %COM_PORT% --baud 921600 --chip esp32s3 ^
        write_flash ^
        0x0000  "%BOOT_BIN%" ^
        0x8000  "%PART_BIN%" ^
        0x10000 "%FW_BIN%"
) else (
    :: App-only flash (OTA-compatible, app partition only)
    %ESPTOOL% --port %COM_PORT% --baud 921600 ^
        write_flash 0x10000 "%FW_BIN%"
)

if not errorlevel 1 set "FLASH_OK=1"

echo.
if "%FLASH_OK%"=="1" (
    echo  ============================================================
    echo   [OK]  Firmware flashed successfully!
    echo.
    echo   Your MeterBridge device is restarting.
    echo   Watch the screen — it will show the WiFi IP address.
    echo   Copy that IP into relay_config.txt  (esp32_ip=YOUR.IP)
    echo  ============================================================
) else (
    echo  ============================================================
    echo   [ERROR] Flash failed!
    echo.
    echo   Troubleshooting:
    echo    1. Hold the BOOT button on the ESP32, then retry.
    echo    2. Try a different USB cable (data cable, not charge-only).
    echo    3. Check Device Manager for the correct COM port.
    echo    4. Try lowering baud: edit this script and change 921600 to 460800.
    echo  ============================================================
)

:DONE
echo.
echo Press any key to close...
pause >nul
endlocal
