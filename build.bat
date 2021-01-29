set BUILD_PATH=shell\linux

rem download mingw: https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/8.1.0/threads-posix/seh/
set EXTRA_PATH=C:\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64\bin\

set PATH=%EXTRA_PATH%;%PATH%

if not exist %BUILD_PATH% (mkdir %BUILD_PATH%)
cd %BUILD_PATH%

mingw32-make -j8 platform=win32 %*

mkdir artifacts
move reicast.exe artifacts\flycast.exe
move nosym-reicast.exe artifacts\nosym-flycast.exe
