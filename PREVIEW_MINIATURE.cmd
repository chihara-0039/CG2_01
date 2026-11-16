@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0project\Tools\StartDiorama.ps1"
if errorlevel 1 pause
