/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/cjoin.c,v 1.4 2008-03-06 19:35:05 m_fischer Exp $
 *
 * JOINS
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
#include "i18n.h"


static int log_join = 0;
typedef struct {
				curveType_e type;
				BOOL_T flip;
				coOrd arcP;
				DIST_T arcR;
				ANGLE_T arcA0, arcA1;
				coOrd pos[2];
		} joinRes_t;

static struct {
		STATE_T state;
		int joinMoveState;
		struct {
				TRKTYP_T realType;
				track_p trk;
				coOrd pos;
				EPINX_T ep;
				trackParams_t params;
#ifdef LATER
				curveType_e type;
				ANGLE_T angle;
				coOrd lineOrig;
				coOrd lineEnd;
				coOrd arcP;
				DIST_T arcR;
				ANGLE_T arcA0, arcA1;
#endif
				} inp[2];
		joinRes_t jRes;
		coOrd inp_pos[2];
		easementData_t jointD[2];
		} Dj;


/*****************************************************************************
 *
 * JOIN
 *
 */


static BOOL_T JoinWithStraight(
		coOrd pos0,
		ANGLE_T a0,
		coOrd pos1,
		ANGLE_T a1,
		joinRes_t * res )
/*
 * Determine a track from a point and angle (pos1,a1) to
 * a straight (given by an origin and angle: pos0, a0)
 */
{
	coOrd Px;
	ANGLE_T b, c;
	DIST_T d;
	DIST_T k;
	coOrd off;
	DOUBLE_T beyond;

	b = NormalizeAngle( a0 - a1 );
LOG( log_join, 2, ( 
			"JwL: pos0=[%0.3f %0.3f] a0=%0.3f pos1=[%0.3f %0.3f] a1=%0.3f b=%0.3f\n",
			pos0.x, pos0.y, a0, pos1.x, pos1.y, a1, b ) )

/* 3 - cases: */
	if (b >= 360.0-connectAngle/2.0 || b <= connectAngle/2.0) {
/* CASE 1: antiparallel */
		FindPos( &off, NULL, pos1, pos0, a0, 10000.0 );
		res->arcR = off.y/2.0;
		res->arcA1 = 180.0;
LOG( log_join, 3, ("JwL: parallel: off.y=%0.3f\n", off.y ) )
		res->arcA0 = NormalizeAngle( a1 - 90.0 );
		Translate( &res->arcP, pos1, res->arcA0, res->arcR );
		if (res->arcR > 0.0) {
			res->flip = 0;
		} else {
			res->arcR = -res->arcR;
			res->flip = 1;
		}
	} else if (b >= 180.0-connectAngle/2.0 && b <= 180.0+connectAngle/2.0) {
/* CASE 2: parallel, possibly colinear? */
		FindPos( &off, &beyond, pos0, pos1, a0, 100000.0 );
LOG( log_join, 3, ("JwL: colinear? off.y=%0.3f\n", off.y ) )
		if (off.y > -connectDistance && off.y < connectDistance) {
			res->type = curveTypeStraight;
			res->pos[0]=pos0;
			res->pos[1]=pos1;
LOG( log_join, 2, ("    = STRAIGHT [%0.3f %0.3f] [%0.3f %0.3f]\n", pos0.x, pos0.y, pos1.x, pos1.y ) )
			return TRUE;
		} else {
			res->type = curveTypeNone;
			ErrorMessage( MSG_SELECTED_TRACKS_PARALLEL );
			return TRUE;
		}
	} else {
/* CASE 3: intersecting */
		if (!FindIntersection( &Px, pos0, a0, pos1, a1 )) {
			res->type = curveTypeNone;
			ErrorMessage( MSG_SELECTED_TRACKS_PARALLEL );
			return TRUE;
		}
		d = FindDistance( pos1, Px );
		k = NormalizeAngle( FindAngle(pos1, Px) - a1 );
		c = (b > 180.0) ? (360.0-b) : b;
		if (k < 90.0 && k > 270.0) 
			c += 180.0;
LOG( log_join, 3, ("     Px=[%0.3f %0.3f] b=%0.3f c=%0.3f d=%0.3f k=%0.3f\n", Px.x, Px.y, b, c, d, k ) )
		res->arcR = d * sin(D2R(c/2.0))/cos(D2R(c/2.0));
		res->arcA1 = 180.0-c;
		if (90.0<k && k<270.0)
		res->arcA1 = 360.0 - res->arcA1;
		if ( (res->arcA1>180.0) == (b>180.0) ) {
			Translate( &res->arcP, pos1, a1-90.0, res->arcR );
			res->arcA0 = NormalizeAngle( a0 - 90.0 );
			res->flip = FALSE;
		} else {
			Translate( &res->arcP, pos1, a1+90.0, res->arcR );
			res->arcA0 = NormalizeAngle( a1 - 90.0 );
			res->flip = TRUE;
		}
	}
LOG( log_join, 2, ("    = CURVE @ Pc=[%0.3f %0.3f] R=%0.3f A0=%0.3f A1=%0.3f Flip=%d\n",
			res->arcP.x, res->arcP.y, res->arcR, res->arcA0, res->arcA1, res->flip ) )
	if (res->arcR<0.0) res->arcR = - res->arcR;
	res->type = curveTypeCurve;
	d = D2R(res->arcA1);
	if (d < 0.0)
		d = 2*M_PI + d;
	InfoMessage( _("Curved Track: Radius=%s Length=%s"),
				FormatDistance(res->arcR), FormatDistance(res->arcR*d) );
	return TRUE;

}

