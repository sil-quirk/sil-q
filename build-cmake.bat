@echo off
REM Build Sil-More for native Windows with SDL3 using CMake and MinGW-w64
REM This script uses MSYS2's MinGW64 environment

echo Building Sil-More for Windows with SDL3 using CMake...
echo.

REM Set up MinGW64 environment
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

REM Run CMake configuration and build
cmake -G "MinGW Makefiles" -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DUSE_SDL=ON -DUSE_GCU=OFF
if errorlevel 1 (
    echo CMake configuration failed!
REM    pause
    exit /b 1
)

cmake --build build --parallel
if errorlevel 1 (
    echo CMake build failed!
REM    pause
    exit /b 1
)

echo.
echo Build successful!
echo.
echo Copying files to deployment folder...

REM Create deployment directory
if not exist sil-more-windows-sdl3 mkdir sil-more-windows-sdl3

REM Copy executable
copy /Y build\sil-more.exe sil-more-windows-sdl3\

REM Copy SDL DLLs
copy /Y C:\msys64\mingw64\bin\SDL3.dll sil-more-windows-sdl3\
copy /Y C:\msys64\mingw64\bin\SDL3_ttf.dll sil-more-windows-sdl3\
copy /Y C:\msys64\mingw64\bin\SDL3_image.dll sil-more-windows-sdl3\

REM Copy MinGW runtime DLLs
for %%f in (
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
    libfreetype-6.dll
    libharfbuzz-0.dll
    libgraphite2.dll
    libglib-2.0-0.dll
    libbrotlidec.dll
    libbrotlicommon.dll
    libbz2-1.dll
    libpng16-16.dll
    zlib1.dll
    libintl-8.dll
    libpcre2-8-0.dll
    libiconv-2.dll
) do (
    if exist "C:\msys64\mingw64\bin\%%f" copy /Y "C:\msys64\mingw64\bin\%%f" sil-more-windows-sdl3\ >nul 2>&1
)

REM Copy game data
if not exist sil-more-windows-sdl3\lib xcopy /E /I /Y lib sil-more-windows-sdl3\lib

REM Always update lib/edit folder to ensure latest data files
if exist sil-more-windows-sdl3\lib\edit rmdir /S /Q sil-more-windows-sdl3\lib\edit
xcopy /E /I /Y lib\edit sil-more-windows-sdl3\lib\edit

echo.
echo ========================================
echo Build complete!
echo ========================================
echo.
echo Executable: sil-more-windows-sdl3\sil-more.exe
echo.

