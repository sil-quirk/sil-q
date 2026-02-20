# Compiling Instructions

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

## Linux/Unix (incl. macOS) with gcc

> NOTE: Most macOS users probably prefer a native macOS app (Cocoa), which is
> described further down below. This section here is for those macOS users who
> want to run Sil-Q in a terminal and/or via X11.

There are several different Linux/Unix (incl. macOS) setups for Sil-Q:

- X11: Allows multiple windows, has correct colours.
- GCU: Works in a terminal using 'curses', has only 16 or 8 colours.

**Step 1**: Modify the Makefile to match your system.

- Verify `Makefile.std` in the `src/` directory and edit as needed.
- Look for the section listing multiple "Variations".
- Choose the combination of compiler flags (CFLAGS) and linker flags (LDFLAGS)
  that you need. The defaults should work on most Linux/Unix systems.
- On macOS, X11 is typically not available by default. In this case, use
  `-DENABLE_X11=0` when running `make`.
- See [`.github/workflows/linux.yml`](../../.github/workflows/linux.yml) and
  [`.github/workflows/mac.yml`](../../.github/workflows/mac.yml)
  (`macos-console` build) for examples.

**Step 2**: Compile Sil-Q. Open a terminal in the top-level directory of the
Sil-Q folder.

```shell
# Go to the src/ directory.
$ cd src/

# Compile the sources.
$ make -f Makefile.std install
```

**Step 3**: Run Sil-Q via the `sil` executable in the top-level directory. Note
that you still need the `lib/` directory to run Sil-Q. It must be located in the
same directory as the `sil` executable.

```shell
# Go back to top-level directory.
$ cd ..

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
> "Security" section, which will have an entry
> "'sil' was blocked to protect your Mac.".
> Here, click on the button "Open Anyway" and confirm that you do
> want to launch Sil-Q. See [Safely open apps on your
> Mac](https://support.apple.com/en-us/102445) in the macOS documentation.

Enjoy!

## Microsoft Windows with Cygwin

**Step 1**: Get the free Cygwin compiler.

- Download the free Cygwin compiler. It provides a shell interface very similar
  to a normal Unix/Linux shell with many useful tools. Install it and start the
  Cygwin terminal.
- Note: Make sure `make` and the mingw C compiler are installed, because they
  may not be included in your Cygwin default installation.

**Step 2**: Compile Sil-Q. Open a terminal in the top-level directory of the
Sil-Q folder.

```shell
# In the Cygwin terminal, go to the src/ directory.
$ cd src/

# Compile the sources.
# pe-x86-64 is for 64-bit windows, pe-i386 for 32-bit Windows.
$ make -f Makefile.cyg PEFORMAT=pe-x86-64 install
```

**Step 3**: Run Sil-Q via the `sil.exe` executable in the top-level directory.
Note that you still need the `lib/` directory to run Sil-Q. It must be located
in the same directory as `sil.exe`.

```shell
# Go back to top-level directory.
$ cd ..

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

**Step 1**: Install Xcode from the Apple App Store.

**Step 2**: Compile Sil-Q. Open a terminal in the top-level directory of the
Sil-Q folder.

> NOTE: Before building for a different set of architectures (Apple Silicon
> `arm64` vs. Intel `x86_64`), run `make -f Makefile.cocoa clean` to clean up
> any object files that may not match your new set of selected architectures.

```shell
# Go to the src/ directory
$ cd src

# Option 1 (default): Generate a universal app that works on both Apple
# Silicon Macs (M1/M2/M3/M4) and Intel-based Macs. Use this option if you
# are unsure.
#           *** Requires Xcode 12.2 or later. ***
$ make -f Makefile.cocoa install

# Option 2: Generate a native app that works only on Apple Silicon Macs.
#           *** Requires Xcode 12.2 or later. ***
$ make -f Makefile.cocoa ARCHS=arm64 install

# Option 3: Generate a native app that works only on Intel-based Macs.
#           If you have an Intel machine, you might be forced to use this
#           option if your Xcode version doesn't support building for
#           Apple Silicon Macs.
$ make -f Makefile.cocoa ARCHS=x86_64 install
```

The compilation generates a macOS application, `Sil.app`, in the top-level
directory of Sil-Q (the parent directory of `src/`). You may move `Sil.app` to
wherever you like, such as your Mac's `Applications` folder.

**Step 3**: Run Sil-Q via the `Sil.app` application.

In a Finder window, navigate to where you placed `Sil.app`. Then double-click on
it to run it.

> NOTE: When Sil-Q launches, you will see a macOS dialog window about granting
> Sil-Q access to your `Documents` folder. You must approve this access, because
> it is required by Sil-Q to store its savefiles for your in-game characters,
> the high score file, and additional game data in the folder `Documents/Sil/`.

Enjoy!
