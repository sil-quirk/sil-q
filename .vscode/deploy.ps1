# Deploy Sil-More to sil-more-windows-sdl3 folder
# Matches the deployment logic from build-cmake.bat

Write-Host "Deploying Sil-More to sil-more-windows-sdl3..." -ForegroundColor Cyan

# Create deployment directory if it doesn't exist
if (-not (Test-Path sil-more-windows-sdl3)) {
    New-Item -ItemType Directory -Path sil-more-windows-sdl3 | Out-Null
    Write-Host "Created deployment directory" -ForegroundColor Green
}

# Copy executable
Write-Host "Copying executable..." -ForegroundColor Yellow
Copy-Item build/sil-more.exe sil-more-windows-sdl3/ -Force

# Copy game data (lib folder) only if it doesn't exist
if (-not (Test-Path sil-more-windows-sdl3/lib)) {
    Write-Host "Copying game data (lib folder)..." -ForegroundColor Yellow
    Copy-Item -Recurse lib sil-more-windows-sdl3/lib -Force
}

# Copy JSON config file if it doesn't exist
if (-not (Test-Path sil-more-windows-sdl3/sil_sdl.json)) {
    Write-Host "Copying SDL configuration file..." -ForegroundColor Yellow
    Copy-Item sil_sdl.json sil-more-windows-sdl3/ -Force
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
        Copy-Item $src sil-more-windows-sdl3/ -Force
        $copiedCount++
    }
}

Write-Host ""
Write-Host "========================================"
Write-Host "Deployment complete!" -ForegroundColor Green
Write-Host "========================================"
Write-Host "Executable: sil-more-windows-sdl3\sil-more.exe"
Write-Host "DLLs copied: $copiedCount"
Write-Host ""
