/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/tbezier.h,v 1.1 2005-12-07 15:47:36 rc-flyer Exp $
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

typedef struct {
		coOrd pos[4];
		DIST_T minCurveRadius;
		ANGLE_T a0, a1;
		DIST_T length;
		dynArr_t arcSegs;
		coOrd descriptionOff;
		DIST_T segsWidth;
		wDrawColor segsColor;
		} BezierData_t;


void BezierSplit(coOrd[4], coOrd[4], coOrd[4] , double );
coOrd BezierPointByParameter(coOrd[4], double);
double BezierMathLength(coOrd[4], double);
coOrd  BezierFirstDerivative(coOrd p[4], double);
coOrd BezierSecondDerivative(coOrd p[4], double);
double BezierCurvature(coOrd[4], double , coOrd *);
double BezierMaxCurve(coOrd[4]);
double BezierMathMinRadius(coOrd[4]);
coOrd BezierMathFindNearestPoint(coOrd *, coOrd[4] , int );
track_p NewBezierTrack(coOrd[4], trkSeg_t * , int );
track_p NewBezierLine(coOrd[4], trkSeg_t * , int, wDrawColor, DIST_T);
DIST_T BezierMathDistance( coOrd *, coOrd[4], int , double * );
void FixUpBezier(coOrd[4], struct extraData*, BOOL_T);
void FixUpBezierSeg(coOrd[4], trkSeg_p , BOOL_T);
void FixUpBezierSegs(trkSeg_p p,int segCnt);
BOOL_T GetBezierSegmentFromTrack(track_p, trkSeg_p);

