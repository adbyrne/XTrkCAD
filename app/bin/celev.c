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
#include <string.h>

#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "param.h"
#include "track.h"
#include "ccurve.h"
#include "utility.h"

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
	{ PD_STRING, elevStationV, "station", PDO_DLGUNDERCMDBUTT|PDO_STRINGLIMITLENGTH, (void*)200, NULL, 0, 0, sizeof(elevStationV)} };
static paramGroup_t elevationPG = { "elev", 0, elevationPLs, sizeof elevationPLs/sizeof elevationPLs[0] };

static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)

static void CreateSquareAnchor(coOrd p) {
	DIST_T d = tempD.scale*0.25;
	int i = anchors_da.cnt;
	DYNARR_SET(trkSeg_t,anchors_da,i+4);
	for (int j =0; j<4;j++) {
		anchors(i+j).type = SEG_STRLIN;
		anchors(i+j).color = wDrawColorBlue;
		anchors(i+j).width = 0;
	}
	anchors(i).u.l.pos[0].x = anchors(i+2).u.l.pos[1].x =
	anchors(i+3).u.l.pos[0].x = anchors(i+3).u.l.pos[1].x = p.x-d/2;

	anchors(i).u.l.pos[0].y = anchors(i).u.l.pos[1].y =
	anchors(i+1).u.l.pos[0].y =	anchors(i+3).u.l.pos[1].y = p.y-d/2;

	anchors(i).u.l.pos[1].x =
	anchors(i+1).u.l.pos[0].x = anchors(i+1).u.l.pos[1].x =
	anchors(i+2).u.l.pos[0].x = p.x+d/2;

	anchors(i+1).u.l.pos[1].y =
	anchors(i+2).u.l.pos[0].y = anchors(i+2).u.l.pos[1].y =
	anchors(i+3).u.l.pos[0].y = p.y+d/2;
}

static void CreateEndAnchor(coOrd p, wBool_t lock) {
	DIST_T d = tempD.scale*0.15;

	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = lock?SEG_FILCRCL:SEG_CRVLIN;
	anchors(i).color = wDrawColorBlue;
	anchors(i).u.c.center = p;
	anchors(i).u.c.radius = d/2;
	anchors(i).u.c.a0 = 0.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).width = 0;
}

static void CreateSplitAnchor(coOrd pos, track_p t) {
	DIST_T d = tempD.scale*0.1;
	DIST_T w = tempD.scale/tempD.dpi*4;
	int i;
	ANGLE_T a = NormalizeAngle(GetAngleAtPoint(t,pos,NULL,NULL)+90.0);
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	i = anchors_da.cnt-1;
	anchors(i).type = SEG_STRLIN;
	anchors(i).color = wDrawColorBlue;
	Translate(&anchors(i).u.l.pos[0],pos,a,GetTrkGauge(t));
	Translate(&anchors(i).u.l.pos[1],pos,a,-GetTrkGauge(t));
	anchors(i).width = w;

}


void static CreateMoveAnchor(coOrd pos) {
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pos,0,TRUE,wDrawColorBlue);
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pos,90,TRUE,wDrawColorBlue);
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	CreateSquareAnchor(pos);
}

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



