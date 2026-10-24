@echo off
REM Art-driven happy response: swaps the 6 AI keyframes during the response.
REM Compare with preview-firmware-happy.bat (single bouncing sprite).
setlocal
set SIM=E:\Projects\chick-watch-assets\lvgl-simulator
set MINGW=C:\Users\jonyl\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin
set SDLBIN=E:\Projects\tools\SDL2-2.32.10\x86_64-w64-mingw32\bin
cd /d "%SIM%"
set PATH=%MINGW%;%SDLBIN%;%PATH%
start "" "bin\main.exe" --happy-keys --happy-loop 2600
endlocal
