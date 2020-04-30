# XTrackCAD Changelog #

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/).

## [5.2.0]

### New and changed features

+ Add Join Line Command to create PolyLines from Straight, Bezier, Curved and PolyLines
+ countless internal changes were done to improve drawing performance and stability
+ Change Magnetic Snap reverse to be on the “Alt” key.
+ Added french translation for the UI, contributed by Jacques Glize
+ Windows: support for utf-8 character encoding in text fields, labels etc.
+ Add new options for SelectMode (Only, Add) and SelectZero (On, Off) to Options->Command.
+ Add regression testing when running demos
+ New Rotate Symbol during Rotate.
+ Profile window: fill color, label formating and positioning
+ Add PNG export for Windows using FreeImage
+ Apply rescale operation to background as well. When the scale,not the gauge, is changed, the background image is resized and repositioned to the same ratio.
+ Update demo files
+ Add the indexing and searching functions for parameter files
+ Add ‘?’ as a means to jump to the properties screen in Select Mode
+ Update Layout File Version to V11 and minimum required version to V5.2.0
+ Add a new Parallel Track Radius Factor option to increase the separation of the parallel track based on Radius. The default value is 0.0 which is no increase. A value of 1.0 is set to be a prototype increase scaled as appropriate.  In OO this will add about 1.5” for a 24” radius.
+ Add support for dotted and dashed lines to Draw Objects
+ Set and un-set favorite property for parameter files
+ Add option to create a Line rather than a Track in Parallel Command
+ Sort parameter file list by compatibility and contents description
+ Upgrade Select DoubleClick: Activate on Link and Documents, Modify for Cornu and Bezier, Draw except Text, otherwise Open Describe.
+ Default to Context Menu on Right-Click, Command Menu Shift+Right-Click
+ Increase visibility of Select Anchors by adding width when in hover.
+ Snap Dimension lines to other objects (tracks of draw objects) regardless of Shift state
+ Add full Module support including documentation for modular layouts.
+ Add two extra track properties and Edit Commands for them - “Bridge” - adds simple abutment symbol
“Ties/No Ties” - hides Ties on a selected track even if zoomed in - this can be useful for dual gauge or in docks and goods yards, where the rails are inset into the ground.
+ Rename “Above” to “Move to Front” and “Below” to “Move to Back” this has caused confusion in the past.
+ Allow Parallel separation of 0 when using different tracks gauges to produce overlaid tracks to simulate dual gauge.
+ Improve detailed editing of circles and curved lines
+ Updates to Demos
+ Add automatic fixed flex track element to hotbar - acts as Cornu and will join tracks as needed
+ Stop writing out tracks with colors
+ Rewrite Notes functions, add link and file reference options to Note
+ First run: turnoff track description and lenght labels to avoid clutter
+ Add new looks for Cursor and instrument cursor draw.
+ Initialize the Sticky dialog to reasonable values on first run.
All commands are sticky except for Helix, Handlaid Turnouts, Turntables and
Connect Two Tracks.
+ Cope with Multiple Cornus joined and either selected or not selected
+ Show Cornu Shape as connected tracks are being moved or rotated.
+ Allow for new Turnouts to be placed on Cornu tracks
+ Add highlighting to Select in which prompts for Move and Rotate and new draw aids when selected - allowing clear work for Shift (Select Connected) and Ctrl (Select an additional individual track)  Ensure that Right-Click is always used for context menus in the main commands.
+ Add Anchor for Join, Describe, Connect/Pull, also make selecting second track easier.
+ Add dynamic anchor prompts for Split that show the split or disconnect that will occur, and make disconnect a little more sticky by snapping to the connect point when close.
+ Change Modify to use Cornu easements if selected
+ Handles for no-track at end Cornu for angle and radius.
+ Show visible anchor points on Move/Rotate
+ Make Room stand-out in display with grey outside it
+ Curved Polygon support
+ Add placing Turnouts on Cornu Track. Fix Cornu extension code.
+ Add Turnouts with curved ends and new Cornu Turnout Designer options to build them.
+ Origin Mode for Draw Objects
+ Add Convert To Cornu from Fixed and Convert To Fixed From Cornu
+ Add Icon for Cornu
+ Extra Draw and Modify command features
+ Improve Polygon Create and Modify and Other Draw Create
+ Improve ruler with “English” measurements in High Zoom
+ Improved and fixed printing of page position
+ Indicate parameter file state by colored icons, show information about compatibility to current scale and gauge
+ Create proper flex-track lengths for pricing.
+ Text for Balloon Help (Tooltips) is created from a JSON format
+ Desktop icon can be created with Windows installation 
+ Installing on Windows overwrittes earlier version
+ Double clicking a note opens the note or the external application
+ Additional note type for web and local file links
+ Add button to select all pages for printing
+ Background images are stored in xtce files 
+ Background images can be loaded into a separate layer
+ New extended file format (.xtce) 
+ Cornus can be used for turnout design

### Bugs fixed

