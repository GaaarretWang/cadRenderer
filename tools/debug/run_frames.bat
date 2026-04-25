@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0..\.."
set "RUN_ARGS="

if "%~1"=="" (
    set "RUN_ARGS=--scene 0 --frames 2000"
) else (
    :parse_args
    if "%~1"=="" goto args_done
    set "RUN_ARGS=!RUN_ARGS! %1"
    shift
    goto parse_args
)

:args_done
pushd "%ROOT%" >nul
call "%ROOT%\build_and_run.bat" !RUN_ARGS!
set "EXIT_CODE=%ERRORLEVEL%"
popd >nul

exit /b %EXIT_CODE%
