@echo off
setlocal
cd /d "%~dp0"
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=%~dp0tools\llvm18\bin;%PATH%"
set "BAR_CLIENT_NINJA=%~dp0tools/build-python/bin/ninja.exe"
set "BAR_CLIENT_NINJA=%BAR_CLIENT_NINJA:\=/%"
"%~dp0tools\build-python\cmake\data\bin\cmake.exe" -S "%~dp0client-online/source" -B "%~dp0tools/client-kit-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl "-DCMAKE_MAKE_PROGRAM=%BAR_CLIENT_NINJA%"
if errorlevel 1 exit /b 1
"%~dp0tools\build-python\cmake\data\bin\cmake.exe" --build "%~dp0tools/client-kit-build" --parallel 2
if errorlevel 1 exit /b 1
"%~dp0tools\build-python\cmake\data\bin\ctest.exe" --test-dir "%~dp0tools/client-kit-build" --output-on-failure
exit /b %errorlevel%
