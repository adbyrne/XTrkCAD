# XTrackCAD 4.3.0 #

This file contains installation instructions and up-to-date information regarding XTrackCad.

## Contents ##

* About XTrackCad
* License Information
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


# Installation #
## Windows ##

XTrackCad has only been tested on Windows 7 and Windows 10. 

The MS-Windows version of XTrackCad is shipped as a self-extracting/
self-installing program using the NSIS Installer from Nullsoft Inc.

Using Windows Explorer, locate the directory in which you downloaded or copied your new version of XTrackCAD.

Start the installation program by double clicking on the 
**[xtrkcad-setup-4.4.0beta1.exe][]** file icon.

Follow the steps in the installation program.

The installation lets you define the directory into which XTrackCAD is 
installed. The directory is created automatically if it doesn't already exist.

A program folder named XTrackCAD 4.4.0beta1 will be created during the installation 
process. This folder contains the program, documentation, parameter and 
example files. An existing installation of earlier versions of XTrackCad is 
not overwritten. 

A new program group named XTrackCad 4.4.0beta1 will be created in the Start menu. 

## Linux ##

XTrackCAD for LINUX is shipped as a RPM file and a self extracting archive.
You will need libc6, X11R6, GTK+2.0, webkitgtk.

### Installing from the RPM package. ### 

Use your operating system's package manager to install XTrackCAD.

### Installing from the self-extracting archive. ###

After downloading open a command line then 

    ./xtrkcad-setup-4.4.0beta1.x86_64.sh --prefix=/usr/local --exclude-subdir

This will install the executable in /usr/local/bin. A directory named 
xtrkcad will be created in /usr/local/share and all files will be unpacked
into it.

If you install XTrackCAD into another directory, set the XTRKCADLIB 
environment variable to point to that directory.

# Release Info #

## Upgrade Information ##

**Note:** This version of XTrackCAD comes with the new cornu feature. In order to support
this feature, the file format for layout files (.xtc) had to be extended.
Files from earlier versions of XTrackCAD can be read without problems. 
Layouts that were saved from this version of the program cannot be read by older 
versions of XTrackCAD. 

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