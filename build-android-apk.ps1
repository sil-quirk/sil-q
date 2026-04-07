param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug'
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

$androidDir = Join-Path $PSScriptRoot 'android'
if (-not (Test-Path $androidDir)) {
    throw "Android project folder not found: $androidDir"
}

Push-Location $androidDir
try {
    $task = if ($Config -eq 'Release') { 'assembleRelease' } else { 'assembleDebug' }

    $javaHome = Resolve-JavaHome
    if ($javaHome) {
        $env:JAVA_HOME = $javaHome
        $env:Path = "$javaHome\bin;$env:Path"
    }

    if (Test-Path '.\\gradlew.bat') {
        & .\\gradlew.bat $task
        if ($LASTEXITCODE -ne 0) {
            throw "Gradle wrapper failed with exit code $LASTEXITCODE"
        }
        $apkPath = if ($Config -eq 'Release') {
            Join-Path $androidDir 'app\build\outputs\apk\release\app-release.apk'
        } else {
            Join-Path $androidDir 'app\build\outputs\apk\debug\app-debug.apk'
        }
        Write-Host "APK build completed via gradlew ($task)." -ForegroundColor Green
        if (Test-Path $apkPath) {
            Write-Host "APK: $apkPath" -ForegroundColor Green
        }
        return
    }

    if (Get-Command gradle -ErrorAction SilentlyContinue) {
        & gradle $task
        if ($LASTEXITCODE -ne 0) {
            throw "Gradle CLI failed with exit code $LASTEXITCODE"
        }
        $apkPath = if ($Config -eq 'Release') {
            Join-Path $androidDir 'app\build\outputs\apk\release\app-release.apk'
        } else {
            Join-Path $androidDir 'app\build\outputs\apk\debug\app-debug.apk'
        }
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
  .\install-android-apk.ps1 -Config $Config
"@
}
finally {
    Pop-Location
}
