@echo off
REM Build Flycast (x64 Release) with the Dreamcast 16MB VRAM option -- same recipe as upstream CI's
REM x86_64-pc-windows-msvc job: a VS x64 environment, CMake, Ninja.
REM
REM MSYS2 must be kept OUT: with C:\msys64\mingw64\bin on PATH, CMake found MSYS2's SDL2, pkg-config
REM and binutils and handed MinGW's headers to MSVC (hundreds of C2059 errors in stdio.h). So the PATH
REM is reset to Windows only before vcvarsall, and the MSYS2 prefixes are excluded from all searches.
set "PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 || exit /b 1
set "VSCM=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%VSCM%\CMake\bin;%VSCM%\Ninja;%PATH%"
cd /d "%~dp0"
if exist build rmdir /s /q build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_DX9=OFF ^
      "-DCMAKE_IGNORE_PREFIX_PATH=C:/msys64/mingw64;C:/msys64/usr;C:/msys64" ^
      "-DCMAKE_IGNORE_PATH=C:/msys64/mingw64/bin;C:/msys64/usr/bin" || exit /b 1
cmake --build build --config Release || exit /b 1
echo BUILD OK
