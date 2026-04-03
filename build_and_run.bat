@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "VCVARS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
    echo ERROR: vcvars64.bat not found: "%VCVARS%"
    pause
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    if errorlevel 1 (
        echo ERROR: failed to create build directory: "%BUILD_DIR%"
        pause
        exit /b 1
    )
)

call "%VCVARS%" x64
if errorlevel 1 (
    echo ERROR: failed to initialize VS2019 environment.
    pause
    exit /b 1
)

cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 16 2019" -A x64 -T v142
if errorlevel 1 (
    echo ERROR: CMake configure failed.
    pause
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release --target Rendering
if errorlevel 1 (
    echo ERROR: build failed.
    pause
    exit /b 1
)

if not exist "%BUILD_DIR%\Rendering.exe" (
    echo ERROR: executable not found: "%BUILD_DIR%\Rendering.exe"
    pause
    exit /b 1
)

pushd "%BUILD_DIR%"
start "" /D "%BUILD_DIR%" Rendering.exe -s 3
set "RUN_EXIT=%ERRORLEVEL%"
popd

if not "%RUN_EXIT%"=="0" (
    echo ERROR: Rendering.exe exited with code %RUN_EXIT%.
    pause
    exit /b %RUN_EXIT%
)

endlocal
exit /b 0
