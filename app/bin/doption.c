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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <ctype.h>
#include "track.h"
#include "ccurve.h"
#include "i18n.h"

static paramIntegerRange_t i0_64 = { 0, 64 };
static paramIntegerRange_t i1_64 = { 1, 64 };
static paramIntegerRange_t i1_100 = { 1, 100 };
static paramIntegerRange_t i1_256 = { 1, 256 };
static paramIntegerRange_t i0_10000 = { 0, 10000 };
static paramIntegerRange_t i1_1000 = { 1, 1000 };
static paramIntegerRange_t i10_1000 = { 10, 1000 };
static paramIntegerRange_t i10_100 = { 10, 100 };
static paramFloatRange_t r0o1_1 = { 0.1, 1 };
static paramFloatRange_t r1_10 = { 1, 10 };
static paramFloatRange_t r1_1000 = { 1, 1000 };
static paramFloatRange_t r0_180 = { 0, 180 };

static void UpdatePrefD( void );

EXPORT long enableBalloonHelp = 1;

long GetChanges( paramGroup_p pg )
{
	long changes;
	long changed;
	int inx;
	for ( changed=ParamUpdate(pg),inx=0,changes=0; changed; changed>>=1,inx++ ) {
		if ( changed&1 )
			changes |= (long)pg->paramPtr[inx].context;
	}
	return changes;
}


static void OptionDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	int quickMoveOld;
	if ( inx < 0 ) return;
	if ( pg->paramPtr[inx].valueP == &enableBalloonHelp ) {
		wEnableBalloonHelp((wBool_t)*(long*)valueP);
	} else if ( pg->paramPtr[inx].valueP == &quickMove ) {
		quickMoveOld = (int)quickMove;
		quickMove = *(long*)valueP;
		UpdateQuickMove(NULL);
		quickMove = quickMoveOld;
	} else if ( pg->paramPtr[inx].valueP == &units ) {
		UpdatePrefD();
	}
}

