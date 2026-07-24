param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

    [ValidateSet('Sideload','Play')]
    [string]$Delivery = 'Sideload',

    [string]$AdbPath,

    [string]$Serial,

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

function Get-ApkMetadata {
    param([string]$ApkPath)

    $metadataPath = Join-Path (Split-Path -Path $ApkPath -Parent) 'output-metadata.json'
    if (-not (Test-Path $metadataPath)) {
        return $null
    }

    try {
        $metadata = Get-Content -Path $metadataPath -Raw | ConvertFrom-Json
        $element = @($metadata.elements)[0]
        if (-not $element) {
            return $null
        }

        return [PSCustomObject]@{
            ApplicationId = $metadata.applicationId
            VersionCode   = $element.versionCode
            VersionName   = $element.versionName
        }
    }
    catch {
        return $null
    }
}

function Invoke-ExternalCommand {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()

    try {
        $process = Start-Process -FilePath $FilePath -ArgumentList $Arguments -NoNewWindow -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
        $stdout = Get-Content -Path $stdoutPath -Raw
        $stderr = Get-Content -Path $stderrPath -Raw

        $combined = @()
        if ($stdout) {
            $combined += $stdout.Trim()
        }
        if ($stderr) {
            $combined += $stderr.Trim()
        }

        return [PSCustomObject]@{
            ExitCode = $process.ExitCode
            StdOut   = $stdout
            StdErr   = $stderr
            Output   = ($combined -join "`n").Trim()
        }
    }
    finally {
        Remove-Item -Path $stdoutPath, $stderrPath -ErrorAction SilentlyContinue
    }
}

function Get-ConnectedDevices {
    param([string]$Adb)

    $deviceResult = Invoke-ExternalCommand -FilePath $Adb -Arguments @('devices')
    if ($deviceResult.ExitCode -ne 0) {
        $details = $deviceResult.Output
        if ($details) {
            throw "adb devices failed with exit code $($deviceResult.ExitCode)`n`n$details"
        }
        throw "adb devices failed with exit code $($deviceResult.ExitCode)"
    }

    $deviceOutput = @($deviceResult.StdOut -split "\r?\n")
    return @($deviceOutput | Where-Object {
        $_ -is [string] -and $_ -match '^(?<serial>\S+)\s+device(\s|$)'
    } | ForEach-Object {
        [PSCustomObject]@{
            Serial = $Matches.serial
            Line   = $_
        }
    })
}

function Resolve-TargetDeviceSerial {
    param(
        [string]$RequestedSerial,
        [object[]]$ConnectedDevices
    )

    if ($ConnectedDevices.Count -eq 0) {
        throw 'No authorized adb device detected. Connect device, enable USB debugging, and accept RSA prompt.'
    }

    if ($RequestedSerial) {
        $selected = @($ConnectedDevices | Where-Object { $_.Serial -eq $RequestedSerial })
        if ($selected.Count -eq 0) {
            $available = ($ConnectedDevices | ForEach-Object { $_.Serial }) -join ', '
            throw "Requested adb device '$RequestedSerial' is not available. Connected devices: $available"
        }
        return $RequestedSerial
    }

    if ($ConnectedDevices.Count -gt 1) {
        $available = ($ConnectedDevices | ForEach-Object { $_.Serial }) -join ', '
        throw "Multiple adb devices detected ($available). Re-run with -Serial <device-serial>."
    }

    return $ConnectedDevices[0].Serial
}

function Get-InstalledPackageInfo {
    param(
        [string]$Adb,
        [string]$Serial,
        [string]$PackageName
    )

    if ([string]::IsNullOrWhiteSpace($PackageName)) {
        return $null
    }

    $result = Invoke-ExternalCommand -FilePath $Adb -Arguments @('-s', $Serial, 'shell', 'dumpsys', 'package', $PackageName)
    if ($result.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($result.StdOut)) {
        return $null
    }

    $output = $result.StdOut
    if ($output -notmatch [regex]::Escape($PackageName)) {
        return $null
    }

    $installer = $null
    $versionName = $null
    $versionCode = $null

    if ($output -match 'installerPackageName=(?<value>\S+)') {
        $installer = $Matches.value
    }
    if ($output -match 'versionName=(?<value>\S+)') {
        $versionName = $Matches.value
    }
    if ($output -match 'versionCode=(?<value>\d+)') {
        $versionCode = $Matches.value
    }

    return [PSCustomObject]@{
        InstallerPackageName = $installer
        VersionName          = $versionName
        VersionCode          = $versionCode
    }
}

