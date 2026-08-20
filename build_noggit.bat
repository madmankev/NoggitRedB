@echo off
REM NoggitRedB Windows Build Script - Automated

setlocal enabledelayedexpansion

echo.
echo ==========================================
echo NoggitRedB Windows Build (Automated)
echo ==========================================
echo.

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
cd /d %SCRIPT_DIR%

REM Check if Qt5 is installed, if not try to find it
if not defined Qt5_DIR (
    echo Searching for Qt5 installation...
    
    REM Common Qt5 installation paths
    if exist "C:\Qt\5.15.2\msvc2019_64" (
        set Qt5_DIR=C:\Qt\5.15.2\msvc2019_64
        echo Found Qt5 at !Qt5_DIR!
    ) else if exist "C:\Qt\5.15\msvc2019_64" (
        set Qt5_DIR=C:\Qt\5.15\msvc2019_64
        echo Found Qt5 at !Qt5_DIR!
    ) else if exist "C:\Qt" (
        for /d %%G in (C:\Qt\*) do (
            if exist "%%G\msvc2019_64\bin\qmake.exe" (
                set Qt5_DIR=%%G\msvc2019_64
                echo Found Qt5 at !Qt5_DIR!
                goto :qt_found
            )
        )
    )
    
    :qt_found
    if not defined Qt5_DIR (
        echo.
        echo ERROR: Qt5 not found in common locations
        echo Please set Qt5_DIR environment variable:
        echo set Qt5_DIR=C:\path\to\Qt\5.15\msvc2019_64
        echo.
        pause
        exit /b 1
    )
)

echo Qt5 location: !Qt5_DIR!
echo.

REM Detect Visual Studio version
set VS_GENERATOR=
set VS_PATH=

echo Checking for Visual Studio installations...

REM Check for VS 2026
if exist "C:\Program Files\Microsoft Visual Studio\2026" (
    set VS_GENERATOR=Visual Studio 18 2026
    set VS_PATH=C:\Program Files\Microsoft Visual Studio\2026
    echo Found Visual Studio 2026
) REM Check for VS 2022
else if exist "C:\Program Files\Microsoft Visual Studio\2022" (
    set VS_GENERATOR=Visual Studio 17 2022
    set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022
    echo Found Visual Studio 2022
) REM Check for VS 2019
else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019" (
    set VS_GENERATOR=Visual Studio 16 2019
    set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019
    echo Found Visual Studio 2019
) else (
    echo ERROR: No supported Visual Studio version found
    echo Please install Visual Studio 2019, 2022, or 2026 with C++ workload
    pause
    exit /b 1
)

echo Using: !VS_GENERATOR!
echo.

REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found in PATH
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Creating build directory...
echo ==========================================
if exist build (
    echo Cleaning old build directory...
    rmdir /s /q build
)
mkdir build
cd build

echo.
echo ==========================================
echo Running CMake configuration...
echo ==========================================
cmake ^
    -DCMAKE_PREFIX_PATH="!Qt5_DIR!" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DNOGGIT_BUILD_NODE_DATAMODELS=ON ^
    -DNOGGIT_ENABLE_TRACY_PROFILER=OFF ^
    -G "!VS_GENERATOR!" ^
    -A x64 ^
    ..

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed
    echo.
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Building NoggitRedB (Release)...
echo ==========================================
echo This may take 30-60 minutes on first build
echo.

cmake --build . --config Release -j 4

if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    echo.
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Build Complete!
echo ==========================================
echo.
echo Executable location: %SCRIPT_DIR%build\bin\noggit.exe
echo.
echo You can now run:
echo   %SCRIPT_DIR%build\bin\noggit.exe
echo.
pause