static BOOL_T JoinWithCurve(
		coOrd pos0,
		DIST_T r0,
		EPINX_T ep0,
		coOrd pos1,
		ANGLE_T a1,				/* Angle perpendicular to track at (pos1) */
		joinRes_t * res )
/*
 * Determine a track point and angle (pos1,a1) to
 * a curve (given by center and radius (pos0, r0).
 * Curve endPt (ep0) determines whether the connection is
 * clockwise or counterclockwise.
 */ 
{
	coOrd p1, pt;
	DIST_T d, r;
	ANGLE_T a, aa, A0, A1;

/* Compute angle of line connecting endPoints: */
	Translate( &p1, pos1, a1, -r0 );
	aa = FindAngle( p1, pos0 );
	a = NormalizeAngle( aa - a1 );
LOG( log_join, 2, ("JwA: pos0=[%0.3f %0.3f] r0=%0.3f ep0=%d pos1=[%0.3f %0.3f] a1=%0.3f\n",
				pos0.x, pos0.y, r0, ep0, pos1.x, pos1.y, a1 ) )
LOG( log_join, 3, ("     p1=[%0.3f %0.3f] aa=%0.3f a=%0.3f\n",
				p1.x, p1.y, aa, a ) )

	if ( (ep0==1 && a > 89.5 && a < 90.5) ||
		 (ep0==0 && a > 269.5 && a < 270.5) ) {
/* The long way around! */
		ErrorMessage( MSG_CURVE_TOO_LARGE );
		res->type = curveTypeNone;

	} else if ( (ep0==0 && a > 89.5 && a < 90.5) ||
				(ep0==1 && a > 269.5 && a < 270.5) ) {
/* Straight: */
		PointOnCircle( &pt, pos0, r0, a1);
LOG( log_join, 2, ("    = STRAIGHT [%0.3f %0.3f] [%0.3f %0.3f]\n", pt.x, pt.y, pos1.x, pos1.y ) )
		InfoMessage( _("Straight Track: Length=%s Angle=%0.3f"),
				FormatDistance(FindDistance( pt, pos1 )), PutAngle(FindAngle( pt, pos1 )) );
		res->type = curveTypeStraight;
		res->pos[0]=pt;
		res->pos[1]=pos1;
		res->flip = FALSE;

	} else {
/* Curve: */
		d = FindDistance( p1, pos0 ) / 2.0;
		r = d/cos(D2R(a));
		Translate( &res->arcP, p1, a1, r );
		res->arcR = r-r0;
LOG( log_join, 3, ("     Curved d=%0.3f C=[%0.3f %0.3f], r=%0.3f a=%0.3f arcR=%0.3f\n",
			d, res->arcP.x, res->arcP.y, r, a, res->arcR ) )
		if ( (ep0==0) == (res->arcR<0) ) {
			A1 = 180 + 2*a;
			A0 = a1;
			res->flip = TRUE;
		} else {
			A1 = 180 - 2*a;
			A0 = a1 - A1;
			res->flip = FALSE;
		}
		if (res->arcR>=0) {
			A0 += 180.0;
		} else {
			res->arcR = - res->arcR;
		}
		res->arcA0 = NormalizeAngle( A0 );
		res->arcA1 = NormalizeAngle( A1 );

		if ( res->arcR*2.0*M_PI*res->arcA1/360.0 > mapD.size.x+mapD.size.y ) {
			ErrorMessage( MSG_CURVE_TOO_LARGE );
			res->type = curveTypeNone;
			return TRUE;
		}

LOG( log_join, 3, ("       A0=%0.3f A1=%0.3f R=%0.3f\n", res->arcA0, res->arcA1, res->arcR ) )
		d = D2R(res->arcA1);
		if (d < 0.0)
			d = 2*M_PI + d;
		InfoMessage( _("Curved Track: Radius=%s Length=%s Angle=%0.3f"),
				FormatDistance(res->arcR), FormatDistance(res->arcR*d), PutAngle(res->arcA1) );
		res->type = curveTypeCurve;
	}
	return TRUE;
}

/*****************************************************************************
 *
 * JOIN
 *
 */


static STATUS_T AdjustJoint(
		BOOL_T adjust,
		ANGLE_T a1,
		DIST_T eR[2],
		ANGLE_T normalAngle )
