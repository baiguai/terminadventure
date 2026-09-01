#!/bin/bash

# Requirements:
# sudo apt install -y mingw-w64

source ./config.sh

# Windows build script for your app
# Compiles your code into a Windows executable using MinGW-w64

set -e

echo "Building $APP_NAME for Windows..."

# Determine build type
BUILD_TYPE_FLAGS=""
BUILD_MESSAGE="DEBUG"
if [ "$1" == "r" ]; then
    BUILD_TYPE_FLAGS="-s" # Strip all symbol tables
    BUILD_MESSAGE="RELEASE"
fi
echo "Performing $BUILD_MESSAGE build."


# Check if source files exist - you can add or remove whichever files you like
for f in "${SOURCES[@]}" "${HEADERS[@]}"; do
    if [ ! -f "$f" ]; then
        echo "Error: $f not found"
        exit 1
    fi
done

# Check for MinGW-w64 compiler
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "Error: MinGW-w64 compiler not found"
    echo "Install with: sudo apt install mingw-w64"
    exit 1
fi

# Create build directory
mkdir -p build-windows

# Compile with MinGW-w64 for Windows
echo "Compiling with MinGW-w64..."
x86_64-w64-mingw32-g++ -std=c++20 \
    -O2 \
    -Wall \
    -Wextra \
    -D_WIN32 \
    -static-libgcc \
    -static-libstdc++ \
    -static \
    $BUILD_TYPE_FLAGS \
    -mwindows \
    -o "build-windows/$APP_NAME.exe" \
    "${SOURCES[@]}" \
    "${LIBS[@]}" \
    -luser32 \
    -lgdi32 \
    -lkernel32 \
    -lwinmm




# Add any custom cp's or other actions here
# mkdir -p "./build-windows/data/themes"
# cp -r ./themes/* "./build-windows/data/themes/" 2>/dev/null || true




echo "Build complete: build-windows/$APP_NAME.exe"
echo "Executable size: $(du -h "build-windows/$APP_NAME.exe" | cut -f1)"
