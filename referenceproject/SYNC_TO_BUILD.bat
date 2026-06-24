@echo off
REM ============================================================
REM StudioBeacon - Sync Network Drive to Build Folder
REM ============================================================
REM This script copies all updated files from the network drive
REM to C:\StudioBeacon for building the installer.
REM ============================================================

set SOURCE="I:\Shared drives\Falcon team drive\Falcon Biz Devt. Projects\FalconEYE Software Dev\personal projects\audio etc\audio etc 12.30.2025\StudioBeacon"
set DEST="C:\StudioBeacon"

echo.
echo ============================================================
echo   StudioBeacon Build Sync
echo ============================================================
echo.
echo Source: %SOURCE%
echo Dest:   %DEST%
echo.

REM Create destination if it doesn't exist
if not exist %DEST% (
    echo Creating destination folder...
    mkdir %DEST%
)

echo Syncing files...
echo.

REM Sync all files (excluding node_modules, dist, .git)
xcopy %SOURCE%\*.* %DEST%\ /Y /D
xcopy %SOURCE%\src %DEST%\src\ /E /Y /D
xcopy %SOURCE%\scripts %DEST%\scripts\ /E /Y /D
xcopy %SOURCE%\assets %DEST%\assets\ /E /Y /D
xcopy %SOURCE%\installer %DEST%\installer\ /E /Y /D

REM Copy root files explicitly
copy %SOURCE%\main.js %DEST%\ /Y
copy %SOURCE%\preload.js %DEST%\ /Y
copy %SOURCE%\package.json %DEST%\ /Y

echo.
echo ============================================================
echo   Sync Complete!
echo ============================================================
echo.
echo Next steps:
echo   1. cd C:\StudioBeacon
echo   2. npm install (if needed)
echo   3. npm run build:win
echo   4. Run Inno Setup on installer\setup.iss
echo.
echo Also copying Lua script directly to REAPER...
copy %SOURCE%\scripts\__startup_studiobeacon.lua "%APPDATA%\REAPER\Scripts\" /Y
echo.
echo Done! Restart REAPER to load the updated script.
echo.
pause
