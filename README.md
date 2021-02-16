# Sil-Q
Sil-Q is a computer role-playing game with a strong emphasis on discovery and
tactical combat. It has a simple but rich combat system which allows for a
great variety of choice.

Sil-Q continues the development of Sil which hasn't been updated for a while.

## Compiling Instructions

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


### Linux or Unix with gcc  (tested with Sil-Q)

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

### Windows with Cygwin   (tested with Sil-Q)

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

### OS X with Xcode  (tested with Sil-Q; Xcode 11.6 on OS X 10.15.5)

1. Get Xcode from the app store if not installed:

   Use App Store to get Xcode, a free set of development tools from Apple.

2. Compile Sil-Q:

   In a Terminal window, change to the src directory and run
   "make -f Makefile.cocoa install".
   Sil-Q should now be compiled and set up as an OS X application, Sil.app,
   in the folder above the src directory.  You may move Sil.app to wherever
   you like.

3. Run Sil-Q:

   In a Finder window, navigate to where you placed Sil.app.  Then double
   click on it to run it.  If you are running 10.15 or later and haven't run
   Sil-Q or Sil before, you'll see a dialog about granting Sil-Q access to
   your Documents folder since it wants to place saved games, the high
   score file, and some other data in Documents/Sil.

