# Sil-Morë

Sil-Morë — Shining Darkness is a version of SIL-Q which incorporates two main ideas.
First, it has real life characters from Tolkien FA and storyline.
Secondly, it uses a system of metaruns, where consequtive runs are connected into one storyline idea more like modern Rougue-light games.


# Road Map

## Release of alpha 0.7 (Storyline updates) (done)

- Balance tweaks
- Add unique RHF flags
  * Earendil
  * Turin (done)
  * Celeborn
  * Maedhros (done)
- New heroes
  * Eol (done) 
- Dynamic tile system (done)
-- Wall tiles (done)
-- Floor tiles (done)
-- Doors (done)
- Unique style for each depth (done)
- Level entrance message depending on the style (done)
- Difficulty levels for current run (done) 
- Automatic load if run is not finished (done)

## Release of beta 0.8 (Visual update)

- Change Score to Character database
- Quest systems
-- Update to oath system
- More Vaults
- More monsters

## Release of beta 0.9 

- Multiple runs support 
- SDL 

### Ideas

- New heroes
- More frequent forges for gnomes
- Calculate the forge probability
- New Oaths
- Pride
- Greed
 
## Done

- Implement main curses (done)
- Implement common RHF flags (done)
  * Cheap cost (done)
  * Morgoth Curse (done)
- Add more debugging functions (done)
- Add unique RHF flags (done)
  * Feanor (done)
  * Telchar (done)
  * Gamil (done)
  * Melian (done)
  * Thingol (done)
  * Tuor (done)
  * Hurin (done)
- Figure out last abilities and balance tweaks (done)
  * All starting abilities (done)
  * Multiple starting abilities (done)


## Release closed alpha 0.5 (done)

- Bug fixes (ongoing)
- UI fixes (done)
  * Start menu (done)
  * Character menu (done)
- Decriptions update (done)

## Release alpha 0.6 (UI updates) (done)

- Bug fixes (ongoing) 
- UI updates 
  * Score menu (done) 
  * Final menu (done) 
- Flavor ideas for final menus (done) 


# Compiling Instructions 

Compiling Sil-Q is not very difficult, and has been tested on Windows, Linux, and OS X.

Makefiles for various other systems still exist as a legacy from Sil. If you manage
to build Sil-Q for a system other than Windows, Linux, or OS X please create a git branch
with any changes necessary, update this file and open a github pull request against
https://github.com/sil-quirk/sil-q.

The first step is the same on all systems, so do this and then look through
this file for advice on your specific system.

0. Install the Sil source code:

   Unzip the file "Sil-src.zip". It will become a folder called "Sil"
   which contains subfolders called "lib" and "src". Move it to wherever
   you want to keep it. The src folder contains all the source code
   while the lib folder contains other files that the game uses.
   When you are done compiling, the game will be automatically installed
   in the Sil folder as well.


## Linux or Unix with gcc  (tested with Sil-Q)

   There are several different unix setups for Sil-Q:

   X11: Allows multiple windows, has correct colours.
   GCU: Works in a terminal using 'curses', has only 16 or 8 colours.
   CAP: Works even in old terminals, but is monochrome.

1. Mess with the Makefile:

   Edit Makefile.std in the src directory.
   Look for the section listing multiple "Variations".
   Choose the variation that you like best.
   Remove the # comments from that section's code.
   Comment out the default section.

2. Compile Sil-Q:

   Run "make -f Makefile.std install" in the src directory.

3. Run Sil-Q:

   Go back to the Sil folder and start Sil-Q with "sil".

## Windows with Cygwin   (tested with Sil-Q)

1. Getting the free Cygwin compiler:

   Download the free Cygwin compiler. It provides a shell interface very
   similar to a normal Unix/Linux shell with many useful tools. Install it
   and start the Cygwin terminal. Make sure to get the 32 bit version.

   Note you will have to ensure "make" and the mingw C compiler are installed
   as they may not be included in your Cygwin default installation.

2. Compile Sil-Q:

   In the Cygwin terminal change to the src directory and run
   "make -f Makefile.cyg install".
   Sil should now be compiled and installed into the Sil folder.
   The executable file will be called Sil.exe.

