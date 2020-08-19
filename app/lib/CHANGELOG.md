# XTrackCAD Changelog #

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/).

## [5.2.0 Beta 2.1]

### Bugs

+ Failure in Parallel Line command fixed
+ Failure in ConvertFrom Bezier fixed
+ Bad track created in ConvertFrom Cornu that does not align (too liberal use of straight)
+ Context Help via F1 works in Wndows
+ Make all commands have target HTML pages for context help
+ Make Font Size be remembered when set from Text command
+ Fixed Radius field to be mainatained in Join after first point selected
+ Param Search UI result box resizeable 
+ Save Bridge Status for Segment Track
+ Updated German translations and translated Help Menu items
+ Clear NoTies if track is hidden
+ ConvertForm and ConvertTo to inherit characterstics of donor Tracks
+ ColorDraw and ColorTrack stored with layout file
+ Help content for Parameter File Dialog
+ Make Rulers overlay content
+ Change Modify/Create Polygon/PolyLine shortcuts to not clash with Pan center as 'c'
+ Track Segments in Structures and Turnouts retain color (including white)

### Added

+ Elevation to show height and offset when mousing over tracks without Shift
+ New anchor for Elevation+Shift to show Split will result
+ Debian Install

### Parameter Files

+ Peco G-45 file

## [5.2.0 Beta 2.0]

### Added

+ Two new line styles: Center Dot and Phantom Dot
+ Preference option to suppress Flex Track in HotBar
+ Pan using Shift+Mousewheel Up/Downand Shift+Ctrl MouseWheel for left/right
+ Pan using Shift+horizontal scroll on GTK
+ Param Reload Button to force reload of a param file
+ Multi Keyword search on param library files
+ "@" Pan to Center and "e" Pan to extents and "0" or "o" Pan to Origin 
+ Middle button also able to select Pan
+ Selectable Icon Button size between 1.0 and 2.0
+ Parallel Lines now can parallel other lines or tracks
+ Add angles for curved line properties
+ Desired Radius feature for non-Cornu Join
+ AutoSave feature and add keep checkpoints between saves
+ Suppress edge rules on layout if close to window edge rulers
+ Option to constrain drawing area to room boundaries on zoom 
+ New Anchors on Describe and Traditional Join
+ AutoSave feature and Backup of checkpoints
+ Anchors on Split within turnouts with Shift
+ Improved Turnout and Section placement when tracks overlap - less jumping
+ Change "@" to "c" for center Pan command in Select and Pan

### Fixed

+ Bug #256 Mislabelled Turnout
+ Bug #330 Use 3 decimal points in rotate angle
+ Bug #331 Correct Custom File Append Message
+ Bug #338 Correct Ruler Text size
+ Linetype changing doesn't change line width
+ Bug #332 Lowercase names in .xtp
+ Bug #339 Grades for 2 ended sectional track
+ Bug #340 Bezier Tracks open properly
+ Dragging in Profile Window fixed
+ Draw Objects remember linetype when saved/restored
+ Axis boundaries removed on Zoom Up and Down
+ Correct icons for xte and xtc in Windows
+ Windows size opening fixes
+ FilledDraw copies now work
+ Hangs on Linux startup
+ Linewidth correct in xpms
+ Elevation Points for clearances cleaned up
+ Convert Fixed->Cornu fixes
+ Bug #345 Fix paths in some Turnouts
+ Respect parmdir setting in configuration file
+ Bug #348 Fix Demo 
+ Bug #349 Fix inaccessible track segments
+ Bug #346 Fix layout file ends for Signal and Block
+ Bug #351 Fix loading layout with Cars off tracks
+ Bug #354 MultiLine Notes
+ Correct Undo when Cars are moved/rotated
+ Fix redraw of >20 elements in move/rotate
+ Ensure webkitgtk only required if not using browser for help
+ Fix mapD scale at startup, allow Map to be larger than 1/2 screen
+ Fix Turnout Trim with Split and Shift
+ Hotbar copes with large track objects (e.g., Turntables)
+ Make sure upper tracks are selected ahead of lower ones
+ Shift to suppress track end joins when drawing overlay tracks
+ Fix Curve from Chord anchors
+ Better sized Split and Connect Anchors
+ Windows PanHere now works
+ Fix Map Resize function on GTK
+ Fix Turnout placement on Cornu for Pins and other issues 
+ Fox Cornu Pin editing


### Added and changed parameter files

+ N-Tomix Track improvements
+ Cleanup double track pieces
+ Micro-Engineering track improvements
+ HO-Endo and Atlas controllers


