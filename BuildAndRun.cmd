@echo off
setlocal
cd /d "%~dp0"
set "PATH=%~dp0toolchain\bin;%PATH%"
set "TEMP=%~dp0.temp"
set "TMP=%~dp0.temp"
if not exist "%TEMP%" mkdir "%TEMP%"
set "KIT_ROOT=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build.ps1"
if errorlevel 1 goto failed
if /I "%~1"=="--build-only" exit /b 0
cd /d "%~dp0build"
start "" "%~dp0build\CoreExplorerLite.exe"
exit /b 0
:failed
echo.
echo Build failed. Please send the error text above.
if /I not "%~1"=="--build-only" pause
exit /b 1
