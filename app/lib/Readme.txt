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
software. See the project homepage at http://www.xtrkcad.org/ for news and
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

1) From the WinXP Explorer (or the Run command on the Start menu)
   execute xtrkcad-setup-4.0.3.exe.
   You will be prompted for a destination directory.  A new program group
   with XTrackCad icons will be created.
2) Open the XTrackCad program by double clicking on the icon and try it.
3) The Unistall icon can be used to uninstall XTrackCad.

By default, the installation progranm creates a new directory for the XTrackCad 
Version 4 files. An existing installation of earlier versions of XTrackCad is not 
overwritten. 
A new program group named XTrackCad 4 will be created in the Start menu. 

4. Installation on Linux
========================

0) You will need libc6, X11R6, GTK+2.0, gtkhtml2
1) Build instructions can be found on http://www.xtrkcad.org/
2) Place the executable into a directory in your path eg. /usr/local/bin
3) The content of the lib/ directory needs to be placed in /usr/local/share/xtrkcad,
	so do: 
	> mkdir /usr/local/share/xtrkcad
	> cp -r ./lib/* /usr/local/share/xtrkcad
4) Try it out by running: /usr/local/bin/xtrkcad


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
==========================

The following parameter files have been added or updated with this release:

Atlas Code 83 HO Scale (atl83ho.xtp)
Atlas HO Scale Cars (atlascho.xtp)
Atlas HO Scale Diesel Engines (atlaseho.xtp)
Atlas N Scale Cars (atlascn.xtp)
Atlas N Scale Diesel Engines (atlasen.xtp)
Atlas N-Scale Track (atlasn.xtp)
Atlas True-Track Code 65 N-Scale (AtlasTrueTrackN.xtp)
Bachmann E-Z Track Components (BachmannEZ-HO.xtp)
Bachmann EZ Track N-Scale (BachmannEZ-N.xtp)
Fast Track HO Scale (FastTrack-HO.xtp)
Jouef Points & Track Sections (JouefHO.xtp)
Kato Unitrack N-Scale (kato-n.xtp)
Kato Unitrack N-Scale Double Track Line (kato-n-DblTrk.xtp)
LGB G-Scale (lgb.xtp)
Lima Points & Track Sections (LimaHO.xtp)
Lionel O/O-27 Scale Track (Lionel-O-O27.xtp)
Micro Trains Z Scale Micro-Track Components (mtl-z.xtp)
Peco HO Scale Turnouts Setrack (pecohost.xtp)
Peco HOm and HOn3½ Turnouts (pecohom.xtp)
Peco HOn30 / 009 Turnouts (pecohon30.xtp)
Peco North American Code 83 HO Scale Turnouts (pecoho83.xtp)
Peco On30 Turnouts Trackwork (Peco-On30.xtp)
Piko G Scale Track (Piko-g.xtp)
Pilz/Tillig Advance Track System (TilligAdvTT.xtp)
Roco Geoline Track Components (RocoGeoLineHO.xtp)
Roco N Scale Turnouts (rocon.xtp)
Showcase Line Code 131 S Scale (S-Trax.xtp)
Tillig HOm Turnouts (tillig-hom.xtp)
Tokyo Marui Pro Z Scale Track Components (ProZ-Track.xtp)
USA Trains  (USA-G.xtp)
Walthers Cornerstone HO Structures (walth-ho.xtp)
Walthers/Shinohara HO-Scale Code 83 (wlthho83.xtp)

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


Thanks for your interest in XTrackCad.
