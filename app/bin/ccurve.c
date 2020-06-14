/** \file ccurve.c
 * CURVE
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

#include "ccurve.h"
 
#include "cjoin.h"
#include "cstraigh.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"

#include "messages.h"
#include "param.h"
#include "param.h"
#include "track.h"
#include "utility.h"
#include "wlib.h"
#include "cbezier.h"
#include "ccornu.h"
#include "layout.h"

/*
 * STATE INFO
 */

typedef enum createState_e {NOCURVE,FIRSTEND_DEF,SECONDEND_DEF,CENTER_DEF} createState_e;

static struct {
		STATE_T state;
		createState_e create_state;
		coOrd pos0;
		coOrd pos1;
		curveData_t curveData;
		track_p trk;
		EPINX_T ep;
		BOOL_T down;
		BOOL_T lock0;
		coOrd middle;
		coOrd end0;
		coOrd end1;
		} Da;

static long curveMode;

static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)
#define array_anchor(N) DYNARR_N(trkSeg_t,*anchor_array,N)


EXPORT int DrawArrowHeads(
		trkSeg_p sp,
		coOrd pos,
		ANGLE_T angle,
		BOOL_T bidirectional,
		wDrawColor color )
{
		coOrd p0, p1;
		DIST_T d, w;
		int inx;
		d = mainD.scale*0.25;
		w = mainD.scale/mainD.dpi*2;
		for ( inx=0; inx<5; inx++ ) {
			sp[inx].type = SEG_STRLIN;
			sp[inx].width = w;
			sp[inx].color = color;
		}
		Translate( &p0, pos, angle, d );
		Translate( &p1, pos, angle+180, bidirectional?d:(d/2.0) );
		sp[0].u.l.pos[0] = p0;
		sp[0].u.l.pos[1] = p1;
		sp[1].u.l.pos[0] = p0;
		Translate( &sp[1].u.l.pos[1], p0, angle+135, d/2.0 );
		sp[2].u.l.pos[0] = p0;
		Translate( &sp[2].u.l.pos[1], p0, angle-135, d/2.0 );
		if (bidirectional) {
			sp[3].u.l.pos[0] = p1;
			Translate( &sp[3].u.l.pos[1], p1, angle-45, d/2.0 );
			sp[4].u.l.pos[0] = p1;
			Translate( &sp[4].u.l.pos[1], p1, angle+45, d/2.0 );
		} else {
			sp[3].u.l.pos[0] = p1;
			sp[3].u.l.pos[1] = p1;
			sp[4].u.l.pos[0] = p1;
			sp[4].u.l.pos[1] = p1;
		}
		return 5;
}

EXPORT int DrawArrowHeadsArray(
		dynArr_t *anchor_array,
		coOrd pos,
		ANGLE_T angle,
		BOOL_T bidirectional,
		wDrawColor color )
{
	int i = (*anchor_array).cnt;
	DYNARR_SET(trkSeg_t,*anchor_array,i+5)
	return DrawArrowHeads(&DYNARR_N(trkSeg_t,*anchor_array,i),pos,angle,bidirectional,color);

}

static void CreateEndAnchor(coOrd p, dynArr_t * anchor_array, wBool_t lock) {
	DIST_T d = tempD.scale*0.15;

	DYNARR_APPEND(trkSeg_t,*anchor_array,1);
	int i = (*anchor_array).cnt-1;
	array_anchor(i).type = lock?SEG_FILCRCL:SEG_CRVLIN;
	array_anchor(i).color = wDrawColorBlue;
	array_anchor(i).u.c.center = p;
	array_anchor(i).u.c.radius = d/2;
	array_anchor(i).u.c.a0 = 0.0;
	array_anchor(i).u.c.a1 = 360.0;
	array_anchor(i).width = 0;
}