## [5.2.0 Beta 1.0]

### Added

+ Notes
+ + Rewritten Notes function, add WebLink and File reference options to Note
+ Background
+ +  Add Background image file to layout with scaling, zooming, offset and angle
+ Archive Format
+ + Add Archive File Format .xtce using Zip libraries to include Background file
+ Parameters
+ + Parameter files have icons to show if they are compatible with current gauge/scale
+ + Sort Parameter files have by compatibility and contents description
+ + Finder for selecting System Parameter Library by Contents lines in files
+ + Set and un-set favorite property for parameter files
+ Track Properties
+ + Bridge track 
+ + Ties/NoTies 
+ Links in Layout
+ + Document links to local files as an Object
+ + WebLinks as an Object
+ Draw
+ + Draw objects can have dotted, dashed, dashed and dotted lines
+ + Rotation Origin for Draw elements so they can be rotated about a non-zero origin
+ + Origin Angle for Draw Objects to allow rotation around a different point
+ + Add Box option to Text so that Text can be outlined
+ + PolyLines (open Polygons)
+ + Polygons and Polylines can have smoothed or rounded vertexes
+ Cornu
+ + Pins for Cornu - morphs Cornu to pass through Pin, splits Cornu on Accept
+ + Edit multiple joined Cornu as single with Pins
+ + Add automatic 15 inch flex track element to Hotbar -> acts as Cornu and will also join tracks as needed
+ + Allow Cornu Tracks in Group 
+ + Add placing Turnouts on Cornu Track
+ + End Point Anchor for FlexTrack (Cornu) pieces
+ + Radius and angle handles for no-track end Cornu
+ + Show Cornu Shape as connected tracks are being moved or rotated
+ + Change Modify to use Cornu easements if selected
+ Commands
+ + New Add Cornu, Add PolyLine, Parallel Line, Convert to Cornu, Convert From Cornu commands
+ + New Join Line command to convert Lines into PolyLines
+ Help
+ + Add Context Help to jump to the current Command section in the manual
+ + Add F1 shortcut to Context help and Shift+F1 to go to the Contents
+ Add Layer by Layer control of color - each layer can exclude itself from Color
+ Parallel
+ + Allow Parallel separation of 0 when using different track gauges to produce overlaid tracks to simulate dual gauge track
+ + Add Radius Factor to Parallel command to space out parallel track more as radius tightens
+ Print
+ + Add PrintPage Positions to show which page goes where in multiple page prints
+ + Add button to select all pages for printing
+ + Added extra page indexes to page registration
+ + Made registration marks print on top of layout elements
+ First Start
+ + Canada now uses English measurements by default
+ + Update to the initial file open location to not set it to the examples directory
+ + First run: turnoff track description and length labels to avoid clutter
+ + Initialize the Sticky dialog to reasonable values on first run
+ + Updated and corrected the TIP file
+ + System Library location autoset so updated parameter files from new version appear in HorBar
+ Other
+ + Allow Turnouts to have with curved ends which are downward compatible using short fixed radius at the ends 
+ + Layers have Module option that are selected and Deselected as a unit
+ + Improve ruler with “English” measurements in High Zoom
+ + Add Examples... menu item on the Help menu to easily find them
+ + Added french translation for the UI, contributed by Jacques Glize
+ + Windows: support for utf-8 character encoding in text fields, labels etc.
+ + Add new options for SelectMode (Only, Add) and SelectZero (On, Off) to Options->Command.
+ + Demos updated
+ + Add regression testing when running demos
+ + Initialize Sticky on first run. All commands are sticky except for Helix, Handlaid Turnouts, Turntables and Connect Two Tracks.
+ + Change Modify to use Cornu easements if selected
+ + New Cornu Turnout Designer options to build all types of Turnouts
+ + Create proper flex-track lengths for pricing
+ + Desktop icon can be created with Windows installation 
+ + Installing on Windows overwrittes earlier version

### New UI

