@echo off
setlocal
REM Build Sil-More for native Windows with pinned SDL source submodules.
REM This script uses MSYS2's MinGW64 environment for the toolchain.
REM Builds TWO versions: standard (user folder) and local build (SIL_USE_LOCAL_DATA)

echo Building Sil-More for Windows with pinned SDL sources using CMake...
echo.

REM Set up MinGW64 environment
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
set "SIL_SDL_SOURCE_ARG=-DSIL_BUILD_WITH_SDL_SOURCES=ON"

if not exist external\SDL\CMakeLists.txt goto :missing_sdl_sources
if not exist external\SDL_image\CMakeLists.txt goto :missing_sdl_sources
if not exist external\SDL_ttf\CMakeLists.txt goto :missing_sdl_sources
if not exist external\SDL_mixer\CMakeLists.txt goto :missing_sdl_sources

echo Using repo-pinned SDL source submodules from external\.
echo.

REM ========================================
REM BUILD 1: Standard build (user folder mode)
REM ========================================
echo [1/2] Building standard version (user folder mode)...
echo.

REM Run CMake configuration and build
cmake -G "MinGW Makefiles" -B build-standard -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DSIL_USE_LOCAL_DATA=OFF %SIL_SDL_SOURCE_ARG%
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
call :copy_sdl_dlls build-standard sil-more-windows-sdl3
if errorlevel 1 exit /b 1

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
    libogg-0.dll
    libvorbis-0.dll
    libvorbisfile-3.dll
    libopus-0.dll
) do (
    if exist "C:\msys64\mingw64\bin\%%f" copy /Y "C:\msys64\mingw64\bin\%%f" sil-more-windows-sdl3\ >nul 2>&1
)

REM Copy game data
if not exist sil-more-windows-sdl3\lib xcopy /E /I /Y /K lib sil-more-windows-sdl3\lib

REM Always update lib/xtra/font folder to ensure latest fonts are deployed
REM (e.g. EBGaramond-Regular.ttf and any newly added typefaces)
if exist sil-more-windows-sdl3\lib\xtra\font rmdir /S /Q sil-more-windows-sdl3\lib\xtra\font
xcopy /E /I /Y /K lib\xtra\font sil-more-windows-sdl3\lib\xtra\font

REM Exclude non-OFL fallback font from deployment; public releases use the
REM documented redistributable font set.
if exist sil-more-windows-sdl3\lib\xtra\font\InputMono-Bold.ttf del /Q sil-more-windows-sdl3\lib\xtra\font\InputMono-Bold.ttf

REM Always update lib/edit folder to ensure latest data files
if exist sil-more-windows-sdl3\lib\edit rmdir /S /Q sil-more-windows-sdl3\lib\edit
xcopy /E /I /Y /K lib\edit sil-more-windows-sdl3\lib\edit

REM Always update lib/pref folder to ensure latest default JSON configs
if exist sil-more-windows-sdl3\lib\pref rmdir /S /Q sil-more-windows-sdl3\lib\pref
xcopy /E /I /Y /K lib\pref sil-more-windows-sdl3\lib\pref

REM Always update lib/xtra/sound folder to ensure latest sound configuration
if exist sil-more-windows-sdl3\lib\xtra\sound rmdir /S /Q sil-more-windows-sdl3\lib\xtra\sound
xcopy /E /I /Y /K lib\xtra\sound sil-more-windows-sdl3\lib\xtra\sound

REM Always update lib/xtra/music folder to ensure latest music tracks
if exist sil-more-windows-sdl3\lib\xtra\music rmdir /S /Q sil-more-windows-sdl3\lib\xtra\music
xcopy /E /I /Y /K lib\xtra\music sil-more-windows-sdl3\lib\xtra\music

REM Keep the deployed xtra audio tree OGG-only.
call :StripWavFiles sil-more-windows-sdl3\lib\xtra

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
cmake -G "MinGW Makefiles" -B build-portable -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DSIL_USE_LOCAL_DATA=ON %SIL_SDL_SOURCE_ARG%
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
call :copy_sdl_dlls build-portable sil-more-windows-sdl3-portable
if errorlevel 1 exit /b 1

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
    libogg-0.dll
    libvorbis-0.dll
    libvorbisfile-3.dll
    libopus-0.dll
) do (
    if exist "C:\msys64\mingw64\bin\%%f" copy /Y "C:\msys64\mingw64\bin\%%f" sil-more-windows-sdl3-portable\ >nul 2>&1
)

