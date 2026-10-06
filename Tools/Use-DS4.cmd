@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0HidHideMode.ps1" -Mode DS4
if errorlevel 1 pause
