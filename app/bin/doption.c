/** \file doption.c
 * Option dialogs
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ccurve.h"
#include "cselect.h"
#include "custom.h"
#include "param.h"
#include "track.h"
#include "common-ui.h"
#include "ctrain.h"

//static paramIntegerRange_t i1_64 = { 1, 64 };
static paramIntegerRange_t i1_100 = { 1, 100 };
static paramIntegerRange_t i0_256 = { 0, 256 };
static paramIntegerRange_t i1_256 = { 1, 256 };
static paramIntegerRange_t i0_10000 = { 0, 10000 };
static paramIntegerRange_t i0_99 = { 0, 99};
static paramIntegerRange_t i1_1000 = { 1, 1000 };
static paramIntegerRange_t i10_1000 = { 10, 1000 };
static paramIntegerRange_t i10_100 = { 10, 100 };
static paramFloatRange_t r0o1_1 = { 0.1, 1 };
static paramFloatRange_t r1_10 = { 1, 10 };
static paramFloatRange_t r1_1000 = { 1, 1000 };
static paramFloatRange_t r0_180 = { 0, 180 };


static void UpdateMeasureFmt(void);

EXPORT long enableAudio = 1;

EXPORT long showFlexTrack = 1;

long GetChanges( paramGroup_p pg )
{
	long changes;
	long changed;
	int inx;
	for ( changed=ParamUpdate(pg),inx=0,changes=0; changed; changed>>=1,inx++ ) {
		if ( changed&1 ) {
			changes |= VP2L(pg->paramPtr[inx].context);
		}
	}
	return changes;
}

static void OptionDlgUpdate(
	paramGroup_p pg,
	int inx,
	void* valueP)
{
	if (inx < 0) { return; }


}

/****************************************************************************
 *
 * Display Dialog
 *
 */

static wWin_p displayW;

static char * autoPanLabels[] = { N_("Auto Pan"), NULL };
static char * drawTunnelLabels[] = { N_("Hide"), N_("Dash"), N_("Normal"), NULL };
static char * drawEndPtLabels3[] = { N_("None"), N_("Turnouts"), N_("All"), NULL };
static char * drawEndPtUnconnectedSize[] = { N_("Normal"), N_("Thick"), N_("Exception"), NULL };
static char * tiedrawLabels[] = { N_("None"), N_("Outline"), N_("Solid"), NULL };
static char * drawCenterCircle[] = { N_("Off"), N_("On"), NULL };
static char * labelEnableLabels[] = { N_("Track Descriptions"), N_("Lengths"), N_("EndPt Elevations"), N_("Track Elevations"), N_("Cars"), NULL };
static char * hotBarLabelsLabels[] = { N_("Part No"), N_("Descr"), NULL };
static char * listLabelsLabels[] = { N_("Manuf"), N_("Part No"), N_("Descr"), NULL };
static char * colorTrackLabels[] = { N_("Object"), N_("Layer"), NULL };
static char * colorDrawLabels[] = { N_("Object"), N_("Layer"), NULL };
static char * liveMapLabels[] = { N_("Live Map"), NULL };
static char * hideTrainsInTunnelsLabels[] = { N_("Hide Trains On Hidden Track"), NULL };
static char * constrainMainLabels[] = {N_("Constrain Drawing Area to Room boundaries"), NULL};
static char * dontHideLabels[] = {N_("Don't Hide System Cursor when program cursor is active"), NULL};


