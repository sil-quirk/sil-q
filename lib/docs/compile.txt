================================================================================
Compiling Instructions
================================================================================

Compiling Sil is not very difficult, and has been tested on Macintosh,
Windows and Linux.

If you do compile Sil with a method marked "(untested)" or on a new system,
then send an email to sil@amirrorclear.net and I will update this file with your
method.

The first step is the same on all systems, so do this and then look through
this file for advice on your specific system. 

0. Install the Sil source code:

   Unzip the file "Sil-src.zip". It will become a folder called "Sil"
   which contains subfolders called "lib" and "src". Move it to wherever
   you want to keep it. The src folder contains all the source code
   while the lib folder contains other files that the game uses.
   When you are done compiling, the game will be automatically installed
   in the Sil folder as well.


==========================================================
Linux or Unix with gcc  (tested with Sil)
==========================================================

   There are several different unix setups for Sil:

   X11: Allows multiple windows, has correct colours.
   GCU: Works in a terminal using 'curses', has only 16 or 8 colours.
   CAP: Works even in old terminals, but is monochrome.

1. Mess with the Makefile:

   Edit Makefile.std in the src directory. 
   Look for the section listing multiple "Variations".
   Choose the variation that you like best.
   Remove the # comments from that section's code.
   Comment out the default section.

2. Compile Sil:

   Run "make -f Makefile.std install" in the src directory.

3. Run Sil:

   Go back to the Sil folder and start Sil with "sil".


==========================================================
Mac OS X   (tested with Sil)
==========================================================

1. Compile Sil: 

   Open a new window in Terminal.
   Go to the src directory.
   Run "make -f Makefile.crb install". 
   Sil should now be compiled and installed into the Sil folder.  
   (Alternatively you could try the Sil.xcodeproj file,
   which may also work -- it is what the main developer uses)

2. Run Sil: 

   Go back to the Sil folder and double click on Sil.


==========================================================
Windows with Cygwin   (tested with Sil)
==========================================================

1. Getting the free Cygwin compiler: 

   Download the free Cygwin compiler. It provides a shell interface very
   similar to a normal Unix/Linux shell with many useful tools. Install it
   and start the shell. (Note: for use in windows, the computer's
   autoexec.bat file needs to be edited to include a path to the Cygwin 
   Folder.  Also, the "make" portion of Cygwin is not in the default 
   download.  You have to search it out and specify that the download
   include "make".)

2. Compile Sil: 

   (Note: In windows open up a DOS window and go to the src directory) 
   Go to the src directory and run "make -f Makefile.cyg install". 
   Sil should now be compiled and installed into the Sil folder.  
   The executable file will be called Sil.exe. 

3. Run Sil: 

   Go back to the Sil folder and run Sil.exe. 


==========================================================
Windows with lcc-win32   (untested)
==========================================================

1. Getting the free lcc-win32 compiler: 

   Download the compiler from http://www.cs.virginia.edu/~lcc-win32/
   and install it.

2. Create the LCC project: 

   Start lcc-win32. Select 'File/New/Project...' from the menu and enter 
   "Sil" as the project name. On the "Definition of a new project" set the 
   working directory to "C:...\Sil\src" (wherever you extracted the source 
   code to) and then select the "Windows executable" option. Press the 'Create' 
   button and answer the question about using the wizard to generate the 
   application skeleton with "no". We want to use the existing Sil source 
   code, so we don't need the skeleton code. 

3. Adding the Sil source files to the project: 

   Add all files from birth.c to load.c to the project, then add the
   "main-win.c" file with the "Add new file..." option and finally the files
   from melee1.c to z-virt.c. Select "Add new file..." again, and this time
   select the "Resources" filetype and select the "sil.rc" file. 

4. Setting various project options: 

   Type "winmm.lib" in the 'Additional files to be included in the link' field. 
   In the 'Debuggersettings' you should change the start directory to
   "C:...\Sil" (again use your own path to the Sil directory). The 'Executable
   to start' should be "C:...\Sil\Sil.exe". The settings part is now finished.
   When asked if you want to "Open ... files?" simply say no. 

5. Compiling: 

   Select 'Compiler/Generate Makefile' from the menu and wait for it to finish.
   Now select 'Compiler/Make' and Sil should be compiled. 

