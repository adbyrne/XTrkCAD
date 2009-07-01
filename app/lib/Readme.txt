This file contains installation instructions and up-to-date information 
regarding XTrackCad.

Contents
========

1. About XTrackCad
2. License Information
3. Installation on Windows
4. Installation on LINUX
5. Upgrading from earlier releases
6. New features in this release
7. New or updated parameter files
8. Where to go for support  

1. About XTrackCad
==================
XTrackCad is a powerful CAD program for designing Model Railroad layouts.

Some highlights:
  - Easy to use.
  - Supports any scale.
  - Supplied with parameter libraries for many popular brands of turnouts,
	plus the capability to define your own.
  - Automatic easement (spiral transition) curve calculation.
  - Extensive help files and video-clip demonstration mode.
  
Availability:
XTrkCad Fork is a project for further development of the original XTrkCad 
software. See the project homepage at http://www.xtrackcad.org/ for news and
current releases.


2. License Information
======================


Copying:

XTrackCad is copyrighted by Dave Bullis and Martin Fischer and licensed as 
free software under the terms of the GNU General Public License which 
you can find in the file COPYING.


3. Installation on Windows
==========================

XTrackCad has only been tested on Windows XP. We no longer support 
Windows 3.1 or Windows 95 anymore. If you still want to run XTrackCad under 
previous versions of Windows, you'll need Version 3 of XTrackCad.

The MS-Windows version of XTrackCad is shipped as a self-extracting/
self-installing program using the NSIS Installer from Nullsoft Inc.

Using Windows Explorer, locate the directory in which you downloaded or 
copied your new version of XTrackCAD.

Start the installation program by double clicking on the 
xtrkcad-setup-4.0.3a.exe file icon.

Follow the steps in the installation program.

The installation lets you define the directory into which XTrackCAD is 
installed. The directory is created automatically if it doesn't already exist.

A program folder named XTrkCAD 4.0.3 will be created during the installation 
process. This folder contains the program, documentation, parameter and 
example files. An existing installation of earlier versions of XTrackCad is 
not overwritten. 

A new program group named XTrkCad 4.0.3a will be created in the Start menu. 

4. Installation on Linux
========================

XTrackCAD for LINUX is shipped as a RPM file and a self-extracting archive (executable). 
You will need libc6, X11R6, GTK+2.0, gtkhtml2.
 
Installing from an RPM file
---------------------------

After downloading the rpm file, do

rpm -ihv xtrkcad-setup-4.0.3a.i386.rpm

Replace -ihv with -Uhv to upgrade an existing XTrackCAD installation.

Installing from the self-extracting archive.
--------------------------------------------

After downloading open a command line then 

./xtrkcad-setup-4.0.3.i386.sh --prefix=/usr/local --exclude-subdir

This will install the executable in /usr/local/bin. A directory named 
xtrkcad will be created in /usr/local/share and all files will be unpacked
into it.

If you install XTrackCAD into another directory, set the XTRKCADLIB 
environment variable to point to that directory.

5. Upgrade Information
======================

No special considerations to be taken.

6. New features in this release
===============================

- Drawing parallels for straight pieces of fixed track is now supported
- Create complete parts list when no track is selected
- Add toolbar icons for New, Load and Save Layout. Enable via View->Toolbar
- Close can now be canceled
- Regrouping of settings in the Display, Command, Preferences dialogs 
- Last layout is loaded on program startup. This behaviour can be controlled 
  via an option in the Options>Preferences dialog.
- Higher precision calculations for room size
- Internationalization Finnish and German are supported (LINUX only)

7. New or updated parameter files
=================================

The following parameter files have been added or updated with this release:

Fast Tracks N-scale Turnouts, Wyes, 3-ways, Crossovers and Slip Switches 
(FastTrack_n.xtp)

8. Where to go for support
==========================

The following web addresses will be helpful for any questions or bug 
reports

The Yahoo!Group mailing list
http://groups.yahoo.com/projects/XTrkCad

The project website for the open source development
http://www.xtrackcad.org/

The official Sourceforge site
http://www.sourceforge.net/groups/xtrkcad-fork/


Thanks for your interest in XTrackCad.
