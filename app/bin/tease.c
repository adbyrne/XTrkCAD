/** \file tease.c
 * TRANSISTION-CURVES (JOINTS)
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

/*

Transistion-curves (aka joints or spirals) connect curves with different
radius (including straights which have infinite radius, indicated by radius=0).
The curve is described by 2 control parameters: R and L.
L is the length along the tangent of the curve and
R is the radius of an arc at the end of the curve.
At any point (l) along the tangent the arc at that point has radius
r=(R*L)/l.
The transition curve offset (x) is the closest distance between the arc 
and the tangent.
The center of any arc is at (l/2, r+x).
See 'ComputeJointPos()' for details on this.

Warning crude ascii graphics!

a                                                                       aa
 aaa                                                                 aaa *
    aaaa                                                         aaaa   *
        aaaaa                                               aaaaa    ***
             aaaaaaa                                  aaaaaa     ****
                    aaaaaaa                    aaaaaaa      *****
                           aaaaaaaaaaaaaaaaaaaa       ******
                                     ^         *******
                                     x ********
                              *******v*
0*****************************TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT
                                    L/2                                   L

'R' and 'L' are curve control parameters.
'0' is curve origin.
'**..TT' is tangent line.
'a' is arc with radius 'R'.
'*' is the transisition curve.
'x' is transisition curve offset.

For a better representation of this, build 'testjoin' and
do 'testjoin psplot 10 10 40 1 | lpr -Ppostscript'
*/

#include <math.h>

#include "ccurve.h"
#include "cselect.h"
#include "cstraigh.h"
#include "cjoin.h"
#include "cundo.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"

static TRKTYP_T T_EASEMENT = -1;

static ANGLE_T JOINT_ANGLE_INCR = 2.0;

struct extraData {
		DIST_T l0, l1;			/* curve start and end parameter */
		DIST_T R, L;			/* curve control parameters */
		BOOL_T flip;			/* T: endPt[1] - is l0 */
		BOOL_T negate;			/* T: curves to the left */
		BOOL_T Scurve;			/* T: is an S-curve */
		coOrd pos;				/* Pos of origin */
		ANGLE_T angle;			/* Angle of curve tangent */
		};

#define xl0			extraData->l0
#define xl1			extraData->l1
#define xR			extraData->R
#define xL			extraData->L
#define xflip		extraData->flip
#define xnegate		extraData->negate
#define xScurve		extraData->Scurve
#define xpos		extraData->pos
#define xangle		extraData->angle

#define EASE_MIN_X	(0.01)

static int log_ease;
static int log_traverseJoint;


static DIST_T FindL(
		DIST_T r,
		DIST_T R,
		DIST_T L )
/*
 * Given a radius (r) return control value (l).
 * This function is it's inverse!
 */
{
	return (r==0.0)?L:(R*L)/r;
}


static void GetLandD(
		DIST_T *RL,
		DIST_T *RD,
		coOrd q,
		coOrd p,
		ANGLE_T a,
		DIST_T R,
		DIST_T L,
		BOOL_T negate,
		BOOL_T Scurve )
{
	DIST_T l, d, x;

	q.x -= p.x;
	q.y -= p.y;
	Rotate( &q, zero, -a );
	l = q.y;
	x = (l*l*l)/(6*R*L);
	if (!negate) {
		d = q.x - x;
	} else {
		d = q.x + x;
	}
	if (RL)
		*RL = l;
	if (RD)
		*RD = d;
}


int OLDEASE = 0;
static void ComputeJoinPos(
		DIST_T l,
		DIST_T R,
		DIST_T L,
		DIST_T *RR,
		ANGLE_T *RA,
		coOrd *P,
		coOrd *PC )
/*
 * Compute position along transition-curve.
 * Also compute angle and position of tangent circle's center.
 */
{
	ANGLE_T a;
	DIST_T r;
	coOrd pp, pc;
	if (l==0.0)
		r = 100000.0;
	else
		r = (R*L)/l;
	pp.y = l;
	pc.y = l/2.0;
	a = asin( l/2.0 / r );
if (OLDEASE){
	pc.x = l*l / (24*r) + r;
	pp.x = pc.x - r*cos(a);
}else{
	pp.x = (l*l*l)/(6*R*L);
	pc.x = pp.x + r*cos(a);
}
/*lprintf( "A %0.3f %0.3f %0.3f [%0.3f %0.3f]\n", a, aa, aaa, q.x, q.y );*/
	if (P)
		*P = pp;
	if (PC)
		*PC = pc;
	if (RR)
		*RR = r;
	if (RA)
		*RA = R2D(a);
}

static DIST_T JoinD(
		DIST_T l,
		DIST_T R,
		DIST_T L )
/*
 * Compute distance from transition-curve origin to specified point.
 * Distance is approximately equal to length of arc from origin
 * to specified point with radius = 2.0 * radius at point.
 * This is a very good approximation (< 0.1%).
 */
{
	DIST_T rr1, d1;
	ANGLE_T a1;
	coOrd p0;
	DIST_T sign = 1;
	if ( l < 0 ) {
		sign = -1;
		l = -l;
	}
	ComputeJoinPos( l, R, L, &rr1, NULL, &p0, NULL );
	rr1 *= 2.0;
	a1=asin(sqrt(p0.x*p0.x + p0.y*p0.y)/2.0/rr1);
	d1 = rr1 * a1 * 2.0;
	return d1*sign;
}


static DIST_T GetLfromD(
		DIST_T D,
		DIST_T R,
		DIST_T L )
{
	DIST_T deltaD, d, l, deltaL;
	l = L/2.0;
	deltaL = L/4.0;
	while ( deltaL>0.0001 ) {
		d = JoinD(l,R,L);
		if ( d < D ) {
			deltaD = D-d;
		} else {
			deltaD = d-D;
		}
		if ( deltaD < 0.000001 )
			return l;
		if ( d < D )
			l += deltaL;
		else
			l -= deltaL;
		deltaL /= 2.0;
	}
/*printf( "GetLfromD( %0.3f %0.3f %0.3f ) = %0.3f\n", D, R, L, l );*/
	return l;
}


#ifdef LATER
static void JoinDistance(
		DIST_T r,
		DIST_T R,
		DIST_T X,
		DIST_T L,
		DIST_T *dr,
		DIST_T *xr,
		DIST_T *lr )
{
	DIST_T l, d, rr;
	coOrd p, pc;
	if (r == 0.0) {
		*dr = 0.0;
		*lr = *xr = 0.0;
		return;
	}
	l = FindL( r, R, L );
	d = JoinD( l, R, L );
	ComputeJoinPos( l, R, L, NULL, NULL, &p, NULL );
LOG( log_ease, 2, ( "joinDistance r=%0.3f rr=%0.3f\n", r, rr ) )
	*xr = pc.x - rr;
	*dr = d;
	*lr = pc.y;
}
#endif

EXPORT STATUS_T ComputeJoint(
		DIST_T r0,
		DIST_T r1,
		easementData_t * e )
