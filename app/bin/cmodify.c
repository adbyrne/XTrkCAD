/** \file cmodify.c
 * TRACK MODIFY
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

#include "cjoin.h"
#include "ccurve.h"
#include "cbezier.h"
#include "ccornu.h"
#include "cstraigh.h"
#include "cundo.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"
#include "drawgeom.h"
#include "common.h"
#include "layout.h"

static struct {
		track_p Trk;
		trackParams_t params;
		coOrd pos00, pos00x, pos01;
		ANGLE_T angle;
		curveData_t curveData;
		easementData_t jointD;
		DIST_T r1;
		BOOL_T valid;
		BOOL_T first;
		} Dex;

static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)

static int log_modify;
static BOOL_T modifyBezierMode;
static BOOL_T modifyCornuMode;
static BOOL_T modifyDrawMode;
static BOOL_T modifyRulerMode;


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

static void CreateCornuAnchor(coOrd p, wBool_t lock) {
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
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	i = anchors_da.cnt-1;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).color = wDrawColorBlue;
	anchors(i).u.c.center = p;
	anchors(i).u.c.radius = d;
	anchors(i).u.c.a0 = 0.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).width = 0;
}


static void CreateRadiusAnchor(coOrd p, ANGLE_T a, BOOL_T bi) {
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),p,a,bi,wDrawColorBlue);
}

/*
 * Call cbezier.c CmdBezModify to alter Bezier Track and Lines.
 * Picking a Bezier will allow control point(s) modifications until terminated with "Enter"
 */
static STATUS_T ModifyBezier(wAction_t action, coOrd pos) {
	STATUS_T rc = C_CONTINUE;
	if (Dex.Trk == NULL) return C_ERROR;   //No track picked yet!
	switch (action&0xFF) {
		case C_START:
		case C_DOWN:
		case C_MOVE:
		case C_UP:
		case C_OK:
		case C_TEXT:
			trackGauge = (IsTrack(Dex.Trk)?GetTrkGauge(Dex.Trk):0.0);
			rc = CmdBezModify(Dex.Trk, action, pos, trackGauge);
			break;
		case C_TERMINATE:
			rc = CmdBezModify(Dex.Trk, action, pos, trackGauge);
			Dex.Trk = NULL;
			modifyBezierMode = FALSE;
			break;
		case C_REDRAW:
			rc = CmdBezModify(Dex.Trk, action, pos, trackGauge);
			break;
	}
	return rc;
}

/*
 * Call ccornu.c CmdCornuModify to alter Cornu Track and Lines.
 * Picking a Cornu will allow end point(s) modifications until terminated with "Enter"
 */
static STATUS_T ModifyCornu(wAction_t action, coOrd pos) {
	STATUS_T rc = C_CONTINUE;
	if (Dex.Trk == NULL) return C_ERROR;   //No track picked yet!
	switch (action&0xFF) {
		case C_START:
		case C_DOWN:
		case C_MOVE:
		case C_UP:
		case C_OK:
		case C_TEXT:
			trackGauge = (IsTrack(Dex.Trk)?GetTrkGauge(Dex.Trk):0.0);
			rc = CmdCornuModify(Dex.Trk, action, pos, trackGauge);
			break;
		case C_TERMINATE:
			rc = CmdCornuModify(Dex.Trk, action, pos, trackGauge);
			Dex.Trk = NULL;
			modifyCornuMode = FALSE;
			break;
		case C_REDRAW:
			rc = CmdCornuModify(Dex.Trk, action, pos, trackGauge);
			break;
	}
	return rc;
}

/*
 * Picking a DRAW will allow point modifications until terminated with "Enter"
 */
