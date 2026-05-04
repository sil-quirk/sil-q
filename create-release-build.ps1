# Create clean standard and portable release build folders matching sil-more_beta 0.9 structure
# Based on analysis of the existing clean build

param(
    [string]$OutputFolder = "sil-more-release",
    [string]$Version = "0.9.1",
    [switch]$IncludeCoverArt = $false
)

Write-Host "Creating clean release build: $OutputFolder" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
if ($IncludeCoverArt) {
    Write-Host "Mode: FULL (with CoverArt)" -ForegroundColor Green
} else {
    Write-Host "Mode: SLIM (without CoverArt)" -ForegroundColor Cyan
}
Write-Host "Portable companion release will also be created." -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

function Remove-WavFilesRecursive {
    param(
        [string]$RootPath
    )

    if (-not (Test-Path $RootPath)) {
        return 0
    }

    $wavFiles = @(Get-ChildItem -Path $RootPath -Recurse -File -Filter "*.wav" -ErrorAction SilentlyContinue)
    foreach ($wav in $wavFiles) {
        Remove-Item -Force $wav.FullName
    }

    return $wavFiles.Count
}

$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }

function Resolve-ScriptRelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $scriptRoot $PathValue))
}

$portableOutputFolder = if ($OutputFolder -match '(?i)-portable$') {
    $OutputFolder
} elseif ($OutputFolder -match '(?i)-release$') {
    $OutputFolder -replace '(?i)-release$', '-portable-release'
} else {
    "$OutputFolder-portable"
}

$buildScriptPath = Resolve-ScriptRelativePath "build-cmake.bat"
$portableScriptPath = Resolve-ScriptRelativePath "create-portable-release-build.ps1"
$deploymentFolderPath = Resolve-ScriptRelativePath "sil-more-windows-sdl3"
$deploymentFolder = Split-Path -Leaf $deploymentFolderPath
$deploymentExePath = Join-Path $deploymentFolderPath "sil-more.exe"
$outputFolderPath = Resolve-ScriptRelativePath $OutputFolder
$portableOutputFolderPath = Resolve-ScriptRelativePath $portableOutputFolder
$libSourceRoot = Resolve-ScriptRelativePath "lib"
$coverArtPath = Resolve-ScriptRelativePath "CoverArt"
$legacyCoverArtPath = Resolve-ScriptRelativePath "sil-more_beta 0.9/CoverArt"
$archiveScriptPath = Resolve-ScriptRelativePath "create-distribution-archive.ps1"
$releaseApkPath = Resolve-ScriptRelativePath "android/app/build/outputs/apk/release/app-release.apk"

# Define game data folders to copy (content only) - edit, pref, xtra, docs
$libFoldersToCopy = @('edit', 'pref', 'xtra', 'docs')

# Folders to create but leave empty (for runtime use, no content copied)
$emptyLibFolders = @('data', 'apex', 'save', 'user')

# Sub-folders to create within apex (for runtime data)
$apexSubfolders = @('metaruns')

