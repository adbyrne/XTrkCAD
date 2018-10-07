# XTrackCAD Changelog #

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/).

## [5.1.2]

### Added
+ Make Debug menu both work and do something useful 
This menu in Options->Debug only appears if the env variable XTRKCADEXTRA is set A "Loosen" command also appears in Modify when set.The Debug window lists any Logging entries (set with "-d loggingname=level" parms).For example, "-d trainMove=5 -d traverseCornu=2" sets two Loglines - one at level 5 and the other 2. The value of the level can be adjusted in the Debug window and then the button "OK" sets it.Given that a level value of 0 means no logging for that logging variable, this menu allows log/tracing to be adjusted on the fly after startup.
Debug Window has a default trace level option. This is the level of Log/Trace that all types of tracing will follow unless they have been specified explicitly in the startup parms or otherwise.
Any log entries created before the first invocation of the window will be included, so a tester could add a LogSet("traverseBezier",0) line into the InitTrkBezier() code while testing or use a -d traverseBezier=0 and then use Debug to set level to 1 and start logging.

## Fixed
+ Make Up and Down Scroll only move 1/2 a screen height (rather than 1/2 a width)
+ Fix Modify redraw for Bezier or Cornu
+ Allow modify of naked Cornu along the Cornu itself if it isn't connected to another Cornu or Bezier 
+ Fix Abend on extend of naked Cornu
+ Make sure Flip Cornu produces a correct relationship between ends and Bezier segments
+ Fix Traverse Cornu for case where there are multiple sub-segments within a Bezier segment 
+ Remove UndoModify from low-level functions - to ensure that they can't be called without a preceding UndoStart and cause error messages
+ Description: correct include tag for Linux
+ Fix possible error when Cloning Structures or Turnouts  
+ Fix bad test for RescaleTrack and no test for RotateTrack.
+ Fix the Modify Polygon Undo problem
+ Fix memory bug when flipping a Polygon
+ Stop Connect always logging without being initialized and fix message if logging is initialized
+ Fix memory violations when logging
+ Fix Double Track components so Train will work properly
+ Improved German translations
+ Include StringLimit code to stop overwrites
+ Fix possible memory overrun when updating a car
+ Improve performance of Window Resizing, especially to a smaller size in GTK

### Added and changed parameter files
+ Parameter files TT Kuehn Peco HO US, Newqida G, Atlas N and Fasttrack Nn3 new or update
+ New parameter file for Weinert Mein Gleis
+ Update parameter file for Z-Rokuhan

## [5.1.1]

### Added

### Fixed
+ Change gtk print.c to use native dpi for real printers and only up def to 600 dpi for file outputs.
+ Fix runaway storage and long wait for Car Inventory->Add
+ Fix map window not resized for map scale change in GTK
+ Last uintptr_t replaced with uint32_t in param.c
+ Control limiting of entered string length with a special flag in paramData_t
+ Downgrade CMake version requirement to 2.8 for CentOS 6 and Ubuntu 14.04.
+ Update print to reflect that GTK always uses PDF now.
+ Add fix for zoom grid performance from V5.2 to V5.1
+ Ensure that white (hidden) straight track segments get written out as white
+ Upgrade Map Icon
+ Add variable Print Override environment variables
XTRKCADPRINTSCALE=scale_factor
XTRKCADPRINTTEXTSCALE=text_factor
scale_factor is a floating point number is frequently 1.0 (if needed)
text_factor is a floating point number frequently 0.75 (if needed)
These variables are ignored if the print is to a file.
These seem most likely to result from Pango or other parts of the PDF chain to the printer being given bad DPI information which causes the scaling(s) to be inaccurate.
If there is enough use, we can add to the print dialog itself.
+ Fix crash when changing the layer from the properties dialog of cornus
+ Fix Cornu Describe Abend when altering Layer.
+ Fix odd lost overlaid labels in Describe when lots of Positions are used
+ All: Check string length for all relevant PD_STRING entry fields
+ Windows: Always set color before drawing text
+ Make sure that Text Segs and Poly Segs copy string and Pts when UnGrouping. Stop weird results and Abends.
+ Make Cairo use pure RGB to retrieve the same color it stored for wDrawColor rather than ask for GDK�s version (which might not be exact).
+ Fix Cornu Rate of Change of Curvature.
+ Fix situation where a Bezier or Cornu is modified when the scale  or gauge has since been set to a different value than the track to be modified. Remember the old track values.
+ Fix Abend on cornu Join to turntable, also make sure cornu Modify leaves more than minlength on connected tracks
+ Fix Abend on Describe or Delete of Compound with more than 4 end points (like a turntable)
+ Try to stop issues with F3 objects in Compounds.
+ Correctly process F segment records (i.e., no version).
+ Remove automatic word wrap in describe Text object, linefeeds can be edited manually

