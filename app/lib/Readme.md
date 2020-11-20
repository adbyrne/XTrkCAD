# GA Release Notes

Welcome to the XtrackCAD V5.2.0 GA release! 

GA has a few high-impact bug fixes since Beta 3.0 that we have tested with the help of our users. 

The V5.2 release started out as just some simple functional enhancements of long-standing, like background images. The idea was to punt on all UI changes to the V6 GTK3 release.  But along the way and due to some sabaticals for developers, things kept getting added and tinkered with. Finally the major UI enhancements you will see were mapped out over the last six months and so we have an incremental enhacement to the UI as well.

Enjoy!

Martin and Dave and Adam, your volunteer developers.

PS The full change log is a file in the XtrkCAD download folder as CHANGELOG.md 

The files written by XTrackCAD 5.2.0 are versioned to only be read by 5.2.0, but it can also read files from earlier versions. If you get into trouble, please reach out, we may be able to help - but always back-up.

We will fix important bugs you find by issuing a dot release. 

To report bugs, please use the SourceForge bugs reporting page https://sourceforge.net/p/xtrkcad-fork/bugs/

To discuss the Release, please use the user forum https://xtrackcad.groups.io/g/main/topics

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
- Also Options->Display->Dont suppress System Cursor

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


## Installation ##

Please see http://xtrkcad-fork.sourceforge.net/Wikka/DownloadInstall.

## Upgrade Information ##

**Note:** This version of XTrackCAD comes with the several new features
like backgroudn images or extensions to notes. In order to support
this feature, an additional file format for layout files (.xtce) was added. 
The old .xtc format is still supported for reading and writing. So
files from earlier versions of XTrackCAD can be read without problems.
Layouts that were saved in the new format cannot be read by older
versions of XTrackCAD.

## Building ##

Please see http://xtrkcad-fork.sourceforge.net/Wikka/BuildNotes

## Where to go for support ##

The following web addresses will be helpful for any questions or bug
reports

- The group mailing list <https://xtrackcad.groups.io/g/main>
- The group Wiki <http://xtrkcad.org>
- The official Sourceforge site <https://sourceforge.net/projects/xtrkcad-fork>

Thanks for your interest in XTrackCAD.
 
