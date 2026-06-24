@echo off
setlocal enabledelayedexpansion
title MeterBridge — Push to GitHub Pages
cd /d "%~dp0.."

echo [==========================================================]
echo   MeterBridge — GitHub Pages Update
echo   Copies screenshots + commits + pushes to GitHub Pages
echo [==========================================================]
echo.

REM ── 1. Copy screenshots into docs ──────────────────────────
echo [...] Copying live screenshots into docs\...
set SRC=%~dp0..\theme_captures
set DST=%~dp0

if not exist "%SRC%" (
    echo [WARN] theme_captures folder not found — skipping image copy.
    echo        Images may already be in docs\, continuing with push.
    goto :git_stage
)

copy /Y "%SRC%\theme_03_130930.png" "%DST%shot_meters_probcast.png" >nul
copy /Y "%SRC%\theme_17_131731.png" "%DST%shot_landscape.png"       >nul
copy /Y "%SRC%\theme_02_130919.png" "%DST%shot_settings.png"        >nul
copy /Y "%SRC%\theme_07_131539.png" "%DST%shot_fft_fire.png"        >nul
copy /Y "%SRC%\theme_10_131612.png" "%DST%shot_fft_classic.png"     >nul
copy /Y "%SRC%\theme_13_131645.png" "%DST%shot_fft_rainbow.png"     >nul
copy /Y "%SRC%\theme_15_131707.png" "%DST%shot_fft_ghost.png"       >nul
copy /Y "%SRC%\theme_12_131635.png" "%DST%shot_fft_ocean.png"       >nul

if exist "C:\dev\mb\live_1.png"    ( copy /Y "C:\dev\mb\live_1.png"    "%DST%shot_meters_live.png" >nul ) else ( copy /Y "%SRC%\theme_03_130930.png" "%DST%shot_meters_live.png" >nul )
if exist "C:\dev\mb\check_now.png" ( copy /Y "C:\dev\mb\check_now.png" "%DST%shot_fft_live.png"    >nul ) else ( copy /Y "%SRC%\theme_07_131539.png" "%DST%shot_fft_live.png"    >nul )

echo [OK] Screenshots copied to docs\

:git_stage
REM ── 2. Stage all docs changes ──────────────────────────────
echo [...] Staging all docs\ changes...
git add docs/
if errorlevel 1 ( echo [ERROR] git add failed. & goto :fail )
echo [OK] Staged.

REM ── 3. Commit ──────────────────────────────────────────────
echo [...] Committing...
git commit -m "docs: add screenshots page + live theme captures for GitHub Pages

- Add docs/screenshots.html — full visual explainer page:
    * 4-step signal chain diagram (JSFX → Lua → Python → ESP32)
    * Live REAPER session screenshots: Codeine Crazy + Pro Broadcast
    * Landscape mode (Deep Ocean, 270 degrees) showcase
    * Tabbed spectrum analyzer gallery (Fire/Classic/Rainbow/Ghost/Ocean)
    * All-themes settings page capture + annotated feature list
    * Lightbox viewer for full-size inspection

- Update docs/index.html:
    * Add Screenshots link to nav bar
    * Fix hero stat: 5 themes → 11 themes
    * Add live-screenshot teaser grid with link to screenshots page

- Add 10x real-hardware PNG captures from live REAPER stream session"

if errorlevel 1 (
    echo [SKIP] Nothing new to commit, or commit failed — checking status...
    git status
)

REM ── 4. Push ────────────────────────────────────────────────
echo [...] Pushing to origin/main...
git push origin main
if errorlevel 1 (
    echo [ERROR] Push failed. Trying 'master' branch...
    git push origin master
    if errorlevel 1 ( echo [ERROR] Push failed on both main and master. & goto :fail )
)

echo.
echo [OK] Done! GitHub Pages will update at:
echo      https://s0wingseason.github.io/ESP32-Touch-Display-REAPER-Advanced-Metering-and-Transport-Controller/
echo.
echo      (GitHub Pages typically takes 30-60 seconds to rebuild)
echo.
goto :end

:fail
echo.
echo [ERROR] Something went wrong. Check the output above.
echo.

:end
pause
