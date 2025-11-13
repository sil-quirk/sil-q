# Deploy Sil-More (Local Data version) to sil-more-windows-sdl3-portable folder
# Matches the deployment logic from build-cmake.bat for local builds

Write-Host "Deploying Sil-More (Local Data) to sil-more-windows-sdl3-portable..." -ForegroundColor Cyan

# Create deployment directory if it doesn't exist
if (-not (Test-Path sil-more-windows-sdl3-portable)) {
    New-Item -ItemType Directory -Path sil-more-windows-sdl3-portable | Out-Null
    Write-Host "Created deployment directory" -ForegroundColor Green
}

# Copy executable
Write-Host "Copying executable..." -ForegroundColor Yellow
Copy-Item build/sil-more.exe sil-more-windows-sdl3-portable/ -Force

# Copy game data (lib folder) only if it doesn't exist
if (-not (Test-Path sil-more-windows-sdl3-portable/lib)) {
    Write-Host "Copying game data (lib folder)..." -ForegroundColor Yellow
    Copy-Item -Recurse lib sil-more-windows-sdl3-portable/lib -Force
} else {
    # Always update lib/edit folder to ensure latest data files
    Write-Host "Updating lib/edit folder..." -ForegroundColor Yellow
    if (Test-Path sil-more-windows-sdl3-portable/lib/edit) {
        Remove-Item -Recurse -Force sil-more-windows-sdl3-portable/lib/edit
    }
    Copy-Item -Recurse lib/edit sil-more-windows-sdl3-portable/lib/edit -Force
    
    # Always update lib/pref folder to ensure latest preference files
    Write-Host "Updating lib/pref folder..." -ForegroundColor Yellow
    if (Test-Path sil-more-windows-sdl3-portable/lib/pref) {
        Remove-Item -Recurse -Force sil-more-windows-sdl3-portable/lib/pref
    }
    Copy-Item -Recurse lib/pref sil-more-windows-sdl3-portable/lib/pref -Force
}

# Copy JSON config file if it doesn't exist
if (-not (Test-Path sil-more-windows-sdl3-portable/sil_sdl.json)) {
    Write-Host "Copying SDL configuration file..." -ForegroundColor Yellow
    Copy-Item sil_sdl.json sil-more-windows-sdl3-portable/ -Force
}

# Copy DLLs
Write-Host "Copying DLLs..." -ForegroundColor Yellow
$dlls = @(
    'SDL3.dll',
    'SDL3_ttf.dll',
    'SDL3_image.dll',
    'libgcc_s_seh-1.dll',
    'libstdc++-6.dll',
    'libwinpthread-1.dll',
    'libfreetype-6.dll',
    'libharfbuzz-0.dll',
    'libgraphite2.dll',
    'libglib-2.0-0.dll',
    'libbrotlidec.dll',
    'libbrotlicommon.dll',
    'libbz2-1.dll',
    'libpng16-16.dll',
    'zlib1.dll',
    'libintl-8.dll',
    'libpcre2-8-0.dll',
    'libiconv-2.dll'
)

$copiedCount = 0
foreach ($dll in $dlls) {
    $src = "C:/msys64/mingw64/bin/$dll"
    if (Test-Path $src) {
        Copy-Item $src sil-more-windows-sdl3-portable/ -Force
        $copiedCount++
    }
}

Write-Host ""
Write-Host "========================================"
Write-Host "Deployment complete! (Local Data Mode)" -ForegroundColor Green
Write-Host "========================================"
Write-Host "Executable: sil-more-windows-sdl3-portable\sil-more.exe"
Write-Host "Data folder: sil-more-windows-sdl3-portable\lib (Local Mode)"
Write-Host "DLLs copied: $copiedCount"
Write-Host ""
