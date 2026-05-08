param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

    [ValidateSet('Sideload','Play')]
    [string]$Delivery = 'Sideload',

    [string]$KeystorePath = $env:SIL_MORE_RELEASE_STORE_FILE,

    [string]$KeystoreAlias = $env:SIL_MORE_RELEASE_KEY_ALIAS
)

$ErrorActionPreference = 'Stop'

function Resolve-JavaHome {
    if ($env:JAVA_HOME -and (Test-Path (Join-Path $env:JAVA_HOME 'bin\java.exe'))) {
        return $env:JAVA_HOME
    }

    $studioJbr = 'C:\Program Files\Android\Android Studio\jbr'
    if (Test-Path (Join-Path $studioJbr 'bin\java.exe')) {
        return $studioJbr
    }

    return $null
}

function Get-DefaultReleaseKeystorePath {
    $homeRoots = @()

    if ($env:USERPROFILE) {
        $homeRoots += $env:USERPROFILE
    }
    if ($HOME) {
        $homeRoots += $HOME
    }

    foreach ($homeRoot in ($homeRoots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
        $candidate = Join-Path $homeRoot '.sil-more\play-upload-keystore.jks'
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

function Get-DefaultReleaseSigningEnvFilePath {
    $homeRoots = @()

    if ($env:USERPROFILE) {
        $homeRoots += $env:USERPROFILE
    }
    if ($HOME) {
        $homeRoots += $HOME
    }

    foreach ($homeRoot in ($homeRoots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
        $candidate = Join-Path $homeRoot '.sil-more\play-upload-keystore.env.ps1'
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

function Import-DefaultReleaseSigningEnvironment {
    $envFile = Get-DefaultReleaseSigningEnvFilePath
    if (-not $envFile) {
        return
    }

    . $envFile
    Write-Host "Loaded release signing environment: $envFile" -ForegroundColor Cyan
}

function Resolve-RequiredFilePath {
    param(
        [AllowNull()]
        [string]$PathValue,

        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        throw "$Label is required. Pass it as a parameter or set the matching SIL_MORE_RELEASE_* environment variable."
    }

    $resolved = Resolve-Path -LiteralPath $PathValue -ErrorAction SilentlyContinue
    if (-not $resolved) {
        throw "$Label not found: $PathValue"
    }

    return $resolved.ProviderPath
}

function Convert-SecureStringToPlainText {
    param(
        [Parameter(Mandatory = $true)]
        [System.Security.SecureString]$SecureString
    )

    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($SecureString)
    try {
        return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
    }
}

function Read-SecretValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$EnvName,

        [Parameter(Mandatory = $true)]
        [string]$Prompt
    )

    $value = [Environment]::GetEnvironmentVariable($EnvName, 'Process')
    if (-not [string]::IsNullOrEmpty($value)) {
        return $value
    }

    $secureValue = Read-Host -Prompt $Prompt -AsSecureString
    if ($secureValue.Length -eq 0) {
        throw "$EnvName cannot be empty."
    }

    return Convert-SecureStringToPlainText $secureValue
}

function Read-OptionalSecretValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$EnvName,

        [Parameter(Mandatory = $true)]
        [string]$Prompt,

        [Parameter(Mandatory = $true)]
        [string]$DefaultValue
    )

    $value = [Environment]::GetEnvironmentVariable($EnvName, 'Process')
    if (-not [string]::IsNullOrEmpty($value)) {
        return $value
    }

    $secureValue = Read-Host -Prompt $Prompt -AsSecureString
    if ($secureValue.Length -eq 0) {
        return $DefaultValue
    }

    return Convert-SecureStringToPlainText $secureValue
}

function Set-ProcessEnvironmentValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [AllowNull()]
        [string]$Value
    )

    [Environment]::SetEnvironmentVariable($Name, $Value, 'Process')
}

function Get-AndroidAssembleTaskName {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Delivery,

        [Parameter(Mandatory = $true)]
        [string]$Config
    )

    $flavor = if ($Delivery -eq 'Play') { 'Play' } else { 'Sideload' }
    $buildType = if ($Config -eq 'Release') { 'Release' } else { 'Debug' }

    return "assemble$flavor$buildType"
}

function Get-AndroidApkPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$AndroidDir,

        [Parameter(Mandatory = $true)]
        [string]$Delivery,

        [Parameter(Mandatory = $true)]
        [string]$Config
    )

    $flavor = $Delivery.ToLowerInvariant()
    $buildType = $Config.ToLowerInvariant()
    return Join-Path $AndroidDir "app\build\outputs\apk\$flavor\$buildType\app-$flavor-$buildType.apk"
}

$androidDir = Join-Path $PSScriptRoot 'android'
if (-not (Test-Path $androidDir)) {
    throw "Android project folder not found: $androidDir"
}

