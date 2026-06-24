@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: MeterBridge — Build and Run
:: Builds both ESP32 firmware and VST3 plugin, then launches REAPER.
:: Double-click this file from File Explorer. No terminal expertise needed.
:: ============================================================================

set "SCRIPT_DIR=%~dp0"
set "LOG_FILE=%SCRIPT_DIR%build_log.txt"

echo. > "%LOG_FILE%"
echo ============================== >> "%LOG_FILE%"
echo  MeterBridge Build Log         >> "%LOG_FILE%"
echo  %date% %time%                 >> "%LOG_FILE%"
echo ============================== >> "%LOG_FILE%"

echo.
echo  ===================================
echo   MeterBridge - Build ^& Run
echo  ===================================
echo.

:: ─── Step 1: Check Toolchain ──────────────────────────────────
echo [...]  Checking toolchain...

:: Check Python
where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found.
    echo         Download from: https://www.python.org/downloads/
    echo         Make sure to check "Add Python to PATH" during install.
    goto :error
)
echo [OK]   Python found.

:: Check Node.js (for potential future web config server)
where node >nul 2>&1
if errorlevel 1 (
    echo [SKIP]  Node.js not found (optional).
) else (
    echo [OK]   Node.js found.
)

:: Check PlatformIO
where pio >nul 2>&1
if errorlevel 1 (
    echo [...]  PlatformIO not found. Installing via pip...
    python -m pip install platformio >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        echo [ERROR] Failed to install PlatformIO.
        echo         Try: pip install platformio
        goto :error
    )
    echo [OK]   PlatformIO installed.
) else (
    echo [OK]   PlatformIO found.
)

:: Check CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo [...]  CMake not found. Attempting to install via winget...
    winget install Kitware.CMake --silent >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        echo [WARN]  Could not auto-install CMake.
        echo         Download from: https://cmake.org/download/
        echo         VST3 plugin build will be skipped.
        set "SKIP_VST=1"
    ) else (
        echo [OK]   CMake installed. You may need to restart this script.
        set "SKIP_VST=1"
    )
) else (
    echo [OK]   CMake found.
)

:: Check for Visual Studio 2022 (MSBuild)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
    if defined VS_PATH (
        echo [OK]   Visual Studio found at: !VS_PATH!
    ) else (
        echo [WARN]  Visual Studio not found. VST3 build will be skipped.
        echo         Download Build Tools: https://visualstudio.microsoft.com/downloads/
        set "SKIP_VST=1"
    )
) else (
    echo [WARN]  Visual Studio Installer not found. VST3 build may fail.
    set "SKIP_VST=1"
)

echo.

:: ─── Step 2: Build ESP32 Firmware ─────────────────────────────
echo [...]  Building ESP32 firmware...
echo. >> "%LOG_FILE%"
echo === ESP32 Firmware Build === >> "%LOG_FILE%"

cd /d "%SCRIPT_DIR%esp32-firmware"
pio run -e crowpanel_7inch >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    echo [ERROR] ESP32 firmware build failed!
    echo         Check %LOG_FILE% for details.
    echo         Common fixes:
    echo           - Ensure ESP32 board package is installed
    echo           - Run: pio pkg install
    goto :error
) else (
    echo [OK]   ESP32 firmware built successfully.
)

cd /d "%SCRIPT_DIR%"
echo.

:: ─── Step 3: Build VST3 Plugin ────────────────────────────────
if defined SKIP_VST (
    echo [SKIP]  Skipping VST3 build (missing CMake or Visual Studio).
    goto :skip_vst
)

echo [...]  Building VST3 plugin...
echo. >> "%LOG_FILE%"
echo === VST3 Plugin Build === >> "%LOG_FILE%"

cd /d "%SCRIPT_DIR%vst-plugin"

if not exist "build" mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    echo [ERROR] CMake configuration failed!
    echo         Check %LOG_FILE% for details.
    goto :error
)

cmake --build . --config Release >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    echo [ERROR] VST3 plugin build failed!
    echo         Check %LOG_FILE% for details.
    goto :error
)

echo [OK]   VST3 plugin built successfully.

:: Copy VST3 to REAPER plugins directory
set "REAPER_PLUGINS=%APPDATA%\REAPER\UserPlugins"
if not exist "%REAPER_PLUGINS%" mkdir "%REAPER_PLUGINS%"

:: Find and copy the VST3 bundle
for /r "." %%f in (*.vst3) do (
    echo [...]  Copying %%~nxf to REAPER plugins...
    xcopy "%%f" "%REAPER_PLUGINS%\" /Y /Q >> "%LOG_FILE%" 2>&1
)

:: Check for VST3 bundle directory
if exist "MeterBridge_artefacts\Release\VST3\MeterBridge.vst3" (
    xcopy "MeterBridge_artefacts\Release\VST3\MeterBridge.vst3" "%REAPER_PLUGINS%\MeterBridge.vst3\" /Y /Q /E >> "%LOG_FILE%" 2>&1
    echo [OK]   VST3 plugin installed to REAPER.
)

cd /d "%SCRIPT_DIR%"

:skip_vst
echo.

:: ─── Step 4: Launch REAPER ────────────────────────────────────
set "REAPER_EXE=C:\Program Files\REAPER (x64)\reaper.exe"
if exist "%REAPER_EXE%" (
    echo [...]  Launching REAPER...
    start "" "%REAPER_EXE%"
    echo [OK]   REAPER launched.
) else (
    echo [SKIP]  REAPER not found at default location.
    echo         Launch REAPER manually and load the MeterBridge VST3 plugin.
)

echo.
echo  ===================================
echo   Build Complete!
echo  ===================================
echo.
echo  Next steps:
echo    1. In REAPER, add MeterBridge VST3 to your master bus
echo    2. Enter your ESP32's IP address in the plugin GUI
echo    3. Click "Connect" to start streaming
echo.
echo  To flash firmware to ESP32, run: flash_esp32.bat
echo.
goto :done

:error
echo.
echo  ===================================
echo   BUILD FAILED
echo  ===================================
echo  Check build_log.txt for details.
echo.

:done
echo Press any key to close...
pause >nul
endlocal