static paramData_t displayPLs[] = {
	{ PD_RADIO, &colorTrack, "color-track", PDO_NOPSHUPD|PDO_DRAW, colorTrackLabels, N_("Color Track"), BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &colorDraw, "color-draw", PDO_NOPSHUPD|PDO_DRAW, colorDrawLabels, N_("Color Draw"), BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &drawTunnel, "tunnels", PDO_NOPSHUPD|PDO_DRAW, drawTunnelLabels, N_("Draw Tunnel"), BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &drawEndPtV, "endpt", PDO_NOPSHUPD|PDO_DRAW, drawEndPtLabels3, N_("Draw EndPts"), BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &drawUnconnectedEndPt, "unconnected-endpt", PDO_NOPSHUPD|PDO_DRAW, drawEndPtUnconnectedSize, N_("Draw Unconnected EndPts"), BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &tieDrawMode, "tiedraw", PDO_NOPSHUPD|PDO_DRAW, tiedrawLabels, N_("Draw Ties"), BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &centerDrawMode, "centerdraw", PDO_NOPSHUPD|PDO_DRAW, drawCenterCircle, N_("Draw Centers"), BC_HORIZONTAL, I2VP(CHANGE_MAIN | CHANGE_MAP) },
	{ PD_LONG, &twoRailScale, "tworailscale", PDO_NOPSHUPD, &i1_256, N_("Two Rail Scale"), 0, I2VP(CHANGE_MAIN) },
	{ PD_FLOAT, &mapD.scale, "mapscale", PDO_NOPSHUPD, &r1_1000, N_("Map Scale"), 0, I2VP(CHANGE_MAP) },
	{ PD_TOGGLE, &dontHideCursor, "donthidecursor", PDO_NOPSHUPD, dontHideLabels, "", BC_HORIZONTAL },
	{ PD_TOGGLE, &constrainMain, "constrainmain", PDO_NOPSHUPD, constrainMainLabels, "", BC_HORIZONTAL },
	{ PD_TOGGLE, &liveMap, "livemap", PDO_NOPSHUPD, liveMapLabels, "", BC_HORIZONTAL },
	{ PD_TOGGLE, &autoPan, "autoPan", PDO_NOPSHUPD, autoPanLabels, "", BC_HORIZONTAL },
#define labelSelect (13)
	{ PD_TOGGLE, &labelEnable, "labelenable", PDO_NOPSHUPD, labelEnableLabels, N_("Label Enable"), 0, I2VP(CHANGE_MAIN) },
	{ PD_LONG, &labelScale, "labelscale", PDO_NOPSHUPD, &i0_256, N_("Label Scale"), 0, I2VP(CHANGE_MAIN) },
	{ PD_LONG, &descriptionFontSize, "description-fontsize", PDO_NOPSHUPD, &i1_1000, N_("Label Font Size"), 0, I2VP(CHANGE_MAIN) },
	{ PD_TOGGLE, &hotBarLabels, "hotbarlabels", PDO_NOPSHUPD, hotBarLabelsLabels, N_("Hot Bar Labels"), BC_HORIZONTAL, I2VP(CHANGE_TOOLBAR) },
	{ PD_TOGGLE, &layoutLabels, "layoutlabels", PDO_NOPSHUPD, listLabelsLabels, N_("Layout Labels"), BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_TOGGLE, &listLabels, "listlabels", PDO_NOPSHUPD, listLabelsLabels, N_("List Labels"), BC_HORIZONTAL, I2VP(CHANGE_PARAMS) },
	/* ATTENTION: update the define below if you add entries above */
#define I_HOTBARLABELS	(19)
	{ PD_COMBOLIST, &carHotbarModeInx, "carhotbarlabels", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT|PDO_LISTINDEX, I2VP(250), N_("Car Labels"), 0, I2VP(CHANGE_SCALE) },
	{ PD_LONG, &trainPause, "trainpause", PDO_NOPSHUPD, &i10_1000, N_("Train Update Delay"), 0, 0 },
	{ PD_TOGGLE, &hideTrainsInTunnels, "hideTrainsInTunnels", PDO_NOPSHUPD, hideTrainsInTunnelsLabels, "", BC_HORIZONTAL }
};
static paramGroup_t displayPG = { "display", PGO_RECORD|PGO_PREFMISC, displayPLs, COUNT( displayPLs ) };


static void DisplayOk( void * junk )
{
	long changes;
	changes = GetChanges( &displayPG );
	wHide( displayW );
	DoChangeNotification(changes);
}



static void DoDisplay( void * junk )
{
	if (displayW == NULL) {
		displayW = ParamCreateDialog( &displayPG, MakeWindowTitle(_("Display Options")),
		                              _("Ok"), DisplayOk, ParamCancel_Restore, TRUE, NULL, 0, OptionDlgUpdate );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Proto"), NULL,
		               I2VP(0x0002) );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Proto/Manuf"),
		               NULL, I2VP(0x0012) );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control,
		               _("Proto/Manuf/Part Number"), NULL, I2VP(0x0312) );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control,
		               _("Proto/Manuf/Partno/Item"), NULL, I2VP(0x4312) );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Manuf/Proto"),
		               NULL, I2VP(0x0021) );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control,
		               _("Manuf/Proto/Part Number"), NULL, I2VP(0x0321) );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control,
		               _("Manuf/Proto/Partno/Item"), NULL, I2VP(0x4321) );
	}

	ParamLoadControls( &displayPG );
	wShow( displayW );
#ifdef LATER
	DisplayChange( CHANGE_SCALE );
#endif
}


EXPORT addButtonCallBack_t DisplayInit( void )
{
	ParamRegister( &displayPG );
#ifdef LATER
	RegisterChangeNotification( DisplayChange );
#endif
	return &DoDisplay;
}

/****************************************************************************
 *
 * Command Options Dialog
 *
 */

static wWin_p cmdoptW;

static char * preSelectLabels[] = { N_("Properties"), N_("Select"), NULL };
static char * selectLabels[] = { N_("Single item selected, +Ctrl Add to selection"), N_("Add to selection, +Ctrl Single item selected"), NULL };
static char * selectZeroLabels[] = { N_("Deselect all on select nothing"), NULL };

#ifdef HIDESELECTIONWINDOW
static char * hideSelectionWindowLabels[] = { N_("Hide"), NULL };
#endif
static char * rightClickLabels[] = {N_("Normal: Command List, Shift: Command Options"), N_("Normal: Command Options, Shift: Command List"), NULL };

