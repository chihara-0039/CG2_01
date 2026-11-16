@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%V in (`"%VSWHERE%" -latest -products * -property productPath`) do set "CG2_IDE=%%V"
if not defined CG2_IDE (
  echo Visual Studio was not found.
  pause
  exit /b 1
)
start "" "%CG2_IDE%" "%~dp0project\CG2_01.sln"
