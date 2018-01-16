/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/trkseg.c,v 1.2 2006-05-30 16:11:55 m_fischer Exp $
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

#include <track.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include "track.h"
#include <common.h>
#include <cbezier.h>
#include <string.h>

#include <tbezier.h>

#include "cjoin.h"
#include "fileio.h"
#include "param.h"
#include "track.h"
#include "utility.h"
#include "misc.h"


/*****************************************************************************
 *
 * TRACK SEGMENTS
 *
 * Notes: Segments are used
 * 1. as temporary elements during editing operations
 * 2. as a means of grouping primitives for compounds
 * 3. as the way of drawing and operating on Bezier curves
 *
 * They are stored as dynamic arrays which can be displayed and operated on as sets.
 *
 */


/*
 * Build a Segment that has a radius and passes through two points. This uses the knowledge
 * that the center of curve is always on an orthogonal line through the bisection of a chord.
 */
EXPORT void ComputeCurvedSeg(
		trkSeg_p s,
		DIST_T radius,
		coOrd p0,
		coOrd p1 )
{
	DIST_T d;
	ANGLE_T a, aa, aaa;
	s->u.c.radius = radius;
	d = FindDistance( p0, p1 )/2.0;
	a = FindAngle( p0, p1 );
	if (radius > 0) {
		aa = R2D(asin( d/radius ));
		aaa = a + (90.0 - aa);
		Translate( &s->u.c.center, p0, aaa, radius );
		s->u.c.a0 = NormalizeAngle( aaa + 180.0 );
		s->u.c.a1 = aa*2.0;
	} else {
		aa = R2D(asin( d/(-radius) ));
		aaa = a - (90.0 - aa);
		Translate( &s->u.c.center, p0, aaa, -radius );
		s->u.c.a0 = NormalizeAngle( aaa + 180.0 - aa *2.0 );
		s->u.c.a1 = aa*2.0;
	}
}


EXPORT coOrd GetSegEndPt(
		trkSeg_p segPtr,
		EPINX_T ep,
		BOOL_T bounds,
		ANGLE_T * angleR )
{
	coOrd pos;
	ANGLE_T angle, a, a0, a1 = 0.0;
	DIST_T r;
	POS_T x0, y0, x1, y1;

	switch (segPtr->type) {
	case SEG_STRTRK:
	case SEG_STRLIN:
	case SEG_DIMLIN:
	case SEG_BENCH:
	case SEG_TBLEDGE:
		pos = segPtr->u.l.pos[ep];
		angle = FindAngle( segPtr->u.l.pos[1-ep], segPtr->u.l.pos[ep] );
		break;
	case SEG_CRVLIN:
	case SEG_CRVTRK:
		a0 = segPtr->u.c.a0;
		a1 = segPtr->u.c.a1;
		r = fabs( segPtr->u.c.radius );
		a = a0;
		if ( (ep==1) == (segPtr->u.c.radius>0) ) {
			a += a1;
			angle = NormalizeAngle( a+90 );
		} else {
			angle = NormalizeAngle( a-90 );
		}
		if (bounds) {
			x0 = r * sin(D2R(a0));
			x1 = r * sin(D2R(a0+a1));
			y0 = r * cos(D2R(a0));
			y1 = r * cos(D2R(a0+a1));
			if (ep == 0) {
				pos.x = segPtr->u.c.center.x + (((a0<=270.0)&&(a0+a1>=270.0)) ?
						(-r) : min(x0,x1));
				pos.y = segPtr->u.c.center.y + (((a0<=180.0)&&(a0+a1>=180.0)) ?
						(-r) : min(y0,y1));
			} else {
				pos.x = segPtr->u.c.center.x + (((a0<= 90.0)&&(a0+a1>= 90.0)) ?
						(r) : max(x0,x1));
				pos.y = segPtr->u.c.center.y + ((a0+a1>=360.0) ?
						(r) : max(y0,y1));
			}
		} else {
			PointOnCircle( &pos, segPtr->u.c.center, fabs(segPtr->u.c.radius), a );
		}
		break;
	case SEG_JNTTRK:
		pos = GetJointSegEndPos( segPtr->u.j.pos, segPtr->u.j.angle, segPtr->u.j.l0, segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.negate, segPtr->u.j.flip, segPtr->u.j.Scurve, ep, &angle );
		break;
    case SEG_BEZTRK:
    case SEG_BEZLIN:
        if (ep ==1) pos = segPtr->u.b.pos[3];       //For Bezier, use the End Points of the overall curve
        else pos = segPtr->u.b.pos[0];
        break;
    default:
		AbortProg("GetSegCntPt(%c)", segPtr->type );
	}
	if ( angleR )
		*angleR = angle;
	return pos;
}

/**
 * Calculate the bounding box for a string.
 *
 * \param coOrd IN position of text
 * \param angle IN text angle
 * \param str IN the string
 * \param fs IN size of font
 * \param loR OUT bottom left corner
 * \param hiR OUT top right corner
 */
EXPORT void GetTextBounds(
		coOrd pos,
		ANGLE_T angle,
		char * str,
		FONTSIZE_T fs,
		coOrd * loR,
		coOrd * hiR )
{
	coOrd size;
	POS_T descent = 0.0;
	coOrd lo, hi;
	coOrd p[4];
	coOrd lastL;
	int i;

	//DrawTextSize2( &mainD, str, NULL, fs, FALSE, &size, &descent);
	DrawMultiLineTextSize(&mainD, str, NULL, fs, FALSE, &size, &lastL );
	// set up the corners of the rectangle
	p[0].x = p[3].x = 0.0;
	p[1].x = p[2].x = size.x;
	p[0].y = p[1].y = size.y-descent;
	p[2].y = p[3].y = -descent;         //"Descent" includes multiple lines

	lo = hi = zero;

	// rotate each point
	for ( i=0; i<4; i++ ) {
		Rotate( &p[i], zero, angle );
		if ( p[i].x < lo.x ) lo.x = p[i].x;
		if ( p[i].y < lo.y ) lo.y = p[i].y;
		if ( p[i].x > hi.x ) hi.x = p[i].x;
		if ( p[i].y > hi.y ) hi.y = p[i].y;
	}

	// now recaclulate the corners
	loR->x = pos.x + lo.x;
	loR->y = pos.y + lo.y;
	hiR->x = pos.x + hi.x;
	hiR->y = pos.y + hi.y;
}


