@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0..\.."
cd /d "%ROOT%"
set "ROOT=%CD%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_INSTALL="
set "DEVENV_EXE="
set "GIT_EXE="

call :find_vs
if not defined VS_INSTALL (
    echo Visual Studio Community 2022 with Desktop development for C++ is required.
    set "install_vs=n"
    set /p install_vs="Install it now? This may show a Windows administrator prompt. [y/N]: "
    if /i "!install_vs!"=="y" (
        call :install_vs_community
        call :find_vs
    )
)

if not defined VS_INSTALL (
    echo.
    echo Visual Studio 2022 with MSVC x64/x86 build tools was not found.
    echo Install "Desktop development with C++" from:
    echo https://visualstudio.microsoft.com/vs/community/
    goto :fail
)

call :find_cmake
if not defined CMAKE_EXE (
    echo.
    echo CMake was not found. Re-run the Visual Studio installer and include C++ CMake tools for Windows.
    goto :fail
)

if not exist "core\deps\glslang\CMakeLists.txt" (
    echo Git submodules are missing.
    call :find_git
    if not defined GIT_EXE (
        echo.
        echo Git was not found. Re-run the Visual Studio installer and include Git for Windows.
        goto :fail
    )

    if not exist ".git" (
        call :init_zip_checkout
        if errorlevel 1 goto :fail
    ) else (
        "!GIT_EXE!" rev-parse --verify HEAD >nul 2>&1
        if errorlevel 1 (
            call :init_zip_checkout
            if errorlevel 1 goto :fail
        ) else (
            set "init_submodules=n"
            set /p init_submodules="Initialize git submodules now? [y/N]: "
            if /i "!init_submodules!"=="y" (
                call :init_submodules
                if errorlevel 1 goto :fail
            ) else (
                goto :fail
            )
        )
    )
)

echo.
echo Generating Visual Studio 2022 x64 project...
"%CMAKE_EXE%" -B build -G "Visual Studio 17 2022" -A x64 -DUSE_DX9=OFF
if errorlevel 1 goto :fail

call :say_success "Visual Studio project generated."

if exist "build\flycast.sln" (
    set "PROJECT_FILE=%CD%\build\flycast.sln"
) else (
    if exist "build\flycast.vcxproj" (
        set "PROJECT_FILE=%CD%\build\flycast.vcxproj"
    ) else (
        echo.
        echo Could not find build\flycast.sln or build\flycast.vcxproj.
        goto :fail
    )
)

echo.
echo Opening:
echo !PROJECT_FILE!
call :find_devenv
if not defined DEVENV_EXE (
    echo.
    echo Visual Studio IDE was not found.
    goto :fail
)
start "" "!DEVENV_EXE!" "!PROJECT_FILE!"

echo.
call :say_success "Build and run in Visual Studio:"
echo Once the project opens in Visual Studio, press F5 to build, run, and debug.
echo If Visual Studio stops on 0xC0000005, uncheck "Break when this exception type is thrown".
goto :done

:find_vs
set "VS_INSTALL="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -version [17.0^,18.0^) -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_INSTALL=%%i"
    )
)
exit /b 0

:find_devenv
set "DEVENV_EXE="
if defined VS_INSTALL (
    set "VS_DEVENV=%VS_INSTALL%\Common7\IDE\devenv.exe"
    if exist "!VS_DEVENV!" (
        set "DEVENV_EXE=!VS_DEVENV!"
        exit /b 0
    )
)
exit /b 0

:find_cmake
set "CMAKE_EXE="
if defined VS_INSTALL (
    set "VS_CMAKE=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if exist "!VS_CMAKE!" (
        set "CMAKE_EXE=!VS_CMAKE!"
        exit /b 0
    )
)
exit /b 0

:find_git
set "GIT_EXE="
if defined VS_INSTALL (
    set "VS_GIT=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\mingw64\bin\git.exe"
    if exist "!VS_GIT!" (
        set "GIT_EXE=!VS_GIT!"
        exit /b 0
    )

    set "VS_GIT=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe"
    if exist "!VS_GIT!" (
        set "GIT_EXE=!VS_GIT!"
        exit /b 0
    )
)
exit /b 0

:init_submodules
"!GIT_EXE!" submodule update --init --recursive
exit /b %errorlevel%

:init_zip_checkout
"!GIT_EXE!" init
if errorlevel 1 exit /b 1

"!GIT_EXE!" remote get-url origin >nul 2>&1
if errorlevel 1 (
    "!GIT_EXE!" remote add origin https://github.com/flyinghead/flycast.git
    if errorlevel 1 exit /b 1
) else (
    "!GIT_EXE!" remote set-url origin https://github.com/flyinghead/flycast.git
    if errorlevel 1 exit /b 1
)

"!GIT_EXE!" fetch --depth 1 origin master
if errorlevel 1 exit /b 1

"!GIT_EXE!" reset --mixed FETCH_HEAD
if errorlevel 1 exit /b 1

"!GIT_EXE!" submodule update --init --recursive
exit /b %errorlevel%

:install_vs_community
set "INSTALLER=%TEMP%\vs_community.exe"
echo Downloading Visual Studio Community 2022 bootstrapper...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_Community.exe' -OutFile $env:TEMP\vs_community.exe"
if errorlevel 1 goto :fail

echo Installing Visual Studio Community 2022. This can take a while.
"%INSTALLER%" --passive --wait --norestart --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.CMake.Project --add Microsoft.VisualStudio.Component.TeamExplorer.MinGit --includeRecommended
exit /b %errorlevel%

:say_success
set "FLYCAST_MESSAGE=%~1"
powershell -NoProfile -Command "Write-Host $env:FLYCAST_MESSAGE -ForegroundColor Green" 2>nul
if errorlevel 1 echo %~1
set "FLYCAST_MESSAGE="
exit /b 0

:fail
echo.
echo Project generation failed.
pause
exit /b 1

:done
pause
