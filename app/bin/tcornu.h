/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/tcornu.h,v 1.1 2005-12-07 15:47:36 rc-flyer Exp $
 */

/*  XTrkCad - Model Railroad CAD
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
		coOrd pos[2];
		ANGLE_T a[2];
		DIST_T r[2];
		DIST_T minCurveRadius;
		DIST_T maxRateofChange;
		DIST_T length;
		dynArr_t arcSegs;
		coOrd descriptionOff;
		} cornuData_t;

typedef struct {
		coOrd pos[2];
		ANGLE_T angle[2];
		DIST_T radius[2];
		} cornuParm_t;


double CornuMaxCurve(coOrd[2],ANGLE_T[2],DIST_T[2]);
double BezierMathMinRadius(coOrd[4]);
coOrd BezierMathFindNearestPoint(coOrd *, coOrd[4] , int );
track_p NewCornuTrack(coOrd[2], ANGLE_T[2], DIST_T[2], trkSeg_t * , int );
DIST_T CornuDistance( coOrd *, coOrd[2], ANGLE_T[2], DIST_T[2], trkSeg_t * ,int , double * );
void FixUpCornu(coOrd[4], struct extraData*, BOOL_T);
BOOL_T GetCornuSegmentsFromTrack(track_p, trkSeg_p);