EXPORT STATUS_T CreateCurve(
		wAction_t action,
		coOrd pos,
		BOOL_T track,
		wDrawColor color,
		DIST_T width,
		long mode,
		dynArr_t * anchor_array,
		curveMessageProc message )
{
	track_p t;
	DIST_T d;
	ANGLE_T a, angle1, angle2;
	static coOrd pos0, p;
	int inx;

	switch ( action ) {
	case C_START:
		DYNARR_RESET(trkSeg_t,*anchor_array);
		DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
		Da.create_state = NOCURVE;
		tempSegs_da.cnt = 0;
		Da.down = FALSE;  						//Not got a valid start yet
		Da.pos0 = zero;
		Da.pos1 = zero;
		switch ( curveMode ) {
		case crvCmdFromEP1:
			if (track) 
				message(_("Drag from endpoint in direction of curve - lock to track open endpoint") );
			else 	
				message (_("Drag from endpoint in direction of curve") );
			break;
		case crvCmdFromTangent:
			if (track)
				message(_("Drag from endpoint to center - lock to track open endpoint") );
			else
				message(_("Drag from endpoint to center") );
			break;
		case crvCmdFromCenter:
			message(_("Drag from center to endpoint") );
			break;
		case crvCmdFromChord:
			message(_("Drag from one to other end of chord") );
			break;
		}
		return C_CONTINUE;
	case C_DOWN:
			DYNARR_RESET(trkSeg_t, *anchor_array);
			for ( inx=0; inx<8; inx++ ) {
				 tempSegs(inx).color = wDrawColorBlack;
				 tempSegs(inx).width = 0;
			}
			tempSegs_da.cnt = 0;
			p = pos;
		    BOOL_T found = FALSE;
		    Da.trk = NULL;
		    if (track) {
				if ((mode == crvCmdFromEP1 || mode == crvCmdFromTangent || (mode == crvCmdFromChord))  &&
						((MyGetKeyState() & WKEY_ALT) == 0 ) == magneticSnap) {
					if ((t = OnTrack(&p, FALSE, TRUE)) != NULL) {
						EPINX_T ep = PickUnconnectedEndPointSilent(p, t);
						if (ep != -1) {
							if (GetTrkScale(t) != (char)GetLayoutCurScale()) {
								wBeep();
								InfoMessage(_("Track is different gauge"));
								return C_CONTINUE;
							}
							Da.trk = t;
							Da.ep = ep;
							pos = GetTrkEndPos(t, ep);
							found = TRUE;
						}
					}
				}
		    } else {
		    	if ((t = OnTrack(&p, FALSE, FALSE)) != NULL) {
		    		if (!IsTrack(t)) {
		    			pos = p;
		    			found = TRUE;
		    		}
		    	}
		    }
			Da.down = TRUE;
			if (!found) SnapPos( &pos );
			Da.lock0 = found;

			if (Da.create_state == NOCURVE)
				Da.pos0 = pos;
			else
				Da.pos1 = pos;

			tempSegs_da.cnt = 1;
			switch (mode) {
			case crvCmdFromEP1:
				tempSegs(0).type = (track?SEG_STRTRK:SEG_STRLIN);
				tempSegs(0).color = color;
				tempSegs(0).width = width;
				Da.create_state = FIRSTEND_DEF;
				Da.end0 = pos;
				CreateEndAnchor(pos,anchor_array,found);
				if (Da.trk) message(_("End locked: Drag out curve start"));
				else message(_("Drag along curve start") );
				break;
			case crvCmdFromTangent:
				Da.create_state = FIRSTEND_DEF;
				tempSegs(0).type = SEG_STRLIN;
				tempSegs(0).color = color;
				Da.create_state = CENTER_DEF;
				CreateEndAnchor(pos,anchor_array,found);
				if (Da.trk) message(_("End locked: Drag out curve center"));
				else message(_("Drag out curve center") );
				break;
			case crvCmdFromCenter:
				tempSegs(0).type = SEG_STRLIN;
				tempSegs(0).color = color;
				Da.create_state = CENTER_DEF;
				CreateEndAnchor(pos,anchor_array,FALSE);
				message(_("Drag out from center to endpoint"));
				break;
			case crvCmdFromChord:
				tempSegs(0).type = (track?SEG_STRTRK:SEG_STRLIN);
				tempSegs(0).color = color;
				tempSegs(0).width = width; 
				CreateEndAnchor(pos,anchor_array,FALSE);
				Da.create_state = FIRSTEND_DEF;
				if (Da.trk)
					message( _("End locked: Drag to other end of chord") );
				else
					message( _("Drag to other end of chord") );
				break;
			}
			tempSegs(0).u.l.pos[0] = tempSegs(0).u.l.pos[1] = pos;
		return C_CONTINUE;

	case C_MOVE:
		DYNARR_RESET(trkSeg_t,*anchor_array);
		DYNARR_APPEND(trkSeg_t,*anchor_array,1);
		if (!Da.down) return C_CONTINUE;
		if (Da.trk) {
			angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk, Da.ep));
			angle2 = NormalizeAngle(FindAngle(pos, Da.pos0)-angle1);
			if (mode ==crvCmdFromEP1 ) {
				if (angle2 > 90.0 && angle2 < 270.0)
					Translate( &pos, Da.pos0, angle1, -FindDistance( Da.pos0, pos )*cos(D2R(angle2)) );
				else pos = Da.pos0;
			} else if ( mode == crvCmdFromChord ) {
				DIST_T dp = -FindDistance(Da.pos0, pos)*sin(D2R(angle2));
				if (DifferenceBetweenAngles(FindAngle(Da.pos0,pos),angle1)>0)
					Translate( &pos, Da.pos0, angle1+90, dp );
				else
					Translate( &pos, Da.pos0, angle1-90, -dp );
			} else if (mode == crvCmdFromCenter) {
				DIST_T dp = -FindDistance(Da.pos0, pos)*sin(D2R(angle2));
				if (angle2 > 90 && angle2 < 270.0)
					Translate( &pos, Da.pos0, angle1+90.0, dp );
				else
					Translate( &pos, Da.pos0, angle1-90.0, dp );
			} else if (mode == crvCmdFromTangent) {
				DIST_T dp = FindDistance(Da.pos0, pos)*sin(D2R(angle2));
				Translate( &pos, Da.pos0, angle1-90.0, dp );
			}
		} else SnapPos(&pos);
		tempSegs_da.cnt =1;
		if (Da.trk && mode == crvCmdFromChord) {
			tempSegs(0).type = SEG_CRVTRK;
			tempSegs(0).u.c.center.x = (pos.x+Da.pos0.x)/2.0;
			tempSegs(0).u.c.center.y = (pos.y+Da.pos0.y)/2.0;
			tempSegs(0).u.c.radius = FindDistance(pos,Da.pos0)/2;
			ANGLE_T a0 = FindAngle(tempSegs(0).u.c.center,Da.pos0);
			ANGLE_T a1 = FindAngle(tempSegs(0).u.c.center,pos);
			if (NormalizeAngle(a0+90-GetTrkEndAngle(Da.trk,Da.ep))<90) {
				tempSegs(0).u.c.a0 = a0;
			} else {
				tempSegs(0).u.c.a0 = a1;
			}
			tempSegs(0).u.c.a1 = 180.0;
		} else tempSegs(0).u.l.pos[1] = pos;
		Da.pos1 = pos;

		d = FindDistance( Da.pos0, Da.pos1 );
		a = FindAngle( Da.pos0, Da.pos1 );
		switch ( mode ) {
		case crvCmdFromEP1:
			if (Da.trk) message( _("Start Locked: Drag out curve start - Angle=%0.3f"), PutAngle(a));
			else message( _("Drag out curve start - Angle=%0.3f"), PutAngle(a) );
			CreateEndAnchor(Da.pos0,anchor_array,Da.lock0);
			DrawArrowHeadsArray( anchor_array, pos, FindAngle(Da.pos0,Da.pos1)+90, TRUE, wDrawColorBlue );
			tempSegs_da.cnt = 1;
			break;
		case crvCmdFromTangent:
			if (Da.trk) message( _("Tangent locked: Drag out center - Radius=%s Angle=%0.3f"), FormatDistance(d), PutAngle(a) );
			else message( _("Drag out center - Radius=%s Angle=%0.3f"), FormatDistance(d), PutAngle(a) );
			CreateEndAnchor(Da.pos1,anchor_array,TRUE);
			DrawArrowHeadsArray( anchor_array, Da.pos0, FindAngle(Da.pos0,Da.pos1)+90, TRUE, wDrawColorBlue );
			tempSegs_da.cnt = 1;
			break;
		case crvCmdFromCenter:
			message( _("Drag to Edge: Radius=%s Angle=%0.3f"), FormatDistance(d), PutAngle(a) );
			CreateEndAnchor(Da.pos0,anchor_array,Da.lock0);
			DrawArrowHeadsArray( anchor_array, Da.pos1, FindAngle(Da.pos1,Da.pos0)+90, TRUE, wDrawColorBlue );
			tempSegs_da.cnt = 1;
			break;
		case crvCmdFromChord:
			if (Da.trk) message( _("Start locked: Drag out chord length=%s angle=%0.3f"), FormatDistance(d), PutAngle(a) );
			else message( _("Drag out chord length=%s angle=%0.3f"), FormatDistance(d), PutAngle(a) );
			Da.middle.x = (Da.pos1.x+Da.pos0.x)/2.0;
			Da.middle.y = (Da.pos1.y+Da.pos0.y)/2.0;
			if (track && Da.trk) {
				ANGLE_T ea = GetTrkEndAngle(Da.trk,Da.ep);
				Translate(&Da.middle,Da.middle,ea,FindDistance(Da.middle,Da.pos0));
			}
			CreateEndAnchor(Da.pos0,anchor_array,TRUE);
			CreateEndAnchor(Da.pos1,anchor_array,FALSE);
			if (!track || !Da.trk)
				DrawArrowHeadsArray( anchor_array, Da.middle, FindAngle(Da.pos0,Da.pos1)+90, TRUE, wDrawColorBlue );
			break;
		}
		return C_CONTINUE;
	case C_UP:
		/* Note - no anchor reset - assumes run after Down/Move */
		if (!Da.down) return C_CONTINUE;
		if (Da.trk) {
			angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk, Da.ep));
			angle2 = NormalizeAngle(FindAngle(pos, Da.pos0)-angle1);
			if (mode == crvCmdFromEP1) {
				if (angle2 > 90.0 && angle2 < 270.0) {			
					Translate( &pos, Da.pos0, angle1, -FindDistance( Da.pos0, pos )*cos(D2R(angle2)) );
					Da.pos1 = pos;
				} else {
					ErrorMessage( MSG_TRK_TOO_SHORT, "Curved ", PutDim(0.0) );
					return C_TERMINATE;
				}
			} else  if (mode == crvCmdFromTangent) {
				DIST_T dp = FindDistance(Da.pos0, pos)*sin(D2R(angle2));
				Translate( &pos, Da.pos0, angle1-90.0, dp );
				Da.pos1 = pos;
			} else {
				DIST_T dp = -FindDistance(Da.pos0, pos)*sin(D2R(angle2));
				if (angle2 > 180.0)
					Translate( &pos, Da.pos0, angle1+90.0, dp );
				else
					Translate( &pos, Da.pos0, angle1-90.0, dp );
				Da.pos1 = pos;
			}
			if (FindDistance(Da.pos0,Da.pos1)<minLength) {
				ErrorMessage( MSG_TRK_TOO_SHORT, "Curved ", PutDim(FindDistance(Da.pos0,Da.pos1)) );
				return C_TERMINATE;
			}
		}
		switch (mode) {
		case crvCmdFromEP1:			
		case crvCmdFromTangent:
		case crvCmdFromCenter:
			for (int i=0;i<(*anchor_array).cnt;i++) {
				DYNARR_N(trkSeg_t,*anchor_array,i).color = drawColorRed;
			}
			break;
		}
		message( _("Drag on Red arrows to adjust curve") );
		return C_CONTINUE;

	default:
		return C_CONTINUE;

	}
}