### Added and changed parameter files

## [5.1.0]

### Added

### Fixed
+ Fix Traditional Easements that are smaller than Sharp value between 0.21 and 0.5
+ Resuming after an abort takes precedence over loading last layout
+ Fix the vanish track segments problem.
+ Change all track segments currently set to white to black in parameter files and examples
+ Changed initial defaults to orange for exception color and snap grid turned off
+ Make snap feature work with rotated or moved lines
+ Fix track description editing for bezier, cornu and curve
+ Fix track description editing for compound (turnouts)
+ Flip sense of records for Bezier and Cornu so that the default (hidden) matches all the records written so far (0). Only those explicitly unhidden with have the bit flipped.
+ Fix Abend when naked cornu end is modified
+ Allow for small rounding errors when checking minimum radius
+ Round down exception radii in Cornu and Bezier
+ Limit maximum length for PD_STRING and enable limits for layout title
+ Add new check for radius > room dimensions when creating helix or circle
+ Fix map window update on zoom with options for gtk, map overlay update on pan/zoom and pan/zoom messages and doc.
+ Update German translations

### Added and changed parameter files

## [5.1.0beta]

### Added
+ Finish the new End Point records which have fixed positions and all fields are output
+ Add Select Track and then Right-Click mode to Connect Tracks to reconnect large numbers
of tracks in one command (and provide an alternative to accurately selecting two close end-points).
+ Pan/Zoom button LeftDrag Pan, RightDrag Zoom
+ Pan/Zoom command adds 0 key to set origin and e key to zoom to extents
+ Improve zoom and pan performance if map is showing
+ Amend Mass Connect to use two passes - one close and one wide
+ Add Select All and Select All Current to popup menus