static void Get1SegBounds( trkSeg_p segPtr, coOrd xlat, ANGLE_T angle, coOrd *lo, coOrd *hi )
{
	int inx;
	coOrd p0, p1, pBez[4], pc;
	ANGLE_T a0, a1;
	coOrd width;
	DIST_T radius;

	width = zero;
	switch ( segPtr->type ) {
	case ' ':
		return;
	case SEG_STRTRK:
	case SEG_CRVTRK:
	case SEG_STRLIN:
	case SEG_DIMLIN:
	case SEG_BENCH:
	case SEG_TBLEDGE:
	case SEG_CRVLIN:
	case SEG_JNTTRK:
			REORIGIN( p0, GetSegEndPt( segPtr, 0, FALSE, NULL ), angle, xlat )
			REORIGIN( p1, GetSegEndPt( segPtr, 1, FALSE, NULL ), angle, xlat )
			if (p0.x < p1.x) {
				lo->x = p0.x;
				hi->x = p1.x;
			} else {
				lo->x = p1.x;
				hi->x = p0.x;
			}
			if (p0.y < p1.y) {
				lo->y = p0.y;
				hi->y = p1.y;
			} else {
				lo->y = p1.y;
				hi->y = p0.y;
			}
			if ( segPtr->type == SEG_CRVTRK ||
				 segPtr->type == SEG_CRVLIN ) {
				REORIGIN( pc, segPtr->u.c.center, angle, xlat );
				a0 = NormalizeAngle( segPtr->u.c.a0 + angle );
				a1 = segPtr->u.c.a1;
				radius = fabs(segPtr->u.c.radius);
				if ( a1 >= 360.0 ) {
					lo->x = pc.x - radius;
					lo->y = pc.y - radius;
					hi->x = pc.x + radius;
					hi->y = pc.y + radius;
					return;
				}
				if ( a0 + a1 >= 360.0 )
					hi->y = pc.y + radius;
				if ( a0 < 90.0 && a0+a1 >= 90.0 )
					hi->x = pc.x + radius;
				if ( a0 < 180 && a0+a1 >= 180.0 )
					lo->y = pc.y - radius;
				if ( a0 < 270.0 && a0+a1 >= 270.0 )
					lo->x = pc.x - radius;
			}
			if ( segPtr->type == SEG_STRLIN ) {
				width.x = segPtr->width * fabs(cos( D2R( FindAngle(p0, p1) ) ) ) / 2.0;
				width.y = segPtr->width * fabs(sin( D2R( FindAngle(p0, p1) ) ) ) / 2.0;
			} else if ( segPtr->type == SEG_CRVLIN ) {
				/* TODO: be more precise about curved line width */
				width.x = width.y = segPtr->width/2.0;
			} else if ( segPtr->type == SEG_BENCH ) {
				width.x = BenchGetWidth( segPtr->u.l.option ) * fabs(cos( D2R( FindAngle(p0, p1) ) ) ) / 2.0;
				width.y = BenchGetWidth( segPtr->u.l.option ) * fabs(sin( D2R( FindAngle(p0, p1) ) ) ) / 2.0;
			}
		break;
	case SEG_POLY:
		/* TODO: be more precise about poly line width */
		width.x = width.y = segPtr->width/2.0;
	case SEG_FILPOLY:
		for (inx=0; inx<segPtr->u.p.cnt; inx++ ) {
			REORIGIN( p0, segPtr->u.p.pts[inx], angle, xlat )
			if (inx==0) {
				*lo = *hi = p0;
			} else {
				if (p0.x < lo->x)
					lo->x = p0.x;
				if (p0.y < lo->y)
					lo->y = p0.y;
				if (p0.x > hi->x)
					hi->x = p0.x;
				if (p0.y > hi->y)
					hi->y = p0.y;
			}
		}
		break;
	case SEG_FILCRCL:
		REORIGIN( p0, segPtr->u.c.center, angle, xlat )
		lo->x = p0.x - fabs(segPtr->u.c.radius);
		hi->x = p0.x + fabs(segPtr->u.c.radius);
		lo->y = p0.y - fabs(segPtr->u.c.radius);
		hi->y = p0.y + fabs(segPtr->u.c.radius);
		break;
	case SEG_TEXT:
		REORIGIN( p0, segPtr->u.t.pos, angle, xlat )
		GetTextBounds( p0, angle+segPtr->u.t.angle, segPtr->u.t.string, segPtr->u.t.fontSize, lo, hi );
		break;
	case SEG_BEZLIN:
	case SEG_BEZTRK:								//Bezier control arms form a "tent" around the curve
		REORIGIN( pBez[0], segPtr->u.b.pos[0], angle, xlat )
		REORIGIN( pBez[1], segPtr->u.b.pos[1], angle, xlat )
		REORIGIN( pBez[2], segPtr->u.b.pos[2], angle, xlat )
		REORIGIN( pBez[3], segPtr->u.b.pos[3], angle, xlat )
		lo->x = hi->x = pBez[0].x;
		lo->y = hi->y = pBez[0].y;
		for (int i=1;i<4;i++) {
			lo->x = lo->x>pBez[i].x?pBez[i].x:lo->x;
			lo->y = lo->y>pBez[i].y?pBez[i].y:lo->y;
			hi->x = hi->x<pBez[i].x?pBez[i].x:hi->x;
			hi->y = hi->y<pBez[i].y?pBez[i].y:hi->y;
		}
		width.x = width.y = segPtr->width/2.0;
		break;
	default:
		;
	}
	lo->x -= width.x;
	lo->y -= width.y;
	hi->x += width.x;
	hi->y += width.y;
}


EXPORT void GetSegBounds(
		coOrd xlat,
		ANGLE_T angle,
		wIndex_t segCnt,
		trkSeg_p segs,
		coOrd * orig_ret,
		coOrd * size_ret )
{
	trkSeg_p s;
	coOrd lo, hi, tmpLo, tmpHi;
	BOOL_T first;

	first = TRUE;
	for (s=segs; s<&segs[segCnt]; s++) {
		if (s->type == ' ')
			continue;
		if (first) {
			Get1SegBounds( s, xlat, angle, &lo, &hi );
			first = FALSE;
		} else {
			Get1SegBounds( s, xlat, angle, &tmpLo, &tmpHi );
			if (tmpLo.x < lo.x)
				lo.x = tmpLo.x;
			if (tmpLo.y < lo.y)
				lo.y = tmpLo.y;
			if (tmpHi.x > hi.x)
				hi.x = tmpHi.x;
			if (tmpHi.y > hi.y)
				hi.y = tmpHi.y;
		}
	}
	if (first) {
		*orig_ret = xlat;
		*size_ret = zero;
		return;
	}
	if (lo.x < hi.x) {
		orig_ret->x = lo.x;
		size_ret->x = hi.x-lo.x;
	} else {
		orig_ret->x = hi.x;
		size_ret->x = lo.x-hi.x;
	}
	if (lo.y < hi.y) {
		orig_ret->y = lo.y;
		size_ret->y = hi.y-lo.y;
	} else {
		orig_ret->y = hi.y;
		size_ret->y = lo.y-hi.y;
	}
}


EXPORT void MoveSegs(
		wIndex_t segCnt,
		trkSeg_p segs,
		coOrd orig )
{
	trkSeg_p s;
	int inx;

	for (s=segs; s<&segs[segCnt]; s++) {
		switch (s->type) {
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
		case SEG_TBLEDGE:
		case SEG_STRTRK:
			s->u.l.pos[0].x += orig.x;
			s->u.l.pos[0].y += orig.y;
			s->u.l.pos[1].x += orig.x;
			s->u.l.pos[1].y += orig.y;
			break;
		case SEG_CRVLIN:
		case SEG_CRVTRK:
		case SEG_FILCRCL:
			s->u.c.center.x += orig.x;
			s->u.c.center.y += orig.y;
			break;
		case SEG_TEXT:
			s->u.t.pos.x += orig.x;
			s->u.t.pos.y += orig.y;
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			for (inx=0; inx<s->u.p.cnt; inx++) {
				s->u.p.pts[inx].x += orig.x;
				s->u.p.pts[inx].y += orig.y;
			}
			break;
		case SEG_JNTTRK:
			s->u.j.pos.x += orig.x;
			s->u.j.pos.y += orig.y;
			break;
        case SEG_BEZTRK:
        case SEG_BEZLIN:
        	for (inx=0;inx<4;inx++) {
        		s->u.b.pos[inx].x +=orig.x;
            	s->u.b.pos[inx].y +=orig.y;
        	}
            FixUpBezierSeg(s->u.b.pos,s,s->type == SEG_BEZTRK);
            break;
		}
	}
}


EXPORT void RotateSegs(
		wIndex_t segCnt,
		trkSeg_p segs,
		coOrd orig,
		ANGLE_T angle )
{
	trkSeg_p s;
	int inx;

	for (s=segs; s<&segs[segCnt]; s++) {
		switch (s->type) {
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
		case SEG_TBLEDGE:
		case SEG_STRTRK:
			Rotate( &s->u.l.pos[0], orig, angle );
			Rotate( &s->u.l.pos[1], orig, angle );
			break;
		case SEG_CRVLIN:
		case SEG_CRVTRK:
		case SEG_FILCRCL:
			Rotate( &s->u.c.center, orig, angle );
			s->u.c.a0 = NormalizeAngle( s->u.c.a0+angle );
			break;
		case SEG_TEXT:
			Rotate( &s->u.t.pos, orig, angle );
			s->u.t.angle = NormalizeAngle( s->u.t.angle+angle );
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			for (inx=0; inx<s->u.p.cnt; inx++) {
				Rotate( &s->u.p.pts[inx], orig, angle );
			}
			break;
		case SEG_JNTTRK:
			Rotate( &s->u.j.pos, orig, angle );
			s->u.j.angle = NormalizeAngle( s->u.j.angle+angle );
			break;
        case SEG_BEZLIN:
        case SEG_BEZTRK:
            Rotate( &s->u.b.pos[0], orig, angle );
            Rotate( &s->u.b.pos[1], orig, angle );
            Rotate( &s->u.b.pos[2], orig, angle );
            Rotate( &s->u.b.pos[3], orig, angle );
            FixUpBezierSeg(s->u.b.pos,s,s->type == SEG_BEZTRK);
            break;
        }
	}
}

