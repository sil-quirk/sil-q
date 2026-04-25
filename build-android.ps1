param(
    [ValidateSet('arm64-v8a','armeabi-v7a','x86_64')]
    [string]$Abi = 'arm64-v8a',

    [ValidateSet('Debug','Release')]
    [string]$Config = 'Release',

    # Set ANDROID_NDK_HOME or pass -NdkPath explicitly
    [string]$NdkPath = $env:ANDROID_NDK_HOME,

    # android-24 is a safe default for modern handhelds
    [string]$Platform = 'android-24'
)

$ErrorActionPreference = 'Stop'

function Get-LatestAndroidSdkPackagePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackageName,

        [Parameter(Mandatory = $true)]
        [string]$RelativeExecutablePath
    )

    $roots = @()

    if ($env:LOCALAPPDATA) {
        $roots += (Join-Path $env:LOCALAPPDATA "Android\Sdk\$PackageName")
    }

    if ($env:ANDROID_HOME) {
        $roots += (Join-Path $env:ANDROID_HOME $PackageName)
    }

    if ($env:ANDROID_SDK_ROOT) {
        $roots += (Join-Path $env:ANDROID_SDK_ROOT $PackageName)
    }

    foreach ($root in ($roots | Select-Object -Unique)) {
        if (-not (Test-Path $root)) { continue }

        $latest = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            Select-Object -First 1

        if (-not $latest) { continue }

        $candidate = Join-Path $latest.FullName $RelativeExecutablePath
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

function Get-LatestNdkPath {
    $roots = @()
    if ($env:ANDROID_NDK_HOME) {
        $roots += $env:ANDROID_NDK_HOME
    }

    if ($env:LOCALAPPDATA) {
        $roots += (Join-Path $env:LOCALAPPDATA 'Android\Sdk\ndk')
    }

    if ($env:ANDROID_HOME) {
        $roots += (Join-Path $env:ANDROID_HOME 'ndk')
    }

    if ($env:ANDROID_SDK_ROOT) {
        $roots += (Join-Path $env:ANDROID_SDK_ROOT 'ndk')
    }

    foreach ($root in ($roots | Select-Object -Unique)) {
        if (-not (Test-Path $root)) { continue }

        $latest = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            Select-Object -First 1

        if ($latest) {
            return $latest.FullName
        }
    }

    return $null
}

function Resolve-CMakePath {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $msys2CMake = 'C:\msys64\mingw64\bin\cmake.exe'
    if (Test-Path $msys2CMake) {
        return $msys2CMake
    }

    $sdkCMake = Get-LatestAndroidSdkPackagePath -PackageName 'cmake' -RelativeExecutablePath 'bin\cmake.exe'
    if ($sdkCMake) {
        return $sdkCMake
    }

    return $null
}

$cmakePath = Resolve-CMakePath
if (-not $cmakePath) {
    throw 'CMake not found. Install CMake or the Android SDK CMake package, or add cmake.exe to PATH.'
}

$cmakeBinDir = Split-Path $cmakePath -Parent
if ($cmakePath -like 'C:\msys64\mingw64\bin\*') {
    $msysPaths = @('C:\msys64\mingw64\bin', 'C:\msys64\usr\bin')
    foreach ($pathEntry in $msysPaths) {
        if ($env:Path -notlike "*$pathEntry*") {
            $env:Path = "$pathEntry;$env:Path"
        }
    }
} elseif ($env:Path -notlike "*$cmakeBinDir*") {
    $env:Path = "$cmakeBinDir;$env:Path"
}

if (-not $NdkPath) {
    $NdkPath = Get-LatestNdkPath
}

if (-not $NdkPath) {
    throw 'Android NDK not found. Install Android SDK + NDK (SDK Manager), or pass -NdkPath explicitly.'
}

$toolchain = Join-Path $NdkPath 'build/cmake/android.toolchain.cmake'
if (-not (Test-Path $toolchain)) {
    throw "NDK toolchain not found at: $toolchain"
}

$buildDir = Join-Path $PSScriptRoot "build-android/$Abi"

Write-Host "Using CMake: $cmakePath" -ForegroundColor Cyan
Write-Host "Using NDK: $NdkPath" -ForegroundColor Cyan
Write-Host "Using toolchain: $toolchain" -ForegroundColor Cyan

$configureArgs = @(
    '-S', $PSScriptRoot,
    '-B', $buildDir,
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    "-DANDROID_ABI=$Abi",
    "-DANDROID_PLATFORM=$Platform",
    '-DSIL_BUILD_WITH_SDL_SOURCES=ON'
)

if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $configureArgs += @('-G', 'Ninja')
    Write-Host 'Generator: Ninja' -ForegroundColor Cyan
} elseif (Get-Command mingw32-make -ErrorAction SilentlyContinue) {
    $configureArgs += @('-G', 'MinGW Makefiles')
    Write-Host 'Generator: MinGW Makefiles' -ForegroundColor Cyan
} else {
    Write-Warning 'No explicit generator selected (ninja/mingw32-make not found). CMake default generator will be used.'
}

& $cmakePath @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

& $cmakePath --build $buildDir --parallel
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE"
}

Write-Host "Built native library for $Abi in $buildDir" -ForegroundColor Green
