\page regression-tests Regression Test Demos

One entry per demo-playback regression test (`RegressionTestAll()`, the `-T`
flag -- see \ref advanced-optional-local-checks "Advanced/optional local
checks"). The description is the demo's own introductory `MESSAGE` text; the
screenshot is the layout's final state after the whole script runs. Regenerate
both via `tools/generate-regression-docs.sh`. Some demos may warrant additional
mid-script screenshots later -- `-d visualdiff=1` already captures one per
`REGRESSION` block too, see \ref advanced-optional-local-checks.

# Introduction (`dmintro.xtr`) {#demo-dmintro}

Welcome to the XTrackCAD demonstration.

This will show some the features of XTrackCAD in an automated presentation.  This window contains a number of controls and a message area (which I hope you are reading now). 

The controls are: Step - advances to the next step of the demo. Next - skips ahead to the next demo. Quit - exits the demo and returns to XTrackCAD. Speed - controls the speed of the demo.

Click Step now for the next message.

\image html dmintro-final.png "Introduction -- final state"

# Mouse Actions (`dmmouse.xtr`) {#demo-dmmouse}

In the drawing area of the main window you can see an hollow upwards arrow which represents the mouse cursor.  In this demo the mouse will move about to show you the actions of different commands.

The hollow arrow represents the mouse cursor without a mouse button being pressed.

*(no track content in this demo -- nothing to screenshot)*

# Dialogs (`dmdialog.xtr`) {#demo-dmdialog}

The demo also simulates entering values and selecting options on various dialogs.

This is simulated by drawing a rectangle around the control when values are entered or changed.

*(no track content in this demo -- nothing to screenshot)*

# Moving about (`dmmovabt.xtr`) {#demo-dmmovabt}

The main drawing area shows a portion of total layout.  You can zoom in or zoom out by choosing 'Zoom In' or 'Zoom Out' in the 'Edit' menu, by using the Zoom buttons on the toolbar or by using the 'Page Down' and 'Page Up' keys.

You can see the entire layout in the Map window.

\image html dmmovabt-final.png "Moving about -- final state"

# Describe (`dmcancel.xtr`) {#demo-dmcancel}

Pushing the &lt;Describe&gt; button will cancel any other command in progress.

Here we will begin to create a curved track which is a two step process.

\image html dmcancel-final.png "Describe -- final state"

# Select (`dmselect.xtr`) {#demo-dmselect}

The &lt;Select&gt; command is used to select tracks.

Selected tracks can be moved or rotated during the &lt;Select&gt; command.

Selected tracks can also be deleted, hidden, listed and exported.

When you move the cursor near a track that could be selected, the track is drawn with thick blue lines.

\image html dmselect-final.png "Select -- final state"

# Straight tracks (`dmstrtrk.xtr`) {#demo-dmstrtrk}

Straight tracks are created by selecting the first End-Point of the track.

\image html dmstrtrk-final.png "Straight tracks -- final state"

# Curved tracks (`dmcrvtrk.xtr`) {#demo-dmcrvtrk}

There are several ways to create a Curved track.

You can choose which to use by clicking on the small button to the left of &lt;Curve&gt; command button if the current Curve command is not the one you want.

The first is by clicking on the first End-Point and dragging in the direction of the Curve.

\image html dmcrvtrk-final.png "Curved tracks -- final state"

# Circles (`dmcircle.xtr`) {#demo-dmcircle}

Like the &lt;Curve&gt; track command, there are several ways to create a Circle track.

The first is to specify a fixed radius and simply drag the Circle into position.

We will change the Radius before proceeding.

\image html dmcircle-final.png "Circles -- final state"

# Turntables (`dmtrntab.xtr`) {#demo-dmtrntab}

Turntables are created by specifying the radius in a dialog box on the Status Bar.  The radius in the dialog can be changed before proceeding.

\image html dmtrntab-final.png "Turntables -- final state"

# Modifying end points  (`dmadjend.xtr`) {#demo-dmadjend}

The unconnected endpoints of a straight or curved track can be changed with the 'Modify Track' command.

\image html dmadjend-final.png "Modifying end points  -- final state"

# Extending (`dmextend.xtr`) {#demo-dmextend}

The unconnected endpoint of any track can also be extended with the &lt;Modify&gt; command using Right-Drag.

\image html dmextend-final.png "Extending -- final state"

# Medium and Thick Tracks (`dmtrkwid.xtr`) {#demo-dmtrkwid}

We can indicate the mainline by making the rails wider.

First we select the mainline tracks...

\image html dmtrkwid-final.png "Medium and Thick Tracks -- final state"

# Straight to straight (`dmjnss.xtr`) {#demo-dmjnss}

Two straight tracks can be joined by selecting the two endoints.  The selected endpoints will be those closest to the cursor when the track is selected.

First, we will select Easements None and then select Join

\image html dmjnss-final.png "Straight to straight -- final state"

# Curve to straight (`dmjncs.xtr`) {#demo-dmjncs}

The &lt;Join&gt; command can also join straight and curved tracks (in either order).

We will enable Cornu easements


\image html dmjncs-final.png "Curve to straight -- final state"

# Circle to circle (`dmjcir.xtr`) {#demo-dmjcir}

You can also join to and from circles.  This will change the circles to curves.

In this example we will join two circles.

\image html dmjcir-final.png "Circle to circle -- final state"

# Joining to turntables (`dmjntt.xtr`) {#demo-dmjntt}

You can connect from any track to a turntable

With a Cornu Easement you can have a turntable as the first point.


\image html dmjntt-final.png "Joining to turntables -- final state"

# Easements (`dmease.xtr`) {#demo-dmease}

This example will show the effect of using easements while joining tracks.

First, we will enable Cornu Easements and select Join

\image html dmease-final.png "Easements -- final state"

# Abutting tracks (`dmjnabut.xtr`) {#demo-dmjnabut}

This examples shows joining tracks whose End-Points are aligned.

Note the 2 pairs of tracks have End-Points that are close and aligned but not connected.

\image html dmjnabut-final.png "Abutting tracks -- final state"

# Move to Join (`dmjnmove.xtr`) {#demo-dmjnmove}

The &lt;Join&gt; command can move one group of tracks to join with another.

First &lt;Select&gt; the tracks you want to move with Ctrl so that they are both selected.

\image html dmjnmove-final.png "Move to Join -- final state"

# Select and Placement (`dmtosel.xtr`) {#demo-dmtosel}

We chose the turnout we want to place by clicking on the HotBar.

\image html dmtosel-final.png "Select and Placement -- final state"

# Building a yard throat. (`dmtoyard.xtr`) {#demo-dmtoyard}

This example show how to layout a yard using Turnouts from the HotBar and the &lt;Parallel&gt; command.

\image html dmtoyard-final.png "Building a yard throat. -- final state"

# Designing turnouts (`dmtodes.xtr`) {#demo-dmtodes}

These examples shows some of the various Turnout Designer windows.  Each window defines a different type of turnout.

In each window there are a number of parameters to fill in and one or two description lines.

You can print the design to check the dimensions before saving them.

*(no track content in this demo -- nothing to screenshot)*

# Group and Ungroup (`dmgroup.xtr`) {#demo-dmgroup}

The &lt;Group&gt; and &lt;Ungroup&gt; commands (on the Tools menu) are a powerful way to manipulate Turnout and Structure definitions.

We'll start with a simple turnout and add a switch machine.

\image html dmgroup-final.png "Group and Ungroup -- final state"

# Triming Turnout Ends (`dmtotrim.xtr`) {#demo-dmtotrim}

Sometimes it's useful to modify turnouts triming one of the ends.

We use the &lt;Split&gt; command for this.

\image html dmtotrim-final.png "Triming Turnout Ends -- final state"

# Handlaid Turnouts (`dmhndld.xtr`) {#demo-dmhndld}

In addition to using the turnout definitions you can create 'Hand Laid Turnout'.

This is two step process:

\image html dmhndld-final.png "Handlaid Turnouts -- final state"

# Elevations (`dmelev.xtr`) {#demo-dmelev}

We have designed part of the layout with a siding, 2 branches and a spiral loop.  We want to set elevations.

Note: make sure you set endpoint elevations on the Display dialog.

\image html dmelev-final.png "Elevations -- final state"

# Profile (`dmprof.xtr`) {#demo-dmprof}

To use the &lt;Profile&gt; command you first need to define Elevations on your layout.

In this example we'll use the Elevations defined in the last example.

You can move or resize the Profile dialog now if you want.

\image html dmprof-final.png "Profile -- final state"

# Delete and Undo (`dmdelund.xtr`) {#demo-dmdelund}

Pressing the &lt;Delete&gt; button lets you delete selected tracks from the layout.

First you select the tracks you want to delete, and then press the &lt;Delete&gt; button.

\image html dmdelund-final.png "Delete and Undo -- final state"

# Splitting and Tunnels (`dmsplit.xtr`) {#demo-dmsplit}

The &lt;Split&gt; command is used to split and disconnect tracks.

\image html dmsplit-final.png "Splitting and Tunnels -- final state"

# Parallel (`dmparall.xtr`) {#demo-dmparall}

This example shows how to create parallel tracks.

\image html dmparall-final.png "Parallel -- final state"

# Helix tracks (`dmhelix.xtr`) {#demo-dmhelix}

Now we will create a helix in the corner of the layout connected to 2 tracks.

\image html dmhelix-final.png "Helix tracks -- final state"

# Exception Tracks (`dmexcept.xtr`) {#demo-dmexcept}

XTrackCAD can help find tracks that are curved too sharply or are too steep.  These tracks are Exception tracks and are drawn in the Exception track color.

In this example we have a curved track with radius of 9 inches and a straight track with a grade of 3.8 percent.  

\image html dmexcept-final.png "Exception Tracks -- final state"

# Rescale (`dmrescal.xtr`) {#demo-dmrescal}

The &lt;Rescale&gt; command will change the size of the selected objects.

Note: due to technical reasons, the To Scale drop down list is blank.  For this demo it should show 'DEMO'.

First we will try rescaling by ratio.

\image html dmrescal-final.png "Rescale -- final state"

# Connect and Tighten - a siding (`dmconn1.xtr`) {#demo-dmconn1}

We have built a siding using Sectional track and have 2 End-Points that don't line up and are not connected automatically when placing the sectional track.

\image html dmconn1-final.png "Connect and Tighten - a siding -- final state"

# Connect and Tighten - figure-8 (`dmconn2.xtr`) {#demo-dmconn2}

In example shows a simple figure-8 layout using Sectional track. You will notice that the tracks do not line up exactly in one location.

\image html dmconn2-final.png "Connect and Tighten - figure-8 -- final state"

# Ruler (`dmruler.xtr`) {#demo-dmruler}

The &lt;Ruler&gt; command draws a Ruler on the layout you can use to measure distances.


\image html dmruler-final.png "Ruler -- final state"

# Table Edges (`dmtbledg.xtr`) {#demo-dmtbledg}

Table Edges are used to mark the edges of the layout, either for aisles or room walls.

\image html dmtbledg-final.png "Table Edges -- final state"

# Benchwork (`dmbench.xtr`) {#demo-dmbench}

You can draw a variety of different types of benchwork: - rectangular (1x2, 2x4 etc) - L girders - T girders

You can also draw them in different orientations.

\image html dmbench-final.png "Benchwork -- final state"

# Dimension Lines (`dmdimlin.xtr`) {#demo-dmdimlin}

Dimension Lines are used to mark the distances between two points.

Here we will create a Dimension Line to show the separation between two tracks.

\image html dmdimlin-final.png "Dimension Lines -- final state"

# Lines (`dmlines.xtr`) {#demo-dmlines}

The Draw Commands are used to draw straight and curved lines on the layout.


\image html dmlines-final.png "Lines -- final state"

# Poly-Shapes (`dmlines2.xtr`) {#demo-dmlines2}

We also draw Polylines and filled shapes.

\image html dmlines2-final.png "Poly-Shapes -- final state"

# Modifying Poly-Shapes (`dmplymod.xtr`) {#demo-dmplymod}

Polylines and polygons (created with the &lt;Draw&gt; command) can be modified by dragging on their corners or edges.

First Left Click on the shape you want to modify.

\image html dmplymod-final.png "Modifying Poly-Shapes -- final state"

# Rotate (`dmrotate.xtr`) {#demo-dmrotate}

The &lt;Rotate&gt; command will pivot the Selected objects.  First Click on the pivot point and then drag to Rotate the objects. In this example we will rotate the selected structure about it's center.

\image html dmrotate-final.png "Rotate -- final state"

# Flip (`dmflip.xtr`) {#demo-dmflip}

The &lt;Flip&gt; command will create a mirror image of the selected objects.

\image html dmflip-final.png "Flip -- final state"

# Control Panels (`dmctlpnl.xtr`) {#demo-dmctlpnl}

This demo will construct a control panel for part of a bigger layout.

\image html dmctlpnl-final.png "Control Panels -- final state"

# Notes (`dmnotes.xtr`) {#demo-dmnotes}

The &lt;Text Note&gt; command lets you attach notes to various spots on the layout.

\image html dmnotes-final.png "Notes -- final state"

