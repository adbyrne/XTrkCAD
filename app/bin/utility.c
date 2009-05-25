/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/utility.c,v 1.2 2009-05-25 18:11:03 m_fischer Exp $
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

#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#endif
#include <math.h>
#include "common.h"
#include "utility.h"

/*****************************************************************************
 *
 * VARIABLES
 *
 */

double radiusGranularity = 1.0/8.0;
DEBUGF_T debugIntersection = 0;

#define CLOSE (1.0)

/*****************************************************************************
 *
 * UTLITY FUNCTIONS
 *
 */



#ifndef min
double max( double a, double b )
{
	if (a>b) return a;
	return b;
}



double min( double a, double b )
{
	if (a<b) return a;
	return b;
}
#endif



double FindDistance( coOrd p0, coOrd p1 )
{
	double dx = p1.x-p0.x, dy = p1.y-p0.y;
	return sqrt( dx*dx + dy*dy );
}



double NormalizeAngle( double a )
{
	while (a<0.0) a += 360.0;
	while (a>=360.0) a -= 360.0;
	if ( a > 360.0-EPSILON ) a = 0.0;
	return a;
}



int IsAligned( double a1, double a2 )
{
	a1 = NormalizeAngle( a1 - a2 + 90.0 );
	return ( a1 < 180 );
}


double D2R( double D )
{
	D = NormalizeAngle(D);
	if (D >= 180.0) D = D - 360.0;
	return D * (M_PI*2) / 360.0;
}



double R2D( double R )
{
	return NormalizeAngle( R * 360.0 / (M_PI*2) );
}



void Rotate( coOrd *p, coOrd orig, double angle )
{
	double x=p->x,y=p->y;
	x -= orig.x;
	y -= orig.y;
	p->x = (POS_T)(x * cos(D2R(angle)) + y * sin(D2R(angle)));
	p->y = (POS_T)(y * cos(D2R(angle)) - x * sin(D2R(angle)));
	p->x += orig.x;
	p->y += orig.y;
}


/**
 * Translate coordinates.
 *
 * \param res OUT new (translated) position
 * \param orig IN old position
 * \param a IN angle
 * \param d IN distance
 */

void Translate( coOrd *res, coOrd orig, double a, double d )
{
	res->x = orig.x + (POS_T)(d * sin( D2R(a)) );
	res->y = orig.y + (POS_T)(d * cos( D2R(a)) );
}



double FindAngle( coOrd p0, coOrd p1 )
{
	double dx = p1.x-p0.x, dy = p1.y-p0.y;
	if (small(dx)) {
		if (dy >=0) return 0.0;
		else return 180.0;
	}
	if (small(dy)) {
		if (dx >=0) return 90.0;
		else return 270.0;
	}
	return R2D(atan2( dx,dy ));
}



BOOL_T PointOnCircle( coOrd * resP, coOrd center, double radius, double angle )
{
	double r;
	r = sin(D2R(angle));
	r = radius * r;
	resP->x = center.x + (POS_T)(radius * sin(D2R(angle)));
	resP->y = center.y + (POS_T)(radius * cos(D2R(angle)));
	return 1;
}



double ConstrainR( double r )
{
	double ret;
	ret = r / radiusGranularity;
	ret = floor( ret + 0.5 );
	ret = ret * radiusGranularity;
	return ret;
}




void FindPos( coOrd * res, double * beyond, coOrd pos, coOrd orig, double angle, double length )
{
	double a0, a1;
	double d;
#ifdef __linux
	static volatile double x;
#else
	double x;
#endif
	a0 = FindAngle( orig, pos );
	a1 = NormalizeAngle( a0 - angle );
	d = FindDistance( orig, pos );
	x = d * cos( D2R( a1 ) );
	if ( x < 0.0 ) {
		res->x = (POS_T)0.0;
	} else if (x > length) {
		res->x = (POS_T)length;
	} else {
		res->x = (POS_T)x;
	}
	if (beyond) *beyond = x - res->x;
	res->y = (POS_T)(d * sin( D2R( a1 )) );
}