static STATUS_T CmdCurve( wAction_t action, coOrd pos )
{
	track_p t;
	DIST_T d;
	static int segCnt;
	STATUS_T rc = C_CONTINUE;

	switch (action) {

	case C_START:
		curveMode = (long)commandContext;
		Da.state = -1;
		Da.pos0 = pos;
		tempSegs_da.cnt = 0;
		segCnt = 0;
		STATUS_T rcode;
		DYNARR_RESET(trkSeg_t,anchors_da);
		return CreateCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, &anchors_da, InfoMessage );

	case C_DOWN:
		if (Da.state == -1) {
			BOOL_T found = FALSE;
			if (curveMode != crvCmdFromCenter ) {
				if (((MyGetKeyState() & WKEY_ALT)==0) == magneticSnap) {
					if ((t = OnTrack(&pos,FALSE,TRUE))!=NULL) {
					   EPINX_T ep = PickUnconnectedEndPointSilent(pos, t);
					   if (ep != -1) {
						   if (GetTrkGauge(t) != GetScaleTrackGauge(GetLayoutCurScale())) {
								wBeep();
								InfoMessage(_("Track is different gauge"));
								return C_CONTINUE;
							}
							pos = GetTrkEndPos(t, ep);
							found = TRUE;
					   }
					}
				}
			}
			if (!found) SnapPos( &pos );
			Da.pos0 = Da.pos1 = pos;
			Da.state = 0;
			rcode = CreateCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, &anchors_da, InfoMessage );
			segCnt = tempSegs_da.cnt ;
			if (!Da.down) Da.state = -1;
			return rcode;
			//Da.pos0 = pos;
		}
		//This is where the user could adjust - if we allow that?
		tempSegs_da.cnt = segCnt;
		return C_CONTINUE;


	case wActionMove:
		if ((Da.state<0) && (curveMode != crvCmdFromCenter)) {
			DYNARR_RESET(trkSeg_t,anchors_da);
			if (((MyGetKeyState() & WKEY_ALT)==0) == magneticSnap) {
				if ((t=OnTrack(&pos,FALSE,TRUE))!= NULL) {
					if (GetTrkGauge(t) == GetScaleTrackGauge(GetLayoutCurScale())) {
						EPINX_T ep = PickUnconnectedEndPointSilent(pos, t);
						if (ep != -1) {
							pos = GetTrkEndPos(t, ep);
							CreateEndAnchor(pos,&anchors_da,FALSE);
						}
					}
				}
			}
		}
		return C_CONTINUE;

	case C_MOVE:
		if (Da.state<0) return C_CONTINUE;
		if ( Da.state == 0 ) {
		    Da.pos1 = pos;
			rc = CreateCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, &anchors_da, InfoMessage );
			segCnt = tempSegs_da.cnt ;
		} else {
			DYNARR_RESET(trkSeg_t,anchors_da);
			// SnapPos( &pos );
			tempSegs_da.cnt = segCnt;
			if (Da.trk) PlotCurve( curveMode, Da.pos0, Da.pos1, pos, &Da.curveData, FALSE );
			else PlotCurve( curveMode, Da.pos0, Da.pos1, pos, &Da.curveData, TRUE );
			if (Da.curveData.type == curveTypeStraight) {
				tempSegs(0).type = SEG_STRTRK;
				tempSegs(0).u.l.pos[0] = Da.pos0;
				tempSegs(0).u.l.pos[1] = Da.curveData.pos1;
				tempSegs_da.cnt = 1;
				segCnt = 1;
				InfoMessage( _("Straight Track: Length=%s Angle=%0.3f"),
						FormatDistance(FindDistance( Da.pos0, Da.curveData.pos1 )),
						PutAngle(FindAngle( Da.pos0, Da.curveData.pos1 )) );
				DrawArrowHeadsArray(&anchors_da,Da.curveData.pos1,FindAngle(Da.pos0, Da.curveData.pos1)+90,TRUE,wDrawColorRed);
			} else if (Da.curveData.type == curveTypeNone) {
				tempSegs_da.cnt = 0;
				segCnt = 0;
				InfoMessage( _("Back") );
			} else if (Da.curveData.type == curveTypeCurve) {
				tempSegs(0).type = SEG_CRVTRK;
				tempSegs(0).u.c.center = Da.curveData.curvePos;
				tempSegs(0).u.c.radius = Da.curveData.curveRadius;
				tempSegs(0).u.c.a0 = Da.curveData.a0;
				tempSegs(0).u.c.a1 = Da.curveData.a1;
				tempSegs_da.cnt = 1;
				segCnt = 1;
				d = D2R(Da.curveData.a1);
				if (d < 0.0)
					d = 2*M_PI+d;
				if ( d*Da.curveData.curveRadius > mapD.size.x+mapD.size.y ) {
					ErrorMessage( MSG_CURVE_TOO_LARGE );
					tempSegs_da.cnt = 0;
					Da.curveData.type = curveTypeNone;
					mainD.funcs->options = 0;
					return C_CONTINUE;
				}
				InfoMessage( _("Curved Track: Radius=%s Angle=%0.3f Length=%s"),
						FormatDistance(Da.curveData.curveRadius), Da.curveData.a1,
						FormatDistance(Da.curveData.curveRadius*d) );
				coOrd pos1;
				Translate(&pos1,Da.curveData.curvePos,Da.curveData.a0+Da.curveData.a1,Da.curveData.curveRadius);
				if (curveMode == crvCmdFromEP1 || curveMode == crvCmdFromChord)
					DrawArrowHeadsArray(&anchors_da,pos,FindAngle(Da.curveData.curvePos,pos),TRUE,wDrawColorRed);
				else if (curveMode == crvCmdFromTangent || curveMode == crvCmdFromCenter) {
					CreateEndAnchor(Da.curveData.pos2,&anchors_da,FALSE);
					DrawArrowHeadsArray(&anchors_da,Da.curveData.pos2,FindAngle(Da.curveData.curvePos,Da.curveData.pos2)+90,TRUE,wDrawColorRed);
				}
				CreateEndAnchor(Da.curveData.curvePos,&anchors_da,TRUE);
			}
		}
		mainD.funcs->options = 0;
		return rc;
	case C_TEXT:
		if ( Da.state == 0 )
			return CreateCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, &anchors_da, InfoMessage );
		/*no break*/
	case C_UP:
		if (Da.state<0) return C_CONTINUE;
		if (Da.state == 0 && ((curveMode != crvCmdFromChord) || (curveMode == crvCmdFromChord && !Da.trk))) {
			SnapPos( &pos );
			Da.pos1 = pos;
			if ((d = FindDistance(Da.pos0,Da.pos1))<minLength) {
				ErrorMessage( MSG_TRK_TOO_SHORT, "Curved ", PutDim(fabs(minLength-d)) );
				return C_TERMINATE;
			}
			Da.state = 1;
			CreateCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, &anchors_da, InfoMessage );
			tempSegs_da.cnt = 1;
			mainD.funcs->options = 0;
			segCnt = tempSegs_da.cnt;
			InfoMessage( _("Drag on Red arrows to adjust curve") );
			return C_CONTINUE;
		} else if ((curveMode == crvCmdFromChord && Da.state == 0 && Da.trk)) {
			pos = Da.middle;
			if ((d = FindDistance(Da.pos0,Da.pos1))<minLength) {
				ErrorMessage( MSG_TRK_TOO_SHORT, "Curved ", PutDim(fabs(minLength-d)) );
				return C_TERMINATE;
			}
			PlotCurve( curveMode, Da.pos0, Da.pos1, Da.middle, &Da.curveData, TRUE );
		}
		mainD.funcs->options = 0;
		tempSegs_da.cnt = 0;
		segCnt = 0;
		Da.state = -1;
		DYNARR_RESET(trkSeg_t,anchors_da);          // No More anchors for this one
		if (Da.curveData.type == curveTypeStraight) {
			if ((d = FindDistance( Da.pos0, Da.curveData.pos1 )) < minLength) {
				ErrorMessage( MSG_TRK_TOO_SHORT, "Curved ", PutDim(fabs(minLength-d)) );
				return C_TERMINATE;
			}
			UndoStart( _("Create Straight Track"), "newCurve - straight" );
			t = NewStraightTrack( Da.pos0, Da.curveData.pos1 );
			if (Da.trk) {
				EPINX_T ep = PickUnconnectedEndPoint(Da.pos0, t);
				if (ep != -1) ConnectTracks(Da.trk, Da.ep, t, ep);
			}
			UndoEnd();
		} else if (Da.curveData.type == curveTypeCurve) {
			if ((d = Da.curveData.curveRadius * Da.curveData.a1 *2.0*M_PI/360.0) < minLength) {
				ErrorMessage( MSG_TRK_TOO_SHORT, "Curved ", PutDim(fabs(minLength-d)) );
				return C_TERMINATE;
			}
			UndoStart( _("Create Curved Track"), "newCurve - curve" );
			t = NewCurvedTrack( Da.curveData.curvePos, Da.curveData.curveRadius,
					Da.curveData.a0, Da.curveData.a1, 0 );
			if (Da.trk) {
				EPINX_T ep = PickUnconnectedEndPoint(Da.pos0, t);
				if (ep != -1) ConnectTracks(Da.trk, Da.ep, t, ep);
			}
			UndoEnd();
		} else {
			return C_ERROR;
		}
		DrawNewTrack( t );
		return C_TERMINATE;

	case C_REDRAW:
		if ( Da.state >= 0 ) {
			DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
			mainD.funcs->options = 0;
		}
		if (anchors_da.cnt)
			DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	case C_CANCEL:
		if (Da.state == 1) {
			tempSegs_da.cnt = 0;
			Da.trk = NULL;
		}
		DYNARR_RESET(trkSeg_t,anchors_da);
		DYNARR_RESET(trkSeg_t,tempSegs_da);
		Da.state = -1;
		segCnt = 0;
		return C_CONTINUE;

	}

	return C_CONTINUE;

}



