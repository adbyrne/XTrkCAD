# XTrackCAD Changelog #

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/).

## [Unreleased]

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
