rem download mingw: https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/8.1.0/threads-posix/seh/
set EXTRA_PATH=C:\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64\bin\
set PATH=%EXTRA_PATH%;%PATH%
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=artifact -G "MinGW Makefiles"
cmake --build build --config Release --parallel 8