static void OptionDlgCancel(
		wWin_p win )
{
	wEnableBalloonHelp( (int)enableBalloonHelp );
	UpdateQuickMove(NULL);
	wHide( win );
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
static char * colorLayersLabels[] = { N_("Tracks"), N_("Other"), NULL };
static char * liveMapLabels[] = { N_("Live Map"), NULL };
static char * hideTrainsInTunnelsLabels[] = { N_("Hide Trains On Hidden Track"), NULL };
static char * zoomCornerLabels[] = {N_("Zoom keeps lower corner in view"), NULL};

extern long trainPause;

static paramData_t displayPLs[] = {
	{ PD_TOGGLE, &colorLayers, "color-layers", PDO_NOPSHUPD|PDO_DRAW, colorLayersLabels, N_("Color Layers"), BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_RADIO, &drawTunnel, "tunnels", PDO_NOPSHUPD|PDO_DRAW, drawTunnelLabels, N_("Draw Tunnel"), BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_RADIO, &drawEndPtV, "endpt", PDO_NOPSHUPD|PDO_DRAW, drawEndPtLabels3, N_("Draw EndPts"), BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_RADIO, &drawUnconnectedEndPt, "unconnected-endpt", PDO_NOPSHUPD|PDO_DRAW, drawEndPtUnconnectedSize, N_("Draw Unconnected EndPts"), BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_RADIO, &tieDrawMode, "tiedraw", PDO_NOPSHUPD|PDO_DRAW, tiedrawLabels, N_("Draw Ties"), BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_RADIO, &centerDrawMode, "centerdraw", PDO_NOPSHUPD|PDO_DRAW, drawCenterCircle, N_("Draw Centers"), BC_HORZ, (void*)(CHANGE_MAIN | CHANGE_MAP) },
	{ PD_LONG, &twoRailScale, "tworailscale", PDO_NOPSHUPD, &i1_64, N_("Two Rail Scale"), 0, (void*)(CHANGE_MAIN) },
	{ PD_LONG, &mapScale, "mapscale", PDO_NOPSHUPD, &i1_256, N_("Map Scale"), 0, (void*)(CHANGE_MAP) },
	{ PD_TOGGLE, &zoomCorner, "zoom-corner", PDO_NOPSHUPD, zoomCornerLabels, "", BC_HORZ },
	{ PD_TOGGLE, &liveMap, "livemap", PDO_NOPSHUPD, liveMapLabels, "", BC_HORZ },
	{ PD_TOGGLE, &autoPan, "autoPan", PDO_NOPSHUPD, autoPanLabels, "", BC_HORZ },
	{ PD_TOGGLE, &labelEnable, "labelenable", PDO_NOPSHUPD, labelEnableLabels, N_("Label Enable"), 0, (void*)(CHANGE_MAIN) },
	{ PD_LONG, &labelScale, "labelscale", PDO_NOPSHUPD, &i0_64, N_("Label Scale"), 0, (void*)(CHANGE_MAIN) },
	{ PD_LONG, &descriptionFontSize, "description-fontsize", PDO_NOPSHUPD, &i1_1000, N_("Label Font Size"), 0, (void*)(CHANGE_MAIN) },
	{ PD_TOGGLE, &hotBarLabels, "hotbarlabels", PDO_NOPSHUPD, hotBarLabelsLabels, N_("Hot Bar Labels"), BC_HORZ, (void*)(CHANGE_TOOLBAR) },
	{ PD_TOGGLE, &layoutLabels, "layoutlabels", PDO_NOPSHUPD, listLabelsLabels, N_("Layout Labels"), BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_TOGGLE, &listLabels, "listlabels", PDO_NOPSHUPD, listLabelsLabels, N_("List Labels"), BC_HORZ, (void*)(CHANGE_PARAMS) },
/* ATTENTION: update the define below if you add entries above */
#define I_HOTBARLABELS	(17)
	{ PD_DROPLIST, &carHotbarModeInx, "carhotbarlabels", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT|PDO_LISTINDEX, (void*)250, N_("Car Labels"), 0, (void*)CHANGE_SCALE },
	{ PD_LONG, &trainPause, "trainpause", PDO_NOPSHUPD, &i10_1000 , N_("Train Update Delay"), 0, 0 },
	{ PD_TOGGLE, &hideTrainsInTunnels, "hideTrainsInTunnels", PDO_NOPSHUPD, hideTrainsInTunnelsLabels, "", BC_HORZ }
 };
static paramGroup_t displayPG = { "display", PGO_RECORD|PGO_PREFMISC, displayPLs, sizeof displayPLs/sizeof displayPLs[0] };


static void DisplayOk( void * junk )
{
	long changes;
	changes = GetChanges( &displayPG );
	wHide( displayW );
	DoChangeNotification(changes);
}


#ifdef LATER
static void DisplayChange( long changes )
{
	if (changes & (CHANGE_SCALE|CHANGE_UNITS))
		if (displayW != NULL && wWinIsVisible(displayW) )
			ParamLoadControls( &displayPG );
}
#endif


static void DoDisplay( void * junk )
{
	if (displayW == NULL) {
		displayW = ParamCreateDialog( &displayPG, MakeWindowTitle(_("Display Options")), _("Ok"), DisplayOk, OptionDlgCancel, TRUE, NULL, 0, OptionDlgUpdate );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Proto"), NULL, (void*)0x0002 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Proto/Manuf"), NULL, (void*)0x0012 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Proto/Manuf/Part Number"), NULL, (void*)0x0312 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Proto/Manuf/Partno/Item"), NULL, (void*)0x4312 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Manuf/Proto"), NULL, (void*)0x0021 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Manuf/Proto/Part Number"), NULL, (void*)0x0321 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, _("Manuf/Proto/Partno/Item"), NULL, (void*)0x4321 );
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
	wEnableBalloonHelp( (int)enableBalloonHelp );
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

static char * moveQlabels[] = {
		N_("Normal"),
		N_("Simple"),
		N_("End-Points"),
		NULL };
		
static char * preSelectLabels[] = { N_("Describe"), N_("Select"), NULL };

#ifdef HIDESELECTIONWINDOW
static char * hideSelectionWindowLabels[] = { N_("Hide"), NULL };
#endif
static char * rightClickLabels[] = {N_("Normal: Command List, Shift: Command Options"), N_("Normal: Command Options, Shift: Command List"), NULL };

EXPORT paramData_t cmdoptPLs[] = {
	{ PD_RADIO, &quickMove, "move-quick", PDO_NOPSHUPD, moveQlabels, N_("Draw Moving Tracks"), BC_HORZ },
	{ PD_RADIO, &preSelect, "preselect", PDO_NOPSHUPD, preSelectLabels, N_("Default Command"), BC_HORZ },
#ifdef HIDESELECTIONWINDOW
	{ PD_TOGGLE, &hideSelectionWindow, PDO_NOPSHUPD, hideSelectionWindowLabels, N_("Hide Selection Window"), BC_HORZ },
#endif
	{ PD_RADIO, &rightClickMode, "rightclickmode", PDO_NOPSHUPD, rightClickLabels, N_("Right Click"), 0 }
	};