/* 
 * Compute joint data given radius of the 2 curves being joined.
 * Radius is =0 for straight tracks and <0 for left-handed curves.
 * S-curves are handled by treating them as 2 transition-curves joined
 * origin to origin.
 */
{
	DIST_T t, l0, l1, d0, d1, rr0, rr1, xx;
	ANGLE_T a, a0, a1;
	coOrd rp0, rpc0, rp1, rpc1;

LOG( log_ease, 4, ( "ComputeJoint( %0.3f, %0.3f )\n", r0, r1 ) )

	if (easementVal <= 0.1) {
		e->d0 = e->d1 = e->x = 0.0;
		return E_NOTREQ;
	}
	if (r0 != 0.0 && fabs(r0) < easeR) {
		ErrorMessage( MSG_RADIUS_LSS_EASE_MIN,
				FormatDistance(fabs(r0)), FormatDistance(easeR) );
		e->d0 = e->d1 = e->x = 0.0;
		return E_ERROR;
	}
	if (r1 != 0.0 && fabs(r1) < easeR) {
		ErrorMessage( MSG_RADIUS_LSS_EASE_MIN, FormatDistance(fabs(r1)), FormatDistance(easeR) );
		e->d0 = e->d1 = e->x = 0.0;
		return E_ERROR;
	}
	if (r0 == 0.0 && r1 == 0.0) {
		/* ASSERT( FALSE );  CHECKME */
		e->d0 = e->d1 = e->x = 0.0;
		return E_NOTREQ;
	}
	e->r0 = r0;
	e->r1 = r1;
	e->Scurve = FALSE;
	if ( ! ( (r0 >= 0 && r1 >= 0) || (r0 <= 0 && r1 <= 0) ) ) {
		/* S-curve */
		e->Scurve = TRUE;
		e->flip = FALSE;
		e->negate = (r0 > 0.0);
		l0 = FindL( fabs(r0), easeR, easeL );
		ComputeJoinPos( l0, easeR, easeL, &rr0, NULL, &rp0, &rpc0 );
		l1 = FindL( fabs(r1), easeR, easeL );
		ComputeJoinPos( l1, easeR, easeL, &rr1, NULL, &rp1, &rpc1 );
		rp1.x = - rp1.x;
		rp1.y = - rp1.y;
		rpc1.x = - rpc1.x;
		rpc1.y = - rpc1.y;
		xx = FindDistance(rpc0, rpc1) - rr0 - rr1;
		a0 = NormalizeAngle( FindAngle(rpc0, rp0) - FindAngle(rpc0, rpc1) );
		a1 = NormalizeAngle( FindAngle(rpc1, rp1) - FindAngle(rpc1, rpc0) );
		d0 = fabs( rr0 * D2R(a0) );
		d1 = fabs( rr1 * D2R(a1) );
	} else {
		/* ! S-curve */
		e->negate = ( (r0==0.0||r1==0.0)? r0>r1 : r0<r1 );
		r0 = fabs(r0);
		r1 = fabs(r1);
		e->flip = FALSE;
		if ( r1 == 0 || (r0 != 0 && r1 > r0 ) ) {
			e->flip = TRUE;
			t=r0; r0=r1; r1=t;
		}
		if (r0 == 0) {
			if (r1 == 0) {
				xx = l0 = l1 = d0 = d1 = 0.0;
			} else {
				l0 = 0.0;
				l1 = FindL( r1, easeR, easeL );
				ComputeJoinPos( l1, easeR, easeL, &rr1, NULL, &rp1, &rpc1 );
				d0 = rpc1.y;
				a1 = FindAngle(rpc1, rp1) - 270.0;
				d1 = rr1 * D2R(a1);
				xx = rpc1.x - rr1;
			}
		} else {
			l0 = FindL( r0, easeR, easeL );
			ComputeJoinPos( l0, easeR, easeL, &rr0, NULL, &rp0, &rpc0 );
			l1 = FindL( r1, easeR, easeL );
			ComputeJoinPos( l1, easeR, easeL, &rr1, NULL, &rp1, &rpc1 );
			a = FindAngle( rpc0, rpc1 );
			a0 = a - FindAngle(rpc0, rp0);/*???*/
			a1 = FindAngle(rpc1, rp1) - a;
			xx = rr0 - ( rr1 + FindDistance(rpc0, rpc1) );
			d0 = rr0 * D2R(a0);
			d1 = rr1 * D2R(a1);
		}
	}
LOG( log_ease, 2, ( "CJoint(%0.3f %0.3f) l0=%0.3f d0=%0.3f l1=%0.3f d1=%0.3f x=%0.3f S%d F%d N%d\n",
				e->r0, e->r1, l0, d0, l1, d1, xx, e->Scurve, e->flip, e->negate ) )
	if (xx < EASE_MIN_X || d0+d1<=minLength) {
		e->d0 = e->d1 = e->x = 0.0;
		return E_NOTREQ;
	} else {
		if (!e->flip) {
			e->d0 = d0;
			e->d1 = d1;
			e->l0 = l0;
			e->l1 = l1;
		} else {
			e->d0 = d1;
			e->d1 = d0;
			e->l0 = l1;
			e->l1 = l0;
		}
		e->x = xx;
		return E_REQ;
	}
}

static track_p NewJoint(
		coOrd pos0,
		ANGLE_T angle0,
		coOrd pos1,
		ANGLE_T angle1,
		DIST_T trackGauge,
		DIST_T R,
		DIST_T L,
		easementData_t * e )
/*
 * Allocate a joint track segment.
 * Allocate a track, save relevant data from (*e),
 * and compute origin and angle of transition-curve.
 * Position is determined relative to endPoints.
 */
{
	track_p trk;
	struct extraData *xx;
	coOrd p, p0, p1, q0, q1;
	static coOrd qZero = { 0.0, 0.0 };
	ANGLE_T az0, a01, b, b01, b1, d, d1;
	trk = NewTrack( 0, T_EASEMENT, 2, sizeof *xx );
	SetTrkScale( trk, GetLayoutCurScale() );
	xx = GetTrkExtraData( trk );
	SetTrkEndPoint( trk, 0, pos0, NormalizeAngle(angle0+180.0) );
	SetTrkEndPoint( trk, 1, pos1, NormalizeAngle(angle1+180.0) );
	xx->R = R;
	xx->L = L;
	xx->flip = e->flip;
	xx->negate = e->negate;
	xx->Scurve = e->Scurve;
	if (!e->flip) {
		xx->l0 = e->l0;
		xx->l1 = e->l1;
		p0 = pos0;
		p1 = pos1;
	} else {
		xx->l0 = e->l1;
		xx->l1 = e->l0;
		p0 = pos1;
		p1 = pos0;
	}
	ComputeJoinPos( xx->l0, R, L, NULL, NULL, &q0, NULL );
	ComputeJoinPos( xx->l1, R, L, NULL, NULL, &q1, NULL );
	if (e->negate) {
		q0.x = -q0.x;
		q1.x = -q1.x;
	}
	b01 = FindAngle( p0, p1 );
	if (!e->Scurve) {
		az0 = FindAngle( qZero, q0 );
		a01 = FindAngle( q0, q1 );
		b1 = NormalizeAngle( b01 - (a01+az0) );
		b = NormalizeAngle( b01 - a01 );
	} else {
		q1.x = -q1.x;
		q1.y = -q1.y;
		az0 = FindAngle( qZero, q0 );
		a01 = FindAngle( q0, q1 );
		b = NormalizeAngle( b01 - a01 );
	}
	/*a = NormalizeAngle(a0+a1-90.0);*/
	p = q0;
	Rotate( &p, qZero, b );
	xx->pos.x = p0.x - p.x;
	xx->pos.y = p0.y - p.y;
	xx->angle = b;
	ComputeBoundingBox( trk );
	d = FindDistance( p0, p1 );
	d1 = FindDistance( q0, q1 );
LOG( log_ease, 1, ( "NewJoint( [%0.3f %0.3f] A%0.3f, [%0.3f %0.3f] A%0.3f\n    B01=%0.3f AZ0=%0.3f A01=%0.3f B=%0.3f D0=%0.3f D1=%0.3f\n",
				pos0.x, pos0.y, angle0, pos1.x, pos1.y, angle1,
				b01, az0, a01, b, d, d1 ) )
	CheckTrackLength( trk );
	return trk;
}

/****************************************
 *
 * GENERIC FUNCTIONS
 *
 */

static DIST_T GetLengthJoint( track_p trk )
{
	struct extraData *xx;
	DIST_T d0, d1;
	xx = GetTrkExtraData(trk);
	d0 = JoinD( xx->l0, xx->R, xx->L );
	d1 = JoinD( xx->l1, xx->R, xx->L );
	if (xx->Scurve)
		return d0+d1;
	else 
		return fabs( d0-d1 );
}


static struct {
		coOrd endPt[2];
		DIST_T elev[2];
		FLOAT_T length;
		coOrd orig;
		ANGLE_T angle;
		DIST_T r;
		DIST_T l;
		DIST_T l0;
		DIST_T l1;
		FLOAT_T grade;
		descPivot_t pivot;
		unsigned int layerNumber;
		} jointData;
typedef enum { E0, Z0, E1, Z1, OR, AL, RR, LL, L0, L1, GR, PV, LY } jointDesc_e;
static descData_t jointDesc[] = {
/*E0*/	{ DESC_POS, N_("End Pt 1: X"), &jointData.endPt[0] },
/*Z0*/	{ DESC_DIM, N_("Z"), &jointData.elev[0] },
/*E1*/	{ DESC_POS, N_("End Pt 2: X"), &jointData.endPt[1] },
/*Z1*/	{ DESC_DIM, N_("Z"), &jointData.elev[1] },
/*OR*/	{ DESC_POS, N_("Origin: X"), &jointData.orig },
/*AL*/	{ DESC_ANGLE, N_("Angle"), &jointData.angle },
/*RR*/	{ DESC_DIM, N_("R"), &jointData.r },
/*LL*/	{ DESC_DIM, N_("L"), &jointData.l },
/*L0*/	{ DESC_DIM, N_("l0"), &jointData.l0 },
/*L1*/	{ DESC_DIM, N_("l1"), &jointData.l1 },
/*GR*/	{ DESC_FLOAT, N_("Grade"), &jointData.grade },
/*PV*/	{ DESC_PIVOT, N_("Pivot"), &jointData.pivot },
/*LY*/	{ DESC_LAYER, N_("Layer"), &jointData.layerNumber },
		{ DESC_NULL } };

