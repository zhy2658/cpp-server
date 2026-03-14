@echo off
cmake --build build --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
bin\server.exe