# Create distribution archives for standard and portable release folders
# and copy the release APK alongside them.

param(
    [string]$ReleaseFolder = "sil-more-release",
    [string]$PortableReleaseFolder = "",
    [string]$Version = "",
    [string]$ArchiveName = "",
    [string]$PortableArchiveName = "",
    [string]$ApkPath = "",
    [switch]$Portable
)

Add-Type -AssemblyName System.IO.Compression
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

function Get-PortableSiblingPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    $parent = Split-Path -Parent $PathValue
    $leaf = Split-Path -Leaf $PathValue

    if ($leaf -match '(?i)-release$') {
        $leaf = $leaf -replace '(?i)-release$', '-portable-release'
    } elseif ($leaf -match '(?i)-portable$') {
        return [System.IO.Path]::GetFullPath($PathValue)
    } else {
        $leaf = "$leaf-portable"
    }

    return [System.IO.Path]::GetFullPath((Join-Path $parent $leaf))
}

function Get-StandardSiblingPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    $parent = Split-Path -Parent $PathValue
    $leaf = Split-Path -Leaf $PathValue

    if ($leaf -match '(?i)-portable-release$') {
        $leaf = $leaf -replace '(?i)-portable-release$', '-release'
    } elseif ($leaf -match '(?i)-portable$') {
        $leaf = $leaf -replace '(?i)-portable$', ''
    } else {
        $leaf = "$leaf-release"
    }

    return [System.IO.Path]::GetFullPath((Join-Path $parent $leaf))
}

function Resolve-ZipPath {
    param(
        [AllowEmptyString()]
        [string]$PathValue,
        [Parameter(Mandatory = $true)]
        [string]$DefaultBaseName
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        $PathValue = "$DefaultBaseName.zip"
    }

    $resolvedPath = Get-FullPath $PathValue
    $extension = [System.IO.Path]::GetExtension($resolvedPath)

    if ([string]::IsNullOrWhiteSpace($extension)) {
        $resolvedPath += ".zip"
    } elseif ($extension -ne ".zip") {
        throw "Archive name must use the .zip extension: $PathValue"
    }

    return $resolvedPath
}

function Resolve-ReleasePair {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PrimaryReleaseFolderPath,
        [string]$PortableReleaseFolderPath = ""
    )

    $primary = [System.IO.Path]::GetFullPath($PrimaryReleaseFolderPath)

    if (-not [string]::IsNullOrWhiteSpace($PortableReleaseFolderPath)) {
        return [pscustomobject]@{
            Standard = $primary
            Portable = [System.IO.Path]::GetFullPath($PortableReleaseFolderPath)
        }
    }

    $primaryLeaf = Split-Path -Leaf $primary
    if ($primaryLeaf -match '(?i)portable') {
        return [pscustomobject]@{
            Standard = Get-StandardSiblingPath $primary
            Portable = $primary
        }
    }

    return [pscustomobject]@{
        Standard = $primary
        Portable = Get-PortableSiblingPath $primary
    }
}

function Assert-NoWavFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FolderPath,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $wavFiles = @(Get-ChildItem -LiteralPath $FolderPath -Recurse -File -Filter "*.wav" -ErrorAction SilentlyContinue)
    if ($wavFiles.Count -gt 0) {
        Write-Host "ERROR: $Label release folder contains .wav files; expected .ogg-only audio assets." -ForegroundColor Red
        $wavFiles | ForEach-Object { Write-Host "  - $($_.FullName)" }
        exit 1
    }
}

function ConvertTo-ZipEntryName {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    return $PathValue.Replace([System.IO.Path]::DirectorySeparatorChar, '/').Replace([System.IO.Path]::AltDirectorySeparatorChar, '/')
}