+ Layout 
+ + Allow viewing of "negative" layout up to half a screen to the left or bottom beyond the origin. Draw Room Walls and a grey zone outside the defined layout
+ + Add rulers on room walls if the display origin is in negative territory
+ Anchors 
+ + Anchors on all main commands - predict what will/can happen when clicked with modify keys (Ctrl,Shift,Alt)
+ + Add hover "anchors" for Select, Move, Rotate, Split, Join, Elevation, Move Description, Parallel
+ + Modify hover Anchors or all Straight, Curved Track, Straight and Curved Draw Objects
+ + Draw Anchors immediately adjust to Shift and Ctrl modifier key state
+ + Anchor for Join, fix anchor for Draw once selected
+ + Change System Cursor in main Window for Describe, Select and Pan/Zoom
+ + Add acnchors to Select for Move and Rotate 
+ + Add Anchors for Connect/Pull - also make selecting second track easier
+ Select Modes
+ + Select hover Anchors thick and in Blue to show what will be selected and Gold what will not be
+ + Select modes to either Select Only or Select Add
+ + Select Zero to provide Deselect All for click on nothing
+ Magnetic Snap
+ + Add Magnetic Snap mode (overriden when Alt held) to snap ends to nearby end points and to place new elements as extensions of existing ones
+ + Tracks Moved and Rotated so that ends touch will Auto-Align to the unmoved elements unless Alt is held
+ + Snap Dimension lines to other objects (tracks of draw objects) without Shift
+ + Right Drag exclusion in Select - different highlight colors for Left and Right Drag
+ Modify/Add
+ + Modify/Add of curved Draw objects is via end points and radius
+ + Modification of Poly objects shows vertexes to aid selection and show if a new one will be added
+ + Precision entry of Modify for Draw objects when Sticky selected
+ + Snapping Poly objects to be 0/90/180/270 from previous line with Shift
+ + Anchor shown for point that is 90 degrees from both last line and first point in Poly
+ + PolyLines and Polygons complete with Enter or Space in Modify or if user clicks away 
+ ShortCuts
+ + Text key shortcuts in Pan/Zoom - Zoom Levels "1-9", Extents "e", Origin "0"  
+ + “Pan Center Here” with a text short-cut “@“ key that works in Select, Pan/Zoom, Modify
+ Context Menus
+ + Default to Context Menu on Right-Click, Command Menu Shift+Right-Click
+ + New Select Context Menus for Selected and UnSelected cases
+ + Numerous updates to context menus including special for Poly Modify
+ DoubleClick in Select
+ + Open a Weblink, a Document 
+ + Modify for Cornu and Bezier, Modify Draw objects except Text
+ Elevation
+ + Elevation cursor shows elevation at point. Adding Shift displays clearance between two tracks\
+ + Make Elev use Ctrl+Left-Click for moving Descriptions
+ Export
+ + Change to export .png bitmap files in GTK
+ + Include Text objects in Bitmap Export
+ Context Menu
+ + Default to Context Menu on Right-Click, Command Menu Shift+Right-Click
+ Other
+ + Show Selected Tracks in Move to Join (Shift held in Join)
+ + New Rotate Symbol during Rotate.
+ + Profile window: fill color, label formating and positioning
+ + Add PNG export for Windows using FreeImage
+ + Apply rescale operation to background as well. When the scale,not the gauge, is changed, the background image is resized and repositioned to the same ratio.
+ + Add "?" as a means to jump to the properties screen in Select Mode
+ + Update Layout File Version to V11 and minimum required version to V5.2.0
+ + Show Cornu Shape as connected tracks are being moved or rotated


## Fixed