static void DoElevUpdate( paramGroup_p pg, int inx, void * valueP )
{
	int oldMode, newMode;
	DIST_T elevNewValue, elevOldValue, diff;

	if ( inx == 0 ) {
		long mode = *(long*)valueP;
		if ( mode < 0 || mode >= 7 )
			return;
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
	UpdateTrkEndElev( elevTrk, elevEp, newMode, elevNewValue, elevStationV );
	TempRedraw(); // DoElevUpdate
}


static void DoElevDone( void * arg )
{
	DoElevUpdate( NULL, 1, NULL );
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
	gradeOk = ComputeElev( trk, ep, FALSE, &elevX, &grade, TRUE );
	computedOk = TRUE;
	if (oldElevationEvaluation || computedOk) {
		sprintf( message, "%0.2f%s", round(PutDim( elevX )*100.0)/100.0, (units==UNITS_METRIC?"cm":"\"") );
		ParamLoadMessage( &elevationPG, I_COMPUTED, message );
		if (gradeOk) {
			sprintf( message, "%0.1f%%", fabs(round(grade*1000.0)/10.0) );
		} else {
			if ( EndPtIsDefinedElev(trk,ep) ) {
				elev = GetElevation(trk);
				dist = GetTrkLength(trk,ep,-1);
				if (dist>0.1)
					sprintf( message, "%0.1f%%", fabs(round((elev-elevX)/dist)*1000.0)/10.0 );
				else
					sprintf( message, _("Undefined") );
				if ( (trk1=GetTrkEndTrk(trk,ep)) && (ep1=GetEndPtConnectedToMe(trk1,trk))>=0 ) {
					elev = GetElevation(trk1);
					dist = GetTrkLength(trk1,ep1,-1);
					if (dist>0.1)
						sprintf( message+strlen(message), " - %0.1f%%", fabs(round((elev-elevX)/dist)*1000.0)/10.0 );
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
	wShow(elevW);
}

static BOOL_T GetPointElev(track_p trk, coOrd pos, DIST_T * height) {
	DIST_T len, len1, elev0, elev1, dist0, dist1;
	if ( IsTrack( trk ) && GetTrkEndPtCnt(trk) == 2 ) {
		dist0 = FindDistance(pos,GetTrkEndPos(trk,0));
		dist1 = FindDistance(pos,GetTrkEndPos(trk,1));
		if (EndPtIsDefinedElev(trk,0))
			elev0 = GetTrkEndElevHeight(trk,0);
		else {
			if (!GetTrkEndElevCachedHeight(trk,0,&elev0,&len)) {
				if (GetTrkLength( trk, 0, 1 )<0.1) return FALSE;
				ComputeElev( trk, 0, FALSE, &elev0, NULL, TRUE );
			}
		}
		if (EndPtIsDefinedElev(trk,1))
			elev1 = GetTrkEndElevHeight(trk,1);
		else {
			if (!GetTrkEndElevCachedHeight(trk,1,&elev1,&len1)) {
				if (GetTrkLength( trk, 0, 1 )<0.1) return FALSE;
				ComputeElev( trk, 0, FALSE, &elev0, NULL, TRUE );
			}
		}
		if (dist1+dist0 < 0.1) {
			*height = elev0;
			return TRUE;
		}
		*height = ((elev1-elev0)*(dist0/(dist0+dist1)))+elev0;
		return TRUE;
	} else if (GetTrkEndPtCnt(trk) == 1 && GetTrkEndElevCachedHeight(trk,0,&elev0,&len)) {
		*height = elev0;
		return TRUE;
	}
	return FALSE;
}


static STATUS_T CmdElevation( wAction_t action, coOrd pos )
{
	track_p trk0, trk1;
	EPINX_T ep0;
	int oldTrackCount;

	switch (action) {
	case C_START:
		if ( elevW == NULL )
			elevW = ParamCreateDialog( &elevationPG, MakeWindowTitle(_("Elevation")), _("Done"), DoElevDone, wHide, TRUE, LayoutElevW, 0, DoElevUpdate );
		elevModeV = 0;
		elevHeightV = 0.0;
		elevStationV[0] = 0;
		ParamLoadControls( &elevationPG );
		ParamGroupRecord( &elevationPG );
		//wShow( elevW );
		ParamControlActive( &elevationPG, I_MODE, FALSE );
		ParamControlActive( &elevationPG, I_HEIGHT, FALSE );
		ParamControlActive( &elevationPG, I_STATION, FALSE );
		ParamLoadMessage( &elevationPG, I_COMPUTED, "" );
		ParamLoadMessage( &elevationPG, I_GRADE, "" );
		InfoMessage( _("Click on end, +Shift to split, +Ctrl to move description") );
		elevTrk = NULL;
		elevUndo = FALSE;
		CmdMoveDescription( action, pos );
		TempRedraw(); // CmdElevation C_START
		return C_CONTINUE;
	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (MyGetKeyState()&WKEY_CTRL) {
			commandContext = (void*) 1;        //Just end points
			CmdMoveDescription( action, pos );
			return C_CONTINUE;
		}
		BOOL_T xing = FALSE;
		coOrd p0 = pos, p2=pos;
		if ((trk0 = OnTrack2(&p0,FALSE, TRUE, FALSE, NULL)) != NULL) {
			EPINX_T ep0 = 0, ep1 = 1;
			DIST_T elev0, elev1;
			if (GetTrkEndPtCnt(trk0) == 2) {
				if (!GetPointElev(trk0,p0,&elev0)) {
					InfoMessage( _("Move to end or track crossing +Shift to split") );
					return C_CONTINUE;
				}
			} else {
				InfoMessage( _("Move to end or track crossing") );
				return C_CONTINUE;
			}
			if ((trk1 = OnTrack2(&p2,FALSE, TRUE, FALSE, trk0)) != NULL) {
				if (IsClose(FindDistance(p0,p2))) {
					if (GetEndPtConnectedToMe(trk0,trk1) == -1) {	//Not simply connected to each other!!!
						if (GetTrkEndPtCnt(trk1) == 2) {
							if (GetPointElev(trk1,p2,&elev1)) {
								if (MyGetKeyState()&WKEY_SHIFT) {
									InfoMessage (_("Crossing - First %0.3f, Second %0.3f, Clearance %0.3f - Click to Split"), PutDim(elev0), PutDim(elev1), PutDim(fabs(elev0-elev1)));
								} else
									InfoMessage (_("Crossing - First %0.3f, Second %0.3f, Clearance %0.3f"), PutDim(elev0), PutDim(elev1), PutDim(fabs(elev0-elev1)));
							}
							CreateSquareAnchor(p2);
							return C_CONTINUE;
						}
					}
				}
			}
			if ((ep0 = PickEndPoint( p0, trk0 )) != -1)  {
				if (IsClose(FindDistance(GetTrkEndPos(trk0,ep0),pos))) {
					CreateEndAnchor(GetTrkEndPos(trk0,ep0),FALSE);
					InfoMessage (_("Track End elevation %0.3f"), PutDim(elev0));
				} else if ((MyGetKeyState()&WKEY_SHIFT) && QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)
						&& !(QueryTrack(trk0,Q_IS_TURNOUT))) {
					InfoMessage( _("Click to split here - elevation %0.3f"), PutDim(elev0));
					CreateSplitAnchor(p0,trk0);
				} else {
					InfoMessage( _("Track Point elevation %0.3f"), PutDim(elev0));
					CreateEndAnchor(p0,TRUE);
				}
			} else InfoMessage( _("Click on end, +Shift to split, +Ctrl to move description") );
		} else
			InfoMessage( _("Click on end, +Shift to split, +Ctrl to move description") );
		return C_CONTINUE;
	case C_DOWN:
	case C_MOVE:
	case C_UP:
		if (MyGetKeyState()&WKEY_CTRL) {
			commandContext = (void*) 1;        //Just end points
			CmdMoveDescription( action, pos );
			DYNARR_RESET(trkSeg_t,anchors_da);
			elevTrk = NULL;
			return C_CONTINUE;
		}
		/*no break*/
	case C_LCLICK:
		;
		p0= pos;
		if ((trk0 = OnTrack( &p0, TRUE, TRUE )) == NULL) {
			wHide(elevW);
			elevTrk = NULL;
			InfoMessage( _("Click on end, +Shift to split, +Ctrl to move description") );
		} else {
			ep0 = PickEndPoint( p0, trk0 );
			if (IsClose(FindDistance(GetTrkEndPos(trk0,ep0),pos))) {
				InfoMessage( _("Point selected!") );
				ElevSelect( trk0, ep0 );
			} else if ( (MyGetKeyState()&WKEY_SHIFT) ) {
				UndoStart( _("Split track"), "SplitTrack( T%d[%d] )", GetTrkIndex(trk0), ep0 );
				oldTrackCount = trackCount;
				if (!QueryTrack(trk0,Q_IS_TURNOUT) &&
					!SplitTrack( trk0, p0, ep0, &trk1, FALSE ))
					return C_CONTINUE;
				InfoMessage( _("Track split!") );
				ElevSelect( trk0, ep0 );
				UndoEnd();
				elevUndo = FALSE;
			}
		}
		DYNARR_RESET(trkSeg_t,anchors_da);
		return C_CONTINUE;
	case C_OK:
		DoElevDone(NULL);
		InfoMessage( "" );
		return C_TERMINATE;
	case C_CANCEL:
		elevTrk = NULL;
		wHide( elevW );
		InfoMessage( "" );
		return C_TERMINATE;
	case C_REDRAW:
		DoElevHilight( NULL );
		HilightSelectedEndPt( TRUE, elevTrk, elevEp );
		if (anchors_da.cnt)
					DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		CmdMoveDescription( action, pos );
		return C_CONTINUE;
	}
	return C_CONTINUE;
}




#include "bitmaps/elev.xpm"

EXPORT void InitCmdElevation( wMenu_p menu )
{
	ParamRegister( &elevationPG );
	AddMenuButton( menu, CmdElevation, "cmdElevation", _("Elevation"), wIconCreatePixMap(elev_xpm), LEVEL0_50, IC_POPUP|IC_LCLICK|IC_RCLICK|IC_WANT_MOVE, ACCL_ELEVATION, NULL );
}

