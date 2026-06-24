@echo off
title StudioBeacon - Install REAPER Integration
color 0A

echo.
echo  ============================================
echo   StudioBeacon - REAPER Integration Setup
echo  ============================================
echo.

set "REAPER_SCRIPTS=%APPDATA%\REAPER\Scripts"
set "SOURCE=%~dp0scripts\__startup_studiobeacon.lua"

echo This will install the REAPER auto-start script.
echo.
echo Target: %REAPER_SCRIPTS%
echo.

if not exist "%REAPER_SCRIPTS%" (
    echo ERROR: REAPER Scripts folder not found!
    echo Expected: %REAPER_SCRIPTS%
    echo.
    echo Please ensure REAPER is installed.
    pause
    exit /b 1
)

echo Copying auto-start script...
copy /Y "%SOURCE%" "%REAPER_SCRIPTS%\__startup_studiobeacon.lua"

if errorlevel 1 (
    echo.
    echo ERROR: Failed to copy script!
    pause
    exit /b 1
)

echo.
echo  ============================================
echo   SUCCESS!
echo  ============================================
echo.
echo The StudioBeacon REAPER bridge will now run
echo AUTOMATICALLY every time REAPER starts.
echo.
echo No further action is required.
echo.
pause
