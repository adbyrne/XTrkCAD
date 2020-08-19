# Beta 2.1 Release Notes

Welcome to the XtrackCAD V5.2.0 Beta 2.1 release! 

Beta 2.1 has very few functional enhancements and quite a lot of bug fixes since Beta 2.0. 

The V5.2 release started out as just some simple functional enhancements of long-standing, like background images. The idea was to punt on all UI changes to the V6 GTK3 release.  But along the way and due to some sabaticals for developers, things kept getting added and tinkered with. Finally the major UI enhancements you will see were mapped out over the last six months and so we have an incremental enhacement to the UI as well.

Enjoy!

Martin and Dave and Adam, your volunteer developers.

PS The full change log is a file in the XtrkCAD download folder as CHANGELOG.md 

## Warning to Users

Please note - this software DOES contain bugs. Do not use it if that fact will bother you. All software contains bugs, of course, but beta software is our chance to find and remove as many of them as possible before unleashing it for all users. You, our beta test partners in this endeavor, are helping by exposing yourself to the risk of failure and then letting us know how it went. This is vital, you need to let us know when things are not happening as you would expect, or if they are (so we know someone is testing).

Take backups! Please. The files written by XTrackCAD 5.2.0 are versioned to only be read by 5.2.0, but it can also read files from earlier versions. If you get into trouble, please reach out, we may be able to help - but not if you didn't back-up.

We will fix important bugs you find by issuing perodic Beta updates. We may not change the code if what you would expect is impossible or would break what we are trying to achieve overall, but we promise that we will consider all your input as carefully as we can. Obviously if the program fails in some way, let us know. But also let us know when you can't work things out, or the way things seem to work seems irrational.  Exercise the Documentation as well (read the Help manual when you get stuck - there's a handy F1 shortcut to the pages for the current function now...)

To report bugs, please use the SourceForge bugs reporting page https://sourceforge.net/p/xtrkcad-fork/bugs/

To discuss the Beta, please use the user forum https://xtrackcad.groups.io/g/main/topics

# Notes on V5.2 UI Philosphy 

XtrackCAD is a veteran product. Conceived in a world of UNIX Vector Graphic UIs, and then ported to Windows as well, the way it worked was optimized for a hardware era where full redrawing the screen was a painful operation. Moving objects around required simplification for drawing and a overwrite approach. The chic UI style at the time was very modal - you picked a tool from a toolbar and then used it by selecting a target - Think Photoshop. Each command/tool was a world unto itself with different rules to master. Learning the program required a lot of practice, but rewarded you with lots of short-cuts and functions that required a sequence of actions. 

Now we are in 2020, our hardware is much more powerful even if we have a machine a few years old. We are used to more responsive and immersive UIs that try to anticipate the user and guide them. Many UIs have a more "pick an object and then take various actions" style. Partly out of necessity XTrackCAD will look familar to its loyal users and largely work the same way for them, but a major focus for the UI improvements has been to try and bridge the semantic gap towards commonly used consumer programs of today to smooth the on-ramp experience. Predictability is key - can I understand what will happen before doing it, and so learn by exploring? Am I able to navigate between common functions more easily, and do objects I see behave in ways that I might expect?

We combed through the user suggestions, thought back to our own first experiences, did surveys of other graphical design products and asked users directly about choices we could make. We also looked at all the feedback we could find online (good or bad) from railway/railroad modellers. This led us to understand that new users thought of XTrackCAD like a set of tracks waiting to be assembled and they were confused by simple things - *why do I have to select turnouts differently from flexible track*?  *Why is the program so fussy about how track lines up - shouldn't it just join*? *Why do some of my commands affect objects I can't even see on screen*? Meanwhile power users had other concerns. *Why can't I draw objects which are accurate like in a "real" CAD*? *Why can't I use Cornu for more than joining*? *Why are the parameter files so hard to pick between*?

There are several things we did not do in the UI because they would have been too complex on a divided Windows/GTK GUI base - those will have to wait for V6. But overall we hope that new users will find V5.2 more accessible, while power users will discover enhanced features that they can use (even ones that have always existed but were well hidden).

Our criteria then is this -> to have to use fewer clicks to get common tasks done, to make those remaining actions more accurate and with fewer side-effects. We want to support easy "snap it together" tracklaying as well as high-precision drawing creation. To allow for better annotation and tracing of real locations, to encourage more production of reusable parts by making finding them easier. Although a modern "look" will await the customization of V6, we wanted to do the best we can with the "classic" UI framework we have inherited standing on the shoulders of the giants who preceeded us.

Adam R

PS If you can't "track" with the new Select method, you can get back "Classic" Select with -

- Options->Command->Select Add and uncheck Select None
- Toggle Magnetic Snap Off

# Install Notes

## Mac OSX

Mac OSX installs an app package which you drag into the Applications folder.  In order to run it, you need to have installed XQuartz from http://www.xquartz.org first. Please remember to log on and off after installing XQuartz for the first time.

On Mojave and earlier, the program may not work on first run after trying to run it - Go to Preferences->Security and Select "Run Anyway". 

On Catalina, XTrackCAD will not work by double-clicking the app - You have to right-select it in Finder inside the Applications folder and select "Open". The app loses access to certain high-risk/high-security folders including Downloads, Documents and Music folders - if you have material you need to use in those areas, copy them into other folders.

## Windows

XTrackCAD v5.2 is a 32 bit application. To suport new features the package comes with three DLLs. Two of them (zlib and zip) require vcruntime140.dll, which is not included in the package. So you'll have to get a 32bit version of that DLL and install it in your path.