/* Find intersection:
   Given 2 lines each described by a point and angle (P0,A0) (P1,A1)
   there exists a common point PC.
	d0x = sin(A0)
	d0y = cos(A0)
	d1x = sin(A1)
	d1y = cos(A1)
	Pc.x = P0.x + N0 * d0x
	Pc.y = P0.y + N0 * d0y
	Pc.x = P1.x + N1 * d1x
	Pc.y = P1.y + N1 * d1y

   Combining:
(1) Pc.x = P0.x + N0 * d0x = P1.x + N1 * d1y
(2) Pc.y = P0.y + N0 * d0y = P1.y + N1 * d1y

   Solve Pc.y for N0:
	P0.y + N0 * d0y = P1.y + N1 * d1y
	N0 * d0y = P1.y + N1 * d1y - P0.y
	N0 = (P1.y + N1 * d1y - P0.y) / d0y
(3) N0 = (P1.y - P0.y + N1 * d1y) / d0y

   Solve Pc.x for N1:
	P0.x + N0 * d0x = P1.x + N1 * d1x
	P0.x + N0 * d0x - P1.x = N1 * d1x
	(P0.x + N0 * d0x - P1.x) / d1x = N1
(4) (P0.x - P1.x + N0 * d0x) / d1x = N1

   Substitute (3) into (4):
		  (P0.x - P1.x + [(P1.y - P0.y + N1 * d1y) / d0y ] * d0x)
	N1 =  -----------------------------------------------------------
									   d1x
	Regroup:
		  (P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x   [ N1 * d1y / d0y ] * d0x
	N1 =  -------------------------------------------- + ------------------------
							   d1x                                  d1x

		  (P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x   N1 * (d1y * d0x / d0y)
	N1 =  -------------------------------------------- + ------------------------
							   d1x                                  d1x

		  (P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x        (d1y * d0x / d0y)
	N1 =  -------------------------------------------- + N1 * --------------------
							   d1x                                   d1x

			   (d1y * d0x / d0y)     (P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x
	N1 * ( 1 - ----------------- ) = --------------------------------------------
					  d1x                             d1x

		 (P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x
		 --------------------------------------------
						  d1x
	N1 = ============================================
				 d1y * d0x / d0y
			 1 - ---------------
					   d1x

		 (P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x
		 --------------------------------------------
						  d1x
	N1 = ============================================
			 d1x - d1y * d0x / d0y
			 ---------------------
					 d1x

   d1x cancel
		(P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x
   N1 = ============================================
				   d1x - d1y * d0x / d0y

		(P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x
   N1 = ============================================
				   d1x*d0y - d1y*d0x
				   -------------------
						  d0y

   Bring up d0y:
		{ ((P0.x - P1.x + [(P1.y - P0.y)/d0y] * d0x } * d0y
   N1 = =======================================================
				   d1x*d0y - d1y*d0x

   Distribute and cancel:
		(P0.x - P1.x) * d0y + (P1.y - P0.y) * d0x
   N1 = =============================================
				   d1x*d0y - d1y*d0x

   if (d1x*d0y - d1y*d0x) = 0 then lines are parallel
*/          

