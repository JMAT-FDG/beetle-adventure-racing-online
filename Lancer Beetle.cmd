@echo off
cd /d "%~dp0tools\host-build"
if not exist "beetle-adventure-racing-recomp.exe" exit /b 1
if exist "%~dp0roms\beetle-usa.z64" (
    start "" "beetle-adventure-racing-recomp.exe" "%~dp0roms\beetle-usa.z64"
) else (
    start "" "beetle-adventure-racing-recomp.exe"
)
