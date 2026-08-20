# NoggitRedB Windows Build Instructions

## Prerequisites

Before running the build script, ensure you have the following installed:

### 1. **Visual Studio 2022**
   - Download: https://visualstudio.microsoft.com/downloads/
   - Required workload: **Desktop development with C++**
   - Install C++ build tools and MSVC compiler

### 2. **CMake 3.11+**
   - Download: https://cmake.org/download/
   - Add to PATH during installation
   - Verify: Open Command Prompt and run `cmake --version`

### 3. **Qt5 (5.15.x recommended)**
   - Download: https://www.qt.io/download-open-source
   - Install to default location or remember the path
   - During installation, select **MSVC 2019 64-bit** component

## Quick Start

### Option 1: Automatic Build (Recommended)

Simply run the batch script - it will auto-detect your Qt5 installation:

```cmd
build_noggit.bat
```

The script will:
- Auto-detect Qt5 location
- Verify Visual Studio 2022 is installed
- Create a build directory
- Configure with CMake
- Compile in Release mode
- Output the executable to `build\bin\noggit.exe`

**Expected time:** 30-60 minutes on first build

### Option 2: Manual Build

If the script fails, follow these steps manually:

```cmd
REM Set Qt5 path if not auto-detected
set Qt5_DIR=C:\Qt\5.15.2\msvc2019_64

REM Create build directory
mkdir build
cd build

REM Configure with CMake
cmake -DCMAKE_PREFIX_PATH="%Qt5_DIR%" -G "Visual Studio 17 2022" -A x64 ..

REM Build in Release mode
cmake --build . --config Release -j 4

REM Run the application
.\bin\noggit.exe
```

## Troubleshooting

### CMake Not Found
- Ensure CMake is installed and added to PATH
- Restart Command Prompt after installation
- Verify: `cmake --version`

### Qt5 Not Found
- Manually set the environment variable:
  ```cmd
  set Qt5_DIR=C:\path\to\Qt\5.15\msvc2019_64
  ```
- Then run `build_noggit.bat` again

### Missing Dependencies
- The build will fail if dependencies are missing
- Check the CMake output for which library is missing
- Install via vcpkg if needed:
  ```cmd
  vcpkg install lua:x64-windows stormlib:x64-windows casclib:x64-windows
  ```

### Build Fails During Compilation
- Clear the build directory: `rmdir /s /q build`
- Run the script again
- Check that all dependencies are properly installed

### Long Compile Times
- First builds take 30-60 minutes due to template instantiation
- Subsequent builds are much faster (incremental)
- Do NOT interrupt the build process

## Output

After successful compilation, you'll find:
- **Executable:** `build\bin\noggit.exe`
- **Runtime files:** `build\bin\` (DLLs, assets, etc.)

Run the editor:
```cmd
build\bin\noggit.exe
```

## Additional Build Options

Edit `build_noggit.bat` and modify the CMake configuration to enable optional features:

```cmd
-DNOGGIT_ENABLE_TRACY_PROFILER=ON      # Enable profiler (slower)
-DFAST_BUILD_NOGGIT_JUMBO=ON           # Faster incremental builds
-DFAST_BUILD_NOGGIT_PCH=ON             # Use precompiled headers
```

## Support

- Official Discord: https://discord.gg/NqvM3xE5uS
- GitHub Issues: https://github.com/madmankev/NoggitRedB/issues
- WoWDev Wiki: https://wowdev.wiki/