6. Run Sil: 

   Go back to the Sil folder and run the Sil.exe.

*. Possible problems:

   When you run Sil, it may abort while initializing its internal files.
   This can happen if the line-endings on the text files in the lib folder are
   wrong. By default, they are set to unix line-endings '\n', but may need to
   be set to DOS line-endings '\r\n'. Either use a tool to do this (there are
   many available) or download a precompiled Windows version and take the lib
   folder from that. The affected files are .txt .hlp & .prf


==========================================================
Windows with lcc-win32 -- using command line (untested)
==========================================================

1. Get the free lcc-win32 compiler:

   Download the compiler from http://www.cs.virginia.edu/~lcc-win32/
   and install it.

2. Adjust the lcc-win32 makefile:

   Go to the Sil src-directory and open "Makefile.lcc" with a
   text editor.  Change the values of 'SIL_PATH' and 'LCC_PATH'
   to the path of your Sil and LCC directories and save the
   Makefile.

3. Compile Sil:

   Run "make -f makefile.lcc" in the Sil src folder.  The game
   should now be compiled and installed into the Sil folder.

4. Run Sil:

   Go back to the Sil folder and run Sil.exe.

*. Possible problems:

   When you run Sil, it may abort while initializing its internal files.
   This can happen if the line-endings on the text files in the lib folder are
   wrong. By default, they are set to unix line-endings '\n', but may need to
   be set to DOS line-endings '\r\n'. Either use a tool to do this (there are
   many available) or download a precompiled Windows version and take the lib
   folder from that. The affected files are .txt .hlp & .prf


==========================================================
Windows with Borland C++ 5.5 commandline tools  (untested)
==========================================================

1. Get the free commandline tools:

   Download the Borland commandline tools from
   http://www.borland.com/bcppbuilder/freecompiler/
   and follow the installation instructions.

2. Compile Sil:

   Open a commandline window and go to the src-directory.
   Run "make -f makefile.bcc install".
   Sil should now be compiled and installed into the Sil folder.

3. Run Sil:

   Go back to the Sil folder and run the Sil.exe.

*. Possible problems:

   Make exits with a "Command line too long" error message:
      Try to add the -l option to the make command.  The make util
      should have the "use long command lines" option enabled by
      default, but some people reported that this is not always
      the case.

   When you run Sil, it may abort while initializing its internal files.
   This can happen if the line-endings on the text files in the lib folder are
   wrong. By default, they are set to unix line-endings '\n', but may need to
   be set to DOS line-endings '\r\n'. Either use a tool to do this (there are
   many available) or download a precompiled Windows version and take the lib
   folder from that. The affected files are .txt .hlp & .prf


==========================================================
DOS + DJGPP  (untested)
==========================================================

1. Install DJGPP:

   You can get the freely available DJGPP C Compiler from 
      http://www.delorie.com/djgpp/
   On the DJGPP-page go to the Zip-Picker
      http://www.delorie.com/djgpp/zip-picker.html
   This page helps you decide which ZIP files you need to download
   and will even try to find a ftp-site near you.
   Get all files and follow the installation instructions.

2. Select the makefile:

   Go to the src-directory and rename 'Makefile.dos' to 'Makefile'.

3. Compile Sil:

   Run "make install".  Sil should now be compiled and installed
   into the Sil folder.

4. Run Sil:

   Go back to the Sil folder and run Sil.exe.

*. Possible problems:

   Make exits with a 'Fatal: Command arguments too long' error message:
      Such problems are usually caused by using the make.exe of another
      compiler (like Borland C++ or cygwin).  DJGPP uses some clever tricks
      to get around the limitations of the MS-DOS "126 characters are enough
      for anybody" commandline that require a specific make tool.  So make
      sure that DJGPP's make.exe is the first (or only) make.exe in your
      path.  See also the DJGPP FAQ entries 16.4, 16.5, and 16.6 for further
      details.

   When you run Sil, it may abort while initializing its internal files.
   This can happen if the line-endings on the text files in the lib folder are
   wrong. By default, they are set to unix line-endings '\n', but may need to
   be set to DOS line-endings '\r\n'. Either use a tool to do this (there are
   many available) or download a precompiled Windows version and take the lib
   folder from that. The affected files are .txt .hlp & .prf


