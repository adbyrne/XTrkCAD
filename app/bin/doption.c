/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/doption.c,v 1.2 2006-02-22 19:20:10 m_fischer Exp $
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
static paramFloatRange_t r1_10000 = { 1, 10000 };
static paramFloatRange_t r0_90 = { 0, 90 };
static paramFloatRange_t r0_180 = { 0, 180 };
static paramFloatRange_t r1_9999999 = { 1, 9999999 };

static void UpdatePrefD( void );

EXPORT long enableBalloonHelp = 1;

static long GetChanges(
		paramGroup_p pg )
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
	} else if ( pg->paramPtr[inx].valueP == &minTrackRadius ) {
		sprintf( message, "minTrackRadius-%s", curScaleName );
		wPrefSetFloat( "misc", message, minTrackRadius );
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
 * Layout Dialog
 *
 */

static wWin_p layoutW;
static coOrd newSize;

static paramData_t layoutPLs[] = {
	{ PD_FLOAT, &newSize.x, "roomsizeX", PDO_NOPREF|PDO_DIM|PDO_NOPSHUPD|PDO_DRAW, &r1_9999999, "Room Width", 0, (void*)(CHANGE_MAIN|CHANGE_MAP) },
	{ PD_FLOAT, &newSize.y, "roomsizeY", PDO_NOPREF|PDO_DIM|PDO_NOPSHUPD|PDO_DRAW|PDO_DLGHORZ, &r1_9999999, "Height", 0, (void*)(CHANGE_MAIN|CHANGE_MAP) },
	{ PD_STRING, &Title1, "title1", PDO_NOPSHUPD, NULL, "Title" },
	{ PD_STRING, &Title2, "title2", PDO_NOPSHUPD, NULL, "" },
	{ PD_DROPLIST, &curScaleDescInx, "scale", PDO_NOPREF|PDO_NOPSHUPD|PDO_NORECORD|PDO_NOUPDACT, (void *)120, "Scale / Gauge", 0, (void*)(CHANGE_SCALE) },
	{ PD_DROPLIST, &curGaugeInx, "gauge", PDO_NOPREF |PDO_NOPSHUPD|PDO_NORECORD|PDO_NOUPDACT|PDO_DLGHORZ, (void *)120, " /", 0, (void *)(CHANGE_SCALE) }};
	

static paramGroup_t layoutPG = { "layout", PGO_RECORD|PGO_PREFMISC, layoutPLs, sizeof layoutPLs/sizeof layoutPLs[0] };

static void LayoutDlgUpdate( paramGroup_p pg, int inx, void * valueP );


static void LayoutOk( void * junk )
{
	long changes;
	
	changes = GetChanges( &layoutPG );
	
	/* [mf Nov. 15, 2005] Get the gauge/scale settings */
	if (changes & CHANGE_SCALE) {
		SetScaleGauge( curScaleDescInx, curGaugeInx );
	}
	/* [mf Nov. 15, 2005] end */
	
	if (changes & CHANGE_MAP) {
		SetRoomSize( newSize );
	}
	wHide( layoutW );
	DoChangeNotification(changes);
}


static void LayoutChange( long changes )
{
	if (changes & (CHANGE_SCALE|CHANGE_UNITS))
		if (layoutW != NULL && wWinIsVisible(layoutW) )
			ParamLoadControls( &layoutPG );
}


static void DoLayout( void * junk )
{
	newSize = mapD.size;
	if (layoutW == NULL) {
		layoutW = ParamCreateDialog( &layoutPG, MakeWindowTitle("Layout Options"), "Ok", LayoutOk, wHide, TRUE, NULL, 0, LayoutDlgUpdate );
		LoadScaleList( (wList_p)layoutPLs[4].control );
		LoadGaugeList( (wList_p)layoutPLs[5].control, curScaleDescInx ); /* set correct gauge list here */
	}
	ParamLoadControls( &layoutPG );
	wShow( layoutW );
}



EXPORT addButtonCallBack_t LayoutInit( void )
{
	ParamRegister( &layoutPG );
	RegisterChangeNotification( LayoutChange );
	return &DoLayout;
}

/* [mf Nov. 15, 2005] Catch changes done in the LayoutDialog */
static void
LayoutDlgUpdate( 
	paramGroup_p pg, 
	int inx,
	void * valueP )
{
	/* did the scale change ? */
	if( inx == 4 ) {
		LoadGaugeList( (wList_p)layoutPLs[5].control, *((int *)valueP) );
	}	
}

/* [mf Nov. 15, 2005] end */

/****************************************************************************
 *
 * Display Dialog
 *
 */

static wWin_p displayW;

static char * drawTunnelLabels[] = { "Hide", "Dash", "Normal", NULL };
static char * drawEndPtLabels3[] = { "None", "Turnouts", "All", NULL };
static char * drawEndPtLabels2[] = { "Off", "On", NULL };
static char *labelEnableLabels[] = { "Track Descriptions", "Lengths", "EndPt Elevations", "Track Elevations", "Cars", NULL };
static char * colorLayersLabels[] = { "Tracks", "Other", NULL };
static char * tiedrawLabels[] = { "None", "Outline", "Solid", NULL };
static char * enableBalloonHelpLabels[] = { "Enable", NULL };
static char * liveMapLabels[] = { "Live", NULL };
static char * preSelectLabels[] = { "Describe", "Select", NULL };
static char * listLabelsLabels[] = { "Manuf", "Part No", "Descr", NULL };
static char * hotBarLabelsLabels[] = { "Part No", "Descr", NULL };
static char * hideTrainsInTunnelsLabels[] = { "Hide", NULL };

static paramData_t displayPLs[] = {
	{ PD_RADIO, &drawTunnel, "tunnels", PDO_NOPSHUPD|PDO_DRAW, drawTunnelLabels, "Draw Tunnel", BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_RADIO, &drawEndPtV, "endpt", PDO_NOPSHUPD|PDO_DRAW, drawEndPtLabels3, "Draw EndPts", BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_TOGGLE, &labelEnable, "labelenable", PDO_NOPSHUPD, labelEnableLabels, "Label Enable", 0, (void*)(CHANGE_MAIN) },
	{ PD_LONG, &descriptionFontSize, "description-fontsize", PDO_NOPSHUPD, &i1_1000, "Label Font Size", 0, (void*)(CHANGE_MAIN) },
	{ PD_LONG, &labelScale, "labelscale", PDO_NOPSHUPD, &i0_64, "Label Scale", 0, (void*)(CHANGE_MAIN) },
	{ PD_TOGGLE, &layoutLabels, "layoutlabels", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT, listLabelsLabels, "Layout Labels", BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_TOGGLE, &colorLayers, "color-layers", PDO_NOPSHUPD|PDO_DRAW, colorLayersLabels, "Color Layers", BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_LONG, &twoRailScale, "tworailscale", PDO_NOPSHUPD, &i1_64, "Two Rail Scale", 0, (void*)(CHANGE_MAIN) },
	{ PD_RADIO, &tieDrawMode, "tiedraw", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT|PDO_DRAW, tiedrawLabels, "Draw Ties", BC_HORZ, (void*)(CHANGE_MAIN) },
	{ PD_LONG, &minGridSpacing, "mingridspacing", PDO_NOPSHUPD|PDO_DRAW, &i1_100, "Min Grid Spacing", 0, 0 },
	{ PD_TOGGLE, &enableBalloonHelp, "balloonhelp", PDO_NOPSHUPD, enableBalloonHelpLabels, "Balloon Help", BC_HORZ },
	{ PD_TOGGLE, &hotBarLabels, "hotbarlabels", PDO_NOPSHUPD, hotBarLabelsLabels, "Hot Bar Labels", BC_HORZ, (void*)(CHANGE_TOOLBAR) },
	{ PD_LONG, &mapScale, "mapscale", PDO_NOPSHUPD, &i1_256, "Map Scale", 0, (void*)(CHANGE_MAP) },
	{ PD_TOGGLE, &liveMap, "livemap", PDO_NOPSHUPD, liveMapLabels, "Live Map", BC_HORZ },
#define I_HOTBARLABELS	(14)
	{ PD_DROPLIST, &carHotbarModeInx, "carhotbarlabels", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT|PDO_LISTINDEX, (void*)250, "Car Labels", 0, (void*)CHANGE_SCALE },
	{ PD_TOGGLE, &hideTrainsInTunnels, "hideTrainsInTunnels", PDO_NOPSHUPD, hideTrainsInTunnelsLabels, "Trains On Hidden Track", BC_HORZ } };
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
		displayW = ParamCreateDialog( &displayPG, MakeWindowTitle("Display Options"), "Ok", DisplayOk, OptionDlgCancel, TRUE, NULL, 0, OptionDlgUpdate );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, "Proto", NULL, (void*)0x0002 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, "Proto/Manuf", NULL, (void*)0x0012 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, "Proto/Manuf/Part Number", NULL, (void*)0x0312 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, "Proto/Manuf/Partno/Item", NULL, (void*)0x4312 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, "Manuf/Proto", NULL, (void*)0x0021 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, "Manuf/Proto/Part Number", NULL, (void*)0x0321 );
		wListAddValue( (wList_p)displayPLs[I_HOTBARLABELS].control, "Manuf/Proto/Partno/Item", NULL, (void*)0x4321 );
	}
	ParamLoadControls( &displayPG );
	wShow( displayW );
