# Create a clean portable release build folder
# Uses SIL_USE_LOCAL_DATA=ON for local data storage

param(
    [string]$OutputFolder = "sil-more-portable-release",
    [string]$Version = "0.9.1",
    [switch]$IncludeCoverArt = $false
)

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
$outputFolderPath = Resolve-ScriptRelativePath $OutputFolder
$standardOutputFolder = if ($OutputFolder -match '(?i)-portable-release$') {
    $OutputFolder -replace '(?i)-portable-release$', '-release'
} elseif ($OutputFolder -match '(?i)-portable$') {
    $OutputFolder -replace '(?i)-portable$', ''
} else {
    "$OutputFolder-standard"
}
$standardOutputFolderPath = Resolve-ScriptRelativePath $standardOutputFolder
$buildScriptPath = Resolve-ScriptRelativePath "build-cmake.bat"
$buildPortableExePath = Resolve-ScriptRelativePath "build-portable/sil-more.exe"
$deploymentFolderPath = Resolve-ScriptRelativePath "sil-more-windows-sdl3-portable"
$libSourceRoot = Resolve-ScriptRelativePath "lib"
$coverArtPath = Resolve-ScriptRelativePath "CoverArt"
$legacyCoverArtPath = Resolve-ScriptRelativePath "sil-more_beta 0.9/CoverArt"
$archiveScriptPath = Resolve-ScriptRelativePath "create-distribution-archive.ps1"
$releaseApkPath = Resolve-ScriptRelativePath "android/app/build/outputs/apk/release/app-release.apk"

Write-Host "Creating portable release build: $outputFolderPath" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Mode: PORTABLE (data stored in lib/ folder)" -ForegroundColor Green
if ($IncludeCoverArt) {
    Write-Host "  With CoverArt" -ForegroundColor Green
}
Write-Host "========================================" -ForegroundColor Cyan

# Define game data folders to copy (content only)
$libFoldersToCopy = @('edit', 'pref', 'xtra', 'docs')

# Folders to create but leave empty (for runtime use)
$emptyLibFolders = @('data', 'apex', 'save', 'user')

# Sub-folders to create within apex
$apexSubfolders = @('metaruns')

# DLLs required for portable Windows SDL runtime
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

# Step 1: Build portable (if not already built)
Write-Host ""
Write-Host "Checking for portable build..." -ForegroundColor Yellow
if (-not (Test-Path $buildPortableExePath)) {
    Write-Host "Portable build not found. Building now..." -ForegroundColor Cyan
    & $buildScriptPath portable
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
}
Write-Host "  [OK] Portable build found"

# Step 2: Create release folder
if (Test-Path $outputFolderPath) {
    Write-Host ""
    Write-Host "Removing existing $outputFolderPath..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $outputFolderPath
}

New-Item -ItemType Directory -Path $outputFolderPath | Out-Null
Write-Host "Created portable release folder: $outputFolderPath" -ForegroundColor Green

# Step 3: Copy executable from portable build
Write-Host ""
Write-Host "Copying portable executable..." -ForegroundColor Yellow
if (Test-Path $buildPortableExePath) {
    Copy-Item $buildPortableExePath $outputFolderPath -Force
    Write-Host "  [OK] sil-more.exe"
} else {
    Write-Host "  [ERROR] sil-more.exe not found: $buildPortableExePath" -ForegroundColor Red
    exit 1
}

# Step 4: Copy DLLs from portable deployment folder
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
    Write-Host "  [WARN] Missing DLLs:" -ForegroundColor Yellow
    $missingDlls | ForEach-Object { Write-Host "    - $_" }
}

# Step 5: Create lib folder structure
Write-Host ""
Write-Host "Creating portable game data structure..." -ForegroundColor Yellow
$libPath = Join-Path $outputFolderPath "lib"
if (-not (Test-Path $libPath)) {
    New-Item -ItemType Directory -Path $libPath | Out-Null
}

