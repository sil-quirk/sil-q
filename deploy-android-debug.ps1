param(
    [string]$AdbPath,

    [string]$Serial,

    [switch]$AllowDowngrade,

    [switch]$LaunchApp
)

$ErrorActionPreference = 'Stop'

$deployScript = Join-Path $PSScriptRoot 'deploy-android.ps1'
if (-not (Test-Path -LiteralPath $deployScript)) {
    throw "Missing script: $deployScript"
}

$deployParams = @{
    Config   = 'Debug'
    Delivery = 'Sideload'
}
if ($AdbPath) {
    $deployParams['AdbPath'] = $AdbPath
}
if ($Serial) {
    $deployParams['Serial'] = $Serial
}
if ($AllowDowngrade) {
    $deployParams['AllowDowngrade'] = $true
}
if ($LaunchApp) {
    $deployParams['LaunchApp'] = $true
}

& $deployScript @deployParams
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "Debug Android deployment failed with exit code $LASTEXITCODE"
}

Write-Host 'Debug Android app deployed as com.silmore.myapp.sideload.debug.' -ForegroundColor Green
