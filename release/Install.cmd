@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install.ps1"
if errorlevel 1 echo Installation failed. Please read the message above; no successful installation was recorded.
pause
