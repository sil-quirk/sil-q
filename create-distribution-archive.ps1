# Create a distribution archive from an OGG-only release build folder

param(
    [string]$ReleaseFolder = "sil-more-release",
    [string]$ArchiveName = ""
)

# Auto-generate archive name from folder name if not provided
if (-not $ArchiveName) {
    $ArchiveName = "$ReleaseFolder.zip"
}

Write-Host "Creating distribution archive..." -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

# Verify release folder exists
if (-not (Test-Path $ReleaseFolder)) {
    Write-Host "ERROR: Release folder not found: $ReleaseFolder" -ForegroundColor Red
    exit 1
}

Write-Host "Source folder: $ReleaseFolder" -ForegroundColor Yellow
Write-Host "Archive name: $ArchiveName" -ForegroundColor Yellow

# Enforce the OGG-only packaging rule before archiving.
$wavFiles = @(Get-ChildItem -Path $ReleaseFolder -Recurse -File -Filter "*.wav" -ErrorAction SilentlyContinue)
if ($wavFiles.Count -gt 0) {
    Write-Host "ERROR: Release folder contains .wav files; expected .ogg-only audio assets." -ForegroundColor Red
    $wavFiles | ForEach-Object { Write-Host "  - $($_.FullName)" }
    exit 1
}

# Remove existing archive if it exists
if (Test-Path $ArchiveName) {
    Write-Host "Removing existing archive..." -ForegroundColor Yellow
    Remove-Item -Force $ArchiveName
}

Write-Host ""
Write-Host "Compressing..." -ForegroundColor Yellow

try {
    Compress-Archive -Path $ReleaseFolder -DestinationPath $ArchiveName -Force
    
    $archiveSize = (Get-Item $ArchiveName).Length / 1MB
    $folderSize = (Get-ChildItem -Recurse $ReleaseFolder | Measure-Object -Property Length -Sum).Sum / 1MB
    $compression = (1 - $archiveSize / $folderSize) * 100
    
    Write-Host ""
    Write-Host "======================================" -ForegroundColor Green
    Write-Host "Archive created successfully!" -ForegroundColor Green
    Write-Host "======================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Archive: $ArchiveName" -ForegroundColor Cyan
    Write-Host "Original size: $([math]::Round($folderSize, 2)) MB"
    Write-Host "Archive size: $([math]::Round($archiveSize, 2)) MB"
    Write-Host "Compression: $([math]::Round($compression, 1))%"
    Write-Host ""
    Write-Host "Ready for distribution!" -ForegroundColor Green
    
} catch {
    Write-Host "ERROR: Failed to create archive" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