EXPORT void FlipSegs(
		wIndex_t segCnt,
		trkSeg_p segs,
		coOrd orig,
		ANGLE_T angle )
{
	trkSeg_p s;
	int inx;
	coOrd * pts;

	for (s=segs; s<&segs[segCnt]; s++) {
		switch (s->type) {
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
		case SEG_TBLEDGE:
		case SEG_STRTRK:
			s->u.l.pos[0].y = -s->u.l.pos[0].y;
			s->u.l.pos[1].y = -s->u.l.pos[1].y;
			break;
		case SEG_CRVTRK:
			s->u.c.radius = - s->u.c.radius;
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			s->u.c.center.y = -s->u.c.center.y;
			if ( s->u.c.a1 < 360.0 )
				s->u.c.a0 = NormalizeAngle( 180.0 - s->u.c.a0 - s->u.c.a1 );
			break;
		case SEG_TEXT:
			s->u.t.pos.y = -s->u.t.pos.y;
/* TODO flip text angle */
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			pts = (coOrd*)MyMalloc( s->u.p.cnt * sizeof *(coOrd*)NULL );
			memcpy( pts, s->u.p.pts, s->u.p.cnt * sizeof *(coOrd*)NULL );
			s->u.p.pts = pts;
			for (inx=0; inx<s->u.p.cnt; inx++) {
				s->u.p.pts[inx].y = -s->u.p.pts[inx].y;
			}
			MyFree(pts);
			break;
		case SEG_JNTTRK:
			s->u.j.pos.y = - s->u.j.pos.y;
			s->u.j.angle = NormalizeAngle( 180.0 - s->u.j.angle );
			s->u.j.negate = ! s->u.j.negate;
			break;
        case SEG_BEZTRK:
        case SEG_BEZLIN:
            s->u.b.pos[0].y = -s->u.b.pos[0].y;
            s->u.b.pos[1].y = -s->u.b.pos[1].y;
            s->u.b.pos[2].y = -s->u.b.pos[2].y;
            s->u.b.pos[3].y = -s->u.b.pos[3].y;
            FixUpBezierSeg(s->u.b.pos,s,s->type == SEG_BEZTRK);
            break;
		}
	}
}


EXPORT void RescaleSegs(
		wIndex_t segCnt,
		trkSeg_p segs,
		DIST_T scale_x,
		DIST_T scale_y,
		DIST_T scale_w )
{
	trkSeg_p s;
	int inx;

	for (s=segs; s<&segs[segCnt]; s++) {
		s->width *= scale_w;
		switch (s->type) {
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
		case SEG_TBLEDGE:
		case SEG_STRTRK:
			s->u.l.pos[0].y *= scale_y;
			s->u.l.pos[0].x *= scale_x;
			s->u.l.pos[1].x *= scale_x;
			s->u.l.pos[1].y *= scale_y;
			break;
		case SEG_CRVTRK:
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			s->u.c.center.x *= scale_x;
			s->u.c.center.y *= scale_y;
			s->u.c.radius *= scale_w;
			break;
		case SEG_TEXT:
			s->u.t.pos.x *= scale_x;
			s->u.t.pos.y *= scale_y;
			s->u.t.fontSize *= scale_w;
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			for (inx=0; inx<s->u.p.cnt; inx++) {
				s->u.p.pts[inx].x *= scale_x;
				s->u.p.pts[inx].y *= scale_y;
			}
			break;
		case SEG_JNTTRK:
			s->u.j.pos.x *= scale_x;
			s->u.j.pos.y *= scale_y;
			s->u.j.R *= scale_w;
			s->u.j.L *= scale_w;
			s->u.j.l0 *= scale_w;
			s->u.j.l1 *= scale_w;
			break;
        case SEG_BEZTRK:
        case SEG_BEZLIN:
            s->u.b.pos[0].y *= scale_y;
            s->u.b.pos[0].x *= scale_x;
            s->u.b.pos[1].x *= scale_x;
            s->u.b.pos[1].y *= scale_y;
            s->u.b.pos[2].y *= scale_y;
            s->u.b.pos[2].x *= scale_x;
            s->u.b.pos[3].x *= scale_x;
            s->u.b.pos[3].y *= scale_y;
            FixUpBezierSeg(s->u.b.pos,s,s->type == SEG_BEZTRK);

            break;

		}
	}
}


EXPORT void CloneFilledDraw(
		int segCnt,
		trkSeg_p segs,
		BOOL_T reorigin )
{
	coOrd * newPts;
	trkSeg_p sp;
	wIndex_t inx;

	for ( sp=segs; sp<&segs[segCnt]; sp++ ) {
		switch (sp->type) {
		case SEG_POLY:
		case SEG_FILPOLY:
			newPts = (coOrd*)MyMalloc( sp->u.p.cnt * sizeof *(coOrd*)0 );
			if ( reorigin ) {
				for ( inx = 0; inx<sp->u.p.cnt; inx++ )
					REORIGIN( newPts[inx], sp->u.p.pts[inx], sp->u.p.angle, sp->u.p.orig );
				sp->u.p.angle = 0;
				sp->u.p.orig = zero;
			} else {
				memcpy( newPts, sp->u.p.pts, sp->u.p.cnt * sizeof *(coOrd*)0 );
			}
			//if (sp->u.p.pts)    Can't do this a pts could be pointing at static
			//	free(sp->u.p.pts);
			sp->u.p.pts = newPts;
			break;
		case SEG_TEXT:
			sp->u.t.string = MyStrdup( sp->u.t.string );
			break;
		case SEG_BEZTRK:
		case SEG_BEZLIN:
			sp->bezSegs.cnt = 0;
			if (sp->bezSegs.ptr) MyFree(sp->bezSegs.ptr);
			sp->bezSegs.ptr = NULL;
			sp->bezSegs.max = 0;
			FixUpBezierSeg(sp->u.b.pos,sp,sp->type == SEG_BEZTRK);
			break;
		default:
			break;
		}
	}
}


EXPORT void FreeFilledDraw(
		int segCnt,
		trkSeg_p segs )
{
	trkSeg_p sp;

	for ( sp=segs; sp<&segs[segCnt]; sp++ ) {
		switch (sp->type) {
		case SEG_POLY:
		case SEG_FILPOLY:
			if ( sp->u.p.pts )
				MyFree( sp->u.p.pts );
			sp->u.p.pts = NULL;
			break;
		case SEG_TEXT:
			if ( sp->u.t.string )
				MyFree( sp->u.t.string );
			sp->u.t.string = NULL;
			break;
		default:
			break;
		}
	}
}

/*
 * DistanceSegs
 *
 * Find the closest point on the Segs to the point pos.
 * Return the distance to the point, the point on the curve and the index of the segment that contains it.
 *
 */

