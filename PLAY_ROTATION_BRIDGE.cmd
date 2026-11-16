@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0project\Tools\OpenRotationCourse.ps1" -Course bridge
if errorlevel 1 pause
