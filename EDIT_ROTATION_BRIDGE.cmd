@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0project\Tools\OpenRotationCourse.ps1" -Course bridge -Editor
if errorlevel 1 pause
