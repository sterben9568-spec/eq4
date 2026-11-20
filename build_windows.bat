@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo ==========================================
echo   RBT EQ - Windows x64 VST3 build
echo ==========================================

where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake 3.22+ not found. Install CMake and enable PATH integration.
  pause
  exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
  echo NOTE: MSVC compiler is not in PATH.
  echo Trying Visual Studio 2022 generator. Install Visual Studio 2022
  echo with the "Desktop development with C++" workload if this fails.
)

cmake -S . -B build-win64 -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail

cmake --build build-win64 --config Release --target RBT_EQ_VST3
if errorlevel 1 goto :fail

echo.
echo BUILD SUCCESSFUL.
echo VST3 bundle:
echo   %CD%\build-win64\RBT_EQ_artefacts\Release\VST3\RBT EQ.vst3
if exist "%CD%\build-win64\RBT_EQ_artefacts\Release\VST3\RBT EQ.vst3" (
  echo.
  echo You can copy the .vst3 bundle to:
  echo   C:\Program Files\Common Files\VST3\
  echo Then restart Studio One and run a plug-in scan.
) else (
  echo The build finished, but the expected bundle path was not found.
)
pause
exit /b 0

:fail
echo.
echo BUILD FAILED. Read the first CMake/MSVC error above.
pause
exit /b 1
