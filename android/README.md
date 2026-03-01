# Sil-More Android build (SDL3)

This folder contains an Android Studio / Gradle project that builds Sil-More as an SDL-based Android app.

## What this does

- Builds the native code via **CMake + Android NDK**.
- Packages game content from the repo `lib/` directory into APK **assets** under `assets/lib/...`.
- Excludes runtime-state folders (`lib/save`, `lib/user`, `lib/apex`) so APKs never ship developer saves/metarun data.
- Expects SDL's Android Java glue (e.g. `org.libsdl.app.SDLActivity`) to be available.

## Prereqs

- Android Studio (or Gradle + JDK)
- Android SDK + NDK installed

## SDL dependencies

This repo's top-level CMake can either:

1. Use `find_package(SDL3 ...)` (desktop-style), OR
2. Build SDL deps from source via `-DSIL_BUILD_WITH_SDL_SOURCES=ON`, expecting:

- `external/SDL` (SDL3)
- `external/SDL_image`
- `external/SDL_ttf`

If you use option (2), clone those repos into `external/`.

Upstream reference: https://wiki.libsdl.org/SDL3/README-android

In SDL3, the Java shim Activity class is `org.libsdl.app.SDLActivity` (from `SDL/android-project/app/src/main/java/org/libsdl/app/SDLActivity.java`). Our `SilMoreActivity` subclasses it.

## Build

1. Open the `android/` folder in Android Studio.
2. Install Android SDK + NDK in Android Studio (SDK Manager).
3. Build/Run the `app` configuration (ABI is set to `arm64-v8a`).

## Command-line native build (optional)

From repo root (PowerShell):

`./build-android.ps1 -Abi arm64-v8a -Config Release`

This script auto-detects NDK from:

- `ANDROID_NDK_HOME`
- `%LOCALAPPDATA%/Android/Sdk/ndk/*`
- `ANDROID_HOME/ndk/*`
- `ANDROID_SDK_ROOT/ndk/*`

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

## Notes

- The game stores saves/config under the app's private storage via SDL user folders.
- No savefiles are bundled in the APK; first launch starts with an empty save directory.
- `sound.json` is seeded into the user folder on first run.