$envNamesToRestore = @(
    'JAVA_HOME',
    'Path',
    'SIL_MORE_RELEASE_STORE_FILE',
    'SIL_MORE_RELEASE_STORE_PASSWORD',
    'SIL_MORE_RELEASE_KEY_ALIAS',
    'SIL_MORE_RELEASE_KEY_PASSWORD'
)
$previousEnvironment = @{}
foreach ($name in $envNamesToRestore) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

$pushedLocation = $false
try {
    $releaseSigning = $null
    if ($Config -eq 'Release') {
        Import-DefaultReleaseSigningEnvironment

        if ([string]::IsNullOrWhiteSpace($KeystorePath)) {
            $KeystorePath = [Environment]::GetEnvironmentVariable('SIL_MORE_RELEASE_STORE_FILE', 'Process')
        }
        if ([string]::IsNullOrWhiteSpace($KeystoreAlias)) {
            $KeystoreAlias = [Environment]::GetEnvironmentVariable('SIL_MORE_RELEASE_KEY_ALIAS', 'Process')
        }
        if ([string]::IsNullOrWhiteSpace($KeystorePath)) {
            $KeystorePath = Get-DefaultReleaseKeystorePath
        }
        if ([string]::IsNullOrWhiteSpace($KeystoreAlias)) {
            $KeystoreAlias = 'upload'
        }

        $keystoreFile = Resolve-RequiredFilePath -PathValue $KeystorePath -Label 'Release keystore'

        $storePassword = Read-SecretValue `
            -EnvName 'SIL_MORE_RELEASE_STORE_PASSWORD' `
            -Prompt 'Release keystore password'

        $keyPassword = Read-OptionalSecretValue `
            -EnvName 'SIL_MORE_RELEASE_KEY_PASSWORD' `
            -Prompt 'Release key password (press Enter to reuse the keystore password)' `
            -DefaultValue $storePassword

        $releaseSigning = [PSCustomObject]@{
            StoreFile     = $keystoreFile
            StorePassword = $storePassword
            KeyAlias      = $KeystoreAlias
            KeyPassword   = $keyPassword
        }
    }

    Push-Location $androidDir
    $pushedLocation = $true

    $task = Get-AndroidAssembleTaskName -Delivery $Delivery -Config $Config

    $javaHome = Resolve-JavaHome
    if ($javaHome) {
        Set-ProcessEnvironmentValue -Name 'JAVA_HOME' -Value $javaHome
        Set-ProcessEnvironmentValue -Name 'Path' -Value "$javaHome\bin;$($previousEnvironment['Path'])"
    }

    $gradleArgs = @($task)
    if ($releaseSigning) {
        Set-ProcessEnvironmentValue -Name 'SIL_MORE_RELEASE_STORE_FILE' -Value $releaseSigning.StoreFile
        Set-ProcessEnvironmentValue -Name 'SIL_MORE_RELEASE_STORE_PASSWORD' -Value $releaseSigning.StorePassword
        Set-ProcessEnvironmentValue -Name 'SIL_MORE_RELEASE_KEY_ALIAS' -Value $releaseSigning.KeyAlias
        Set-ProcessEnvironmentValue -Name 'SIL_MORE_RELEASE_KEY_PASSWORD' -Value $releaseSigning.KeyPassword

        $gradleArgs = @('-PSIL_MORE_REQUIRE_RELEASE_SIGNING=true') + $gradleArgs

        Write-Host "Using release keystore: $($releaseSigning.StoreFile)" -ForegroundColor Cyan
        Write-Host "Using release key alias: $($releaseSigning.KeyAlias)" -ForegroundColor Cyan
    }

    if (Test-Path '.\\gradlew.bat') {
        & .\\gradlew.bat @gradleArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Gradle wrapper failed with exit code $LASTEXITCODE"
        }
        $apkPath = Get-AndroidApkPath -AndroidDir $androidDir -Delivery $Delivery -Config $Config
        Write-Host "APK build completed via gradlew ($task)." -ForegroundColor Green
        if (Test-Path $apkPath) {
            Write-Host "APK: $apkPath" -ForegroundColor Green
        }
        return
    }

    if (Get-Command gradle -ErrorAction SilentlyContinue) {
        & gradle @gradleArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Gradle CLI failed with exit code $LASTEXITCODE"
        }
        $apkPath = Get-AndroidApkPath -AndroidDir $androidDir -Delivery $Delivery -Config $Config
        Write-Host "APK build completed via gradle CLI ($task)." -ForegroundColor Green
        if (Test-Path $apkPath) {
            Write-Host "APK: $apkPath" -ForegroundColor Green
        }
        return
    }

    throw @"
No Gradle CLI found and no gradle wrapper present.

Use Android Studio to build the APK:
1) Open android/ project.
2) Let Gradle sync complete.
3) Build > Build APK(s).

Then install with:
  .\install-android-apk.ps1 -Config $Config -Delivery $Delivery
"@
}
finally {
    if ($pushedLocation) {
        Pop-Location
    }
    foreach ($entry in $previousEnvironment.GetEnumerator()) {
        Set-ProcessEnvironmentValue -Name $entry.Key -Value $entry.Value
    }
}