static STATUS_T ModifyDraw(wAction_t action, coOrd pos) {
	STATUS_T rc = C_CONTINUE;
	if (Dex.Trk == NULL) return C_ERROR;   //No item picked yet!
	switch (action&0xFF) {
		case C_START:
		case C_DOWN:
		case C_MOVE:
		case C_UP:
			rc = ModifyTrack( Dex.Trk, action, pos );
			break;
		case C_OK:
		case C_TEXT:
			//Delete or '0' - continues
			if (action>>8 == 127 || action>>8 == 8 || 	// Del or backspace
					action>>8 == 'o' || action>>8 == 'p' ||
					action>>8 == 'l' || action>>8 == 'c' ||
					action>>8 == 'r' || action>>8 == 's' ||
					action>>8 == 'e' || action>>8 == 'f' ||
					action>>8 == 'v' ||
					(action>>8 >= 48 && action>>8 <= 52)) return ModifyTrack( Dex.Trk, action, pos );
			//Enter/Space does not
			if (action>>8 !=32 && action>>8 != 13) return C_CONTINUE;
			UndoStart( _("Modify Track"), "Modify( T%d[%d] )", GetTrkIndex(Dex.Trk), Dex.params.ep );
			UndoModify( Dex.Trk );
			rc = ModifyTrack( Dex.Trk, action, pos );
			if (rc != C_CONTINUE) modifyDrawMode = FALSE;
			UndoEnd();
			break;
		case C_FINISH:
		case C_CONFIRM:
		case C_TERMINATE:
			rc = ModifyTrack( Dex.Trk, action, pos );
			Dex.Trk = NULL;
			modifyDrawMode = FALSE;
			tempSegs_da.cnt = 0;
			rc = C_CONTINUE;
			break;
		case C_REDRAW:
			rc = ModifyTrack( Dex.Trk, action, pos );
			break;
		case C_CMDMENU:
			rc = ModifyTrack( Dex.Trk, action, pos );
			break;
		default:
			break;
	}
	return rc;
}


STATUS_T CmdModify(
		wAction_t action,
		coOrd pos )