#ifdef LATER
	DisplayChange( CHANGE_SCALE );
#endif
}


EXPORT addButtonCallBack_t DisplayInit( void )
{
	if (!bEnableFlex)
		displayPLs[1].winData = drawEndPtLabels2;
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

static char *moveQlabels[] = {
		"Normal",
		"Simple",
		"End-Points",
		NULL };
#ifdef HIDESELECTIONWINDOW
static char * hideSelectionWindowLabels[] = { "Hide", NULL };
#endif
static char * rightClickLabels[] = {
		"Normal: Command List, Shift: Command Options",
		"Normal: Command Options, Shift: Command List",
		NULL };

EXPORT paramData_t cmdoptPLs[] = {
	{ PD_RADIO, &quickMove, "move-quick", PDO_NOPSHUPD, moveQlabels, "Draw Moving Tracks", BC_HORZ },
	{ PD_FLOAT, &turntableAngle, "turntable-angle", PDO_NOPSHUPD, &r0_180, "Turntable Angle" },
	{ PD_RADIO, &preSelect, "preselect", PDO_NOPSHUPD, preSelectLabels, "Default Command", BC_HORZ },
#ifdef HIDESELECTIONWINDOW
	{ PD_TOGGLE, &hideSelectionWindow, PDO_NOPSHUPD, hideSelectionWindowLabels, "Hide Selection Window", BC_HORZ },
#endif
	{ PD_LONG, &maxCouplingSpeed, "coupling-speed-max", PDO_NOPSHUPD, &i10_100, "Max Coupling Speed", 0 },
	{ PD_RADIO, &rightClickMode, "rightclickmode", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT, rightClickLabels, "Right Click", 0 }
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
		cmdoptW = ParamCreateDialog( &cmdoptPG, MakeWindowTitle("Command Options"), "Ok", CmdoptOk, OptionDlgCancel, TRUE, NULL, 0, OptionDlgUpdate );
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
extern long trainPause;

static wIndex_t distanceFormatInx;
static char * unitsLabels[] = { "English", "Metric", NULL };
static char * angleSystemLabels[] = { "Polar", "Cartesian", NULL };
static char * autoPanLabels[] = { "", NULL };

static paramData_t prefPLs[] = {
	{ PD_LONG, &checkPtInterval, "checkpoint", PDO_NOPSHUPD|PDO_FILE, &i0_10000, "Check Point" },
	{ PD_RADIO, &units, "units", PDO_NOPSHUPD|PDO_NOUPDACT, unitsLabels, "Units", BC_HORZ, (void*)(CHANGE_MAIN|CHANGE_UNITS) },
#define I_DSTFMT		(2)
	{ PD_DROPLIST, &distanceFormatInx, "dstfmt", PDO_NOPSHUPD|PDO_LISTINDEX, (void*)150, "Length Format", 0, (void*)(CHANGE_MAIN|CHANGE_UNITS) },
	{ PD_RADIO, &angleSystem, "anglesystem", PDO_NOPSHUPD, angleSystemLabels, "Angles", BC_HORZ },
	{ PD_FLOAT, &minLength, "minlength", PDO_DIM|PDO_SMALLDIM|PDO_NOPSHUPD, &r0o1_1, "Minimum Track Length" },
	{ PD_FLOAT, &connectDistance, "connectdistance", PDO_DIM|PDO_SMALLDIM|PDO_NOPSHUPD, &r0o1_1, "Connection Distance", },
	{ PD_FLOAT, &connectAngle, "connectangle", PDO_NOPSHUPD, &r1_10, "Connection Angle" },
	{ PD_LONG, &dragPixels, "dragpixels", PDO_NOPSHUPD|PDO_DRAW, &r1_1000, "Drag Distance" },
	{ PD_LONG, &dragTimeout, "dragtimeout", PDO_NOPSHUPD|PDO_DRAW, &i1_1000, "Drag Timeout" },
	{ PD_TOGGLE, &autoPan, "autoPan", PDO_NOPSHUPD, autoPanLabels, "Auto Pan", BC_HORZ, NULL },
	{ PD_TOGGLE, &listLabels, "listlabels", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT, listLabelsLabels, "List Labels", BC_HORZ, (void*)(CHANGE_PARAMS) },
	{ PD_FLOAT, &minTrackRadius, "mintrackradius", PDO_DIM|PDO_NOPSHUPD|PDO_NOPREF, &r1_10000, "Minimum Track Radius", 0, (void*)(CHANGE_MAIN) },
		/*minTrackRadiusPrefS*/
	{ PD_FLOAT, &maxTrackGrade, "maxtrackgrade", PDO_NOPSHUPD, &r0_90 , "Maximum Track Grade", 0, (void*)(CHANGE_MAIN) },
	{ PD_LONG, &trainPause, "trainpause", PDO_NOPSHUPD, &i10_1000 , "Train Update Delay", 0, 0 } };
static paramGroup_t prefPG = { "pref", PGO_RECORD|PGO_PREFMISC, prefPLs, sizeof prefPLs/sizeof prefPLs[0] };


typedef struct {
		char * name;
		long fmt;
	} dstFmts_t;
static dstFmts_t englishDstFmts[] = {
		{ "999.999",			DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|3 },
		{ "999.99",				DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|2 },
		{ "999.9",				DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|1 },
		{ "999 7/8",			DISTFMT_FMT_NONE|DISTFMT_FRACT_FRC|3 },
		{ "999 63/64",			DISTFMT_FMT_NONE|DISTFMT_FRACT_FRC|6 },
		{ "999' 11.999\"",		DISTFMT_FMT_SHRT|DISTFMT_FRACT_NUM|3 },
		{ "999' 11.99\"",		DISTFMT_FMT_SHRT|DISTFMT_FRACT_NUM|2 },
		{ "999' 11.9\"",		DISTFMT_FMT_SHRT|DISTFMT_FRACT_NUM|1 },
		{ "999' 11 7/8\"",		DISTFMT_FMT_SHRT|DISTFMT_FRACT_FRC|3 },
		{ "999' 11 63/64\"",	DISTFMT_FMT_SHRT|DISTFMT_FRACT_FRC|6 },
		{ "999ft 11.999in",		DISTFMT_FMT_LONG|DISTFMT_FRACT_NUM|3 },
		{ "999ft 11.99in",		DISTFMT_FMT_LONG|DISTFMT_FRACT_NUM|2 },
		{ "999ft 11.9in",		DISTFMT_FMT_LONG|DISTFMT_FRACT_NUM|1 },
		{ "999ft 11 7/8in",		DISTFMT_FMT_LONG|DISTFMT_FRACT_FRC|3 },
		{ "999ft 11 63/64in",	DISTFMT_FMT_LONG|DISTFMT_FRACT_FRC|6 },
		{ NULL, 0 } };
static dstFmts_t metricDstFmts[] = {
		{ "999.999",			DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|3 },
		{ "999.99",				DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|2 },
		{ "999.9",				DISTFMT_FMT_NONE|DISTFMT_FRACT_NUM|1 },
		{ "999.999mm",			DISTFMT_FMT_MM|DISTFMT_FRACT_NUM|3 },
		{ "999.99mm",			DISTFMT_FMT_MM|DISTFMT_FRACT_NUM|2 },
		{ "999.9mm",			DISTFMT_FMT_MM|DISTFMT_FRACT_NUM|1 },
		{ "999.999cm",			DISTFMT_FMT_CM|DISTFMT_FRACT_NUM|3 },
		{ "999.99cm",			DISTFMT_FMT_CM|DISTFMT_FRACT_NUM|2 },
		{ "999.9cm",			DISTFMT_FMT_CM|DISTFMT_FRACT_NUM|1 },
		{ "999.999m",			DISTFMT_FMT_M|DISTFMT_FRACT_NUM|3 },
		{ "999.99m",			DISTFMT_FMT_M|DISTFMT_FRACT_NUM|2 },
		{ "999.9m",				DISTFMT_FMT_M|DISTFMT_FRACT_NUM|1 },
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 } };
static dstFmts_t *dstFmts[] = { englishDstFmts, metricDstFmts };

		
		
static void LoadDstFmtList( void )
{
	int inx;
	wListClear( (wList_p)prefPLs[I_DSTFMT].control );
	for ( inx=0; dstFmts[units][inx].name; inx++ )
		wListAddValue( (wList_p)prefPLs[I_DSTFMT].control, dstFmts[units][inx].name, NULL, (void*)dstFmts[units][inx].fmt );
}


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
	for ( inx = 0; inx<sizeof prefPLs/sizeof prefPLs[0]; inx++ ) {
		if ( (prefPLs[inx].option&PDO_DIM) ) {
			ParamLoadControl( &prefPG, inx );
#ifdef LATER
			val = wFloatGetValue( (wFloat_p)prefPLs[inx].control );
			if ( newUnits == UNITS_METRIC )
				val *= 2.54;
			else
				val /= 2.54;
			wFloatSetValue( (wFloat_p)prefPLs[inx].control, val );
#endif
		}
	}
	LoadDstFmtList();
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
		NoticeMessage2( 0, MSG_CONN_PARAMS_TOO_SMALL, "Ok", NULL ) ;
	}
	wHide( prefW );
	DoChangeNotification(changes);
}



