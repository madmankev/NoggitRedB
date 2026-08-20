#!/bin/bash
set -e

echo "==========================================" 
echo "NoggitRedB Build Script"
echo "==========================================" 

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected Linux"
    echo "Installing dependencies..."
    sudo apt-get update -y
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        freeglut3-dev \
        libgl1-mesa-dev \
        libboost-all-dev

    # Qt 5: 'qt5-default' was removed from Debian 11 / Ubuntu 20.04 onwards,
    # install the dev metapackages directly instead.
    if ! sudo apt-get install -y qt5-default 2>/dev/null; then
        sudo apt-get install -y \
            qtbase5-dev \
            qtbase5-dev-tools \
            qttools5-dev
    fi

    # libstorm-dev is not packaged on all distributions; CMake vendors its
    # own fallback, so a missing system package must not abort the script.
    sudo apt-get install -y libstorm-dev || \
        echo "warning: libstorm-dev unavailable, using the vendored StormLib"

elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS"
    echo "Installing dependencies via Homebrew..."
    brew install cmake qt@5 boost
    # Qt 5 is keg-only on Homebrew:
    export CMAKE_PREFIX_PATH="$(brew --prefix qt@5):${CMAKE_PREFIX_PATH}"
fi

echo ""
echo "==========================================" 
echo "Creating build directory..."
echo "==========================================" 
mkdir -p build
cd build

echo ""
echo "==========================================" 
echo "Running CMake configuration..."
echo "==========================================" 
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DNOGGIT_BUILD_NODE_DATAMODELS=ON \
    -DNOGGIT_ENABLE_TRACY_PROFILER=OFF \
    ..

echo ""
echo "==========================================" 
echo "Building NoggitRedB..."
echo "==========================================" 

if [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$OSTYPE" == "darwin"* ]]; then
    make -j $(nproc)
else
    cmake --build . --config Release -j
fi

echo ""
echo "==========================================" 
echo "Build Complete!"
echo "==========================================" 
echo "Executable location: ./build/bin/noggit"
echo "Run with: ./build/bin/noggit"
