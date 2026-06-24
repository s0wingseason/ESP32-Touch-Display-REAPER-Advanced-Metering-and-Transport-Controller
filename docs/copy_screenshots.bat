@echo off
echo Copying screenshots into docs...
set SRC=C:\dev\mb\theme_captures
set DST=%~dp0

copy /Y "%SRC%\theme_03_130930.png" "%DST%shot_meters_probcast.png"
copy /Y "%SRC%\theme_17_131731.png" "%DST%shot_landscape.png"
copy /Y "%SRC%\theme_02_130919.png" "%DST%shot_settings.png"
copy /Y "%SRC%\theme_07_131539.png" "%DST%shot_fft_fire.png"
copy /Y "%SRC%\theme_10_131612.png" "%DST%shot_fft_classic.png"
copy /Y "%SRC%\theme_13_131645.png" "%DST%shot_fft_rainbow.png"
copy /Y "%SRC%\theme_15_131707.png" "%DST%shot_fft_ghost.png"
copy /Y "%SRC%\theme_12_131635.png" "%DST%shot_fft_ocean.png"

REM Shot of live meters (Codeine Crazy / first live capture)
if exist "C:\dev\mb\live_1.png" (
    copy /Y "C:\dev\mb\live_1.png" "%DST%shot_meters_live.png"
) else (
    copy /Y "%SRC%\theme_03_130930.png" "%DST%shot_meters_live.png"
)
if exist "C:\dev\mb\check_now.png" (
    copy /Y "C:\dev\mb\check_now.png" "%DST%shot_fft_live.png"
) else (
    copy /Y "%SRC%\theme_07_131539.png" "%DST%shot_fft_live.png"
)

echo.
echo Done! All screenshots are in the docs folder.
echo You can now open docs\screenshots.html in a browser to preview.
pause
