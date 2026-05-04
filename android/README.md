# Sil-More Android build (SDL3)

This folder contains an Android Studio / Gradle project that builds Sil-More as an SDL-based Android app.

## What this does

- Builds the native code via **CMake + Android NDK**.
- Packages game content from the repo `lib/` directory into APK **assets** under `assets/lib/...`.
- Excludes runtime-state folders (`lib/save`, `lib/user`, `lib/apex`) so APKs never ship developer saves/metarun data.
- Packages audio as `.ogg` only and excludes the legacy `.wav` assets plus `lib/xtra/sound/packs`.
- Expects SDL's Android Java glue (e.g. `org.libsdl.app.SDLActivity`) to be available.

## Prereqs

- Git
- Android Studio (or Gradle + JDK)
- Android SDK + NDK + CMake installed

## SDL dependencies

This repo's top-level CMake supports both package-based and source-based SDL builds.
Android uses the source-based path and expects:

- `external/SDL` (SDL3)
- `external/SDL_image`
- `external/SDL_ttf`
- `external/SDL_mixer`

These directories are tracked as pinned Git submodules in the repo root. Before opening `android/` in Android Studio, initialize them from the repo root:

- `git submodule update --init --recursive`

That restores the exact SDL revisions used by the Android build, including nested upstream submodules.

Upstream reference: https://wiki.libsdl.org/SDL3/README-android

In SDL3, the Java shim Activity class is `org.libsdl.app.SDLActivity` (from `SDL/android-project/app/src/main/java/org/libsdl/app/SDLActivity.java`). Our `SilMoreActivity` subclasses it.

## Build

1. Clone the repo and enter it.
2. Run `git submodule update --init --recursive` from the repo root.
3. Install Android SDK + NDK + CMake in Android Studio (SDK Manager).
4. Open the `android/` folder in Android Studio.
5. Let Gradle sync complete.
6. Build/Run the `app` configuration (ABI is set to `arm64-v8a`).

## Command-line native build (optional)

From repo root (PowerShell):

`./build-android.ps1 -Abi arm64-v8a -Config Release`

This script auto-detects NDK from:

- `ANDROID_NDK_HOME`
- `%LOCALAPPDATA%/Android/Sdk/ndk/*`
- `ANDROID_HOME/ndk/*`
- `ANDROID_SDK_ROOT/ndk/*`

It also auto-detects `cmake.exe` from:

- `PATH`
- `C:\msys64\mingw64\bin\cmake.exe`
- `%LOCALAPPDATA%/Android/Sdk/cmake/*/bin/cmake.exe`
- `ANDROID_HOME/cmake/*/bin/cmake.exe`
- `ANDROID_SDK_ROOT/cmake/*/bin/cmake.exe`

## Deploy to device (ADB)

After Android Studio builds an APK, install with:

`adb install -r android/app/build/outputs/apk/debug/app-debug.apk`

Or use repo helper scripts from root:

- `./build-android-apk.ps1 -Config Debug`
- `./install-android-apk.ps1 -Config Debug`
- `./deploy-android.ps1 -Config Release -LaunchApp`

The install helper can allow downgrade installs when needed:

- `./install-android-apk.ps1 -Config Debug -AllowDowngrade`

The build helper auto-detects Java from Android Studio JBR if `JAVA_HOME` is not set.

Then launch from the device launcher.

## Play Store app bundle (AAB)

Google Play uploads need a release/upload key, not the local debug key. Keep the keystore and passwords out of the repo and pass them through environment variables:

```powershell
$env:SIL_MORE_RELEASE_STORE_FILE = 'C:\path\to\upload-keystore.jks'
$env:SIL_MORE_RELEASE_KEY_ALIAS = 'upload'
$env:SIL_MORE_RELEASE_STORE_PASSWORD = '<keystore password>'
$env:SIL_MORE_RELEASE_KEY_PASSWORD = '<key password>'

.\build-android-bundle.ps1 -CompileSdk 35
```

`SIL_MORE_RELEASE_KEY_PASSWORD` may be omitted if the key password is the same as the keystore password; the script will prompt and lets Enter reuse the keystore password. Install Android SDK Platform 35 before passing `-CompileSdk 35`. The script defaults to `-TargetSdk 35` and writes `sil-more-<version>.aab` in the repo root.

## Notes

- The game stores saves/config under the app's private storage via SDL user folders.
- No savefiles are bundled in the APK; first launch starts with an empty save directory.
- `sound.json` is seeded into the user folder on first run.
