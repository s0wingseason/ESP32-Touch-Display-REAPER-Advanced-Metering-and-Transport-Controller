@echo off
setlocal enabledelayedexpansion
echo.
echo  ===================================================
echo   MeterBridge v2.0-RC1 - Release Packaging
echo  ===================================================
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package_rc1.ps1"
echo.
echo  Press any key to close...
pause >nul
endlocal
