@echo off
title StudioBeacon - Build Installer
color 0A

echo.
echo  ============================================
echo   StudioBeacon - Installer Builder
echo  ============================================
echo.

set TARGET=C:\StudioBeacon
set SOURCE=%~dp0

REM Kill any running instances
taskkill /F /IM electron.exe >nul 2>&1

echo [1/5] Copying files to %TARGET%...
echo.

REM Clean target
if exist "%TARGET%" rmdir /s /q "%TARGET%" >nul 2>&1
mkdir "%TARGET%" >nul 2>&1

REM Copy core files
copy "%SOURCE%package.json" "%TARGET%\" >nul
copy "%SOURCE%main.js" "%TARGET%\" >nul
copy "%SOURCE%preload.js" "%TARGET%\" >nul
copy "%SOURCE%*.md" "%TARGET%\" >nul 2>&1
copy "%SOURCE%*.txt" "%TARGET%\" >nul 2>&1

REM Copy folders
xcopy "%SOURCE%src" "%TARGET%\src\" /E /Y /I /Q >nul 2>&1
xcopy "%SOURCE%assets" "%TARGET%\assets\" /E /Y /I /Q >nul 2>&1
xcopy "%SOURCE%config" "%TARGET%\config\" /E /Y /I /Q >nul 2>&1
xcopy "%SOURCE%scripts" "%TARGET%\scripts\" /E /Y /I /Q >nul 2>&1
xcopy "%SOURCE%installer" "%TARGET%\installer\" /E /Y /I /Q >nul 2>&1

echo [OK] Files copied
echo.

REM Switch to local directory
cd /d "%TARGET%"

echo [2/5] Installing dependencies...
echo.
call npm install
if errorlevel 1 (
    echo [ERROR] npm install failed!
    pause
    exit /b 1
)
echo [OK] Dependencies installed
echo.

echo [3/5] Building Electron application...
echo.
call npm run build
if errorlevel 1 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)
echo [OK] Application built
echo.

REM Check for Inno Setup
set INNO_CMD=
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "INNO_CMD=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
) else if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set "INNO_CMD=C:\Program Files\Inno Setup 6\ISCC.exe"
)

if defined INNO_CMD (
    echo [4/5] Creating installer with Inno Setup...
    echo.
    
    if not exist "installer\output" mkdir "installer\output"
    
    "%INNO_CMD%" "installer\setup.iss"
    if errorlevel 1 (
        echo [WARNING] Inno Setup failed.
    ) else (
        echo [OK] Installer created!
    )
) else (
    echo [4/5] Inno Setup not found - skipping installer creation
    echo       You can run the app from: %TARGET%\dist\win-unpacked\StudioBeacon.exe
)

echo.
echo [5/5] Copying output back to source...
if exist "%TARGET%\dist" (
    xcopy "%TARGET%\dist" "%SOURCE%dist\" /E /Y /I /Q >nul 2>&1
)
if exist "%TARGET%\installer\output" (
    xcopy "%TARGET%\installer\output" "%SOURCE%installer\output\" /E /Y /I /Q >nul 2>&1
)

echo.
echo  ============================================
echo   BUILD COMPLETE!
echo  ============================================
echo.

if exist "%TARGET%\installer\output\StudioBeacon_Setup_*.exe" (
    echo Installer: %TARGET%\installer\output\
    dir /b "%TARGET%\installer\output\*.exe"
    echo.
    start "" "%TARGET%\installer\output"
) else if exist "%TARGET%\dist\win-unpacked\StudioBeacon.exe" (
    echo Portable EXE: %TARGET%\dist\win-unpacked\StudioBeacon.exe
    echo.
    set /p RUN="Run StudioBeacon now? (Y/N): "
    if /i "%RUN%"=="Y" start "" "%TARGET%\dist\win-unpacked\StudioBeacon.exe"
)

pause