function Get-RelativeZipPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RootPath,
        [Parameter(Mandatory = $true)]
        [string]$ItemPath
    )

    $trimChars = [char[]]@([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $rootFullPath = [System.IO.Path]::GetFullPath($RootPath).TrimEnd($trimChars)
    $itemFullPath = [System.IO.Path]::GetFullPath($ItemPath)

    if ($itemFullPath.Length -le $rootFullPath.Length) {
        return ""
    }

    $relativePath = $itemFullPath.Substring($rootFullPath.Length).TrimStart($trimChars)
    return ConvertTo-ZipEntryName $relativePath
}

function New-ForwardSlashZipArchive {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceFolderPath,
        [Parameter(Mandatory = $true)]
        [string]$ArchivePath
    )

    $archiveRootName = ConvertTo-ZipEntryName ([System.IO.Path]::GetFileNameWithoutExtension($ArchivePath))
    $archiveRootName = $archiveRootName.Trim('/')
    if ([string]::IsNullOrWhiteSpace($archiveRootName)) {
        throw "Archive file name must not be empty: $ArchivePath"
    }

    $archiveStream = [System.IO.File]::Open($ArchivePath, [System.IO.FileMode]::CreateNew)
    $zipArchive = New-Object System.IO.Compression.ZipArchive($archiveStream, [System.IO.Compression.ZipArchiveMode]::Create, $false)

    try {
        $null = $zipArchive.CreateEntry("$archiveRootName/")

        $directories = @(Get-ChildItem -LiteralPath $SourceFolderPath -Recurse -Directory -Force)
        foreach ($directory in $directories) {
            $relativePath = Get-RelativeZipPath -RootPath $SourceFolderPath -ItemPath $directory.FullName
            if ([string]::IsNullOrWhiteSpace($relativePath)) {
                continue
            }

            $entryName = "$archiveRootName/$relativePath/"
            $null = $zipArchive.CreateEntry($entryName)
        }

        $files = @(Get-ChildItem -LiteralPath $SourceFolderPath -Recurse -File -Force)
        foreach ($file in $files) {
            $relativePath = Get-RelativeZipPath -RootPath $SourceFolderPath -ItemPath $file.FullName
            if ([string]::IsNullOrWhiteSpace($relativePath)) {
                continue
            }

            $entryName = "$archiveRootName/$relativePath"
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $zipArchive,
                $file.FullName,
                $entryName,
                [System.IO.Compression.CompressionLevel]::Optimal
            ) | Out-Null
        }
    } finally {
        if ($zipArchive) {
            $zipArchive.Dispose()
        }
        if ($archiveStream) {
            $archiveStream.Dispose()
        }
    }
}

function Assert-ZipEntryNamesArePortable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ArchivePath,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $zipArchive = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $backslashEntries = @($zipArchive.Entries | Where-Object { $_.FullName.Contains('\') } | Select-Object -First 5 -ExpandProperty FullName)
        if ($backslashEntries.Count -gt 0) {
            throw "$Label archive contains Windows-style backslash paths: $($backslashEntries -join ', ')"
        }
    } finally {
        if ($zipArchive) {
            $zipArchive.Dispose()
        }
    }
}

function New-DistributionArchive {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [string]$SourceFolderPath,
        [Parameter(Mandatory = $true)]
        [string]$ArchivePath
    )

    if (Test-Path -LiteralPath $ArchivePath) {
        Write-Host "Removing existing $Label archive..." -ForegroundColor Yellow
        Remove-Item -LiteralPath $ArchivePath -Force
    }

    Write-Host "Compressing $Label archive..." -ForegroundColor Yellow
    New-ForwardSlashZipArchive -SourceFolderPath $SourceFolderPath -ArchivePath $ArchivePath
    Assert-ZipEntryNamesArePortable -ArchivePath $ArchivePath -Label $Label

    $archiveSizeBytes = (Get-Item -LiteralPath $ArchivePath).Length
    $folderSizeBytes = (Get-ChildItem -LiteralPath $SourceFolderPath -Recurse -File | Measure-Object -Property Length -Sum).Sum
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

    Write-Host "  [OK] $Label archive created" -ForegroundColor Green
    Write-Host "    Archive: $ArchivePath" -ForegroundColor Cyan
    Write-Host "    Original size: $([math]::Round($folderSize, 2)) MB"
    Write-Host "    Archive size: $([math]::Round($archiveSize, 2)) MB"
    Write-Host "    Compression: $([math]::Round($compression, 1))%"
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
$releasePair = Resolve-ReleasePair -PrimaryReleaseFolderPath $releaseFolderPath -PortableReleaseFolderPath $PortableReleaseFolder
$standardReleaseFolderPath = $releasePair.Standard
$portableReleaseFolderPath = $releasePair.Portable

if ($standardReleaseFolderPath -ieq $portableReleaseFolderPath) {
    Write-Host "ERROR: Standard and portable release folders must be different." -ForegroundColor Red
    exit 1
}

$defaultArchiveBaseName = "sil-more-$Version"
$standardArchivePath = Resolve-ZipPath -PathValue $ArchiveName -DefaultBaseName $defaultArchiveBaseName
if ([string]::IsNullOrWhiteSpace($PortableArchiveName)) {
    $standardArchiveBaseName = [System.IO.Path]::GetFileNameWithoutExtension($standardArchivePath)
    $portableArchivePath = Join-Path (Split-Path -Parent $standardArchivePath) "$standardArchiveBaseName-portable.zip"
} else {
    $portableArchivePath = Resolve-ZipPath -PathValue $PortableArchiveName -DefaultBaseName "$defaultArchiveBaseName-portable"
}
$releaseApkFileName = "$defaultArchiveBaseName.apk"

$releaseApkSourcePath = if ([string]::IsNullOrWhiteSpace($ApkPath)) {
    Get-FullPath "android/app/build/outputs/apk/sideload/release/app-sideload-release.apk"
} else {
    Get-FullPath $ApkPath
}

if ([string]::IsNullOrWhiteSpace((Split-Path -Leaf $standardArchivePath))) {
    Write-Host "ERROR: Could not determine archive file name from: $standardArchivePath" -ForegroundColor Red
    exit 1
}

if ($standardArchivePath -ieq $portableArchivePath) {
    Write-Host "ERROR: Standard and portable archive paths must be different." -ForegroundColor Red
    exit 1
}

Write-Host "Creating distribution archives..." -ForegroundColor Cyan
Write-Host "===================================" -ForegroundColor Cyan

if (-not (Test-Path -LiteralPath $standardReleaseFolderPath -PathType Container)) {
    Write-Host "ERROR: Standard release folder not found: $standardReleaseFolderPath" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path -LiteralPath $portableReleaseFolderPath -PathType Container)) {
    Write-Host "ERROR: Portable release folder not found: $portableReleaseFolderPath" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path -LiteralPath $releaseApkSourcePath -PathType Leaf)) {
    Write-Host "ERROR: Release APK not found: $releaseApkSourcePath" -ForegroundColor Red
    exit 1
}

