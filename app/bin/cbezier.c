/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/cbezier.c,v 1.4 2008-03-06 19:35:04 arichards Exp $
 *
 * Bezier Command
 *
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


#include "track.h"
#include "ccurve.h"
#include "cbezier.h"
#include "cstraigh.h"
#include "cjoin.h"
#include "i18n.h"


/*
 * STATE INFO
 */

static struct {
		STATE_T state;
		coOrd pos[4];
        int selectPoint;
		curveData_t curveData;
		track_p trk[2];
		EPINX_T ep[2];
		} Da;

/*
 * Draw a ControlArm. 
 * If the end or control point is unlocked place a filled circle on it.
 * By convention, A red color indicates that this arm, end or control point is "active".
 */

EXPORT int DrawControlArm(
                           trkSeg_p sp,
                           coOrd pos0,
                           coOrd pos1,
                           BOOL_T end_lock,
                           BOOL_T cp_lock,
                           wDrawColor color_arm,
                           wDrawColor color_end,
                           wDrawColor color_cp )
{
    coOrd p0, p1;
    DIST_T d, w;
    int inx;
    d = mainD.scale*0.25;
    w = mainD.scale/mainD.dpi*4; /*double width*/
    for ( inx=0; inx<2; inx++ ) {
        sp[inx].type = SEG_STRLIN;
        sp[inx].width = w;
        sp[inx].color = color_arm;
    }
    int i=0;
    sp[i].u.l.pos[0] = pos0;
    sp[i].u.l.pos[1] = pos1;
    if (!cp_lock) { 
    	sp[i].type = SEG_FILCRCL;
    	sp[i].u.c.center = pos1;
    	sp[i].u.c.radius = d;
    	sp[inx].color = color_cp;
    	i++;
    }
    if (!end_lock) {
    	sp[i].type = SEG_FILCRCL;
    	sp[i].u.c.center = pos0;
    	sp[i].u.c.radius = d;
    	sp[inx].color = color_end;
    	i++;
    }
    return i;
}


