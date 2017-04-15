# XTrackCAD 4.3.0 #

This file contains installation instructions and up-to-date information regarding XTrackCad.

## Contents ##

* About XTrackCad
* License Information
* New features in this release
* Installation 
* Upgrading from earlier releases
* Bugs fixed
* Building
* Where to go for support  

## About XTrackCad ##

XTrackCad is a powerful CAD program for designing Model Railroad layouts.

Some highlights:

* Easy to use.
* Supports any scale.
* Supplied with parameter libraries for many popular brands of turnouts, plus the capability to define your own.
* Automatic easement (spiral transition) curve calculation.
* Extensive help files and video-clip demonstration mode.
  
Availability:
XTrkCad Fork is a project for further development of the original XTrkCad 
software. See the project homepage at <http://www.xtrackcad.org/> for news and current releases.

## License Information ##

**Copying:**

XTrackCad is copyrighted by Dave Bullis and Martin Fischer and licensed as 
free software under the terms of the GNU General Public License v2 which 
you can find in the file COPYING.

## New features ##

### Version 4.3.0 ###

* Linux: Install xtrkcad.desktop and xtrkcad.png for desktop integration
* All: Fix color button initialization
* Add control and sensor types
* Development of signal feature was finished
* Add extra AppleHelp files
* All: Add documentation for layout control elements
* Write proper Describe, Move, and Rotate functions for Signals.
* Add high lighting for signals.
* Add AppleHelp bundle generation including index
* First pass at high lighting layout control elements.
* Linux/OSX: Set directory for rc file
* All: Rework DXF export, add support for metric measurements
* All: Add dynstring to build and modify DynstrMalloc interface
* All: Add dynstring library and unit tests
* All: enable unit testing with CMocka
* Add Layout Control Element management dialog

### Version 4.2.4 ###
* All: Parameter file for Minitrains HOe-009-HOn30 Track System
* All: Improved and new parameter files Kato N and Super O
* All: Corrected parameter file for Peco HOn30/OO9 track
* All: New parameter file for Peco HOn3 turnouts
* All: New and updated parameter files for Z-scale
* All: added a few more length formats
* All: added additional zoom and marco zoom steps 
* All: Increase meximum number of layer buttons to 99
* All: The parameter file dialog now allows loading and unloading several files together