Try https://www.microsoft.com/en-US/download/details.aspx?id=48145

## Linux

Installing the Debian package (xtrkcad-setup-5.2.0Beta2.0-1.x86_64.deb) or the RPM package (xtrkcad-setup-5.2.0Beta2.0-1.x86_64.rpm) will install in /usr/local/bin/ and /usr/local/share/.
Super-user access will be required to install this.
Be aware that you don't invoke any other existing *xtrkcad* installation when running the program.

The debian package is new (and experimental).

Installing the shell archive xtrkcad-setup-5.2.0Beta2.0-1.x86_64.sh will install in the current directory.  You will be given the choice of whether to install the bin/ and share/ directories in the current directory or in a subdirectory (xtrkcad-setup-5.2.0Beta2.0-1.x86_64/).
You must set the environment variable XTRKCADLIB to the location of the share/xtrkcad directory:
- *export XTRKCADLIB=`pwd`/share/xtrkcad* or
- *export XTRKCADLIB=`pwd`/xtrkcad-setup-5.2.0Beta2.0-1.x86_64/share/xtrkcad*

To run *xtrkcad* you will need to run it from a terminal window.  For the shell archive, the installed bin/ directory must be in your path or the path to the bin/ directory must be specified.
We're working on integrating *xtrkcad* with the menu system.

*webkitgtk* is not used for displaying help.  Instead your system browser will be used.

======================================================================================================

# XTrackCAD 5.2.0 Version Notes#

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

## Dependencies ##

XTrackCAD starting with V5.2 depends on some external libraries:

+ zlib Compression Library https://www.zlib.net/
+ libzip for handling zip files https://libzip.org/

Additionally on Windows only

+ FreeImage image handling http://freeimage.sourceforge.net/

The Windows, Mac and Linux installerer come with these libraries so no additional downloads
are necessary. 

If building from source or not using the packaging manager, the necessary libraries will need to
be installed using the software installation tools of the 
operating system.

## Windows ##

XTrackCad has only been tested on Windows 10.

The MS-Windows version of XTrackCad is shipped as a self-extracting/
self-installing program using the NSIS Installer from Nullsoft Inc.

Using Windows Explorer, locate the directory in which you downloaded or copied your new version of XTrackCAD.

Start the installation program by double clicking on the
**[xtrkcad-setup-5.2.0.exe][]** file icon.

Follow the steps in the installation program.

The installation lets you define the directory into which XTrackCAD is
installed. The directory is created automatically if it doesn't already exist.

A program folder named XTrackCAD 5 will be created during the installation
process. This folder contains the program, documentation, parameter and
example files. An existing installation of earlier versions of XTrackCad is
not overwritten.

A new program group named XTrackCAD 5 will be created in the Start menu.

## OSX ##

XTrackCAD for Mac is shipped as a self-installing OSX package
Start the install by double clicking on the 
**[xtrkcad-osx-5.2.0.dmg][]** file icon.

Drag the package and drop into the Applications folder. 

You may receive a prompt telling you that the package is not signed. To install it anyway, go to the System Preferences page and select Security & Privacy. Hit the button marked "Install Anyway".

If you have a previous version you will be asked if you want to replace it or install a second version. 

You will need to have installed the correct level of XQuartz for your level of OSX to run XTrackCAD  on Mac - go to http://www.xquartz.org/ and download and then install the package. Remember to log out and in again (or reboot) if this your first xQuartz install.

Once the XQuartz package has installed go to the XtrkCAD icon in Applications and double click on it. You may again be told the program is not signed. If so, again go to Systems Preferences->Security & Privacy and hit "Run Anyway".

On Catalina, you will need to start the program by Right-clicking on the icon and selecting "Open".

## Linux ##

XTrackCAD for LINUX is shipped as a RPM file and a self extracting archive.

If you change the install package you should set the XTRKCADLIB enviroment variable 

For example if the install is within the /usr/local/share/xtrkcad directory. you could use -

env XTRKCADLIB="/usr/local/share/xtrkcad/" xtrkcad

### Installing from the RPM package. ###

Use your operating system's package manager to install XTrackCAD.


### Installing from the self-extracting archive. ###

After downloading open a command line then as root run

    ./xtrkcad-setup-5.2.0.x86_64.sh --prefix=/usr/local --exclude-subdir

This will install the executable in /usr/local/bin. A directory named
xtrkcad will be created in /usr/local/share and all files will be unpacked
into it.

If you install XTrackCAD into another directory, set the XTRKCADLIB
environment variable to point to that directory.

# Release Info #

## Upgrade Information ##

**Note:** This version of XTrackCAD comes with the several new features
like backgroudn images or extensions to notes. In order to support
this feature, an additional file format for layout files (.xtce) was added. 
The old .xtc format is still supported for reading and writing. So
files from earlier versions of XTrackCAD can be read without problems.
Layouts that were saved in the new format cannot be read by older
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
* Move to the V5.2 branch using "hg update V5.2"
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
  - gnome-icon-theme
  - ziplib
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
  Studio 15". After a few moments you will see two options to
  configure: CMAKE_INSTALL_PREFIX and XTRKCAD_USE_GTK.
* Use CMAKE_INSTALL_PREFIX to control where the software will be installed.
  The default "c:/Program Files/XTrkCAD" is a good choice.
* Use XTRKCAD_USE_GETTEXT to add new locales (language translations). Choose
  "OFF" to use XTrackCAD's default language (English). Refer to
  <http://www.xtrkcad.org/Wikka/Internationalization> for additional information.
* Use XTRKCAD_USE_GTK to control the user-interface back-end. Choose "OFF"
  for Windows.
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
 
