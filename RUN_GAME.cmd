@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0project\Tools\StartGame.ps1"
if errorlevel 1 pause
