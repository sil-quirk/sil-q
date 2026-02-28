param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

    [string]$AdbPath,

    [switch]$AllowDowngrade
)

$ErrorActionPreference = 'Stop'

function Resolve-AdbPath {
    param([string]$Provided)

    if ($Provided) {
        if (Test-Path $Provided) { return $Provided }
        throw "Provided adb path does not exist: $Provided"
    }

    $cmd = Get-Command adb -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $default = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
    if (Test-Path $default) { return $default }

    throw 'adb not found. Add platform-tools to PATH or pass -AdbPath.'
}

$adb = Resolve-AdbPath -Provided $AdbPath

$apk = if ($Config -eq 'Release') {
    Join-Path $PSScriptRoot 'android\app\build\outputs\apk\release\app-release.apk'
} else {
    Join-Path $PSScriptRoot 'android\app\build\outputs\apk\debug\app-debug.apk'
}

if (-not (Test-Path $apk)) {
    throw "APK not found: $apk`nBuild it first via Android Studio or build-android-apk.ps1"
}

& $adb devices
if ($LASTEXITCODE -ne 0) {
    throw "adb devices failed with exit code $LASTEXITCODE"
}

$deviceLines = & $adb devices
$connected = @($deviceLines | Where-Object { $_ -match "^\S+\s+device$" })
if ($connected.Count -eq 0) {
    throw 'No authorized adb device detected. Connect device, enable USB debugging, and accept RSA prompt.'
}

$installArgs = @('install', '-r')
if ($AllowDowngrade) {
    $installArgs += '-d'
}
$installArgs += $apk

& $adb @installArgs
if ($LASTEXITCODE -ne 0) {
    throw "adb install failed with exit code $LASTEXITCODE"
}

Write-Host "Installed APK: $apk" -ForegroundColor Green