/*
 * Compute how to join 2 tracks and then compute the transition-curve
 * from the 2 tracks to the joint.
 * The 2nd contact point (Dj.inp[1].pos) can be moved by (Dj.jointD[1].x)
 * before computing the connection curve.  This allows for the
 * transition-curve.
 *
 * This function is called iteratively to fine-tune the offset (X) required
 * for the transition-curves.
 * The first call does not move the second contact point.  Subsequent calls
 * move the contact point by the previously computed offset.
 * Hopefully, this converges on a stable value for the offset quickly.
 */
{
	coOrd p0, p1;
	ANGLE_T a0=0;
	coOrd pc;
	DIST_T eRc;
	DIST_T l, d=0;
	
	if (adjust)
		Translate( &p1, Dj.inp[1].pos, a1, Dj.jointD[1].x );
	else
		p1 = Dj.inp[1].pos;

	switch ( Dj.inp[0].params.type ) {
	case curveTypeCurve:
		if (adjust) {
			a0 = FindAngle( Dj.inp[0].params.arcP, Dj.jRes.pos[0] ) +
					((Dj.jointD[0].Scurve==TRUE || Dj.jointD[0].flip==FALSE)?0:+180);
			Translate( &pc, Dj.inp[0].params.arcP, a0, Dj.jointD[0].x );
LOG( log_join, 2, (" Move P0 X%0.3f A%0.3f  P1 X%0.3f A%0.3f)\n",
					Dj.jointD[0].x, a0, Dj.jointD[1].x, a1 ) )
		} else {
			pc = Dj.inp[0].params.arcP;
		}
		if (!JoinWithCurve( pc, Dj.inp[0].params.arcR,
						Dj.inp[0].params.ep, p1, normalAngle, &Dj.jRes ))
			return FALSE;
		break;
	case curveTypeStraight:
		if (adjust) {
			a0 = Dj.inp[0].params.angle + (Dj.jointD[0].negate?-90.0:+90.0);
			Translate( &p0, Dj.inp[0].params.lineOrig, a0, Dj.jointD[0].x );
LOG( log_join, 2, (" Move P0 X%0.3f A%0.3f  P1 X%0.3f A%0.3f\n",
					Dj.jointD[0].x, a0, Dj.jointD[1].x, a1 ) )
		} else {
			p0 = Dj.inp[0].params.lineOrig;
		}
		if (!JoinWithStraight( p0, Dj.inp[0].params.angle, p1, Dj.inp[1].params.angle, &Dj.jRes ))
			return FALSE;
		break;
	default:
		break;
	}

	if (Dj.jRes.type == curveTypeNone) {
		return FALSE;
	}

	if (Dj.jRes.type == curveTypeCurve) {
		eRc = Dj.jRes.arcR;
		if (Dj.jRes.flip==1)
			eRc = -eRc;
	} else
		eRc = 0.0;

	if ( ComputeJoint( eR[0], eRc, &Dj.jointD[0] ) == E_ERROR ||
		 ComputeJoint( -eR[1], -eRc, &Dj.jointD[1] ) == E_ERROR ) {
		return FALSE;
	}

#ifdef LATER
	for (inx=0; inx<2; inx++) {
		if (Dj.inp[inx].params.type == curveTypeStraight ) {
			d = FindDistance( Dj.inp[inx].params.lineOrig, Dj.inp_pos[inx] );
			if (d < Dj.jointD[inx].d0) {
				InfoMessage( _("Track (%d) is too short for transition-curve by %0.3f"), 
						GetTrkIndex(Dj.inp[inx].trk),
						PutDim(fabs(Dj.jointD[inx].d0-d)) );
				return FALSE;
			}
		}
	}
#endif

	l = Dj.jointD[0].d0 + Dj.jointD[1].d0;
	if (Dj.jRes.type == curveTypeCurve ) {
		d = Dj.jRes.arcR * Dj.jRes.arcA1 * 2.0*M_PI/360.0;
	} else if (Dj.jRes.type == curveTypeStraight ) {
		d = FindDistance( Dj.jRes.pos[0], Dj.jRes.pos[1] );
	}
	d -= l;
	if ( d <= minLength ) {
		InfoMessage( _("Connecting track is too short by %0.3f"), PutDim(fabs(minLength-d)) );
		return FALSE;
	}

	if (Dj.jRes.type == curveTypeCurve) {
		PointOnCircle( &Dj.jRes.pos[Dj.jRes.flip], Dj.jRes.arcP,
				Dj.jRes.arcR, Dj.jRes.arcA0 );
		PointOnCircle( &Dj.jRes.pos[1-Dj.jRes.flip], Dj.jRes.arcP,
				Dj.jRes.arcR, Dj.jRes.arcA0+Dj.jRes.arcA1 );
	}

	if (adjust)
		Translate( &Dj.inp_pos[0], Dj.jRes.pos[0], a0+180.0, Dj.jointD[0].x );

	return TRUE;
}


