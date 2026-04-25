@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0..\.."
set "EXTRA_ARGS="

:parse_args
if "%~1"=="" goto args_done
set "EXTRA_ARGS=!EXTRA_ARGS! %1"
shift
goto parse_args

:args_done
pushd "%ROOT%" >nul
call "%ROOT%\test_all_scenes_compare.bat" !EXTRA_ARGS!
set "EXIT_CODE=%ERRORLEVEL%"
popd >nul

exit /b %EXIT_CODE%