REM Copy game data
if not exist sil-more-windows-sdl3-portable\lib xcopy /E /I /Y /K lib sil-more-windows-sdl3-portable\lib

REM Always update lib/xtra/font folder to ensure latest fonts are deployed
REM (e.g. EBGaramond-Regular.ttf and any newly added typefaces)
if exist sil-more-windows-sdl3-portable\lib\xtra\font rmdir /S /Q sil-more-windows-sdl3-portable\lib\xtra\font
xcopy /E /I /Y /K lib\xtra\font sil-more-windows-sdl3-portable\lib\xtra\font

REM Exclude non-OFL fallback font from deployment; public releases use the
REM documented redistributable font set.
if exist sil-more-windows-sdl3-portable\lib\xtra\font\InputMono-Bold.ttf del /Q sil-more-windows-sdl3-portable\lib\xtra\font\InputMono-Bold.ttf

REM Always update lib/edit folder to ensure latest data files
if exist sil-more-windows-sdl3-portable\lib\edit rmdir /S /Q sil-more-windows-sdl3-portable\lib\edit
xcopy /E /I /Y /K lib\edit sil-more-windows-sdl3-portable\lib\edit

REM Always update lib/pref folder to ensure latest default JSON configs
if exist sil-more-windows-sdl3-portable\lib\pref rmdir /S /Q sil-more-windows-sdl3-portable\lib\pref
xcopy /E /I /Y /K lib\pref sil-more-windows-sdl3-portable\lib\pref
REM Always update lib/xtra/sound folder to ensure latest sound configuration
if exist sil-more-windows-sdl3-portable\lib\xtra\sound rmdir /S /Q sil-more-windows-sdl3-portable\lib\xtra\sound
xcopy /E /I /Y /K lib\xtra\sound sil-more-windows-sdl3-portable\lib\xtra\sound

REM Always update lib/xtra/music folder to ensure latest music tracks
if exist sil-more-windows-sdl3-portable\lib\xtra\music rmdir /S /Q sil-more-windows-sdl3-portable\lib\xtra\music
xcopy /E /I /Y /K lib\xtra\music sil-more-windows-sdl3-portable\lib\xtra\music

REM Keep the deployed xtra audio tree OGG-only.
call :StripWavFiles sil-more-windows-sdl3-portable\lib\xtra

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

goto :EOF

:StripWavFiles
if exist "%~1" (
    for /R "%~1" %%f in (*.wav) do del /Q "%%f" >nul 2>&1
)
exit /B 0

:missing_sdl_sources
echo This Windows build expects the repo-pinned SDL source submodules under external\.
echo Run: git submodule update --init --recursive
exit /b 1

:copy_sdl_dlls
set "BUILD_DIR=%~1"
set "DEPLOY_DIR=%~2"

if not exist "%BUILD_DIR%\_deps\SDL\SDL3.dll" (
    echo Missing built SDL3.dll in %BUILD_DIR%\_deps\SDL
    exit /b 1
)
if not exist "%BUILD_DIR%\_deps\SDL_ttf\SDL3_ttf.dll" (
    echo Missing built SDL3_ttf.dll in %BUILD_DIR%\_deps\SDL_ttf
    exit /b 1
)
if not exist "%BUILD_DIR%\_deps\SDL_image\SDL3_image.dll" (
    echo Missing built SDL3_image.dll in %BUILD_DIR%\_deps\SDL_image
    exit /b 1
)
if not exist "%BUILD_DIR%\_deps\SDL_mixer\SDL3_mixer.dll" (
    echo Missing built SDL3_mixer.dll in %BUILD_DIR%\_deps\SDL_mixer
    exit /b 1
)

copy /Y "%BUILD_DIR%\_deps\SDL\SDL3.dll" "%DEPLOY_DIR%\"
copy /Y "%BUILD_DIR%\_deps\SDL_ttf\SDL3_ttf.dll" "%DEPLOY_DIR%\"
copy /Y "%BUILD_DIR%\_deps\SDL_image\SDL3_image.dll" "%DEPLOY_DIR%\"
copy /Y "%BUILD_DIR%\_deps\SDL_mixer\SDL3_mixer.dll" "%DEPLOY_DIR%\"

exit /b 0

