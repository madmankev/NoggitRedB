#!/bin/bash
set -e

echo "=========================================="
echo "NoggitRedB Build Script"
echo "=========================================="

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected Linux"
    echo "Installing dependencies..."
    sudo apt-get update -y
    sudo apt-get install -y \
        build-essential \
        cmake \
        freeglut3-dev \
        libboost-all-dev \
        qt5-default \
        libstorm-dev \
        git

elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS"
    echo "Installing dependencies via Homebrew..."
    brew install cmake qt@5 boost
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
