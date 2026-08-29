@echo off
setlocal EnableExtensions

rem Generate dc_*_layout.h via dc/tools/CMakeLists.txt (portable).
rem
rem Usage:
rem   gen_layouts.cmd [PORT_DIR]
rem
rem   PORT_DIR  Directory with dc_variable_cfg.h / dc_param_cfg.h;
rem             layout headers are written here.
rem             Default: %DC_ROOT%\test\port
rem
rem Optional environment:
rem   DC_PORT_DIR   Same as PORT_DIR (argument wins).
rem   DC_TOOLS_BUILD  CMake build directory (default: <tools>\build).

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

if not defined DC_ROOT set "DC_ROOT=%SCRIPT_DIR%\.."
for %%I in ("%DC_ROOT%") do set "DC_ROOT=%%~fI"

set "PORT_DIR=%~1"
if not defined PORT_DIR set "PORT_DIR=%DC_PORT_DIR%"
if not defined PORT_DIR set "PORT_DIR=%DC_ROOT%\test\port"
for %%I in ("%PORT_DIR%") do set "PORT_DIR=%%~fI"

if not exist "%PORT_DIR%\dc_variable_cfg.h" (
    echo [gen_layouts] ERROR: missing "%PORT_DIR%\dc_variable_cfg.h"
    exit /b 1
)
if not exist "%PORT_DIR%\dc_param_cfg.h" (
    echo [gen_layouts] ERROR: missing "%PORT_DIR%\dc_param_cfg.h"
    exit /b 1
)

if not defined DC_TOOLS_BUILD set "DC_TOOLS_BUILD=%SCRIPT_DIR%\build"
for %%I in ("%DC_TOOLS_BUILD%") do set "DC_TOOLS_BUILD=%%~fI"

where cmake >nul 2>&1
if errorlevel 1 (
    echo [gen_layouts] ERROR: cmake not in PATH.
    exit /b 1
)

echo [gen_layouts] DC_ROOT=%DC_ROOT%
echo [gen_layouts] PORT_DIR=%PORT_DIR%
echo [gen_layouts] BUILD_DIR=%DC_TOOLS_BUILD%

set "CMAKE_GEN="
set "CMAKE_EXTRA="
if exist "%DC_TOOLS_BUILD%\CMakeCache.txt" (
    goto :do_configure
)
where mingw32-make >nul 2>&1
if not errorlevel 1 (
    set "CMAKE_GEN=MinGW Makefiles"
    set "CMAKE_EXTRA=-DCMAKE_C_COMPILER=gcc"
)
if not defined CMAKE_GEN (
    where ninja >nul 2>&1
    if not errorlevel 1 set "CMAKE_GEN=Ninja"
)

:do_configure
if defined CMAKE_GEN (
    cmake -G "%CMAKE_GEN%" %CMAKE_EXTRA% -S "%SCRIPT_DIR%" -B "%DC_TOOLS_BUILD%" -DDC_PORT_DIR="%PORT_DIR%"
) else (
    cmake -S "%SCRIPT_DIR%" -B "%DC_TOOLS_BUILD%" -DDC_PORT_DIR="%PORT_DIR%"
)
if errorlevel 1 exit /b 1

cmake --build "%DC_TOOLS_BUILD%" --target gen_layouts
if errorlevel 1 exit /b 1

echo [gen_layouts] OK
exit /b 0
