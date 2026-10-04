@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "SERTOS_DIR=%SCRIPT_DIR%\..\sertos"
pushd "%SCRIPT_DIR%" || exit /b 1
set "CLEAN_BUILD=0"
set "INSTRUMENTED=OFF"
set "BUILD_CONFIG=Debug"

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="-c" goto :parse_clean
if /i "%~1"=="-i" goto :parse_instrumented
if /i "%~1"=="-h" goto :show_help
if /i "%~1"=="--help" goto :show_help
if /i "%~1"=="debug" goto :parse_debug
if /i "%~1"=="release" goto :parse_release
if /i "%~1"=="renode" goto :parse_renode
echo [ERROR] Unknown option: %~1
goto :invalid_args

:parse_clean
set "CLEAN_BUILD=1"
shift
goto :parse_args

:parse_instrumented
set "INSTRUMENTED=ON"
shift
goto :parse_args

:parse_debug
set "BUILD_CONFIG=Debug"
shift
goto :parse_args

:parse_release
set "BUILD_CONFIG=Release"
shift
goto :parse_args

:parse_renode
set "BUILD_CONFIG=Renode"
shift
goto :parse_args

:args_done
where cmake.exe >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake was not found on PATH.
    goto :build_error
)

if "%INSTRUMENTED%"=="ON" (
    echo [BUILD] Instrumented SertOS Cortex-M0+ library
    cmake -S "%SERTOS_DIR%" -B "build\sertos-instrumented" -G Ninja -D "CMAKE_TOOLCHAIN_FILE:FILEPATH=%SCRIPT_DIR%\cmake\gcc-arm-none-eabi.cmake" -D "SERTOS_PORT=cortex-m0plus"
    if errorlevel 1 goto :build_error
    cmake --build build\sertos-instrumented --target sertos_kernel_instrumented
    if errorlevel 1 goto :build_error
)

echo [CONFIGURE] %BUILD_CONFIG% build; instrumented=%INSTRUMENTED%
cmake --preset %BUILD_CONFIG% -DSERTOS_APP_INSTRUMENTED=%INSTRUMENTED%
if errorlevel 1 goto :build_error

if "%CLEAN_BUILD%"=="1" (
    echo [CLEAN] Removing %BUILD_CONFIG% build outputs...
    cmake --build build\%BUILD_CONFIG% --target clean
    if errorlevel 1 goto :build_error
)

echo [BUILD] STM32G070 %BUILD_CONFIG% firmware
cmake --build --preset %BUILD_CONFIG%
if errorlevel 1 goto :build_error

echo [SUCCESS] Firmware build completed.
popd
endlocal
exit /b 0

:show_help
echo.
echo Usage: build.bat [Debug^|Release^|Renode] [-c] [-i]
echo.
echo Options:
echo   Debug    Build the Debug preset (default)
echo   Release  Build the Release preset
echo   Renode   Build the Renode preset
echo   -c       Clean the selected preset outputs before building
echo   -i  Build the instrumented SertOS library and instrument selected app sources
echo   -h  Show this help
echo.
echo Examples:
echo   build.bat
echo   build.bat Release
echo   build.bat Renode
echo   build.bat -i
echo   build.bat Release -c
echo   build.bat Renode -i
echo   build.bat -c -i
echo.
popd
endlocal
exit /b 0

:invalid_args
echo Usage: build.bat [Debug^|Release^|Renode] [-c] [-i]
popd
endlocal
exit /b 2

:build_error
echo [ERROR] Firmware build failed.
popd
endlocal
exit /b 1