EXPORT DIST_T DistanceSegs(
		coOrd orig,
		ANGLE_T angle,
		wIndex_t segCnt,
		trkSeg_p segPtr,
		coOrd * pos,
		wIndex_t * inx_ret )
{
	DIST_T d, dd = 100000.0, ddd;
	coOrd p0, p1, p2, pt, lo, hi;
	BOOL_T found = FALSE;
	wIndex_t inx, lin;
	segProcData_t segProcData2;
	p0 = *pos;
	Rotate( &p0, orig, -angle );
	p0.x -= orig.x;
	p0.y -= orig.y;
	d = dd;
	for ( inx=0; segCnt>0; segPtr++,segCnt--,inx++) {
		p1 = p0;
		switch (segPtr->type) {
		case SEG_STRTRK:
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
		case SEG_TBLEDGE:
			dd = LineDistance( &p1, segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
			if ( segPtr->type == SEG_BENCH ) {
				if ( dd < BenchGetWidth( segPtr->u.l.option )/2.0 )
					dd = 0.0;
			}
			break;
		case SEG_CRVTRK:
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			dd = CircleDistance( &p1, segPtr->u.c.center, fabs(segPtr->u.c.radius), segPtr->u.c.a0, segPtr->u.c.a1 );
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			ddd = 100000.0;
			for (lin=0;lin<segPtr->u.p.cnt;lin++) {
				pt = p0;
				if (lin < segPtr->u.p.cnt-1 )
					ddd = LineDistance( &pt, segPtr->u.p.pts[lin], segPtr->u.p.pts[lin+1] );
				else
					ddd = LineDistance( &pt, segPtr->u.p.pts[lin], segPtr->u.p.pts[0] );
				if ( ddd < dd ) {
					dd = ddd;
					p1 = pt;
				}
			}
			break;
        case SEG_BEZTRK:
        case SEG_BEZLIN:
        		dd = 100000.0;
        		pt = p0;
        		for (int i = 0;i<segPtr->bezSegs.cnt;i++) {
        			segProcData2.distance.pos1 = pt;
        			SegProc(SEGPROC_DISTANCE,&DYNARR_N(trkSeg_t,segPtr->bezSegs,i),&segProcData2);
        			if (segProcData2.distance.dd<dd) {
        				dd = segProcData2.distance.dd;
        				p1 = segProcData2.distance.pos1;
        			}
        		}
            break;
		case SEG_TEXT:
			/*GetTextBounds( segPtr->u.t.pos, angle+segPtr->u.t.angle, segPtr->u.t.string, segPtr->u.t.fontSize, &lo, &hi );*/
			GetTextBounds( zero, 0, segPtr->u.t.string, segPtr->u.t.fontSize, &lo, &hi ); //lo and hi are relative to seg origin
			p0.x -= segPtr->u.t.pos.x;
			p0.y -= segPtr->u.t.pos.y;
			Rotate( &p0, zero, -segPtr->u.t.angle );
			DIST_T dx, dy;
			if (p0.x > lo.x && p0.x < hi.x && p0.y >lo.y && p0.y < hi.y) {  //Within rectangle - therefore small dist
				hi.x /= 2.0;
				hi.y /= 2.0;
				dd = 0.1*FindDistance(hi, p0)/FindDistance(lo,hi);  // Proportion to mean that the closer we to the center or the smaller the target in overlapping cases, the more likely we pick it
				break;
			}
			hi.x /= 2.0;   // rough center of rectangle
			hi.y /= 2.0;
			if (fabs((p0.x-hi.x)/hi.x)<fabs((p0.y-hi.y)/hi.y)) {  	// Proportionally closer to x
				if (p0.x > hi.x) dd = (p0.x - hi.x);
				else dd = fabs(p0.x-hi.x);
			} else {												// Closer to y
				if (p0.y > hi.y) dd = (p0.y - hi.y);
				else dd = fabs(p0.y-hi.y);
			}
			/*printf( "dx=%0.4f dy=%0.4f dd=%0.3f\n", dx, dy, dd );*/
/*
			if ( p0.x >= lo.x && p0.x <= hi.x &&
				 p0.y >= lo.y && p0.y <= hi.y ) {
				p1.x = (lo.x+hi.x)/2.0;
				p1.y = (lo.y+hi.y)/2.0;
				dd = FindDistance( p0, p1 );
			}
*/
			break;
		case SEG_JNTTRK:
			dd = JointDistance( &p1, segPtr->u.j.pos, segPtr->u.j.angle, segPtr->u.j.l0, segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.negate, segPtr->u.j.Scurve );
			break;
		default:
			dd = 100000.0;
		}
		if (dd < d) {
			d = dd;
			p2 = p1;
			if (inx_ret)
				*inx_ret = inx;
			found = TRUE;
		}
	}
	if (found) {
		p2.x += orig.x;
		p2.y += orig.y;
		Rotate( &p2, orig, angle );
		*pos = p2;
	}
	return d;
}

/*
 * Get the angle at a point on the segments closest to pos1
 * Optionally return the index of the segment and the distance to that point
 *
 */
EXPORT ANGLE_T GetAngleSegs(
		wIndex_t segCnt,
		trkSeg_p segPtr,
		coOrd * pos1,						// Now IN/OUT OUT =
		wIndex_t * segInxR,
		DIST_T * dist,
		BOOL_T * seg_backwards,
		wIndex_t * subSegInxR,
		BOOL_T * negative_radius)
{
	wIndex_t inx;
	ANGLE_T angle = 0.0;
	coOrd p0;
	DIST_T d, dd;
	segProcData_t segProcData;
	coOrd pos2 = * pos1;
	BOOL_T negative = FALSE;
	BOOL_T backwards = FALSE;
	if (subSegInxR) *subSegInxR = -1;

	d = DistanceSegs( zero, 0.0, segCnt, segPtr, &pos2, &inx ); //
	if (dist) * dist = d;
	segPtr += inx;
	segProcData.getAngle.pos = pos2;
	switch ( segPtr->type ) {
	case SEG_STRTRK:
	case SEG_STRLIN:
	case SEG_DIMLIN:
	case SEG_BENCH:
	case SEG_TBLEDGE:
		StraightSegProc( SEGPROC_GETANGLE, segPtr, &segProcData );
		angle = segProcData.getAngle.angle;
		break;
	case SEG_CRVTRK:
	case SEG_CRVLIN:
	case SEG_FILCRCL:
		CurveSegProc( SEGPROC_GETANGLE, segPtr, &segProcData );
		angle = segProcData.getAngle.angle;
		negative = segProcData.getAngle.negative_radius;
		backwards = segProcData.getAngle.backwards;
		break;
	case SEG_JNTTRK:
		JointSegProc( SEGPROC_GETANGLE, segPtr, &segProcData );
		angle = segProcData.getAngle.angle;
		break;
    case SEG_BEZTRK:
    case SEG_BEZLIN:
        BezierSegProc( SEGPROC_GETANGLE, segPtr, &segProcData );
        angle = segProcData.getAngle.angle;
        negative = segProcData.getAngle.negative_radius;
        backwards = segProcData.getAngle.backwards;
        if (subSegInxR) *subSegInxR = segProcData.getAngle.bezSegInx;
        break;
	case SEG_POLY:
	case SEG_FILPOLY:
		p0 = pos2;
		dd = LineDistance( &p0, segPtr->u.p.pts[segPtr->u.p.cnt-1], segPtr->u.p.pts[0] );
		angle = FindAngle( segPtr->u.p.pts[segPtr->u.p.cnt-1], segPtr->u.p.pts[0] );
		for ( inx=0; inx<segPtr->u.p.cnt-1; inx++ ) {
			p0 = pos2;
			d = LineDistance( &p0, segPtr->u.p.pts[inx], segPtr->u.p.pts[inx+1] );
			if ( d < dd ) {
				dd = d;
				angle = FindAngle( segPtr->u.p.pts[inx], segPtr->u.p.pts[inx+1] );
			}
		}
		break;
	case SEG_TEXT:
		angle = segPtr->u.t.angle;
		break;
	default:
		AbortProg( "GetAngleSegs(%d)", segPtr->type );
	}
	if ( segInxR ) *segInxR = inx;
	if (seg_backwards) *seg_backwards = backwards;
	if (negative_radius) *negative_radius = negative;

	* pos1 = pos2;
	return angle;
}

/****************************************************************************
 *
 * Color
 *
 ****************************************************************************/

typedef struct {
		FLOAT_T h, s, v;
		} hsv_t;
static FLOAT_T max_s;
static FLOAT_T max_v;
static dynArr_t hsv_da;
#define hsv(N) DYNARR_N( hsv_t, hsv_da, N )

static void Hsv2rgb(
		hsv_t	hsv,
		long	*rgb )
{
  int i;
  FLOAT_T f, w, q, t, r=0, g=0, b=0;

  if (hsv.s == 0.0)
	hsv.s = 0.000001;

  if (hsv.h == -1.0)
	{
	  r = hsv.v;
	  g = hsv.v;
	  b = hsv.v;
	}
  else
	{
	  if (hsv.h == 360.0)
		hsv.h = 0.0;
	  hsv.h = hsv.h / 60.0;
	  i = (int) hsv.h;
	  f = hsv.h - i;
	  w = hsv.v * (1.0 - hsv.s);
	  q = hsv.v * (1.0 - (hsv.s * f));
	  t = hsv.v * (1.0 - (hsv.s * (1.0 - f)));

	  switch (i)
		{
		case 0:
		  r = hsv.v;
		  g = t;
		  b = w;
		  break;
		case 1:
		  r = q;
		  g = hsv.v;
		  b = w;
		  break;
		case 2:
		  r = w;
		  g = hsv.v;
		  b = t;
		  break;
		case 3:
		  r = w;
		  g = q;
		  b = hsv.v;
		  break;
		case 4:
		  r = t;
		  g = w;
		  b = hsv.v;
		  break;
		case 5:
		  r = hsv.v;
		  g = w;
		  b = q;
		  break;
		}
	}
	*rgb = wRGB( (int)(r*255), (int)(g*255), (int)(b*255) );
}


static void Rgb2hsv(
		long	 rgb,
		hsv_t	*hsv )
{
  FLOAT_T r, g, b;
  FLOAT_T max, min, delta;

  r = ((rgb>>16)&0xFF)/255.0;
  g = ((rgb>>8)&0xFF)/255.0;
  b = ((rgb)&0xFF)/255.0;

  max = r;
  if (g > max)
	max = g;
  if (b > max)
	max = b;

  min = r;
  if (g < min)
	min = g;
  if (b < min)
	min = b;

  hsv->v = max;

  if (max != 0.0)
	hsv->s = (max - min) / max;
  else
	hsv->s = 0.0;

  if (hsv->s == 0.0)
	hsv->h = -1.0;
  else
	{
	  delta = max - min;

	  if (r == max)
		hsv->h = (g - b) / delta;
	  else if (g == max)
		hsv->h = 2.0 + (b - r) / delta;
	  else if (b == max)
		hsv->h = 4.0 + (r - g) / delta;

	  hsv->h = hsv->h * 60.0;

	  if (hsv->h < 0.0)
		hsv->h = hsv->h + 360;
	}
}


static void Fill_hsv(
		wIndex_t segCnt,
		trkSeg_p segPtr,
		hsv_t * hsvP )
{
	int inx;

	max_s = 0.0;
	max_v = 0.0;
	for ( inx=0; inx<segCnt; inx++ ) {
		Rgb2hsv( wDrawGetRGB(segPtr[inx].color), &hsvP[inx] );
		if ( hsvP[inx].h >= 0 ) {
			if ( max_s < hsvP[inx].s )
				max_s = hsvP[inx].s;
			if ( max_v < hsvP[inx].v )
				max_v = hsvP[inx].v;
		}
	}
}

EXPORT void RecolorSegs(
		wIndex_t cnt,
		trkSeg_p segs,
		wDrawColor color )
{
	long rgb;
	wIndex_t inx;
	hsv_t hsv0;
	FLOAT_T h, s, v;

	DYNARR_SET( hsv_t, hsv_da, cnt );
	Fill_hsv( cnt, segs, &hsv(0) );
	rgb = wDrawGetRGB( color );
	Rgb2hsv( rgb, &hsv0 );
	h = hsv0.h;
	if ( max_s > 0.25 )
		s = hsv0.s/max_s;
	else
		s = 1.0;
	if ( max_v > 0.25 )
		v = hsv0.v/max_v;
	else
		v = 1.0;
	for ( inx=0; inx<cnt; inx++,segs++ ) {
		hsv0 = hsv(inx);
		if ( hsv0.h < 0 ) /* ignore black */
			continue;
		hsv0.h = h;
		hsv0.s *= s;
		hsv0.v *= v;
		Hsv2rgb( hsv0, &rgb );
		segs->color = wDrawFindColor( rgb );
	}
}



/****************************************************************************
 *
 * Input/Output
 *
 ****************************************************************************/


static void AppendPath( signed char c )
{
	if (pathPtr == NULL) {
		pathMax = 100;
		pathPtr = (signed char*)MyMalloc( pathMax );
	} else if (pathCnt >= pathMax) {
		pathMax += 100;
		pathPtr = (signed char*)MyRealloc( pathPtr, pathMax );
	}
	pathPtr[pathCnt++] = c;
}


EXPORT BOOL_T ReadSegs( void )
{
	char *cp, *cpp;
	BOOL_T rc=FALSE;
	trkSeg_p s;
	trkEndPt_p e;
	long rgb;
	int i;
	DIST_T elev0, elev1;
	BOOL_T hasElev;
	BOOL_T isPolyV2;
	char type;
	char * plain_text;
	long option;
	BOOL_T subsegs = FALSE;

	descriptionOff = zero;
	tempSpecial[0] = '\0';
	tempCustom[0] = '\0';
	DYNARR_RESET( trkSeg_t, tempSegs_da );
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );
	pathCnt = 0;
	while ( (cp = GetNextLine()) != NULL ) {
		while (isspace(*cp)) cp++;
		hasElev = FALSE;
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			rc = TRUE;
			subsegs = FALSE;
			break;
		}
		if ( strncmp(cp, "SUBSEGS", 7) == 0) {
			subsegs = TRUE;
			continue;
		}
		if (strncmp (cp, "SUBSEND", 7) == 0) {
			subsegs = FALSE;
			continue;
		}
		if ( *cp == '\n' || *cp == '#' ) {
			continue;
		}
		type = *cp++;
		hasElev = FALSE;
		if ( *cp == '3' ) {
			cp++;
			hasElev = TRUE;
		}
		isPolyV2 = FALSE;
		if (*cp == '4') {
			cp++;
			hasElev = TRUE;
			isPolyV2 = TRUE;
		}
		switch (type) {
		case SEG_STRLIN:
		case SEG_TBLEDGE:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = type;
			if ( !GetArgs( cp, hasElev?"lwpfpf":"lwpYpY",
				&rgb, &s->width, &s->u.l.pos[0], &elev0, &s->u.l.pos[1], &elev1 ) ) {
				rc = FALSE;
				break;
			}
			s->u.l.option = 0;
			s->color = wDrawFindColor( rgb );
			break;
		case SEG_DIMLIN:
		case SEG_BENCH:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = type;
			if ( !GetArgs( cp, hasElev?"lwpfpfl":"lwpYpYZ",
				&rgb, &s->width, &s->u.l.pos[0], &elev0, &s->u.l.pos[1], &elev1, &option ) ) {
				rc = FALSE;
				break;
			}
			if ( type == SEG_DIMLIN )
				s->u.l.option = option;
			else
				s->u.l.option = BenchInputOption(option);
			s->color = wDrawFindColor( rgb );
			break;
		case SEG_CRVLIN:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = SEG_CRVLIN;
			if ( !GetArgs( cp, hasElev?"lwfpfff":"lwfpYff",
				&rgb, &s->width,
				 &s->u.c.radius,
				 &s->u.c.center,
				 &elev0,
				 &s->u.c.a0, &s->u.c.a1 ) ) {
				rc = FALSE;
				break;
			}
			s->color = wDrawFindColor( rgb );
			break;
		case SEG_STRTRK:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = SEG_STRTRK;
			s->bezSegs.max = 0;
			s->bezSegs.cnt = 0;
			s->bezSegs.ptr = NULL;
			if ( !GetArgs( cp, hasElev?"lwpfpf":"lwpYpY",
				&rgb, &s->width,
				&s->u.l.pos[0], &elev0,
				&s->u.l.pos[1], &elev1 ) ) {
				rc = FALSE;
				break;
			}
			s->color = wDrawFindColor( rgb );
			break;
		case SEG_CRVTRK:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = SEG_CRVTRK;
			s->bezSegs.max = 0;
			s->bezSegs.cnt = 0;
			s->bezSegs.ptr = NULL;
			if ( !GetArgs( cp, hasElev?"lwfpfff":"lwfpYff",
				 &rgb, &s->width,
				 &s->u.c.radius,
				 &s->u.c.center,
				 &elev0,
				 &s->u.c.a0, &s->u.c.a1 ) ) {
				rc = FALSE;
				break;
			}
			s->color = wDrawFindColor( rgb );
			break;
		case SEG_JNTTRK:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = SEG_JNTTRK;
			if ( !GetArgs( cp, hasElev?"lwpffffffl":"lwpYfffffl",
				 &rgb, &s->width,
				 &s->u.j.pos,
				 &elev0,
				 &s->u.j.angle,
				 &s->u.j.l0,
				 &s->u.j.l1,
				 &s->u.j.R,
				 &s->u.j.L,
				 &option ) ) {
				rc = FALSE;
				break;
			}
			s->u.j.negate = ( option&1 )!=0;
			s->u.j.flip = ( option&2 )!=0;
			s->u.j.Scurve = ( option&4 )!=0;
			s->color = wDrawFindColor( rgb );
			break;
        case SEG_BEZTRK:
            DYNARR_APPEND( trkSeg_t, tempSegs_da, 10);
            s = &tempSegs(tempSegs_da.cnt-1);
            s->type=SEG_BEZTRK;
            s->bezSegs.max=0;
            s->bezSegs.ptr= NULL;
            s->bezSegs.cnt=0;
            if ( !GetArgs( cp, hasElev?"lwpppp":"lwpppp",
                &rgb, &s->width,
                &s->u.b.pos[0],
                &s->u.b.pos[1],
                &s->u.b.pos[2],
                &s->u.b.pos[3])) {
                rc = FALSE;
                break;
            }
            s->color = wDrawFindColor( rgb );
            break;
        case SEG_BEZLIN:
            DYNARR_APPEND( trkSeg_t, tempSegs_da, 10);
            s = &tempSegs(tempSegs_da.cnt-1);
            s->type=SEG_BEZLIN;
            s->bezSegs.max=0;
            s->bezSegs.ptr= NULL;
            s->bezSegs.cnt=0;
            if ( !GetArgs( cp, hasElev?"lwpppp":"lwpppp",
                &rgb, &s->width,
                &s->u.b.pos[0],
                &s->u.b.pos[1],
                &s->u.b.pos[2],
                &s->u.b.pos[3])) {
                rc = FALSE;
                break;
            }
            s->color = wDrawFindColor( rgb );
            break;
		case SEG_FILCRCL:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = SEG_FILCRCL;
			if ( !GetArgs( cp, hasElev?"lwfpf":"lwfpY",
				 &rgb, &s->width,
				 &s->u.c.radius,
				 &s->u.c.center,
				 &elev0 ) ) {
				rc = FALSE;
				/*??*/break;
			}
			s->u.c.a0 = 0.0;
			s->u.c.a1 = 360.0;
			s->color = wDrawFindColor( rgb );
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = type;
			if (isPolyV2) {
				if ( !GetArgs( cp, "lwdd",
					 &rgb, &s->width,
					 &s->u.p.cnt, &s->u.p.polyType) ) {
					rc = FALSE;
					/*??*/break;
				}
			} else {
				if ( !GetArgs( cp, "lwd",
						&rgb, &s->width,
						&s->u.p.cnt) ) {
					rc = FALSE;
					/*??*/break;
				}
			}
			s->color = wDrawFindColor( rgb );
			s->u.p.pts = (coOrd*)MyMalloc( s->u.p.cnt * sizeof *(coOrd*)NULL );
			for ( i=0; i<s->u.p.cnt; i++ ) {
				cp = GetNextLine();
				if (cp == NULL || !GetArgs( cp, hasElev?"pf":"pY", &s->u.p.pts[i], &elev0 ) ) {
					rc = FALSE;
					/*??*/break;
				}
			}
			s->u.p.angle = 0.0;
			s->u.p.orig = zero;
			break;
		case SEG_TEXT:
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			s = &tempSegs(tempSegs_da.cnt-1);
			s->type = type;
			s->u.t.fontP = NULL;
			char * expandedText;
			if ( !GetArgs( cp, "lpf0fq", &rgb, &s->u.t.pos, &s->u.t.angle, &s->u.t.fontSize, &expandedText ) ) {
				rc = FALSE;
				/*??*/break;
			}
			plain_text = ConvertFromEscapedText(expandedText);
			s->u.t.string = plain_text;
			MyFree(expandedText);
			s->color = wDrawFindColor( rgb );
			break;
		case SEG_UNCEP:
		case SEG_CONEP:
			DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 10 );
			e = &tempEndPts(tempEndPts_da.cnt-1);
			if (type == SEG_CONEP) {
				if ( !GetArgs( cp, "dc", &e->index, &cp ) ) {
					rc = FALSE;
					/*??*/break;
				}
			} else {
				e->index = -1;
			}
			if ( !GetArgs( cp, "pfc",
				&e->pos, &e->angle, &cp) ) {
				rc = FALSE;
				/*??*/break;
			}
			e->elev.option = 0;
			e->elev.u.height = 0.0;
			e->elev.doff = zero;
			e->option = 0;
			if ( cp != NULL ) {
				if (paramVersion < 7) {
					GetArgs( cp, "dfp", &e->elev.option,  &e->elev.u.height, &e->elev.doff, &cp );
					/*??*/break;
				}
				GetArgs( cp, "lpc", &option, &e->elev.doff, &cp );
				e->option = option >> 8;
				e->elev.option = (int)(option&0xFF);
				if ( (e->elev.option&ELEV_MASK) != ELEV_NONE ) {
					switch (e->elev.option&ELEV_MASK) {
					case ELEV_DEF:
						GetArgs( cp, "fc", &e->elev.u.height, &cp );
						break;
					case ELEV_STATION:
						GetArgs( cp, "qc", &e->elev.u.name, &cp );
						/*??*/break;
					default:
						;
					}
				}
			}
			break;
		case SEG_PATH:
			while (isspace(*cp)) cp++;
			if (*cp == '\"') cp++;
			while ( *cp != '\"') AppendPath((signed char)*cp++);
			AppendPath(0);
			cp++;
			while (1) {
				i = (int)strtol(cp, &cpp, 10);
				if (cp == cpp)
					/*??*/break;
				cp = cpp;
				AppendPath( (signed char)i );
			}
			AppendPath( 0 );
			AppendPath( 0 );
			break;
		case SEG_SPEC:
			strncpy( tempSpecial, cp+1, sizeof tempSpecial - 1 );
			break;
		case SEG_CUST:
			strncpy( tempCustom, cp+1, sizeof tempCustom - 1 );
			break;
		case SEG_DOFF:
			if ( !GetArgs( cp, "ff", &descriptionOff.x, &descriptionOff.y ) ) {
				rc = FALSE;
				/*??*/break;
			}
			break;
		default:
			break;
		}
	}
	AppendPath( 0 );
	return rc;
}

