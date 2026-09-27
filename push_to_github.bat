@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0push_to_github.ps1"
pause
