/** /file celev.c
 *  ELEVATION
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
#include <math.h>

#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "param.h"
#include "track.h"

static wWin_p elevW;

static long elevModeV;
static char elevStationV[STR_SIZE];
static DIST_T elevHeightV = 0.0;

static DIST_T elevOldValue;
static track_p elevTrk;
static EPINX_T elevEp;
static BOOL_T elevUndo = FALSE;

static char * elevModeLabels[] = { N_("None"), N_("Defined"), N_("Hidden"),
	N_("Computed"), N_("Grade"), N_("Station"), N_("Ignore"), NULL };
static paramFloatRange_t r_1000_1000 = { -1000, 1000 };

static paramData_t elevationPLs[] = {
#define I_MODE			(0)
	{ PD_RADIO, &elevModeV, "mode", 0, elevModeLabels, "" },
#define I_HEIGHT			(1)
	{ PD_FLOAT, &elevHeightV, "value", PDO_DIM|PDO_DLGNEWCOLUMN, &r_1000_1000 },
#define I_COMPUTED			(2)
	{ PD_MESSAGE, NULL, "computed", 0, (void*)80 },
#define I_GRADE			(3)
	{ PD_MESSAGE, NULL, "grade", 0, (void*)80 },
#define I_STATION			(4)
	{ PD_STRING, elevStationV, "station", PDO_DLGUNDERCMDBUTT, (void*)200 } };
static paramGroup_t elevationPG = { "elev", 0, elevationPLs, sizeof elevationPLs/sizeof elevationPLs[0] };


static void LayoutElevW(
		paramData_t * pd,
		int inx,
		wPos_t colX,
		wPos_t * x,
		wPos_t * y )
{
	static wPos_t h = 0;
	switch ( inx ) {
	case I_HEIGHT:
		h = wControlGetHeight( elevationPLs[I_MODE].control )/((sizeof elevModeLabels/sizeof elevModeLabels[0])-1);
#ifndef WINDOWS
		h += 3;
#endif
		*y = DlgSepTop+h+h/2;
		break;
	case I_COMPUTED:
	case I_GRADE:
	case I_STATION:
		*y = DlgSepTop+h*(inx+1);
		break;
	}
}


static int GetElevMode( void )
{
	int mode;
	int newMode;
	static int modeMap[7] = { ELEV_NONE, ELEV_DEF|ELEV_VISIBLE, ELEV_DEF, ELEV_COMP|ELEV_VISIBLE, ELEV_GRADE|ELEV_VISIBLE, ELEV_STATION|ELEV_VISIBLE, ELEV_IGNORE };
	mode = (int)elevModeV;
	if (mode<0||mode>=7)
		return -1;
	newMode = modeMap[mode];
	return newMode;
}


#ifdef LATER
static void DoElevRadio( long mode, void * context )
{
	if ( mode < 0 || mode >= 7 )
		return;
#ifdef ELEVM
	ParamLoadMessage( elevMessageM, "" );
#endif
	ParamControlActive( &elevationPG, I_HEIGHT, FALSE );
	ParamControlActive( &elevationPG, I_STATION, FALSE );
	switch ( mode ) {
	case 0:
		break;
	case 1:
	case 2:
		ParamControlActive( &elevationPG, I_HEIGHT, TRUE );
		break;
	case 3:
	case 4:
#ifdef OLDELEV
		if ( !( (rc0 == FDE_DEF && rc1 == FDE_DEF) ||
				(rc0 == FDE_DEF && rc1 == FDE_END) ||
				(rc0 == FDE_END && rc1 == FDE_DEF) ) ) {
			ParamLoadMessage( &elevationPG, I_MSG, _("There are no reachable Defined Elevations") );
			ParamLoadControl( &elevationPG, I_MODE );
			return;
		}
#endif
		break;
	case 5:
		wControlActive( (wControl_p)elevStationS, TRUE );
		break;
	}
	elevModeV = mode;
	DoElevUpdate( NULL, 1, NULL );
}
#endif


static void DoElevUpdate( paramGroup_p pg, int inx, void * valueP )
{
	int oldMode, newMode;
	coOrd pos;
	DIST_T elevNewValue, elevOldValue, diff;
	DIST_T radius;

	if ( inx == 0 ) {
		long mode = *(long*)valueP;
		if ( mode < 0 || mode >= 7 )
			return;
#ifdef ELEVM
		ParamLoadMessage( elevMessageM, "" );
#endif
		ParamControlActive( &elevationPG, I_HEIGHT, FALSE );
		ParamControlActive( &elevationPG, I_STATION, FALSE );
		switch ( mode ) {
		case 0:
			break;
		case 1:
		case 2:
			ParamControlActive( &elevationPG, I_HEIGHT, TRUE );
			break;
		case 3:
		case 4:
#ifdef OLDELEV
			if ( !( (rc0 == FDE_DEF && rc1 == FDE_DEF) ||
					(rc0 == FDE_DEF && rc1 == FDE_END) ||
					(rc0 == FDE_END && rc1 == FDE_DEF) ) ) {
				ParamLoadMessage( &elevationPG, I_MSG, _("There are no reachable Defined Elevations") );
				ParamLoadControl( &elevationPG, I_MODE );
				return;
			}
#endif
			break;
		case 5:
			ParamControlActive( &elevationPG, I_STATION, TRUE );
			break;
		}
		elevModeV = mode;
	}
	ParamLoadData( &elevationPG );
	newMode = GetElevMode();
	if (newMode == -1)
		return;
	if (elevTrk == NULL)
		return;
	oldMode = GetTrkEndElevUnmaskedMode( elevTrk, elevEp );
	elevNewValue = 0.0;
	if ((newMode&ELEV_MASK) == ELEV_DEF)
		elevNewValue = elevHeightV;
	if (oldMode == newMode) {
		if ((newMode&ELEV_MASK) == ELEV_DEF) {
			elevOldValue = GetTrkEndElevHeight( elevTrk, elevEp );
			diff = fabs( elevOldValue-elevNewValue );
			if ( diff < 0.02 )
				return;
		} else if ((newMode&ELEV_MASK) == ELEV_STATION) {
			if ( strcmp(elevStationV, GetTrkEndElevStation( elevTrk, elevEp ) ) == 0)
				return;
		} else {
			return;
		}
	}
	if (elevUndo == FALSE) {
		UndoStart( _("Set Elevation"), "Set Elevation" );
		elevUndo = TRUE;
	}
	pos = GetTrkEndPos( elevTrk, elevEp );
	radius = 0.05*mainD.scale;
	if ( radius < trackGauge/2.0 )
		radius = trackGauge/2.0;
	if ( (oldMode&ELEV_MASK)==ELEV_DEF || (oldMode&ELEV_MASK)==ELEV_IGNORE )
		DrawFillCircle( &tempD, pos, radius,
			((oldMode&ELEV_MASK)==ELEV_DEF?elevColorDefined:elevColorIgnore));
	HilightSelectedEndPt(FALSE, elevTrk, elevEp);
	UpdateTrkEndElev( elevTrk, elevEp, newMode, elevNewValue, elevStationV );
	HilightSelectedEndPt(TRUE, elevTrk, elevEp);
	if ( (newMode&ELEV_MASK)==ELEV_DEF || (newMode&ELEV_MASK)==ELEV_IGNORE )
		DrawFillCircle( &tempD, pos, radius,
			((newMode&ELEV_MASK)==ELEV_DEF?elevColorDefined:elevColorIgnore));
}


static void DoElevDone( void * arg )
{
	DoElevUpdate( NULL, 1, NULL );
	HilightElevations( FALSE );
	HilightSelectedEndPt( FALSE, elevTrk, elevEp );
	elevTrk = NULL;
	Reset();
}


static void DoElevHilight( void * junk )
{
	HilightElevations( TRUE );
}


static void ElevSelect( track_p trk, EPINX_T ep )
{
	int mode;
	DIST_T elevX, grade, elev, dist;
	long radio;
	BOOL_T computedOk;
	BOOL_T gradeOk = TRUE;
	track_p trk1;
	EPINX_T ep1;

	DoElevUpdate( NULL, 1, NULL );
	elevOldValue = 0.0;
	elevHeightV = 0.0;
	elevStationV[0] = 0;
	HilightSelectedEndPt(FALSE, elevTrk, elevEp);
	elevTrk = trk;
	elevEp = ep;
	mode = GetTrkEndElevUnmaskedMode( trk, ep );
	ParamLoadControls( &elevationPG );
	ParamControlActive( &elevationPG, I_MODE, TRUE );
	ParamControlActive( &elevationPG, I_HEIGHT, FALSE );
	ParamControlActive( &elevationPG, I_STATION, FALSE );
	ParamLoadMessage( &elevationPG, I_COMPUTED, "" );
	ParamLoadMessage( &elevationPG, I_GRADE, "" );
	switch (mode & ELEV_MASK) {
	case ELEV_NONE:
		radio = 0;
		break;
	case ELEV_DEF:
		if ( mode & ELEV_VISIBLE )
			radio = 1;
		else
			radio = 2;
		elevHeightV = GetTrkEndElevHeight(trk,ep);
		elevOldValue = elevHeightV;
		ParamLoadControl( &elevationPG, I_HEIGHT );
		ParamControlActive( &elevationPG, I_HEIGHT, TRUE );
		break;
	case ELEV_COMP:
		radio = 3;
		break;
	case ELEV_GRADE:
		radio = 4;
		break;
	case ELEV_STATION:
		radio = 5;
		strcpy( elevStationV, GetTrkEndElevStation(trk,ep) );
		ParamLoadControl( &elevationPG, I_STATION );
		ParamControlActive( &elevationPG, I_STATION, TRUE );
		break;
	case ELEV_IGNORE:
		radio = 6;
		break;
	default:
		radio = 0;
	}
	elevModeV = radio;
	ParamLoadControl( &elevationPG, I_MODE );
#ifdef OLDELEV
if (oldElevationEvaluation) {
	int dir;
	ANGLE_T a;
	int rc0, rc1;
	DIST_T elev0, elev1, dist0, dist1;
	a = GetTrkEndAngle( trk, ep );
	dir = ( a > 270 || a < 90 );
	rc0 = FindDefinedElev( trk, ep, dir, FALSE, &elev0, &dist0 );
	rc1 = FindDefinedElev( trk, ep, 1-dir, FALSE, &elev1, &dist1 );
	if ( rc0 == FDE_DEF ) {
		sprintf( message, _("Elev = %s"), FormatDistance(elev0) );
		ParamLoadMessage( elev1ElevM, message );
		sprintf( message, _("Dist = %s"), FormatDistance(dist0) );
		ParamLoadMessage( elev1DistM, message );
#ifdef LATER
		if (dist0 > 0.1)
			sprintf( message, "%0.1f%%", elev0/dist0 );
		else
			sprintf( message, _("Undefined") );
		ParamLoadMessage( elev1GradeM, message );
#endif
	} else {
		ParamLoadMessage( elev1ElevM, "" );
		ParamLoadMessage( elev1DistM, "" );
		/*ParamLoadMessage( elev1GradeM, "" );*/
	}
	if ( rc1 == FDE_DEF ) {
		sprintf( message, _("Elev = %s"), FormatDistance(elev1) );
		ParamLoadMessage( elev2ElevM, message );
		sprintf( message, _("Dist = %s"), FormatDistance(dist1) );
		ParamLoadMessage( elev2DistM, message );
#ifdef LATER
		if (dist1 > 0.1)
			sprintf( message, "%0.1f%%", elev1/dist1 );
		else
			sprintf( message, _("Undefined") );
		ParamLoadMessage( elev2GradeM, message );
#endif
	} else {
		ParamLoadMessage( elev2ElevM, "" );
		ParamLoadMessage( elev2DistM, "" );
		/*ParamLoadMessage( elev2GradeM, "" );*/
	}
	computedOk = TRUE;
	if (rc0 == FDE_DEF && rc1 == FDE_DEF) {
		grade = (elev1-elev0)/(dist0+dist1);
		elevX = elev0 + grade*dist0;
	} else if (rc0 == FDE_DEF && rc1 == FDE_END) {
		grade = 0.0;
		elevX = elev0;
	} else if (rc0 == FDE_END && rc1 == FDE_DEF) {
		elevX = elev1;
		grade = 0.0;
	} else {
		gradeOk = FALSE;
		computedOk = FALSE;
	}
} else {
#endif
	gradeOk = ComputeElev( trk, ep, FALSE, &elevX, &grade );
	computedOk = TRUE;
#ifdef OLDELEV
}
#endif
	if (oldElevationEvaluation || computedOk) {
		sprintf( message, "%0.2f%s", PutDim( elevX ), (units==UNITS_METRIC?"cm":"\"") );
		ParamLoadMessage( &elevationPG, I_COMPUTED, message );
		if (gradeOk) {
			sprintf( message, "%0.1f%%", fabs(grade*100) );
		} else {
			if ( EndPtIsDefinedElev(trk,ep) ) {
				elev = GetElevation(trk);
				dist = GetTrkLength(trk,ep,-1);
				if (dist>0.1)
					sprintf( message, "%0.1f%%", fabs((elev-elevX)/dist)*100.0 );
				else
					sprintf( message, _("Undefined") );
				if ( (trk1=GetTrkEndTrk(trk,ep)) && (ep1=GetEndPtConnectedToMe(trk1,trk))>=0 ) {
					elev = GetElevation(trk1);
					dist = GetTrkLength(trk1,ep1,-1);
					if (dist>0.1)
						sprintf( message+strlen(message), " - %0.1f%%", fabs((elev-elevX)/dist)*100.0 );
					else
						sprintf( message+strlen(message), " - %s", _("Undefined") );
				}
			} else {
				strcpy( message, _("Undefined") );
			}
		}
		ParamLoadMessage( &elevationPG, I_GRADE, message );
		if ( (mode&ELEV_MASK)!=ELEV_DEF ) {
			elevHeightV = elevX;
			ParamLoadControl( &elevationPG, I_HEIGHT );
		}
	}
	HilightSelectedEndPt(TRUE, elevTrk, elevEp);
}


