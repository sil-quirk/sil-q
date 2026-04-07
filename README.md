# Sil-Morë

Sil-Morë — Shining Darkness is a version of SIL-Q which incorporates two main ideas.
First, it has real life characters from Tolkien FA and storyline.
Secondly, it uses a system of metaruns, where consequtive runs are connected into one storyline idea more like modern Rougue-light games.

# Compiling (SDL Version)

## Windows

### Prerequisites
- MSYS2 with MinGW64 (install from https://www.msys2.org/)
- CMake
- SDL3 libraries

### Building
1. Install MSYS2 and open the MINGW64 terminal.
2. Install required packages:
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL3 mingw-w64-x86_64-SDL3_image mingw-w64-x86_64-SDL3_ttf make
   ```
3. Navigate to the Sil-More source directory.
4. Run the build script:
   ```bash
   ./build-cmake.bat
   ```
   Or manually configure and build:
   ```bash
   cmake -G "MinGW Makefiles" -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
   cmake --build build --parallel
   ```
5. The executable will be in `build/sil-more.exe` and deployed to `sil-more-windows-sdl3/`.
6. Run from the deployment directory: `cd sil-more-windows-sdl3 && ./sil-more.exe`

## Linux

### Prerequisites
- GCC or Clang compiler
- CMake
- SDL3 development libraries

### Building
1. Install dependencies:
   - **Debian/Ubuntu:**
     ```bash
     sudo apt install build-essential cmake libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev
     ```
   - **Fedora:**
     ```bash
     sudo dnf install gcc cmake SDL3-devel SDL3-image-devel SDL3-ttf-devel
     ```
   - **Arch:**
     ```bash
     sudo pacman -S base-devel cmake sdl3
     paru -S sdl3_ttf sdl3_image # or use any other AUR helper
     ```

2. Navigate to the Sil-More source directory.
3. Configure and build:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel
   ```
4. The executable will be in `build/sil-more`.
5. Run the game:
   ```bash
   ./build/sil-more
   ```

## macOS

### Prerequisites
- Xcode Command Line Tools
- Homebrew (recommended for SDL3)
- CMake

### Building
1. Install Xcode Command Line Tools:
   ```bash
   xcode-select --install
   ```
2. Install Homebrew if not already installed (see https://brew.sh/).
3. Install dependencies:
   ```bash
   brew install cmake sdl3 sdl3-image sdl3-ttf
   ```
4. Navigate to the Sil-More source directory.
5. Configure and build:
   ```bash
   cmake -B build
   cmake --build build --parallel
   ```
6. The executable will be in `build/sil-more`.
7. Run the game:
   ```bash
   ./build/sil-more
   ```

# Steam Deck installation

- Long press power button and enter Desktop Mode.
- Download latest Steam Deck release (from release page .zip file marked as steamdeck).
- You can also download it to the flash drive and then mount in desktop mode (bottom right corner).
- Unzip to any folder (even downloads will work fine).
- Open Steam and in the top menu select games->Add a Non-Steam Game
- Find Sil-More.exe and add it. Change the name to Sil-More in Properties.
- In properties -> compatibilty use current version of Proton.
- (optional) If you want to see artwork in your steam deck menus, Properties -> Customization. We have prepaired a CoverArt folder for you (do not forget to png in filter).
- Open controller setup. You should see the layout on Community layouts or in search. If not, open the link Steam-Deck-layout-Sil-More in game folder, current link is steam://controllerconfig/4068119597/3573052583.
- Return back to Game mode and enjoy Sil-More!  

## macOS

### Prerequisites

Sil-more requires some C compiler (any modern like gcc or clang would work), `ncurses`, and, optionally, `git`. As for the C compiler and `git`, these are likely already installed on your Mac. You can check it. Start Terminal.app (or any other terminal emulator of your choice) and run:

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
git clone https://github.com/k0rtesss/sil-more
```

Or simply download and unpack the ZIP archive from the GitHub page clicking on Code > Download ZIP.

In any case, in terminal, navigate to the folder:

```shell
cd ~/sil-more
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

# Road Map
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
## Release of beta 0.8 (Visual update) (done)

- Quest systems 
  * Tulkas -> kill unique -> get artifact (done) 
  * Quest vault implementation (done) 
  * Mandos -> kill specific -> get ability (done) 
  * Aule -> forge -> get ability (done) 
  * Niena -> spawn -> get ability (done) 
  * Orome (done)
- Update to oath system (done)
  * After completed quest you get oaths (done)
- UI
  * Better oath texts (done)
  * Better oath menu (done)
- New oaths
  * Smith (done)
  * Valor (done)
- Bug fixes (ongoing) 
  * save names (done)
  * new metarun saves (done)
  * rubble (done)
  * speacial monster (done)
## Release of beta 0.87 (Quests and Oaths update) (done) 

- Fullscreen mode for windows (done) 
- Combat rolls logs (done) 
- Help screen update (done) 
- Combat rolls logs (done) 
- Steamdeck keybinds, art, etc (done)
- Bug fixes (ongoing) 
  * OathBreaking (done)
## Release of beta 0.88 (Steam Deck update)

- SDL
## Release of beta 0.9 


### Ideas

- More frequent forges for dwarves
- Calculate the forge probability
- Pride
- Greed
- More Vaults
- More monsters
- Change Score to Character database
- Multiple runs support 
- New Quests
  * Manwe
  * Este
