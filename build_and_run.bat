@echo off
setlocal

set "BUILD_ONLY=0"
if /I "%~1"=="--build-only" set "BUILD_ONLY=1"

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "EXE_PATH=%BUILD_DIR%\Rendering.exe"
set "VCVARS=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
    echo ERROR: vcvars64.bat not found: "%VCVARS%"
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    if errorlevel 1 (
        echo ERROR: failed to create build directory: "%BUILD_DIR%"
        exit /b 1
    )
)

call "%VCVARS%" x64
if errorlevel 1 (
    echo ERROR: failed to initialize VS2019 environment.
    exit /b 1
)

echo [1/3] Configuring...
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 16 2019" -A x64 -T v142
if errorlevel 1 (
    echo ERROR: CMake configure failed.
    exit /b 1
)

echo [2/3] Building...
cmake --build "%BUILD_DIR%" --config Release --target Rendering -- /m
if errorlevel 1 (
    echo ERROR: build failed.
    exit /b 1
)

if not exist "%EXE_PATH%" (
    echo ERROR: executable not found: "%EXE_PATH%"
    exit /b 1
)

if "%BUILD_ONLY%"=="1" (
    echo Build completed successfully.
    exit /b 0
)

echo [3/3] Running...
pushd "%BUILD_DIR%"
"%EXE_PATH%" -s 6
popd

exit /b 0
