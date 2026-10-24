@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM  chick-watch animation frame dump - double click to run.
REM  No bash / no command line knowledge needed.
REM
REM  Double click          : dump happy jump, 80 frames (66ms apart)
REM  With arguments        : anim-frames.bat <happy^|angry^|sleep> [count] [every]
REM  Example               : anim-frames.bat angry 120 2
REM
REM  Output: E:\Projects\tools\frames_<name>\f_000.png ...
REM ============================================================

set SIM=E:\Projects\chick-watch-assets\lvgl-simulator
set PY=C:\Users\jonyl\.workbuddy\binaries\python\envs\default\Scripts\python.exe
set MINGW=C:\Users\jonyl\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin
set SDLBIN=E:\Projects\tools\SDL2-2.32.10\x86_64-w64-mingw32\bin
set CMAKEBIN=C:\Program Files\CMake\bin

set DEMO=%1
if "%DEMO%"=="" set DEMO=happy
set COUNT=%2
if "%COUNT%"=="" set COUNT=80
set EVERY=%3
if "%EVERY%"=="" set EVERY=2

set OUT=E:\Projects\tools\frames_%DEMO%
set PREFIX=%OUT%/f

if not exist "%OUT%" mkdir "%OUT%"
del /q "%OUT%\*.bmp" 2>nul
del /q "%OUT%\*.png" 2>nul

echo [1/2] Rendering and dumping: %DEMO% (%COUNT% frames, every %EVERY% x 33ms) ...
cd /d "%SIM%"
set PATH=%MINGW%;%SDLBIN%;%CMAKEBIN%;%PATH%
"bin\main.exe" --%DEMO%-demo --shot %COUNT% %EVERY% "%PREFIX%"

echo [2/2] Converting BMP to PNG ...
"%PY%" "%SIM%\tools\bmp2png.py" "%PREFIX%"

echo.
echo Done: %OUT%
start "" "%OUT%"
endlocal
