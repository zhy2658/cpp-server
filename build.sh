#!/bin/bash
set -e

echo "[INFO] =========================================="
echo "[INFO] Starting one-click build process..."
echo "[INFO] =========================================="

# Set build type to Release by default
BUILD_TYPE=Release
BUILD_DIR=build
BIN_DIR=bin

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "[INFO] Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

# Configure CMake (Unix Makefiles, Release mode)
echo "[INFO] Configuring CMake..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# Build the project
echo "[INFO] Building project..."
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j$(nproc)

# Create bin directory
if [ ! -d "$BIN_DIR" ]; then
    echo "[INFO] Creating bin directory..."
    mkdir -p "$BIN_DIR"
fi

# Copy executable and config
echo "[INFO] Copying artifacts to $BIN_DIR..."
if [ -f "$BUILD_DIR/server" ]; then
    cp "$BUILD_DIR/server" "$BIN_DIR/"
else
    echo "[WARNING] server executable not found in $BUILD_DIR. Check build output."
fi

if [ -f "config.yaml" ]; then
    cp "config.yaml" "$BIN_DIR/"
fi

echo "[SUCCESS] =========================================="
echo "[SUCCESS] Build complete!"
echo "[SUCCESS] Artifacts are located in the '$BIN_DIR' directory."
echo "[SUCCESS] =========================================="
