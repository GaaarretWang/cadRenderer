@echo off
setlocal EnableExtensions

powershell -NoProfile -Command "Get-Process Rendering -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue"
exit /b %ERRORLEVEL%