### Version 4.2.3 ###
* All: updated German translations
* All: TT Kuehn added items
* Linux/OSX: New printing system
* All: tomix-n.xtp Added or Updated multiple items
* All: Add length format with six decimal places for English units (feature wish #33)
* All: Feature Request #35: add hotkey for switching map window on and off
* All: Make the installation's param directory the default on initial run
* All: several new and updated parameter files
* All: add Nm gauge and some Nm track parameters

### Version 4.2.2 ###
* All: Update German translations
* All: extended parameter files f𲠋ato HO and Walthers N structures
* All: Added and extended parameter files
* All: Add support for HOf scale/gauge and Busch track parameter files
* Linux/OSX: Replace file selector dialog with newer file chooser dialog
* All: Make editing of custom track work
* All: Fix Atlas HO Code 100 parameter file
* All: new Kato Unitrack N-Scale parameter file
* All: add measurement units to train speed

### Version 4.2.1 ###
* Increase the number of layer buttons to 99
* All: Update Eishindo T Gauge parameter file
* All: Paste clipboard content at position of mouse pointer
* All: Never mark curve centers of turnouts
* All: Make drawing of center markings (crosshair) an user option
* add ability to set text color when creating them
* Windows: Draw and print crosshair to mark the center of an arc
* Add the ability to change layers of a piece of track in the describe box.

### Version 4.2.0 ###
* New and updated parameter files and layout examples
* Apply user preferences for dimensions to elevations
* Add ability to update color of Text in properties
* Fix compile problem on FreeBSD
* Fix Oracle Solaris Studio 12.3 warnings
* partially completed Brazilian Portuguese translation (57%)
* Improve German translations 
* Merged webkit help system from Debian
* Update help CSS to the Wiki's new default look

## Bugs fixed ##

### Version 4.3.0 ###
* All: Save and set user defined values for Text command
* All: Fix path handling error after Save/Save As
* All: Fix color button initialization
* Fix Malloc error (wrong size in ReadBlock, causing Guard1 hose error)
* Updated cblock.c and cswitchmotor.c to handle possible broken blocks and switchmotors and not crash (check for NULL pointers)
* Bulletproof against undefined (NULL pointer) track segments in SwitchMotor code.
* Bulletproof against undefined (NULL pointer) track segments in Block code.
* Linux/OSX: Preparing for GTK3 migration, second set of changes
* Clean up help files for Apple Help Indexer
* Linux/OSX: Preparing for GTK3 migration, first set of changes
* All: New and updated parameter files for Lego and Peco H0 Code 83
* Linux/OSX: Add OSX specific files
* Linux/OSX: Split help functions in generic and platform specific piece
* Description: Correct some translation typos
* Linux/OSX: Fix #163 GCC format-security errors
* All: Update parameter file for Kuehn TT track
* Add forward reference resolution for Blocks and Switchmotors
* Cleanly delete blocks and switch motors when their tracks are deleted (avoid dangling pointers)
* Updated Spec file
* Expand help path name for longer bundle directories
* Attempt to fix help files crash issue
* Add GLIB Schemas to GTK-Bundle
* All: Improved German translations
* Update Readme for OSX package
* Add support for gtk-mac-bundler
* fix bugs in xtrkcad-bundler code
* Fix parm dialog default for GTK builds
* Add a post-build script for auto-creating OSX images
* Remove failing copy command for gtk-bundle share
* Linux/OSX: Fix uninitialized param file directory on first run
* All: Add TTi scale / gauge
* All: New paramter files for Frateschi HO and Auhagen TTi

### Version 4.2.4a ###
* All: Update help
* Linux: Installation from self extracting archive accepts install path
* Linux/OSX: Remove absolute path from source packages
* All: remove all dependencies on obsolete XTRKCAD_USE_LAYOUTCONTROL conditional
* All: New parameter file for Micro Engineering HOn3 Turnouts
* Windows: Fix bug 157 Crash on color change when drawing lines
* All: Updated parameter file for Kato N scale
* All: Fix build problem with block and switchmotor feature

### Version 4.2.4 ###
* Linux/OSX: Correct search order for config files
* All: Layout control functions are always included
* Linux/OSX: add a default file extension when none is present on save
* Linux/OSX: Update package build
* All: Fix compiler warnings for pointer to in casts on 64 bit systems
* All: Correct file comments
* All: Consistent spelling
* All: Fix definition for Atlas Code 83 3/4" straight track

### Version 4.2.3b ###
* Windows: Fix numeric overflow when reading layouts created on Linux
* Linux/OSX: Fix crash when closing Train Control window
* Linux/OSX: Fix crash when closing Change Elevation Window
* All: new and improved parameter files for Tillig track

### Version 4.2.3a ###
* Windows: Make UI translation work with directory structure of 64 bit Windows
* Linux/OSX: Use defaults for printer and page settings on first run

### Version 4.2.3 ###
* All: Fix bug #143: Roco N Turnout 22247
* All: fix invalid const variable usage
* Linux/OSX: cairo is required and no longer optional
* All: Fix bug #137 remove accelerator keys from block and switchmotor functions
* All: Fixed wrapping of messages in status line
* All: fix parameter files for On30
* All: fix some compiler warnings
* All: Fix bug #137 ie. crash on in intial run
* All: fix possible signed / unsigned char problems
* Windows: fix some compiler warnings

### Version 4.2.2 ###
* All: proper initialization of gauge on initial run
* All: Added missing Language code header in Finnish and Brazilian Portuguese translations
* Linux/MacOSX Fix for string conversion issues found using -Wformat-security default compiler flag on modern Linux distros
* All: Bugfix: enable changing the layer of drawing elements from Describe dialog
* All: Fix formatting of parts list, use monospace font on Linux and OSX
* All: fix buffer overflow bug on overly long title lines (bug 120)
* All: Fix memory violation bug on initial run of XTrackCAD
* All: help and message fixes, load example directories on first start
* All: Fix cmake backwards compatibility
* All: Fix Backspace-Handling when field is emptied completly
* All: Suppress warning from CMake versions  3.0
* Linux: Additional files needed for Debian packages

### Version 4.2.1 ###
* All: Fix problem with blank line in American prototype file
* All: fix one occurence if undo assert bug
* All: Fix setting and getting minimum radius in Layout Options dialog
* Windows: add round() missing in older Visual Studio versions
* All: Fix locale problem with Export/Import
* Linux/OSX: fix line width when printing
* Linux/OSX: Add text rotation to gtk/cairo
* Windows: Fix text rotation from Describe dialog
* Fix initial HotBar Text Issue
* Fix shift-modify abend
* Fix redraw on Undo and Redo
* All: fix bug 23 - make line width independent of zoom factor a creation time
* Remove Ruler Text when selecting ruler button
* Windows: Correct size calculation for radio buttons and checkboxes
* Fix ghosts after delete
* Fix modify
* Fix ghost tracks and text on mac
* Fix color stack protection bug
* Windows Fix Bug 108: make sure that the line width is at least 1
* ALL Fix bug 110: Proper error message on invalid scale in parameter file
* Linux: Add valid ChangeLog to the RPM package
* fix layerlist
* Linux: Make RPM package generator work
* Linux/OSX Draw and print crosshair to mark center of arc
* ALL: Complete German translation for UI, messages and demos
* Linux/OSX Fix bug 103:  Icons are unreadable
* Linux/OSX Fix bug 101: Print command fails with path+name > 42 chars
* fix kato-n parameter file
* Change the default input to be the same as the default output.
* Correction to parameter file, Marklin 5119 is a Right not a Left turnout

### Version 4.2.0 ###
* Fix I18N on Windows
* Fix bug 48: created invalid XPMs when many colors were used
* Windows: associate application icon to xtc files
* Fixed installation problem on Windows 7 when profile directory did not exist
* Add math library libm to link library list.
* sscanf extra format string parameter removed
* Changed the font size used to print XtrackCAD in the engineering data box.
* Update in app/README - correct instructions for Mercurial access
* Fixed bug 3121382 - made menu item and dialog box labeling consistent for custom management
* Fix bug 3310506, 3121372 (partly) - Minimum gauge is persisted, gauge is automatically selected in Layout Options
* Fixed bug 3524218: print scale is shown correctly on print out.
* Fixed bug 3468014 -  build instructions for OSX in README have been updated
* Fixed bug 3535258 - Broken PostScript in German locale
* Fixed bug 3375218 - Odometer Reads A Multiple Of Locos
* Fixed MSVC compile problem and added missing function to mswlib
* Updated doxygen configuration file to doxygen version 1.8.2
* Added code to properly determine the postscript fonts occurring in a document.
* Fixed the syntax of the Document Structure Comments.
* Circle line tangent/center were interchanged
* Fix cairo text drawing bugs by forcing painting with frequent redraws.
* Locale prefix change to conform to FHS (tracker bug 3049900)
* Internationalization support added for help button text.
* FIX: replaced hard-coded XTRKCAD_LOCALE_DIR path with 'locale' based on application library directory (XTRKCAD_LOCALE_DIR is defined at makefiles generation time and does not reflect the place where the application is actually installed)
* FIX: now it should work with CMake-2.8.1
* Get command line parameter handling correct
* make load last layout option work
* Pango version check at compile time
* Block and Switchmotor updates
* FIX: "Gauge" translation support
* FIX: application crash due to a double value used as a "%*" sprintf. scenario is "Manage" -> "Parts List..." -> "Price" (checkbox).
* Bug fix for setting the minimum radius
* FIX: hotbar redraw too slow under gtk-quartz
* FIX: linux still crashed due to a cairo context access after its drawable destruction
* FIX: workaround for OSX with GTK-Quartz -> pixmaps are not rendered when using the mask; and replaced gtk_pixmap_new deprecated function with gtk_image_new_from_pixmap
* FIX: crash when displaying a non utf8 string in balloon help
* FIX: warning removed: pointer targets in passing argument 1 of 'strcpy' differ in signedness
* FIX: removed GTK run-time references to /opt/local in root directory
* FIX: removed remained Xlib dependencies and added gtk configuration files when generating the OSX bundle
* FIX: image in about dialog box was not being displayed
* FIX: deallocate PangoFontDescription using the right function
* FIX: EXC_BAD_ACCESS when displaying about dialog
* ENH: replace the old font select dialog with the GTK standard one, and some code cleanup
* FIX - text in layout and selection were not aligned
* New 'About' and new icons
* Add source for new button icons
* LINUX Desktop File
* New application icon
* Improved support for bitmaps
* New tip of the day icon
* Enhanced bitmap display control
* Improve internationalization support, use simple gettext on Win32

# Installation #
## Windows ##

XTrackCad has only been tested on Windows 7. 

The MS-Windows version of XTrackCad is shipped as a self-extracting/
self-installing program using the NSIS Installer from Nullsoft Inc.

Using Windows Explorer, locate the directory in which you downloaded or copied your new version of XTrackCAD.

Start the installation program by double clicking on the 
**[xtrkcad-setup-4.3.0.exe][]** file icon.

Follow the steps in the installation program.

The installation lets you define the directory into which XTrackCAD is 
installed. The directory is created automatically if it doesn't already exist.

A program folder named XTrackCAD 4.3.0 will be created during the installation 
process. This folder contains the program, documentation, parameter and 
example files. An existing installation of earlier versions of XTrackCad is 
not overwritten. 

A new program group named XTrackCad 4.3.0 will be created in the Start menu. 

## Linux ##

XTrackCAD for LINUX is shipped as a RPM file and a self extracting archive.
You will need libc6, X11R6, GTK+2.0, webkitgtk.

### Installing from the RPM package. ### 

Use your operating system's package manager to install XTrackCAD.

### Installing from the self-extracting archive. ###

After downloading open a command line then 

    ./xtrkcad-setup-4.3.0.x86_64.sh --prefix=/usr/local --exclude-subdir

This will install the executable in /usr/local/bin. A directory named 
xtrkcad will be created in /usr/local/share and all files will be unpacked
into it.

If you install XTrackCAD into another directory, set the XTRKCADLIB 
environment variable to point to that directory.

# Release Info #

## Upgrade Information ##

The available options for number formats have been extended. Check your setting 
in Options>Preferences

# Building #
## Overview ##

The following instructions detail building XTrackCAD using CMake. CMake is a
cross-platform build system, available at http://www.cmake.org, that can be
used to generate builds for a variety of build tools ranging from "make" to
Visual Studio and XCode. Using CMake you can build XTrackCAD on Windows,
GNU/Linux, and Mac OSX using the build tool(s) of your choice.

### Building XTrackCAD on GNU/Linux ###

* Obtain the current sources from Mercurial; I assume that they are stored locally at
  "~/src/xtrkcad".
  Note that the correct URL for read-only access to Mercurial is
  <http://xtrkcad-fork.hg.sourceforge.net:8000/hgroot/xtrkcad-fork/xtrkcad>
* Create a separate build directory; for these instructions I assume that
  your build directory is "~/build/xtrkcad".
* Run CMake from the build directory, passing it the path to the source
  directory:

    $ cd ~/build/xtrkcad
    $ ccmake ~/src/xtrkcad

* Press the "c" key to configure the build. After a few moments you will see
  four options to configure: CMAKE_BUILD_TYPE, CMAKE_INSTALL_PREFIX,
  XTRKCAD_USE_GTK, and XTRKCAD_USE_GTK_CAIRO.
* Use CMAKE_BUILD_TYPE to control the build type. Enter "Debug" for a debug
  build, "Release" for a release build, etc.
* Use CMAKE_INSTALL_PREFIX to control where the software will be installed.
  For this example, I assume "~/install/xtrkcad".
* Use XTRKCAD_USE_GETTEXT to add new locales (language translations). Choose
  "OFF" to use XTrackCAD's default language (English). Refer to
  http://www.xtrkcad.org/Wikka/Internationalization for additional information.
* Use XTRKCAD_USE_GTK to control the user-interface back-end. Choose "OFF"
  for Windows, "ON" for all other platforms.
* Use XTRKCAD_USE_GTK_CAIRO to enable optional high-quality antialiased
  Cairo rendering for the GTK back-end. This option has no effect unless you are
  using the GTK back-end.
* Use XTRKCAD_USE_DOXYGEN to enable the production of type, function, etc.,
  documentation from the the source code. Requires doxygen if enabled.
  Enable if and only if you intend to hack on the code. 
* If you made any changes, press the "c" key again to update your new
  configuration.
* Once everything is configured to your satisfaction, press the "g" key to
  generate makefiles for your build.
* Compile XTrkCad using your new build:

    $ make

* Install the new binary:

    $ make install

* Run the installed binary:

    $ ~/install/xtrkcad/bin/xtrkcad

* If XTRKCAD_USE_DOXYGEN was enabled:

    $ make docs-doxygen
  
  to create the internals documentation. Read this documentation by pointing 
  your web browser at ~/build/xtrkcad/docs/doxygen/html/index.html.

### Building XTrackCAD on Mac OSX ###

* You will need to install the following dependencies - I recommend using
  <http://www.macports.org> to obtain them:
  - GTK2
  - webkit
  - gnome-icon-theme 
* Once the prerequisites are installed the build instructions are the same
  as for the GNU/Linux build, above.
* Remember that to run XTrackCAD on OSX, you need to have X11 running with
  your DISPLAY set.

### Building XTrackCAD on Windows ###

* Obtain the current sources from Mercurial; I assume that they are stored locally at
  "c:/src/xtrkcad".
  Note that the correct URL for read-only access to Mercurial is
  <http://xtrkcad-fork.hg.sourceforge.net:8000/hgroot/xtrkcad-fork/xtrkcad>
* Use the Windows Start menu to run CMake (cmake-gui).
* Specify the source and build directories in the CMake window. You must
  provide a build directory outside the source tree - I use "c:/build/xtrkcad".
* Press the "Configure" button to configure the build. You will be prompted
  for the type of build to generate. Choose your desired tool - I used "Visual
  Studio 10". After a few moments you will see three options to
  configure: CMAKE_INSTALL_PREFIX, XTRKCAD_USE_GTK, and XTRKCAD_USE_GTK_CAIRO.
* Use CMAKE_INSTALL_PREFIX to control where the software will be installed.
  The default "c:/Program Files/XTrkCAD" is a good choice.
* Use XTRKCAD_USE_GETTEXT to add new locales (language translations). Choose
  "OFF" to use XTrackCAD's default language (English). Refer to
  <http://www.xtrkcad.org/Wikka/Internationalization> for additional information.
* Use XTRKCAD_USE_GTK to control the user-interface back-end. Choose "OFF"
  for Windows.
* Use XTRKCAD_USE_GTK_CAIRO to enable optional high-quality antialiased
  Cairo rendering for the GTK back-end. This option has no effect unless on
  Windows.
* Use XTRKCAD_USE_DOXYGEN to enable the production of type, function, etc.,
  documentation from the the source code. Requires doxygen if enabled.
  Enable if and only if you intend to hack on the code. 
* If you made any changes, press the "Configure" button again to update your
  new configuration.
* Once everything is configured to your satisfaction, press the "OK" button
  to generate project files for your build.
* Compile XTrackCad using the new project files. For example, start MSVC and
  open the "XTrkCAD.sln" solution file which is located in your build directory.
* Build the "BUILD_ALL" project to build the software.
* Build the "INSTALL" project to install the software.
* Run XTrackCAD by double-clicking its icon located in the install directory -
  for example: c:/Program Files/XTrkCAD/bin/xtrkcad.exe.

## Where to go for support ##

The following web addresses will be helpful for any questions or bug 
reports

- The Yahoo!Group mailing list <http://groups.yahoo.com/projects/XTrkCad>
- The project website for the open source development <http://www.xtrackcad.org/>
- The official Sourceforge site <http://www.sourceforge.net/groups/xtrkcad-fork/>

Thanks for your interest in XTrackCAD.