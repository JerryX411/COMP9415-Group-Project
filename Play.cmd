@echo off
rem Run the merged source build, not the historical Playable snapshot.
call "%~dp0BuildAndRun.cmd" %*
exit /b %errorlevel%
