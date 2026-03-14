@echo off
setlocal

echo [INFO] ==========================================
echo [INFO] Starting one-click build process...
echo [INFO] ==========================================

:: Set build type to Release by default
set BUILD_TYPE=Release
set BUILD_DIR=build
set BIN_DIR=bin

:: Create build directory if it doesn't exist
if not exist %BUILD_DIR% (
    echo [INFO] Creating build directory...
    mkdir %BUILD_DIR%
)

:: Configure CMake (MinGW Makefiles, Release mode)
echo [INFO] Configuring CMake...
cmake -S . -B %BUILD_DIR% -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %errorlevel%
)

:: Build the project
echo [INFO] Building project...
cmake --build %BUILD_DIR% --config %BUILD_TYPE%
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b %errorlevel%
)

:: Create bin directory
if not exist %BIN_DIR% (
    echo [INFO] Creating bin directory...
    mkdir %BIN_DIR%
)

:: Copy executable and config
echo [INFO] Copying artifacts to %BIN_DIR%...
if exist %BUILD_DIR%\server.exe (
    copy /Y %BUILD_DIR%\server.exe %BIN_DIR%\
) else (
    echo [WARNING] server.exe not found in %BUILD_DIR%. Check build output.
)

if exist config.yaml copy /Y config.yaml %BIN_DIR%\

echo [SUCCESS] ==========================================
echo [SUCCESS] Build complete!
echo [SUCCESS] Artifacts are located in the '%BIN_DIR%' directory.
echo [SUCCESS] ==========================================

pause