EXPORT BOOL_T WriteSegs(
		FILE * f,
		wIndex_t segCnt,
		trkSeg_p segs )
{
    return WriteSegsEnd(f,segCnt,segs,TRUE);
}


EXPORT BOOL_T WriteSegsEnd(
		FILE * f,
		wIndex_t segCnt,
		trkSeg_p segs, BOOL_T writeEnd)

{
	int i, j;
	BOOL_T rc = TRUE;
	long option;
	char * escaped_text;

	for ( i=0; i<segCnt; i++ ) {
		switch ( segs[i].type ) {
		case SEG_STRTRK:
			rc &= fprintf( f, "\t%c 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n",
				segs[i].type, segs[i].width,
				segs[i].u.l.pos[0].x, segs[i].u.l.pos[0].y,
				segs[i].u.l.pos[1].x, segs[i].u.l.pos[1].y ) > 0;
			break;
		case SEG_STRLIN:
		case SEG_TBLEDGE:
			rc &= fprintf( f, "\t%c3 %ld %0.6f %0.6f %0.6f 0 %0.6f %0.6f 0\n",
				segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
				segs[i].u.l.pos[0].x, segs[i].u.l.pos[0].y,
				segs[i].u.l.pos[1].x, segs[i].u.l.pos[1].y ) > 0;
			break;
		case SEG_DIMLIN:
			rc &= fprintf( f, "\t%c3 %ld %0.6f %0.6f %0.6f 0 %0.6f %0.6f 0 %ld\n",
				segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
				segs[i].u.l.pos[0].x, segs[i].u.l.pos[0].y,
				segs[i].u.l.pos[1].x, segs[i].u.l.pos[1].y,
				segs[i].u.l.option ) > 0;
			break;
		case SEG_BENCH:
			rc &= fprintf( f, "\t%c3 %ld %0.6f %0.6f %0.6f 0 %0.6f %0.6f 0 %ld\n",
				segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
				segs[i].u.l.pos[0].x, segs[i].u.l.pos[0].y,
				segs[i].u.l.pos[1].x, segs[i].u.l.pos[1].y,
				BenchOutputOption(segs[i].u.l.option) ) > 0;
			break;
		case SEG_CRVTRK:
			rc &= fprintf( f, "\t%c %ld %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f\n",
				segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
				segs[i].u.c.radius,
				segs[i].u.c.center.x, segs[i].u.c.center.y,
				segs[i].u.c.a0, segs[i].u.c.a1 ) > 0;
			break;
		case SEG_JNTTRK:
			option = (segs[i].u.j.negate?1:0) + (segs[i].u.j.flip?2:0) + (segs[i].u.j.Scurve?4:0);
			rc &= fprintf( f, "\t%c %ld %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %ld\n",
				segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
				segs[i].u.j.pos.x, segs[i].u.j.pos.y,
				segs[i].u.j.angle,
				segs[i].u.j.l0,
				segs[i].u.j.l1,
				segs[i].u.j.R,
				segs[i].u.j.L,
				option )>0;
			break;
        case SEG_BEZTRK:
        case SEG_BEZLIN:
            rc &= fprintf( f, "\t%c3 %ld %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f\n",
                segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
                segs[i].u.l.pos[0].x, segs[i].u.l.pos[0].y,
                segs[i].u.l.pos[1].x, segs[i].u.l.pos[1].y,
                segs[i].u.l.pos[2].x, segs[i].u.l.pos[2].y,
                segs[i].u.l.pos[3].x, segs[i].u.l.pos[3].y ) > 0;
            rc &= fprintf(f,"\tSUBSEGS\n");
            rc &= WriteSegsEnd(f,segs[i].bezSegs.cnt,segs[i].bezSegs.ptr,FALSE);
            rc &= fprintf(f,"\tSUBSEND\n");
            break;
		case SEG_CRVLIN:
			rc &= fprintf( f, "\t%c3 %ld %0.6f %0.6f %0.6f %0.6f 0 %0.6f %0.6f\n",
				segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
				segs[i].u.c.radius,
				segs[i].u.c.center.x, segs[i].u.c.center.y,
				segs[i].u.c.a0, segs[i].u.c.a1 ) > 0;

			break;
		case SEG_FILCRCL:
			rc &= fprintf( f, "\t%c3 %ld %0.6f %0.6f %0.6f %0.6f 0\n",
				segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
				segs[i].u.c.radius,
				segs[i].u.c.center.x, segs[i].u.c.center.y ) > 0;
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			rc &= fprintf( f, "\t%c4 %ld %0.6f %d %d \n",
				segs[i].type, wDrawGetRGB(segs[i].color), segs[i].width,
				segs[i].u.p.cnt, segs[i].u.p.polyType ) > 0;
			for ( j=0; j<segs[i].u.p.cnt; j++ )
				rc &= fprintf( f, "\t\t%0.6f %0.6f 0\n",
						segs[i].u.p.pts[j].x, segs[i].u.p.pts[j].y ) > 0;
			break;
		case SEG_TEXT: /* 0pf0fq */
			escaped_text = ConvertToEscapedText(segs[i].u.t.string);
			rc &= fprintf( f, "\t%c %ld %0.6f %0.6f %0.6f 0 %0.6f \"%s\"\n",
				segs[i].type, wDrawGetRGB(segs[i].color),
				segs[i].u.t.pos.x, segs[i].u.t.pos.y, segs[i].u.t.angle,
				segs[i].u.t.fontSize, escaped_text ) > 0;
			MyFree(escaped_text);
			break;
		}
	}
	if (writeEnd) rc &= fprintf( f, "\tEND\n" )>0;
	return rc;
}


