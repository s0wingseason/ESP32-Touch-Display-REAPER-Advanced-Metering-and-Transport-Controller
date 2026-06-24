@echo off
setlocal enabledelayedexpansion
title FalconEYE Firmware Builder

echo [OK] Validating toolchain...
where pio >nul 2>nul
if %errorlevel% neq 0 (
    echo [SKIP] Global PlatformIO not found. Checking local python environment...
    where python >nul 2>nul
    if %errorlevel% neq 0 (
        echo [ERROR] Python is not installed or not in PATH. Please install Python.
        pause
        exit /b 1
    )
)

echo [OK] Building and Uploading Firmware...
python -m platformio run --target upload
if %errorlevel% neq 0 (
    echo [ERROR] Build or Upload failed!
    pause
    exit /b 1
)

echo [OK] Build complete! Streaming serial logs...
echo ========================================================
python -m platformio device monitor --baud 115200
echo ========================================================
pause
