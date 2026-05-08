param(
    [ValidateRange(1, 100)]
    [int]$TargetSdk = 35,

    # Leave at 0 to use android/app/build.gradle's default compile SDK.
    [ValidateRange(0, 100)]
    [int]$CompileSdk = 0,

    [string]$KeystorePath = $env:SIL_MORE_RELEASE_STORE_FILE,

    [string]$KeystoreAlias = $env:SIL_MORE_RELEASE_KEY_ALIAS,

    [string]$OutputPath,

    [switch]$Clean,

    [switch]$SkipVerify
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

function Resolve-AndroidSdkRoot {
    $roots = @()

    if ($env:ANDROID_HOME) {
        $roots += $env:ANDROID_HOME
    }

    if ($env:ANDROID_SDK_ROOT) {
        $roots += $env:ANDROID_SDK_ROOT
    }

    if ($env:LOCALAPPDATA) {
        $roots += (Join-Path $env:LOCALAPPDATA 'Android\Sdk')
    }

    foreach ($root in ($roots | Select-Object -Unique)) {
        if (Test-Path (Join-Path $root 'platforms')) {
            return $root
        }
    }

    return $null
}

function Assert-AndroidPlatformInstalled {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ApiLevel
    )

    $sdkRoot = Resolve-AndroidSdkRoot
    if (-not $sdkRoot) {
        throw 'Android SDK not found. Set ANDROID_HOME/ANDROID_SDK_ROOT or install the Android SDK through Android Studio.'
    }

    $platformPath = Join-Path $sdkRoot "platforms\android-$ApiLevel"
    if (-not (Test-Path $platformPath)) {
        throw "Android SDK Platform android-$ApiLevel not found at: $platformPath`nInstall it from Android Studio SDK Manager or run: sdkmanager `"platforms;android-$ApiLevel`""
    }
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

function Get-GameVersion {
    $definesPath = Join-Path $PSScriptRoot 'src\defines.h'
    $versionLine = Select-String -Path $definesPath -Pattern '#define\s+VERSION_STRING\s+"([^"]+)"' | Select-Object -First 1
    if (-not $versionLine) {
        throw "Could not find VERSION_STRING in $definesPath"
    }

    return $versionLine.Matches[0].Groups[1].Value
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

if ($TargetSdk -lt 35) {
    Write-Warning "Target SDK $TargetSdk is below the current Google Play phone/tablet requirement of API 35."
}

if ($CompileSdk -gt 0) {
    Assert-AndroidPlatformInstalled -ApiLevel $CompileSdk
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

try {
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

    $javaHome = Resolve-JavaHome
    if (-not $javaHome) {
        throw 'Java 17 not found. Set JAVA_HOME or install Android Studio with its bundled JBR.'
    }

    $version = Get-GameVersion
    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        $OutputPath = Join-Path $PSScriptRoot "sil-more-$version.aab"
    } elseif (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
        $OutputPath = Join-Path (Get-Location) $OutputPath
    }
    $OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

    $outputDir = Split-Path -Path $OutputPath -Parent
    if (-not (Test-Path $outputDir)) {
        New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    }

    Set-ProcessEnvironmentValue -Name 'JAVA_HOME' -Value $javaHome
    Set-ProcessEnvironmentValue -Name 'Path' -Value "$javaHome\bin;$($previousEnvironment['Path'])"
    Set-ProcessEnvironmentValue -Name 'SIL_MORE_RELEASE_STORE_FILE' -Value $keystoreFile
    Set-ProcessEnvironmentValue -Name 'SIL_MORE_RELEASE_STORE_PASSWORD' -Value $storePassword
    Set-ProcessEnvironmentValue -Name 'SIL_MORE_RELEASE_KEY_ALIAS' -Value $KeystoreAlias
    Set-ProcessEnvironmentValue -Name 'SIL_MORE_RELEASE_KEY_PASSWORD' -Value $keyPassword

    $gradleArgs = @(
        '--no-daemon',
        '--console=plain',
        "-PSIL_MORE_TARGET_SDK=$TargetSdk",
        '-PSIL_MORE_REQUIRE_RELEASE_SIGNING=true'
    )

    if ($CompileSdk -gt 0) {
        $gradleArgs += "-PSIL_MORE_COMPILE_SDK=$CompileSdk"
    }

    if ($Clean) {
        $gradleArgs += 'clean'
    }

    $gradleArgs += ':app:bundlePlayRelease'

    Write-Host "Building Play Store app bundle..." -ForegroundColor Cyan
    Write-Host "Version: $version" -ForegroundColor Cyan
    Write-Host "Target SDK: $TargetSdk" -ForegroundColor Cyan
    if ($CompileSdk -gt 0) {
        Write-Host "Compile SDK: $CompileSdk" -ForegroundColor Cyan
    } else {
        Write-Host 'Compile SDK: project default' -ForegroundColor Cyan
    }
    Write-Host "Keystore: $keystoreFile" -ForegroundColor Cyan
    Write-Host "Key alias: $KeystoreAlias" -ForegroundColor Cyan

    Push-Location $androidDir
    try {
        if (Test-Path '.\gradlew.bat') {
            & .\gradlew.bat @gradleArgs
        } elseif (Get-Command gradle -ErrorAction SilentlyContinue) {
            & gradle @gradleArgs
        } else {
            throw 'No Gradle CLI found and no Gradle wrapper present.'
        }

        if ($LASTEXITCODE -ne 0) {
            throw "Gradle bundlePlayRelease failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }

    $bundlePath = Join-Path $androidDir 'app\build\outputs\bundle\playRelease\app-play-release.aab'
    if (-not (Test-Path $bundlePath)) {
        throw "Expected app bundle not found: $bundlePath"
    }

    if (-not $SkipVerify) {
        $jarsigner = Join-Path $javaHome 'bin\jarsigner.exe'
        if (-not (Test-Path $jarsigner)) {
            $jarsignerCommand = Get-Command jarsigner -ErrorAction SilentlyContinue
            if ($jarsignerCommand) {
                $jarsigner = $jarsignerCommand.Source
            }
        }

        if (Test-Path $jarsigner) {
            & $jarsigner -verify $bundlePath | Out-Null
            if ($LASTEXITCODE -ne 0) {
                throw "jarsigner verification failed for: $bundlePath"
            }
            Write-Host 'Bundle signature verified with jarsigner.' -ForegroundColor Green
        } else {
            Write-Warning 'jarsigner was not found; bundle signature verification was skipped.'
        }
    }

    if ([System.IO.Path]::GetFullPath($bundlePath).TrimEnd('\') -ne $OutputPath.TrimEnd('\')) {
        Copy-Item -LiteralPath $bundlePath -Destination $OutputPath -Force
    }

    Write-Host "App bundle created: $OutputPath" -ForegroundColor Green
}
finally {
    foreach ($entry in $previousEnvironment.GetEnumerator()) {
        Set-ProcessEnvironmentValue -Name $entry.Key -Value $entry.Value
    }
}
