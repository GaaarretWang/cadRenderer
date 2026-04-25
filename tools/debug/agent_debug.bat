@echo off
setlocal EnableExtensions

set "ROOT=%~dp0..\.."
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "EXE_PATH=%BUILD_DIR%\Rendering.exe"
set "VCVARS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
set "AGENT_EXIT_CODE=0"

if not exist "%VCVARS%" (
    echo ERROR: vcvars64.bat not found: "%VCVARS%"
    exit /b 1
)

REM Keep the shared environment setup above stable.
REM Always launch my own build/run/compare/git/remove commands from this file.
REM Unified invocation from repo root: & .\tools\debug\agent_debug.bat
REM Edit only the block below when a one-off debug command is needed.
REM Typical commands in the editable block include build, run, compare,
REM git add/commit, file cleanup, and process cleanup.
REM AGENT_EDIT_START
pushd "%ROOT%" >nul
git add tools\debug\agent_debug.bat
if errorlevel 1 goto agent_done

git commit -m "chore: reset agent debug template"

:agent_done
set "AGENT_EXIT_CODE=%ERRORLEVEL%"
popd >nul
REM AGENT_EDIT_END

exit /b %AGENT_EXIT_CODE%