static paramData_t cmdoptPLs[] = {
	{ PD_RADIO, &preSelect, "preselect", PDO_NOPSHUPD, preSelectLabels, N_("Default Command"), BC_HORIZONTAL },
#ifdef HIDESELECTIONWINDOW
	{ PD_TOGGLE, &hideSelectionWindow, PDO_NOPSHUPD, hideSelectionWindowLabels, N_("Hide Selection Window"), BC_HORIZONTAL },
#endif
	{ PD_RADIO, &rightClickMode, "rightclickmode", PDO_NOPSHUPD, rightClickLabels, N_("Right Click"), 0 },
	{ PD_RADIO, &selectMode, "selectmode", PDO_NOPSHUPD, selectLabels, N_("Select Mode"), 0},
	{ PD_TOGGLE, &selectZero, "selectzero", PDO_NOPSHUPD, selectZeroLabels, "", 0 }
};
static paramGroup_t cmdoptPG = { "cmdopt", PGO_RECORD|PGO_PREFMISC, cmdoptPLs, COUNT( cmdoptPLs ) };

static void CmdoptOk( void * junk )
{
	long changes;
	changes = GetChanges( &cmdoptPG );
	wHide( cmdoptW );
	DoChangeNotification(changes);
}


static void CmdoptChange( long changes )
{
	if (changes & CHANGE_CMDOPT)
		if (cmdoptW != NULL && wWinIsVisible(cmdoptW) ) {
			ParamLoadControls( &cmdoptPG );
		}
}



static void DoCmdopt( void * junk )
{
	if (cmdoptW == NULL) {
		cmdoptW = ParamCreateDialog( &cmdoptPG, MakeWindowTitle(_("Command Options")),
		                             _("Ok"), CmdoptOk, ParamCancel_Restore, TRUE, NULL, 0, OptionDlgUpdate );
	}
	ParamLoadControls( &cmdoptPG );
	wShow( cmdoptW );
}


EXPORT addButtonCallBack_t CmdoptInit( void )
{
	ParamRegister( &cmdoptPG );
	RegisterChangeNotification( CmdoptChange );
	return &DoCmdopt;
}


/*****************************************************************************
 *
 *  Color
 *
 */

static wWin_p colorW;

static paramData_t colorPLs[] = {
	{ PD_COLORLIST, &snapGridColor, "snapgrid", PDO_NOPSHUPD, NULL, N_("Snap Grid"), 0, I2VP(CHANGE_GRID) },
	{ PD_COLORLIST, &markerColor, "marker", PDO_NOPSHUPD, NULL, N_("Marker"), 0, I2VP(CHANGE_GRID) },
	{ PD_COLORLIST, &borderColor, "border", PDO_NOPSHUPD, NULL, N_("Border"), 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &crossMajorColor, "crossmajor", PDO_NOPSHUPD, NULL, N_("Primary Axis"), 0, 0 },
	{ PD_COLORLIST, &crossMinorColor, "crossminor", PDO_NOPSHUPD, NULL, N_("Secondary Axis"), 0, 0 },
	{ PD_COLORLIST, &normalColor, "normal", PDO_NOPSHUPD, NULL, N_("Normal Track"), 0, I2VP(CHANGE_MAIN|CHANGE_PARAMS) },
	{ PD_COLORLIST, &selectedColor, "selected", PDO_NOPSHUPD, NULL, N_("Selected Track"), 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &profilePathColor, "profile", PDO_NOPSHUPD, NULL, N_("Profile Path"), 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &exceptionColor, "exception", PDO_NOPSHUPD, NULL, N_("Exception Track"), 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &tieColor, "tie", PDO_NOPSHUPD, NULL, N_("Track Ties"), 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &bridgeColor, "bridge", PDO_NOPSHUPD, NULL, N_("Bridge Base"), 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &roadbedColor, "roadbed", PDO_NOPSHUPD, NULL, N_("Track Roadbed"), 0, I2VP(CHANGE_MAIN) }
};
static paramGroup_t colorPG = { "rgbcolor", PGO_RECORD|PGO_FULLDIALOGFROMBUILDER, colorPLs, COUNT( colorPLs ) };



static void ColorOk( void * junk )
{
	long changes;
	changes = GetChanges( &colorPG );
	wHide( colorW );
	if ( (changes&CHANGE_GRID) && GridIsVisible() ) {
		changes |= CHANGE_MAIN;
	}
	DoChangeNotification( changes );
}


static void DoColor( void * junk )
{
	if (colorW == NULL) {
		colorW = ParamCreateDialog( &colorPG, MakeWindowTitle(_("Color")), _("Ok"),
		                            ColorOk, ParamCancel_Restore, TRUE, NULL, 0, NULL );
	}
	ParamLoadControls( &colorPG );
	wShow( colorW );
}


EXPORT addButtonCallBack_t ColorInit( void )
{
	ParamRegister( &colorPG );
	return &DoColor;
}