static STATUS_T DoMoveToJoin( coOrd pos )
{
		if ( selectedTrackCount <= 0 ) {
			ErrorMessage( MSG_NO_SELECTED_TRK );
			return C_CONTINUE;
		}
		if ( (Dj.inp[Dj.joinMoveState].trk = OnTrack( &pos, TRUE, TRUE )) == NULL )
			return C_CONTINUE;
		if (!CheckTrackLayer( Dj.inp[Dj.joinMoveState].trk ) )
			return C_CONTINUE;
		Dj.inp[Dj.joinMoveState].params.ep = PickUnconnectedEndPoint( pos, Dj.inp[Dj.joinMoveState].trk ); /* CHECKME */
		if ( Dj.inp[Dj.joinMoveState].params.ep == -1 ) {
#ifdef LATER
			ErrorMessage( MSG_NO_ENDPTS );
#endif
			return C_CONTINUE;
		}
#ifdef LATER
		if ( GetTrkEndTrk( Dj.inp[Dj.joinMoveState].trk, Dj.inp[Dj.joinMoveState].params.ep ) ) {
			ErrorMessage( MSG_SEL_EP_CONN );
			return C_CONTINUE;
		}
#endif
		if (Dj.joinMoveState == 0) {
			Dj.joinMoveState++;
			InfoMessage( GetTrkSelected(Dj.inp[0].trk)?
				_("Click on an unselected End-Point"):
				_("Click on a selected End-Point") );
			Dj.inp[0].pos = pos;
			DrawFillCircle( &tempD, Dj.inp[0].pos, 0.10*mainD.scale, selectedColor );
			return C_CONTINUE;
		}
		if ( GetTrkSelected(Dj.inp[0].trk) == GetTrkSelected(Dj.inp[1].trk) ) {
			ErrorMessage( MSG_2ND_TRK_NOT_SEL_UNSEL, GetTrkSelected(Dj.inp[0].trk)
					?  _("unselected") : _("selected") );
			return C_CONTINUE;
		}
		DrawFillCircle( &tempD, Dj.inp[0].pos, 0.10*mainD.scale, selectedColor );
		if (GetTrkSelected(Dj.inp[0].trk))
			MoveToJoin( Dj.inp[0].trk, Dj.inp[0].params.ep, Dj.inp[1].trk, Dj.inp[1].params.ep );
		else
			MoveToJoin( Dj.inp[1].trk, Dj.inp[1].params.ep, Dj.inp[0].trk, Dj.inp[0].params.ep );
		Dj.joinMoveState = 0;
		return C_TERMINATE;
}


static STATUS_T CmdJoin(
		wAction_t action,
		coOrd pos )