BOOL_T FindIntersection( coOrd *Pc, coOrd P0, double A0, coOrd P1, double A1 )
{
	double dx0, dy0, dx1, dy1, N1;
	double d;

#ifndef WINDOWS
	if (debugIntersection >= 3)
		printf("FindIntersection( [%0.3f %0.3f] A%0.3f  [%0.3f %0.3f] A%0.3f\n",
				P0.x, P0.y, A0, P1.x, P1.y, A1 );
#endif

	dx0 = sin( D2R( A0 ) );
	dy0 = cos( D2R( A0 ) );
	dx1 = sin( D2R( A1 ) );
	dy1 = cos( D2R( A1 ) );
	d = dx1 * dy0 - dx0 * dy1;
	if (d < EPSILON && d > -EPSILON) {
#ifndef WINDOWS
		if (debugIntersection >=3 ) printf("dx1 * dy0 - dx0 * dy1 = %0.3f\n", d );
#endif
		return FALSE;
	}
/*
 *       (P0.x - P1.x) * d0y + (P1.y - P0.y) * d0x
 *  N1 = =============================================
 *                  d1x*d0y - d1y*d0x
 */
	N1 = dy0 * (P0.x - P1.x) + dx0 * (P1.y - P0.y );
	N1 = N1 / d;
	Pc->x = P1.x + (POS_T)(N1*dx1);
	Pc->y = P1.y + (POS_T)(N1*dy1);
#ifndef WINDOWS
	if (debugIntersection >=3 ) printf( "     [%0.3f,%0.3f]\n", Pc->x, Pc->y );
#endif
	return TRUE;
}


EPINX_T PickArcEndPt( coOrd pc, coOrd p0, coOrd p1 )
{
	double a;
	a = NormalizeAngle( FindAngle( pc, p1 ) - FindAngle( pc, p0 ) );
	if (a > 180.0)
		return 0;
	else
		return 1;
}

EPINX_T PickLineEndPt( coOrd p0, double a0, coOrd p1 )
{
	double a;
	a = NormalizeAngle( FindAngle( p0, p1 ) - a0 );
	if (a < 90.0 || a > 270 )
		return 0;
	else
		return 1;
}

double LineDistance( coOrd *p, coOrd p0, coOrd p1 )
{
	double d, a;
	coOrd pp, zero;
	zero.x = zero.y = (POS_T)0.0;
	d = FindDistance( p0, p1 );
	a = FindAngle( p0, p1 );
	pp.x = p->x-p0.x;
	pp.y = p->y-p0.y;
	Rotate( &pp, zero, -a );
	if (pp.y < 0.0-EPSILON) {
		d = FindDistance( p0, *p );
		*p = p0;
		return d;
	} else if (pp.y > d+EPSILON ) {
		d = FindDistance( p1, *p );
		*p = p1;
		return d;
	} else {
		p->x = p0.x + (POS_T)(pp.y*sin(D2R(a)));
		p->y = p0.y + (POS_T)(pp.y*cos(D2R(a)));
		return pp.x>=0? pp.x : -pp.x;
	}
}



double CircleDistance( coOrd *p, coOrd c, double r, double a0, double a1 )
{
	double d;
	double a,aa;
	coOrd pEnd;
	d = FindDistance( c, *p );
	a = FindAngle( c, *p );
	aa = NormalizeAngle( a - a0 );
	if (a1 >= 360.0 || aa <= a1) {
		d = fabs(d-r);
		PointOnCircle( p, c, r, a );
	} else {
		if ( aa < a1+(360.0-a1)/2.0 ) {
			PointOnCircle( &pEnd, c, r, a0+a1 );
		} else {
			PointOnCircle( &pEnd, c, r, a0 );
		}
		d = FindDistance( *p, pEnd );
		*p = pEnd;
	}
	return d;
}



coOrd AddCoOrd( coOrd p0, coOrd p1, double a )
{
	coOrd res, zero;
	zero.x = zero.y = (POS_T)0.0;
	Rotate(&p1, zero, a );
	res.x = p0.x + p1.x;
	res.y = p0.y + p1.y;
	return res;
}

BOOL_T InRect( coOrd pos, coOrd rect )
{
	if (pos.x >= 0.0 && pos.x <= rect.x && pos.y >= 0.0 && pos.y <= rect.y)
		return 1;
	else
		return 0;
}