# Copy game data folders
$foldersCopied = 0
$musicFilesCopied = 0
foreach ($folder in $libFoldersToCopy) {
    $srcFolder = Join-Path $libSourceRoot $folder
    $dstFolder = Join-Path $libPath $folder
    
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

            # Remove proprietary fonts not in public releases
            $inputMonoPath = "$dstFolder/font/InputMono-Bold.ttf"
            if (Test-Path $inputMonoPath) {
                Remove-Item -Force $inputMonoPath
                Write-Host "  [OK] lib/$folder (font: removed InputMono-Bold.ttf)"
            } else {
                Write-Host "  [SKIP] lib/$folder/font/InputMono-Bold.ttf (not found)"
            }

            # Remove 'packs' subfolders in sound
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

            # Keep OGG-only for release
            $wavFilesRemoved = Remove-WavFilesRecursive $dstFolder
            if ($wavFilesRemoved -gt 0) {
                Write-Host "  [OK] lib/$folder (audio: removed $wavFilesRemoved .wav files)"
            } else {
                Write-Host "  [SKIP] lib/$folder (audio: no .wav files found)"
            }
            
            # Keep only 16x16.png in graf
            $grafPath = "$dstFolder/graf"
            if (Test-Path $grafPath) {
                Get-ChildItem $grafPath -File | Where-Object { $_.Name -ne "16x16.png" } | Remove-Item -Force
                $grafItems = (Get-ChildItem $grafPath -File | Measure-Object).Count
                Write-Host "  [OK] lib/$folder (graf: $grafItems files, only 16x16.png kept)"
            }

            # Count music files
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

# Create empty folders for runtime use
foreach ($folder in $emptyLibFolders) {
    $emptyPath = "$libPath/$folder"
    if (-not (Test-Path $emptyPath)) {
        New-Item -ItemType Directory -Path $emptyPath | Out-Null
        Write-Host "  [CREATED] lib/$folder (empty)"
        
        # Create subfolders within apex
        if ($folder -eq "apex") {
            foreach ($subfolder in $apexSubfolders) {
                $subPath = "$emptyPath/$subfolder"
                New-Item -ItemType Directory -Path $subPath | Out-Null
            }
        }
    }
}

# Copy CoverArt if requested
if ($IncludeCoverArt) {
    Write-Host ""
    Write-Host "Copying CoverArt..." -ForegroundColor Yellow
    if (Test-Path $coverArtPath) {
        Copy-Item -Recurse $coverArtPath (Join-Path $outputFolderPath "CoverArt") -Force
        $artCount = (Get-ChildItem -Recurse $coverArtPath | Measure-Object).Count
        Write-Host "  [OK] CoverArt ($artCount items)"
    } elseif (Test-Path $legacyCoverArtPath) {
        Copy-Item -Recurse $legacyCoverArtPath (Join-Path $outputFolderPath "CoverArt") -Force
        $artCount = (Get-ChildItem -Recurse (Join-Path $outputFolderPath "CoverArt") | Measure-Object).Count
        Write-Host "  [OK] CoverArt ($artCount items)"
    } else {
        Write-Host "  [SKIP] CoverArt not found"
    }
}

# Step 6: Create manifest
Write-Host ""
Write-Host "Creating portable release manifest..." -ForegroundColor Yellow

$manifestText = "Sil-More Portable Release Build - Manifest`n"
$manifestText += "Version: $Version`n"
$manifestText += "Build Mode: Portable (SIL_USE_LOCAL_DATA=ON)`n"
$manifestText += "Created: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n"
$manifestText += "Build: $(Get-Date -Format 'yyyyMMdd_HHmmss')`n"
$manifestText += "`n"
$manifestText += "=== STRUCTURE ===" + "`n"
$manifestText += "sil-more.exe                 - Main game executable (portable mode)`n"
$manifestText += "lib/`n"
$manifestText += "  edit/                      - Game data definitions (20 .txt files, misc/ excluded)`n"
$manifestText += "  pref/                      - Default preferences and keybinds (30 files)`n"
$manifestText += "  data/                      - Compiled game data (auto-generated)`n"
$manifestText += "  xtra/                      - Extended resources (fonts, sound, music, minimal graphics)`n"
$manifestText += "    font/                    - Font files`n"
$manifestText += "    sound/                   - Audio files (.ogg only; sound packs excluded)`n"
$manifestText += "    music/                   - Background music files (.ogg only)`n"
$manifestText += "    graf/                    - Graphics (only 16x16.png)`n"
$manifestText += "  docs/                      - Documentation and manuals (6 files)`n"
$manifestText += "  apex/                      - Runtime data directory (auto-generated)`n"
$manifestText += "  save/                      - Saved games (auto-generated)`n"
$manifestText += "  user/                      - User data (auto-generated)`n"
$manifestText += "*.dll                        - SDL3 runtime libraries ($($requiredDlls.Count) DLLs)`n"
if ($IncludeCoverArt) {
    $manifestText += "CoverArt/                    - Game cover art and promotional images`n"
}
$manifestText += "`n"
$manifestText += "=== KEY DIFFERENCES FROM STANDARD BUILD ===" + "`n"
$manifestText += "- Portable mode stores all user data (saves, settings) in lib/ folder`n"
$manifestText += "- No system-wide user profile required`n"
$manifestText += "- Can be run from any location (USB drive, etc.)`n"
$manifestText += "- Configuration: Edit lib/pref/pref.prf`n"
$manifestText += "- Sound config: Edit lib/pref/sound.json`n"
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

$manifestText | Set-Content (Join-Path $outputFolderPath "MANIFEST.txt")
Write-Host "  [OK] MANIFEST.txt"

# Step 7: Calculate and display results
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Portable release build complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Location: $outputFolderPath" -ForegroundColor Cyan

$folderSize = (Get-ChildItem -Recurse $outputFolderPath | Measure-Object -Property Length -Sum).Sum / 1MB
$folderSizeGB = $folderSize / 1024
if ($folderSize -gt 1024) {
    Write-Host "Size: $([math]::Round($folderSizeGB, 2)) GB"
} else {
    Write-Host "Size: $([math]::Round($folderSize, 2)) MB"
}

Write-Host ""
Write-Host "Contents:" -ForegroundColor Yellow
Write-Host "  - Executable: sil-more.exe (portable mode)"
Write-Host "  - SDL3 Runtime: $copiedDlls DLLs"
Write-Host "  - Game Data: $foldersCopied folders + empty directories"
Write-Host "  - Music files: $musicFilesCopied .ogg files"
if ($IncludeCoverArt) {
    Write-Host "  - CoverArt: Included"
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Test: $(Join-Path $outputFolderPath 'sil-more.exe')"
Write-Host "  2. Saves will be stored in: lib/save/"
Write-Host "  3. Package distributions after the standard release folder exists: $archiveScriptPath -ReleaseFolder $standardOutputFolderPath -PortableReleaseFolder $outputFolderPath -Version $Version -ApkPath $releaseApkPath"
Write-Host ""