/*
 * Extend and alter a track.
 * Extend a track with a curve or straight and optionally an easement.
 * Alter a ruler.
 * Modify a Bezier.
 */
{

	track_p trk, trk1;
	ANGLE_T a0;
	DIST_T d;
	ANGLE_T a;
	EPINX_T inx;
	curveType_e curveType;
	static BOOL_T changeTrackMode;


	STATUS_T rc;
	static DIST_T trackGauge;

	if ( changeTrackMode ) {
		if ( action == C_MOVE )
			action = C_RMOVE;
		if ( action == C_UP )
			action = C_RUP;
	}

	switch (action&0xFF) {

	case C_START:
		DYNARR_RESET(trkSeg_t,anchors_da);
		InfoMessage( _("Select a track to modify, Left-Click change length, Right-Click to add flextrack") );
		Dex.Trk = NULL;
		tempSegs_da.cnt = 0;
		/*ChangeParameter( &easementPD );*/
		trackGauge = 0.0;
		changeTrackMode = modifyRulerMode = FALSE;
		modifyBezierMode = FALSE;
		modifyCornuMode = FALSE;
		modifyDrawMode = FALSE;
		return C_CONTINUE;

	case C_DOWN:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (modifyBezierMode)
			return ModifyBezier(C_DOWN, pos);
		if (modifyCornuMode)
			return ModifyCornu(C_DOWN, pos);
		if (modifyDrawMode)
			return ModifyDraw(C_DOWN, pos);
		DYNARR_SET( trkSeg_t, tempSegs_da, 2 );
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		tempSegs(1).color = wDrawColorBlack;
		tempSegs(1).width = 0;
		tempSegs_da.cnt = 0;
		Dex.Trk = OnTrack( &pos, TRUE, FALSE );
		//Dex.Trk = trk;
		if (Dex.Trk == NULL) {
			if ( ModifyRuler( C_DOWN, pos ) == C_CONTINUE )
				modifyRulerMode = TRUE;
			return C_CONTINUE;
		}
		if (!CheckTrackLayer( Dex.Trk ) ) {
			Dex.Trk = NULL;

			return C_CONTINUE;
		}
		trackGauge = (IsTrack(Dex.Trk)?GetTrkGauge(Dex.Trk):0.0);
		if (QueryTrack( Dex.Trk, Q_CAN_MODIFY_CONTROL_POINTS )) { //Bezier
			modifyBezierMode = TRUE;
			if (ModifyBezier(C_START, pos) != C_CONTINUE) {			//Call Start with track
				modifyBezierMode = FALSE;							//Function rejected Bezier
				Dex.Trk =NULL;
				tempSegs_da.cnt = 0;
			}
			return C_CONTINUE;										//That's it
		}
		if (QueryTrack( Dex.Trk, Q_IS_CORNU )) { 					//Cornu
			modifyCornuMode = TRUE;
			if (ModifyCornu(C_START, pos) != C_CONTINUE) {			//Call Start with track
				modifyCornuMode = FALSE;							//Function rejected Bezier
				Dex.Trk =NULL;
				tempSegs_da.cnt = 0;
			}
			return C_CONTINUE;										//That's it
		}

		if (QueryTrack( Dex.Trk, Q_IS_DRAW )) {
			modifyDrawMode = TRUE;
			if (ModifyDraw(C_START, pos) != C_CONTINUE) {
				modifyDrawMode = FALSE;
				Dex.Trk = NULL;
				tempSegs_da.cnt = 0;
			}
			return C_CONTINUE;
		}

		if ((MyGetKeyState()&WKEY_CTRL)) goto extendTrack;

		if ( (MyGetKeyState()&WKEY_SHIFT) &&      //Free to change radius
			 QueryTrack( Dex.Trk, Q_CAN_MODIFYRADIUS )&&
			 ((inx=PickUnconnectedEndPoint(pos,Dex.Trk)) >= 0 )) {
			trk = Dex.Trk;
			while ( (trk1=GetTrkEndTrk(trk,1-inx)) &&        //Means next track to mine even if can be end...
					QueryTrack(trk1, Q_CANNOT_BE_ON_END) ) {
				inx = GetEndPtConnectedToMe( trk1, trk );
				trk = trk1;
			}
			if (trk1) {
				UndoStart( _("Change Track"), "Change( T%d[%d] )", GetTrkIndex(Dex.Trk), Dex.params.ep );
				inx = GetEndPtConnectedToMe( trk1, trk );
                Dex.Trk = NULL;
                DeleteTrack(trk, TRUE);					//Get rid of original track
				if ( !GetTrkEndTrk( trk1, inx ) ) {
					Dex.Trk = trk1;
					Dex.pos00 = GetTrkEndPos( Dex.Trk, inx );
					changeTrackMode = TRUE;
					goto CHANGE_TRACK;
				}
			}
			ErrorMessage( MSG_CANNOT_CHANGE );
		}
		InfoMessage(_("Drag to set size, +Shift to change radius"));
		ModifyTrack(Dex.Trk, C_START, pos);         //Basically trim via Modify...
		rc = ModifyTrack( Dex.Trk, C_DOWN, pos );
		if ( rc != C_CONTINUE ) {
			Dex.Trk = NULL;
			rc = C_CONTINUE;
		}
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
        MainRedraw();
        MapRedraw();
		return rc;

	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (modifyBezierMode) return C_CONTINUE;
		if (modifyCornuMode) return C_CONTINUE;
		if (modifyDrawMode) return C_CONTINUE;
		track_p t;
		if (((t=OnTrack(&pos,FALSE,TRUE))!= NULL) && CheckTrackLayer( t )) {
			if (GetTrkScale(t) == (char)GetLayoutCurScale()) {
				EPINX_T ep = PickUnconnectedEndPointSilent(pos, t);
				if (QueryTrack( t, Q_IS_CORNU )) {
					CreateCornuAnchor(pos,FALSE);
				} else if ( QueryTrack( t, Q_CAN_MODIFY_CONTROL_POINTS )) {
					CreateRadiusAnchor(pos,NormalizeAngle(GetAngleAtPoint(t,pos,NULL,NULL)+90.0),TRUE);
					CreateEndAnchor(pos,FALSE);
				} else if (QueryTrack(t,Q_CAN_ADD_ENDPOINTS)){     //Turntable
					trackParams_t tp;
					if (!GetTrackParams(PARAMS_CORNU, t, pos, &tp)) return C_CONTINUE;
					ANGLE_T a = tp.angle;
					Translate(&pos,tp.ttcenter,a,tp.ttradius);
					CreateRadiusAnchor(pos,a,FALSE);
				} else if (QueryTrack(t,Q_CAN_EXTEND)) {
					if (ep != -1) {
						if (MyGetKeyState()&WKEY_CTRL) {
							pos = GetTrkEndPos(t,ep);
							CreateEndAnchor(pos,FALSE);
							CreateRadiusAnchor(pos,GetTrkEndAngle(t,ep),FALSE);
							CreateRadiusAnchor(pos,GetTrkEndAngle(t,ep)+90,TRUE);
						} else {
							CreateEndAnchor(pos,FALSE);
							if ((MyGetKeyState()&WKEY_SHIFT) && 					//Shift Down
								QueryTrack( t, Q_CAN_MODIFYRADIUS ) &&				// Straight or Curve
								((inx=PickUnconnectedEndPointSilent(pos,t)) >= 0 )) { //Which has an open end
								if (GetTrkEndTrk(t,1-inx))					// Has to have a track on other end
									CreateRadiusAnchor(pos,NormalizeAngle(GetAngleAtPoint(t,pos,NULL,NULL)+90.0),TRUE);
							}
							CreateRadiusAnchor(pos,GetAngleAtPoint(t,pos,NULL,NULL),TRUE);
						}
					}
				} else if (ep>=0){													//Turnout
					pos = GetTrkEndPos(t, ep);
					CreateEndAnchor(pos,TRUE);
					if ( (MyGetKeyState()&WKEY_CTRL)) {
						CreateRadiusAnchor(pos,NormalizeAngle(GetTrkEndAngle(t,ep)),FALSE);
						CreateRadiusAnchor(pos,GetTrkEndAngle(t,ep)+90,TRUE);
						CreateEndAnchor(pos,TRUE);
					} else {
						CreateRadiusAnchor(pos,NormalizeAngle(GetTrkEndAngle(t,ep)),FALSE);
						CreateEndAnchor(pos,TRUE);
					}
				}
			}
		} else if (((t=OnTrack(&pos,FALSE,FALSE))!= NULL) && !GetLayerFrozen( GetTrkLayer( t )) && QueryTrack( t, Q_IS_DRAW )) {
			DrawTrack( t, &mainD, wDrawColorBlue );
			CreateEndAnchor(pos,FALSE);
		}
		if (anchors_da.cnt)
				DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		if (tempSegs_da.cnt)
				DrawSegs( &mainD, zero, 0.0, &anchors(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	case C_MOVE:
		if ( modifyRulerMode )
			return ModifyRuler( C_MOVE, pos );
		if (Dex.Trk == NULL)
			return C_CONTINUE;
		if ( modifyBezierMode )
			return ModifyBezier(C_MOVE, pos);
		if ( modifyCornuMode )
			return ModifyCornu(C_MOVE, pos);
		if ( modifyDrawMode)
			return ModifyDraw(C_MOVE, pos);
		if ((MyGetKeyState()&WKEY_CTRL))
			goto extendTrackMove;
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorWhite );
		tempSegs_da.cnt = 0;

		SnapPos( &pos );
		rc = ModifyTrack( Dex.Trk, C_MOVE, pos );
		if ( rc != C_CONTINUE ) {
			rc = C_CONTINUE;
			Dex.Trk = NULL;
		}
        MainRedraw();
        MapRedraw();
		return rc;

	case C_UP:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (Dex.Trk == NULL)
			return C_CONTINUE;
		if ( modifyRulerMode )
			return ModifyRuler( C_MOVE, pos );
		if ( modifyBezierMode )
			return ModifyBezier( C_UP, pos);
		if (modifyCornuMode)
			return ModifyCornu(C_UP, pos);
		if (modifyDrawMode)
			return ModifyDraw(C_UP, pos);
		if ((MyGetKeyState()&WKEY_CTRL)) goto extendTrackUp;

		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorWhite );
		tempSegs_da.cnt = 0;

		SnapPos( &pos );
		UndoStart( _("Modify Track"), "Modify( T%d[%d] )", GetTrkIndex(Dex.Trk), Dex.params.ep );
		UndoModify( Dex.Trk );
		rc = ModifyTrack( Dex.Trk, C_UP, pos );
		UndoEnd();
        Dex.Trk = NULL;
        MainRedraw();
        MapRedraw();
		return rc;

	case C_RDOWN:									//This is same as context menu....
extendTrack:
		DYNARR_RESET(trkSeg_t,anchors_da);
		changeTrackMode = TRUE;
		modifyRulerMode = FALSE;
		modifyBezierMode = FALSE;
		modifyCornuMode = FALSE;
		modifyDrawMode = FALSE;
		Dex.Trk = OnTrack( &pos, TRUE, TRUE );
		if (Dex.Trk) {
			if (!CheckTrackLayer( Dex.Trk ) ) {
				Dex.Trk = NULL;
				return C_CONTINUE;
			}
			trackGauge = GetTrkGauge( Dex.Trk );
			Dex.pos00 = pos;
CHANGE_TRACK:
			if (GetTrackParams( PARAMS_EXTEND, Dex.Trk, Dex.pos00, &Dex.params)) {
				if (Dex.params.ep == -1) {
					Dex.Trk = NULL;
					return C_CONTINUE;
					break;
				}
				if (Dex.params.ep == 0) {
					Dex.params.arcR = -Dex.params.arcR;
				}
				Dex.pos00 = GetTrkEndPos(Dex.Trk,Dex.params.ep);
				Dex.angle = GetTrkEndAngle( Dex.Trk,Dex.params.ep);
				Translate( &Dex.pos00x, Dex.pos00, Dex.angle, 10.0 );
LOG( log_modify, 1, ("extend endPt[%d] = [%0.3f %0.3f] A%0.3f\n",
							Dex.params.ep, Dex.pos00.x, Dex.pos00.y, Dex.angle ) )
				InfoMessage( _("Drag to add flex track") );
			} else {
				return C_ERROR;
			}
		}
		Dex.first = TRUE;
        MainRedraw();
        MapRedraw();
        /* no break */
	case C_RMOVE:
extendTrackMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorWhite );
		tempSegs_da.cnt = 0;
		Dex.valid = FALSE;
		if (Dex.Trk == NULL) return C_CONTINUE;
		SnapPos( &pos );
		if ( Dex.first && FindDistance( pos, Dex.pos00 ) <= minLength )
			return C_CONTINUE;
		Dex.first = FALSE;
		Dex.pos01 = Dex.pos00;

		if (Dex.params.type == curveTypeCornu) {    			//Always Restrict Cornu drag out to match end
			ANGLE_T angle2 = NormalizeAngle(FindAngle(pos, Dex.pos00)-Dex.angle);
			if (angle2 > 90.0 && angle2 < 270.0) {
				if (Dex.params.cornuRadius[Dex.params.ep] == 0) {
					Translate( &pos, Dex.pos00, Dex.angle, FindDistance( Dex.pos00, pos ) );
				} else {
					ANGLE_T angle = FindAngle(Dex.params.cornuCenter[Dex.params.ep],pos)-
							FindAngle(Dex.params.cornuCenter[Dex.params.ep],Dex.pos00);
					pos=Dex.pos00;
					Rotate(&pos,Dex.params.cornuCenter[Dex.params.ep],angle);
				}
			} else pos = Dex.pos00;					//Only out from end
			PlotCurve( crvCmdFromCornu, Dex.pos00, Dex.pos00x, pos, &Dex.curveData, FALSE );
		} else
			PlotCurve( crvCmdFromEP1, Dex.pos00, Dex.pos00x, pos, &Dex.curveData, TRUE );
		curveType = Dex.curveData.type;
		if ( curveType == curveTypeStraight ) {
			Dex.r1 = 0.0;
			if (Dex.params.type == curveTypeCurve) {
				if (ComputeJoint( Dex.params.arcR, Dex.r1, &Dex.jointD ) == E_ERROR)
					return C_CONTINUE;
				d = Dex.params.len - Dex.jointD.d0;
				if (d <= minLength) {
					ErrorMessage( MSG_TRK_TOO_SHORT, "First ", PutDim(fabs(minLength-d)) );
					return C_CONTINUE;
				}
			} else {
				Dex.jointD.d1 = 0.0;
			}
			tempSegs(0).type = SEG_STRTRK;
			tempSegs(0).width = 0;
			tempSegs(0).u.l.pos[0] = Dex.pos01;
			tempSegs(0).u.l.pos[1] = Dex.curveData.pos1;
			d = FindDistance( Dex.pos01, Dex.curveData.pos1 );
			d -= Dex.jointD.d1;
			if (d <= minLength) {
				ErrorMessage( MSG_TRK_TOO_SHORT, "Extending ", PutDim(fabs(minLength-d)) );
				return C_CONTINUE;
			}
			tempSegs_da.cnt = 1;
			Dex.valid = TRUE;
			if (action != C_RDOWN)
				InfoMessage( _("Straight Track: Length=%s Angle=%0.3f"),
					FormatDistance( FindDistance( Dex.curveData.pos1, Dex.pos01 ) ),
					PutAngle( FindAngle( Dex.pos01, Dex.curveData.pos1 ) ) );
		} else if ( curveType == curveTypeNone ) {
			if (action != C_RDOWN)
				InfoMessage( _("Back") );
			return C_CONTINUE;
		} else if ( curveType == curveTypeCurve ) {
			Dex.r1 = Dex.curveData.curveRadius;
			if ( QueryTrack( Dex.Trk, Q_IGNORE_EASEMENT_ON_EXTEND ) ) {
							       /* Ignore easements when extending turnouts or turntables */
				Dex.jointD.x =
				Dex.jointD.r0 = Dex.jointD.r1 = 
				Dex.jointD.l0 = Dex.jointD.l1 = 
				Dex.jointD.d0 = Dex.jointD.d1 = 0.0;
				Dex.jointD.flip = Dex.jointD.negate = Dex.jointD.Scurve = FALSE;
				d = Dex.curveData.curveRadius * Dex.curveData.a1 * 2.0*M_PI/360.0;
			} else {					/* Easement code */
				if (easementVal<0.0) {  //Cornu Join - need to estimate a "good" easement length
					d = Dex.curveData.curveRadius * Dex.curveData.a1 * 2.0*M_PI/360.0;
					Dex.jointD.d0 = Dex.jointD.d1 =0.75*72*12/GetTrkScale(Dex.Trk); //Easement 1.5 cars long to start
					if (Dex.jointD.d0>(GetTrkLength(Dex.Trk,0,1)/2))
						Dex.jointD.d0 = GetTrkLength(Dex.Trk,0,1)/2;
					if (Dex.jointD.d1>d/2)
						Dex.jointD.d1 = d/2;
					Dex.jointD.negate = DifferenceBetweenAngles(Dex.angle,FindAngle(Dex.pos00,pos))<0.0;
					Dex.jointD.x = 2*trackGauge;  //Signal an easement present to JoinTracks
				} else {
					if ( easeR > 0.0 && Dex.r1 < easeR ) {
						ErrorMessage( MSG_RADIUS_LSS_EASE_MIN,
							FormatDistance( Dex.r1 ), FormatDistance( easeR ) );
						return C_CONTINUE;
					}
					if ( Dex.r1*2.0*M_PI*Dex.curveData.a1/360.0 > mapD.size.x+mapD.size.y ) {
						ErrorMessage( MSG_CURVE_TOO_LARGE );
						return C_CONTINUE;
					}
					if ( NormalizeAngle( FindAngle( Dex.pos00, pos ) - Dex.angle ) > 180.0 )
						if (ComputeJoint( Dex.params.arcR, Dex.r1, &Dex.jointD ) == E_ERROR)
							return C_CONTINUE;
					d = Dex.params.len - Dex.jointD.d0;
					if (d <= minLength) {
						ErrorMessage( MSG_TRK_TOO_SHORT, "First ", PutDim(fabs(minLength-d)) );
						return C_CONTINUE;
					}
				}
				d -= Dex.jointD.d1;
				a0 = Dex.angle + (Dex.jointD.negate?-90.0:+90.0);
				Translate( &Dex.pos01, Dex.pos00, a0, Dex.jointD.x );
				Translate( &Dex.curveData.curvePos, Dex.curveData.curvePos,
							a0, Dex.jointD.x );
LOG( log_modify, 2, ("A=%0.3f X=%0.3f\n", a0, Dex.jointD.x ) )
			}
			if (d <= minLength) {
				ErrorMessage( MSG_TRK_TOO_SHORT, "Extending ", PutDim(fabs(minLength-d)) );
				return C_CONTINUE;
			}
			tempSegs(0).type = SEG_CRVTRK;
			tempSegs(0).width = 0;
			tempSegs(0).u.c.center = Dex.curveData.curvePos;
			tempSegs(0).u.c.radius = Dex.curveData.curveRadius,
			tempSegs(0).u.c.a0 = Dex.curveData.a0;
			tempSegs(0).u.c.a1 = Dex.curveData.a1;
			tempSegs_da.cnt = 1;
			double da = D2R(Dex.curveData.a1);
			if (da < 0.0)
				da = 2*M_PI + da;
			a = NormalizeAngle( Dex.angle - FindAngle( Dex.pos00, Dex.curveData.curvePos ) );
			if ( a < 180.0 )
				a = NormalizeAngle( Dex.curveData.a0-90 );
			else
				a = NormalizeAngle( Dex.curveData.a0+Dex.curveData.a1+90.0 );
			Dex.valid = TRUE;
			if (action != C_RDOWN)
				InfoMessage( _("Curve Track: Radius=%s Length=%s Angle=%0.3f"),
					FormatDistance( Dex.curveData.curveRadius ),
					FormatDistance( Dex.curveData.curveRadius * da),
					Dex.curveData.a1 );
		}
        MainRedraw();
        MapRedraw();
		return C_CONTINUE;

	case C_RUP:
extendTrackUp:
		changeTrackMode = FALSE;
		tempSegs_da.cnt = 0;
		if (Dex.Trk == NULL) return C_CONTINUE;
		if (!Dex.valid)
			return C_CONTINUE;
		UndoStart( _("Extend Track"), "Extend( T%d[%d] )", GetTrkIndex(Dex.Trk), Dex.params.ep );
		trk = NULL;
		curveType = Dex.curveData.type;
   
		if ( curveType == curveTypeStraight ) {
			if (QueryTrack(Dex.Trk,Q_CAN_EXTEND))   //Check it isn't a turnout end....
				if ( Dex.params.type == curveTypeStraight &&
						FindDistance(Dex.pos01, Dex.curveData.pos1) > 0 ) {
					UndoModify( Dex.Trk );
					AdjustStraightEndPt( Dex.Trk, Dex.params.ep, Dex.curveData.pos1 );
					UndoEnd();
					DrawNewTrack(Dex.Trk );
					MainRedraw();
					MapRedraw();
					return C_TERMINATE;
			}
			if (FindDistance(Dex.pos01, Dex.curveData.pos1) == 0) return C_ERROR;
LOG( log_modify, 1, ("L = %0.3f, P0 = %0.3f, P1 = %0.3f\n",
						Dex.params.len, Dex.pos01, Dex.curveData.pos1 ) )
			trk = NewStraightTrack( Dex.pos01, Dex.curveData.pos1 );
			inx = 0;

		} else if ( curveType == curveTypeCurve ) {
LOG( log_modify, 1, ("R = %0.3f, A0 = %0.3f, A1 = %0.3f\n",
						Dex.curveData.curveRadius, Dex.curveData.a0, Dex.curveData.a1 ) )
			trk = NewCurvedTrack( Dex.curveData.curvePos, Dex.curveData.curveRadius,
						Dex.curveData.a0, Dex.curveData.a1, 0 );
			inx = PickUnconnectedEndPoint( Dex.pos01, trk );
			if (inx == -1)
				return C_ERROR;

		} else {
			return C_ERROR;
		}
		CopyAttributes( Dex.Trk, trk );
		if (Dex.jointD.d1 == 0)
			ConnectTracks(Dex.Trk, Dex.params.ep, trk, inx);
		else
			JoinTracks( Dex.Trk, Dex.params.ep, Dex.pos00, trk, inx, Dex.pos01, &Dex.jointD );
		UndoEnd();
		tempSegs_da.cnt = 0;
        MainRedraw();
        MapRedraw();
		return C_TERMINATE;

	case C_REDRAW:
		if (modifyBezierMode) return ModifyBezier(C_REDRAW, pos);
		if (modifyCornuMode) return ModifyCornu(C_REDRAW, pos);
		if (modifyDrawMode) return ModifyDraw(C_REDRAW, pos);
		if ( (!changeTrackMode) && Dex.Trk && !QueryTrack( Dex.Trk,	 Q_MODIFY_REDRAW_DONT_UNDRAW_TRACK ) )
		   UndrawNewTrack( Dex.Trk );
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		if (anchors_da.cnt)
			DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );

		return C_CONTINUE;

	case C_TEXT:
		if ( !Dex.Trk )
			return C_CONTINUE;
		if (modifyBezierMode)
			return ModifyBezier(action, pos);
		if (modifyCornuMode)
			return ModifyCornu(action, pos);
		if (modifyDrawMode)
			return ModifyDraw(action, pos);
		return ModifyTrack( Dex.Trk, action, pos );

	case C_LCLICK:
		if ( modifyDrawMode) {
			rc = ModifyDraw(C_DOWN, pos);
			if (rc == C_CONTINUE)
				return ModifyDraw(C_UP, pos);
		}
		/*no break*/
	default:
		if (modifyBezierMode) return ModifyBezier(action, pos);
		if (modifyCornuMode)  return ModifyCornu(action, pos);
		if (modifyDrawMode) return ModifyDraw(action, pos);
		if (Dex.Trk)
			return ModifyTrack( Dex.Trk, action, pos );
		return C_CONTINUE;
	}
	return C_CONTINUE;
}


/*****************************************************************************
 *
 * INITIALIZATION
 *
 */

#include "bitmaps/extend.xpm"

void InitCmdModify( wMenu_p menu )
{
	modifyCmdInx = AddMenuButton( menu, CmdModify, "cmdModify", _("Modify"), wIconCreatePixMap(extend_xpm), LEVEL0_50, IC_STICKY|IC_POPUP|IC_WANT_MOVE|IC_CMDMENU, ACCL_MODIFY, NULL );
	log_modify = LogFindIndex( "modify" );
}