static paramGroup_t cmdoptPG = { "cmdopt", PGO_RECORD|PGO_PREFMISC, cmdoptPLs, sizeof cmdoptPLs/sizeof cmdoptPLs[0] };

EXPORT paramData_p moveQuickPD = &cmdoptPLs[0];

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
		if (cmdoptW != NULL && wWinIsVisible(cmdoptW) )
			ParamLoadControls( &cmdoptPG );
}


static void DoCmdopt( void * junk )
{
	if (cmdoptW == NULL) {
		cmdoptW = ParamCreateDialog( &cmdoptPG, MakeWindowTitle(_("Command Options")), _("Ok"), CmdoptOk, OptionDlgCancel, TRUE, NULL, 0, OptionDlgUpdate );
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

/****************************************************************************
 *
 * Preferences
 *
 */

static wWin_p prefW;
static long displayUnits;

static wIndex_t distanceFormatInx;
static char * unitsLabels[] = { N_("English"), N_("Metric"), NULL };
static char * angleSystemLabels[] = { N_("Polar"), N_("Cartesian"), NULL };
static char * enableBalloonHelpLabels[] = { N_("Balloon Help"), NULL };
static char * startOptions[] = { N_("Load Last Layout"), N_("Start New Layout"), NULL };

static paramData_t prefPLs[] = {
	{ PD_RADIO, &angleSystem, "anglesystem", PDO_NOPSHUPD, angleSystemLabels, N_("Angles"), BC_HORZ },
	{ PD_RADIO, &units, "units", PDO_NOPSHUPD|PDO_NOUPDACT, unitsLabels, N_("Units"), BC_HORZ, (void*)(CHANGE_MAIN|CHANGE_UNITS) },
#define I_DSTFMT		(2)
	{ PD_DROPLIST, &distanceFormatInx, "dstfmt", PDO_DIM|PDO_NOPSHUPD|PDO_LISTINDEX, (void*)150, N_("Length Format"), 0, (void*)(CHANGE_MAIN|CHANGE_UNITS) },
	{ PD_FLOAT, &minLength, "minlength", PDO_DIM|PDO_SMALLDIM|PDO_NOPSHUPD, &r0o1_1, N_("Min Track Length") },
	{ PD_FLOAT, &connectDistance, "connectdistance", PDO_DIM|PDO_SMALLDIM|PDO_NOPSHUPD, &r0o1_1, N_("Connection Distance"), },
	{ PD_FLOAT, &connectAngle, "connectangle", PDO_NOPSHUPD, &r1_10, N_("Connection Angle") },
	{ PD_FLOAT, &turntableAngle, "turntable-angle", PDO_NOPSHUPD, &r0_180, N_("Turntable Angle") },
	{ PD_LONG, &maxCouplingSpeed, "coupling-speed-max", PDO_NOPSHUPD, &i10_100, N_("Max Coupling Speed"), 0 },
	{ PD_TOGGLE, &enableBalloonHelp, "balloonhelp", PDO_NOPSHUPD, enableBalloonHelpLabels, "", BC_HORZ },
	{ PD_LONG, &dragPixels, "dragpixels", PDO_NOPSHUPD|PDO_DRAW, &r1_1000, N_("Drag Distance") },
	{ PD_LONG, &dragTimeout, "dragtimeout", PDO_NOPSHUPD|PDO_DRAW, &i1_1000, N_("Drag Timeout") },
	{ PD_LONG, &minGridSpacing, "mingridspacing", PDO_NOPSHUPD|PDO_DRAW, &i1_100, N_("Min Grid Spacing"), 0, 0 },
	{ PD_LONG, &checkPtInterval, "checkpoint", PDO_NOPSHUPD|PDO_FILE, &i0_10000, N_("Check Point") },
	{ PD_RADIO, &onStartup, "onstartup", PDO_NOPSHUPD, startOptions, N_("On Program Startup"), 0, NULL }
	};
static paramGroup_t prefPG = { "pref", PGO_RECORD|PGO_PREFMISC, prefPLs, sizeof prefPLs/sizeof prefPLs[0] };


typedef struct {
		char * name;
		long fmt;
	} dstFmts_t;
static dstFmts_t englishDstFmts[] = {
		{ N_("999.999999"),			DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|6 },
		{ N_("999.99999"),			DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|5 },
		{ N_("999.9999"),			DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|4 },
		{ N_("999.999"),			DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|3 },
		{ N_("999.99"),				DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|2 },
		{ N_("999.9"),				DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|1 },
		{ N_("999 7/8"),			DISTFMT_FMT_NONE|DISTFMT_FRACT_FRC|3 },
		{ N_("999 63/64"),			DISTFMT_FMT_NONE|DISTFMT_FRACT_FRC|6 },
		{ N_("999' 11.999\""),		DISTFMT_FMT_SHRT|DISTFMT_FRACT_NUM|3 },
		{ N_("999' 11.99\""),		DISTFMT_FMT_SHRT|DISTFMT_FRACT_NUM|2 },
		{ N_("999' 11.9\""),		DISTFMT_FMT_SHRT|DISTFMT_FRACT_NUM|1 },
		{ N_("999' 11 7/8\""),		DISTFMT_FMT_SHRT|DISTFMT_FRACT_FRC|3 },
		{ N_("999' 11 63/64\""),	DISTFMT_FMT_SHRT|DISTFMT_FRACT_FRC|6 },
		{ N_("999ft 11.999in"),		DISTFMT_FMT_LONG|DISTFMT_FRACT_NUM|3 },
		{ N_("999ft 11.99in"),		DISTFMT_FMT_LONG|DISTFMT_FRACT_NUM|2 },
		{ N_("999ft 11.9in"),		DISTFMT_FMT_LONG|DISTFMT_FRACT_NUM|1 },
		{ N_("999ft 11 7/8in"),		DISTFMT_FMT_LONG|DISTFMT_FRACT_FRC|3 },
		{ N_("999ft 11 63/64in"),	DISTFMT_FMT_LONG|DISTFMT_FRACT_FRC|6 },
		{ NULL, 0 } };
static dstFmts_t metricDstFmts[] = {
		{ N_("999.999"),			DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|3 },
		{ N_("999.99"),				DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|2 },
		{ N_("999.9"),				DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|1 },
		{ N_("999.999mm"),			DISTFMT_FMT_MM|DISTFMT_FRACT_NUM|3 },
		{ N_("999.99mm"),			DISTFMT_FMT_MM|DISTFMT_FRACT_NUM|2 },
		{ N_("999.9mm"),			DISTFMT_FMT_MM|DISTFMT_FRACT_NUM|1 },
		{ N_("999.999cm"),			DISTFMT_FMT_CM|DISTFMT_FRACT_NUM|3 },
		{ N_("999.99cm"),			DISTFMT_FMT_CM|DISTFMT_FRACT_NUM|2 },
		{ N_("999.9cm"),			DISTFMT_FMT_CM|DISTFMT_FRACT_NUM|1 },
		{ N_("999.999m"),			DISTFMT_FMT_M|DISTFMT_FRACT_NUM|3 },
		{ N_("999.99m"),			DISTFMT_FMT_M|DISTFMT_FRACT_NUM|2 },
		{ N_("999.9m"),				DISTFMT_FMT_M|DISTFMT_FRACT_NUM|1 },
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 } };
static dstFmts_t *dstFmts[] = { englishDstFmts, metricDstFmts };

/**
 * Load the selection list for number formats with the appropriate list of variants.
 */		
		
static void LoadDstFmtList( void )
{
	int inx;
	wListClear( (wList_p)prefPLs[I_DSTFMT].control );
	for ( inx=0; dstFmts[units][inx].name; inx++ )
		wListAddValue( (wList_p)prefPLs[I_DSTFMT].control, _(dstFmts[units][inx].name), NULL, (void*)dstFmts[units][inx].fmt );
}

/**
* Handle changing of measurement system. The list of number formats is loaded 
* and the first entry is selected as default value. 
*/

static void UpdatePrefD( void )
{
	long newUnits, oldUnits;
	int inx;

	if ( prefW==NULL || (!wWinIsVisible(prefW)) || prefPLs[1].control==NULL )
		return;
	newUnits = wRadioGetValue( (wChoice_p)prefPLs[1].control );
	if ( newUnits == displayUnits )
		return;
	oldUnits = units;
	units = newUnits;
	LoadDstFmtList();
	distanceFormatInx = 0;
	for ( inx = 0; inx<sizeof prefPLs/sizeof prefPLs[0]; inx++ ) {
		if ( (prefPLs[inx].option&PDO_DIM) ) {
			ParamLoadControl( &prefPG, inx );
		}
	}
	units = oldUnits;
	displayUnits = newUnits;
}


static void PrefOk( void * junk )
{
	wBool_t resetValues = FALSE;
	long changes;
	changes = GetChanges( &prefPG );
	if (connectAngle < 1.0) {
		connectAngle = 1.0;
		resetValues = TRUE;
	}
	if (connectDistance < 0.1) {
		connectDistance = 0.1;
		resetValues = TRUE;
	}
	if (minLength < 0.1) {
		minLength = 0.1;
		resetValues = TRUE;
	}
	if ( resetValues ) {
		NoticeMessage2( 0, MSG_CONN_PARAMS_TOO_SMALL, _("Ok"), NULL ) ;
	}

	wHide( prefW );
	DoChangeNotification(changes);
}



static void DoPref( void * junk )
{
	if (prefW == NULL) {
		prefW = ParamCreateDialog( &prefPG, MakeWindowTitle(_("Preferences")), _("Ok"), PrefOk, wHide, TRUE, NULL, 0, OptionDlgUpdate );
		LoadDstFmtList();
	}
	ParamLoadControls( &prefPG );
	displayUnits = units;
	wShow( prefW );
}


EXPORT addButtonCallBack_t PrefInit( void )
{
	ParamRegister( &prefPG );
	if (connectAngle < 1.0)
		connectAngle = 1.0;
	if (connectDistance < 0.1)
		connectDistance = 0.1;
	if (minLength < 0.1)
		minLength = 0.1;
	return &DoPref;
}


EXPORT long GetDistanceFormat( void )
{
	 while ( dstFmts[units][distanceFormatInx].name == NULL )
		distanceFormatInx--;
	 return dstFmts[units][distanceFormatInx].fmt;
}

/*****************************************************************************
 *
 *  Color
 *
 */

static wWin_p colorW;

static paramData_t colorPLs[] = {
	{ PD_COLORLIST, &snapGridColor, "snapgrid", PDO_NOPSHUPD, NULL, N_("Snap Grid"), 0, (void*)(CHANGE_GRID) },
	{ PD_COLORLIST, &markerColor, "marker", PDO_NOPSHUPD, NULL, N_("Marker"), 0, (void*)(CHANGE_GRID) },
	{ PD_COLORLIST, &borderColor, "border", PDO_NOPSHUPD, NULL, N_("Border"), 0, (void*)(CHANGE_MAIN) },
	{ PD_COLORLIST, &crossMajorColor, "crossmajor", PDO_NOPSHUPD, NULL, N_("Primary Axis"), 0, 0 },
	{ PD_COLORLIST, &crossMinorColor, "crossminor", PDO_NOPSHUPD, NULL, N_("Secondary Axis"), 0, 0 },
	{ PD_COLORLIST, &normalColor, "normal", PDO_NOPSHUPD, NULL, N_("Normal Track"), 0, (void*)(CHANGE_MAIN|CHANGE_PARAMS) },
	{ PD_COLORLIST, &selectedColor, "selected", PDO_NOPSHUPD, NULL, N_("Selected Track"), 0, (void*)(CHANGE_MAIN) },
	{ PD_COLORLIST, &profilePathColor, "profile", PDO_NOPSHUPD, NULL, N_("Profile Path"), 0, (void*)(CHANGE_MAIN) },
	{ PD_COLORLIST, &exceptionColor, "exception", PDO_NOPSHUPD, NULL, N_("Exception Track"), 0, (void*)(CHANGE_MAIN) },
	{ PD_COLORLIST, &tieColor, "tie", PDO_NOPSHUPD, NULL, N_("Track Ties"), 0, (void*)(CHANGE_MAIN) } };
static paramGroup_t colorPG = { "rgbcolor", PGO_RECORD|PGO_PREFGROUP, colorPLs, sizeof colorPLs/sizeof colorPLs[0] };



static void ColorOk( void * junk )
{
	long changes;
	changes = GetChanges( &colorPG );
	wHide( colorW );
	if ( (changes&CHANGE_GRID) && GridIsVisible() )
		changes |= CHANGE_MAIN;
	DoChangeNotification( changes );
}


static void DoColor( void * junk )
{
	if (colorW == NULL)
		colorW = ParamCreateDialog( &colorPG, MakeWindowTitle(_("Color")), _("Ok"), ColorOk, wHide, TRUE, NULL, 0, NULL );
	ParamLoadControls( &colorPG );
	wShow( colorW );
}


EXPORT addButtonCallBack_t ColorInit( void )
{
	ParamRegister( &colorPG );
	return &DoColor;
}