+ Fix Paste positioning issues and document.
+ Add Escape to hide many dialog Windows
+ Fix embedded quote in Text object issue. Don�t re-de-escape quotes.
+ Actually restore the GTK main window to be the size saved at last Exit. If that won�t fit, it will have been resized down. Also remove the �base� geometry settings - they aren�t for what the code thought they were.  The result should be between 30% and 100% of the max screen size.
+ Fix several highlighting and a Read bug for switchMotor
+ Fix crash on reading turnout motors from file
+ Fix #327, array bounds where not considered when creating layer list
+ To ease translation a text's source file and line are added in a comment to
the pot and po files
+ Remove Ruler at and of demo
+ TableEdges and DimLines are always Black
+ Only check circle radius for available room size when creating a circle
+ Fix some display bugs in the Profile window.
+ Clean up option flags
+ "Quit" can be cancelled
+ Fix crash when creating blocks from a larger number (>10?) of tracks elements
+ Fix crashes when loading and deleting block definitions
+ Set button label in Parameter dialog to Hide / Unhide as suggested in bug #319
+ Reduce Zoom messages to useful set
+ Adjust Ties algo to give a more even look
+ Stop Delete key (not backspace) working in other commands than Select
+ Fix icons to reflect circle drawing properly
+ Fix bug in Describe of BenchWork when setting angle
+ Fix display of offset structures in HotBar
+ Updated and corrected the TIP file.
+ Respect Turntable Angle for Modify and Join
+ Fix splash screen overlay by removing it when a dialog pops up under it.
+ Fix file save when file filter is missing
+ Increase details on Turnout Path failure message
+ Fix for Export Parameter File Menu
+ Correct the tip for Metric/English units. Although Canada is officially metric, everyone here models in inches.
+ Fix for #301 On first run, File|Open and File|Parameter Files open wrong directory
+ Fix for #300 Problems with Car Inventory dialogs
+ Fix for #299 Order of params in HotBar popup menu is backwards
+ Fix for #297 After Turnout window is created, unable to select from HotBar
+ Fix: Turntable Join with Cornu
+ #296 Escape key doesn't cancel the first dialog
+ Fix for #295 Crash when playing recorded sessions
+ Fix for #294 Linux: Cursor not visible when running demos
+ Fix - circle<->circle Cornu regression
+ Fix for #292	Join 2 curves with a straight and easements leaves a kink
+ Fix for bug #291 Incorrect tip shown at initial startup
+ Fix bounding box for multi-line text
+ Fix cursor position on Text add for GTK - side-effect of back-level Pango patch for some versions of Linux
+ Fix varieties of reversed multi-segment Cornu
+ Fix for split to preserve elevation properly (keep end point elev type and station name (if any) - new split end point gets elev_none.
+ Fix Block Load error with random endCnt
+ Fix failure on Modify of Cornu with one end disconnected
+ Approximating large radius curves is fixed
+ Fix bug #277: Describe demo doesn't work
+ Improve re-calculation of all end elevations on redraw.  Cache end elevations and distances on end points.
+ Fix problem of closing Notice Windows with the red button in GTK leading to a frozen application (Modal)
+ Fixes for GTK Text Positioning in Draw and Print
+ Added extra page indexes to page registration
+ Made registration marks print on top of layout elements
+ Fix profile to ensure more space on LHS and RHS based on text sizes used.
+ Fix dialog resize issue with small screens
+ Windows: fix uninstaller script so uninstall removes all entries from Start Menu
+ Fix rounding problem when connecting track

### Parameter file additions and changes

+ Updated and new parameter files for Maerklin and Walthers Cornerstone
+ Updated and renamed parameter files for Maerklin C, K and M and
Atlas O-scale 2 rail and 3 rail
+ Upgrade Tomix 1421 Buffer to stop Train before buffer
+ Reduce number of parameter files for Kato N scale
+ Add American and HOn3 car prototypes
+ Added parameter file for Remco Mighty Casey
+ Fix Atlas 832 Curve to have ends on track


## [5.1.2]

### Added
+ Make Debug menu both work and do something useful
This menu in Options->Debug only appears if the env variable XTRKCADEXTRA is set A "Loosen" command also appears in Modify when set.The Debug window lists any Logging entries (set with "-d loggingname=level" parms).For example, "-d trainMove=5 -d traverseCornu=2" sets two Loglines - one at level 5 and the other 2. The value of the level can be adjusted in the Debug window and then the button "OK" sets it.Given that a level value of 0 means no logging for that logging variable, this menu allows log/tracing to be adjusted on the fly after startup.
Debug Window has a default trace level option. This is the level of Log/Trace that all types of tracing will follow unless they have been specified explicitly in the startup parms or otherwise.
Any log entries created before the first invocation of the window will be included, so a tester could add a LogSet("traverseBezier",0) line into the InitTrkBezier() code while testing or use a -d traverseBezier=0 and then use Debug to set level to 1 and start logging.

### Fixed
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
+ HO Peco parameters with corrected turnout definitions and new Mehano parameters

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
+ Make Cairo use pure RGB to retrieve the same color it stored for wDrawColor rather than ask for GDKï¿½s version (which might not be exact).
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
