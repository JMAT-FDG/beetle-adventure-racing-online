@echo off
setlocal
cd /d "%~dp0"
for %%I in ("%~dp0.") do set "BAR_PROJECT=%%~sI"
if not exist "source\RecompiledFuncs\funcs.h" (
  echo Le code C du jeu doit d'abord etre genere depuis decomp/build/recomp.elf.
  exit /b 1
)
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=%~dp0tools\build-python\cmake\data\bin;%~dp0tools\build-python\bin;%~dp0tools\llvm18\bin;%PATH%"
set "BAR_LLVM_RC=%BAR_PROJECT%/tools/llvm18/bin/llvm-rc.exe"
set "BAR_LLVM_RC=%BAR_LLVM_RC:\=/%"
set "BAR_NINJA=%BAR_PROJECT%/tools/build-python/bin/ninja.exe"
set "BAR_NINJA=%BAR_NINJA:\=/%"
cmake -S "%BAR_PROJECT%/source" -B "%BAR_PROJECT%/tools/host-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON "-DCMAKE_MAKE_PROGRAM=%BAR_NINJA%" "-DCMAKE_RC_COMPILER=%BAR_LLVM_RC%" "-DBEETLE_RC_COMPILER=%BAR_LLVM_RC%" "-DBEETLE_DIRECTX_HEADERS=%BAR_PROJECT%/build-support/directx-headers"
if errorlevel 1 exit /b 1
cmake --build "%BAR_PROJECT%/tools/host-build" --parallel 4
exit /b %errorlevel%