/*
 * Join 2 tracks.
 */
{
	DIST_T d=0, l;
	coOrd off, p1;
	EPINX_T ep;
	track_p trk=NULL;
	DOUBLE_T beyond;
	STATUS_T rc;
	ANGLE_T normalAngle=0;
	EPINX_T inx;
	ANGLE_T a, a1;
	DIST_T eR[2];
	BOOL_T ok;

	switch (action) {

	case C_START:
		InfoMessage( _("Left click - join with track, Shift Left click - move to join") );
		Dj.state = 0;
		Dj.joinMoveState = 0;
		/*ParamGroupRecord( &easementPG );*/
		return C_CONTINUE;

	case C_DOWN:
		if ( (Dj.state == 0 && (MyGetKeyState() & WKEY_SHIFT) != 0) || Dj.joinMoveState != 0 )
			return DoMoveToJoin( pos );

		DYNARR_SET( trkSeg_t, tempSegs_da, 3 );
		tempSegs(0).color = drawColorBlack;
		tempSegs(0).width = 0;
		tempSegs(1).color = drawColorBlack;
		tempSegs(1).width = 0;
		tempSegs(2).color = drawColorBlack;
		tempSegs(2).width = 0;
		tempSegs_da.cnt = 0;
		Dj.joinMoveState = 0;
/* Populate (Dj.inp[0]) and check for connecting abutting tracks */
		if (Dj.state == 0) {
			if ( (Dj.inp[0].trk = OnTrack( &pos, TRUE, TRUE )) == NULL)
				return C_CONTINUE;
			if (!CheckTrackLayer( Dj.inp[0].trk ) )
				return C_CONTINUE;
			Dj.inp[0].pos = pos;
LOG( log_join, 1, ("JOIN: 1st track %d @[%0.3f %0.3f]\n",
						GetTrkIndex(Dj.inp[0].trk), Dj.inp[0].pos.x, Dj.inp[1].pos.y ) )
			if (!GetTrackParams( PARAMS_1ST_JOIN, Dj.inp[0].trk, pos, &Dj.inp[0].params ))
				return C_CONTINUE;
			Dj.inp[0].realType = GetTrkType(Dj.inp[0].trk);
			InfoMessage( _("Select 2nd track") );
			Dj.state = 1;
			DrawFillCircle( &tempD, Dj.inp[0].pos, 0.10*mainD.scale, selectedColor );
			return C_CONTINUE;
		} else {
			if ( (Dj.inp[1].trk = OnTrack( &pos, TRUE, TRUE )) == NULL)
				return C_CONTINUE;
			if (!CheckTrackLayer( Dj.inp[1].trk ) )
				return C_CONTINUE;
			Dj.inp[1].pos = pos;
			if (!GetTrackParams( PARAMS_2ND_JOIN, Dj.inp[1].trk, pos, &Dj.inp[1].params ))
				return C_CONTINUE;
			if ( Dj.inp[0].trk == Dj.inp[1].trk ) {
				ErrorMessage( MSG_JOIN_SAME );
				return C_CONTINUE;
			}
			Dj.inp[1].realType = GetTrkType(Dj.inp[1].trk);
			if ( IsCurveCircle( Dj.inp[0].trk ) )
				Dj.inp[0].params.ep = PickArcEndPt( Dj.inp[0].params.arcP, Dj.inp[0].pos, pos );
			if ( IsCurveCircle( Dj.inp[1].trk ) )
				Dj.inp[1].params.ep = PickArcEndPt( Dj.inp[1].params.arcP, pos, Dj.inp[0].pos );

LOG( log_join, 1, ("      2nd track %d, @[%0.3f %0.3f] EP0=%d EP1=%d\n",
						GetTrkIndex(Dj.inp[1].trk), Dj.inp[1].pos.x, Dj.inp[1].pos.y,
						Dj.inp[0].params.ep, Dj.inp[1].params.ep ) )
LOG( log_join, 1, ("P1=[%0.3f %0.3f]\n", pos.x, pos.y ) )
			if ( GetTrkEndTrk(Dj.inp[0].trk,Dj.inp[0].params.ep) != NULL) {
				ErrorMessage( MSG_TRK_ALREADY_CONN, _("First") );
				return C_CONTINUE;
			}
			if ( Dj.inp[1].params.ep >= 0 &&
				 GetTrkEndTrk(Dj.inp[1].trk,Dj.inp[1].params.ep) != NULL) {
				ErrorMessage( MSG_TRK_ALREADY_CONN, _("Second") );
				return C_CONTINUE;
			}

			rc = C_CONTINUE;
			if ( MergeTracks( Dj.inp[0].trk, Dj.inp[0].params.ep,
							  Dj.inp[1].trk, Dj.inp[1].params.ep ) )
				rc = C_TERMINATE;
			else if ( Dj.inp[0].params.ep >= 0 && Dj.inp[1].params.ep >= 0 ) {
				if ( Dj.inp[0].params.type == curveTypeStraight &&
					 Dj.inp[1].params.type == curveTypeStraight &&
					 ExtendStraightToJoin( Dj.inp[0].trk, Dj.inp[0].params.ep,
										   Dj.inp[1].trk, Dj.inp[1].params.ep ) )
					rc = C_TERMINATE;
				if ( ConnectAbuttingTracks( Dj.inp[0].trk, Dj.inp[0].params.ep,
											Dj.inp[1].trk, Dj.inp[1].params.ep ) )
					rc = C_TERMINATE;
			}
			if ( rc == C_TERMINATE ) {
				DrawFillCircle( &tempD, Dj.inp[0].pos, 0.10*mainD.scale, selectedColor );
				return rc;
			}
			if ( QueryTrack( Dj.inp[0].trk, Q_CANNOT_BE_ON_END ) ||
				 QueryTrack( Dj.inp[1].trk, Q_CANNOT_BE_ON_END ) ) {
				ErrorMessage( MSG_JOIN_EASEMENTS );
				return C_CONTINUE;
			}

			DrawFillCircle( &tempD, Dj.inp[0].pos, 0.10*mainD.scale, selectedColor );
			Dj.state = 2;
			Dj.jRes.flip = FALSE;
		}
		tempSegs_da.cnt = 0;

	case C_MOVE:

LOG( log_join, 3, ("P1=[%0.3f %0.3f]\n", pos.x, pos.y ) )
		if (Dj.state != 2)
			return C_CONTINUE;

		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, drawColorBlack );
		tempSegs_da.cnt = 0;
		tempSegs(0).color = drawColorBlack;
		ok = FALSE;

/* Populate (Dj.inp[1]) */ 
		if ( QueryTrack(Dj.inp[1].trk,Q_REFRESH_JOIN_PARAMS_ON_MOVE) ) {
			if ( !GetTrackParams( PARAMS_2ND_JOIN, Dj.inp[1].trk, pos, &Dj.inp[1].params ) )
				return C_CONTINUE;
		}
		beyond = 1.0;
		switch ( Dj.inp[1].params.type ) {
		case curveTypeCurve:
			normalAngle = FindAngle( Dj.inp[1].params.arcP, pos );
			Dj.inp[1].params.angle = NormalizeAngle( normalAngle +
											  ((Dj.inp[1].params.ep==0)?-90.0:90.0));
			PointOnCircle( &Dj.inp[1].pos, Dj.inp[1].params.arcP,
							Dj.inp[1].params.arcR, normalAngle );
			if (Dj.inp[0].params.ep == Dj.inp[1].params.ep)
				normalAngle = NormalizeAngle( normalAngle + 180.0 );
			break;
		case curveTypeStraight:
			FindPos( &off, &beyond, pos, Dj.inp[1].params.lineOrig, Dj.inp[1].params.angle,
					 100000 );
			Translate( &Dj.inp[1].pos, Dj.inp[1].params.lineOrig, Dj.inp[1].params.angle,
					   off.x );
			normalAngle = NormalizeAngle( Dj.inp[1].params.angle +
										  ((Dj.inp[0].params.ep==0)?-90.0:90.0) );
			break;
		case curveTypeNone:
		case curveTypeBezier:
		case curveTypeCornu:
			break;
		}

/* Compute the radius of the 2 tracks, for ComputeE() */
		for (inx=0;inx<2;inx++)
			if (Dj.inp[inx].params.type == curveTypeCurve) {
				eR[inx] = Dj.inp[inx].params.arcR;
				if (Dj.inp[inx].params.ep == inx)
					eR[inx] = - eR[inx];
			} else
				eR[inx] = 0.0;

		if (!AdjustJoint( FALSE, 0.0, eR, normalAngle ))
			goto errorReturn;
			/*return C_CONTINUE;*/

		if (beyond < -0.000001) {
#ifdef VERBOSE
printf("pos=[%0.3f,%0.3f] lineOrig=[%0.3f,%0.3f], angle=%0.3f = off=[%0.3f,%0.3f], beyond=%0.3f\n",
pos.x, pos.y, Dj.inp[1].params.lineOrig.x, Dj.inp[1].params.lineOrig.y, Dj.inp[1].params.angle, off.x, off.y, beyond );
#endif
			InfoMessage( _("Beyond end of 2nd track") );
			goto errorReturn;
		}
		Dj.inp_pos[0] = Dj.jRes.pos[0];
		Dj.inp_pos[1] = Dj.jRes.pos[1];

LOG( log_join, 3, (" -E   POS0=[%0.3f %0.3f] POS1=[%0.3f %0.3f]\n",
					Dj.jRes.pos[0].x, Dj.jRes.pos[0].y,
					Dj.jRes.pos[1].x, Dj.jRes.pos[1].y ) )

		if ( Dj.jointD[0].x!=0.0 || Dj.jointD[1].x!=0.0 ) {

/* Compute the transition-curve, hopefully twice is enough */
			a1 = Dj.inp[1].params.angle + (Dj.jointD[1].negate?-90.0:+90.0);
			if ((!AdjustJoint( TRUE, a1, eR, normalAngle )) ||
				(!AdjustJoint( TRUE, a1, eR, normalAngle )) )
				goto errorReturn;
				/*return C_CONTINUE;*/

			if (logTable(log_join).level >= 3) {
				Translate( &p1, Dj.jRes.pos[1], a1+180.0, Dj.jointD[1].x );
				LogPrintf(" X0=%0.3f, P1=[%0.3f %0.3f]\n",
						FindDistance( Dj.inp_pos[0], Dj.jRes.pos[0] ), p1.x, p1.y );
				LogPrintf(" E+   POS0=[%0.3f %0.3f]..[%0.3f %0.3f] POS1=[%0.3f %0.3f]..[%0.3f %0.3f]\n",
						Dj.inp_pos[0].x, Dj.inp_pos[0].y,
						Dj.jRes.pos[0].x, Dj.jRes.pos[0].y,
						p1.x, p1.y, Dj.jRes.pos[1].x, Dj.jRes.pos[1].y );
			}
		}

		switch ( Dj.inp[0].params.type ) {
		case curveTypeStraight:
			FindPos( &off, &beyond, Dj.inp_pos[0], Dj.inp[0].params.lineOrig,
					 Dj.inp[0].params.angle, 100000.0 );
			if (beyond < 0.0) {
				InfoMessage(_("Beyond end of 1st track"));
				goto errorReturn;
				/*Dj.jRes.type = curveTypeNone;
				return C_CONTINUE;*/
			}
			d = FindDistance( Dj.inp_pos[0], Dj.inp[0].params.lineOrig );
			break;
		case curveTypeCurve:
			if (IsCurveCircle(Dj.inp[0].trk)) {
				d = 10000.0;
			} else {
				a = FindAngle( Dj.inp[0].params.arcP, Dj.inp_pos[0] );
				if (Dj.inp[0].params.ep == 0)
					a1 = NormalizeAngle( Dj.inp[0].params.arcA0+Dj.inp[0].params.arcA1-a );
				else
					a1 = NormalizeAngle( a-Dj.inp[0].params.arcA0 );
				d = Dj.inp[0].params.arcR * a1 * 2.0*M_PI/360.0;
			}
			break;
		default:
			AbortProg( "cmdJoin - unknown type[0]" );
		}
		d -= Dj.jointD[0].d0;
		if ( d <= minLength ) {
			ErrorMessage( MSG_TRK_TOO_SHORT, _("First "), PutDim(fabs(minLength-d)) );
			goto errorReturn;
			/*Dj.jRes.type = curveTypeNone;
			return C_CONTINUE;*/
		}

		switch ( Dj.inp[1].params.type ) {
		case curveTypeStraight:
			d = FindDistance( Dj.inp_pos[1], Dj.inp[1].params.lineOrig );
			break;
		case curveTypeCurve:
			if (IsCurveCircle(Dj.inp[1].trk)) {
				d = 10000.0;
			} else {
				a = FindAngle( Dj.inp[1].params.arcP, Dj.inp_pos[1] );
				if (Dj.inp[1].params.ep == 0)
					a1 = NormalizeAngle( Dj.inp[1].params.arcA0+Dj.inp[1].params.arcA1-a );
				else
					a1 = NormalizeAngle( a-Dj.inp[1].params.arcA0 );
				d = Dj.inp[1].params.arcR * a1 * 2.0*M_PI/360.0;
			}
			break;
		default:
			AbortProg( "cmdJoin - unknown type[1]" );
		}
		d -= Dj.jointD[1].d0;
		if ( d <= minLength ) {
			ErrorMessage( MSG_TRK_TOO_SHORT, _("Second "), PutDim(fabs(minLength-d)) );
			goto errorReturn;
			/*Dj.jRes.type = curveTypeNone;
			return C_CONTINUE;*/
		}

		l = Dj.jointD[0].d0 + Dj.jointD[1].d0;
		if ( l > 0.0 ) {
			if ( Dj.jRes.type == curveTypeCurve ) {
				d = Dj.jRes.arcR * Dj.jRes.arcA1 * 2.0*M_PI/360.0;
			} else if ( Dj.jRes.type == curveTypeStraight ) {
				d = FindDistance( Dj.jRes.pos[0], Dj.jRes.pos[1] );
			}
			if ( d < l ) {
				ErrorMessage( MSG_TRK_TOO_SHORT, _("Connecting "), PutDim(fabs(minLength-d)) );
				goto errorReturn;
				/*Dj.jRes.type = curveTypeNone;
				return C_CONTINUE;*/
			}
		}

/* Setup temp track */
		for ( ep=0; ep<2; ep++ ) {
			switch( Dj.inp[ep].params.type ) {
			case curveTypeCurve:
				tempSegs(tempSegs_da.cnt).type = SEG_CRVTRK;
				tempSegs(tempSegs_da.cnt).u.c.center = Dj.inp[ep].params.arcP;
				tempSegs(tempSegs_da.cnt).u.c.radius = Dj.inp[ep].params.arcR;
				if (IsCurveCircle( Dj.inp[ep].trk ))
					break;
				a = FindAngle( Dj.inp[ep].params.arcP, Dj.inp_pos[ep] );
				a1 = NormalizeAngle( a-Dj.inp[ep].params.arcA0 );
				if (a1 <= Dj.inp[ep].params.arcA1)
					break;
				if (Dj.inp[ep].params.ep == 0) {
					tempSegs(tempSegs_da.cnt).u.c.a0 = a;
					tempSegs(tempSegs_da.cnt).u.c.a1 = NormalizeAngle(Dj.inp[ep].params.arcA0-a);
				} else {
					tempSegs(tempSegs_da.cnt).u.c.a0 = Dj.inp[ep].params.arcA0+Dj.inp[ep].params.arcA1;
					tempSegs(tempSegs_da.cnt).u.c.a1 = a1-Dj.inp[ep].params.arcA1;
				}
				tempSegs_da.cnt++;
				break;
			case curveTypeStraight:
				if ( FindDistance( Dj.inp[ep].params.lineOrig, Dj.inp[ep].params.lineEnd ) <
					 FindDistance( Dj.inp[ep].params.lineOrig, Dj.inp_pos[ep] ) ) {
					tempSegs(tempSegs_da.cnt).type = SEG_STRTRK;
					tempSegs(tempSegs_da.cnt).u.l.pos[0] = Dj.inp[ep].params.lineEnd;
					tempSegs(tempSegs_da.cnt).u.l.pos[1] = Dj.inp_pos[ep];
					tempSegs_da.cnt++;
				}
				break;
			default:
				;
			}
		}

		ok = TRUE;
errorReturn:
		if (!ok)
			 tempSegs(tempSegs_da.cnt).color = drawColorRed;
		switch( Dj.jRes.type ) {
		case curveTypeCurve:
			tempSegs(tempSegs_da.cnt).type = SEG_CRVTRK;
			tempSegs(tempSegs_da.cnt).u.c.center = Dj.jRes.arcP;
			tempSegs(tempSegs_da.cnt).u.c.radius = Dj.jRes.arcR;
			tempSegs(tempSegs_da.cnt).u.c.a0 = Dj.jRes.arcA0;
			tempSegs(tempSegs_da.cnt).u.c.a1 = Dj.jRes.arcA1;
			tempSegs_da.cnt++;
			break;
		case curveTypeStraight:
			tempSegs(tempSegs_da.cnt).type = SEG_STRTRK;
			tempSegs(tempSegs_da.cnt).u.l.pos[0] = Dj.jRes.pos[0];
			tempSegs(tempSegs_da.cnt).u.l.pos[1] = Dj.jRes.pos[1];
			tempSegs_da.cnt++;
			break;
		case curveTypeNone:
			tempSegs_da.cnt = 0;
			break;
		default:
			AbortProg( "Bad track type %d", Dj.jRes.type );
		}
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, drawColorBlack );
		if (!ok)
			Dj.jRes.type = curveTypeNone;
		return C_CONTINUE;

	case C_UP:
		if (Dj.state == 0)
			return C_CONTINUE;
		if (Dj.state == 1) {
			InfoMessage( _("Select 2nd track") );
			return C_CONTINUE;
		}
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, drawColorBlack );
		tempSegs(0).color = drawColorBlack;
		tempSegs_da.cnt = 0;
		if (Dj.jRes.type == curveTypeNone) {
			Dj.state = 1;
			DrawFillCircle( &tempD, Dj.inp[0].pos, 0.10*mainD.scale, selectedColor );
			InfoMessage( _("Select 2nd track") );
			return C_CONTINUE;
		}
		UndoStart( _("Join Tracks"), "newJoin" );
		switch (Dj.jRes.type) {
		case curveTypeStraight:
			trk = NewStraightTrack( Dj.jRes.pos[0], Dj.jRes.pos[1] );
			Dj.jRes.flip = FALSE;
			break;
		case curveTypeCurve:
			trk = NewCurvedTrack( Dj.jRes.arcP, Dj.jRes.arcR,
								  Dj.jRes.arcA0, Dj.jRes.arcA1, 0 );
			break;
		case curveTypeNone:
		case curveTypeBezier:
		case curveTypeCornu:
			return C_CONTINUE;
		}

		CopyAttributes( Dj.inp[0].trk, trk );
		UndrawNewTrack( Dj.inp[0].trk );
		UndrawNewTrack( Dj.inp[1].trk );
		ep = Dj.jRes.flip?1:0;
		Dj.state = 0;
		rc = C_TERMINATE;
		if ( (!JoinTracks( Dj.inp[0].trk, Dj.inp[0].params.ep, Dj.inp_pos[0],
					trk, ep, Dj.jRes.pos[0], &Dj.jointD[0] ) ) ||
			 (!JoinTracks( Dj.inp[1].trk, Dj.inp[1].params.ep, Dj.inp_pos[1],
					trk, 1-ep, Dj.jRes.pos[1], &Dj.jointD[1] ) ) )
			rc = C_ERROR;

		UndoEnd();
		DrawNewTrack( Dj.inp[0].trk );
		DrawNewTrack( Dj.inp[1].trk );
		DrawNewTrack( trk );
		return rc;