foreach ($candidatePath in @($standardArchivePath, $portableArchivePath)) {
    if (Test-PathInsideDirectory -CandidatePath $candidatePath -DirectoryPath $standardReleaseFolderPath) {
        Write-Host "ERROR: Archive path must be outside the standard release folder: $candidatePath" -ForegroundColor Red
        exit 1
    }

    if (Test-PathInsideDirectory -CandidatePath $candidatePath -DirectoryPath $portableReleaseFolderPath) {
        Write-Host "ERROR: Archive path must be outside the portable release folder: $candidatePath" -ForegroundColor Red
        exit 1
    }
}

$standardArchiveDirectory = Split-Path -Parent $standardArchivePath
if ($standardArchiveDirectory -and -not (Test-Path -LiteralPath $standardArchiveDirectory)) {
    New-Item -ItemType Directory -Path $standardArchiveDirectory | Out-Null
}

$portableArchiveDirectory = Split-Path -Parent $portableArchivePath
if ($portableArchiveDirectory -and -not (Test-Path -LiteralPath $portableArchiveDirectory)) {
    New-Item -ItemType Directory -Path $portableArchiveDirectory | Out-Null
}

$releaseApkDestinationPath = Join-Path $standardArchiveDirectory $releaseApkFileName
if (-not (Test-PathInsideDirectory -CandidatePath $releaseApkDestinationPath -DirectoryPath $standardReleaseFolderPath) -and
    -not (Test-PathInsideDirectory -CandidatePath $releaseApkDestinationPath -DirectoryPath $portableReleaseFolderPath)) {
    # OK. The APK will be copied beside the archives.
} else {
    Write-Host "ERROR: APK destination must be outside the release folders: $releaseApkDestinationPath" -ForegroundColor Red
    exit 1
}

if ([System.IO.Path]::GetFullPath($releaseApkDestinationPath).TrimEnd('\') -eq [System.IO.Path]::GetFullPath($releaseApkSourcePath).TrimEnd('\')) {
    Write-Host "ERROR: APK destination resolves to the source file itself: $releaseApkDestinationPath" -ForegroundColor Red
    exit 1
}

Write-Host "Standard release folder: $standardReleaseFolderPath" -ForegroundColor Yellow
Write-Host "Portable release folder: $portableReleaseFolderPath" -ForegroundColor Yellow
Write-Host "Version: $Version" -ForegroundColor Yellow
Write-Host "Standard archive: $standardArchivePath" -ForegroundColor Yellow
Write-Host "Portable archive: $portableArchivePath" -ForegroundColor Yellow
Write-Host "Release APK: $releaseApkSourcePath" -ForegroundColor Yellow
Write-Host "APK copy target: $releaseApkDestinationPath" -ForegroundColor Yellow

Assert-NoWavFiles -FolderPath $standardReleaseFolderPath -Label "standard"
Assert-NoWavFiles -FolderPath $portableReleaseFolderPath -Label "portable"

Write-Host ""
Write-Host "Preparing archives..." -ForegroundColor Yellow

try {
    New-DistributionArchive -Label "standard" -SourceFolderPath $standardReleaseFolderPath -ArchivePath $standardArchivePath
    New-DistributionArchive -Label "portable" -SourceFolderPath $portableReleaseFolderPath -ArchivePath $portableArchivePath

    Write-Host ""
    Write-Host "Copying release APK..." -ForegroundColor Yellow
    Copy-Item -LiteralPath $releaseApkSourcePath -Destination $releaseApkDestinationPath -Force
    Write-Host "  [OK] APK copied to $releaseApkDestinationPath" -ForegroundColor Green

    Write-Host ""
    Write-Host "===================================" -ForegroundColor Green
    Write-Host "Distribution bundle created successfully!" -ForegroundColor Green
    Write-Host "===================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Standard archive: $standardArchivePath" -ForegroundColor Cyan
    Write-Host "Portable archive: $portableArchivePath" -ForegroundColor Cyan
    Write-Host "Release APK: $releaseApkDestinationPath" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Ready for distribution!" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Failed to create distribution bundle" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
