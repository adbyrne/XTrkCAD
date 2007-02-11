This file contains installation instructions and up-to-date information 
regarding XTrkCad.

Contents
========

1. About XTrkCad
2. License Information
3. Installation on Windows
4. Installation on LINUX
5. Upgrading from earlier releases
6. New features in this release
7. New or updated parameter files
8. Where to go for support  

1. About XTrkCad
================
XTrkCad is a powerful CAD program for designing Model Railroad layouts.

Some highlights:
  - Easy to use.
  - Supports any scale.
  - Supplied with parameter libraries for many popular brands of turnouts,
	plus the capability to define your own.
  - Automatic easement (spiral transition) curve calculation.
  - Extensive help files and video-clip demonstration mode.
  
Availability:
XTrkCad Fork is a project for further development of the original XTrkCad 
software. See the project homepage at http://www.xtrkcad.org/ for news and
current releases.


2. License Information
======================


Copying:

XTrkCad is copyrighted by Dave Bullis and licensed as free software under 
the terms of the GNU General Public License which you can find in the file 
COPYING.


3. Installation on Windows
==========================

XTrkCad has only been tested on Windows XP. We no longer support 
Windows 3.1 or Windows 95 anymore. If you still want to run XTrkCad under 
previous versions of Windows, you'll need Version 3 of XTrkCad.

The MS-Windows version of XTrkCad is shipped as a self-extracting/
self-installing program using the NSIS Installer from Nullsoft Inc.

1) From the WinXP Explorer (or the Run command on the Start menu)
   execute XTC401.EXE.
   You will be prompted for a destination directory.  A new program group
   with XTrkCad icons will be created.
2) Open the XTrkCad program by double clicking on the icon and try it.
3) The Unistall icon can be used to uninstall XTrkCad.

By default, the installation progranm creates a new directory for the XTrkCad 
Version 4 files. An existing installation of earlier versions of XTrkCad is not 
overwritten. 
A new program group named XTrkCad 4 will be created in the Start menu. 

4. Installation on Linux
========================

0) You will need libc6, X11R6, GTK+2.0
1) cd /usr/local/lib
2) tar xvfz ${TMPDIR}/xtrkcad-linux-elf.4.0.1.tar.gz
   ; where TMPDIR is where you downloaded XTrkCad
3) ln -s /usr/local/lib/xtrkcad/xtrkcad /usr/local/bin/xtrkcad ; optional
4) you can use xtrkcad/xtc64.xpm as an icon for XTrkCad if supported by
   your window manager
5) Try it out by running: /usr/local/bin/xtrkcad



5. Upgrade Information
======================

Version 4 of XTrkCad introduces a new way of handling checkpoint files. 
This will allow resuming your work after a system crash much easier than 
before. 

If you start XTrkCad V.4 the first time after an upgrade from a earlier 
version, a popup with the question:
"Program was not terminated properly. Do you want to resume working on the 
previous trackplan?"

Select "Ignore" only if you're sure that the work from your last session 
has been correctly saved. If uncertain or restarting after an earlier 
failure select "Resume".


6. New features in this release
===============================

Probably the biggest change in this version of XTrkCad is the new structure
of the main menu. After starting XTrkCad, you'll see the following items in 
the main menu:

File - Open, save, print layouts, parameter files handling
Edit - Select, deselect, cut and paste, edit visual properties of track
View - Zoom in and out, grid operations, refresh screen
Add - Place new track pieces: straight, curved, turnouts etc.
Change - Change existing track: flip, rotate, move, elevation etc.  
Draw - Add basic objects like lines, benchwork, text
Manage - Custom management, turnout designer, run trains
Options - Change settings for the application or the layout
Macro - Record or playback macros
Windows - Select active window, reopen map window if closed
Help - Access the user documentation

All functionality available in earlier versions of XTrkCad can still be 
found under these menu topics. 

The following functions have been added in this version of XTrkCad:

File | Revert - Revert to last saved state of the layout plan.
Edit | Invert selection - invert the selection state of all visible objects
Edit | Select Stranded Track - Select all track pieces that are not 
connected to any other piece of track. May be useful for cleaning up.

NOTES:

1) Check ChangeLog in the installation directory for further change 
   information
2) Look at the XTrkCad help file (either via the Help menu in XTrkCad or by
   opening the XTrkCad help icon).
3) Try the demos, via the Help|Demos menu in XTrkCad.

7. New or updated parameter files
==========================

The following parameter files have been added or updated with this release:

Atlas N Code 55 Track - Dwyane Ward
Design Preservation Model N Structures - Ralph Boyd
Kato N Unitrack - Ralph Boyd
Model Power N Structures - Ralph Boyd
Peco N Code 55 Track - Andrew Crosland
Peco HOn30/OO9 Track - Martin Fischer

8. Where to go for support
==========================

The following web addresses will be helpful for any questions or bug 
reports

The Yahoo!Group mailing list
http://groups.yahoo.com/projects/XTrkCad

The project website for the open source development
http://www.xtrkcad.org/

The official Sourceforge site
http://www.sourceforge.net/groups/xtrkcad-fork/


Thanks for your interest in XTrkCad.