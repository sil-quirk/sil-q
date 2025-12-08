@echo off
REM Build Sil-More for native Windows with SDL3 using CMake and MinGW-w64
REM This script uses MSYS2's MinGW64 environment
REM Builds TWO versions: standard (user folder) and local build (SIL_USE_LOCAL_DATA)

echo Building Sil-More for Windows with SDL3 using CMake...
echo.

REM Set up MinGW64 environment
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

REM ========================================
REM BUILD 1: Standard build (user folder mode)
REM ========================================
echo [1/2] Building standard version (user folder mode)...
echo.

REM Run CMake configuration and build
cmake -G "MinGW Makefiles" -B build-standard -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DSIL_USE_LOCAL_DATA=OFF
if errorlevel 1 (
    echo CMake configuration failed for standard build!
    exit /b 1
)

cmake --build build-standard --parallel
if errorlevel 1 (
    echo CMake build failed for standard build!
    exit /b 1
)

echo.
echo Standard build successful!
echo.
echo Copying files to deployment folder (standard)...

REM Create deployment directory
if not exist sil-more-windows-sdl3 mkdir sil-more-windows-sdl3

REM Copy executable
copy /Y build-standard\sil-more.exe sil-more-windows-sdl3\

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
if not exist sil-more-windows-sdl3\lib xcopy /E /I /Y /K lib sil-more-windows-sdl3\lib

REM Always update lib/edit folder to ensure latest data files
if exist sil-more-windows-sdl3\lib\edit rmdir /S /Q sil-more-windows-sdl3\lib\edit
xcopy /E /I /Y /K lib\edit sil-more-windows-sdl3\lib\edit

REM Always update lib/pref folder to ensure latest preference files
if exist sil-more-windows-sdl3\lib\pref rmdir /S /Q sil-more-windows-sdl3\lib\pref
xcopy /E /I /Y /K lib\pref sil-more-windows-sdl3\lib\pref

REM Always update lib/xtra/sound folder to ensure latest sound configuration
if exist sil-more-windows-sdl3\lib\xtra\sound rmdir /S /Q sil-more-windows-sdl3\lib\xtra\sound
xcopy /E /I /Y /K lib\xtra\sound sil-more-windows-sdl3\lib\xtra\sound

REM Copy tileset graphic
if not exist sil-more-windows-sdl3\lib\xtra\graf mkdir sil-more-windows-sdl3\lib\xtra\graf
copy /Y lib\xtra\graf\16x16.png sil-more-windows-sdl3\lib\xtra\graf\

echo.
echo Standard version complete: sil-more-windows-sdl3\sil-more.exe
echo.

REM ========================================
REM BUILD 2: Local build (SIL_USE_LOCAL_DATA mode)
REM ========================================
echo [2/2] Building local version (SIL_USE_LOCAL_DATA mode)...
echo.

REM Run CMake configuration and build with local data flag
cmake -G "MinGW Makefiles" -B build-portable -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DSIL_USE_LOCAL_DATA=ON
if errorlevel 1 (
    echo CMake configuration failed for local build!
    exit /b 1
)

cmake --build build-portable --parallel
if errorlevel 1 (
    echo CMake build failed for local build!
    exit /b 1
)

echo.
echo Local build successful!
echo.
echo Copying files to deployment folder (local)...

REM Create local deployment directory
if not exist sil-more-windows-sdl3-portable mkdir sil-more-windows-sdl3-portable

REM Copy executable
copy /Y build-portable\sil-more.exe sil-more-windows-sdl3-portable\

REM Copy SDL DLLs
copy /Y C:\msys64\mingw64\bin\SDL3.dll sil-more-windows-sdl3-portable\
copy /Y C:\msys64\mingw64\bin\SDL3_ttf.dll sil-more-windows-sdl3-portable\
copy /Y C:\msys64\mingw64\bin\SDL3_image.dll sil-more-windows-sdl3-portable\

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
    if exist "C:\msys64\mingw64\bin\%%f" copy /Y "C:\msys64\mingw64\bin\%%f" sil-more-windows-sdl3-portable\ >nul 2>&1
)

REM Copy game data
if not exist sil-more-windows-sdl3-portable\lib xcopy /E /I /Y /K lib sil-more-windows-sdl3-portable\lib

REM Always update lib/edit folder to ensure latest data files
if exist sil-more-windows-sdl3-portable\lib\edit rmdir /S /Q sil-more-windows-sdl3-portable\lib\edit
xcopy /E /I /Y /K lib\edit sil-more-windows-sdl3-portable\lib\edit

REM Always update lib/pref folder to ensure latest preference files
if exist sil-more-windows-sdl3-portable\lib\pref rmdir /S /Q sil-more-windows-sdl3-portable\lib\pref
xcopy /E /I /Y /K lib\pref sil-more-windows-sdl3-portable\lib\pref
REM Always update lib/xtra/sound folder to ensure latest sound configuration
if exist sil-more-windows-sdl3-portable\lib\xtra\sound rmdir /S /Q sil-more-windows-sdl3-portable\lib\xtra\sound
xcopy /E /I /Y /K lib\xtra\sound sil-more-windows-sdl3-portable\lib\xtra\sound

REM Copy tileset graphic
if not exist sil-more-windows-sdl3-portable\lib\xtra\graf mkdir sil-more-windows-sdl3-portable\lib\xtra\graf
copy /Y lib\xtra\graf\16x16.png sil-more-windows-sdl3-portable\lib\xtra\graf\

echo.
echo Local version complete: sil-more-windows-sdl3-portable\sil-more.exe
echo.
echo ========================================
echo Both builds complete!
echo ========================================
echo.
echo Standard (user folder): sil-more-windows-sdl3\sil-more.exe
echo Local (lib folder):     sil-more-windows-sdl3-portable\sil-more.exe
echo.