static DIST_T circleRadius = 18.0;
static long helixTurns = 5;
static ANGLE_T helixAngSep = 0.0;
static DIST_T helixElev = 0.0;
static DIST_T helixRadius = 18.0;
static DIST_T helixGrade = 0.0;
static DIST_T helixVertSep = 0.0;
static DIST_T origVertSep = 0.0;
static wWin_p helixW;
#define H_ELEV			(0)
#define H_RADIUS		(1)
#define H_TURNS			(2)
#define H_ANGSEP		(3)
#define H_GRADE			(4)
#define H_VERTSEP		(5)
static int h_orders[7];
static int h_clock;

EXPORT long circleMode;

static void ComputeHelix( paramGroup_p, int, void * );

static paramFloatRange_t r0_360 = { 0, 360 };
static paramFloatRange_t r0_1000000 = { 0, 1000000 };
static paramIntegerRange_t i1_1000000 = { 1, 1000000 };
static paramFloatRange_t r1_10000 = { 1, 10000 };
static paramFloatRange_t r0_100= { 0, 100 };

static paramData_t helixPLs[] = {
	{ PD_FLOAT, &helixElev, "elev", PDO_DIM, &r0_1000000, N_("Elevation Difference") },
	{ PD_FLOAT, &helixRadius, "radius", PDO_DIM, &r1_10000, N_("Radius") },
	{ PD_LONG, &helixTurns, "turns", 0, &i1_1000000, N_("Turns") },
	{ PD_FLOAT, &helixAngSep, "angSep", 0, &r0_360, N_("Angular Separation") },
	{ PD_FLOAT, &helixGrade, "grade", 0, &r0_100, N_("Grade") },
	{ PD_FLOAT, &helixVertSep, "vertSep", PDO_DIM, &r0_1000000, N_("Vertical Separation") },
#define I_HELIXMSG		(6)
	{ PD_MESSAGE, N_("Total Length"), NULL, PDO_DLGRESETMARGIN, (void*)200 } };
