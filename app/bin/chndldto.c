/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/chndldto.c,v 1.4 2008-03-06 19:35:05 m_fischer Exp $
 *
 * CURVE
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
#include "cstraigh.h"
#include "cjoin.h"
#include "compound.h"
#include <math.h>
#include "i18n.h"

#define PTRACE(X)

/*
 * STATE INFO
 */
static struct {
		STATE_T state;
		coOrd normalP;
		ANGLE_T normalA;
		track_p normalT;
		coOrd reverseP;
		coOrd reverseP1;
		ANGLE_T reverseA;
		DIST_T frogNo;
		ANGLE_T frogA;
		curveData_t curveData;
		} Dhlt;


static STATUS_T CmdHandLaidTurnout( wAction_t action, coOrd pos )
{
	ANGLE_T angle, angle2, angle3, reverseR, pointA, reverseA1, angle0;
	EPINX_T ep, ep1, ep2, ep2a=-1, ep2b=-1, pointEp0, pointEp1;
	DIST_T dist, reverseD, pointD;
	coOrd off, intersectP;
	coOrd pointP, pointC, pointP1, reverseC, point0;
	track_p trk, trk1, trk2, trk2a=NULL, trk2b=NULL, pointT;
	trkSeg_p segP;
	BOOL_T right;
	track_p trks[4], *trkpp;

	switch (action) {

	case C_START:
		InfoMessage( _("Place frog and drag angle") );
		DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
		Dhlt.state = 0;
		Dhlt.normalT = NULL;
		tempSegs_da.cnt = 0;
		DYNARR_SET( trkSeg_t, tempSegs_da, 2 );
		tempSegs(0).color = drawColorBlack;
		tempSegs(0).width = 0;
		tempSegs(1).color = drawColorBlack;
		tempSegs(1).width = 0;
		return C_CONTINUE;

	case C_DOWN:
		if (Dhlt.state == 0) {
			if ((Dhlt.normalT = OnTrack( &pos, TRUE, TRUE )) == NULL)
				break;
			if ( QueryTrack( Dhlt.normalT, Q_NOT_PLACE_FROGPOINTS ) ) {
				ErrorMessage( MSG_CANT_PLACE_FROGPOINTS, _("frog") );
				Dhlt.normalT = NULL;
				break;
			}
			Dhlt.normalP = Dhlt.reverseP = Dhlt.reverseP1 = pos;
			Dhlt.normalA = GetAngleAtPoint( Dhlt.normalT, Dhlt.normalP, NULL, NULL );
			InfoMessage( _("Drag to set angle") );
			DrawLine( &tempD, Dhlt.reverseP, Dhlt.reverseP1, 0, wDrawColorBlack );
			Dhlt.state = 1;
			pointC = pointP = pointP1 = reverseC = zero;
			return C_CONTINUE;
		}

	case C_MOVE:
	case C_UP:
		if (Dhlt.normalT == NULL)
			break;
		if (Dhlt.state == 1) {
			DrawLine( &tempD, Dhlt.reverseP, Dhlt.reverseP1, 0, wDrawColorBlack );
			Dhlt.reverseP1 = pos;
			Dhlt.reverseA = FindAngle( Dhlt.reverseP, Dhlt.reverseP1 );
			Dhlt.frogA = NormalizeAngle( Dhlt.reverseA - Dhlt.normalA );
/*printf( "RA=%0.3f FA=%0.3f ", Dhlt.reverseA, Dhlt.frogA );*/
			if (Dhlt.frogA > 270.0) {
				Dhlt.frogA = 360.0-Dhlt.frogA;
				right = FALSE;
			} else if (Dhlt.frogA > 180) {
				Dhlt.frogA = Dhlt.frogA - 180.0;
				Dhlt.normalA = NormalizeAngle( Dhlt.normalA + 180.0 );
				/*ep = Dhlt.normalEp0; Dhlt.normalEp0 = Dhlt.normalEp1; Dhlt.normalEp1 = ep;*/
				right = TRUE;
			} else if (Dhlt.frogA > 90.0) {
				Dhlt.frogA = 180.0 - Dhlt.frogA;
				Dhlt.normalA = NormalizeAngle( Dhlt.normalA + 180.0 );
				/*ep = Dhlt.normalEp0; Dhlt.normalEp0 = Dhlt.normalEp1; Dhlt.normalEp1 = ep;*/
				right = FALSE;
			} else {
				right = TRUE;
			}
/*printf( "NA=%0.3f FA=%0.3f R=%d\n", Dhlt.normalA, Dhlt.frogA, right );*/
			Dhlt.frogNo = tan(D2R(Dhlt.frogA));
			if (Dhlt.frogNo > 0.01)
				Dhlt.frogNo = 1.0/Dhlt.frogNo;
			else
				Dhlt.frogNo = 0.0;
			if (action == C_MOVE) {
				if (Dhlt.frogNo != 0) {
					InfoMessage( _("Angle = %0.2f Frog# = %0.2f"), Dhlt.frogA, Dhlt.frogNo );
				} else {
					InfoMessage( _("Frog angle is too close to 0") );
				}
			} else {
				InfoMessage( _("Select point position") );
				Dhlt.state = 2;
				Translate( &Dhlt.reverseP, Dhlt.reverseP, Dhlt.normalA+(right?+90:-90), trackGauge );
				Translate( &Dhlt.reverseP1, Dhlt.reverseP1, Dhlt.normalA+(right?+90:-90), trackGauge );
			}
			DrawLine( &tempD, Dhlt.reverseP, Dhlt.reverseP1, 0, wDrawColorBlack );
			return C_CONTINUE;
		} else if ( Dhlt.state == 2 ) {
			DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
			tempSegs_da.cnt = 0;
			pointP = pos;
			if ((pointT = OnTrack( &pointP, TRUE, TRUE )) == NULL)
				break;
			if ( QueryTrack( pointT, Q_NOT_PLACE_FROGPOINTS ) ) {
				ErrorMessage( MSG_CANT_PLACE_FROGPOINTS, _("points") );
				break;
			}
			dist = FindDistance( Dhlt.normalP, pointP );
			pointA = GetAngleAtPoint( pointT, pointP, &pointEp0, &pointEp1 );
			angle = NormalizeAngle( pointA + 180.0 - Dhlt.reverseA );
PTRACE(( "rA=%0.1f pA=%0.1f a=%0.1f ", Dhlt.reverseA, pointA, angle ))
			if ( angle > 90.0 &&  angle < 270.0 ) {
				pointA = NormalizeAngle( pointA + 180.0 );
				angle = NormalizeAngle( angle + 180.0 );
PTRACE(( " {pA=%0.1f a=%0.1f} ", pointA, angle ))
			} else {
				ep = pointEp0; pointEp0 = pointEp1; pointEp1 = ep;
			}
			if (angle > 180.0) {
				angle = 360.0 - angle;
				right = TRUE;
			} else {
				right = FALSE;
			}
PTRACE(( "r=%c a=%0.1f ", right?'T':'F', angle ))
			Translate( &off, pointP, pointA+180.0, trackGauge*2.0 );
			if ((trk = OnTrack( &off, TRUE, TRUE )) == NULL)
				break;
			if ( QueryTrack( trk, Q_NOT_PLACE_FROGPOINTS ) ) {
				ErrorMessage( MSG_CANT_PLACE_FROGPOINTS, _("points") );
				break;
			}
			off = pointP;
			Rotate( &off, Dhlt.reverseP, 180-Dhlt.reverseA );
			off.x -= Dhlt.reverseP.x;
			off.y -= Dhlt.reverseP.y;
			if (right)
				off.x = -off.x;
PTRACE(( "off=[%0.3f %0.3f] ", off.x, off.y ))
			if (off.y < 0) {
				ErrorMessage( MSG_MOVE_POINTS_OTHER_SIDE );
PTRACE(("\n"))
				break;
			}
			if (off.x < 0) {
				ErrorMessage( MSG_MOVE_POINTS_AWAY_CLOSE );
PTRACE(("\n"))
				break;
			}
			angle2 = FindAngle( zero, off );
PTRACE(( "a2=%0.1f\n", angle2 ))
			if (angle < 0.5) {
				if ( off.x < connectDistance ) {
					tempSegs(0).type = SEG_STRTRK;
					tempSegs(0).color = wDrawColorBlack;
					tempSegs(0).u.l.pos[0] = pointP;
					tempSegs(0).u.l.pos[1] = Dhlt.reverseP;
					tempSegs(1).type = SEG_STRTRK;
					tempSegs(1).color = wDrawColorBlack;
					tempSegs(1).u.l.pos[0] = Dhlt.reverseP;
					Translate( &tempSegs(1).u.l.pos[1], Dhlt.reverseP, Dhlt.reverseA, trackGauge );
					tempSegs_da.cnt = 2;
				} else {
					 ErrorMessage( MSG_MOVE_POINTS_AWAY_NO_INTERSECTION );
					 break;
				}
			} else if (angle < angle2) {
				ErrorMessage( MSG_MOVE_POINTS_AWAY_NO_INTERSECTION );
				break;
			} else {
				if (!FindIntersection( &intersectP, Dhlt.reverseP, Dhlt.reverseA+180.0, pointP, pointA+180.0 ))
					break;
				reverseD = FindDistance( Dhlt.reverseP, intersectP );
				pointD = FindDistance( pointP, intersectP );
				if (reverseD > pointD) {
					reverseR = pointD/tan(D2R(angle/2.0));
					Translate( &reverseC, pointP, pointA+(right?-90:+90), reverseR );
PTRACE(( "rR=%0.3f rC=[%0.3f %0.3f]\n", reverseR, reverseC.x, reverseC.y ))
					tempSegs(0).type = SEG_CRVTRK;
					tempSegs(0).color = wDrawColorBlack;
					tempSegs(0).u.c.center = reverseC;
					tempSegs(0).u.c.radius = reverseR;
					tempSegs(0).u.c.a0 = NormalizeAngle(pointA + (right?(+90.0):(-90.0-angle)) );
					tempSegs(0).u.c.a1 = angle;
					tempSegs(1).type = SEG_STRTRK;
					tempSegs(1).color = wDrawColorBlack;
					PointOnCircle( &tempSegs(1).u.l.pos[0], reverseC, reverseR, tempSegs(0).u.c.a0 + (right?angle:0.0) );
					tempSegs(1).u.l.pos[1] = Dhlt.reverseP;
					tempSegs(2).type = SEG_STRTRK;
					tempSegs(2).color = wDrawColorBlack;
					tempSegs(2).u.l.pos[0] = Dhlt.reverseP;
					Translate( &tempSegs(2).u.l.pos[1], Dhlt.reverseP, Dhlt.reverseA, trackGauge );
					tempSegs_da.cnt = 3;
				} else {
					reverseR = reverseD/tan(D2R(angle/2.0));
					reverseR *= sqrt(reverseD/pointD);
					Translate( &reverseC, Dhlt.reverseP, Dhlt.reverseA+(right?+90:-90), reverseR );
					Translate( &pointP1, pointP, pointA+(right?-90:+90), reverseR );
					dist = FindDistance( reverseC, pointP );
					angle2 = R2D( asin( reverseR/dist ) );
					angle3 = FindAngle( pointP, reverseC );
					if (right)
						angle2 = NormalizeAngle(angle3 - pointA+180) - angle2;
					else
						angle2 = NormalizeAngle(pointA+180 - angle3) - angle2;
					reverseA1 = angle-angle2;
PTRACE(( " a2=%0.1f rA1=%0.1f\n", angle2, reverseA1 ))
					tempSegs(0).type = SEG_STRTRK;
					tempSegs(0).color = wDrawColorBlack;
					tempSegs(0).u.l.pos[0] = pointP;
					tempSegs(1).u.c.a0 = NormalizeAngle(Dhlt.reverseA + (right?(-90.0-reverseA1):+90.0));
					PointOnCircle( &tempSegs(0).u.l.pos[1], reverseC, reverseR, tempSegs(1).u.c.a0 + (right?0.0:reverseA1) );
					tempSegs(1).type = SEG_CRVTRK;
					tempSegs(1).color = wDrawColorBlack;
					tempSegs(1).u.c.center = reverseC;
					tempSegs(1).u.c.radius = reverseR;
					tempSegs(1).u.c.a1 = reverseA1;
					tempSegs(2).type = SEG_STRTRK;
					tempSegs(2).color = wDrawColorBlack;
					tempSegs(2).u.l.pos[0] = Dhlt.reverseP;
					Translate( &tempSegs(2).u.l.pos[1], Dhlt.reverseP, Dhlt.reverseA, trackGauge );
					tempSegs_da.cnt = 3;
				}
			}
			if (action != C_UP) {
				dist = FindDistance( pointP, Dhlt.normalP );
				InfoMessage( _("Length = %0.2f Angle = %0.2f Frog# = %0.2f"), dist, Dhlt.frogA, Dhlt.frogNo );
				DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
				return C_CONTINUE;
			}
			UndoStart( _("Create Hand Laid Turnout"), "Hndldto( T%d[%d] )", GetTrkIndex(pointT), pointEp0 );
			UndoModify( pointT );
			if (!SplitTrack( pointT, pointP, pointEp0, &trk1, TRUE ))
				break;
			dist = trackGauge*2.0;
			if ( !trk1 ) {
				trk1 = pointT;
				pointT = NULL;
			}
			ep1 = PickEndPoint( pointP, trk1 );
			if (!RemoveTrack( &trk1, &ep1, &dist ))
				break;
			point0 = GetTrkEndPos( trk1, ep1 );
			angle0 = NormalizeAngle(GetTrkEndAngle(trk1,ep1)+180.0);
			trk2 = NULL;
			trkpp = trks;
			for (segP=&tempSegs(0); segP < &tempSegs(tempSegs_da.cnt); segP++ ) {
				switch (segP->type) {
				case SEG_STRTRK:
					trk2b = NewStraightTrack( segP->u.l.pos[0], segP->u.l.pos[1] );
					ep2b = 0;
					break;
				case SEG_CRVTRK:
					trk2b = NewCurvedTrack( segP->u.c.center, fabs(segP->u.c.radius), segP->u.c.a0, segP->u.c.a1, 0 );
					ep2b = (right?0:1);
				}
				if (trk2 == NULL) {
					trk2 = trk2b;
					ep2 = ep2b;
				} else {
					ConnectTracks( trk2a, ep2a, trk2b, ep2b );
				}
				*trkpp++ = trk2a = trk2b;
				ep2a = 1-ep2b;
			}
			*trkpp = NULL;
			dist = trackGauge*2.0;
			if (!RemoveTrack( &trk2, &ep2, &dist ))
				break;
			trk = NewHandLaidTurnout( pointP, pointA,
				point0, angle0,
				GetTrkEndPos(trk2,ep2), NormalizeAngle(GetTrkEndAngle(trk2,ep2)+180.0), Dhlt.frogA );
			DrawEndPt( &mainD, trk1, ep1, wDrawColorWhite );
			if ( pointT ) {
				DrawEndPt( &mainD, pointT, pointEp0, wDrawColorWhite );
				ConnectTracks( trk, 0, pointT, pointEp0 );
			}
			ConnectTracks( trk, 2, trk2, ep2 );
			ConnectTracks( trk, 1, trk1, ep1 );
			DrawEndPt( &mainD, trk1, ep1, wDrawColorBlack );
			DrawTrack( trk1, &mainD, wDrawColorBlack );
			if ( pointT ) {
				DrawEndPt( &mainD, pointT, pointEp0, wDrawColorBlack );
				DrawTrack( pointT, &mainD, wDrawColorBlack );
			}
			DrawTrack( trk, &mainD, wDrawColorBlack );
			for (trkpp=trks; *trkpp; trkpp++)
				DrawTrack( *trkpp, &mainD, wDrawColorBlack );
			DrawLine( &tempD, Dhlt.reverseP, Dhlt.reverseP1, 0, wDrawColorBlack );
		
			Dhlt.state = 0;
			return C_TERMINATE;
		}

	case C_REDRAW:
		if (Dhlt.state >= 1)
			DrawLine( &tempD, Dhlt.reverseP, Dhlt.reverseP1, 0, wDrawColorBlack );
		if (Dhlt.state >= 2)
			DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	case C_CANCEL:
		if (Dhlt.state >= 1)
			DrawLine( &tempD, Dhlt.reverseP, Dhlt.reverseP1, 0, wDrawColorBlack );
		if (Dhlt.state >= 2) {
			DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
			tempSegs_da.cnt = 0;
		}
		return C_CONTINUE;

	}

	return C_CONTINUE;

}


#include "bitmaps/hndldto.xpm"

EXPORT void InitCmdHandLaidTurnout( wMenu_p menu )
{
	AddMenuButton( menu, CmdHandLaidTurnout, "cmdHandLaidTurnout", _("HandLaidTurnout"), wIconCreatePixMap(hndldto_xpm), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_HNDLDTO, NULL );
}