### Fixed
+ Open external sites in separate window from help (bug #219)
+ Fix the initial position of rotated elements
+ Make sure all connection parms do not exceed bounds from options dialogs
+ Fix Abend on naked cornu modify
+ Fix invalid value modification when selecting new number format
+ Fix error threshold for bezier to avoid weird curves if connection distance high
+ Re-enable splitting fixed track for straight, curved and bumpers
+ Fix prompt for Join Start
+ Fix Abend when using connect for first track to turntable

### Added and changed parameter files

## [5.0.0.beta5]

### Added
+ Allow Turnout Placement on Bezier Tracks, also improve Bezier splitting logic
to be more precise
+ Ensure Cornu is deleted if connected track is deleted (like Easement)
+ Make an unfilled Box into a Polygon with a RECTANGLE shape. Add special edit
capability for filled and unfilled Boxes in Modify that preserve their shape and
allow for either editing at the corner or on a side. Add user prompts during
editing.
+ All: Add multi line text fields in drawing
+ Windows: Select monospaced font for parts list

### Fixed
+ Linux/OSX: Fix memory leak when updating status bar
+ Stop turnout placement on helix, ensure no turnout placement on bezier or cornu
+ Set the width of the benchwork selector
+ Restore Labels in HotBar to full size and sort out layout even without labels
+ Fix Lionel files which were binary and had bad end lines
+ Fix assorted leaks and adjust the rate of change of curvature calc
+ Windows: Fix text handling for multi line edit fields, bug #198
+ Windows: Fix printing multi page parts lists
+ All: Fix car part files
+ Windows: Fix parts list
+ All: Fix Traverse of rotated turnouts and issues with inaccurate segments in turnouts

### Added and Changed Parameter Files
+ Z-Atlas55.xtp Updated, added #6 Turnouts and 19d Crossing. Dimensions were scale from online images.
+ F-NMRA-RP12-21.xtp NMRA F Scale Std Gauge 13&#39; prototypical track centers
+ G-NMRA-RP12-23.xtp NMRA G Scale Std Gauge 13&#39; prototypical track centers

## [5.0.0.beta4]

### Added
+ Lock center of rotation to center of turntable if within 1/4 of turntable radius when clicked
+ Allow Connect Two Tracks to connect correctly aligned stall tracks to a turntable like normal tracks
+ Add Precise Move Right-Click submenu
+ Additional German translations

### Fixed
+ Linux/OSX: Fix Note invisibility
+ Linux: FIX RPM dependencies for browser builds
+ Linux/OSX: Fixed invisibility of Note icon on trackplan
+ Fixed crash in demo mode
+ Update copyright notice in About dialog
+ Cope with larger system fonts set by user
+ Linux/OSX: Make sure the draw area and the message are correctly placed after redraw
+ Fix Abend in Train with Car Label Display enabled
+ Allow Cornu Join to work on Circles and Helixes
+ Fix Layers Abend
+ Update map window after Quick Move and Quick Rotate

## [5.0.0.beta3]

### Added
+ Add Micro-Move using Shift-Ctrl-Arrows in Move Command

### Changed
+ Improve Cornu documentation

###Fixed
+ Allow GTK window width to shrink, resize on restart to fit inside and show on available monitor(s)
+ Fix toolbar ballons blank after resize
+ Fix Display Elevations bug
+ Fix Bezier displayed radius and center for second end
+ Upgrade Describe for Compounds and reduce real estate for larger items by rendering POS X, Y on one line
+ Fix for up arrow panning down instead of up when un-shifted
+ Fixes for Traverse inside a Compound/Turnout.

## [4.4.0.beta2] - 2017-11-09

###Fixed
+ Cornu problem with saving that led to bad curves on file open
+ Cornu and Bezier problem with List Parts - led to abend plus bad lengths
+ Failure in Train if a train hits an end-point

###Added
+ Constrain add to unconnected Cornu end in Modify via right drag to be correct radius

## [4.4.0.beta1]

### Added
+ New Cornu track feature for more flexible easements
+ New Bezier tracks and lines
+ Snapping of new straight, curved and Bezier tracks to unconnected end points
+ Snapping of new straight, curved and Bezier lines to line segment ends
+ Use region specific defaults on initial run of program
+ Keep separate current directories per file type
+ Add option to highlight unconnected end points
+ Add option to keep lower corner in zoom
+ MicroStep Pan Buttons
+ Add toolbar button to toggle map window on and off

### Changed
+ During build the local browser can be enabled to show help, therefore replacing the built-in webkit viewer
+ Improved "New" layout function
+ User prompts added to Zoom function
+ Added and improved German translations
+ Add Print option to force out Centerline. Adjust dashed line for GTK to reduce gap between dashes

### Changed Parameter Files
+ Fixed three way turnout in Tillig TT Advanced track
+ Clean up CTC-Panel, Peco N Code 80 Streamline, KB Scale and On30 parameters
+ Fix scale definitions and clean up S-Trax, Hubner, Atlas O Scale parameters
+ Added structures to Walthers Cornerstone in HO and N
+ Extended T-Eishindo
+ Renamed rocon.xtp and updated N-fl for new ownership
+ Cleanup and reorganize Peco N track parameters
+ Cleanup and create separate files for Peco HO/OO track ranges
+ Peco HO Code 70, 83 and 100 - Added inspection pit
+ Peco N Code 55 and Code 80 - Added inspection pit
+ Atlas Code 100 HO-Added Bridges and Turntable
+ Atlas Code 83 HO-Added more Bridges
+ Kato HO-Added #4 and #6 Single Slip (Lefthand Crossover) Turnouts
+ Kato N-Added Open (inspection) Pit 20-016
+ Z Rokuhan-Added New Short Iron and Deck Girder Bridges and 440mm PC and Std Straight Track Sections
+ Add Micro Engineering parameter file
+ Updated and corrected parameter file for LGB

### Fixed
+ Keep layer count up to date and store relevant layers on save
+ Enable changing layers for "Notes" from "Properties" dialog
+ Fix bug #203 Train turns on itself at a buffer
+ Fix broken DXF export by putting correct version into DXF export
+ Allow larger number of files to be selected at once for loading
+ Fix bug #196 wrong layer number in object properties
+ Changing "Layout Options" can be cancelled
+ Consistently change "Describe" label to "Properties"
+ Make Text immediatetly update color as well as size
+ Fix filename handling bugs
+ Fix option dialog and end point drawing for normal endpoints
+ Numpad + and - can be used like standard + and - for zoom control
+ Make sure a number format is set after changing the measurement system
+ Linux/OSX: Fix color button bug
+ Fix extra data re-allocation logic in csignal.c
+ Updated spec file for EL6 RPMs