static paramGroup_t helixPG = { "helix", PGO_PREFMISCGROUP, helixPLs, sizeof helixPLs/sizeof helixPLs[0] };

static paramData_t circleRadiusPLs[] = {
	{ PD_FLOAT, &circleRadius, "radius", PDO_DIM, &r1_10000 } };
static paramGroup_t circleRadiusPG = { "circle", 0, circleRadiusPLs, sizeof circleRadiusPLs/sizeof circleRadiusPLs[0] };


static void ComputeHelix(
		paramGroup_p pg,
		int h_inx,
		void * data )
{
	DIST_T totTurns;
	DIST_T length;
	long updates = 0;
	if ( h_inx < 0 || h_inx >= sizeof h_orders/sizeof h_orders[0] )
		return;
	ParamLoadData( &helixPG );
	totTurns = helixTurns + helixAngSep/360.0;
	length = totTurns * helixRadius * (2 * M_PI);
	h_orders[h_inx] = ++h_clock;
	switch ( h_inx ) {
	case H_ELEV:
		if (h_orders[H_TURNS]<h_orders[H_VERTSEP] &&
			origVertSep > 0.0) {
			helixTurns = (int)floor(helixElev/origVertSep - helixAngSep/360.0);
			totTurns = helixTurns + helixAngSep/360.0;
			updates |= (1<<H_TURNS);
		}
		if (totTurns > 0) {
			helixVertSep = helixElev/totTurns;
			updates |= (1<<H_VERTSEP);
		}
		break;
	case H_TURNS:
	case H_ANGSEP:
		helixVertSep = helixElev/totTurns;
		updates |= (1<<H_VERTSEP);
		break;
	case H_VERTSEP:
		if (helixVertSep > 0.0) {
			origVertSep = helixVertSep;
			helixTurns = (int)floor(helixElev/origVertSep - helixAngSep/360.0);
			updates |= (1<<H_TURNS);
			totTurns = helixTurns + helixAngSep/360.0;
			if (totTurns > 0) {
				helixVertSep = helixElev/totTurns;
				updates |= (1<<H_VERTSEP);
			}
		}
		break;
	case H_GRADE:
	case H_RADIUS:
		break;
	}
	if ( totTurns > 0.0 ) {
		if ( h_orders[H_RADIUS]>=h_orders[H_GRADE] ||
			 (helixGrade==0.0 && totTurns>0 && helixRadius>0) ) {
			if ( helixRadius > 0.0 ) {
				helixGrade = helixElev/(totTurns*helixRadius*(2*M_PI))*100.0;
				updates |= (1<<H_GRADE);
			}
		} else {
			if( helixGrade > 0.0 ) {
				helixRadius = helixElev/(totTurns*(helixGrade/100.0)*2.0*M_PI);
				updates |= (1<<H_RADIUS);
			}
		}
	}
	length = totTurns * helixRadius * (2 * M_PI);
	for ( h_inx=0; updates; h_inx++,updates>>=1 ) {
		if ( (updates&1) )
			ParamLoadControl( &helixPG, h_inx );
	}
	if (length > 0.0)
		sprintf( message, _("Total Length  %s"), FormatDistance(length) );
	else
		strcpy( message, "                           " );
	ParamLoadMessage( &helixPG, I_HELIXMSG, message );
}


