@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "EXE_PATH=%BUILD_DIR%\Rendering.exe"
set "SCENES_JSON=%ROOT%\asset\data\json\Scenes.json"
set "EXTRA_RENDER_ARGS="

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--help" goto show_help
set "EXTRA_RENDER_ARGS=!EXTRA_RENDER_ARGS! %~1"
shift
goto parse_args

:args_done
echo === Start Testing All Scenes ===
echo Test mode: save
echo Current directory: %ROOT%
echo.

if not exist "%EXE_PATH%" (
    echo Rendering executable not found, building first...
    echo.
    call "%ROOT%\build_and_run.bat" --build-only
    if errorlevel 1 (
        echo ERROR: build failed.
        exit /b 1
    )
    if not exist "%EXE_PATH%" (
        echo ERROR: executable still not found: "%EXE_PATH%"
        exit /b 1
    )
) else (
    echo Found executable: %EXE_PATH%
    echo.
)

set "PASS_COUNT=0"
set "FAIL_COUNT=0"
set "TOTAL_COUNT=0"

for /f "tokens=2 delims=:" %%A in ('findstr /R /C:"\"id\": [0-9][0-9]*" "%SCENES_JSON%"') do (
    for /f "tokens=1 delims=, " %%I in ("%%A") do (
        set /a TOTAL_COUNT+=1
        set "SCENE_ID=%%I"
        set "SCENE_NAME=scene_%%I"
        set "SCENE_PASS=1"
        set "LOG_FILE=%TEMP%\scene_test_%%I_!RANDOM!.log"

        if "%%I"=="0" set "SCENE_NAME=airplane_parts"
        if "%%I"=="1" set "SCENE_NAME=dashboard"
        if "%%I"=="2" set "SCENE_NAME=spheres"
        if "%%I"=="3" set "SCENE_NAME=whole_engine"
        if "%%I"=="4" set "SCENE_NAME=helicopter_engine"
        if "%%I"=="5" set "SCENE_NAME=cockpit"
        if "%%I"=="6" set "SCENE_NAME=glass_models_all"
        if "%%I"=="7" set "SCENE_NAME=cangbi"
        if "%%I"=="8" set "SCENE_NAME=airplane_parts_cull_off_pcf"
        if "%%I"=="9" set "SCENE_NAME=airplane_parts_cull_on_pcss"
        if "%%I"=="10" set "SCENE_NAME=spheres_depth_plane_pcss"
        if "%%I"=="11" set "SCENE_NAME=spheres_depth_real_pcf"

        echo Testing scene !SCENE_ID!: !SCENE_NAME!
        echo ----------------------------------------
        echo Running command: (cd /d "%BUILD_DIR%" ^&^& Rendering.exe --scene !SCENE_ID! --frames 2 --save-ref!EXTRA_RENDER_ARGS!)

        pushd "%BUILD_DIR%" >nul
        "%EXE_PATH%" --scene !SCENE_ID! --frames 2 --save-ref!EXTRA_RENDER_ARGS! > "!LOG_FILE!" 2>&1
        set "EXIT_CODE=!ERRORLEVEL!"
        popd >nul

        type "!LOG_FILE!"
        echo Program exit code: !EXIT_CODE!

        findstr /C:"Program stopped" "!LOG_FILE!" >nul
        if errorlevel 1 (
            echo X Scene !SCENE_ID! ^(!SCENE_NAME!^) did not report "Program stopped"
            set "SCENE_PASS=0"
        ) else (
            echo OK Scene !SCENE_ID! ^(!SCENE_NAME!^) exited normally
        )

        findstr /C:"Reference saved:" "!LOG_FILE!" >nul
        if errorlevel 1 (
            echo X Scene !SCENE_ID! ^(!SCENE_NAME!^) failed to save reference
            set "SCENE_PASS=0"
        ) else (
            echo OK Scene !SCENE_ID! ^(!SCENE_NAME!^) reference saved
        )

        if "!SCENE_PASS!"=="1" (
            set /a PASS_COUNT+=1
        ) else (
            set /a FAIL_COUNT+=1
        )

        echo.
        echo Detailed log saved to: !LOG_FILE!
        echo.
    )
)

echo === Testing Complete ===
echo.
echo Result: %PASS_COUNT%/%TOTAL_COUNT% passed, %FAIL_COUNT%/%TOTAL_COUNT% failed
echo.
echo Reference directory: "%ROOT%\test_references"
if exist "%ROOT%\test_references" (
    dir "%ROOT%\test_references"
) else (
    echo (directory does not exist)
)
echo.
echo Hint: detailed logs are stored as "%TEMP%\scene_test_*.log"

if %FAIL_COUNT% gtr 0 exit /b 1
exit /b 0

:show_help
echo Usage: test_all_scenes_save.bat [extra Rendering args]
echo.
echo Saves all scene outputs as reference images.
exit /b 0