function New-AdbInstallFailureMessage {
    param(
        [int]$ExitCode,
        [string]$AdbOutput,
        [object]$ApkMetadata,
        [string]$TargetSerial,
        [object]$InstalledPackageInfo
    )

    $lines = @("adb install failed with exit code $ExitCode")

    if ($TargetSerial) {
        $lines += "Device: $TargetSerial"
    }

    if ($ApkMetadata) {
        $lines += "Package: $($ApkMetadata.ApplicationId)"
        $lines += "Version: $($ApkMetadata.VersionName) ($($ApkMetadata.VersionCode))"
    }

    $trimmedOutput = $AdbOutput.Trim()
    if ($trimmedOutput) {
        $lines += "adb output:"
        $lines += $trimmedOutput
    }

    if ($trimmedOutput -match 'INSTALL_FAILED_UPDATE_INCOMPATIBLE') {
        $packageName = if ($ApkMetadata -and $ApkMetadata.ApplicationId) {
            $ApkMetadata.ApplicationId
        } else {
            'the target package'
        }

        if ($InstalledPackageInfo -and $InstalledPackageInfo.InstallerPackageName -eq 'com.android.vending') {
            $lines += "Cause: the installed copy of $packageName came from Google Play and is signed with Google Play's app signing key."
            $lines += 'This local APK is signed with the upload key, which is correct for AAB upload but does not match a Play-installed app.'
            $lines += 'Fix: install the update through a Play track/internal app sharing. This script will not uninstall an existing app because that would delete its local data.'
        } else {
            $lines += "Cause: an installed copy of $packageName is signed with a different key."
            $lines += "Fix: rebuild/sign the APK with the same key as the installed app. This script will not uninstall the existing app because that would delete its local data."
        }
    }
    elseif ($trimmedOutput -match 'INSTALL_FAILED_VERSION_DOWNGRADE') {
        $lines += 'Cause: the device already has a newer versionCode installed.'
        $lines += 'Fix: re-run with -AllowDowngrade, or build an APK with a versionCode at least as new as the installed app. This script will not uninstall the existing app because that would delete its local data.'
    }
    elseif ($trimmedOutput -match 'INSTALL_FAILED_NO_MATCHING_ABIS') {
        $lines += 'Cause: the APK does not contain native libraries for this device ABI.'
        $lines += 'Fix: rebuild the APK with an ABI that matches the target device.'
    }
    elseif ($trimmedOutput -match 'more than one device/emulator') {
        $lines += 'Cause: adb sees multiple targets.'
        $lines += 'Fix: re-run with -Serial <device-serial>.'
    }

    return $lines -join "`n"
}

function Get-AndroidApkPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,

        [Parameter(Mandatory = $true)]
        [string]$Delivery,

        [Parameter(Mandatory = $true)]
        [string]$Config
    )

    $flavor = $Delivery.ToLowerInvariant()
    $buildType = $Config.ToLowerInvariant()
    return Join-Path $RepoRoot "android\app\build\outputs\apk\$flavor\$buildType\app-$flavor-$buildType.apk"
}

$adb = Resolve-AdbPath -Provided $AdbPath

$apk = Get-AndroidApkPath -RepoRoot $PSScriptRoot -Delivery $Delivery -Config $Config

if (-not (Test-Path $apk)) {
    throw "APK not found: $apk`nBuild it first via Android Studio or build-android-apk.ps1 -Config $Config -Delivery $Delivery"
}

$apkMetadata = Get-ApkMetadata -ApkPath $apk
if (-not $apkMetadata -or [string]::IsNullOrWhiteSpace($apkMetadata.ApplicationId)) {
    throw 'Could not determine the APK applicationId from output-metadata.json. Refusing to install because the target package identity cannot be verified.'
}

$connected = Get-ConnectedDevices -Adb $adb
$targetSerial = Resolve-TargetDeviceSerial -RequestedSerial $Serial -ConnectedDevices $connected
$installedPackageInfo = Get-InstalledPackageInfo -Adb $adb -Serial $targetSerial -PackageName $apkMetadata.ApplicationId

$installArgs = @('-s', $targetSerial, 'install', '-r')
if ($AllowDowngrade) {
    $installArgs += '-d'
}
$installArgs += $apk

$packageLabel = if ($apkMetadata -and $apkMetadata.ApplicationId) {
    $apkMetadata.ApplicationId
} else {
    Split-Path -Path $apk -Leaf
}

Write-Host "Installing $packageLabel to $targetSerial..." -ForegroundColor Cyan
Write-Host 'Install mode: in-place replacement (-r); existing app data will be preserved.' -ForegroundColor DarkGray

$installResult = Invoke-ExternalCommand -FilePath $adb -Arguments $installArgs
if ($installResult.ExitCode -ne 0) {
    throw (New-AdbInstallFailureMessage -ExitCode $installResult.ExitCode -AdbOutput $installResult.Output -ApkMetadata $apkMetadata -TargetSerial $targetSerial -InstalledPackageInfo $installedPackageInfo)
}

if ($installResult.Output) {
    Write-Host $installResult.Output
}

Write-Host "Installed APK: $apk" -ForegroundColor Green