static void HelixCancel( wWin_p win )
{
	wHide( helixW );
}


static void ChangeHelixW( long changes )
{
	if ( (changes & CHANGE_UNITS) &&
		 helixW != NULL &&
		 wWinIsVisible(helixW) ) {
		ParamLoadControls( &helixPG );
		ComputeHelix( NULL, 6, NULL );
	}
}




static STATUS_T CmdCircleCommon( wAction_t action, coOrd pos, BOOL_T helix )
{
	track_p t;
	static coOrd pos0;
	wControl_p controls[2];
	char * labels[1];

	switch (action) {

	case C_START:
		if (helix) {
			if (helixW == NULL)
				helixW = ParamCreateDialog(&helixPG, MakeWindowTitle(_("Helix")), NULL, NULL, HelixCancel, TRUE, NULL, 0, ComputeHelix);
			ParamLoadControls(&helixPG);
			ParamGroupRecord(&helixPG);
			ComputeHelix(NULL, 6, NULL);
			wShow(helixW);
			memset(h_orders, 0, sizeof h_orders);
			h_clock = 0;
		} else {
			ParamLoadControls(&circleRadiusPG);
			ParamGroupRecord(&circleRadiusPG);
			switch (circleMode) {
			case circleCmdFixedRadius:
				controls[0] = circleRadiusPLs[0].control;
				controls[1] = NULL;
				labels[0] = N_("Circle Radius");
				InfoSubstituteControls(controls, labels);
				break;
			case circleCmdFromTangent:
				InfoSubstituteControls(NULL, NULL);
				InfoMessage(_("Click on Circle Edge"));
				break;
			case circleCmdFromCenter:
				InfoSubstituteControls(NULL, NULL);
				InfoMessage(_("Click on Circle Center"));
				break;
			}
		}
		tempSegs_da.cnt = 0;
		return C_CONTINUE;

	case C_DOWN:
		DYNARR_SET(trkSeg_t, tempSegs_da, 1);
		tempSegs_da.cnt = 0;
		if (helix) {
			if (helixRadius <= 0.0) {
				ErrorMessage(MSG_RADIUS_GTR_0);
				return C_ERROR;
			}
			if (helixTurns <= 0) {
				ErrorMessage(MSG_HELIX_TURNS_GTR_0);
				return C_ERROR;
			}
			ParamLoadData(&helixPG);
		} else {
			ParamLoadData(&circleRadiusPG);
			switch (circleMode) {
			case circleCmdFixedRadius:
				if (circleRadius <= 0.0) {
					ErrorMessage(MSG_RADIUS_GTR_0);
					return C_ERROR;
				}
				break;
			case circleCmdFromTangent:
				InfoSubstituteControls(NULL, NULL);
				InfoMessage(_("Drag to Center"));
				break;
			case circleCmdFromCenter:
				InfoSubstituteControls(NULL, NULL);
				InfoMessage(_("Drag to Edge"));
				break;
			}
		}
		SnapPos(&pos);
		tempSegs(0).u.c.center = pos0 = pos;
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		return C_CONTINUE;

	case C_MOVE:
		SnapPos(&pos);
		tempSegs(0).u.c.center = pos;
		if (!helix) {
			switch (circleMode) {
			case circleCmdFixedRadius:
				break;
			case circleCmdFromCenter:
				tempSegs(0).u.c.center = pos0;
				circleRadius = FindDistance(tempSegs(0).u.c.center, pos);
				InfoMessage(_("Radius=%s"), FormatDistance(circleRadius));
				break;
			case circleCmdFromTangent:
				circleRadius = FindDistance(tempSegs(0).u.c.center, pos0);
				InfoMessage(_("Radius=%s"), FormatDistance(circleRadius));
				break;
			}
		}
		tempSegs(0).type = SEG_CRVTRK;
		tempSegs(0).u.c.radius = helix ? helixRadius : circleRadius;
		tempSegs(0).u.c.a0 = 0.0;
		tempSegs(0).u.c.a1 = 360.0;
		tempSegs_da.cnt = 1;
		return C_CONTINUE;

	case C_UP:
		if (helix) {
			if (helixRadius > mapD.size.x || helixRadius > mapD.size.y) {
				ErrorMessage(MSG_RADIUS_TOO_BIG);
				return C_ERROR;
			}
			if (helixRadius > 10000) {
				ErrorMessage(MSG_RADIUS_GTR_10000);
				return C_ERROR;
			}
			UndoStart(_("Create Helix Track"), "newHelix");
			t = NewCurvedTrack(tempSegs(0).u.c.center, helixRadius, 0.0, 0.0, helixTurns);
		} else {
			if (circleRadius > mapD.size.x || circleRadius > mapD.size.y) {
				ErrorMessage(MSG_RADIUS_TOO_BIG);
				return C_ERROR;
			}
			if (circleRadius <= 0) {
				ErrorMessage(MSG_RADIUS_GTR_0);
				return C_ERROR;
			}
			if ((circleRadius > 100000) || (helixRadius > 10000)) {
				ErrorMessage(MSG_RADIUS_GTR_10000);
				return C_ERROR;
			}
			UndoStart(_("Create Circle Track"), "newCircle");
			t = NewCurvedTrack(tempSegs(0).u.c.center, circleRadius, 0.0, 0.0, 0);
		}
		UndoEnd();
		DrawNewTrack(t);
		if (helix)
			wHide( helixW );
		else
			InfoSubstituteControls( NULL, NULL );
		tempSegs_da.cnt = 0;
		return C_TERMINATE;

	case C_REDRAW:
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	case C_CANCEL:
		if (helix)
			wHide( helixW );
		else
			InfoSubstituteControls( NULL, NULL );
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}
}


