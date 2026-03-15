@echo off
cd /d "%~dp0"
cmake --build build --config Release
if %errorlevel% neq 0 exit /b %errorlevel%

REM 运行 build 目录下刚编译的 server.exe，工作目录在项目根目录以便加载 config.yaml
cd /d "%~dp0"
build\server.exe
echo.
echo Server process ended (exit code %errorlevel%). Press any key to close...
pause >nul