static STATUS_T CmdElevation( wAction_t action, coOrd pos )
{
	track_p trk0, trk1;
	EPINX_T ep0;
	int oldTrackCount;

	switch (action) {
	case C_START:
		if ( elevW == NULL )
			elevW = ParamCreateDialog( &elevationPG, MakeWindowTitle(_("Elevation")), _("Done"), DoElevDone, NULL, TRUE, LayoutElevW, 0, DoElevUpdate );
		elevModeV = 0;
		elevHeightV = 0.0;
		elevStationV[0] = 0;
		ParamLoadControls( &elevationPG );
		ParamGroupRecord( &elevationPG );
		wShow( elevW );
		ParamControlActive( &elevationPG, I_MODE, FALSE );
		ParamControlActive( &elevationPG, I_HEIGHT, FALSE );
		ParamControlActive( &elevationPG, I_STATION, FALSE );
		ParamLoadMessage( &elevationPG, I_COMPUTED, "" );
		ParamLoadMessage( &elevationPG, I_GRADE, "" );
		InfoMessage( _("Select End-Point") );
		HilightElevations( TRUE );
		elevTrk = NULL;
		elevUndo = FALSE;
		return C_CONTINUE;
	case C_RDOWN:
	case C_RMOVE:
	case C_RUP:
		CmdMoveDescription( action-C_RDOWN+C_DOWN, pos );
		return C_CONTINUE;
	case C_LCLICK:
		if ((trk0 = OnTrack( &pos, TRUE, TRUE )) == NULL) {
			return C_CONTINUE;
		}
		if ( (MyGetKeyState()&WKEY_SHIFT) ) {
			ep0 = PickEndPoint( pos, trk0 );
			UndoStart( _("Split Track"), "SplitTrack( T%d[%d] )", GetTrkIndex(trk0), ep0 );
			oldTrackCount = trackCount;
			if (!SplitTrack( trk0, pos, ep0, &trk1, FALSE ))
				return C_CONTINUE;
			ElevSelect( trk0, ep0 );
			UndoEnd();
			elevUndo = FALSE;
		} else {
			ep0 = PickEndPoint( pos, trk0 );
			ElevSelect( trk0, ep0 );
			return C_CONTINUE;
		}
		return C_CONTINUE;
	case C_OK:
		DoElevDone(NULL);
		return C_TERMINATE;
	case C_CANCEL:
		HilightElevations( FALSE );
		HilightSelectedEndPt( FALSE, elevTrk, elevEp );
		elevTrk = NULL;
		wHide( elevW );
		return C_TERMINATE;
	case C_REDRAW:
		DoElevHilight( NULL );
		HilightSelectedEndPt( TRUE, elevTrk, elevEp );
		return C_CONTINUE;
	}
	return C_CONTINUE;
}




#include "bitmaps/elev.xpm"

EXPORT void InitCmdElevation( wMenu_p menu )
{
	ParamRegister( &elevationPG );
	AddMenuButton( menu, CmdElevation, "cmdElevation", _("Elevation"), wIconCreatePixMap(elev_xpm), LEVEL0_50, IC_POPUP|IC_LCLICK, ACCL_ELEVATION, NULL );
}

