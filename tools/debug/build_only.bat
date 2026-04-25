@echo off
setlocal EnableExtensions

set "ROOT=%~dp0..\.."
pushd "%ROOT%" >nul
call "%ROOT%\build_and_run.bat" --build-only
set "EXIT_CODE=%ERRORLEVEL%"
popd >nul

exit /b %EXIT_CODE%