#ifdef LATER
	case C_LCLICK:
		if ( (MyGetKeyState() & WKEY_SHIFT) == 0 ) {
			rc = CmdJoin( C_DOWN, pos );
			if (rc == C_TERMINATE)
				return rc;
			return CmdJoin( C_UP, pos );
		}
		if ( selectedTrackCount <= 0 ) {
			ErrorMessage( MSG_NO_SELECTED_TRK );
			return C_CONTINUE;
		}
		if ( (Dj.inp[Dj.joinMoveState].trk = OnTrack( &pos, TRUE, TRUE )) == NULL )
			return C_CONTINUE;
		if (!CheckTrackLayer( Dj.inp[Dj.joinMoveState].trk ) )
			return C_CONTINUE;
		Dj.inp[Dj.joinMoveState].params.ep = PickUnconnectedEndPoint( pos, Dj.inp[Dj.joinMoveState].trk ); /* CHECKME */
		if ( Dj.inp[Dj.joinMoveState].params.ep == -1 ) {
#ifdef LATER
			ErrorMessage( MSG_NO_ENDPTS );
#endif
			return C_CONTINUE;
		}
#ifdef LATER
		if ( GetTrkEndTrk( Dj.inp[Dj.joinMoveState].trk, Dj.inp[Dj.joinMoveState].params.ep ) ) {
			ErrorMessage( MSG_SEL_EP_CONN );
			return C_CONTINUE;
		}
