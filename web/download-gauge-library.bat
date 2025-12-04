@echo off
REM OpenPonyLogger - Canvas-Gauges Download Helper (Windows)
REM This script downloads the canvas-gauges library for offline use

echo ==========================================
echo OpenPonyLogger - Gauge Library Downloader
echo ==========================================
echo.

REM Check if curl is available (Windows 10 1803+ includes curl)
where curl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: curl not found
    echo.
    echo Please download manually from:
    echo https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js
    echo.
    echo Or install curl from: https://curl.se/windows/
    pause
    exit /b 1
)

echo Downloading canvas-gauges library...
echo Source: GitHub Releases (v2.1.7)
echo.

curl -L -o gauge.min.js https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js

if %ERRORLEVEL% EQU 0 (
    if exist gauge.min.js (
        echo.
        echo SUCCESS: Download complete!
        echo File: gauge.min.js
        for %%A in (gauge.min.js) do echo Size: %%~zA bytes
        echo.
        echo File placement:
        echo   Place gauge.min.js in the same directory as index.html
        echo.
        echo Test offline operation:
        echo   1. Disconnect from internet
        echo   2. Open index.html in browser
        echo   3. Verify all gauges display
        echo.
        echo Ready for offline deployment!
    ) else (
        echo ERROR: Download failed - file not found
    )
) else (
    echo ERROR: Download failed
    echo.
    echo Alternative: Download manually from:
    echo https://github.com/Mikhus/canvas-gauges/releases/download/v2.1.7/gauge.min.js
)

echo.
pause
