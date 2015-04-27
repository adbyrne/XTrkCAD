   XTrackCAD 4.2.0

This file contains installation instructions and up-to-date
information regarding XTrackCad.

  Contents

 - About XTrackCad
 - License Information
 - New features in this release
 - Installation
 - Upgrading from earlier releases
 - Bugs fixed
 - Building
 - Where to go for support

  About XTrackCad

XTrackCad is a powerful CAD program for designing Model Railroad
layouts.

Some highlights:

 - Easy to use.
 - Supports any scale.
 - Supplied with parameter libraries for many popular brands of
   turnouts, plus the capability to define your own.
 - Automatic easement (spiral transition) curve calculation.
 - Extensive help files and video-clip demonstration mode.

Availability: XTrkCad Fork is a project for further development
of the original XTrkCad software. See the project
homepage at http://www.xtrackcad.org/ for news and current releases.

  License Information

Copying:

XTrackCad is copyrighted by Dave Bullis and Martin Fischer and
licensed as free software under the terms of the GNU General Public
License v2 which you can find in the file COPYING.

  New features in this release

 - New and updated parameter files and layout examples
 - Apply user preferences for dimensions to elevations
 - Add ability to update color of Text in properties
 - Fix compile problem on FreeBSD
 - Fix Oracle Solaris Studio 12.3 warnings
 - partially completed Brazilian Portuguese translation (57%)
 - Improve German translations
 - Merged webkit help system from Debian
 - Update help CSS to the Wiki's new default look

   Installation

  Windows

XTrackCad has only been tested on Windows 7.

The MS-Windows version of XTrackCad is shipped as a
self-extracting/ self-installing program using the NSIS Installer
from Nullsoft Inc.

Using Windows Explorer, locate the directory in which you downloaded
or copied your new version of XTrackCAD.

Start the installation program by double clicking on the
xtrkcad-setup-4.2.0.exe file icon.

Follow the steps in the installation program.

The installation lets you define the directory into which
XTrackCAD is installed. The directory is created automatically if it
doesn't already exist.

A program folder named XTrackCAD 4.2.0 will be created
during the installation process. This folder contains the program,
documentation, parameter and example files. An existing installation
of earlier versions of XTrackCad is not overwritten.

A new program group named XTrackCad 4.2.0 will be
created in the Start menu.

  Linux

XTrackCAD for LINUX is shipped as a self-extracting archive
(executable). You will need libc6, X11R6, GTK+2.0, webkitgtk.

 Installing from the self-extracting archive.

After downloading open a command line then

./xtrkcad-setup-4.2.0.x86_64.sh --prefix=/usr/local --exclude-subdir

This will install the executable in /usr/local/bin. A directory named
xtrkcad will be created in /usr/local/share and all files will be
unpacked into it.

If you install XTrackCAD into another directory, set
the XTRKCADLIB environment variable to point to that directory.

   Release Info

  Upgrade Information

In order to prevent problems when moving files between different
platforms, the symbol '½' was removed from the scale definitions and
parameter files for narrow gauge definitions. As customary '.5' or
the corresponding gauge in inches eg. 30 is used instead. Layouts
using the old definition can be still be loaded. Use the 'Layout
Parameter' dialog to set the updated definition.

  Bugs fixed