EXPORT void SegProc(
		segProc_e cmd,
		trkSeg_p segPtr,
		segProcData_p data )
{
	switch (segPtr->type) {
	case SEG_STRTRK:
	case SEG_STRLIN:
		StraightSegProc( cmd, segPtr, data );
		break;
	case SEG_CRVTRK:
	case SEG_CRVLIN:
		CurveSegProc( cmd, segPtr, data );
		break;
	case SEG_JNTTRK:
		JointSegProc( cmd, segPtr, data );
		break;
    case SEG_BEZTRK:
    case SEG_BEZLIN:
        BezierSegProc( cmd, segPtr, data);
        break;
    default:
		AbortProg( "SegProg( %d )", segPtr->type );
		break;
	}
}



/*
 *	Draw Segs
 */

EXPORT void DrawDimLine(
		drawCmd_p d,
		coOrd p0,
		coOrd p1,
		char * dimP,
		wFontSize_t fs,
		FLOAT_T middle,
		wDrawWidth width,
		wDrawColor color,
		long option )
{
	ANGLE_T a0, a1;
	wFont_p fp;
	coOrd size, p, pc;
	DIST_T dist, dist1, fx, fy;
	POS_T x, y;
	coOrd textsize;

	if ( middle < 0.0 ) middle = 0.0;
	if ( middle > 1.0 ) middle = 1.0;
	a0 = FindAngle( p0, p1 );
	dist = fs/144.0;

	if ( ( option & 0x10 ) == 0 ) {
		Translate( &p, p0, a0-45, dist );
		DrawLine( d, p0, p, 0, color );
		Translate( &p, p0, a0+45, dist );
		DrawLine( d, p0, p, 0, color );
	}
	if ( ( option & 0x20 ) == 0 ) {
		Translate( &p, p1, a0-135, dist );
		DrawLine( d, p1, p, 0, color );
		Translate( &p, p1, a0+135, dist );
		DrawLine( d, p1, p, 0, color );
	}

	if ( fs < 2*d->scale ) {
		DrawLine( d, p0, p1, 0, color );
		return;
	}
	fp = wStandardFont( (option&0x01)?F_TIMES:F_HELV, FALSE, FALSE );
	dist =	FindDistance( p0, p1 );
	DrawTextSize( &mainD, dimP, fp, fs, TRUE, &textsize );
	size.x = textsize.x/2.0;
	size.y = textsize.y/2.0;
	dist1 = FindDistance( zero, size );
	if ( dist <= dist1*2 ) {
		DrawLine( d, p0, p1, 0, color );
		return;
	}
		a1 = FindAngle( zero, size );
		p.x = p0.x+(p1.x-p0.x)*middle;
		p.y = p0.y+(p1.y-p0.y)*middle;
		pc = p;
		p.x -= size.x;
		p.y -= size.y;
		fx = fy = 1;
		if (a0>180) {
			a0 = a0-180;
			fx = fy = -1;
		}
		if (a0>90) {
			a0 = 180-a0;
			fy *= -1;
		}
		if (a0>a1) {
			x = size.x;
			y = x * tan(D2R(90-a0));
		} else {
			y = size.y;
			x = y * tan(D2R(a0));
		}
		DrawString( d, p, 0.0, dimP, fp, fs, color );
		p = pc;
		p.x -= fx*x;
		p.y -= fy*y;
		DrawLine( d, p0, p, 0, color );
		p = pc;
		p.x += fx*x;
		p.y += fy*y;
		DrawLine( d, p, p1, 0, color );
}

