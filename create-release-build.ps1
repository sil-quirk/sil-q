# Create a clean release build folder matching sil-more_beta 0.9 structure
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
Write-Host "========================================" -ForegroundColor Cyan

# Define game data folders to copy (content only) - edit, pref, xtra, docs
$libFoldersToCopy = @('edit', 'pref', 'xtra', 'docs')

# Folders to create but leave empty (for runtime use, no content copied)
$emptyLibFolders = @('data', 'apex', 'save', 'user')

# Sub-folders to create within apex (for runtime data)
$apexSubfolders = @('metaruns')

# DLLs required for SDL3 runtime - EXACT list from sil-more_beta 0.9
$requiredDlls = @(
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

# Create release folder
if (Test-Path $OutputFolder) {
    Write-Host "Removing existing $OutputFolder..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $OutputFolder
}

New-Item -ItemType Directory -Path $OutputFolder | Out-Null
Write-Host "Created release folder: $OutputFolder" -ForegroundColor Green

# Copy executable
Write-Host ""
Write-Host "Copying executable..." -ForegroundColor Yellow
if (Test-Path "build-standard/sil-more.exe") {
    Copy-Item "build-standard/sil-more.exe" "$OutputFolder/" -Force
    Write-Host "  [OK] sil-more.exe"
} else {
    Write-Host "  [ERROR] sil-more.exe not found in build-standard/" -ForegroundColor Red
    exit 1
}

# Copy DLLs from deployment folder
Write-Host ""
Write-Host "Copying SDL3 runtime DLLs..." -ForegroundColor Yellow
$copiedDlls = 0
$missingDlls = @()

foreach ($dll in $requiredDlls) {
    $srcPath = "sil-more-windows-sdl3/$dll"
    if (Test-Path $srcPath) {
        Copy-Item $srcPath "$OutputFolder/" -Force
        $copiedDlls++
    } else {
        # Also check in lib folder as fallback
        $libPath = "lib/$dll"
        if (Test-Path $libPath) {
            Copy-Item $libPath "$OutputFolder/" -Force
            $copiedDlls++
        } else {
            $missingDlls += $dll
        }
    }
}

Write-Host "  [OK] Copied $copiedDlls DLLs"
if ($missingDlls.Count -gt 0) {
    Write-Host "  [WARN] Missing DLLs:" -ForegroundColor Yellow
    $missingDlls | ForEach-Object { Write-Host "    - $_" }
}

# Create lib folder structure
Write-Host ""
Write-Host "Creating game data structure..." -ForegroundColor Yellow
$libPath = "$OutputFolder/lib"
if (-not (Test-Path $libPath)) {
    New-Item -ItemType Directory -Path $libPath | Out-Null
}

# Copy game data folders (with special handling for certain folders)
$foldersCopied = 0
$musicFilesCopied = 0
foreach ($folder in $libFoldersToCopy) {
    $srcFolder = "lib/$folder"
    $dstFolder = "$libPath/$folder"
    
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

            # Remove .wav files from the *root* of the sound folder (keep sounds in subfolders)
            $rootSoundPath = "$dstFolder/sound"
            if (Test-Path $rootSoundPath) {
                $wavFiles = Get-ChildItem -Path $rootSoundPath -File -Filter "*.wav" -ErrorAction SilentlyContinue
                if ($wavFiles -and $wavFiles.Count -gt 0) {
                    foreach ($wf in $wavFiles) {
                        Remove-Item -Force $wf.FullName
                    }
                    Write-Host "  [OK] lib/$folder (sound: removed $($wavFiles.Count) .wav files from sound root)"
                } else {
                    Write-Host "  [SKIP] lib/$folder (sound: no .wav files at root)"
                }
            }
            
            # Remove non-16x16.png files from graf subfolder
            $grafPath = "$dstFolder/graf"
            if (Test-Path $grafPath) {
                Get-ChildItem $grafPath -File | Where-Object { $_.Name -ne "16x16.png" } | Remove-Item -Force
                $grafItems = (Get-ChildItem $grafPath -File | Measure-Object).Count
                Write-Host "  [OK] lib/$folder (graf: $grafItems files, only 16x16.png kept)"
            }

            # Count music files if present in xtra
            $musicPath = "$dstFolder/music"
            if (Test-Path $musicPath) {
                $musicFilesCopied = (Get-ChildItem -Recurse $musicPath -File | Measure-Object).Count
                Write-Host "  [OK] lib/$folder (music: $musicFilesCopied files)"
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
    $emptyPath = "$libPath/$folder"
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
    if (Test-Path "CoverArt") {
        Copy-Item -Recurse "CoverArt" "$OutputFolder/CoverArt" -Force
        $artCount = (Get-ChildItem -Recurse "CoverArt" | Measure-Object).Count
        Write-Host "  [OK] CoverArt ($artCount items)"
    } elseif (Test-Path "sil-more_beta 0.9/CoverArt") {
        Copy-Item -Recurse "sil-more_beta 0.9/CoverArt" "$OutputFolder/CoverArt" -Force
        $artCount = (Get-ChildItem -Recurse "$OutputFolder/CoverArt" | Measure-Object).Count
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
$manifestText += "    sound/                   - Audio files (sound packs excluded; root .wav files excluded)`n"
$manifestText += "    music/                   - Background music files`n"
$manifestText += "    graf/                    - Graphics (only 16x16.png)`n"
$manifestText += "  docs/                      - Documentation and manuals (6 files)`n"
$manifestText += "  apex/                      - Runtime data directory (EMPTY - for metarun data)`n"
$manifestText += "  save/                      - Runtime data directory (EMPTY - for player saves)`n"
$manifestText += "  user/                      - Runtime data directory (EMPTY - for user data)`n"
$manifestText += "*.dll                        - SDL3 runtime libraries (18 DLLs)`n"
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
$manifestText += "- All 18 DLL files must remain in the game folder`n"
$manifestText += "- No external dependencies beyond what's included`n"
$manifestText += "`n"
$manifestText += "=== CONFIGURATION ===" + "`n"
$manifestText += "Edit lib/pref/pref.prf to customize game settings`n"
$manifestText += "`n"

$manifestText | Set-Content "$OutputFolder/MANIFEST.txt"
Write-Host "  [OK] MANIFEST.txt"

# Calculate and display final size
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Release build complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Location: ./$OutputFolder" -ForegroundColor Cyan

$folderSize = (Get-ChildItem -Recurse $OutputFolder | Measure-Object -Property Length -Sum).Sum / 1MB
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
Write-Host "  - Music files: $musicFilesCopied files"
if ($IncludeCoverArt) {
    Write-Host "  - CoverArt: Included"
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Test: .\$OutputFolder\sil-more.exe"
Write-Host "  2. Archive: .\create-distribution-archive.ps1 -ReleaseFolder $OutputFolder"
Write-Host ""