static STATUS_T CmdCircle( wAction_t action, coOrd pos )
{
	if ( action == C_START ) {
		circleMode = (long)commandContext;
	}
	return CmdCircleCommon( action, pos, FALSE );
}


static STATUS_T CmdHelix( wAction_t action, coOrd pos )
{
	return CmdCircleCommon( action, pos, TRUE );
}

#include "bitmaps/curve1.xpm"
#include "bitmaps/curve2.xpm"
#include "bitmaps/curve3.xpm"
#include "bitmaps/curve4.xpm"
#include "bitmaps/bezier.xpm"
#include "bitmaps/cornu.xpm"
#include "bitmaps/circle1.xpm"
#include "bitmaps/circle2.xpm"
#include "bitmaps/circle3.xpm"

EXPORT void InitCmdCurve( wMenu_p menu )
{
	AddMenuButton( menu, CmdCornu, "cmdCornu", _("Cornu Curve"), wIconCreatePixMap(cornu_xpm), LEVEL0_50, IC_STICKY|IC_POPUP2|IC_WANT_MOVE, ACCL_CORNU, (void*)cornuCmdCreateTrack);

	ButtonGroupBegin( _("Curve Track"), "cmdCircleSetCmd", _("Curve Tracks") );
	AddMenuButton( menu, CmdCurve, "cmdCurveEndPt", _("Curve from End-Pt"), wIconCreatePixMap( curve1_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2|IC_WANT_MOVE, ACCL_CURVE1, (void*)0 );
	AddMenuButton( menu, CmdCurve, "cmdCurveTangent", _("Curve from Tangent"), wIconCreatePixMap( curve2_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2|IC_WANT_MOVE, ACCL_CURVE2, (void*)1 );
	AddMenuButton( menu, CmdCurve, "cmdCurveCenter", _("Curve from Center"), wIconCreatePixMap( curve3_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2|IC_WANT_MOVE, ACCL_CURVE3, (void*)2 );
	AddMenuButton( menu, CmdCurve, "cmdCurveChord", _("Curve from Chord"), wIconCreatePixMap( curve4_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2|IC_WANT_MOVE, ACCL_CURVE4, (void*)3 );
	AddMenuButton( menu, CmdBezCurve, "cmdBezier", _("Bezier Curve"), wIconCreatePixMap(bezier_xpm), LEVEL0_50, IC_STICKY|IC_POPUP2|IC_WANT_MOVE, ACCL_BEZIER, (void*)bezCmdCreateTrack );
	ButtonGroupEnd();

	ButtonGroupBegin( _("Circle Track"), "cmdCurveSetCmd", _("Circle Tracks") );
	AddMenuButton( menu, CmdCircle, "cmdCircleFixedRadius", _("Fixed Radius Circle"), wIconCreatePixMap( circle1_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_CIRCLE1, (void*)0 );
	AddMenuButton( menu, CmdCircle, "cmdCircleTangent", _("Circle from Tangent"), wIconCreatePixMap( circle2_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_CIRCLE2, (void*)1 );
	AddMenuButton( menu, CmdCircle, "cmdCircleCenter", _("Circle from Center"), wIconCreatePixMap( circle3_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_CIRCLE3, (void*)2 );
	ButtonGroupEnd();

	ParamRegister( &circleRadiusPG );
	ParamCreateControls( &circleRadiusPG, NULL );

}

/**
* Append the helix command to the pulldown menu. The helix doesn't use an icon, so it is only
* available through the pulldown
*
* \param varname1 IN pulldown menu
* \return
*/

void InitCmdHelix(wMenu_p menu)
{
    AddMenuButton(menu, CmdHelix, "cmdHelix", _("Helix"), NULL, LEVEL0_50,
                  IC_STICKY|IC_INITNOTSTICKY|IC_POPUP2, ACCL_HELIX, NULL);
    ParamRegister(&helixPG);
    RegisterChangeNotification(ChangeHelixW);
}