/*
 * Display the array of segments.
 * Note that Bezier segments in particular contain sub-arrays of Curve and Straight segments.
 */
EXPORT void DrawSegsO(
		drawCmd_p d,
		track_p trk,
		coOrd orig,
		ANGLE_T angle,
		trkSeg_p segPtr,
		wIndex_t segCnt,
		DIST_T trackGauge,
		wDrawColor color,
		long options )
{
	wIndex_t i, j;
	coOrd p0, p1, p2, p3, c;
	ANGLE_T a0;
	wDrawColor color1, color2;
	DIST_T factor = d->dpi/d->scale;
	trkSeg_p tempPtr;

	long option;
	wFontSize_t fs;

	for (i=0; i<segCnt; i++,segPtr++ ) {
		if (color == wDrawColorBlack) {
			color1 = segPtr->color;
			color2 = wDrawColorBlack;
		} else {
			color1 = color2 = color;
		}
		if ( (options&DTS_TIES)!=0 ) {
			if ( segPtr->color == wDrawColorWhite )
				continue;
			switch (segPtr->type) {
			case SEG_STRTRK:
				REORIGIN( p0, segPtr->u.l.pos[0], angle, orig )
				REORIGIN( p1, segPtr->u.l.pos[1], angle, orig )
				DrawStraightTies( d, trk, p0, p1, color );
				break;
			case SEG_CRVTRK:
				a0 = NormalizeAngle(segPtr->u.c.a0 + angle);
				REORIGIN( c, segPtr->u.c.center, angle, orig );
				DrawCurvedTies( d, trk, c, fabs(segPtr->u.c.radius), a0, segPtr->u.c.a1, color );
				break;
			case SEG_JNTTRK:
				REORIGIN( p0, segPtr->u.j.pos, angle, orig );
				DrawJointTrack( d, p0, NormalizeAngle(segPtr->u.j.angle+angle), segPtr->u.j.l0, segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.negate, segPtr->u.j.flip, segPtr->u.j.Scurve, trk, -1, -1, trackGauge, color1, options );
				break;
            case SEG_BEZTRK:
                REORIGIN(p0, segPtr->u.b.pos[0], angle, orig);
                REORIGIN(p1, segPtr->u.b.pos[1], angle, orig);
                REORIGIN(p2, segPtr->u.b.pos[2], angle, orig);
                REORIGIN(p3, segPtr->u.b.pos[3], angle, orig);
                tempPtr = segPtr->bezSegs.ptr;
                for(int j=0;j<segPtr->bezSegs.cnt;j++,tempPtr++) {   //Loop through sub parts (only Trks supported)
                	if (tempPtr->type == SEG_CRVTRK) {
                		a0 = NormalizeAngle(tempPtr->u.c.a0 + angle);
                		REORIGIN( c, tempPtr->u.c.center, angle, orig );
                		DrawCurvedTies( d, trk, c, fabs(tempPtr->u.c.radius), a0, tempPtr->u.c.a1, color );
                	}
                	if (tempPtr->type == SEG_STRTRK) {
                		REORIGIN( p0, tempPtr->u.l.pos[0], angle, orig )
                		REORIGIN( p1, tempPtr->u.l.pos[1], angle, orig )
						DrawStraightTies( d, trk, p0, p1, color );
                	}
                }
                break;
            }
			continue;
		}
		switch (segPtr->type) {
		case SEG_STRTRK:
		case SEG_CRVTRK:
		case SEG_JNTTRK:
        case SEG_BEZTRK:
		case SEG_TEXT:
			break;
		default:
			if (d->options&DC_QUICK)
				return;
			if ((d->options&DC_SIMPLE) != 0 &&
				trackGauge != 0.0)
				return;
		}
		switch (segPtr->type) {
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
		case SEG_TBLEDGE:
		case SEG_STRTRK:
			REORIGIN( p0, segPtr->u.l.pos[0], angle, orig )
			REORIGIN( p1, segPtr->u.l.pos[1], angle, orig )
			switch (segPtr->type) {
			case SEG_STRTRK:
				if (color1 == wDrawColorBlack)
					color1 = normalColor;
				if ( segPtr->color == wDrawColorWhite )
					break;
				DrawStraightTrack( d,
					p0, p1,
					FindAngle(p0, p1 ),
					NULL, trackGauge, color1, options );
				break;
			case SEG_STRLIN:
				DrawLine( d, p0, p1, (wDrawWidth)floor(segPtr->width*factor+0.5), color1 );
				break;
			case SEG_DIMLIN:
			case SEG_BENCH:
			case SEG_TBLEDGE:
				if ( (d->options&DC_GROUP) ||
					 (segPtr->type == SEG_DIMLIN && d->funcs == &tempSegDrawFuncs) ) {
					DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
					tempPtr = &tempSegs(tempSegs_da.cnt-1);
					memcpy( tempPtr, segPtr, sizeof segPtr[0] );
					tempPtr->u.l.pos[0] = p0;
					tempPtr->u.l.pos[1] = p1;
				} else {
					switch ( segPtr->type ) {
					case SEG_DIMLIN:
						fs = descriptionFontSize*4;
						option = segPtr->u.l.option;
						fs /= (option==0?8:option==1?4:option==2?2:1);
						if ( fs < 2 )
							fs = 2;
						DrawDimLine( d, p0, p1, FormatDistance(FindDistance(p0,p1)), fs, 0.5, 0, color, option & 0x00 );
						break;
					case SEG_BENCH:
						DrawBench( d, p0, p1, color1, color2, options, segPtr->u.l.option );
						break;
					case SEG_TBLEDGE:
						DrawLine( d, p0, p1, (wDrawWidth)floor(3.0/mainD.dpi*d->dpi+0.5) , color );
						break;
					}
				}
				break;
			}
			break;
		case SEG_CRVLIN:
		case SEG_CRVTRK:
			a0 = NormalizeAngle(segPtr->u.c.a0 + angle);
			REORIGIN( c, segPtr->u.c.center, angle, orig );
			if (segPtr->type == SEG_CRVTRK) {
				if (color1 == wDrawColorBlack)
					color1 = normalColor;
				if ( segPtr->color == wDrawColorWhite )
					break;
				p0.x = p0.y = p1.x = p1.y = 0;
				DrawCurvedTrack( d,
					c,
					fabs(segPtr->u.c.radius),
					a0, segPtr->u.c.a1,
					p0, p1,
					NULL, trackGauge, color1, options );
			} else {
				DrawArc( d, c, fabs(segPtr->u.c.radius), a0, segPtr->u.c.a1,
						FALSE, (wDrawWidth)floor(segPtr->width*factor+0.5), color1 );
			}
			break;
        case SEG_BEZTRK:
        case SEG_BEZLIN:
            if (segPtr->type == SEG_BEZTRK) {
                if (color1 == wDrawColorBlack)
                    color1 = normalColor;
                if ( segPtr->color == wDrawColorWhite )
                    break;
            }
            REORIGIN(p0, segPtr->u.b.pos[0], angle, orig);
            REORIGIN(p1, segPtr->u.b.pos[1], angle, orig);
            REORIGIN(p2, segPtr->u.b.pos[2], angle, orig);
            REORIGIN(p3, segPtr->u.b.pos[3], angle, orig);

            for(int j=0;j<segPtr->bezSegs.cnt;j++) {     //Loop through sub Segs
            	tempPtr = &DYNARR_N(trkSeg_t,segPtr->bezSegs,j);
            	switch (tempPtr->type) {
        			case SEG_CRVTRK:
        			case SEG_CRVLIN:
        				a0 = NormalizeAngle(tempPtr->u.c.a0 + angle);
        				REORIGIN( c, tempPtr->u.c.center, angle, orig );
        				if (tempPtr->type == SEG_CRVTRK) {
        					if (color1 == wDrawColorBlack)	color1 = normalColor;
        					if ( tempPtr->color == wDrawColorWhite )  break;
        					p0.x = p0.y = p1.x = p1.y = 0;
        					DrawCurvedTrack( d,
            		   					c,
            		   					fabs(tempPtr->u.c.radius),
            		   					a0, tempPtr->u.c.a1,
            		   					p0, p1,
            		   					NULL, trackGauge, color1, options );
        				} else if (tempPtr->type == SEG_CRVLIN) {
        					DrawArc( d, c, fabs(tempPtr->u.c.radius), a0, tempPtr->u.c.a1,
        							FALSE, (wDrawWidth)floor(tempPtr->width*factor+0.5), color1 );
        				}
        				break;
        			case SEG_STRTRK:
        				if (color1 == wDrawColorBlack)	color1 = normalColor;
        				if ( tempPtr->color == wDrawColorWhite ) break;
        				REORIGIN(p0,tempPtr->u.l.pos[0], angle, orig);
        				REORIGIN(p1,tempPtr->u.l.pos[1], angle, orig);
        				DrawStraightTrack( d,
        									p0, p1,
											FindAngle(p0, p1 ),
											NULL, trackGauge, color1, options );
            			break;
        			case SEG_STRLIN:
        				REORIGIN(p0,tempPtr->u.l.pos[0], angle, orig);
        				REORIGIN(p1,tempPtr->u.l.pos[1], angle, orig);
        				DrawLine( d, p0, p1, (wDrawWidth)floor(tempPtr->width*factor+0.5), color1 );
        				break;
            	}
            }
            break;
		case SEG_JNTTRK:
			REORIGIN( p0, segPtr->u.j.pos, angle, orig );
			DrawJointTrack( d, p0, NormalizeAngle(segPtr->u.j.angle+angle), segPtr->u.j.l0, segPtr->u.j.l1, segPtr->u.j.R, segPtr->u.j.L, segPtr->u.j.negate, segPtr->u.j.flip, segPtr->u.j.Scurve, NULL, -1, -1, trackGauge, color1, options );
			break;
		case SEG_TEXT:
			REORIGIN( p0, segPtr->u.t.pos, angle, orig )
			DrawMultiString( d, p0, segPtr->u.t.string, segPtr->u.t.fontP, segPtr->u.t.fontSize, color1, NormalizeAngle(angle + segPtr->u.t.angle), NULL, NULL );
			break;
		case SEG_FILPOLY:
			if ( (d->options&DC_GROUP) == 0 &&
				 d->funcs != &tempSegDrawFuncs ) {
				/* Note: if we call tempSegDrawFillPoly we get a nasty bug
				/+ because we don't make a private copy of p.pts */
				coOrd *tempPts = malloc(sizeof(coOrd)*segPtr->u.p.cnt);
//				coOrd tempPts[segPtr->u.p.cnt];
				for (j=0;j<segPtr->u.p.cnt;j++) {
					REORIGIN( tempPts[j], segPtr->u.p.pts[j], angle, orig );
				}
				DrawFillPoly( d, segPtr->u.p.cnt, tempPts, color1 );
				free(tempPts);
				break;
			} /* else fall thru */
		case SEG_POLY:
			if ( (d->options&DC_GROUP) ) {
				DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
				tempPtr = &tempSegs(tempSegs_da.cnt-1);
				memcpy( tempPtr, segPtr, sizeof segPtr[0] );
				tempPtr->u.p.orig = orig;
				tempPtr->u.p.angle = angle;
				break;
			}
			REORIGIN( p0, segPtr->u.p.pts[0], angle, orig )
			c = p0;
			for (j=1; j<segPtr->u.p.cnt; j++) {
				REORIGIN( p1, segPtr->u.p.pts[j], angle, orig );
				DrawLine( d, p0, p1, (wDrawWidth)floor(segPtr->width*factor+0.5), color1 );
				p0 = p1;
			}
			DrawLine( d, p0, c, (wDrawWidth)floor(segPtr->width*factor+0.5), color1 );
			break;
		case SEG_FILCRCL:
			REORIGIN( c, segPtr->u.c.center, angle, orig )
			if ( (d->options&DC_GROUP) != 0 ||
				 d->funcs != &tempSegDrawFuncs ) {
				DrawFillCircle( d, c, fabs(segPtr->u.c.radius), color1 );
			} else {
				DrawArc( d, c, fabs(segPtr->u.c.radius), 0, 360,
						FALSE, (wDrawWidth)0, color1 );
			}
			break;
		}
	}
}


/*
 * Draw Segments without setting DTS_ options.
 */

EXPORT void DrawSegs(
		drawCmd_p d,
		coOrd orig,
		ANGLE_T angle,
		trkSeg_p segPtr,
		wIndex_t segCnt,
		DIST_T trackGauge,
		wDrawColor color )
{
	DrawSegsO( d, NULL, orig, angle, segPtr, segCnt, trackGauge, color, 0 );
}

/*
 * Free dynamic storage added to each of an array of Track Segments.
 */
EXPORT void CleanSegs(dynArr_t * seg_p) {
	if (seg_p->cnt ==0) return;
	for (int i=0;i<seg_p->cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t,* seg_p,i);
		if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			if (t.bezSegs.ptr) MyFree(t.bezSegs.ptr);
			t.bezSegs.cnt = 0;
			t.bezSegs.max = 0;
			t.bezSegs.ptr = NULL;
		}
	}
	seg_p->cnt = 0;
	if (seg_p->ptr) MyFree(seg_p->ptr);
	seg_p->ptr = NULL;
	seg_p->max = 0;
}


