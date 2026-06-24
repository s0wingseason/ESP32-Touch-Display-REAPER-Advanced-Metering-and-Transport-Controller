@echo off
setlocal
cd /d "%~dp0"
echo [LOGGER] Starting persistent ESP32 USB Logger...
echo [LOGGER] Logs will be saved to esp_scan_log.txt
echo [LOGGER] Press Ctrl+C to stop.
:loop
python dump_serial.py
echo [LOGGER] Connection lost or port busy. Retrying in 2 seconds...
timeout /t 2 >nul
goto loop