EXPORT STATUS_T CreateBezCurve(
		wAction_t action,
		coOrd pos,
		BOOL_T track,
		wDrawColor color,
		DIST_T width,
		long mode,
		int controlArm,
		int selectedPoint,
		bezMessageProc message )
{
	track_p t;
	DIST_T d;
	ANGLE_T a, angle1, angle2;
	static coOrd pos0, pos3, p;
	int inx;

	switch ( action ) {
	case C_START:
		DYNARR_SET( trkSeg_t, tempSegs_da, 8 )
		if (track) 
			InfoMessage( _("Drag from End-Point in direction of curve - Shift locks to track open end-point") );
		else 	
			InfoMessage( _("Drag from End-Point in direction of curve - Shift locks to line end-point") );

		return C_CONTINUE;
	case C_DOWN:
		for ( inx=0; inx<8; inx++ ) {
			tempSegs(inx).color = wDrawColorBlack;
			tempSegs(inx).width = 0;
		}
		tempSegs_da.cnt = 0;
		p = pos;
		BOOL_T found = FALSE;
		Da.trk[controlArm] = NULL;	    
		if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
			if (track) {
				if ((t = OnTrack(&p, TRUE, TRUE)) != NULL) {
			   		EPINX_T ep = PickUnconnectedEndPoint(p, t);
			   		if (ep != -1) {
			   			Da.trk[controlArm] = t;
			   			Da.ep[controlArm] = ep;
						pos = GetTrkEndPos(t, ep);
		   				found = TRUE;
		  			}
		   		}
		   	} else {
		   		if ((t = OnTrack2(&p,TRUE, FALSE, TRUE)) != NULL) {
		   			if (!IsTrack(t)) {
		 				Da.trk[controlArm] = t;
		   				EPINX_T ep = PickEndPoint(p,t);
		   				Da.ep[controlArm] =	ep;
		   				pos = GetTrkEndPos(t, ep);
			   			found = TRUE;
			   		}
			   	}
			}
		} 	
		if (!found) SnapPos( &pos );
		
		if (controlArm == 0) {
			pos0 = pos;
			Da.pos[0] = pos;
			Da.pos[1] = pos;
			tempSegs(0).u.b.pos[0] = pos;
        	tempSegs(0).u.b.pos[1] = pos;
		} else pos3 = pos;
		Da.pos[2] = pos;
		Da.pos[3] = pos;
		tempSegs(0).u.b.pos[2] = pos;
        tempSegs(0).u.b.pos[3] = pos;
		
		tempSegs(0).type = (track?SEG_BEZTRK:SEG_BEZLIN);
		tempSegs(0).color = color;
		tempSegs(0).width = width;
				
		if (*Da.trk) message(_("End Locked: Drag out control arm"));
		else message( _("Drag out control arm"));
        
		return C_CONTINUE;

	case C_MOVE:
		if (Da.trk[controlArm]) {
			angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[controlArm], Da.ep[controlArm]));
			angle2 = NormalizeAngle(FindAngle(pos, Da.pos[selectedPoint])-angle1);
			if (angle2 > 90.0 && angle2 < 270.0)
				Translate( &pos, Da.pos[controlArm*3], angle1, -FindDistance( Da.pos[controlArm*3], pos )*cos(D2R(angle2)) );
			else pos = Da.pos[controlArm*3];
		} else SnapPos(&pos);
		tempSegs(0).u.b.pos[selectedPoint] = pos;
		if (controlArm == 0) {
			d = FindDistance( pos0, pos );
			a = FindAngle( pos0, pos );
		} else {
			d = FindDistance( pos3, pos );
			a = FindAngle( pos3, pos );
		}
		if (Da.trk[controlArm]) message( _("End Locked: Drag out control arm - Angle=%0.3f"), PutAngle(a));
		else message( _("Drag out control arm - Angle=%0.3f"), PutAngle(a) );
		tempSegs_da.cnt = 1;
		tempSegs_da.cnt += 
			DrawControlArm(&tempSegs(1),Da.pos[controlArm*3],pos, TRUE, FALSE, 
				drawColorBlack, drawColorBlack, drawColorRed);
		if (controlArm > 0) 
	 		tempSegs_da.cnt += 
	 			DrawControlArm(&tempSegs(tempSegs_da.cnt),Da.pos[0],Da.pos[1], FALSE, FALSE, 
	 			drawColorBlack, drawColorBlack, drawColorRed);
		return C_CONTINUE;

	case C_UP:
		if (Da.trk[controlArm]) {
			angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[controlArm], Da.ep[controlArm]));
			angle2 = NormalizeAngle(FindAngle(pos, pos0)-angle1);
			if (angle2 > 90.0 && angle2 < 270.0) {			
				Translate( &pos, Da.pos[controlArm*3], angle1, -FindDistance( Da.pos[controlArm*3], pos )*cos(D2R(angle2)) );
				Da.pos[controlArm+1] = pos;
			} else {
				ErrorMessage( MSG_TRK_TOO_SHORT, "Bezier ", PutDim(0.0) );
				return C_TERMINATE;
			}
		}	
		tempSegs_da.cnt = 1;		
		tempSegs_da.cnt += 
			DrawControlArm( &tempSegs(1), Da.pos[controlArm*3], pos, TRUE, TRUE, 
				drawColorBlack, drawColorBlack, drawColorBlack );

        if (controlArm == 0)
        	message( _("Select 2nd End - Shift will lock to unconnected track") );
		return C_CONTINUE;

	default:
		return C_CONTINUE;

   }
}



