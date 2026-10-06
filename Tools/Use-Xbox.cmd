@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0HidHideMode.ps1" -Mode Xbox
if errorlevel 1 pause