static BOOL_T IntersectLine( POS_T *fx0, POS_T *fy0, POS_T x1, POS_T y1, POS_T x, POS_T y )
{
	POS_T x0=*fx0, y0=*fy0, dx, dy;
	BOOL_T rc;
#ifdef TEST
	printf("        IntersectLine( P0=[%0.2f %0.2f] P1=[%0.2f %0.2f] X=%0.2f Y=%0.2f\n",
		x0, y0, x1, y1, x, y );
#endif
	dx = x1-x0;
	dy = y1-y0;
	if (dy==0.0) {
		if (y0 == y)
			rc = TRUE;
		else
			rc = FALSE;
	} else {
		x0 += (y-y0) * dx/dy;
		if (x0 < -EPSILON || x0 > x) {
			rc = FALSE;
		} else {
			*fx0 = x0;
			*fy0 = y;
			rc = TRUE;
		}
	}
#ifdef TEST
	if (rc)
		printf("        = TRUE [%0.2f %0.2f]\n", *fx0, *fy0 );
	else
		printf("        = FALSE\n");
#endif
	return rc;
}

/*
 * intersectBox - find point on box boundary ([0,0],[size]) where
 *                line from p0 (interior) to p1 (exterior) intersects
 */
static void IntersectBox( coOrd *p1, coOrd p0, coOrd size, int x1, int y1 )
{
#ifdef TEST
	printf("    IntersectBox( P1=[%0.2f %0.2f] P0=[%0.2f %0.2f] S=[%0.2f %0.2f] X1=%d Y1=%d\n",
		p1->x, p1->y, p0.x, p0.y, size.x, size.y, x1, y1 );
#endif
	if ( y1!=0 &&
		 IntersectLine( &p1->x, &p1->y, p0.x, p0.y, size.x, (y1==-1?(POS_T)0.0:size.y) ))
		return;
	else if ( x1!=0 &&
		 IntersectLine( &p1->y, &p1->x, p0.y, p0.x, size.y, (x1==-1?(POS_T)0.0:size.x) ))
		return;
#ifndef WINDOWS
	else
		fprintf(stderr, "intersectBox bogus\n" );
#endif
}

BOOL_T ClipLine( coOrd *fp0, coOrd *fp1, coOrd orig, double angle, coOrd size )
{
	coOrd p0 = *fp0, p1 = * fp1;
	int x0, y0, x1, y1;

#ifdef TEST
	printf("ClipLine( P0=[%0.2f %0.2f] P1=[%0.2f %0.2f] O=[%0.2f %0.2f] A=%0.2f S=[%0.2f %0.2f]\n",
		p0.x, p0.y, p1.x, p1.y, orig.x, orig.y, angle, size.x, size.y );
#endif

	Rotate( &p0, orig, -angle );
	Rotate( &p1, orig, -angle );
	p0.x -= orig.x; p0.y -= orig.y;
	p1.x -= orig.x; p1.y -= orig.y;

	/* categorize point as to sector:
		-1,1     |    0,1      |     1,1
	 ------------+-------------S----------
		-1,0     |    0,0      |     1,0
	 ------------O-------------+----------
		-1,-1    |    0,-1     +     1,-1
	*/
	if ( p0.x < 0.0-EPSILON )         x0 = -1;
	else if ( p0.x > size.x+EPSILON ) x0 = 1;
	else                              x0 = 0;
	if ( p0.y < 0.0-EPSILON )         y0 = -1;
	else if ( p0.y > size.y+EPSILON ) y0 = 1;
	else                              y0 = 0;
	if ( p1.x < 0.0-EPSILON )         x1 = -1;
	else if ( p1.x > size.x+EPSILON ) x1 = 1;
	else                              x1 = 0;
	if ( p1.y < 0.0-EPSILON )         y1 = -1;
	else if ( p1.y > size.y+EPSILON ) y1 = 1;
	else                              y1 = 0;

#ifdef TEST
	printf("  X0=%d Y0=%d X1=%d Y1=%d\n", x0, y0, x1, y1 );
#endif

	/* simple cases: one or both points within box */
	if ( x0==0 && y0==0 ) {
		if ( x1==0 && y1==0 ) {
			/* both within box */
			return 1;
		}
		/* p0 within, p1 without */
		IntersectBox( &p1, p0, size, x1, y1 );
		p1.x += orig.x; p1.y += orig.y;
		Rotate( &p1, orig, angle );
		*fp1 = p1;
		return 1;
	}

	if ( x1==0 && y1==0 ) {
		/* p1 within, p0 without */
		IntersectBox( &p0, p1, size, x0, y0 );
		p0.x += orig.x; p0.y += orig.y;
		Rotate( &p0, orig, angle );
		*fp0 = p0;
		return 1;
	}

	/* both points without box and cannot intersect */
	if ( (x0==x1 && y0==y1) || /* within same sector (but not the middle one) */
		 (x0!=0 && x0==x1) ||  /* both right or left */
		 (y0!=0 && y0==y1) )   /* both above or below */
		return 0;

#ifdef TEST
	printf("  complex intersection\n");
#endif

	/* possible intersection */
	if ( y0!=0 &&
		 IntersectLine( &p0.x, &p0.y, p1.x, p1.y, size.x, (y0==-1?(POS_T)0.0:size.y) ))
		IntersectBox( &p1, p0, size, x1, y1 );
	else if ( y1!=0 &&
		 IntersectLine( &p1.x, &p1.y, p0.x, p0.y, size.x, (y1==-1?(POS_T)0.0:size.y) ))
		IntersectBox( &p0, p1, size, x0, y0 );
	else if ( x0!=0 &&
		 IntersectLine( &p0.y, &p0.x, p1.y, p1.x, size.y, (x0==-1?(POS_T)0.0:size.x) ))
		IntersectBox( &p1, p0, size, x1, y1 );
	else if ( x1!=0 &&
		 IntersectLine( &p1.y, &p1.x, p0.y, p0.x, size.y, (x1==-1?(POS_T)0.0:size.x) ))
		IntersectBox( &p0, p1, size, x0, y0 );
	else {
		return 0;
	}
	p0.x += orig.x; p0.y += orig.y;
	p1.x += orig.x; p1.y += orig.y;
	Rotate( &p0, orig, angle );
	Rotate( &p1, orig, angle );
	*fp0 = p0;
	*fp1 = p1;
	return 1;
}

