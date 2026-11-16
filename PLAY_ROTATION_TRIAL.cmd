@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0project\Tools\StartRotationTrial.ps1"
if errorlevel 1 pause
