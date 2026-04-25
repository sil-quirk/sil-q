# Create a distribution archive from an OGG-only release build folder
# Supports standard and portable release layouts

param(
    [string]$ReleaseFolder = "sil-more-release",
    [string]$Version = "",
    [string]$ArchiveName = "",
    [switch]$Portable
)

Add-Type -AssemblyName System.IO.Compression.FileSystem
$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }

function Get-FullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $scriptRoot $PathValue))
}

function Test-PathInsideDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CandidatePath,
        [Parameter(Mandatory = $true)]
        [string]$DirectoryPath
    )

    $candidate = [System.IO.Path]::GetFullPath($CandidatePath).TrimEnd('\')
    $directory = [System.IO.Path]::GetFullPath($DirectoryPath).TrimEnd('\')
    $directoryPrefix = $directory + [System.IO.Path]::DirectorySeparatorChar

    return $candidate.Equals($directory, [System.StringComparison]::OrdinalIgnoreCase) -or
        $candidate.StartsWith($directoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Read-Host "Enter release version"
}

$Version = $Version.Trim()
if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "ERROR: Release version is required." -ForegroundColor Red
    exit 1
}

if ($Version.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0) {
    Write-Host "ERROR: Release version contains characters that are invalid in file names: $Version" -ForegroundColor Red
    exit 1
}

$releaseFolderPath = Get-FullPath $ReleaseFolder
$releaseFolderName = Split-Path -Leaf $releaseFolderPath
if (-not $Portable -and $releaseFolderName -match 'portable') {
    $Portable = $true
}

$defaultArchiveBaseName = "sil-more-$Version"
if ($Portable) {
    $defaultArchiveBaseName += "-portable"
}

# Auto-generate archive name from version if not provided
if (-not $ArchiveName) {
    $ArchiveName = "$defaultArchiveBaseName.zip"
}

$archivePath = Get-FullPath $ArchiveName

if (-not [System.IO.Path]::GetExtension($archivePath)) {
    $archivePath += ".zip"
} elseif ([System.IO.Path]::GetExtension($archivePath) -ne ".zip") {
    Write-Host "ERROR: Archive name must use the .zip extension: $ArchiveName" -ForegroundColor Red
    exit 1
}

$archiveRootName = [System.IO.Path]::GetFileNameWithoutExtension($archivePath)
if ([string]::IsNullOrWhiteSpace($archiveRootName)) {
    Write-Host "ERROR: Could not determine archive folder name from: $ArchiveName" -ForegroundColor Red
    exit 1
}

$stagingParentPath = Join-Path ([System.IO.Path]::GetTempPath()) ("sil-more-archive-" + [System.Guid]::NewGuid().ToString("N"))
$stagingReleasePath = Join-Path $stagingParentPath $archiveRootName

Write-Host "Creating distribution archive..." -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

# Verify release folder exists
if (-not (Test-Path -LiteralPath $releaseFolderPath -PathType Container)) {
    Write-Host "ERROR: Release folder not found: $ReleaseFolder" -ForegroundColor Red
    exit 1
}

if (Test-PathInsideDirectory -CandidatePath $archivePath -DirectoryPath $releaseFolderPath) {
    Write-Host "ERROR: Archive path must be outside the release folder: $ArchiveName" -ForegroundColor Red
    exit 1
}

$archiveDirectory = Split-Path -Parent $archivePath
if ($archiveDirectory -and -not (Test-Path -LiteralPath $archiveDirectory)) {
    New-Item -ItemType Directory -Path $archiveDirectory | Out-Null
}

if ($Portable) {
    $archiveMode = "PORTABLE"
} else {
    $archiveMode = "STANDARD"
}

Write-Host "Source folder: $releaseFolderPath" -ForegroundColor Yellow
Write-Host "Mode: $archiveMode" -ForegroundColor Yellow
Write-Host "Version: $Version" -ForegroundColor Yellow
Write-Host "Archive folder: $archiveRootName" -ForegroundColor Yellow
Write-Host "Archive name: $archivePath" -ForegroundColor Yellow

# Enforce the OGG-only packaging rule before archiving.
$wavFiles = @(Get-ChildItem -LiteralPath $releaseFolderPath -Recurse -File -Filter "*.wav" -ErrorAction SilentlyContinue)
if ($wavFiles.Count -gt 0) {
    Write-Host "ERROR: Release folder contains .wav files; expected .ogg-only audio assets." -ForegroundColor Red
    $wavFiles | ForEach-Object { Write-Host "  - $($_.FullName)" }
    exit 1
}

# Remove existing archive if it exists
if (Test-Path -LiteralPath $archivePath) {
    Write-Host "Removing existing archive..." -ForegroundColor Yellow
    Remove-Item -Force $archivePath
}

Write-Host ""
Write-Host "Preparing staged release folder..." -ForegroundColor Yellow

try {
    New-Item -ItemType Directory -Path $stagingReleasePath -Force | Out-Null
    Get-ChildItem -LiteralPath $releaseFolderPath -Force | Copy-Item -Destination $stagingReleasePath -Recurse -Force

    Write-Host "Compressing..." -ForegroundColor Yellow

    # Compress-Archive is unreliable on this tree and can mis-handle nested files;
    # ZipFile.CreateFromDirectory produces the expected release zip consistently.
    # We stage the release under the archive base name so the zip contains a
    # top-level folder like sil-more-0.9.1/ regardless of the source folder name.
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $stagingReleasePath,
        $archivePath,
        [System.IO.Compression.CompressionLevel]::Optimal,
        $true
    )

    $archiveSizeBytes = (Get-Item -LiteralPath $archivePath).Length
    $folderSizeBytes = (Get-ChildItem -LiteralPath $releaseFolderPath -Recurse -File | Measure-Object -Property Length -Sum).Sum
    if (-not $folderSizeBytes) {
        $folderSizeBytes = 0
    }

    $archiveSize = $archiveSizeBytes / 1MB
    $folderSize = $folderSizeBytes / 1MB
    if ($folderSizeBytes -gt 0) {
        $compression = (1 - ($archiveSizeBytes / $folderSizeBytes)) * 100
    } else {
        $compression = 0
    }
    
    Write-Host ""
    Write-Host "======================================" -ForegroundColor Green
    Write-Host "Archive created successfully!" -ForegroundColor Green
    Write-Host "======================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Archive: $archivePath" -ForegroundColor Cyan
    Write-Host "Original size: $([math]::Round($folderSize, 2)) MB"
    Write-Host "Archive size: $([math]::Round($archiveSize, 2)) MB"
    Write-Host "Compression: $([math]::Round($compression, 1))%"
    Write-Host ""
    Write-Host "Ready for distribution!" -ForegroundColor Green
    
} catch {
    Write-Host "ERROR: Failed to create archive" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
} finally {
    if (Test-Path -LiteralPath $stagingParentPath) {
        Remove-Item -LiteralPath $stagingParentPath -Recurse -Force -ErrorAction SilentlyContinue
    }
}