#ifdef LATER
BOOL_T ClipArc( double a0, double a1, coOrd pos, double radius, coOrd orig, double angle, double size )
{
	i = -1;
	state = unknown;
	if (pos.y + radius < 0.0 ||
		pos.y - radius > size.y ||
		pos.x + radius < 0.0 ||
		pos.x - radius > size.x )
		return 0;

	if (pos.y + radius <= size.y ||
		pos.y - radius >= 0.0 ||
		pos.x + radius <= size.x ||
		pos.x - radius >= 0.0 )
		return 1;

	if (pos.y + radius > size.y) {
		a = R2D(acos( (size.y-pos.y) / radius ) ));
		if (pos.x + radius*cos(R2D(a)) > size.x) {
			state = outside;
		} else {
			state = inside;
			i++;
			aa[i].a0 = a;
		}
	} else {
		state = inside;
		i++;
		aa[i].a0 = 0;
	}
}
#endif

#ifdef TEST
void Test( double p0x, double p0y, double p1x, double p1y, double origx, double origy, double angle, double sizex, double sizey )
{

	coOrd p0, p1, orig, size, p0a, p1a;
	BOOL_T rc;
	p0.x = p0x; p0.y = p0y; p1.x = p1x; p1.y = p1y;
	orig.x = origx; orig.y = origy;
	size.x = sizex; size.y = sizey;
	p0a = p0; p1a = p1;
	rc = ClipLine( &p0, &p1, orig, angle, size );
	printf("clipLine=%d P0=[%0.3f %0.3f] P1=[%0.3f %0.3f]\n", rc,
		p0.x, p0.y, p1.x, p1.y );
}

INT_T Main( INT_T argc, char *argv[] )
{
	double a[9];
	int i;
	if (argc != 10) {
		printf("usage: a x0 y0 x1 y1 xo yo a xs ys\n");
		Exit(1);
	}
	argv++;
	for (i=0;i<9;i++)
		a[i] = atof( *argv++ );

	Test( a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8] );
}
#endif
