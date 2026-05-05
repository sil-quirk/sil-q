param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Release',

    [string]$AdbPath,

    [string]$KeystorePath = $env:SIL_MORE_RELEASE_STORE_FILE,

    [string]$KeystoreAlias = $env:SIL_MORE_RELEASE_KEY_ALIAS,

    [switch]$AllowDowngrade,

    [switch]$LaunchApp
)

$ErrorActionPreference = 'Stop'

$repoRoot = $PSScriptRoot
$buildScript = Join-Path $repoRoot 'build-android-apk.ps1'
$installScript = Join-Path $repoRoot 'install-android-apk.ps1'

if (-not (Test-Path $buildScript)) {
    throw "Missing script: $buildScript"
}
if (-not (Test-Path $installScript)) {
    throw "Missing script: $installScript"
}

$buildParams = @{
    Config = $Config
}
if ($Config -eq 'Release') {
    if ($KeystorePath) {
        $buildParams['KeystorePath'] = $KeystorePath
    }
    if ($KeystoreAlias) {
        $buildParams['KeystoreAlias'] = $KeystoreAlias
    }
}

& $buildScript @buildParams

$installParams = @{
    Config = $Config
}
if ($AdbPath) {
    $installParams['AdbPath'] = $AdbPath
}
if ($AllowDowngrade) {
    $installParams['AllowDowngrade'] = $true
}

& $installScript @installParams

if ($LaunchApp) {
    $adb = if ($AdbPath) {
        $AdbPath
    } else {
        $cmd = Get-Command adb -ErrorAction SilentlyContinue
        if ($cmd) {
            $cmd.Source
        } else {
            Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
        }
    }

    if (-not (Test-Path $adb)) {
        throw "adb not found for launch step: $adb"
    }

    & $adb shell am start -n com.silmore.myapp/com.silqh.silmore.SilMoreActivity
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to launch app via adb (exit code $LASTEXITCODE)"
    }

    Write-Host 'Launched com.silmore.myapp.' -ForegroundColor Green
}

Write-Host "Deploy complete for $Config." -ForegroundColor Green
