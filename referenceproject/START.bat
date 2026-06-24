@echo off
title StudioBeacon

echo.
echo  ============================================
echo   StudioBeacon - Starting
echo  ============================================
echo.

set TARGET=C:\StudioBeacon
set SOURCE=%~dp0

echo Source: %SOURCE%
echo Target: %TARGET%
echo.

echo Killing old processes...
taskkill /F /IM electron.exe >nul 2>&1

echo.
echo Backing up settings...
if exist "%TARGET%\config\settings.json" (
    mkdir "%TEMP%\SB_Backup" >nul 2>&1
    copy "%TARGET%\config\settings.json" "%TEMP%\SB_Backup\settings.json" >nul 2>&1
    echo   Settings backed up.
)

echo.
echo Cleaning old installation...
rmdir /s /q "%TARGET%" >nul 2>&1
mkdir "%TARGET%" >nul 2>&1

echo.
echo Copying files...
copy "%SOURCE%package.json" "%TARGET%\" >nul
copy "%SOURCE%main.js" "%TARGET%\" >nul
copy "%SOURCE%preload.js" "%TARGET%\" >nul
copy "%SOURCE%*.md" "%TARGET%\" >nul 2>&1
copy "%SOURCE%*.txt" "%TARGET%\" >nul 2>&1
copy "%SOURCE%*.bat" "%TARGET%\" >nul 2>&1

echo Copying folders...
xcopy "%SOURCE%src" "%TARGET%\src\" /E /Y /I /Q >nul 2>&1
xcopy "%SOURCE%assets" "%TARGET%\assets\" /E /Y /I /Q >nul 2>&1
xcopy "%SOURCE%config" "%TARGET%\config\" /E /Y /I /Q >nul 2>&1
xcopy "%SOURCE%scripts" "%TARGET%\scripts\" /E /Y /I /Q >nul 2>&1
xcopy "%SOURCE%installer" "%TARGET%\installer\" /E /Y /I /Q >nul 2>&1

echo.
echo Syncing REAPER bridge script...
if not exist "%APPDATA%\REAPER\Scripts" (
    mkdir "%APPDATA%\REAPER\Scripts" >nul 2>&1
)
copy "%SOURCE%scripts\__startup_studiobeacon.lua" "%APPDATA%\REAPER\Scripts\" /Y >nul 2>&1
if exist "%APPDATA%\REAPER\Scripts\__startup_studiobeacon.lua" (
    echo   [OK] REAPER bridge script installed
) else (
    echo   [WARN] Could not copy REAPER bridge script
)

echo.
echo Restoring settings...
if exist "%TEMP%\SB_Backup\settings.json" (
    mkdir "%TARGET%\config" >nul 2>&1
    copy "%TEMP%\SB_Backup\settings.json" "%TARGET%\config\settings.json" >nul 2>&1
    copy "%TEMP%\SB_Backup\settings.json" "%TARGET%\config\settings_backup.json" >nul 2>&1
    rmdir /s /q "%TEMP%\SB_Backup" >nul 2>&1
    echo   Settings restored.
)

echo.
echo Checking files...
if exist "%TARGET%\package.json" (
    echo   [OK] package.json
) else (
    echo   [MISSING] package.json - CRITICAL ERROR
    pause
    exit /b 1
)

if exist "%TARGET%\main.js" (
    echo   [OK] main.js
) else (
    echo   [MISSING] main.js - CRITICAL ERROR
    pause
    exit /b 1
)

echo.
cd /d "%TARGET%"
echo Working directory: %CD%
echo.

if not exist "node_modules\electron" (
    echo Installing dependencies...
    echo This takes 1-2 minutes on first run.
    echo.
    call npm install
)

echo.
echo ============================================
echo   Launching StudioBeacon...
echo   Press Ctrl+C to stop
echo ============================================
echo.

call npm start

echo.
echo Application closed.
pause
