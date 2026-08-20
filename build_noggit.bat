@echo off
REM NoggitRedB Windows Build Script

echo ==========================================
echo NoggitRedB Build Script (Windows)
echo ==========================================

REM Check if Qt5 is installed
if not defined Qt5_DIR (
    echo.
    echo WARNING: Qt5_DIR environment variable not set
    echo Please set Qt5_DIR to your Qt5 installation directory
    echo Example: set Qt5_DIR=C:\Qt\5.15\msvc2019_64
    echo.
)

echo.
echo ==========================================
echo Creating build directory...
echo ==========================================
if not exist build mkdir build
cd build

echo.
echo ==========================================
echo Running CMake configuration...
echo ==========================================
if defined Qt5_DIR (
    cmake ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_PREFIX_PATH=%Qt5_DIR% ^
        -DNOGGIT_BUILD_NODE_DATAMODELS=ON ^
        -DNOGGIT_ENABLE_TRACY_PROFILER=OFF ^
        -G "Visual Studio 17 2022" ^
        -A x64 ^
        ..
) else (
    cmake ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DNOGGIT_BUILD_NODE_DATAMODELS=ON ^
        -DNOGGIT_ENABLE_TRACY_PROFILER=OFF ^
        -G "Visual Studio 17 2022" ^
        -A x64 ^
        ..
)

if errorlevel 1 (
    echo CMake configuration failed
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Building NoggitRedB...
echo ==========================================
cmake --build . --config Release -j

if errorlevel 1 (
    echo Build failed
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Build Complete!
echo ==========================================
echo Executable location: build\bin\noggit.exe
echo.
pause
