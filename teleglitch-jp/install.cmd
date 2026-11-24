@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
set "result=%errorlevel%"
if not "%result%"=="0" echo Operation failed. See the message above.
pause
exit /b %result%
