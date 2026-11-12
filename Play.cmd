@echo off
setlocal
cd /d "%~dp0"
if exist "%~dp0CoreExplorer.exe" (
  start "" "%~dp0CoreExplorer.exe" %*
  exit /b 0
)
if exist "%~dp0build\Release\CoreExplorer.exe" (
  start "" "%~dp0build\Release\CoreExplorer.exe" %*
  exit /b 0
)
if exist "%~dp0build\CoreExplorer.exe" (
  start "" "%~dp0build\CoreExplorer.exe" %*
  exit /b 0
)
call "%~dp0BuildAndRun.cmd" %*
exit /b %errorlevel%