static void UpdateJoint( track_p trk, int inx, descData_p descUpd, BOOL_T final )
{
	EPINX_T ep;
	switch (inx) {
	case Z0:
	case Z1:
		ep = (inx==Z0?0:1);
		UpdateTrkEndElev( trk, ep, GetTrkEndElevUnmaskedMode(trk,ep), jointData.elev[ep], NULL );
		ComputeElev( trk, 1-ep, FALSE, &jointData.elev[1-ep], NULL );
		if ( jointData.length > minLength )
			jointData.grade = fabs( (jointData.elev[0]-jointData.elev[1])/jointData.length )*100.0;
		else
			jointData.grade = 0.0;
		jointDesc[GR].mode |= DESC_CHANGE;
		jointDesc[inx==Z0?Z1:Z0].mode |= DESC_CHANGE;
		return;
	case LY:
		SetTrkLayer( trk, jointData.layerNumber );
		break;
	default:
		return;
	}
}


static void DescribeJoint(
		track_p trk,
		char * str,
		CSIZE_T len )
/*
 * Print some interesting info about the track.
 */
{
	struct extraData *xx = GetTrkExtraData(trk);
	int fix0, fix1;

	sprintf( str, _("Joint Track(%d): Layer=%d Length=%0.3f EP=[%0.3f,%0.3f A%0.3f] [%0.3f,%0.3f A%0.3f]"), GetTrkIndex(trk),
		GetTrkLayer(trk)+1,
		GetLengthJoint( trk ),
		GetTrkEndPosXY(trk,0), GetTrkEndAngle(trk,0),
		GetTrkEndPosXY(trk,1), GetTrkEndAngle(trk,1) );

	fix0 = GetTrkEndTrk(trk,0)!=NULL;
	fix1 = GetTrkEndTrk(trk,1)!=NULL;

	jointData.endPt[0] = GetTrkEndPos(trk,0);
	jointData.endPt[1] = GetTrkEndPos(trk,1);
	jointData.length = GetLengthJoint(trk);
	jointData.orig = xx->pos;
	jointData.angle = xx->angle;
	jointData.r = xx->R;
	jointData.l = xx->L;
	jointData.l0 = xx->l0;
	jointData.l1 = xx->l1;
	jointData.layerNumber = GetTrkLayer(trk);
	ComputeElev( trk, 0, FALSE, &jointData.elev[0], NULL );
	ComputeElev( trk, 1, FALSE, &jointData.elev[1], NULL );
	if ( jointData.length > minLength )
		jointData.grade = fabs( (jointData.elev[0]-jointData.elev[1])/jointData.length )*100.0;
	else
		jointData.grade = 0.0;

	jointDesc[E0].mode =
	jointDesc[E1].mode = 
	jointDesc[OR].mode = 
	jointDesc[AL].mode = 
	jointDesc[RR].mode = 
	jointDesc[LL].mode = 
	jointDesc[L0].mode = 
	jointDesc[L1].mode = 
		DESC_RO;
	jointDesc[Z0].mode = (EndPtIsDefinedElev(trk,0)?0:DESC_RO)|DESC_NOREDRAW;
	jointDesc[Z1].mode = (EndPtIsDefinedElev(trk,1)?0:DESC_RO)|DESC_NOREDRAW;
	jointDesc[GR].mode = DESC_RO;
	jointDesc[PV].mode = (fix0|fix1)?DESC_IGNORE:0;
	jointDesc[LY].mode = DESC_NOREDRAW;
	jointData.pivot = (fix0&fix1)?DESC_PIVOT_NONE:
						fix0?DESC_PIVOT_FIRST:
						fix1?DESC_PIVOT_SECOND:
						DESC_PIVOT_MID;

	DoDescribe( _("Easement Track"), trk, jointDesc, UpdateJoint );
}

static void GetJointPos(
		coOrd * RP,
		ANGLE_T * RA,
		DIST_T l,
		DIST_T R,
		DIST_T L,
		coOrd P,
		ANGLE_T A,
		BOOL_T N )
/*
 * Compute position of point on transition-curve.
 */
{
	coOrd p1;
	static coOrd pZero = {0.0,0.0};
	ComputeJoinPos( l, R, L, NULL, RA, &p1, NULL );
	if (N)
		p1.x = -p1.x;
	Rotate( &p1, pZero, A );
	if (RP) {
		RP->x = P.x + p1.x;
		RP->y = P.y + p1.y;
	}
	if (RA)
		*RA = NormalizeAngle( A + (N?-*RA:*RA) );
}


EXPORT DIST_T JointDistance(
		coOrd * q,
		coOrd pos,
		ANGLE_T angle,
		DIST_T l0,
		DIST_T l1,
		DIST_T R,
		DIST_T L,
		BOOL_T negate,
		BOOL_T Scurve )
{
	DIST_T d, l;
	coOrd p0 = *q;
	GetLandD( &l, &d, p0, pos, angle, R, L, negate, Scurve );
	if (Scurve) {
		if ( l < -l1 )
			l = -l1;
		else if ( l > l0 )
			l = l0;
	} else {
		if ( l < l0 )
			l = l0;
		else if ( l > l1 )
			l = l1;
	}
	GetJointPos( q, NULL, l, R, L, pos, angle, negate );
	d = FindDistance( p0, *q );
	return d;
}


static DIST_T DistanceJoint(
		track_p trk,
		coOrd * p )
/*
 * Determine how close (p) is to (t).
 */
{
	struct extraData * xx = GetTrkExtraData(trk);
	return JointDistance( p, xx->pos, xx->angle, xx->l0, xx->l1, xx->R, xx->L, xx->negate, xx->Scurve );
}


#ifdef LATER
static void DrawJointSegment1(
		drawCmd_p d,
		wIndex_t cnt,
		DIST_T l0,
		DIST_T l1,
		DIST_T R,
		DIST_T L,
		coOrd P,
		ANGLE_T A,
		BOOL_T N,
		track_p trk,
		DIST_T trackGauge,
		wDrawColor color )
/*
 * Draw a transition-curve from (l0) to (l1),
 * at angle (A) from origin (P).
 */
{
	DIST_T l, lincr;
	wIndex_t i;
	coOrd p0, p1;
	long widthOptions = DTS_RIGHT|DTS_LEFT|DTS_TIES;

	if (GetTrkWidth(trk) == 2)
		widthOptions |= DTS_THICK2;
	if (GetTrkWidth(trk) == 3)
		widthOptions |= DTS_THICK3;

	l = l0;
	lincr = (l1-l0)/cnt;
	GetJointPos( &p0, NULL, l0, R, L, P, A, N );
	for (i=1; i<=cnt; i++) {
		l += lincr;
		GetJointPos( &p1, NULL, l, R, L, P, A, N );
		DrawStraightTrack( d, p0, p1,
				FindAngle( p1, p0 ), trk, trackGauge, color, widthOptions );
		p0 = p1;
	}
}
#endif

static void DrawJointSegment(
		drawCmd_p d,
		wIndex_t cnt,
		DIST_T l0,
		DIST_T l1,
		DIST_T R,
		DIST_T L,
		coOrd P,
		ANGLE_T A,
		BOOL_T N,
		DIST_T trackGauge,
		wDrawColor color,
		long widthOptions,
		track_p trk )
/*
 * Draw a transition-curve from (l0) to (l1),
 * at angle (A) from origin (P).
 */
{
	DIST_T ll;
	wIndex_t i;
	coOrd p0, p1;
	ANGLE_T a0, a1;
	int cnt1;

	ComputeJoinPos( l0, R, L, NULL, &a0, NULL, NULL );
	ComputeJoinPos( l1, R, L, NULL, &a1, NULL, NULL );
	a1 = a1-a0;
	if ( (d->options&DC_QUICK) ) {
		cnt1 = 1;
	} else {
		cnt1 = (int)floor(a1/JOINT_ANGLE_INCR) + 1;
		a1 /= cnt1;
	}

	widthOptions |= DTS_RIGHT|DTS_LEFT|DTS_TIES;
	GetJointPos( &p0, NULL, l0, R, L, P, A, N );
	for (i=1; i<=cnt1; i++) {
		a0 += a1;
		ll = sqrt( sin(D2R(a0)) * 2 * R * L );
		GetJointPos( &p1, NULL, ll, R, L, P, A, N );
		DrawStraightTrack( d, p0, p1, FindAngle( p1, p0 ), trk, trackGauge,
								color, widthOptions );
		p0 = p1;
	}
}


EXPORT coOrd GetJointSegEndPos(
		coOrd pos,
		ANGLE_T angle,
		DIST_T l0,
		DIST_T l1,
		DIST_T R,
		DIST_T L,
		BOOL_T negate,
		BOOL_T flip,
		BOOL_T Scurve,
		EPINX_T ep,
		ANGLE_T * angleR )
{
	coOrd p1;
	DIST_T ll;
	if ( flip ) ep = 1-ep;
	ll = (ep==0?l0:l1);
	if ( Scurve ) {
		if ( ep==1 )
			angle += 180;
	}
	GetJointPos( &p1, &angle, ll, R, L, pos, angle, negate );
	if ( angleR ) {
		if ( (!Scurve) && ep==0 )
			angle = NormalizeAngle(angle+180);
		*angleR = angle;
	}
	return p1;
}