#endif
		if (Dj.joinMoveState == 0) {
			Dj.joinMoveState++;
			InfoMessage( GetTrkSelected(Dj.inp[0].trk)?
				_("Click on an unselected End-Point"):
				_("Click on a selected End-Point") );
			return C_CONTINUE;
		}
		if ( GetTrkSelected(Dj.inp[0].trk) == GetTrkSelected(Dj.inp[1].trk) ) {
			ErrorMessage( MSG_2ND_TRK_NOT_SEL_UNSEL, GetTrkSelected(Dj.inp[0].trk)
					? _("unselected") : _("selected") );
			return C_CONTINUE;
		}
		if (GetTrkSelected(Dj.inp[0].trk))
			MoveToJoin( Dj.inp[0].trk, Dj.inp[0].params.ep, Dj.inp[1].trk, Dj.inp[1].params.ep );
		else
			MoveToJoin( Dj.inp[1].trk, Dj.inp[1].params.ep, Dj.inp[0].trk, Dj.inp[0].params.ep );
		Dj.joinMoveState = 0;
		return C_TERMINATE;
		break;
#endif
	case C_CANCEL:
	case C_REDRAW:
		if ( Dj.joinMoveState == 1 || Dj.state == 1 ) {
			DrawFillCircle( &tempD, Dj.inp[0].pos, 0.10*mainD.scale, selectedColor );
		}
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		break;


	}
	return C_CONTINUE;

}

/*****************************************************************************
 *
 * INITIALIZATION
 *
 */

#include "bitmaps/join.xpm"

void InitCmdJoin( wMenu_p menu )
{
	joinCmdInx = AddMenuButton( menu, CmdJoin, "cmdJoin", _("Join"), wIconCreatePixMap(join_xpm), LEVEL0_50, IC_STICKY|IC_POPUP, ACCL_JOIN, NULL );
	log_join = LogFindIndex( "join" );
}

