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
   execute XTC402.EXE.
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

0) You will need libc6, X11R6, GTK+2.0, gtkhtml2
1) cd /usr/local/lib
2) tar xvfz ${TMPDIR}/xtrkcad-linux-elf.4.0.1.tar.gz
   ; where TMPDIR is where you downloaded XTrkCad
3) ln -s /usr/local/lib/xtrkcad/xtrkcad /usr/local/bin/xtrkcad ; optional
4) you can use xtrkcad/xtc64.xpm as an icon for XTrkCad if supported by
   your window manager
5) Try it out by running: /usr/local/bin/xtrkcad



5. Upgrade Information
======================

5.1 Windows Version
-------------------

The main initialization file xtrkcad.ini is no longer stored in the Windows
directory but in the user's settings. During the installation the file is 
copied to the new location so your settings remain intact.

6. New features in this release
===============================

- Program Help files have been extensively reworked and reformatted. The help 
system has been converted to HTML format. 
- Zooming in and out from a drawing can now be controlled with a wheel mouse.
- Colors and the flag settings for the layers can now	be saved in the 
preferences. On opening a new layout or upon	startup of XTrkCad these settings 
are automatically loaded.
- XTrkCad now has a real splash window during startup


7. New or updated parameter files
==========================

The following parameter files have been added or updated with this release:

atlaso2rail.xtp
br.xtp               British Rolling Stock
eu.xtp
atl100ho.xtp			Atlas HO Code 100
rocho100.xtp			Roco HO Code 
AtlasTrueTrk.xtp		Atlas HO True Track
nmra-0-lapped.xtp		Lapped O scale turnouts
LionelFasttrack.xtp	Lionel Fasttrack
amb-n.xtp				American Model Builders N Structures
dpm-ho.xtp           Design Preservation Model HO Structures
dpm-n.xtp            Design Preservation Model N Structures
mtl-z.xtp            Microtraisn Z Scale Track
peco-O-Bullhead.xtp  Peco O Scale Track
tomix-n.xtp          Tomix N Scale Track
toolkit-n.xtp        Useful Shapes and Tools for N Scale
kato-ho.xtp          KAT HO Track
atlasn55.xtp         Atlas N Code 55 Track 
LifeLike-N.xtp       Lifelike N Track
kato-n-DblTrk.xtp    Kato N Double Track
atl83ho.xtp          Atlas Code 83 HO Track
atlascho.xtp         Atlas HO Track
atlascn.xtp          Atlas N Track
smltown.xtp          Smalltown HO Structures
mrklnhoc.xtp         Maerklin HO C-Track
wlthho83.xtp         Walthers Code 83 HO Track
mp-n.xtp             Model Power N Structures
bach-n.xtp           Bachmann N Scale Structures
walth-n.xtp          Walthers N Scale Structures


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
