# Compiling Instructions

> [!NOTE]
> You can download ready-to-use Sil-Q packages for Windows, macOS, and Linux
> from the [Releases](https://github.com/sil-quirk/sil-q/releases) page.
> While you can compile the game from sources, this is no longer needed for most
> users since release 1.5.1.

## Preparation (for all platforms)

The initial step is to download the Sil-Q source code. You have two options.

**Option 1: Clone the Sil-Q git repository.**

If you have `git` installed, git clone the Sil-Q repository and then proceed
with one of the next sections (depending on your operating system) to compile
the game:

```shell
# Clone the Sil-Q repository and change to its directory
$ git clone git@github.com:sil-quirk/sil-q.git Sil-Q && cd Sil-Q
```

**Option 2: Download the Sil-Q source release file.**

If you have downloaded the `Sil-src.zip` source release file:

- Unzip the file `Sil-src.zip`. It will become a folder called `Sil-Q-src`,
  which contains sub-folders called `lib/` and `src/`.
  - The `src/` folder contains all the source code.
  - The `lib/` folder contains other files that the game uses. **Once compiled,
    the Sil-Q game executable (such as `sil.exe` on Windows) still requires the
    files and folders under the `lib/` directory.**
- Move the `Sil-Q-src` folder to wherever you want to keep it. It is recommended
  to rename the folder to `Sil-Q`.
- Proceed with one of the next sections (depending on your operating) system to
  compile the game. Once compiled, the game will be automatically installed in
  the `Sil-Q` folder as well.

## Microsoft Windows with Cygwin

**Step 1**: Get the free Cygwin compiler.

- Download the free Cygwin compiler. It provides a shell interface very similar
  to a normal Unix/Linux shell with many useful tools. Install it and start the
  Cygwin terminal.
- Note: Make sure `cmake`, `ninja`, and the mingw C compiler are installed,
  because they may not be included in your Cygwin default installation.

**Step 2**: Compile Sil-Q. Open a terminal in the top-level directory of the
Sil-Q folder.

```shell
$ cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
$ cp build/sil.exe sil.exe
```

**Step 3**: Run Sil-Q via the `sil.exe` executable in the top-level directory.
Note that you still need the `lib/` directory to run Sil-Q. It must be located
in the same directory as `sil.exe`.

```shell
# Run the Sil-Q executable to start the game.
$ ./sil.exe
```

Enjoy!

## Windows with Visual Studio 2022 (experimental, tested on MSVC 2022)

> WARNING: This is a very new and very raw port. It requires further testing.

**Step 1**: Install Microsoft Visual Studio 2022.

**Step 2**: Compile Sil-Q.

- Assuming you have MSVC 2022, this should be as simple as selecting "Debug" or
  "Release", opening `sil-q.sln` in the `msvc2022/` directory, and selecting
  "Build Solution" from the Build menu.

**Step 3**: Run Sil-Q via the `sil.exe` executable in the top-level directory.
Note that you still need the `lib/` directory to run Sil-Q. It must be located
in the same directory as `sil.exe`.

Enjoy!

## macOS native app (Cocoa) with Xcode

> These instructions were tested with Xcode 11.6 on macOS 10.15.5 (Catalina) and
> with Xcode 26.2 on macOS 26.2 (Tahoe).

**Step 1**: Install Xcode from the Apple App Store. You also need `cmake` and
`ninja`, which you can install with Homebrew via `brew install cmake ninja`.

**Step 2**: Compile Sil-Q. Open a terminal in the top-level directory of the
Sil-Q folder.

```shell
$ cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
$ cp -R build/Sil.app Sil.app
```

This generates a universal macOS application, `Sil.app`, in the top-level
directory of Sil-Q. You may move `Sil.app` to wherever you like, such as your
Mac's `Applications` folder.

> NOTE: Building with `-DCMAKE_BUILD_TYPE=Debug` instead signs the app with the
> `get-task-allow` entitlement, which Xcode's Instruments needs in order to
> attach to a running Sil-Q. Release builds are not signed with it.

**Step 3**: Run Sil-Q via the `Sil.app` application.

```shell
# Run Sil.app to start the game.
$ open Sil.app

# Alternatively, use macOS Finder to navigate to where you placed `Sil.app`,
# then double-click on it to run it.
```

> NOTE: When Sil-Q launches, you will see a macOS dialog window about granting
> Sil-Q access to your `Documents` folder. You must approve this access, because
> it is required by Sil-Q to store its savefiles for your in-game characters,
> the high score file, and additional game data in the folder `Documents/Sil/`.

Enjoy!

## Linux/Unix (incl. non-Cocoa macOS) with gcc or clang

> NOTE: Most macOS users probably prefer a native macOS app (Cocoa), which is
> described further above. This section here is for those macOS users who want
> to run Sil-Q in a terminal and/or via X11.

There are several graphical frontends available for Sil-Q:

- X11: Allows multiple windows, has correct colours.
- GCU: Works in a terminal using curses/ncurses, has only 16 or 8 colours.

**Step 1**: Install X11 and/or GCU (curses/ncurses) dependencies. You also need
`cmake` and `ninja`.

> Debian/Ubuntu Linux:
> `sudo apt-get install gcc cmake ninja-build libncursesw5-dev libx11-dev`
>
> Fedora Linux:
> `sudo dnf install -y gcc cmake ninja-build ncurses-devel libX11-devel`
>
> macOS systems (with default macOS clang compiler and homebrew, no X11):
> `brew install cmake ninja ncurses`

**Step 2**: Compile Sil-Q. Open a terminal in the top-level directory of the
Sil-Q folder.

```shell
$ cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
$ cp build/sil sil
```

On macOS you typically want to build without X11 support:

```shell
$ cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DSUPPORT_COCOA_FRONTEND=OFF -DSUPPORT_X11_FRONTEND=OFF
$ cmake --build build
$ cp build/sil sil
```

**Step 3**: Run Sil-Q via the `sil` executable in the top-level directory. Note
that you still need the `lib/` directory to run Sil-Q. It must be located in the
same directory as the `sil` executable.

```shell
# Run the Sil-Q executable to start the game.
$ ./sil
```

> NOTE: If you are on macOS 10.15 or later and haven't run Sil-Q before, the
> macOS Gatekeeper security feature may prevent you from starting the `sil`
> executable.
> You may see a macOS dialog window "sil Not Opened" and a note about Apple
> not being able to verify that the app is free of malware. In the dialog
> window, click the button "Done" (not "Move to Trash"). Then go to your Mac's
> `System Settings` > `Privacy & Security`, scroll all the way down to the
> "Security" section, which will have an entry "'sil' was blocked to protect
> your Mac.". Here, click on the button "Open Anyway" and confirm that you do
> want to launch Sil-Q. See [Safely open apps on your
> Mac](https://support.apple.com/en-us/102445) in the macOS documentation.

Enjoy!
