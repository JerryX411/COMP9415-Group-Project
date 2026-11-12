@echo off
setlocal
cd /d "%~dp0"
set "TEMP=%~dp0.temp"
set "TMP=%~dp0.temp"
if not exist "%TEMP%" mkdir "%TEMP%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build.ps1"
if errorlevel 1 goto failed
if /I "%~1"=="--build-only" exit /b 0
if exist "%~dp0build\Release\CoreExplorerLite.exe" (
 cd /d "%~dp0build\Release"
 start "" "%~dp0build\Release\CoreExplorerLite.exe"
) else (
 cd /d "%~dp0build"
 start "" "%~dp0build\CoreExplorerLite.exe"
)
exit /b 0
:failed
echo.
echo Build failed. Please send the error text above.
if /I not "%~1"=="--build-only" pause
exit /b 1
