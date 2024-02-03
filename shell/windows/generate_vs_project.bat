@echo off
setlocal EnableDelayedExpansion
cmake --version >nul 2>&1 && (
    set cmake=1
) || (
    echo please install cmake first
)

if !cmake! equ 1 (
    cd /d "%~dp0..\.."
    echo 1: Visual Studio 2022
    echo 2: Visual Studio 2019
    echo 3: Visual Studio 2017
    set "num=1"
    set /p num="Enter your preference [1]: "
    if !num! equ 2 (
        set "generator=Visual Studio 16 2019" 
    )else if !num! equ 3 (
        set "generator=Visual Studio 15 2017"
    ) else (
        set "generator=Visual Studio 17 2022"
    )
    echo Generating !generator! ...
    cmake -B build -G "!generator!" -A x64
    start build\flycast.vcxproj
)

pause