EXPORT void DrawJointTrack(
		drawCmd_p d,
		coOrd pos,
		ANGLE_T angle,
		DIST_T l0,
		DIST_T l1,
		DIST_T R,
		DIST_T L,
		BOOL_T negate,
		BOOL_T flip,
		BOOL_T Scurve,
		track_p trk,
		EPINX_T ep0,
		EPINX_T ep1,
		DIST_T trackGauge,
		wDrawColor color,
		long options )
{
	wIndex_t cnt;
	DIST_T len;
	trkSeg_p segPtr;

	if ( (d->options&DC_SEGTRACK) ) {
		DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
		segPtr = &tempSegs(tempSegs_da.cnt-1);
		segPtr->type = SEG_JNTTRK;
		segPtr->width = 0;
		segPtr->color = wDrawColorBlack;
		segPtr->u.j.pos = pos;
		segPtr->u.j.angle = angle;
		segPtr->u.j.l0 = l0;
		segPtr->u.j.l1 = l1;
		segPtr->u.j.R = R;
		segPtr->u.j.L = L;
		segPtr->u.j.negate = negate;
		segPtr->u.j.flip = flip;
		segPtr->u.j.Scurve = Scurve;
		return;
	}
LOG( log_ease, 4, ( "DJT( (X%0.3f Y%0.3f A%0.3f) \n", pos.x, pos.y, angle ) )
#ifdef LATER
	scale2rail = (d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale;

	if (options&DTS_THICK2)
		width = 2;
	if (options&DTS_THICK3)
		width = 3;
#ifdef WINDOWS
	width *= (wDrawWidth)(d->dpi/mainD.dpi);
#else
	if (d->options&DC_PRINT)
		width *= 300/75;
#endif
#endif
	if (color == wDrawColorBlack)
		color = normalColor;
	if (!Scurve) {
		/* print segments about 0.20" long */
		len = (l0-l1)/(0.20*d->scale);
		cnt = (int)ceil(fabs(len));
		if (cnt == 0 || (d->options&DC_QUICK)) cnt = 1;
		DrawJointSegment( d, cnt, l0, l1, R, L, pos,
						angle, negate, trackGauge, color, options, trk );
	} else {
		/* print segments about 0.20" long */
		cnt = (int)ceil((l0)/(0.20*d->scale));
		if (cnt == 0 || (d->options&DC_QUICK)) cnt = 1;
		DrawJointSegment( d, cnt, 0, l0, R, L, pos,
						angle, negate, trackGauge, color, options, trk );
		cnt = (int)ceil((l1)/(0.20*d->scale));
		if (cnt == 0 || (d->options&DC_QUICK)) cnt = 1;
		DrawJointSegment( d, cnt, 0, l1, R, L, pos,
						angle+180, negate, trackGauge, color, options, trk );
	}
	if ( (d->funcs->options & wDrawOptTemp) == 0 && (d->options&DC_QUICK) == 0 ) {
		DrawEndPt( d, trk, ep0, color );
		DrawEndPt( d, trk, ep1, color );
	}
}


static void DrawJoint(
		track_p trk,
		drawCmd_p d,
		wDrawColor color )
/*
 * Draw a transition-curve.
 */
{
	struct extraData * xx = GetTrkExtraData(trk);
	long widthOptions = 0;

	if (GetTrkWidth(trk) == 2)
		widthOptions = DTS_THICK2;
	if (GetTrkWidth(trk) == 3)
		widthOptions = DTS_THICK3;
	DrawJointTrack( d, xx->pos, xx->angle, xx->l0, xx->l1, xx->R, xx->L, xx->negate, xx->flip, xx->Scurve, trk, 0, 1, GetTrkGauge(trk), color, widthOptions );
}


static void DeleteJoint(
		track_p t )
/* Delete track - nothing to do */
{
}

static BOOL_T WriteJoint(
		track_p t,
		FILE * f )
/*
 * Write track data to a file (f).
 */
{
	struct extraData * xx = GetTrkExtraData(t);
	BOOL_T rc = TRUE;
	rc &= fprintf(f, "JOINT %d %d %ld 0 0 %s %d %0.6f %0.6f %0.6f %0.6f %d %d %d %0.6f %0.6f 0 %0.6f\n",
		GetTrkIndex(t), GetTrkLayer(t), (long)GetTrkWidth(t),
		GetTrkScaleName(t), GetTrkVisible(t), xx->l0, xx->l1, xx->R, xx->L,
		xx->flip, xx->negate, xx->Scurve, xx->pos.x, xx->pos.y, xx->angle )>0;
	rc &= WriteEndPt( f, t, 0 );
	rc &= WriteEndPt( f, t, 1 );
	rc &= fprintf(f, "\tEND\n" )>0;
	return rc;
}

static void ReadJoint(
		char * line )
/*
 * Read track data from a file (f).
 */
{
	track_p trk;
	TRKINX_T index;
	BOOL_T visible;
	struct extraData e, *xx;
	char scale[10];
	wIndex_t layer;
	long options;
	DIST_T elev;

	if ( !GetArgs( line+6, paramVersion<3?"dXZsdffffdddpYf":paramVersion<9?"dLl00sdffffdddpYf":"dLl00sdffffdddpff",
		&index, &layer, &options, scale, &visible, &e.l0, &e.l1, &e.R, &e.L,
		&e.flip, &e.negate, &e.Scurve, &e.pos, &elev, &e.angle) )
		return;
	trk = NewTrack( index, T_EASEMENT, 0, sizeof e );
	xx = GetTrkExtraData(trk);
	SetTrkVisible(trk, visible);
	SetTrkScale(trk, LookupScale(scale));
	SetTrkLayer(trk, layer);
	SetTrkWidth(trk, (int)(options&3));
	*xx = e;
	ReadSegs();
	SetEndPts( trk, 2 );
	ComputeBoundingBox( trk );
}

static void MoveJoint(
		track_p trk,
		coOrd orig )
/*
 * Move a track.
 */
{
	struct extraData * xx = GetTrkExtraData(trk);
	xx->pos.x += orig.x;
	xx->pos.y += orig.y;
	ComputeBoundingBox( trk );
}

static void RotateJoint(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
/*
 * Rotate a track.
 */
{
	struct extraData * xx = GetTrkExtraData(trk);
	Rotate( &xx->pos, orig, angle );
	xx->angle = NormalizeAngle( xx->angle+angle );
	ComputeBoundingBox( trk );
}


static void RescaleJoint( track_p trk, FLOAT_T ratio )
{
	struct extraData *xx = GetTrkExtraData(trk);
	xx->pos.x *= ratio;
	xx->pos.y *= ratio;
	xx->R *= ratio;
	xx->L *= ratio;
	xx->l0 *= ratio;
	xx->l1 *= ratio;
}


static ANGLE_T GetAngleJoint( track_p trk, coOrd pos, EPINX_T * ep0, EPINX_T * ep1 )
{
	struct extraData * xx = GetTrkExtraData(trk);
	DIST_T l;
	ANGLE_T a;
	if ( ep0 && ep1 ) {
		if (xx->flip) {
			*ep0 = 1;
			*ep1 = 0;
		} else {
			*ep0 = 0;
			*ep1 = 1;
		}
	}
	GetLandD( &l, NULL, pos, xx->pos, xx->angle, xx->R, xx->L, xx->negate, xx->Scurve );
	if (small(l)) {
		a = xx->angle;
	} else {
/*    if (xx->Scurve && NormalizeAngle(FindAngle(xx->pos,pos)-xx->angle+90.0) > 180.0)*/
		if (xx->Scurve && l < 0.0) {
			GetJointPos( NULL, &a, -l, xx->R, xx->L, xx->pos, xx->angle+180.0, xx->negate );
			a = NormalizeAngle( a-180.0 );
		} else {
			GetJointPos( NULL, &a, l, xx->R, xx->L, xx->pos, xx->angle, xx->negate );
		}
	}
	return NormalizeAngle(a+180.0);
}


static void SplitJointA(
		coOrd * posR,
		EPINX_T ep,
		struct extraData * xx,
		struct extraData * xx1,
		ANGLE_T * aR )
{
	struct extraData * xx0;
	BOOL_T flip;
	DIST_T l;

	*xx1 = *xx;
	if ( (ep==1) == (!xx->flip) ) {
		xx0 = xx;
		flip = FALSE;
	} else {
		xx0 = xx1;
		xx1 = xx;
		flip = TRUE;
	}
	GetLandD( &l, NULL, *posR, xx->pos, xx->angle, xx->R, xx->L, xx->negate, xx->Scurve );

	if (!xx->Scurve) {
		if (l < xx->l0 || l > xx->l1) {
			NoticeMessage2( 0, "splitJoint: ! %0.3f <= %0.3f <= %0.3f", _("Ok"), NULL, xx->l0, l, xx->l1 );
			if ( l < xx->l0 ) l = xx->l0;
			else if ( l > xx->l1 ) l = xx->l1;
		}
		GetJointPos( posR, aR, l, xx->R, xx->L, xx->pos, xx->angle, xx->negate );
		xx0->l1 = xx1->l0 = l;
	} else if (small(l)){
		xx0->Scurve = xx1->Scurve = 0;
		xx0->l1 = xx0->l0;
		xx0->flip = !xx0->flip;
		xx1->angle = NormalizeAngle(xx1->angle+180.0);
		xx0->l0 = xx1->l0 = 0;
		*posR = xx->pos;
		*aR = xx1->angle;
	} else {
		GetJointPos( posR, aR, l, xx->R, xx->L, xx->pos, xx->angle, xx->negate );
		if (l > 0) {
			xx0->Scurve = 0;
			xx0->l1 = xx0->l0;
			xx0->flip = !xx0->flip;
			xx0->l0 = l;
			xx1->l0 = l;
		} else {
			xx1->Scurve = 0;
			xx1->l0 = -l;
			xx1->angle = NormalizeAngle( xx1->angle+180.0 );
			xx0->l1 = -l;
		}
		*aR = NormalizeAngle( *aR+180.0 );
	}
	if (flip)
		*aR = NormalizeAngle( *aR + 180.0 );
}


static BOOL_T SplitJoint( track_p trk, coOrd pos, EPINX_T ep, track_p * leftover, EPINX_T *ep0, EPINX_T *ep1 )
{
	struct extraData *xx, *xx1;
	track_p trk1;
	ANGLE_T a;

	xx = GetTrkExtraData(trk);
	trk1 = NewTrack( 0, T_EASEMENT, 2, sizeof *xx );
	xx1 = GetTrkExtraData(trk1);
	*xx1 = *xx;
	SetTrkEndPoint( trk1, ep, GetTrkEndPos(trk,ep), GetTrkEndAngle(trk,ep) );
	*leftover = trk1;
	*ep0 = 1-ep;
	*ep1 = ep;
	SplitJointA( &pos, ep, xx, xx1, &a );
	SetTrkEndPoint( trk, ep, pos, a );
	SetTrkEndPoint( trk1, 1-ep, pos, NormalizeAngle(a+180.0) );

	ComputeBoundingBox( trk );
	ComputeBoundingBox( trk1 );
	return TRUE;
}


static BOOL_T TraverseJoint(
		coOrd * posR,
		ANGLE_T *angleR,
		DIST_T *distR,
		coOrd pos,
		ANGLE_T angle,
		DIST_T l0,
		DIST_T l1,
		DIST_T R,
		DIST_T L,
		BOOL_T negate,
		BOOL_T flip,
		BOOL_T Scurve )
{
		
	DIST_T l, lx, d, dx, ll0, ll1, d0, d1;
	BOOL_T from_tangent, flip_angle;

	GetLandD( &l, &d, *posR, pos, angle, R, L, negate, Scurve );

LOG( log_traverseJoint, 2, ( "TJ: [%0.3f %0.3f] D%0.3f l0:%0.3f l1:%0.3f [%0.3f %0.3f] A%0.3f N%d F%d S%d = l:%0.3f ",
		posR->x, posR->y, *distR, l0, l1, pos.x, pos.y, angle, negate, flip, Scurve, l ) )

	if ( (!Scurve) ) {
		if ( l < l0 ) l = l0;
		else if ( l > l1 ) l = l1;
	} else {
		if ( l > l0 ) l = l0;
		else if ( l < -l1 ) l = -l1;
	}

	lx = l;
	from_tangent = !flip;
	flip_angle = from_tangent;
	if ( !Scurve ) {
		ll0 = l0;
		ll1 = l1;
	} else if ( l > 0 ) {
		ll1 = l0;
		ll0 = 0;
	} else {
		ll0 = 0;
		ll1 = l1;
		lx = -l;
		from_tangent = !from_tangent;
	}
	dx = JoinD( lx, R, L );
	d0 = JoinD( ll0, R, L );
	d1 = JoinD( ll1, R, L );
	if ( from_tangent )
		d = d1 - dx;
	else
		d = dx - d0;
	if ( *distR < d ) {
		if ( from_tangent ) {
			d = dx + *distR;
		} else {
			d = dx - *distR;
		}
		lx = GetLfromD( d, R, L );
		if ( l < 0 )
			lx = - lx;
		/* compute posR and angleR */
		GetJointPos( posR, angleR, lx, R, L, pos, angle, negate );
		if ( ! flip_angle )
			*angleR = NormalizeAngle( *angleR + 180.0 );
		*distR = 0;
		goto doreturn;
	}
	*distR -= d;
	if ( Scurve && (!from_tangent) ) {
		/* skip over midpoint */
		if ( l > 0 )
			d = JoinD( l1, R, L );
		else
			d = JoinD( l0, R, L );
		if ( *distR < d ) {
			lx = GetLfromD( *distR, R, L );
			if ( l > 0 )
				lx = - lx;
			GetJointPos( posR, angleR, lx, R, L, pos, angle, negate );
			if ( ! flip_angle )
				*angleR = NormalizeAngle( *angleR + 180.0 );
			*distR = 0;
			goto doreturn;
		}
		*distR -= d;
	}
doreturn:
LOG( log_traverseJoint, 2, ( " [%0.3f %0.3f] A%0.3f D%0.3f\n", posR->x, posR->y, *angleR, *distR ) )
	return TRUE;
}


static BOOL_T TraverseJointTrack(
		traverseTrack_p trvTrk,
		DIST_T * distR )
{
	track_p trk = trvTrk->trk;
	struct extraData * xx = GetTrkExtraData(trk);
	BOOL_T rc;
	EPINX_T ep;
	ANGLE_T angle;
	BOOL_T flip;

	angle = NormalizeAngle( xx->angle-trvTrk->angle );
	flip = ( angle < 270 && angle > 90 );
	rc = TraverseJoint( &trvTrk->pos, &trvTrk->angle, distR, xx->pos, xx->angle, xx->l0, xx->l1, xx->R, xx->L, xx->negate, flip, xx->Scurve );
	if ( *distR > 0 ) {
		ep = (flip?0:1);
		if ( xx->flip )
			ep = 1-ep;
		if ( xx->Scurve )
			ep = 1-ep;
		trvTrk->pos = GetTrkEndPos( trk, ep );
		trvTrk->angle = GetTrkEndAngle( trk, ep );
		trvTrk->trk = GetTrkEndTrk( trk, ep );
	}
	return rc;
}


static BOOL_T EnumerateJoint( track_p trk )
{
	if (trk != NULL) {
		ScaleLengthIncrement( GetTrkScale(trk), GetLengthJoint(trk) );
	}
	return TRUE;
}

static BOOL_T TrimJoint( track_p trk, EPINX_T ep, DIST_T maxX )
{
	DeleteTrack( trk, FALSE );
	return TRUE;
}


static BOOL_T MergeJoint(
		track_p trk0,
		EPINX_T ep0,
		track_p trk1,
		EPINX_T ep1 )
{
	track_p trk2;
	EPINX_T ep2=-1;
	coOrd pos;
	ANGLE_T a;
	struct extraData *xx0 = GetTrkExtraData(trk0);
	struct extraData *xx1 = GetTrkExtraData(trk1);

	if ( ep0 == ep1 )
		return FALSE;
	if ( xx0->R != xx1->R ||
		 xx0->L != xx1->L ||
		 xx0->flip != xx1->flip ||
		 xx0->negate != xx1->negate ||
		 xx0->angle != xx1->angle ||
		 xx0->Scurve ||
		 xx1->Scurve ||
		 FindDistance( xx0->pos, xx1->pos ) > connectDistance )
		return FALSE;

	UndoStart( _("Merge Easements"), "MergeJoint( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
	UndoModify( trk0 );
	UndrawNewTrack( trk0 );
	trk2 = GetTrkEndTrk( trk1, 1-ep1 );
	if (trk2) {
		ep2 = GetEndPtConnectedToMe( trk2, trk1 );
		DisconnectTracks( trk1, 1-ep1, trk2, ep2 );
	}

	if (ep0 == 0) {
		xx0->l0 = xx1->l0;
	} else {
		xx0->l1 = xx1->l1;
	}

	pos = GetTrkEndPos( trk1, 1-ep1 );
	a = GetTrkEndAngle( trk1, 1-ep1 );
	SetTrkEndPoint( trk0, ep0, pos, a );
	ComputeBoundingBox( trk0 );

	DeleteTrack( trk1, TRUE );
	if (trk2) {
		ConnectTracks( trk0, ep0, trk2, ep2 );
	}
	DrawNewTrack( trk0 );
	return TRUE;
}


static BOOL_T GetParamsJoint( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	params->type = curveTypeStraight;
	params->ep = PickUnconnectedEndPoint( pos, trk );
	if (params->ep == -1)
		 return FALSE;
	params->lineOrig = GetTrkEndPos(trk,params->ep);
	params->lineEnd = params->lineOrig;
	params->angle = GetTrkEndAngle(trk,params->ep);
	params->len = 0.0;
	params->arcR = 0.0;
	return TRUE;
}


static BOOL_T MoveEndPtJoint( track_p *trk, EPINX_T *ep, coOrd pos, DIST_T d )
{
	return FALSE;
}


static BOOL_T QueryJoint( track_p trk, int query )
{
	struct extraData * xx = GetTrkExtraData(trk);
	track_p trk1;

	switch ( query ) {
	case Q_CANNOT_BE_ON_END:
	case Q_IGNORE_EASEMENT_ON_EXTEND:
	case Q_ISTRACK:
		return TRUE;
	case Q_CAN_PARALLEL:
		if ( xx->Scurve ) {
			if ( FindDistance( xx->pos, GetTrkEndPos(trk,0) ) <= minLength ||
				 FindDistance( xx->pos, GetTrkEndPos(trk,1) ) <= minLength )
				return FALSE;
			UndoStart( _("Split Easement Curve"), "queryJoint T%d Scurve", GetTrkIndex(trk) );
			SplitTrack( trk, xx->pos, 0, &trk1, FALSE );
		}
		return TRUE;
	default:
		return FALSE;
	}
}


static void FlipJoint(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraData * xx = GetTrkExtraData(trk);
	FlipPoint( &xx->pos, orig, angle );
	xx->angle = NormalizeAngle( 2*angle - xx->angle );
	xx->negate = !xx->negate;
	ComputeBoundingBox( trk );
}


static BOOL_T MakeParallelJoint(
		track_p trk,
		coOrd pos,
		DIST_T sep,
		track_p * newTrkR,
		coOrd * p0R,
		coOrd * p1R )
{
	struct extraData * xx = GetTrkExtraData(trk), *xx1;
	ANGLE_T angle, A;
	coOrd p0, p1, P, q1, r1;
	DIST_T d, d0;
	DIST_T R, L, l0, l1, len, dl;
	int cnt, inx;

	if ( xx->Scurve )
		return FALSE;
	GetLandD( NULL, &d, pos, xx->pos, xx->angle, xx->R, xx->L, xx->negate, FALSE );
	angle = 90.0;
	if ( (d < 0) == xx->negate )
		sep = -sep;
	if ( xx->negate )
		angle = -90.0;
	if ( xx->flip )
		angle = -angle;
	p0 = GetTrkEndPos(trk,0);
	p1 = GetTrkEndPos(trk,1);
	d0 = FindDistance( p0, p1 );
	Translate( &p0, p0, GetTrkEndAngle(trk,0)+angle, sep );
	Translate( &p1, p1, GetTrkEndAngle(trk,1)-angle, sep );
	d = FindDistance( p0, p1 );
	angle = R2D(asin(xx->L/2/xx->R));
	A = xx->angle;
	R = xx->R + sep*sin(D2R(angle));

	dl = JoinD( xx->l1, xx->R, xx->L ) - JoinD( xx->l0, xx->R, xx->L );
/*printf( "D = %0.3f %0.3f\n", d, dl );*/
	d /= d0;
	R = xx->R * d;
	L = xx->L * d;
	l0 = xx->l0 * d;
	l1 = xx->l1 * d;
/*printf( "  R=%0.3f, L=%0.3f, l0=%0.3f, l1=%0.3f\n", R, L, l0, l1 );*/
	Translate( &P, xx->pos, xx->angle+(xx->negate?90:-90), sep );
	ComputeJoinPos( l1, R, L, NULL, NULL, &q1, NULL );
	r1 = (xx->flip?p0:p1);
	r1.x -= P.x;
	r1.y -= P.y;
	Rotate( &r1, zero, -A );
	if ( xx->negate )
		r1.x = -r1.x;
	if ( r1.x > 0 && q1.x > 0 ) {
/*printf( "    %0.3f %0.3f, R=%0.3f ", q1.x, r1.x, R );*/
		R *= q1.x/r1.x;
/*printf( " %0.3f\n", R );*/
	}

	if ( newTrkR ) {
		*newTrkR = NewTrack( 0, T_EASEMENT, 2, sizeof *xx );
		xx1 = GetTrkExtraData( *newTrkR );
		*xx1 = *xx;
		xx1->angle = A;
		xx1->R = R;
		xx1->L = L;
		xx1->l0 = l0;
		xx1->l1 = l1;
		xx1->pos = P;
		SetTrkEndPoint( *newTrkR, 0, p0, GetTrkEndAngle(trk,0) );
		SetTrkEndPoint( *newTrkR, 1, p1, GetTrkEndAngle(trk,1) );
		ComputeBoundingBox( *newTrkR );
	} else {
		/* print segments about 0.20" long */
		dl = fabs(l0-l1);
		len = dl/(0.20*mainD.scale);
		cnt = (int)ceil(len);
		if (cnt == 0 || (mainD.options&DC_QUICK)) cnt = 1;
		dl /= cnt;
		DYNARR_SET( trkSeg_t, tempSegs_da, cnt );
		for ( inx=0; inx<cnt; inx++ ) {
			tempSegs(inx).color = wDrawColorBlack;
			tempSegs(inx).width = 0;
			tempSegs(inx).type = SEG_STRTRK;
			if ( inx == 0 ) {
				GetJointPos( &tempSegs(inx).u.l.pos[0], NULL, l0, R, L, P, A, xx->negate );
			} else {
				tempSegs(inx).u.l.pos[0] = tempSegs(inx-1).u.l.pos[1];
			}
			l0 += dl;
			GetJointPos( &tempSegs(inx).u.l.pos[1], NULL, l0, R, L, P, A, xx->negate );
		}
		tempSegs_da.cnt = cnt;
	}
	if ( p0R ) *p0R = p0;
	if ( p1R ) *p1R = p1;
	return TRUE;
}


static trackCmd_t easementCmds = {
		"JOINT",
		DrawJoint,
		DistanceJoint,
		DescribeJoint,
		DeleteJoint,
		WriteJoint,
		ReadJoint,
		MoveJoint,
		RotateJoint,
		RescaleJoint,
		NULL,		/* audit */
		GetAngleJoint,
		SplitJoint,
		TraverseJointTrack,
		EnumerateJoint,
		NULL,		/* redraw */
		TrimJoint,
		MergeJoint,
		ExtendStraightFromOrig,
		GetLengthJoint,
		GetParamsJoint,
		MoveEndPtJoint,
		QueryJoint,
		NULL,		/* ungroup */
		FlipJoint,
		NULL,
		NULL,
		NULL,
		MakeParallelJoint };


EXPORT void JointSegProc(
		segProc_e cmd,
		trkSeg_p segPtr,
		segProcData_p data )
{
	DIST_T l;
	ANGLE_T a;
	BOOL_T flip;
	struct extraData * xx, xxx[2];
	coOrd p;
	int inx;
	EPINX_T ep0;

	switch (cmd) {

	case SEGPROC_TRAVERSE1:
		GetLandD( &l, NULL, data->traverse1.pos, segPtr->u.j.pos, segPtr->u.j.angle, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.negate, segPtr->u.j.Scurve );
		if (small(l)) {
			a = segPtr->u.j.angle;
		} else {
			if (segPtr->u.j.Scurve && l < 0.0) {
				GetJointPos( NULL, &a, -l, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.pos, segPtr->u.j.angle+180.0, segPtr->u.j.negate );
				a = NormalizeAngle( a-180.0 );
			} else {
				GetJointPos( NULL, &a, l, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.pos, segPtr->u.j.angle, segPtr->u.j.negate );
			}
		}
		a = NormalizeAngle( data->traverse1.angle+a );
		data->traverse1.backwards = (a < 270 && a > 90 );
		if ( !segPtr->u.j.Scurve ) {
			if ( data->traverse1.backwards==0 )
				data->traverse1.dist = JoinD( l, segPtr->u.j.R, segPtr->u.j.L ) - JoinD( segPtr->u.j.l0, segPtr->u.j.R, segPtr->u.j.L );
			else
				data->traverse1.dist = JoinD( segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L ) - JoinD( l, segPtr->u.j.R, segPtr->u.j.L );
		} else {
			data->traverse1.backwards = !data->traverse1.backwards;
			if ( data->traverse1.backwards==0 )
				data->traverse1.dist = JoinD( segPtr->u.j.l0, segPtr->u.j.R, segPtr->u.j.L ) - JoinD( l, segPtr->u.j.R, segPtr->u.j.L );
			else
				data->traverse1.dist = JoinD( segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L ) + JoinD( l, segPtr->u.j.R, segPtr->u.j.L );
		}
		if ( segPtr->u.j.flip )
			data->traverse1.backwards = !data->traverse1.backwards;
LOG( log_traverseJoint, 1, ( "TJ0: ?[%0.3f %0.3f] A=%0.3f l=%0.3f J[%0.3f %0.3f] A=%0.3f l0=%0.3f l1=%0.3f R=%0.3f L=%0.3f N:%d F:%d S:%d = a=%0.3f D=%0.3f B=%d\n",
		data->traverse1.pos.x, data->traverse1.pos.y, data->traverse1.angle,
		l,
		segPtr->u.j.pos.x, segPtr->u.j.pos.y, segPtr->u.j.angle,
		segPtr->u.j.l0, segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L,
		segPtr->u.j.negate, segPtr->u.j.flip, segPtr->u.j.Scurve,
		a, data->traverse1.dist, data->traverse1.backwards ) );
		break;

	case SEGPROC_TRAVERSE2:
		flip = segPtr->u.j.flip;
		if (data->traverse2.segDir!=0)
			flip = !flip;
		if (segPtr->u.j.Scurve)
			flip = !flip;
		data->traverse2.pos = GetSegEndPt( segPtr, data->traverse2.segDir, FALSE, NULL );
		TraverseJoint( &data->traverse2.pos, &data->traverse2.angle, &data->traverse2.dist, segPtr->u.j.pos, segPtr->u.j.angle, segPtr->u.j.l0, segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.negate, flip, segPtr->u.j.Scurve );
		break;

	case SEGPROC_DRAWROADBEDSIDE:
		/* TODO: JointSegProc( SEGPROC_DRAWROADBEDSIDE, ...  */
		break;

	case SEGPROC_DISTANCE:
		data->distance.dd = JointDistance( &data->distance.pos1, segPtr->u.j.pos, segPtr->u.j.angle, segPtr->u.j.l0, segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.negate, segPtr->u.j.Scurve );
		break;

	case SEGPROC_FLIP:
		segPtr->u.j.flip = !segPtr->u.j.flip;
		break;

	case SEGPROC_NEWTRACK:
		data->newTrack.trk = NewTrack( 0, T_EASEMENT, 2, sizeof *xx );
		xx = GetTrkExtraData(data->newTrack.trk);
		xx->pos = segPtr->u.j.pos;
		xx->angle = segPtr->u.j.angle;
		xx->l0 = segPtr->u.j.l0;
		xx->l1 = segPtr->u.j.l1;
		xx->R = segPtr->u.j.R;
		xx->L = segPtr->u.j.L;
		xx->negate = segPtr->u.j.negate;
		xx->flip = segPtr->u.j.flip;
		xx->Scurve = segPtr->u.j.Scurve;
		ep0 = 0;
		if ( xx->flip )
			ep0 = 1-ep0;
		if ( xx->Scurve )
			ep0 = 1-ep0;
		GetJointPos( &p, &a, xx->l0, xx->R, xx->L, xx->pos, xx->angle, xx->negate );
		if ( !xx->Scurve )
			a = NormalizeAngle(a+180.0);
		SetTrkEndPoint( data->newTrack.trk, ep0, p, a );
		a = xx->angle;
		if ( xx->Scurve )
			a = NormalizeAngle(a+180.0);
		GetJointPos( &p, &a, xx->l1, xx->R, xx->L, xx->pos, a, xx->negate );
		if ( xx->Scurve )
			a = NormalizeAngle(a+180.0);
		SetTrkEndPoint( data->newTrack.trk, 1-ep0, p, a );
		ComputeBoundingBox( data->newTrack.trk );
		data->newTrack.ep[0] = 0;
		data->newTrack.ep[1] = 1;
		break;

	case SEGPROC_LENGTH:
		if ( !segPtr->u.j.Scurve )
			data->length.length = JoinD( segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L ) - JoinD( segPtr->u.j.l0, segPtr->u.j.R, segPtr->u.j.L );
		else
			data->length.length = JoinD( segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L ) + JoinD( segPtr->u.j.l0, segPtr->u.j.R, segPtr->u.j.L );
		break;

	case SEGPROC_SPLIT:
		xxx[0].pos = segPtr->u.j.pos;
		xxx[0].angle = segPtr->u.j.angle;
		xxx[0].l0 = segPtr->u.j.l0;
		xxx[0].l1 = segPtr->u.j.l1;
		xxx[0].R = segPtr->u.j.R;
		xxx[0].L = segPtr->u.j.L;
		xxx[0].negate = segPtr->u.j.negate;
		xxx[0].flip = segPtr->u.j.flip;
		xxx[0].Scurve = segPtr->u.j.Scurve;
		SplitJointA( &data->split.pos, 0, &xxx[0], &xxx[1], &a );
		for ( inx=0; inx<2; inx++ ) {
			xx = &xxx[(!segPtr->u.j.flip)?1-inx:inx];
			data->split.newSeg[inx] = *segPtr;
			data->split.newSeg[inx].u.j.pos = xx->pos;
			data->split.newSeg[inx].u.j.angle = xx->angle;
			data->split.newSeg[inx].u.j.l0 = xx->l0;
			data->split.newSeg[inx].u.j.l1 = xx->l1;
			data->split.newSeg[inx].u.j.R = xx->R;
			data->split.newSeg[inx].u.j.L = xx->L;
			data->split.newSeg[inx].u.j.negate = xx->negate;
			data->split.newSeg[inx].u.j.flip = xx->flip;
			data->split.newSeg[inx].u.j.Scurve = xx->Scurve;
			if ( !xx->Scurve )
				data->split.length[inx] = JoinD( xx->l1, xx->R, xx->L ) - JoinD( xx->l0, xx->R, xx->L );
			else
				data->split.length[inx] = JoinD( xx->l1, xx->R, xx->L ) + JoinD( xx->l0, xx->R, xx->L );
		}
		break;

	case SEGPROC_GETANGLE:
		GetLandD( &l, NULL, data->getAngle.pos, segPtr->u.j.pos, segPtr->u.j.angle, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.negate, segPtr->u.j.Scurve );
		if (small(l)) {
			a = segPtr->u.j.angle;
		} else {
			if (segPtr->u.j.Scurve && l < 0.0) {
				GetJointPos( NULL, &a, -l, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.pos, segPtr->u.j.angle+180.0, segPtr->u.j.negate );
				a = NormalizeAngle( a-180.0 );
			} else {
				GetJointPos( NULL, &a, l, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.pos, segPtr->u.j.angle, segPtr->u.j.negate );
			}
		}
		data->getAngle.angle = a;
		break;
	}
}



#ifndef TEST
BOOL_T JoinTracks(
		track_p trk0,
		EPINX_T ep0,
		coOrd pos0,
		track_p trk1,
		EPINX_T ep1,
		coOrd pos1,
		easementData_t * e )
/*
 * Join 2 tracks with joint described in (e).
 * (pos0) and (pos1) are points that would be connected if there was no
 * transition-curve.
 * If there is then:
 *      (pos0) and (pos1) have been moved (x) apart.
 *      Adjust the endPoints by moving (pos0) and (pos1) by (e->d0) and (e->d1)
 *      along the track.
 * Connect the tracks.
 */
{
	track_p joint;

LOG( log_ease, 1, ( "join T%d[%d] @[%0.3f %0.3f], T%d[%d] @[%0.3f %0.3f]\n",
				GetTrkIndex(trk0), ep0, pos0.x, pos0.y, GetTrkIndex(trk1), ep1, pos1.x, pos1.y ) )

	if ( GetTrkType(trk0) == T_EASEMENT ) {
		DIST_T d;
		ANGLE_T aa;
		d = FindDistance( GetTrkEndPos(trk0,ep0), GetTrkEndPos(trk1,ep1) );
		aa = NormalizeAngle( GetTrkEndAngle(trk0,ep0) - GetTrkEndAngle(trk1,ep1) + 180.0 + connectAngle/2.0 );
		if ( d <= connectDistance && aa <= connectAngle ) {
			ConnectTracks( trk0, ep0, trk1, ep1 );
		}
		return TRUE;
	}

	/* Move the endPoint for (trk0) */
	if (!MoveEndPt( &trk0, &ep0, pos0, e->d0 ))
		return FALSE;

	/* Move the endPoint for (trk1) */
	if (!MoveEndPt( &trk1, &ep1, pos1, e->d1 ))
		return FALSE;

LOG( log_ease, 1, ( "   EASE R%0.3f..%0.3f L%0.3f..%0.3f\n",
				e->r0, e->r1, e->d0, e->d1 ) )

	/* Connect the tracks */
	if (e->x == 0.0) {
		/* No transition-curve */
		ConnectTracks( trk0, ep0, trk1, ep1 );
	} else {
		/* Connect with transition-curve */
		joint = NewJoint( GetTrkEndPos(trk0,ep0), GetTrkEndAngle(trk0,ep0),
						 GetTrkEndPos(trk1,ep1), GetTrkEndAngle(trk1,ep1),
						 GetTrkGauge(trk0), easeR, easeL, e );
		CopyAttributes( trk0, joint );
		ConnectTracks( trk1, ep1, joint, 1 );
		ConnectTracks( trk0, ep0, joint, 0 );
		DrawNewTrack( joint );
	}
	return TRUE;
}


EXPORT void UndoJoint(
		track_p trk,
		EPINX_T ep,
		track_p trk1,
		EPINX_T ep1 )
{
	struct extraData * xx;
	DIST_T d;

	if ( GetTrkType(trk1) != T_EASEMENT ) 
		return;
	xx = GetTrkExtraData(trk1);
	if ( ep1 == 0 )
		d = xx->L/2.0 - xx->l0;
	else
		d = xx->l1 - xx->L/2.0;
	if ( d < 0.01 )
		return;
	UndrawNewTrack( trk );
	MoveEndPt( &trk, &ep, GetTrkEndPos(trk,ep), -d );
	DrawNewTrack( trk );
}
#endif

/*****************************************************************************
 *
 * INITIALIZATION
 *
 */



void InitTrkEase( void )
{
	T_EASEMENT = InitObject( &easementCmds );
	log_ease = LogFindIndex( "ease" );
	log_traverseJoint = LogFindIndex( "traverseJoint" );
}


/*****************************************************************************
 *
 * TEST
 *
 */

#ifdef TEST


void ErrorMessage( char * msg, ... )
{
	lprintf( "%s\n", msg );
}

void InfoMessage( char * msg, ... )
{
	lprintf( "%s\n", msg );
}

scaleInfo_p curScale;

track_p NewTrack( TRKINX_T a, TRKTYP_T b, EPINX_T c, TRKTYP_T d )
{
	return NULL;
}

void DrawStraightTrack( drawCmd_p a, coOrd b, coOrd c, ANGLE_T d,
						DIST_T trackGauge, wDrawColor color, int opts )
{
}

void DrawNewTrack( track_p t )
{
}

static DIST_T JoinDalt(
		DIST_T x,
		DIST_T R,
		DIST_T L )
/*
 * Alternative distance computation, integrate over the curve.
 */
{
#define DCNT (1000)
	DIST_T d;
	wIndex_t i;
	coOrd p0, p1;
	d = 0.0;
	p0.x = p0.y = 0.0;
	for ( i=1;i<=DCNT; i++) {
		ComputeJoinPos( x*((DIST_T)i)/((DIST_T)DCNT), R, L, NULL, NULL, &p1, NULL );
		d += FindDistance( p0, p1 );
		p0 = p1;
	}
	return d;
}


test_plot( INT_T argc, char * argv[] )
{
	DIST_T l, X, L, rr, ra, d, d1, R;
	coOrd p, pc, p1;
	INT_T i, C;
	if (argc != 4) {
		lprintf("%s R L C\n", argv[0]);
		Exit(1);
	}
	argv++;
	R = atof( *argv++ );
	L = atof( *argv++ );
	C = atol( *argv++ );
	X = L*L/(24*R);
	lprintf("R=%0.3f X=%0.3f L=%0.3f\n", R, X, L );
	
	for (i=0;i<=C;i++) {
		l = L*((DIST_T)i)/((DIST_T)C);
		d = JoinD( l, R, L );
		d1 = JoinDalt( l, R, L );
		ComputeJoinPos( l, R, L, &rr, &ra, &p, &pc );
		lprintf("d: [%0.3f %0.3f] [%0.3f %03f] R=%0.3f A=%0.3f D=%0.3f D1=%0.3f X=%0.4f\n",
		i, p.x, p.y, pc.x, pc.y, rr, ra, d, d1, pc.x-rr );
	}
}

test_psplot( INT_T argc, char * argv[] )
{
	DIST_T l, L, rr, ra, d, d1, R, S, X;
	coOrd p, q, pc, p1;
	INT_T i, C;
	if (argc != 5) {
		lprintf("%s R L C S\n", argv[0]);
		Exit(1);
	}
	argv++;
	easeR = R = atof( *argv++ );
	easeL = L = atof( *argv++ );
	C = atol( *argv++ );
	S = atof( *argv++ );
	X = L*L/(24*R);
	
lprintf("%%! kvjfv\nsave\n0 setlinewidth\n");
lprintf("/Times-BoldItalic findfont 16 scalefont setfont\n");
lprintf("36 36 moveto (R=%0.3f X=%0.3f L=%0.3f S=%0.3f) show\n", easeR, X, L, S );
/*lprintf("24 768 translate -90 rotate\n");*/
lprintf("gsave\n72 72 translate\n");
lprintf("%0.3f %0.3f scale\n", 72.0/S, 72.0/S );
lprintf("%0.3f %0.3f moveto %0.3f %0.3f lineto stroke\n", 0.0, 0.0, L, 0.0 );
lprintf("%0.3f %0.3f %0.3f 270.0 90.0 arc stroke\n", L/2.0, easeR+X, easeR );
lprintf("%0.3f %0.3f %0.3f 0.0 360.0 arc stroke\n", 0.0, 0.0, 0.25 );
	q.x = q.y = 0.0;
	for (i=0;i<=C;i++) {
		l = L*((DIST_T)i)/((DIST_T)C);
		ComputeJoinPos( l, R, L, &rr, &ra, &p, &pc );
lprintf("%0.3f %0.3f moveto %0.3f %0.3f lineto stroke\n", q.x, q.y, p.x, p.y );
		q = p;
	}
lprintf("%0.3f %0.3f %0.3f 0.0 360.0 arc stroke\n", p.x, p.y, 0.25 );
lprintf("grestore\nrestore\nshowpage\n%%Trailer\n%%Pages: 1\n");
}

void Test_compute( INT_T argc, char * argv[] )
{
	DIST_T r0, r1, x, l0, l1, R, X, d;
	coOrd q0, q1, qc0, qc1;
	easementData_t e;
	if (argc != 5) {
		lprintf("compute R0 R1 R L\n");
		Exit(1);
	}
	/*debugEase = 5;*/
	argv++;
	r0 = atof( *argv++);
	r1 = atof( *argv++);
	easementVal = 1.0;
	easeR = atof( *argv++);
	easeL = atof( *argv++);
	ComputeJoint( r0, r1, &e );
	ComputeJoinPos( e.l0, easeR, easeL, NULL, NULL, &q0, &qc0 );
	ComputeJoinPos( e.l1, easeR, easeL, NULL, NULL, &q1, &qc1 );
	if (e.Scurve) {
		q1.x = - q1.x; q1.y = - q1.y;
		qc1.x = - qc1.x; qc1.y = - qc1.y;
	}
	d = FindDistance( q0, q1 );
	lprintf("ENDPT  [%0.3f %0.3f] [%0.3f %0.3f]\n", q0.x, q0.y, q1.x, q1.y );
	lprintf("CENTER [%0.3f %0.3f] [%0.3f %0.3f]\n", qc0.x, qc0.y, qc1.x, qc1.y );
	lprintf("ComputeJoint( %0.3f %0.3f) { %0.3f %0.3f %0.3f } D0=%0.5f D1=%0.5f, D=%0.3f\n",
				r0, r1, easeR, easeL, e.x, e.d0, e.d1, d );
}

void Test_findL( INT_T argc, char * argv[] )
{
	DIST_T l, r, R, L;
	if (argc != 5) {
		lprintf("findL r R L\n");
		Exit(1);
	}
	/*debugEase = 5;*/
	argv++;
	r = atof( *argv++ );
	R = atof( *argv++ );
	L = atof( *argv++ );
	l = FindL( r, R, L );
	lprintf("FindL( %0.3f %0.3f %0.3f ) = %0.3f\n", r, R, L, l );
}


main( INT_T argc, char * argv[] )
{
INT_T flagX = 0;
INT_T flagV = 0;
	if (argc<1) {
		lprintf("plot|compute\n");
		Exit(1);
	}
	argv++; argc--;
	while (argv[0][0] == '-') {
		switch (argv[0][1]) {
		case 'x':
			flagX++;
			argc--;argv++;
			break;
		case 'v':
			flagV++;
			argc--;argv++;
			break;
		default:
			lprintf("Huh: %s\n", *argv );
			argc--;argv++;
			break;
		}
	}
	if (strcmp(argv[0],"plot")==0) {
		Test_plot( argc, argv );
	} else if (strcmp(argv[0],"psplot")==0) {
		Test_psplot( argc, argv );
	} else if (strcmp(argv[0],"compute")==0) {
		Test_compute( argc, argv );
	} else if (strcmp(argv[0],"findL")==0) {
		Test_findL( argc, argv );
	} else {
		lprintf("unknown cmd %s\n", argv[0] );
	}
}
#endif
