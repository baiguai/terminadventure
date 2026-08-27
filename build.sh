#!/bin/bash

# Build Script

set -e  # Exit on any error
source ./config.sh

# Determine build type
BUILD_TYPE="Debug"
if [ "$1" == "r" ]; then
    BUILD_TYPE="Release"
    echo "Performing RELEASE build."
else
    echo "Performing DEBUG build (default)."
fi

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

echo "Building Windows EXE..."
./build-windows.sh || echo "Warning: Windows build failed, continuing with Linux build..."

# Navigate to build directory
cd build

# Generate CMakeLists.txt from template using config.sh values
echo "Generating CMakeLists.txt..."
cp ../CMakeLists.txt.in ../CMakeLists.txt

# Inject app name
sed -i "s/<<TARGET_NAME>>/$APP_NAME/g" ../CMakeLists.txt

# Inject source files
SOURCES_TMP=$(mktemp)
for s in "${SOURCES[@]}"; do
    echo "    $s" >> "$SOURCES_TMP"
done
sed -i "/^<<SOURCES>>$/{
    r $SOURCES_TMP
    d
}" ../CMakeLists.txt
rm -f "$SOURCES_TMP"

# Inject library files
for lib in "${LIBS[@]}"; do
    if [[ "$lib" == *::* ]]; then
        echo "target_link_libraries($APP_NAME PRIVATE $lib)" >> ../CMakeLists.txt
    else
        echo "target_link_libraries($APP_NAME PRIVATE \${CMAKE_SOURCE_DIR}/$lib)" >> ../CMakeLists.txt
    fi
done

# Configure with CMake
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Build the project
echo "Compiling..."
make

# Check if build was successful
if [ -f "bin/$APP_NAME" ]; then

    # Copy the config file alongside the app (the app looks for it next to the executable)
    if [ -f "../config/commands.conf" ]; then
        cp "../config/commands.conf" "bin/"
        echo "Config copied to: $(pwd)/bin/commands.conf"
    else
        echo "Warning: config/commands.conf not found - config not copied"
    fi
    if [ -f "../config/scribboleth.html" ]; then
        cp "../config/scribboleth.html" "bin/"
        echo "Template .html copied to: $(pwd)/bin/scribboleth.html"
    else
        echo "Warning: config/scribboleth.html not found - template not copied"
    fi
    if [ -f "../config/init.conf" ]; then
        cp "../config/init.conf" "bin/"
        echo "Init config copied to: $(pwd)/bin/init.conf"
    else
        echo "Warning: config/init.conf not found - init config not copied"
    fi
    if [ -d "../build-windows/bin" ]; then
        cp "../config/commands.conf" "../build-windows/bin/" 2>/dev/null || \
            echo "Warning: could not copy config to build-windows/bin/"
        cp "../config/scribboleth.html" "../build-windows/bin/" 2>/dev/null || \
            echo "Warning: could not copy template to build-windows/bin/"
        cp "../config/init.conf" "../build-windows/bin/" 2>/dev/null || \
            echo "Warning: could not copy init config to build-windows/bin/"
    fi

    echo "-- Build successful --"
    echo "Executable: $(pwd)/bin/$APP_NAME"
    echo ""
    echo "To run $APP_NAME:"
    echo "  ./bin/$APP_NAME"
    echo ""
    echo "Or from the parent directory:"
    echo "  ./build/bin/$APP_NAME"
else
    echo "! failed !"
    exit 1
fi