static void DoPref( void * junk )
{
	if (prefW == NULL) {
		prefW = ParamCreateDialog( &prefPG, MakeWindowTitle("Preferences"), "Ok", PrefOk, wHide, TRUE, NULL, 0, OptionDlgUpdate );
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
	{ PD_COLORLIST, &snapGridColor, "snapgrid", PDO_NOPSHUPD, NULL, "Snap Grid", 0, (void*)(CHANGE_GRID) },
	{ PD_COLORLIST, &markerColor, "marker", PDO_NOPSHUPD, NULL, "Marker", 0, (void*)(CHANGE_GRID) },
	{ PD_COLORLIST, &borderColor, "border", PDO_NOPSHUPD, NULL, "Border", 0, (void*)(CHANGE_MAIN) },
	{ PD_COLORLIST, &crossMajorColor, "crossmajor", PDO_NOPSHUPD, NULL, "Primary Axis", 0, 0 },
	{ PD_COLORLIST, &crossMinorColor, "crossminor", PDO_NOPSHUPD, NULL, "Secondary Axis", 0, 0 },
	{ PD_COLORLIST, &normalColor, "normal", PDO_NOPSHUPD, NULL, "Normal Track", 0, (void*)(CHANGE_MAIN|CHANGE_PARAMS) },
	{ PD_COLORLIST, &selectedColor, "selected", PDO_NOPSHUPD, NULL, "Selected Track", 0, (void*)(CHANGE_MAIN) },
	{ PD_COLORLIST, &profilePathColor, "profile", PDO_NOPSHUPD, NULL, "Profile Path", 0, (void*)(CHANGE_MAIN) },
	{ PD_COLORLIST, &exceptionColor, "exception", PDO_NOPSHUPD, NULL, "Exception Track", 0, (void*)(CHANGE_MAIN) },
	{ PD_COLORLIST, &tieColor, "tie", PDO_NOPSHUPD, NULL, "Track Ties", 0, (void*)(CHANGE_MAIN) } };
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
		colorW = ParamCreateDialog( &colorPG, MakeWindowTitle("Color"), "Ok", ColorOk, wHide, TRUE, NULL, 0, NULL );
	ParamLoadControls( &colorPG );
	wShow( colorW );
}


EXPORT addButtonCallBack_t ColorInit( void )
{
	ParamRegister( &colorPG );
	return &DoColor;
}

