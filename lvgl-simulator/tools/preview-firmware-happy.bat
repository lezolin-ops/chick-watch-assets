@echo off
REM Preview the FIRMWARE happy animation in a window, looping forever.
REM Same code path a tap runs on the real device (6 keyframes + programmatic
REM squash/stretch, 1600ms), not the AI sequence used by --happy-demo.
REM Usage: preview-firmware-happy.bat [gap_ms]

setlocal
set SIM=E:\Projects\chick-watch-assets\lvgl-simulator
set MINGW=C:\Users\jonyl\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin
set SDLBIN=E:\Projects\tools\SDL2-2.32.10\x86_64-w64-mingw32\bin

set GAP=%1
if "%GAP%"=="" set GAP=2600

cd /d "%SIM%"
set PATH=%MINGW%;%SDLBIN%;%PATH%
start "" "bin\main.exe" --happy-loop %GAP%
endlocal