+ Make sure that Delete key works only if in Select Mode
+ Clean up HotBar right click display order documentation
+ Remove timed validation of text entries, use Enter-key, Click Away or Tab to trigger instead
+ Drawing of Ties to use Polygons, reducing the load of unfilled Ties on Redraw
+ Draw temp Polygons and Circles when moving them (all unfilled and simplified) 
+ Add up/down arrow keys scrolling in Select
+ Ctrl+Left-Click to rotate Turnouts - in common with other rotates
+ Make sure Draw commits simple elements even if Esc is subsequently pressed
+ Make Note icon size better
+ Make Bezier and Cornu use standard colors during construction
+ Improve Grid drawing performance 
+ Make Status fields insensitive when visible
+ Respect Turntable Angle for Modify and Join to Turntable
+ Save State including filenames and options when Save, Save As or Open commands are run
+ Change size of Select “spot” for Note/Link based on display scale.
+ Reduce impact of high radius curves by selecting out those parts that can’t be displayed before drawing them
+ Support Esc in closing all dialog windows
+ Make Display Layer color options clearer
+ Stop writing out tracks with colors in layout
+ Ensure that Parallel Track still duplicates the Tunnel/Width characteristics
+ Fix Turnout Group Path in cases where there are draw elements present
+ Show parameter files that are R/O
+ Print Polygons work in GTK
+ Make repeated arrow key moves in Move into one Undo
+ Adjust Ties algo to give a more even look in short tracks
+ Fix bounding box for multi-line text
+ Fix Describe Window sizing 
+ Fix for split to preserve elevation properly (keep end point elev type and station name (if any) - new split end point gets elev_none
+ Fix handling of \n in multiline comments
+ Fix flex-track lengths for pricing
+ Fix splash screen overlay by removing it when a dialog pops up under it
+ Fix Turntable Join with Cornu
+ Order of params in HotBar popup menu is corrected
+ Allow Trains to run properly on Double Track components
+ Respect order of non-draw elements in Group
+ Make Up and Down Scroll only move 1/2 a screen height (rather than 1/2 a width)
+ Stop Curve, Straight, Bezier and Cornu joining/snapping to a different scale track
+ Restrict max radius in curves to 10000 inches or less
+ Windows: Fix entry lengths > 78, run trigger action when pasting from clipboard
+ Fix Splitting between Turnouts
+ Cursor made visible when running demos in GTK
+ Fix window size startup on GTK
+ Fix included double quote in Text field
+ Fix Paste position
+ Fix display of offset structures in HotBar
+ Fix embedded quote in Text object issue. Don't re-de-escape quotes
+ Restore the GTK main window to be the size saved at last Exit
+ Fix several highlighting issues and a Read bug for switchMotor
+ Fix crash on reading turnout motors from file
+ Fix #327, array bounds where not considered when creating layer list
+ To ease translation a text's source file and line are added in a comment to the pot and po files
+ Remove Ruler at and of demo
+ Fix TableEdges and DimLines are always Black
+ Only check circle radius for available room size when creating a circle
+ Fix some display bugs in the Profile window.
+ Clean up option flags
+ "Quit" can be cancelled
+ Fix crash when creating blocks from a larger number (>10?) of tracks elements
+ Fix crashes when loading and deleting block definitions
+ Set button label in Parameter dialog to Hide / Unhide as suggested in bug #319
+ Reduce Zoom messages to useful set
+ Fix icons to reflect circle drawing properly
+ Fix bug in Describe of BenchWork when setting angle
+ Fix file save when file filter is missing
+ Increase details on Turnout Path failure message
+ Fix for Export Parameter File Menu
+ Fix for #301 On first run, File|Open and File|Parameter Files open wrong directory
+ Fix for #300 Problems with Car Inventory dialogs
+ Fix for #299 Order of params in HotBar popup menu is backwards
+ Fix for #297 After Turnout window is created, unable to select from HotBar
+ Fix: Turntable Join with Cornu
+ Fix for #296 Escape key doesn't cancel the first dialog
+ Fix for #295 Crash when playing recorded sessions
+ Fix for #294 Linux: Cursor not visible when running demos
+ Fix - circle<->circle Cornu regression
+ Fix for #292    Join 2 curves with a straight and easements leaves a kink
+ Fix for bug #291 Incorrect tip shown at initial startup
+ Fix bounding box for multi-line text
+ Fix cursor position on Text add for GTK - side-effect of back-level Pango patch for some versions of Linux
+ Fix varieties of reversed multi-segment Cornu
+ Fix for split to preserve elevation properly (keep end point elev type and station name (if any) - new split end point gets elev_none.
+ Fix Block Load error with random endCnt
+ Fix failure on Modify of Cornu with one end disconnected
+ Approximating large radius curves is fixed
+ Fix for #277: Describe demo doesn't work
+ Improve re-calculation of all end elevations on redraw.  Cache end elevations and distances on end points.
+ Fix problem of closing Notice Windows with the red button in GTK leading to a frozen application (Modal)
+ Fixes for GTK Text Positioning in Draw and Print
+ Fix profile to ensure more space on LHS and RHS based on text sizes used.
+ Fix dialog resize issue with small screens
+ Windows: fix uninstaller script so uninstall removes all entries from Start Menu
+ Fix rounding problem when connecting track

### Added and changed parameter files
+ Updated and new parameter files for Maerklin and Walthers Cornerstone
+ Updated and renamed parameter files for Maerklin C, K and M and Atlas O-scale 2 rail and 3 rail
+ Upgrade Tomix 1421 Buffer to stop Train before buffer
+ Reduce number of parameter files for Kato N scale
+ Add American and HOn3 car prototypes
+ Added parameter file for Remco Mighty Casey
+ HO-Peco-Code75Finescale and HO-Peco-Code100Streamline new definitions for Ys, curved, catchpoints, 3-ways
+ N-Peco-Code55Finescale and N-Peco-Code80Streamline new definitions for curved\
+ Fix Atlas 832 Curve to have ends on track


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
+ Make Cairo use pure RGB to retrieve the same color it stored for wDrawColor rather than ask for GDKs version (which might not be exact).
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
