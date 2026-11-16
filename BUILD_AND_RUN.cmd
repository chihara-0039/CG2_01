@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0project\Tools\BuildRelease.ps1"
if errorlevel 1 (
  echo Build failed. The game was not started.
  pause
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0project\Tools\StartGame.ps1"
if errorlevel 1 pause