3. Run Sil-Q:

   Go back to the Sil folder and run Sil.exe.

## Windows with Visual Studio 2022 (experimental, tested with Sil-Q on MSVC 2022)

1. Acquire Microsoft Visual Studio 2022.

2. Compile Sil-Q:

   Assuming you have MSVC 2022, this should be as simple as selecting Debug or
   Release, opening sil-q.sln in the msvc2019 directory and selecting Build Solution
   from the Build menu.

3. Run Sil-Q:

   Go back to the Sil folder and run Sil.exe.

NOTE: This is a very new and very raw port, and requires testing.

## macOS

### Prerequisites

Sil-q requires some C compiler (any modern like gcc or clang would work), `ncurses`, and, optionally, `git`. As for the C compiler and `git`, these are likely already installed on your Mac. You can check it. Start Terminal.app (or any other terminal emulator of your choice) and run:

```shell
gcc -dumpversion
```

If it outputs something like '17.0.0' (or other version, depending on your macOS version, that doesn't really matter for this project), you're good. Otherwise, you should install Apple's Xcode Command Line Tools.

#### Xcode Command Line Tools

To do it, run, again, in your terminal:

```shell
xcode-select --install
```

This step will take some time as it will download the files from Apple's servers. It will also ask you whether you want to install it. Agree. After it's done, verify it's working:

```shell
xcode-select -p
```

It should show you the path to where it's installed, like this:

```
/Library/Developer/CommandLineTools
```

Aside from the C and C++ compiler, it also installs `git`.


#### Homebrew

Now, we also need to install homebrew which is a package manager for macOS often used to install development tools and command line utilities. If you already have it installed, of course skip this step. Otherwise, follow [this guide](https://docs.brew.sh/Installation) on their official website.

#### ncurses

This version of sil-q only works in the terminal, so it requires a library to draw ASCII graphics. This is called `ncurses` and is easily installed via homebrew:

```shell
brew install ncurses
```

This will probably also pull its dependencies as brew tracks and installs them automatically.


### Building

#### Getting the code

Now to the actual building! First, clone the repo:

```shell
git clone https://github.com/k0rtesss/sil-qh
```

Or simply download and unpack the ZIP archive from the GitHub page clicking on Code > Download ZIP.

In any case, in terminal, navigate to the folder:

```shell
cd ~/sil-qh
```

or wherever you've cloned or downloaded it.

#### Compiling

Now just change to the source folder:

```shell
cd src
```

and compile the code:

```shell
make -f Makefile.std
```

### Running

If everything went well, you can play! To start sil-qh, change back to the parent folder:

```shell
cd ..
```

and run:

```shell
./src/sil
```

It's essential to run the game from the parent folder as it looks for the game data in locations relative to it.

If the game complains about the metarun file, create the folder it wants:

```shell
mkdir lib/apex/metaruns
```

and restart.

## OS X with Xcode  (tested with Sil-Q; Xcode 11.6 on OS X 10.15.5)

1. Get Xcode from the app store if not installed:

   Use App Store to get Xcode, a free set of development tools from Apple.

2. Compile Sil-Q:

   In a Terminal window, change to the src directory and run
   "make -f Makefile.cocoa install".
   Sil-Q should now be compiled and set up as an OS X application, Sil.app,
   in the folder above the src directory.  You may move Sil.app to wherever
   you like.

   If you are using an arm-based Mac and want a native application, use
   "make -f Makefile.cocoa ARCHS=arm64 install"
   instead of the command given above.  To generate a universal application
   that will run natively on either x86_64 or arm, use
   "make -f Makefile.cocoa ARCHS='x86_64 arm64' install".
   Building for arm likely requires at least Xcode 12.2 or later.  Before
   building for a different set of architectures, run
   "make -f Makefile.cocoa clean" to clean up any object files that may not
   match your new set of selected architectures.

3. Run Sil-Q:

   In a Finder window, navigate to where you placed Sil.app.  Then double
   click on it to run it.  If you are running 10.15 or later and haven't run
   Sil-Q or Sil before, you'll see a dialog about granting Sil-Q access to
   your Documents folder since it wants to place saved games, the high
   score file, and some other data in Documents/Sil.