The following bugs have been fixed with this release:

 - Fix I18N on Windows
 - Fix bug 48: created invalid XPMs when many colors were used
 - Windows: associate application icon to xtc files
 - Fixed installation problem on Windows 7 when profile directory
   did not exist
 - Add math library libm to link library list.
 - sscanf extra format string parameter removed
 - Changed the font size used to print XtrackCAD in
   the engineering data box.
 - Update in app/README - correct instructions for Mercurial
   access
 - Fixed bug 3121382 - made menu item and dialog box labeling
   consistent for custom management
 - Fix bug 3310506, 3121372 (partly) - Minimum gauge is persisted,
   gauge is automatically selected in Layout Options
 - Fixed bug 3524218: print scale is shown correctly on print out.
 - Fixed bug 3468014 - build instructions for OSX in README have
   been updated
 - Fixed bug 3535258 - Broken PostScript in German
   locale
 - Fixed bug 3375218 - Odometer Reads A Multiple Of Locos
 - Fixed MSVC compile problem and added missing function to mswlib
 - Updated doxygen configuration file to doxygen version 1.8.2
 - Added code to properly determine the postscript fonts occurring
   in a document.
 - Fixed the syntax of the Document Structure Comments.
 - Circle line tangent/center were interchanged
 - Fix cairo text drawing bugs by forcing painting with frequent
   redraws.
 - Locale prefix change to conform to FHS (tracker bug 3049900)
 - Internationalization support added for help button text.
 - FIX: replaced hard-coded XTRKCAD_LOCALE_DIR path with 'locale'
   based on application library directory (XTRKCAD_LOCALE_DIR is
   defined at makefiles generation time and does not reflect the
   place where the application is actually installed)
 - FIX: now it should work with CMake-2.8.1
 - Get command line parameter handling correct
 - make load last layout option work
 - Pango version check at compile time
 - Block and Switchmotor updates
 - FIX: "Gauge" translation support
 - FIX: application crash due to a double value used as a "%*"
   sprintf. scenario is "Manage" -> "Parts List..." -> "Price"
   (checkbox).
 - Bug fix for setting the minimum radius
 - FIX: hotbar redraw too slow under gtk-quartz
 - FIX: linux still crashed due to a cairo context access after
   its drawable destruction
 - FIX: workaround for OSX with GTK-Quartz -> pixmaps are not
   rendered when using the mask; and replaced gtk_pixmap_new
   deprecated function with gtk_image_new_from_pixmap
 - FIX: crash when displaying a non utf8 string in balloon help
 - FIX: warning removed: pointer targets in passing argument 1 of
   'strcpy' differ in signedness
 - FIX: removed GTK run-time references to /opt/local in root
   directory
 - FIX: removed remained Xlib dependencies and added gtk
   configuration files when generating the OSX bundle
 - FIX: image in about dialog box was not being displayed
 - FIX: deallocate PangoFontDescription
   using the right function
 - FIX: EXC_BAD_ACCESS when displaying about dialog
 - ENH: replace the old font select dialog with the GTK standard
   one, and some code cleanup
 - FIX - text in layout and selection were not aligned
 - New 'About' and new icons
 - Add source for new button icons
 - LINUX Desktop File
 - New application icon
 - Improved support for bitmaps
 - New tip of the day icon
 - Enhanced bitmap display control
 - Improve internationalization support, use simple gettext on
   Win32

   Building

  Overview

The following instructions detail building XTrackCAD
using CMake. CMake is a cross-platform build system, available at
http://www.cmake.org, that can be used to generate builds for a
variety of build tools ranging from "make" to Visual Studio and
XCode. Using CMake you can build XTrackCAD on Windows,
GNU/Linux, and Mac OSX using the build tool(s) of your choice.

 Building XTrackCAD on GNU/Linux

 - Obtain the current sources from Mercurial; I assume that they are
   stored locally at "/src/xtrkcad". Note that the correct URL for
   read-only access to Mercurial is
  
   http://xtrkcad-fork.hg.sourceforge.net:8000/hgroot/xtrkcad-fork/xtrkcad
   
 - Create a separate build directory; for these instructions I
   assume that your build directory is "/build/xtrkcad".
 - Run CMake from the build directory, passing it the path to the
   source directory:

$ cd /build/xtrkcad $ ccmake /src/xtrkcad

 - Press the "c" key to configure the build. After a few moments you
   will see four options to configure: CMAKE_BUILD_TYPE,
   CMAKE_INSTALL_PREFIX, XTRKCAD_USE_GTK, and XTRKCAD_USE_GTK_CAIRO.
 - Use CMAKE_BUILD_TYPE to control the build type. Enter "Debug"
   for a debug build, "Release" for a release build, etc.
 - Use CMAKE_INSTALL_PREFIX to control where the software will be
   installed. For this example, I assume "/install/xtrkcad".
 - Use XTRKCAD_USE_GETTEXT to add new locales (language
   translations). Choose "OFF" to use XTrackCAD's
   default language (English). Refer to
   http://www.xtrkcad.org/Wikka/Internationalization for additional
   information.
 - Use XTRKCAD_USE_GTK to control the user-interface back-end.
   Choose "OFF" for Windows, "ON" for all other platforms.
 - Use XTRKCAD_USE_GTK_CAIRO to enable optional high-quality
   antialiased Cairo rendering for the GTK back-end. This option has
   no effect unless you are using the GTK back-end.
 - Use XTRKCAD_USE_DOXYGEN to enable the production of type,
   function, etc., documentation from the the source code. Requires
   doxygen if enabled. Enable if and only if you intend to hack on
   the code.
 - If you made any changes, press the "c" key again to update your
   new configuration.
 - Once everything is configured to your satisfaction, press the
   "g" key to generate makefiles for your build.
 - Compile XTrkCad using your new build:

$ make

 - Install the new binary:

$ make install

 - Run the installed binary:

$ /install/xtrkcad/bin/xtrkcad

 - If XTRKCAD_USE_DOXYGEN was enabled:

$ make docs-doxygen

to create the internals documentation. Read this documentation by
pointing your web browser at
/build/xtrkcad/docs/doxygen/html/index.html.

 Building XTrackCAD on Mac OSX

 - You will need to install the following dependencies - I recommend
   using http://www.macports.org to
   obtain them: o GTK2 o webkit o gnome-icon-theme
 - Once the prerequisites are installed the build instructions are
   the same as for the GNU/Linux build, above.
 - Remember that to run XTrackCAD on OSX, you need
   to have X11 running with your DISPLAY set.

 Building XTrackCAD on Windows

 - Obtain the current sources from Mercurial; I assume that they are
   stored locally at "c:/src/xtrkcad". Note that the correct URL for
   read-only access to Mercurial is
  
   http://xtrkcad-fork.hg.sourceforge.net:8000/hgroot/xtrkcad-fork/xtrkcad
   
 - Use the Windows Start menu to run CMake.
 - Specify the source and build directories in the CMake window.
   You must provide a build directory outside the source tree - I use
   "c:/build/xtrkcad".
 - Press the "Configure" button to configure the build. You will
   be prompted for the type of build to generate. Choose your desired
   tool - I used "Visual Studio 10". After a few moments you will see
   three options to configure: CMAKE_INSTALL_PREFIX, XTRKCAD_USE_GTK,
   and XTRKCAD_USE_GTK_CAIRO.
 - Use CMAKE_INSTALL_PREFIX to control where the software will be
   installed. The default "c:/Program Files/XTrkCAD" is a good
   choice.
 - Use XTRKCAD_USE_GETTEXT to add new locales (language
   translations). Choose "OFF" to use XTrackCAD's
   default language (English). Refer to
   http://www.xtrkcad.org/Wikka/Internationalization for additional
   information.
 - Use XTRKCAD_USE_GTK to control the user-interface back-end.
   Choose "OFF" for Windows.
 - Use XTRKCAD_USE_GTK_CAIRO to enable optional high-quality
   antialiased Cairo rendering for the GTK back-end. This option has
   no effect unless you are using the GTK back-end.
 - Use XTRKCAD_USE_DOXYGEN to enable the production of type,
   function, etc., documentation from the the source code. Requires
   doxygen if enabled. Enable if and only if you intend to hack on
   the code.
 - If you made any changes, press the "Configure" button again to
   update your new configuration.
 - Once everything is configured to your satisfaction, press the
   "OK" button to generate project files for your build.
 - Compile XTrackCad using the new project files.
   For example, start MSVC and open the "XTrkCAD.sln" solution file
   which is located in your build directory.
 - Build the "BUILD_ALL" project to build the software.
 - Build the "INSTALL" project to install the software.
 - Run XTrackCAD by double-clicking its icon located
   in the install directory - for example: c:/Program
   Files/XTrkCAD/bin/xtrkcad.exe.

  Where to go for support

The following web addresses will be helpful for any questions or bug
reports

 - The Yahoo!Group mailing list
 - The project website for the open source development
 - The official Sourceforge site

Thanks for your interest in XTrackCAD.