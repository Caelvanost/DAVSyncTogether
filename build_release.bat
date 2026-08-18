@echo off
setlocal EnableExtensions EnableDelayedExpansion

for /f "tokens=3" %%V in ('findstr /R /C:"VERSION [0-9][0-9.]*" CMakeLists.txt') do set VERSION=%%V
if not defined VERSION set VERSION=0.1.0

set BUILD_DIR=build\release
set STAGE_DIR=build\package
set DIST_DIR=dist
set ZIP_NAME=DAVSyncTogether-%VERSION%.zip

if not defined VCPKG_ROOT (
    echo [ERROR] VCPKG_ROOT is not defined.
    exit /b 1
)

if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
if exist "%STAGE_DIR%" rmdir /S /Q "%STAGE_DIR%"
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

cmake -S . -B "%BUILD_DIR%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

mkdir "%STAGE_DIR%\SKSE\Plugins"

set DLL_PATH=
for /R "%BUILD_DIR%" %%F in (DAVSyncTogether.dll) do set DLL_PATH=%%F
if not defined DLL_PATH (
    echo [ERROR] DAVSyncTogether.dll was not found after build.
    exit /b 1
)

copy /Y "%DLL_PATH%" "%STAGE_DIR%\SKSE\Plugins\DAVSyncTogether.dll" >nul

if exist "%DIST_DIR%\%ZIP_NAME%" del /Q "%DIST_DIR%\%ZIP_NAME%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%STAGE_DIR%\*' -DestinationPath '%DIST_DIR%\%ZIP_NAME%' -Force"
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo  DAVSync Together v%VERSION% - Release package ready
echo  %DIST_DIR%\%ZIP_NAME%
echo ============================================================

endlocal
