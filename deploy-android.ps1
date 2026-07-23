param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Release',

    [ValidateSet('Sideload','Play')]
    [string]$Delivery = 'Sideload',

    [string]$AdbPath,

    [string]$Serial,

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

function Get-AndroidApplicationId {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Delivery,

        [Parameter(Mandatory = $true)]
        [string]$Config
    )

    if ($Delivery -eq 'Play') {
        $applicationId = 'com.silmore.myapp'
    } else {
        $applicationId = 'com.silmore.myapp.sideload'
    }

    if ($Config -eq 'Debug') {
        return "$applicationId.debug"
    }

    return $applicationId
}

$buildParams = @{
    Config = $Config
    Delivery = $Delivery
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
    Delivery = $Delivery
}
if ($AdbPath) {
    $installParams['AdbPath'] = $AdbPath
}
if ($Serial) {
    $installParams['Serial'] = $Serial
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

    $applicationId = Get-AndroidApplicationId -Delivery $Delivery -Config $Config
    $launchArgs = @()
    if ($Serial) {
        $launchArgs += @('-s', $Serial)
    }
    $launchArgs += @('shell', 'am', 'start', '-n', "$applicationId/com.silqh.silmore.SilMoreActivity")

    & $adb @launchArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to launch app via adb (exit code $LASTEXITCODE)"
    }

    Write-Host "Launched $applicationId." -ForegroundColor Green
}

Write-Host "Deploy complete for $Delivery $Config." -ForegroundColor Green