# DLLs required for the Windows SDL runtime
$requiredDlls = @(
    'SDL3.dll',
    'SDL3_ttf.dll',
    'SDL3_image.dll',
    'SDL3_mixer.dll',
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

Write-Host ""
Write-Host "Checking for standard Windows deployment..." -ForegroundColor Yellow
if (-not (Test-Path $deploymentExePath)) {
    Write-Host "Standard deployment not found. Building now..." -ForegroundColor Cyan
    & $buildScriptPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
}

if (-not (Test-Path $deploymentExePath)) {
    Write-Host "  [ERROR] Standard deployment executable not found: $deploymentExePath" -ForegroundColor Red
    exit 1
}

Write-Host "  [OK] Standard deployment found"

# Create release folder
if (Test-Path $outputFolderPath) {
    Write-Host "Removing existing $outputFolderPath..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $outputFolderPath
}

New-Item -ItemType Directory -Path $outputFolderPath | Out-Null
Write-Host "Created release folder: $outputFolderPath" -ForegroundColor Green

# Copy executable
Write-Host ""
Write-Host "Copying executable..." -ForegroundColor Yellow
if (Test-Path $deploymentExePath) {
    Copy-Item $deploymentExePath $outputFolderPath -Force
    Write-Host "  [OK] sil-more.exe"
} else {
    Write-Host "  [ERROR] sil-more.exe not found in $deploymentFolder/" -ForegroundColor Red
    exit 1
}

# Copy DLLs from deployment folder
Write-Host ""
Write-Host "Copying SDL3 runtime DLLs..." -ForegroundColor Yellow
$copiedDlls = 0
$missingDlls = @()

foreach ($dll in $requiredDlls) {
    $srcPath = Join-Path $deploymentFolderPath $dll
    if (Test-Path $srcPath) {
        Copy-Item $srcPath $outputFolderPath -Force
        $copiedDlls++
    } else {
        # Also check in lib folder as fallback
        $libPath = Join-Path $libSourceRoot $dll
        if (Test-Path $libPath) {
            Copy-Item $libPath $outputFolderPath -Force
            $copiedDlls++
        } else {
            $missingDlls += $dll
        }
    }
}

Write-Host "  [OK] Copied $copiedDlls DLLs"
if ($missingDlls.Count -gt 0) {
    Write-Host "  [ERROR] Missing DLLs:" -ForegroundColor Red
    $missingDlls | ForEach-Object { Write-Host "    - $_" }
    exit 1
}

# Create lib folder structure
Write-Host ""
Write-Host "Creating game data structure..." -ForegroundColor Yellow
$releaseLibPath = Join-Path $outputFolderPath "lib"
if (-not (Test-Path $releaseLibPath)) {
    New-Item -ItemType Directory -Path $releaseLibPath | Out-Null
}

# Copy game data folders (with special handling for certain folders)
$foldersCopied = 0
$musicFilesCopied = 0
foreach ($folder in $libFoldersToCopy) {
    $srcFolder = Join-Path $libSourceRoot $folder
    $dstFolder = Join-Path $releaseLibPath $folder
    
    if (Test-Path $srcFolder) {
        if ($folder -eq "edit") {
            # Special: Copy edit but exclude misc subfolder
            New-Item -ItemType Directory -Path $dstFolder | Out-Null
            Get-ChildItem $srcFolder -File | Copy-Item -Destination $dstFolder -Force
            $itemCount = (Get-ChildItem $srcFolder -File | Measure-Object).Count
            Write-Host "  [OK] lib/$folder ($itemCount files, misc/ excluded)"
        } elseif ($folder -eq "xtra") {
            # Special: xtra requires careful copying
            Copy-Item -Recurse $srcFolder $dstFolder -Force

            # Public release builds use the documented OFL font set only.
            $inputMonoPath = "$dstFolder/font/InputMono-Bold.ttf"
            if (Test-Path $inputMonoPath) {
                Remove-Item -Force $inputMonoPath
                Write-Host "  [OK] lib/$folder (font: removed InputMono-Bold.ttf)"
            } else {
                Write-Host "  [SKIP] lib/$folder/font/InputMono-Bold.ttf (not found)"
            }

            # Remove 'packs' subfolders in sound (sound packs are not included in the release)
            $soundPath = "$dstFolder/sound"
            $musicPath = "$dstFolder/music"
            if (Test-Path $soundPath) {
                $packs = Get-ChildItem -Path $soundPath -Directory -Recurse -Force | Where-Object { $_.Name -ieq "packs" }
                if ($packs -and $packs.Count -gt 0) {
                    foreach ($p in $packs) {
                        Remove-Item -Recurse -Force $p.FullName
                    }
                    Write-Host "  [OK] lib/$folder (sound: removed 'packs' directories)"
                } else {
                    Write-Host "  [SKIP] lib/$folder/sound/packs (not found)"
                }
            }

            # Keep the release tree OGG-only.
            $wavFilesRemoved = Remove-WavFilesRecursive $dstFolder
            if ($wavFilesRemoved -gt 0) {
                Write-Host "  [OK] lib/$folder (audio: removed $wavFilesRemoved .wav files)"
            } else {
                Write-Host "  [SKIP] lib/$folder (audio: no .wav files found)"
            }
            
            # Remove non-16x16.png files from graf subfolder
            $grafPath = "$dstFolder/graf"
            if (Test-Path $grafPath) {
                Get-ChildItem $grafPath -File | Where-Object { $_.Name -ne "16x16.png" } | Remove-Item -Force
                $grafItems = (Get-ChildItem $grafPath -File | Measure-Object).Count
                Write-Host "  [OK] lib/$folder (graf: $grafItems files, only 16x16.png kept)"
            }

            # Count music files if present in xtra
            if (Test-Path $musicPath) {
                $musicFilesCopied = (Get-ChildItem -Path $musicPath -Recurse -File -Filter "*.ogg" | Measure-Object).Count
                Write-Host "  [OK] lib/$folder (music: $musicFilesCopied .ogg files)"
            } else {
                Write-Host "  [SKIP] lib/$folder/music (not found)"
            }

            $itemCount = (Get-ChildItem -Recurse $dstFolder | Measure-Object).Count
            $foldersCopied++
            continue
        } else {
            # Normal: Copy entire folder
            Copy-Item -Recurse $srcFolder $dstFolder -Force
            $itemCount = (Get-ChildItem -Recurse $srcFolder | Measure-Object).Count
            Write-Host "  [OK] lib/$folder ($itemCount items)"
        }
        $foldersCopied++
    } else {
        Write-Host "  [SKIP] lib/$folder (not found)"
    }
}

# Create empty folders for runtime use (data, apex, save, user)
foreach ($folder in $emptyLibFolders) {
    $emptyPath = Join-Path $releaseLibPath $folder
    if (-not (Test-Path $emptyPath)) {
        New-Item -ItemType Directory -Path $emptyPath | Out-Null
        Write-Host "  [CREATED] lib/$folder (empty)"
        
        # Create subfolders within apex for metarun data
        if ($folder -eq "apex") {
            foreach ($subfolder in $apexSubfolders) {
                $subPath = "$emptyPath/$subfolder"
                New-Item -ItemType Directory -Path $subPath | Out-Null
            }
        }
    }
}

# Copy CoverArt folder if requested
if ($IncludeCoverArt) {
    Write-Host ""
    Write-Host "Copying CoverArt..." -ForegroundColor Yellow
    $releaseCoverArtPath = Join-Path $outputFolderPath "CoverArt"
    if (Test-Path $coverArtPath) {
        Copy-Item -Recurse $coverArtPath $releaseCoverArtPath -Force
        $artCount = (Get-ChildItem -Recurse $coverArtPath | Measure-Object).Count
        Write-Host "  [OK] CoverArt ($artCount items)"
    } elseif (Test-Path $legacyCoverArtPath) {
        Copy-Item -Recurse $legacyCoverArtPath $releaseCoverArtPath -Force
        $artCount = (Get-ChildItem -Recurse $releaseCoverArtPath | Measure-Object).Count
        Write-Host "  [OK] CoverArt ($artCount items)"
    } else {
        Write-Host "  [SKIP] CoverArt not found"
    }
}

# Create a manifest file
Write-Host ""
Write-Host "Creating release manifest..." -ForegroundColor Yellow

$manifestText = "Sil-More Release Build - Manifest`n"
$manifestText += "Version: $Version`n"
$manifestText += "Created: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n"
$manifestText += "Build: $(Get-Date -Format 'yyyyMMdd_HHmmss')`n"
$manifestText += "`n"
$manifestText += "=== STRUCTURE ===" + "`n"
$manifestText += "sil-more.exe                 - Main game executable`n"
$manifestText += "lib/`n"
$manifestText += "  edit/                      - Game data definitions (20 .txt files, misc/ excluded)`n"
$manifestText += "  pref/                      - Default preferences and keybinds (30 files)`n"
$manifestText += "  data/                      - Empty directory (for future use)`n"
$manifestText += "  xtra/                      - Extended resources (fonts, sound, music, minimal graphics)`n"
$manifestText += "    font/                    - Font files`n"
$manifestText += "    sound/                   - Audio files (.ogg only; sound packs excluded)`n"
$manifestText += "    music/                   - Background music files (.ogg only)`n"
$manifestText += "    graf/                    - Graphics (only 16x16.png)`n"
$manifestText += "  docs/                      - Documentation and manuals (6 files)`n"
$manifestText += "  apex/                      - Runtime data directory (EMPTY - for metarun data)`n"
$manifestText += "  save/                      - Runtime data directory (EMPTY - for player saves)`n"
$manifestText += "  user/                      - Runtime data directory (EMPTY - for user data)`n"
$manifestText += "*.dll                        - SDL3 runtime libraries ($($requiredDlls.Count) DLLs)`n"
if ($IncludeCoverArt) {
    $manifestText += "CoverArt/                    - Game cover art and promotional images`n"
}
$manifestText += "`n"
$manifestText += "=== TO RUN ===" + "`n"
$manifestText += "1. Extract this folder to any location`n"
$manifestText += "2. Run: sil-more.exe`n"
$manifestText += "`n"
$manifestText += "=== REQUIREMENTS ===" + "`n"
$manifestText += "- Windows (64-bit)`n"
$manifestText += "- All $($requiredDlls.Count) DLL files must remain in the game folder`n"
$manifestText += "- No external dependencies beyond what's included`n"
$manifestText += "`n"
$manifestText += "=== CONFIGURATION ===" + "`n"
$manifestText += "Edit lib/pref/pref.prf to customize game settings`n"
$manifestText += "`n"

$manifestText | Set-Content (Join-Path $outputFolderPath "MANIFEST.txt")
Write-Host "  [OK] MANIFEST.txt"

# Create the portable companion release build.
Write-Host ""
Write-Host "Creating portable release build..." -ForegroundColor Yellow
& $portableScriptPath -OutputFolder $portableOutputFolder -Version $Version -IncludeCoverArt:$IncludeCoverArt
if (-not $?) {
    Write-Host "Portable release build failed!" -ForegroundColor Red
    exit 1
}

# Calculate and display final size
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Release builds complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Standard location: $outputFolderPath" -ForegroundColor Cyan
Write-Host "Portable location: $portableOutputFolderPath" -ForegroundColor Cyan

$folderSize = (Get-ChildItem -Recurse $outputFolderPath | Measure-Object -Property Length -Sum).Sum / 1MB
$folderSizeGB = $folderSize / 1024
if ($folderSize -gt 1024) {
    Write-Host "Size: $([math]::Round($folderSizeGB, 2)) GB"
} else {
    Write-Host "Size: $([math]::Round($folderSize, 2)) MB"
}

Write-Host ""
Write-Host "Contents:" -ForegroundColor Yellow
Write-Host "  - Executable: sil-more.exe"
Write-Host "  - SDL3 Runtime: $copiedDlls DLLs"
Write-Host "  - Game Data: $foldersCopied folders + empty directories"
Write-Host "  - Music files: $musicFilesCopied .ogg files"
if ($IncludeCoverArt) {
    Write-Host "  - CoverArt: Included"
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Test: $(Join-Path $outputFolderPath 'sil-more.exe')"
Write-Host "  2. Test portable: $(Join-Path $portableOutputFolderPath 'sil-more.exe')"
Write-Host "  3. Package distributions: $archiveScriptPath -ReleaseFolder $outputFolderPath -PortableReleaseFolder $portableOutputFolderPath -Version $Version -ApkPath $releaseApkPath"
Write-Host ""