static STATUS_T CmdBezCurve( wAction_t action, coOrd pos )
{
	track_p t;
	DIST_T d;
	static int segCnt;
	STATUS_T rc = C_CONTINUE;

	switch (action) {

	case C_START:
		curveMode = (long)commandContext;
		Da.state = -1;
		tempSegs_da.cnt = 0;
		return CreateBezCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, 0, InfoMessage );
		
	case C_TEXT:
		if ( Da.state == 0 )
			return CreateBezCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, 0, InfoMessage );
		else
			return CreateBezCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, 1, InfoMessage );

	case C_DOWN:
		if ( Da.state == -1 ) {
			SnapPos( &pos );
			Da.pos[0] = pos;
			Da.state = 0;  //Draw 
			Da.selectPoint = 1;
			return CreateBezCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, 0, InfoMessage );
			//Da.pos0 = pos;
		} else if (Da.state == 1) {
			SnapPos( &pos );
			Da.pos[3] = pos;
			Da.state = 2;
			Da.selectPoint = 3;
			tempSegs_da.cnt = segCnt;
			return CreateBezCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, 1, InfoMessage );
		} else {
			double dd = 0;
			Da.selectPoint = -1;
		    for (int i=0;i<3;i++) {
		    	if (Da.selectPoint == -1 || FindDistance(Da.pos[i],pos) < dd) {
		    		if ((i==0 && Da.trk[0]) || (i==3 && Da.trk[1])) continue;
		    		dd = FindDistance(Da.pos[i],pos);
		    		Da.selectPoint = i;
		    	}
		    	if (!close(dd)) Da.selectPoint = 0;
		    	else pos = Da.pos[Da.selectPoint]; 
		    }
		    switch (Da.selectPoint) {
		    	case 1: DrawControlArm(&tempSegs(1),Da.pos[0],Da.pos[1], TRUE, FALSE, 
							drawColorRed, drawColorBlack, drawColorBlack, drawColorRed); 
						break;
		    	case 2: DrawControlArm(&tempSegs(1),Da.pos[0],Da.pos[1], FALSE, TRUE, 
							drawColorRed, drawColorBlack, drawColorBlack, drawColorRed); 
						break;
		    	case 3: DrawControlArm(&tempSegs(1),Da.pos[2],Da.pos[3], TRUE, FALSE, 
							drawColorRed, drawColorBlack, drawColorBlack, drawColorRed); 
						break;
		    	case 4: DrawControlArm(&tempSegs(1),Da.pos[2], Da.pos[3], TRUE, FALSE, 
							drawColorRed, drawColorBlack, drawColorBlack, drawColorRed); 
						break;
		    	default:
			}
			
			DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
			mainD.funcs->options = 0;
		    
		} 	 
		return C_CONTINUE;
			
	case C_MOVE:
		mainD.funcs->options = wDrawOptTemp;
		DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
        if ( Da.state == 0 || Da.state == 2) {
			SnapPos( &pos );
			//Da.pos[1] = pos;
			rc = CreateBezCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, Da.state/2 , InfoMessage );
		} else if (Da.state == 3) {
			if (track && Da.trk[Da.selectPoint/2]) {
				angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[Da.selectPoint],Da.pos[0]);
				angle2 = NormalizeAngle(FindAngle(pos, Da.pos[Da.selectPoint])-angle1);
				if (angle2 > 90.0 && angle2 < 270.0)
					Translate( &pos, Da.pos[Da.selectPoint/2], angle1, -FindDistance( Da.pos[Da.selectPoint/2], pos )*cos(D2R(angle2)) );
				else pos = Da.pos[Da.selectPoint];
				switch (Da.selectPoint) {
		    	case 1: DrawControlArm(&tempSegs(1),Da.pos[0],Da.pos[1], TRUE, FALSE, 
							drawColorRed, drawColorBlack, drawColorBlack, drawColorRed); 
						break;
		    	case 2: DrawControlArm(&tempSegs(1),Da.pos[0],Da.pos[1], FALSE, TRUE, 
							drawColorRed, drawColorBlack, drawColorBlack, drawColorRed); 
						break;
		    	case 3: DrawControlArm(&tempSegs(1),Da.pos[2],Da.pos[3], TRUE, FALSE, 
							drawColorRed, drawColorBlack, drawColorBlack, drawColorRed); 
						break;
		    	case 4: DrawControlArm(&tempSegs(1),Da.pos[2], Da.pos[3], TRUE, FALSE, 
							drawColorRed, drawColorBlack, drawColorBlack, drawColorRed); 
						break;
		    	default:
		    	}
			}		
		} else {
			return C_CONTINUE;
		}
		DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		mainD.funcs->options = 0;
		return rc;


	case C_UP:
		mainD.funcs->options = wDrawOptTemp;
		DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		if (Da.state == 0 || Da.state == 2) {
			SnapPos( &pos );
			if (Da.state == 0) Da.pos[1]= pos;
			else Da.pos[2] = pos;
			Da.state = Da.state + 1;
			CreateBezCurve( action, pos, TRUE, wDrawColorBlack, 0, curveMode, Da.state/2, InfoMessage );
			DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
			mainD.funcs->options = 0;
			segCnt = tempSegs_da.cnt;
            else InfoMessage( _("Select control point or end and drag to adjust, Space or Enter to finish") );
			return C_CONTINUE;
        } else {
        	Snap(&pos);
        	Da.pos[Da.selectPoint] = pos;
			
			
			
			ainD.funcs->options = 0;
			tempSegs_da.cnt = 0;
			Da.state = -1;
			if (Da.curveData.type == curveTypeStraight) {
				if ((d=FindDistance( Da.pos[0], Da.curveData.pos1 )) <= minLength) {
					ErrorMessage( MSG_TRK_TOO_SHORT, "Curved ", PutDim(fabs(minLength-d)) );
					return C_TERMINATE;
				}
				UndoStart( _("Create Straight Track"), "newCurve - straight" );
				t = NewStraightTrack( Da.pos[0], Da.curveData.pos1 );
				UndoEnd();
			} else if (Da.curveData.type == curveTypeCurve) {
				if ((d= Da.curveData.curveRadius * Da.curveData.a1 *2.0*M_PI/360.0) <= minLength) {
					ErrorMessage( MSG_TRK_TOO_SHORT, "Curved ", PutDim(fabs(minLength-d)) );
					return C_TERMINATE;
				}
				UndoStart( _("Create Curved Track"), "newCurve - curve" );
				t = NewCurvedTrack( Da.curveData.curvePos, Da.curveData.curveRadius,
						Da.curveData.a0, Da.curveData.a1, 0 );
				if (Da.trk) {
					EPINX_T ep = PickUnconnectedEndPoint(Da.pos[0], t);
					if (ep != -1) ConnectTracks(Da.trk, Da.ep, t, ep);
				}
				UndoEnd();
			} else {
				return C_ERROR;
			}
			DrawNewTrack( t );
			return C_TERMINATE;
        }
		
            
    case C_OK:
         if ((d=BezierLength(Da.pos[0],Da.pos[1],Da.pos[2],Da.pos[3],0.0))<=minLength) {
                ErrorMessage( MSG_TRK_TOO_SHORT, "Bezier ", PutDim(fabs(minLength-d)) );
                return C_TERMINATE;
         }
         UndoStart( _("Create Bezier Track"), "newCurve - Bezier");
         t = NewBezierTrack(Da.pos[0],Da.pos[1],Da.pos[2],Da.pos[3]);
         UndoEnd();

        Da.state = -1;
        return C_TERMINATE;

	case C_REDRAW:
		if ( Da.state >= 0 ) {
			mainD.funcs->options = wDrawOptTemp;
			DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
			mainD.funcs->options = 0;
		}
		return C_CONTINUE;

	case C_CANCEL:
		if (Da.state == 1) {
			mainD.funcs->options = wDrawOptTemp;
			DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
			mainD.funcs->options = 0;
			tempSegs_da.cnt = 0;
			Da.trk = NULL;
		}
		Da.state = -1;
		return C_CONTINUE;
		
	default:


	return C_CONTINUE;
	}

}



EXPORT long circleMode;


#include "bitmaps/beztrack.xpm"
#include "bitmaps/bezline.xpm"

EXPORT void InitCmdBezier( wMenu_p menu )
{	
	AddMenuButton( menu, CreateBezCurve, "cmdBezierTrack", _("Bezier Track"), wIconCreatePixMap( beztrack_xpm ),
        LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_CURVE5, (void*)4 );
    AddMenuButton( menu, CreateBezCurve, "cmdBezierLine", _("Bezier Line"), wIconCreatePixMap( bezline_xpm ),
        LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_CURVE5, (void*)4 );    
	ParamRegister( &bezierPG );
	RegisterChangeNotification( ChangeBezierW );

}
